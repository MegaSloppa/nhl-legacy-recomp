// Legacy Rogue - stat finder (RE-5, round 20, 2026-11-04, Theresa). PROBE ONLY (NHL_ROGUE_PROBE=1),
// READ-ONLY: nothing here writes guest memory or hooks a function.
//
// Goal: find where the game keeps the final score and the pro's stat line, so the result entry can be
// filled in automatically (goal calls miss simulated goals, so counting events is not enough).
// Method (the one that found the ratings at +1240): during one BAP game, read a set of memory regions
// once a second and count how often every byte changes; take checkpoints at game start, a few seconds
// after every goal call and period end (hooks in rogue_static_probe.cpp), every 2 minutes (sim goals)
// and at game over. At game over write rogue_stats_probe.txt: every small integer that changed a few
// times, with its value at each checkpoint. The owner's box score then picks the fields.
// Regions: the pro object, the match object, the heap objects they point to (one level), and the
// image globals 0x83C00000..0x83CC0000 (they hold the CONFIRMED state / match / pro words).
#include "generated/default/nhllegacy_init.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <rex/filesystem.h>
#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include "rogue/addresses.h"
#include "rogue/probe.h"
#include "rogue/rogue_mod.h"

namespace rogue::probe {
namespace {
constexpr uint32_t kGlobalsBase = 0x83C00000u, kGlobalsSize = 0xC0000u;  // 768 KB
constexpr uint32_t kProSize = 0x2000u, kMatchSize = 0x4000u, kChildSize = 0x800u, kMinSize = 0x400u;
constexpr int kMaxChildrenPro = 96, kMaxChildrenMatch = 160;
constexpr float kPeriodicS = 120.f, kAfterEventS = 4.f, kAfterOverS = 5.f;
constexpr uint16_t kNoisy = 300;      // bytes changing more often are not kept in checkpoints
constexpr uint16_t kMaxCounter = 120;  // a stat changes at most this often in one game
constexpr size_t kMaxLines = 6000;

struct Region { std::string name; uint32_t va = 0, size = 0; size_t off = 0; };
struct Checkpoint { std::string label; float t = 0.f; std::vector<std::pair<uint32_t, uint8_t>> diff; };
struct Due { std::string label; float at = 0.f; };

std::mutex g_mu;  // worker state below
std::vector<Region> g_regions;
std::vector<uint8_t> g_first, g_prev, g_cp, g_tmp;
std::vector<uint16_t> g_changes;
std::vector<Checkpoint> g_cps;
std::chrono::steady_clock::time_point g_t0;
bool g_pending_start = false, g_active = false;
float g_next_periodic = 0.f, g_end_at = -1.f;
int g_scans = 0, g_skipped = 0, g_reports = 0;
std::mutex g_due_mu;  // events from guest threads
std::vector<Due> g_due;
std::atomic<bool> g_thread{false}, g_want_start{false}, g_want_end{false}, g_want_resume{false}, g_want_write{false};
std::atomic<bool> g_live{false};
std::atomic<float> g_now{0.f};
char g_status[200] = "Stat finder: waits for a BAP game";
std::mutex g_status_mu;

uint8_t* VB() {
  auto* ks = rex::runtime::current_kernel_state();
  return (ks && ks->memory()) ? ks->memory()->virtual_membase() : nullptr;
}
const uint8_t* HostOf(const uint8_t* vb, uint32_t va) { return vb + va + REX_PHYS_HOST_OFFSET(va); }
bool CanRead(const uint8_t* p, size_t n) {
  MEMORY_BASIC_INFORMATION mbi{};
  if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
  if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
  return p + n <= static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
}
uint32_t BeAt(const uint8_t* h) { return (uint32_t(h[0]) << 24) | (uint32_t(h[1]) << 16) | (uint32_t(h[2]) << 8) | h[3]; }
bool IsHeap(uint32_t va) { return va >= 0x40000000u && !(va >= 0x82000000u && va < 0x84000000u); }
float Now() { return std::chrono::duration<float>(std::chrono::steady_clock::now() - g_t0).count(); }

void SetStatus(const char* fmt, ...) {
  std::lock_guard<std::mutex> lk(g_status_mu);
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(g_status, sizeof(g_status), fmt, ap);
  va_end(ap);
}

// Largest readable size <= want (halving down to kMinSize); 0 if not readable.
uint32_t ReadableSize(const uint8_t* vb, uint32_t va, uint32_t want) {
  for (uint32_t s = want; s >= kMinSize; s /= 2)
    if (CanRead(HostOf(vb, va), s)) return s;
  return 0;
}
void AddRegion(const uint8_t* vb, const std::string& name, uint32_t va, uint32_t want) {
  va &= ~3u;
  const uint32_t s = ReadableSize(vb, va, want);
  if (!s) return;
  Region r;
  r.name = name; r.va = va; r.size = s; r.off = g_first.size();
  g_first.resize(r.off + s);
  std::memcpy(g_first.data() + r.off, HostOf(vb, va), s);
  g_regions.push_back(r);
}
void AddChildren(const uint8_t* vb, const Region& parent, int max, std::set<uint32_t>& seen) {
  int n = 0;
  for (uint32_t o = 0; o + 4 <= parent.size && n < max; o += 4) {
    const uint32_t p = BeAt(g_first.data() + parent.off + o) & ~3u;
    if (!IsHeap(p) || seen.count(p)) continue;
    bool inside = false;
    for (const Region& r : g_regions) inside = inside || (p >= r.va && p < r.va + r.size);
    if (inside) continue;
    seen.insert(p);
    char nm[48];
    std::snprintf(nm, sizeof(nm), "%s[+0x%04X]", parent.name.c_str(), o);
    const size_t before = g_regions.size();
    AddRegion(vb, nm, p, kChildSize);
    n += g_regions.size() > before;
  }
}

bool Build() {
  const uint8_t* vb = VB();
  if (!vb) return false;
  const uint32_t pro = rogue::AutoBapPlayer();
  const uint32_t match = CanRead(HostOf(vb, ROGUE_VA_MATCH_PTR), 4) ? BeAt(HostOf(vb, ROGUE_VA_MATCH_PTR)) : 0u;
  if (!pro || !IsHeap(match)) return false;
  g_regions.clear(); g_first.clear(); g_cps.clear();
  AddRegion(vb, "pro", pro, kProSize);
  AddRegion(vb, "match", match, kMatchSize);
  std::set<uint32_t> seen;
  const std::vector<Region> roots = g_regions;
  for (const Region& r : roots) {
    if (r.name == "pro") AddChildren(vb, r, kMaxChildrenPro, seen);
    if (r.name == "match") AddChildren(vb, r, kMaxChildrenMatch, seen);
  }
  // Globals last (round 21): round 20's report was capped inside them (0x83C36C00.. holds index tables
  // that look like counters) and never reached the child objects.
  AddRegion(vb, "g", kGlobalsBase, kGlobalsSize);
  g_prev = g_first; g_cp = g_first; g_tmp.resize(g_first.size());
  g_changes.assign(g_first.size(), 0);
  g_scans = g_skipped = 0;
  return !g_regions.empty();
}

void Scan() {
  const uint8_t* vb = VB();
  if (!vb) return;
  for (const Region& r : g_regions) {
    const uint8_t* h = HostOf(vb, r.va);
    if (!CanRead(h, r.size)) { ++g_skipped; continue; }
    std::memcpy(g_tmp.data() + r.off, h, r.size);
    for (size_t i = r.off, e = r.off + r.size; i < e; ++i)
      if (g_tmp[i] != g_prev[i]) {
        g_prev[i] = g_tmp[i];
        if (g_changes[i] < 0xFFFFu) ++g_changes[i];
      }
  }
  ++g_scans;
}

void TakeCheckpoint(const std::string& label) {
  Checkpoint c;
  c.label = label;
  c.t = Now();
  for (size_t i = 0; i < g_prev.size(); ++i)
    if (g_prev[i] != g_cp[i] && g_changes[i] <= kNoisy) { c.diff.emplace_back(uint32_t(i), g_prev[i]); g_cp[i] = g_prev[i]; }
  g_cps.push_back(std::move(c));
}

const Region* RegionOf(size_t flat) {
  for (const Region& r : g_regions)
    if (flat >= r.off && flat < r.off + r.size) return &r;
  return nullptr;
}
bool Fits(const std::vector<int64_t>& v) {
  int64_t lo = v[0], hi = v[0];
  for (int64_t x : v) { lo = std::min(lo, x); hi = std::max(hi, x); }
  return lo >= -60 && hi <= 400 && hi != lo;
}
void Emit(FILE* f, size_t flat, const char* view, const std::vector<int64_t>& v, size_t& lines) {
  if (lines >= kMaxLines) return;
  const Region* r = RegionOf(flat);
  if (!r) return;
  const uint32_t o = uint32_t(flat - r->off);
  std::fprintf(f, "%s+0x%04X (va %08X) %-4s ch=%-3u:", r->name.c_str(), o, r->va + o, view, unsigned(g_changes[flat]));
  for (int64_t x : v) std::fprintf(f, " %lld", (long long)x);
  std::fputc('\n', f);
  ++lines;
}

void WriteReport() {
  const auto path = rex::filesystem::GetExecutableFolder() / "rogue_stats_probe.txt";
  FILE* f = std::fopen(path.string().c_str(), "w");
  if (!f) { SetStatus("Stat finder: could not write rogue_stats_probe.txt"); return; }
  std::fprintf(f, "Legacy Rogue stat finder (RE-5, round 20). Written %.0f s after the game start; %d scans, %d region reads skipped.\n",
               double(Now()), g_scans, g_skipped);
  std::fprintf(f, "Upload this file with the game's box score (final score, shots, and your G / A / +/- / PIM / SOG / hits / TOI).\n\n");
  std::fprintf(f, "checkpoints (values below are listed in this order):\n");
  for (size_t k = 0; k < g_cps.size(); ++k)
    std::fprintf(f, "  #%zu t=%.0f %s (%zu bytes changed)\n", k, double(g_cps[k].t), g_cps[k].label.c_str(), g_cps[k].diff.size());
  std::fprintf(f, "\nregions: %zu\n", g_regions.size());
  for (const Region& r : g_regions) std::fprintf(f, "  %s va=%08X size=0x%X\n", r.name.c_str(), r.va, r.size);
  // Candidate words: some byte changed, no byte changed more than a stat could.
  std::vector<size_t> cand;
  for (const Region& r : g_regions)
    for (size_t w = r.off; w + 4 <= r.off + r.size; w += 4) {
      uint16_t mx = 0;
      for (int b = 0; b < 4; ++b) mx = std::max(mx, g_changes[w + b]);
      if (mx > 0 && mx <= kMaxCounter) cand.push_back(w);
    }
  // Values of every candidate word at every checkpoint (replay the checkpoint diffs).
  std::vector<uint8_t> cur = g_first;
  std::vector<uint32_t> vals(cand.size() * g_cps.size());
  for (size_t k = 0; k < g_cps.size(); ++k) {
    for (const auto& d : g_cps[k].diff) cur[d.first] = d.second;
    for (size_t c = 0; c < cand.size(); ++c) vals[c * g_cps.size() + k] = BeAt(cur.data() + cand[c]);
  }
  std::fprintf(f, "\ncandidates: %zu changed words (max %u changes per byte); small integers only, max %zu lines\n",
               cand.size(), unsigned(kMaxCounter), kMaxLines);
  size_t lines = 0;
  const size_t n = g_cps.size();
  std::vector<int64_t> v(n);
  for (size_t c = 0; c < cand.size() && n; ++c) {
    const uint32_t* w = &vals[c * n];
    for (size_t k = 0; k < n; ++k) v[k] = int32_t(w[k]);
    if (Fits(v)) { Emit(f, cand[c], "s32", v, lines); continue; }
    bool any = false;
    for (int h = 0; h < 2; ++h) {
      for (size_t k = 0; k < n; ++k) v[k] = int16_t(uint16_t(w[k] >> (h ? 0 : 16)));
      if (Fits(v)) { Emit(f, cand[c] + (h ? 2 : 0), "s16", v, lines); any = true; continue; }
      for (int b = 0; b < 2; ++b) {
        const int sh = (h ? 8 : 24) - 8 * b;
        for (size_t k = 0; k < n; ++k) v[k] = int8_t(uint8_t(w[k] >> sh));
        if (Fits(v)) { Emit(f, cand[c] + size_t(2 * h + b), "s8", v, lines); any = true; }
      }
    }
    if (any) continue;
    bool ok = true;
    for (size_t k = 0; k < n && ok; ++k) {
      float x;
      std::memcpy(&x, &w[k], 4);
      ok = std::isfinite(x) && std::fabs(x) <= 400.f && std::fabs(x * 2.f - std::round(x * 2.f)) < 1e-4f;
      v[k] = int64_t(std::lround(double(x) * 2.0));  // printed as half-units below
    }
    if (ok && Fits(v)) Emit(f, cand[c], "f32x2", v, lines);
  }
  std::fprintf(f, "\n%zu lines written%s. f32x2 = float value times 2.\n", lines, lines >= kMaxLines ? " (CAPPED)" : "");
  std::fclose(f);
  ++g_reports;
}

void Worker() {
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_want_start.exchange(false)) { g_pending_start = true; g_active = false; g_end_at = -1.f; }
    if (g_pending_start) {
      if (!Build()) { SetStatus("Stat finder: waiting for the pro and the match"); continue; }
      g_pending_start = false;
      g_active = true;
      g_t0 = std::chrono::steady_clock::now();
      g_next_periodic = kPeriodicS;
      { std::lock_guard<std::mutex> dl(g_due_mu); g_due.clear(); }
      TakeCheckpoint("game start");
    }
    if (g_want_resume.exchange(false) && !g_regions.empty()) { g_active = true; g_end_at = -1.f; TakeCheckpoint("game resumed"); }
    if (!g_active) continue;
    g_live = true;
    Scan();
    const float t = Now();
    g_now = t;
    if (g_want_end.exchange(false) && g_end_at < 0.f) g_end_at = t + kAfterOverS;
    {
      std::lock_guard<std::mutex> dl(g_due_mu);
      for (auto it = g_due.begin(); it != g_due.end();)
        if (t >= it->at) { TakeCheckpoint(it->label); it = g_due.erase(it); } else ++it;
    }
    if (t >= g_next_periodic) { TakeCheckpoint("every 2 min"); g_next_periodic += kPeriodicS; }
    if (g_want_write.exchange(false)) { TakeCheckpoint("panel: write now"); WriteReport(); }
    if (g_end_at >= 0.f && t >= g_end_at) {
      TakeCheckpoint("game over");
      WriteReport();
      g_end_at = -1.f;
      g_active = false;
      g_live = false;
    }
    SetStatus("Stat finder: %s, %zu regions (%zu KB), %zu checkpoints, %d report(s) written",
              g_active ? "watching" : "done (report written)", g_regions.size(), g_first.size() / 1024,
              g_cps.size(), g_reports);
  }
}
void EnsureThread() {
  if (!g_thread.exchange(true)) std::thread(Worker).detach();
}
}  // namespace

void StatGameStart() {
  if (!Enabled()) return;
  EnsureThread();
  g_want_start = true;
}
void StatGameOver() { if (Enabled()) g_want_end = true; }
void StatResume() { if (Enabled()) g_want_resume = true; }
void StatEvent(const char* label) {
  if (!Enabled() || !g_live.load()) return;
  std::lock_guard<std::mutex> dl(g_due_mu);
  if (g_due.size() < 64) g_due.push_back(Due{label, g_now.load() + kAfterEventS});
}
void StatWriteNow() { if (Enabled()) g_want_write = true; }
const char* StatStatus() {
  static char copy[200];
  std::lock_guard<std::mutex> lk(g_status_mu);
  std::memcpy(copy, g_status, sizeof(copy));
  return copy;
}
}  // namespace rogue::probe
