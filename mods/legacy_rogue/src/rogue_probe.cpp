// Legacy Rogue - TUN-4 probe: what does a tweak-registry record actually hold?
//
// Background (docs/current-status.md, TUN-4). The parent ITunableStore reads and writes
// record_va+8. In docs/tunable_values_runtime.txt the 11,158 resolved values collapse to
// 4 (7,227), 2 (1,360), 1 (1,110) and 12/14/11/13 (1,441): bool-typed names read 1,
// float-typed names read 4 (even multipliers), every gTweakByGameStyle* reads 13.
// Owner test 2026-10-29: gTweakByGameStyleGameSpeedLevel 13 -> 0 and -> 20 in a match had
// no visible effect, and the game's own speed slider did not change the 13.
// HYPOTHESIS (unproven): +8 is a type/kind tag; the real value lives elsewhere in the
// record or behind a pointer in it (e.g. the gXxx global in .data).
//
// This probe dumps, for a few tunables: every record that points at the name string,
// 16 bytes before and 48 bytes after it, and 16 bytes at every pointer found in the
// record. Run it at the menu, in a match, and after an overlay edit, then diff.
//
// Snapshots are read-only. The TUN-5 write test below is the only code that writes, and
// only on a button press. Same committed-memory walk as the parent
// TunableRuntimeStore::Build (src/tunable_runtime.h). Output: rogue_probe.txt next to
// the exe, appended, one block per snapshot.
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
#include <chrono>
#include <mutex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <rex/filesystem.h>
#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include "rogue/probe.h"

namespace rogue::probe {
namespace {

constexpr uint32_t kImgBase = 0x82000000u;  // same constants as tunable_runtime.h
constexpr uint32_t kImgSize = 0x1EA0000u;
constexpr int kMaxHits = 8;    // per name; the image itself also holds pointers to names
constexpr int kBefore = 16;    // bytes dumped before the name pointer
constexpr int kAfter = 48;     // bytes dumped from the name pointer on

struct Target { const char* name; const char* note; };
const Target kTargets[] = {
  {"gDekeRatEffect", "float, store shows ~0.2 (one of the few plausible floats)"},
  {"gDekeRatEffectCpu", "float, plausible float in store"},
  {"gAccelImprovementTuner", "float, plausible float in store"},
  {"gSlapShotPowerMultiplier", "float multiplier; store shows raw 4"},
  {"gSlapShotPowerScale", "float multiplier; store shows raw 4"},
  {"gSlapShotPowerSlider", "schema: slider 0..100; store shows 12"},
  {"gTweakByGameStyleGameSpeedLevel", "schema: int 0..20; store shows 13; edit had no effect"},
  {"gTweakByGameStyleSlapShotPowerLevel", "Cannon perk target; store shows 13"},
  {"gTweakByGameStyleWristShotAccuracyLevel", "Sniper perk target; store shows 13"},
  {"gBeAPro_LineEnergyBoost", "Iron Lungs target; store shows 4"},
  {"gEnableLynxBinaryFileLoad", "bool; store shows 1"},
  {"gLODShader", "int; store shows 2"},
};
constexpr int kNumTargets = int(sizeof(kTargets) / sizeof(kTargets[0]));

std::atomic<int> g_state{0};  // 0 idle, 1 running, 2 done, 3 failed
std::atomic<unsigned> g_last_ms{0};

bool EnvOn(const char* n) { const char* e = std::getenv(n); return e && *e == '1'; }

bool Readable(const uint8_t* p, size_t n) {
  MEMORY_BASIC_INFORMATION mbi{};
  if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
  if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
  return p + n <= static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
}
uint32_t Be(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
double F(uint32_t b) { float f; std::memcpy(&f, &b, 4); return double(f); }

// Host address of a guest pointer, same translation as rogue_hooks.cpp / diag_hooks.cpp.
const uint8_t* Host(const uint8_t* vbase, uint32_t va) {
  return vbase + va + REX_PHYS_HOST_OFFSET(va);
}

void PrintTarget(FILE* f, const uint8_t* host, uint32_t va) {
  std::fprintf(f, "        -> %08X:", va);
  for (int k = 0; k < 16; k += 4) {
    const uint32_t w = Be(host + k);
    std::fprintf(f, " %08X(%g)", w, F(w));
  }
  bool ascii = true;
  for (int k = 0; k < 4; ++k) ascii = ascii && host[k] >= 0x20 && host[k] < 0x7F;
  if (ascii) {
    char s[33] = {};
    for (int k = 0; k < 32 && host[k] >= 0x20 && host[k] < 0x7F; ++k) s[k] = char(host[k]);
    std::fprintf(f, "  \"%s\"", s);
  }
  std::fprintf(f, "\n");
}

void DumpRecord(FILE* f, const uint8_t* vbase, uint32_t rec, uint32_t name_va) {
  for (int off = -kBefore; off < kAfter; off += 4) {
    const uint32_t a = rec + uint32_t(off);
    const uint8_t* p = vbase + a;  // raw view: the record was found through this view
    if (!Readable(p, 4)) { std::fprintf(f, "      rec%+4d: --------\n", off); continue; }
    const uint32_t w = Be(p);
    const char* tag = off == 0 ? "  <- name pointer"
                    : off == 8 ? "  <- record+8 (what Engine Tunables edits)" : "";
    std::fprintf(f, "      rec%+4d: %08X  i32=%-11d f32=%-13g%s\n", off, w, int32_t(w), F(w), tag);
    if (off > 0 && w != name_va && w >= 0x40000000u && w < 0xFFFF0000u) {
      const uint8_t* h = Host(vbase, w);
      if (Readable(h, 32)) PrintTarget(f, h, w);
    }
  }
}

void Snapshot(std::string label) {
  const DWORD t0 = GetTickCount();
  auto* ks = rex::runtime::current_kernel_state();
  const uint8_t* vbase = (ks && ks->memory()) ? ks->memory()->virtual_membase() : nullptr;
  const uint8_t* img = vbase ? vbase + kImgBase : nullptr;
  const auto path = rex::filesystem::GetExecutableFolder() / "rogue_probe.txt";
  FILE* f = vbase && Readable(img, 4) ? std::fopen(path.string().c_str(), "a") : nullptr;
  if (!f) { g_state.store(3); return; }
  std::fprintf(f, "\n=== Legacy Rogue TUN-4 probe: %s (tick %lu) ===\n", label.c_str(),
               (unsigned long)t0);

  // 1. Name strings ("\0gName\0") in the mapped image.
  struct NameVa { uint32_t va; int target; };
  std::vector<NameVa> names;
  for (int t = 0; t < kNumTargets; ++t) {
    const char* n = kTargets[t].name;
    const uint32_t len = uint32_t(std::strlen(n));
    int count = 0;
    for (uint32_t i = 1; i + len + 1 < kImgSize; ++i) {
      if (img[i] != uint8_t(n[0]) || img[i - 1] != 0 || img[i + len] != 0) continue;
      if (std::memcmp(img + i, n, len) != 0) continue;
      names.push_back({kImgBase + i, t});
      ++count;
    }
    std::fprintf(f, "name %-40s strings=%d  (%s)\n", n, count, kTargets[t].note);
  }
  if (names.empty()) { std::fprintf(f, "no names found\n"); std::fclose(f); g_state.store(2); return; }
  std::sort(names.begin(), names.end(), [](const NameVa& a, const NameVa& b) { return a.va < b.va; });
  const uint32_t lo = names.front().va, hi = names.back().va;

  // 2. Every aligned big-endian word equal to a name VA = a record (or a code/pool ref).
  struct Hit { int target; uint32_t name_va, rec; };
  std::vector<Hit> hits;
  std::vector<int> per(kNumTargets, 0);
  const uint8_t* p = vbase;
  const uint8_t* end = vbase + size_t(0x100000000ull);
  while (p < end) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) break;
    auto* rb = static_cast<const uint8_t*>(mbi.BaseAddress);
    const size_t rs = mbi.RegionSize;
    if (mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) && rb >= vbase) {
      for (size_t o = 0; o + 4 <= rs; o += 4) {
        const uint32_t v = Be(rb + o);
        if (v < lo || v > hi) continue;
        auto it = std::lower_bound(names.begin(), names.end(), v,
                                   [](const NameVa& a, uint32_t x) { return a.va < x; });
        if (it == names.end() || it->va != v || per[it->target] >= kMaxHits) continue;
        ++per[it->target];
        hits.push_back({it->target, v, uint32_t((rb + o) - vbase)});
      }
    }
    p = rb + rs;
    if (p <= rb) break;
  }

  // 3. Dump grouped by tunable.
  for (int t = 0; t < kNumTargets; ++t) {
    std::fprintf(f, "\n--- %s: %d reference(s)%s\n", kTargets[t].name, per[t],
                 per[t] >= kMaxHits ? " (capped)" : "");
    int k = 0;
    for (const Hit& h : hits) {
      if (h.target != t) continue;
      const bool in_img = h.rec >= kImgBase && h.rec < kImgBase + kImgSize;
      std::fprintf(f, "  ref #%d at %08X (%s), name string %08X\n", k++, h.rec,
                   in_img ? "inside image" : "outside image", h.name_va);
      DumpRecord(f, vbase, h.rec, h.name_va);
    }
  }
  const unsigned ms = unsigned(GetTickCount() - t0);
  std::fprintf(f, "=== end of snapshot (%u ms) ===\n", ms);
  std::fclose(f);
  g_last_ms.store(ms);
  g_state.store(2);
}
}  // namespace

bool Enabled() {
  static const bool on = EnvOn("NHL_ROGUE") && EnvOn("NHL_ROGUE_PROBE");
  return on;
}
bool Busy() { return g_state.load() == 1; }

void RunAsync(const char* label) {
  if (!Enabled() || Busy()) return;
  g_state.store(1);
  std::thread([l = std::string(label)] { Snapshot(l); }).detach();
}

const char* Status() {
  static char buf[128];
  switch (g_state.load()) {
    case 1: return "Scanning guest memory (can take up to a minute, game may stutter)...";
    case 2: std::snprintf(buf, sizeof(buf), "Done in %u ms (rogue_probe.txt / rogue_tweak_catalog.txt)", g_last_ms.load()); return buf;
    case 3: return "Failed: guest memory not available or rogue_probe.txt could not be opened";
    default: return "Idle";
  }
}

// ---------------------------------------------------------------- write test (TUN-5b/5c)
// TUN-5c targets replaced 2026-10-29 by hip-check tunables (TUN-10). TUN-5b RESULT (owner, 2026-10-29): N+32 IS the live value. gSlapShotPowerScale x0.2 made slap
// shots limp, x10 (above its own max 2.0) made them very fast; gLODShader 0/2 changed rendering
// immediately. Game-style tables (all entries 0/20) had no visible effect. TUN-5c targets below:
// hit force, fall thresholds, check reach, one penalty chance, and higher slap-shot scales.
// TUN-5 wrote N+20 and had no effect. TUN-8 static scan (2026-10-29): real code reads
// base+40 = N+32 (gHitTune_GlobalScale in sub_83588BF8, gSlapShotPowerScale in sub_833E87E0,
// gLODShader in sub_8361FA70, ...), and game-style values are read through the heap copy:
// sub_829F8A20 loads H = *(N+36), then the table pointer *(H+32), then the entries.
// HYPOTHESIS: N+32 is the live value (N+20 is probably the reset/default value; they are equal
// at stock, which is why TUN-4 could not tell them apart). This test writes:
//   scalar: image N+32 and heap H+36 (same field in the copy, whose name pointer is at H+4)
//   table : every entry of the heap table *(H+32) and the static table *(N+24)
// Every write is logged to rogue_probe.txt. Restore or restart to undo.
namespace {
enum class Kind { kScalar, kTable };  // scalar: float (tag 4) or int (tag 2), decided at lookup
struct Preset { const char* label; bool mul; float v; };
struct WriteTest {
  const char* name;
  Kind kind;
  Preset p[3];
  bool tried = false, found = false, active = false;
  int n = 0;                 // number of patched words
  uint32_t va[8] = {};       // guest addresses
  bool raw[8] = {};          // true = image raw view, false = translated view
  uint32_t stock[8] = {};
  int last = -1;
  bool is_float = true;
};
WriteTest g_wt[] = {
  // Hip check selection (read by sub_83411E90 / sub_83411C48). Directions are GUESSES.
  {"gHipCheckRadiusMax", Kind::kScalar, {{"x3", true, 3.f}, {"x6", true, 6.f}, {"x10", true, 10.f}}},        // 0.3
  {"gHipCheckRadiusMaxest", Kind::kScalar, {{"x2", true, 2.f}, {"x5", true, 5.f}, {"x10", true, 10.f}}},     // 1.0
  {"gHipCheckSpeedMin", Kind::kScalar, {{"x0", true, 0.f}, {"x0.5", true, 0.5f}, {"x2", true, 2.f}}},       // 0.25
  {"gHipCheckAngleMin", Kind::kScalar, {{"x0", true, 0.f}, {"x0.5", true, 0.5f}, {"x2", true, 2.f}}},       // 0.5
  {"gHipCheckAngleMax", Kind::kScalar, {{"x0.5", true, 0.5f}, {"x2", true, 2.f}, {"x3", true, 3.f}}},       // 0.6
  {"gHipCheckBalanceRatioTotalThreshold", Kind::kScalar, {{"x0.2", true, 0.2f}, {"x0.5", true, 0.5f}, {"x2", true, 2.f}}},  // 1.25
  // Hip check knockdown / contact (read by sub_83429698 / sub_8342C628)
  {"gHipCheckBalanceThreshold", Kind::kScalar, {{"x0.2", true, 0.2f}, {"x1.5", true, 1.5f}, {"x2", true, 2.f}}},  // 0.5
  {"gHipCheckContactCOMRatio", Kind::kScalar, {{"x0.2", true, 0.2f}, {"x1.3", true, 1.3f}, {"x2", true, 2.f}}},   // 0.75
  // Hit force (read by sub_83589230)
  {"gHitPowerForPhysicsGlobalModifier", Kind::kScalar, {{"x3", true, 3.f}, {"x10", true, 10.f}, {"x20", true, 20.f}}},  // 0.5
};
// One-click guess at "every check can become a hip check, and hits hard".
struct ComboStep { const char* name; float factor; };
const ComboStep kHipCombo[] = {
  {"gHipCheckRadiusMax", 6.f}, {"gHipCheckRadiusMaxest", 2.f}, {"gHipCheckSpeedMin", 0.f},
  {"gHipCheckAngleMin", 0.f}, {"gHipCheckAngleMax", 3.f}, {"gHitPowerForPhysicsGlobalModifier", 10.f},
};
constexpr int kNumWt = int(sizeof(g_wt) / sizeof(g_wt[0]));

uint8_t* VBase() {
  auto* ks = rex::runtime::current_kernel_state();
  return (ks && ks->memory()) ? ks->memory()->virtual_membase() : nullptr;
}
uint8_t* HostW(uint8_t* vb, uint32_t va) { return vb + va + REX_PHYS_HOST_OFFSET(va); }
uint8_t* At(uint8_t* vb, const WriteTest& t, int k) {
  return t.raw[k] ? vb + t.va[k] : HostW(vb, t.va[k]);
}
void StoreBe(uint8_t* p, uint32_t b) {
  p[0] = uint8_t(b >> 24); p[1] = uint8_t(b >> 16); p[2] = uint8_t(b >> 8); p[3] = uint8_t(b);
}
uint32_t FBits(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
float BitsF(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }

void LogWrite(const char* what, const WriteTest& t) {
  const auto path = rex::filesystem::GetExecutableFolder() / "rogue_probe.txt";
  if (FILE* f = std::fopen(path.string().c_str(), "a")) {
    std::fprintf(f, "write-test-b tick=%lu %s %s:", (unsigned long)GetTickCount(), what, t.name);
    if (uint8_t* vb = VBase())
      for (int k = 0; k < t.n; ++k) std::fprintf(f, " %08X=%08X", t.va[k], Be(At(vb, t, k)));
    std::fprintf(f, "\n");
    std::fclose(f);
  }
}

void Add(WriteTest& t, uint32_t va, bool raw, uint8_t* vb) {
  t.va[t.n] = va; t.raw[t.n] = raw;
  t.stock[t.n] = Be(raw ? vb + va : HostW(vb, va));
  ++t.n;
}

bool Resolve(WriteTest& t) {
  if (t.tried) return t.found;
  uint8_t* vb = VBase();
  if (!vb) return false;
  t.tried = true;
  const uint8_t* img = vb + kImgBase;
  const uint32_t len = uint32_t(std::strlen(t.name));
  uint32_t name_va = 0;
  int count = 0;
  for (uint32_t i = 1; i + len + 1 < kImgSize; ++i) {
    if (img[i] != uint8_t(t.name[0]) || img[i - 1] != 0 || img[i + len] != 0) continue;
    if (std::memcmp(img + i, t.name, len) != 0) continue;
    name_va = kImgBase + i;
    ++count;
  }
  if (count != 1) return false;
  for (uint32_t o = 8; o + 40 <= kImgSize; o += 4) {
    if (Be(img + o) != name_va) continue;
    const uint32_t want_tag = Be(img + o + 8);
    if (t.kind == Kind::kTable ? want_tag != 13u : (want_tag != 4u && want_tag != 2u)) continue;
    t.is_float = want_tag == 4u;
    const uint32_t vt = Be(img + o - 8);
    if (vt < kImgBase || vt >= kImgBase + kImgSize) continue;
    const uint32_t heap = Be(img + o + 36);
    if (heap < 0x40000000u) continue;
    uint8_t* h = HostW(vb, heap);
    if (!Readable(h, 40) || Be(h) != want_tag || Be(h + 4) != name_va) continue;
    const uint32_t n_va = kImgBase + o;
    if (t.kind == Kind::kTable) {
      const uint32_t cnt = Be(img + o + 20);
      const uint32_t stat = Be(img + o + 24);
      const uint32_t dyn = Be(h + 32);
      if (cnt == 0 || cnt > 3 || stat < kImgBase || dyn < 0x40000000u) continue;
      if (!Readable(HostW(vb, dyn), 4 * cnt)) continue;
      for (uint32_t k = 0; k < cnt; ++k) Add(t, dyn + 4 * k, false, vb);
      for (uint32_t k = 0; k < cnt; ++k) Add(t, stat + 4 * k, true, vb);
    } else {
      Add(t, n_va + 32, true, vb);   // image N+32 (base+40), the field real code loads
      Add(t, heap + 36, false, vb);  // heap copy, same field
    }
    t.found = true;
    return true;
  }
  return false;
}
}  // namespace

int WriteTestCount() { return kNumWt; }
const char* WriteTestName(int i) { return g_wt[i].name; }
const char* WriteTestPresetLabel(int i, int p) { return g_wt[i].p[p].label; }

namespace {
bool ApplyFactor(WriteTest& t, bool mul, float v, const char* label) {
  uint8_t* vb = VBase();
  if (!vb || !Resolve(t)) return false;
  for (int k = 0; k < t.n; ++k) {
    uint32_t bits;
    if (t.kind == Kind::kScalar && t.is_float) bits = FBits(mul ? BitsF(t.stock[k]) * v : v);
    else bits = uint32_t(int32_t(mul ? float(int32_t(t.stock[k])) * v : v));
    StoreBe(At(vb, t, k), bits);
  }
  t.active = true;
  LogWrite(label, t);
  return true;
}
}  // namespace

bool WriteTestApply(int i, int p) {
  if (!Enabled() || i < 0 || i >= kNumWt || p < 0 || p > 2) return false;
  WriteTest& t = g_wt[i];
  t.last = p;
  return ApplyFactor(t, t.p[p].mul, t.p[p].v, t.p[p].label);
}

void WriteTestHipCombo() {
  if (!Enabled()) return;
  for (const ComboStep& s : kHipCombo)
    for (WriteTest& t : g_wt)
      if (!std::strcmp(t.name, s.name)) ApplyFactor(t, true, s.factor, "hip-combo");
}

void WriteTestRestore(int i) {
  if (i < 0 || i >= kNumWt) return;
  WriteTest& t = g_wt[i];
  uint8_t* vb = VBase();
  if (!vb || !t.found || !t.active) return;
  for (int k = 0; k < t.n; ++k) StoreBe(At(vb, t, k), t.stock[k]);
  t.active = false;
  t.last = -1;
  LogWrite("restore", t);
}

void WriteTestRestoreAll() { for (int i = 0; i < kNumWt; ++i) WriteTestRestore(i); }

void WriteTestDescribe(int i, char* buf, unsigned n) {
  const WriteTest& t = g_wt[i];
  uint8_t* vb = VBase();
  if (!t.tried) { std::snprintf(buf, n, "not looked up yet (press a button)"); return; }
  if (!t.found || !vb) { std::snprintf(buf, n, "NOT FOUND: layout check failed, nothing written"); return; }
  int w = 0;
  w += std::snprintf(buf + w, n - w, "live:");
  for (int k = 0; k < t.n && w < int(n) - 24; ++k) {
    const uint32_t b = Be(At(vb, t, k));
    if (t.kind == Kind::kScalar && t.is_float) w += std::snprintf(buf + w, n - w, " %.3f", BitsF(b));
    else w += std::snprintf(buf + w, n - w, " %d", int32_t(b));
  }
  if (w < int(n) - 24)
    std::snprintf(buf + w, n - w, "%s", t.active ? "  | ACTIVE" : "  | stock");
}

// ---------------------------------------------------------------- TUN-9 catalog
// Walks the image for every tweak object (signature: N -> "g..." name string, tag at N+8,
// type class at N-8 inside the image, heap copy *(N+36) whose +4 is the same name pointer) and
// writes rogue_tweak_catalog.txt (tab-separated) next to the exe: the real live value (N+32), N+20 and the
// min/max at N+24/N+28. Read-only. Feeds tools/find_tweak_consumers.py --catalog.
namespace {
bool IsTweakName(const uint8_t* s, uint32_t max) {
  if (s[0] != 'g') return false;
  uint32_t i = 1;
  for (; i < max && s[i]; ++i) {
    const uint8_t ch = s[i];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_'))
      return false;
  }
  return i > 2 && i < max;
}
void FormatVal(char* out, size_t n, uint32_t tag, uint32_t b) {
  if (tag == 4) std::snprintf(out, n, "%g", F(b));
  else if (tag == 2) std::snprintf(out, n, "%d", int32_t(b));
  else std::snprintf(out, n, "0x%08X", b);
}
void CatalogWorker() {
  const DWORD t0 = GetTickCount();
  auto* ks = rex::runtime::current_kernel_state();
  uint8_t* vb = (ks && ks->memory()) ? ks->memory()->virtual_membase() : nullptr;
  const auto path = rex::filesystem::GetExecutableFolder() / "rogue_tweak_catalog.txt";
  FILE* f = vb ? std::fopen(path.string().c_str(), "w") : nullptr;
  if (!f) { g_state.store(3); return; }
  const uint8_t* img = vb + kImgBase;
  std::fprintf(f, "name\ttag\tN\theap\tvalue_N32\tN20\tmin_N24\tmax_N28\traw_N32\n");
  unsigned count = 0;
  for (uint32_t o = 8; o + 40 <= kImgSize; o += 4) {
    const uint32_t np = Be(img + o);
    if (np < kImgBase + 1 || np >= kImgBase + kImgSize - 2) continue;
    if (img[np - kImgBase] != 'g') continue;
    const uint32_t tag = Be(img + o + 8);
    if (tag == 0 || tag > 63) continue;
    const uint32_t vt = Be(img + o - 8);
    if (vt < kImgBase || vt >= kImgBase + kImgSize) continue;
    if (!IsTweakName(img + (np - kImgBase), std::min<uint32_t>(96, kImgBase + kImgSize - np))) continue;
    const uint32_t heap = Be(img + o + 36);
    if (heap < 0x40000000u) continue;
    const uint8_t* h = Host(vb, heap);
    if (!Readable(h, 40) || Be(h + 4) != np) continue;
    char v32[32], v20[32], v24[32], v28[32];
    FormatVal(v32, sizeof(v32), tag, Be(img + o + 32));
    FormatVal(v20, sizeof(v20), tag, Be(img + o + 20));
    FormatVal(v24, sizeof(v24), tag, Be(img + o + 24));
    FormatVal(v28, sizeof(v28), tag, Be(img + o + 28));
    std::fprintf(f, "%s\t%u\t%08X\t%08X\t%s\t%s\t%s\t%s\t%08X\n",
                 reinterpret_cast<const char*>(img + (np - kImgBase)), tag, kImgBase + o, heap, v32,
                 v20, v24, v28, Be(img + o + 32));
    ++count;
  }
  std::fprintf(f, "# %u objects, %lu ms\n", count, (unsigned long)(GetTickCount() - t0));
  std::fclose(f);
  g_last_ms.store(unsigned(GetTickCount() - t0));
  g_state.store(2);
}
}  // namespace

void CatalogAsync() {
  if (!Enabled() || Busy()) return;
  g_state.store(1);
  std::thread([] { CatalogWorker(); }).detach();
}

// ---------------------------------------------------------------- RE-1 sweep (round 3)
// Rounds 1-2 (2026-10-29): the hip-check family (sub_83411E90 ...) and all 15 player-event
// handlers (gEvtValPE_*, file 44) NEVER fired, in an exhibition and in a BAP game. The hook
// method itself is fine: hook_refs.txt shows the function table registers `sub_X`, so direct
// AND indirect calls reach the override (sub_83411E90 has 3 direct callers and still never ran).
// Round 3 hooks EVERY function (141) that statically reads a hit / check / balance / fall /
// collision tunable (TUN-9 readers, filtered; list + reasons in docs/re-findings.md). The log
// has one line per second with the per-function call deltas (only non-zero ones, by address),
// plus up to 2 call lines per second for functions that are NOT per-frame (< 30 calls in the
// previous second). Hit code = functions whose counts jump at the owner's marked hits.
namespace {
const char* const kHooks[] = {
  "828B9258",
  "828CC300",
  "828FEBD8",
  "82929FB0",
  "8292AE50",
  "8292B550",
  "8292B808",
  "82936F28",
  "829380A0",
  "8293A148",
  "8293A3E8",
  "8293A998",
  "8293AB78",
  "82A3B770",
  "82DBEFB0",
  "82DBF2A0",
  "83310E68",
  "8331B8F0",
  "83328180",
  "83339558",
  "8333F9C0",
  "833442E8",
  "8334F930",
  "833A1F40",
  "833B90E0",
  "833B9730",
  "833B9AE8",
  "833BF998",
  "833C09E8",
  "833CA500",
  "833E0BF8",
  "833E3D58",
  "833EBD80",
  "833F27E8",
  "833F3330",
  "833F8AC8",
  "8340C690",
  "8340C7E0",
  "8340DB90",
  "83411AD0",
  "83412298",
  "83427068",
  "83429800",
  "834299F0",
  "83429EC8",
  "8342A2A8",
  "8342A938",
  "8342B290",
  "8342B8C8",
  "8342C628",
  "8342CDD0",
  "83437DF8",
  "834551C8",
  "834553D8",
  "83459350",
  "83465C48",
  "83467988",
  "8346E460",
  "83470DF8",
  "83470E48",
  "83476288",
  "834859E0",
  "834BF428",
  "834D8580",
  "834E9FA8",
  "834F0A48",
  "834F24E0",
  "835038B8",
  "83505630",
  "8350E600",
  "8351AE18",
  "8351B5A0",
  "8351B6D0",
  "83522198",
  "83524F60",
  "835334A0",
  "83552AB8",
  "8357F748",
  "8357F9A0",
  "8357FAC0",
  "83580BF8",
  "83580CE8",
  "83580E90",
  "83581208",
  "83582C90",
  "835831D0",
  "835834E0",
  "835844B0",
  "83585028",
  "835853A8",
  "835888E8",
  "83588BF8",
  "83588ED8",
  "83589190",
  "83589570",
  "83589B38",
  "8358A348",
  "8358D100",
  "8358D310",
  "8358D478",
  "8358DBD0",
  "8358DFC0",
  "8358EB88",
  "8358EE30",
  "83593FC8",
  "83594968",
  "83594BD0",
  "83594CB8",
  "83595320",
  "83596B38",
  "83597360",
  "83597FF0",
  "83599B68",
  "8359A1D0",
  "8359B378",
  "8359B600",
  "8359B7C8",
  "8359BA08",
  "8359BF10",
  "8359C3F8",
  "8359C700",
  "8359FAF0",
  "8359FE68",
  "835A06E8",
  "835A1640",
  "835A18C0",
  "835A2338",
  "835A3618",
  "835A4258",
  "835A47D0",
  "835A4BE0",
  "835A5F10",
  "835A62E8",
  "835A6CE0",
  "835A7078",
  "835A8690",
  "835A98D0",
  "835BD388",
  "835CF688",
  "835E7990",
  "835EB920"};
constexpr int kNumHooks = int(sizeof(kHooks) / sizeof(kHooks[0]));
std::atomic<unsigned> g_hook_calls[kNumHooks];
std::atomic<unsigned> g_hook_prev[kNumHooks];   // calls in the previous second
std::atomic<unsigned> g_hook_sec[kNumHooks];
std::atomic<int> g_hook_sec_n[kNumHooks];
std::atomic<bool> g_flusher{false};
std::mutex g_hook_mu;

void HookLog(const char* line) {
  std::lock_guard<std::mutex> lk(g_hook_mu);
  const auto path = rex::filesystem::GetExecutableFolder() / "rogue_probe.txt";
  if (FILE* f = std::fopen(path.string().c_str(), "a")) { std::fputs(line, f); std::fclose(f); }
}

void StartFlusher() {
  bool expected = false;
  if (!g_flusher.compare_exchange_strong(expected, true)) return;
  std::thread([] {
    static unsigned last[kNumHooks] = {};
    static char buf[8192];
    for (;;) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      int w = std::snprintf(buf, sizeof(buf), "hooks tick=%lu", (unsigned long)GetTickCount());
      bool any = false;
      for (int i = 0; i < kNumHooks; ++i) {
        const unsigned now = g_hook_calls[i].load();
        const unsigned d = now - last[i];
        g_hook_prev[i].store(d);
        last[i] = now;
        if (!d) continue;
        any = true;
        if (w < int(sizeof(buf)) - 32) w += std::snprintf(buf + w, sizeof(buf) - w, " %s=%u", kHooks[i], d);
      }
      if (any) { std::snprintf(buf + w, sizeof(buf) - w, "\n"); HookLog(buf); }
    }
  }).detach();
}
}  // namespace

void HookHit(int i, uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6, uint32_t r7,
             unsigned long long lr) {
  if (!Enabled()) return;
  g_hook_calls[i].fetch_add(1, std::memory_order_relaxed);
  StartFlusher();
  if (g_hook_prev[i].load(std::memory_order_relaxed) >= 30) return;  // per-frame: counts only
  const unsigned sec = unsigned(GetTickCount() / 1000u);
  if (g_hook_sec[i].exchange(sec) != sec) g_hook_sec_n[i].store(0);
  if (g_hook_sec_n[i].fetch_add(1) >= 2) return;
  char buf[256];
  std::snprintf(buf, sizeof(buf), "call %s tick=%lu r3=%08X r4=%08X r5=%08X r6=%08X r7=%08X lr=%08llX\n",
                kHooks[i], (unsigned long)GetTickCount(), r3, r4, r5, r6, r7, lr);
  HookLog(buf);
}

void Mark() {
  if (!Enabled()) return;
  char buf[96];
  std::snprintf(buf, sizeof(buf), "MARK tick=%lu\n", (unsigned long)GetTickCount());
  HookLog(buf);
}
}  // namespace rogue::probe

// Strong overrides of the generated functions (same pattern as src/diag_hooks.cpp).
#define ROGUE_PROBE_HOOK(addr, idx)                                                     \
  REX_EXTERN(__imp__sub_##addr);                                                        \
  extern "C" REX_FUNC(sub_##addr) {                                                     \
    rogue::probe::HookHit(idx, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, \
                          (unsigned long long)ctx.lr);                                  \
    __imp__sub_##addr(ctx, base);                                                       \
  }
ROGUE_PROBE_HOOK(828B9258, 0)  // gSpeedBoostRatioForMinBalanceLoss
ROGUE_PROBE_HOOK(828CC300, 1)  // gAccelToProhibitForwardTurns, gBalanceCarrierFallThreshold
ROGUE_PROBE_HOOK(828FEBD8, 2)  // gEffectiveMinEnergy, gMinEnergyForNoEffectPK
ROGUE_PROBE_HOOK(82929FB0, 3)  // gAlwaysUpdateBalanceRatio, gBalanceAdjustmentEnd
ROGUE_PROBE_HOOK(8292AE50, 4)  // gHitBasedLengthCoefficient
ROGUE_PROBE_HOOK(8292B550, 5)  // gCollisionBodyTension
ROGUE_PROBE_HOOK(8292B808, 6)  // gContactCOMDelta, gDefaultCom
ROGUE_PROBE_HOOK(82936F28, 7)  // gHitBasedBlacoutCoefficient, gHitBasedLengthCoefficient
ROGUE_PROBE_HOOK(829380A0, 8)  // gClampDeltaAngle, gDrivenFallRelaxedModifier
ROGUE_PROBE_HOOK(8293A148, 9)  // gCollisionVelocityCompensationValue
ROGUE_PROBE_HOOK(8293A3E8, 10)  // gBalanceMax, gBalanceReactionMax
ROGUE_PROBE_HOOK(8293A998, 11)  // gStumbleVelocityCompensationValue
ROGUE_PROBE_HOOK(8293AB78, 12)  // gStumbleBodyTension
ROGUE_PROBE_HOOK(82A3B770, 13)  // gDefSkaterBalance, gDefSkaterBodyChecking
ROGUE_PROBE_HOOK(82DBEFB0, 14)  // gBoardHitIntensityScale
ROGUE_PROBE_HOOK(82DBF2A0, 15)  // gPlayerHitIntensityScale
ROGUE_PROBE_HOOK(83310E68, 16)  // gHandlePotentialKnockedOverPenaltiesTime, gHandlePotentialNotKnockedOverPenaltiesTime
ROGUE_PROBE_HOOK(8331B8F0, 17)  // gKnockDownValue
ROGUE_PROBE_HOOK(83328180, 18)  // gBoardPinchNoCollisionFrames
ROGUE_PROBE_HOOK(83339558, 19)  // gMinPushAwayScale
ROGUE_PROBE_HOOK(8333F9C0, 20)  // gBoardCollisionStressForMaxEffect, gBoardCollisionStressForMinEffect
ROGUE_PROBE_HOOK(833442E8, 21)  // gSlowFallenGuysAfterThis
ROGUE_PROBE_HOOK(8334F930, 22)  // gAugmentHipCheckYVelocity
ROGUE_PROBE_HOOK(833A1F40, 23)  // gLayDownCollisionRadiusPadInches
ROGUE_PROBE_HOOK(833B90E0, 24)  // gDirectHitForMax, gDirectHitForMin
ROGUE_PROBE_HOOK(833B9730, 25)  // gGPPhysicalPreparednessForShooting
ROGUE_PROBE_HOOK(833B9AE8, 26)  // gGPFallMax, gGPFallMin
ROGUE_PROBE_HOOK(833BF998, 27)  // gMaxLowHitRatioForEffect, gMinLowHitRatioForEffect
ROGUE_PROBE_HOOK(833C09E8, 28)  // gHitInjuryChanceMultiplier
ROGUE_PROBE_HOOK(833CA500, 29)  // gBoardsCheckingEffectiveRating, gGPStumbleMin
ROGUE_PROBE_HOOK(833E0BF8, 30)  // gGPFallMin
ROGUE_PROBE_HOOK(833E3D58, 31)  // gBaseHitComparisonPriority
ROGUE_PROBE_HOOK(833EBD80, 32)  // gHitsForIntimidatingShift
ROGUE_PROBE_HOOK(833F27E8, 33)  // gBigHitterMinHitPct
ROGUE_PROBE_HOOK(833F3330, 34)  // gTeamHitsMinHits
ROGUE_PROBE_HOOK(833F8AC8, 35)  // gDesperationNoCollisionFrames
ROGUE_PROBE_HOOK(8340C690, 36)  // gPhysicsRatingsDiffMax, gPhysicsRatingsDiffMin
ROGUE_PROBE_HOOK(8340C7E0, 37)  // gSpecialCaseModifierShooting
ROGUE_PROBE_HOOK(8340DB90, 38)  // gBodyTensionBase
ROGUE_PROBE_HOOK(83411AD0, 39)  // gWeightOfRelativeSpeedToTarget, gWeightOfSpeedToTarget
ROGUE_PROBE_HOOK(83412298, 40)  // gCheckeeDefaultBalance, gCheckerDefaultBalance
ROGUE_PROBE_HOOK(83427068, 41)  // gBodyTensionBase
ROGUE_PROBE_HOOK(83429800, 42)  // gBothBehaviorBalanceTunerMax, gBothBehaviorBalanceTunerMid
ROGUE_PROBE_HOOK(834299F0, 43)  // gBalanceInequality, gBodyTensionBase
ROGUE_PROBE_HOOK(83429EC8, 44)  // gBalanceInequality
ROGUE_PROBE_HOOK(8342A2A8, 45)  // gBothBehaviorBalanceTunerMax, gBothBehaviorBalanceTunerMid
ROGUE_PROBE_HOOK(8342A938, 46)  // gDefaultBalanceThreshold, gDefaultContactCOMRatio
ROGUE_PROBE_HOOK(8342B290, 47)  // gPlayerPlayerDistanceToEnable
ROGUE_PROBE_HOOK(8342B8C8, 48)  // gPlayerPlayerDistanceToEnable
ROGUE_PROBE_HOOK(8342C628, 49)  // gHipCheckContactCOMRatio, gHipCheckNoVelocityUpdateFrames
ROGUE_PROBE_HOOK(8342CDD0, 50)  // gPlayerPlayerDistanceToEnable
ROGUE_PROBE_HOOK(83437DF8, 51)  // gDefaultBalance, gHipCheckRestartWithLowControlPenalty
ROGUE_PROBE_HOOK(834551C8, 52)  // gBodyTensionBase
ROGUE_PROBE_HOOK(834553D8, 53)  // gBodyTensionBase
ROGUE_PROBE_HOOK(83459350, 54)  // gDegreesForFullLowHipSwing, gHipCheckLowIntentMinimalDistance
ROGUE_PROBE_HOOK(83465C48, 55)  // gMaxFatigue, gRightSickFramesForHold
ROGUE_PROBE_HOOK(83467988, 56)  // gBodyTensionBase
ROGUE_PROBE_HOOK(8346E460, 57)  // gBodyTensionBase
ROGUE_PROBE_HOOK(83470DF8, 58)  // gBodyTensionBase
ROGUE_PROBE_HOOK(83470E48, 59)  // gBodyTensionBase
ROGUE_PROBE_HOOK(83476288, 60)  // gAITuneAIHittingTunePenInjuryModifier, gAITuneAIHittingTunePenPlayoffsModifier
ROGUE_PROBE_HOOK(834859E0, 61)  // gPenaltyChanceCrossChecking, gPenaltyChanceHitIntoBoards
ROGUE_PROBE_HOOK(834BF428, 62)  // gBFFallOverMaxThreatDist, gChanceToFallOver
ROGUE_PROBE_HOOK(834D8580, 63)  // gPlayPhysical_Hits, gPlayPhysical_WasHit
ROGUE_PROBE_HOOK(834E9FA8, 64)  // gBalancePenalty_GlobalScale, gBalancePenalty_HitBumped
ROGUE_PROBE_HOOK(834F0A48, 65)  // gHitterMaxDistToDestEffort, gHitterMaxDistToTarget
ROGUE_PROBE_HOOK(834F24E0, 66)  // gManualHipCheckWinHeightFactor, gSpeedBasedHipCheckProportion
ROGUE_PROBE_HOOK(835038B8, 67)  // gBoardPlayFootCollisionRadius
ROGUE_PROBE_HOOK(83505630, 68)  // gBodyTensionBase
ROGUE_PROBE_HOOK(8350E600, 69)  // gBoardCollisionAnticipationTime
ROGUE_PROBE_HOOK(8351AE18, 70)  // gAutoHitAimingAtTargetDist, gAutoHitEarlyHitFrames
ROGUE_PROBE_HOOK(8351B5A0, 71)  // gAutoHitDefaultCancelActionRefractoryPeriod
ROGUE_PROBE_HOOK(8351B6D0, 72)  // gMinRelSpeedForAutoCheck, gMinSpeedToCareAboutRelSpeedForAutoCheck
ROGUE_PROBE_HOOK(83522198, 73)  // gAggressiveCheckButtonTap
ROGUE_PROBE_HOOK(83524F60, 74)  // gAutoCheckCancelDelay, gHipCheckRefractoryTime
ROGUE_PROBE_HOOK(835334A0, 75)  // gPairUpAutoHitChance, gPairUpAutoHitChanceSuperstarVictim
ROGUE_PROBE_HOOK(83552AB8, 76)  // gCheckingContinueAngle
ROGUE_PROBE_HOOK(8357F748, 77)  // gCheckSpeedModerate, gCheckSpeedSlow
ROGUE_PROBE_HOOK(8357F9A0, 78)  // gEndorphineHitInjuryTestFrequency
ROGUE_PROBE_HOOK(8357FAC0, 79)  // gMaxDistForHit
ROGUE_PROBE_HOOK(83580BF8, 80)  // gHowDoIStepUpGuarding
ROGUE_PROBE_HOOK(83580CE8, 81)  // gStepUp_Shove_IncludeBackAnglePies
ROGUE_PROBE_HOOK(83580E90, 82)  // gHowDoIStepUpShove
ROGUE_PROBE_HOOK(83581208, 83)  // gDeflectionNoCollisionFrames
ROGUE_PROBE_HOOK(83582C90, 84)  // gBenchSitHitReactXOffset
ROGUE_PROBE_HOOK(835831D0, 85)  // gBenchHitBoardDist, gBenchHitReactionDist
ROGUE_PROBE_HOOK(835834E0, 86)  // gBenchHitReactionDist
ROGUE_PROBE_HOOK(835844B0, 87)  // gHookReactTimeScale, gTripFallFrequency
ROGUE_PROBE_HOOK(83585028, 88)  // gHitAssistAccelScale, gHitAssistMaxSpeedScale
ROGUE_PROBE_HOOK(835853A8, 89)  // gAssumedHitterSpeed
ROGUE_PROBE_HOOK(835888E8, 90)  // gMaxFramesToAllowCheckingLayer
ROGUE_PROBE_HOOK(83588BF8, 91)  // gBiasedStrengthSizeAvg, gHitTune_GlobalScale
ROGUE_PROBE_HOOK(83588ED8, 92)  // gMaxCheckIntentForHipCheck
ROGUE_PROBE_HOOK(83589190, 93)  // gEndorphineBoardHitSizeDiff
ROGUE_PROBE_HOOK(83589570, 94)  // gBackPieHitSpeedThreshold, gCheckCollisionRadiusMoving
ROGUE_PROBE_HOOK(83589B38, 95)  // gBigHitIntimidation, gBigHitIntimidationReduction
ROGUE_PROBE_HOOK(8358A348, 96)  // gTimeAfterReleaseForHitToCount
ROGUE_PROBE_HOOK(8358D100, 97)  // gTriggerCancelDist
ROGUE_PROBE_HOOK(8358D310, 98)  // gMaxDistToDestForHitCancelInMeters
ROGUE_PROBE_HOOK(8358D478, 99)  // gInterceptTimeForCheckIntentTrigger, gMaxDistToDestForHitInMeters
ROGUE_PROBE_HOOK(8358DBD0, 100)  // gGuardingActionEffort
ROGUE_PROBE_HOOK(8358DFC0, 101)  // gIZGuardingActionEffort
ROGUE_PROBE_HOOK(8358EB88, 102)  // gFinishChecks_MaxTimeBuffer, gFinishChecks_MaxUndisciplinedLateHit
ROGUE_PROBE_HOOK(8358EE30, 103)  // gExtraShoveDistanceScale, gStepUp_Shove_DistFromBoards
ROGUE_PROBE_HOOK(83593FC8, 104)  // gElbowLikelihoodScale
ROGUE_PROBE_HOOK(83594968, 105)  // gDistForOpponentToBeConsideredCheckable, gMinNumFramesBetweenHit
ROGUE_PROBE_HOOK(83594BD0, 106)  // gDefaultBalance
ROGUE_PROBE_HOOK(83594CB8, 107)  // gBenchHitMaxCheckeeSpeed, gEndorphineBoardHitMinCheckingRating
ROGUE_PROBE_HOOK(83595320, 108)  // gBoardplayGameEventFrame, gBoardplaySizeDiffCheckExit
ROGUE_PROBE_HOOK(83596B38, 109)  // gPlayerFaceDiffMaxDegrees, gPlayerSizeDiffMax
ROGUE_PROBE_HOOK(83597360, 110)  // gMomentumMaintainedWin, gMomentumPhysicsCheckingAttributeWeightBoost
ROGUE_PROBE_HOOK(83597FF0, 111)  // gMaxFramesSinceLastTouchForNoPenalty
ROGUE_PROBE_HOOK(83599B68, 112)  // gCheckingActionAngleToFacing, gCheckingActionEffort
ROGUE_PROBE_HOOK(8359A1D0, 113)  // gNearBoardsHighSlotCheckAngle
ROGUE_PROBE_HOOK(8359B378, 114)  // gHowDoIStepUpPoking
ROGUE_PROBE_HOOK(8359B600, 115)  // gHowDoIStepUpBoardPin, gStepUp_BoardPin_DefMaxSpeed
ROGUE_PROBE_HOOK(8359B7C8, 116)  // gShoveFrequency, gShoveMinScore
ROGUE_PROBE_HOOK(8359BA08, 117)  // gChecking_NoSwitchTargetDist, gStepUp_SortMethod
ROGUE_PROBE_HOOK(8359BF10, 118)  // gCheckingHumanPrevCarrierTicks
ROGUE_PROBE_HOOK(8359C3F8, 119)  // gStepUpPossibleHitMaxDistApart, gStepUpPossibleHitPlayerReach
ROGUE_PROBE_HOOK(8359C700, 120)  // gChecking_SlowSpeedDecline, gChecking_SlowSpeedDeclineRadiusMax
ROGUE_PROBE_HOOK(8359FAF0, 121)  // gDefaultBalance
ROGUE_PROBE_HOOK(8359FE68, 122)  // gBracingPenalty, gFacingAngleDifferenceWeight
ROGUE_PROBE_HOOK(835A06E8, 123)  // gDigAnimPctPenaltyCheckEnd, gDigAnimPctPenaltyCheckStart
ROGUE_PROBE_HOOK(835A1640, 124)  // gSlashActionEffort
ROGUE_PROBE_HOOK(835A18C0, 125)  // gCheckingFromBehind_DistReq, gCheckingFromBehind_SpeedDiff
ROGUE_PROBE_HOOK(835A2338, 126)  // gHowDoIStepUpMinValidScore
ROGUE_PROBE_HOOK(835A3618, 127)  // gMissedBoardhitDelayFrames
ROGUE_PROBE_HOOK(835A4258, 128)  // gAllowHitFromBehindBoardDist, gHitTune_ClosingSpeedForMax
ROGUE_PROBE_HOOK(835A47D0, 129)  // gMinStressForCoincidental
ROGUE_PROBE_HOOK(835A4BE0, 130)  // gMaxForceDistBoardsDistRatio, gMinHeightForBoarding
ROGUE_PROBE_HOOK(835A5F10, 131)  // gSlashHighChance
ROGUE_PROBE_HOOK(835A62E8, 132)  // gChecking_SlowSpeedDeclineProbMax
ROGUE_PROBE_HOOK(835A6CE0, 133)  // gSmallHitIntimidation
ROGUE_PROBE_HOOK(835A7078, 134)  // gFinishChecks_MaxTimeBuffer, gInstigationChanceIntimidation
ROGUE_PROBE_HOOK(835A8690, 135)  // gBenchHitResultVel, gBoardplayAngleDiff
ROGUE_PROBE_HOOK(835A98D0, 136)  // gMaxFramesToFindCollidingPlayer
ROGUE_PROBE_HOOK(835BD388, 137)  // gDeviation_FallingRecoveryTime
ROGUE_PROBE_HOOK(835CF688, 138)  // gMinPushAwayScale
ROGUE_PROBE_HOOK(835E7990, 139)  // gBoardCollisionLeadTime, gHitLossChancePerFrame
ROGUE_PROBE_HOOK(835EB920, 140)  // gNoPlayerCollisionFrames, gPlayerCollisionClosestPlayerDist
