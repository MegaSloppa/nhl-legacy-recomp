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
#include "rogue/fx.h"
#include "rogue/rogue_mod.h"

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

// ---------------------------------------------------------------- RE-1 hit watch (round 7)
// Round 6 RESULT (owner BAP game, 2026-10-30): the claim works; the power test is deterministic:
// sub_833B9028 walks a 5-entry table at 0x83ABF528 (28 bytes each, threshold float at +4) and
// returns the first entry whose threshold >= power: <= 0.5 entry 0 (0x83ABF528, no hit
// reaction), <= 0.75 entry 3 (0x83ABF57C, hit), above entry 4 (0x83ABF598, knockdown). All 1836
// logged decisions match. The dispatcher and choice hooks moved to rogue_hooks.cpp (perks use
// them) and call ReactionBegin / ReactionPowerIn / ReactionChosen / ReactionEnd here.
// Round 7 tests an automatic BAP-player marker: byte +0x22C of the player object was 0x00 on the
// claimed player and 0xFF on all 20 others (one session); also logged: +0x040, +0x248, +0x3D0.
// CONFIRMED (rounds 3-5): sub_833E0BF8 = hit contact (hooked in rogue_hooks.cpp, forwards to
// ContactSeen), knockdown reaction 0x83ABF598, skater / goalie class pointers.
// Round 5 RESULT (owner, 2026-10-30, exhibition + BAP): byte +0x38 is NOT a human flag. Read after
// the reaction it takes 2..12 on victims (a reaction / animation state), 0 on hitters; the two
// "YOU" knockdowns in BAP were other players. sub_834EA6D0(player, 0) is a TEAM query: 0 for one
// team, 1 for the other (exhibition idx 0-19 -> 0, 20-39 -> 1; BAP idx 0-19 -> 1, 20-39 -> 0).
// Static read of sub_83429560 (reaction_slice.txt), the only caller of the hit contact:
//   power = sub_833CA500(victim, hitter, ...)   (stored at r1+84 = the hit contact's *r5)
//   desc  = sub_833B9028(f1 = power)            (reaction descriptor: 0x83ABF57C / 0x83ABF598)
//   state = sub_83429248(victim, desc+20); != 19 -> sub_8350C728 / sub_83429300 apply it
//   then sub_833E0BF8 (bookkeeping), sub_833FBEA0, sub_8332D4D0(victim+80, 316 / 314, desc+8/+12)
// Round 5 power (58 contacts): knockdowns 0.763-0.962, normal 0.501-0.734 (threshold ~0.75).
// Round 6 (kept): the owner CLAIMS his player once per game (panel: "That was me"), the hit watch tags it
// ME, the claim dumps every player object once (to find a field that marks the human player), and
// an optional TEST multiplies the power the reaction choice sees, for the claimed player's hits
// only (proof that perks can force knockdowns). Pass-through unless the test is switched on.
namespace {
const char* const kHooks[] = {"8357F9A0", "83589B38", "829380A0", "834EA6D0", "828CC300",
                              "833E0BF8", "833B9028"};
enum : int { kCredit = 0, kStrength, kFall, kTeamQ, kSnapshot, kContact, kReaction };
constexpr int kNumHooks = int(sizeof(kHooks) / sizeof(kHooks[0]));
constexpr uint32_t kReactKnockdown = 0x83ABF598u;  // CONFIRMED rounds 3-5
constexpr uint32_t kReactNormal = 0x83ABF57Cu;     // CONFIRMED rounds 3-5
constexpr uint32_t kClassSkater = 0x82271C10u;     // CONFIRMED rounds 4-5
constexpr uint32_t kClassGoalie = 0x8223F648u;     // CONFIRMED rounds 4-5
constexpr uint32_t kOffIndex = 0x39;               // player index byte (sub_833E0BF8 reads +57)
constexpr uint32_t kOffTeam = 0x30;                // team side byte (round 4 dumps)
constexpr uint32_t kOffAuto = 0x22C;               // CANDIDATE: byte 0x00 = BAP / human player, 0xFF = AI
constexpr unsigned long kLinkMs = 1500;            // strength / credit / fall within this
constexpr unsigned long kSameHitMs = 500;          // same victim again = same hit
constexpr int kMaxEvents = 12;
constexpr int kLinesPerSec = 40;
constexpr int kMaxPlayers = 64;
constexpr int kDumpBytes = 1024;

std::atomic<unsigned> g_hook_calls[kNumHooks];
std::atomic<bool> g_flusher{false};
std::atomic<float> g_boost{1.f};  // power test factor for the claimed player's hits
std::mutex g_hook_mu;  // log file
std::mutex g_ev_mu;    // events, ME counts, line budget
std::mutex g_pl_mu;    // player table (from the team query)
unsigned g_line_sec[kNumHooks] = {};
int g_line_n[kNumHooks] = {};
HitEvent g_ev[kMaxEvents];
int g_ev_head = 0, g_ev_count = 0;
YouStats g_you;
uint32_t g_you_last_victim = 0;
unsigned long g_you_last_tick = 0;
struct PlayerEntry { uint32_t player; uint32_t team_q; };
PlayerEntry g_pl[kMaxPlayers];
int g_pl_n = 0;
// Set while sub_83429560 (reaction dispatcher) runs on this thread.
thread_local bool t_in_reaction = false;
thread_local uint32_t t_hitter = 0, t_victim = 0;
thread_local bool t_boosted = false;
thread_local uint32_t t_desc = 0;

void HookLog(const char* line) {
  std::lock_guard<std::mutex> lk(g_hook_mu);
  const auto path = rex::filesystem::GetExecutableFolder() / "rogue_probe.txt";
  if (FILE* f = std::fopen(path.string().c_str(), "a")) { std::fputs(line, f); std::fclose(f); }
}

void StartFlusher() {
  bool expected = false;
  if (!g_flusher.compare_exchange_strong(expected, true)) return;
  HookLog("hitwatch v4 (RE-1 round 7) started\n");
  std::thread([] {
    unsigned last[kNumHooks] = {};
    char buf[512];
    for (;;) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      int w = std::snprintf(buf, sizeof(buf), "hooks tick=%lu", (unsigned long)GetTickCount());
      bool any = false;
      for (int i = 0; i < kNumHooks; ++i) {
        const unsigned now = g_hook_calls[i].load();
        const unsigned d = now - last[i];
        last[i] = now;
        if (!d) continue;
        if (i != kTeamQ) any = true;  // the team query runs all the time: no line on its own
        if (w < int(sizeof(buf)) - 32) w += std::snprintf(buf + w, sizeof(buf) - w, " %s=%u", kHooks[i], d);
      }
      if (any) { std::snprintf(buf + w, sizeof(buf) - w, "\n"); HookLog(buf); }
    }
  }).detach();
}

// Caller holds g_ev_mu.
bool LineBudget(int i) {
  const unsigned sec = unsigned(GetTickCount() / 1000u);
  if (g_line_sec[i] != sec) { g_line_sec[i] = sec; g_line_n[i] = 0; }
  return g_line_n[i]++ < (i == kSnapshot ? 2 : kLinesPerSec);
}
bool Budget(int i) {
  std::lock_guard<std::mutex> lk(g_ev_mu);
  return LineBudget(i);
}

struct PlayerInfo {
  uint32_t cls = 0; int index = -1; int team = -1;
  int auto_byte = -1; uint32_t w040 = 0, w248 = 0, w3d0 = 0;
  bool Auto() const { return auto_byte == 0; }
};
PlayerInfo ReadPlayer(uint32_t va) {
  PlayerInfo p;
  uint8_t* vb = VBase();
  if (!vb || va < 0x40000000u) return p;
  const uint8_t* h = Host(vb, va);
  if (!Readable(h, 0x3D4)) return p;
  p.cls = Be(h);
  p.index = h[kOffIndex];
  p.team = h[kOffTeam];
  p.auto_byte = h[kOffAuto];
  p.w040 = Be(h + 0x40);
  p.w248 = Be(h + 0x248);
  p.w3d0 = Be(h + 0x3D0);
  return p;
}
void Name(char* out, size_t n, uint32_t va, const PlayerInfo& p) {
  const bool me = va && va == rogue::BapPlayer();
  if (me && p.Auto()) std::snprintf(out, n, "ME*");
  else if (me) std::snprintf(out, n, "ME");
  else if (p.Auto()) std::snprintf(out, n, "AUTO");
  else if (p.cls == kClassGoalie) std::snprintf(out, n, "G%04X", unsigned(va & 0xFFFFu));
  else std::snprintf(out, n, "%04X", unsigned(va & 0xFFFFu));
}

// Newest contact within kLinkMs for the pair (any pair if any_pair). Caller holds g_ev_mu.
HitEvent* Recent(uint32_t hitter, uint32_t victim, bool any_pair) {
  const unsigned long now = GetTickCount();
  for (int k = 0; k < g_ev_count; ++k) {
    HitEvent& e = g_ev[(g_ev_head - 1 - k + 2 * kMaxEvents) % kMaxEvents];
    if (now - e.tick > kLinkMs) break;
    if (any_pair || (e.hitter == hitter && e.victim == victim)) return &e;
  }
  return nullptr;
}

void LogCall(int i, uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6, uint32_t r7,
             unsigned long long lr) {
  if (!Budget(i)) return;
  char buf[256];
  std::snprintf(buf, sizeof(buf), "call %s tick=%lu r3=%08X r4=%08X r5=%08X r6=%08X r7=%08X lr=%08llX\n",
                kHooks[i], (unsigned long)GetTickCount(), r3, r4, r5, r6, r7, lr);
  HookLog(buf);
}

void DumpObject(const char* what, uint32_t va) {
  uint8_t* vb = VBase();
  if (!vb || va < 0x40000000u) return;
  const uint8_t* h = Host(vb, va);
  char b[160];
  if (!Readable(h, kDumpBytes)) {
    std::snprintf(b, sizeof(b), "dump %s %08X: not readable\n", what, va);
    HookLog(b);
    return;
  }
  const PlayerInfo p = ReadPlayer(va);
  std::string s;
  std::snprintf(b, sizeof(b), "dump %s %08X idx=%d team=%d class=%08X (%d bytes, big-endian words):\n",
                what, va, p.index, p.team, p.cls, kDumpBytes);
  s += b;
  for (int o = 0; o < kDumpBytes; o += 32) {
    int w = std::snprintf(b, sizeof(b), "  +%03X:", o);
    for (int k = 0; k < 32; k += 4) w += std::snprintf(b + w, sizeof(b) - w, " %08X", Be(h + o + k));
    std::snprintf(b + w, sizeof(b) - w, "\n");
    s += b;
  }
  HookLog(s.c_str());
}
}  // namespace

void ContactSeen(uint32_t victim, uint32_t hitter, uint32_t reaction, float power,
                 unsigned long long lr) {
  if (!Enabled()) return;
  g_hook_calls[kContact].fetch_add(1, std::memory_order_relaxed);
  StartFlusher();
  const uint32_t me = rogue::BapPlayer();
  const PlayerInfo ph = ReadPlayer(hitter), pv = ReadPlayer(victim);
  const bool kd = reaction == kReactKnockdown;
  const bool my_hit = me && hitter == me && victim != me;
  const bool boosted = t_in_reaction && t_boosted && t_victim == victim;
  bool counted = false, counted_kd = false, log;
  {
    std::lock_guard<std::mutex> lk(g_ev_mu);
    const unsigned long now = GetTickCount();
    if (my_hit) {
      counted = !(g_you_last_victim == victim && now - g_you_last_tick < kSameHitMs);
      if (counted) ++g_you.hits;
      g_you_last_victim = victim;
      g_you_last_tick = now;
      if (kd && pv.cls == kClassSkater) { ++g_you.knockdowns; counted_kd = true; }
    }
    if (me && victim == me && kd) ++g_you.knocked_down;
    if (me) {  // auto marker vs claim, for both players of the contact
      for (int k = 0; k < 2; ++k) {
        const uint32_t va = k ? victim : hitter;
        const bool a = (k ? pv : ph).Auto();
        if (va == me) a ? ++g_you.auto_agree : ++g_you.auto_missed;
        else if (a) ++g_you.auto_wrong;
      }
    } else if (ph.Auto() || pv.Auto()) {
      ++g_you.auto_seen;
    }
    HitEvent& e = g_ev[g_ev_head];
    e = HitEvent{};
    e.tick = now;
    e.hitter = hitter;
    e.victim = victim;
    e.reaction = reaction;
    e.knockdown = kd;
    e.power = power;
    e.hitter_me = me && hitter == me;
    e.victim_me = me && victim == me;
    e.hitter_goalie = ph.cls == kClassGoalie;
    e.hitter_auto = ph.Auto();
    e.victim_auto = pv.Auto();
    e.victim_goalie = pv.cls == kClassGoalie;
    e.boosted = boosted;
    e.counted = counted;
    g_ev_head = (g_ev_head + 1) % kMaxEvents;
    if (g_ev_count < kMaxEvents) ++g_ev_count;
    log = LineBudget(kContact);
  }
  if (log) {
    char buf[480];
    std::snprintf(buf, sizeof(buf),
                  "contact tick=%lu victim=%08X hitter=%08X react=%08X power=%.3f hidx=%d vidx=%d "
                  "hteam=%d vteam=%d hclass=%08X vclass=%08X me=%08X boosted=%d counted=%d kd_counted=%d "
                  "hauto=%d vauto=%d h040=%08X v040=%08X h248=%08X v248=%08X h3d0=%08X v3d0=%08X lr=%08llX\n",
                  (unsigned long)GetTickCount(), victim, hitter, reaction, double(power), ph.index,
                  pv.index, ph.team, pv.team, ph.cls, pv.cls, me, int(boosted), int(counted),
                  int(counted_kd), ph.auto_byte, pv.auto_byte, ph.w040, pv.w040, ph.w248, pv.w248,
                  ph.w3d0, pv.w3d0, lr);
    HookLog(buf);
  }
  char hn[16], vn[16], t[112];
  Name(hn, sizeof(hn), hitter, ph);
  Name(vn, sizeof(vn), victim, pv);
  const bool goalie_kd = kd && pv.cls == kClassGoalie;
  std::snprintf(t, sizeof(t), "%s%s%s  %s -> %s",
                kd ? "KNOCKDOWN" : reaction == kReactNormal ? "HIT" : "CONTACT (new kind)",
                goalie_kd ? " (goalie shrugs)" : "", boosted ? " (boosted)" : "", hn, vn);
  fx::Push(kd && !goalie_kd ? fx::Kind::kKnockdown : fx::Kind::kMilestone, t);
}

void HookHit(int i, uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6, uint32_t r7,
             unsigned long long lr) {
  if (!Enabled()) return;
  g_hook_calls[i].fetch_add(1, std::memory_order_relaxed);
  StartFlusher();
  LogCall(i, r3, r4, r5, r6, r7, lr);
  std::lock_guard<std::mutex> lk(g_ev_mu);
  if (i == kCredit) {
    if (HitEvent* e = Recent(r4, r5, false)) e->credited = true;
  } else if (i == kStrength) {
    if (HitEvent* e = Recent(r4, r5, false))
      if (e->strength < 0 && r7 < 16u) e->strength = int(r7);
  } else if (i == kFall) {
    if (HitEvent* e = Recent(0, 0, true)) e->fall = true;
  }
}

// sub_834EA6D0(player, 0) = team query (round 5). Used here to collect every player object.
void TeamQueryResult(uint32_t player, uint32_t arg, uint32_t result, unsigned long long lr) {
  if (!Enabled() || arg != 0u || player < 0x40000000u) return;
  g_hook_calls[kTeamQ].fetch_add(1, std::memory_order_relaxed);
  StartFlusher();
  bool fresh = false;
  {
    std::lock_guard<std::mutex> lk(g_pl_mu);
    int k = 0;
    for (; k < g_pl_n; ++k) if (g_pl[k].player == player) break;
    if (k == g_pl_n) {
      if (g_pl_n >= kMaxPlayers) return;
      g_pl[g_pl_n++] = {player, result};
      fresh = true;
    } else {
      g_pl[k].team_q = result;
    }
  }
  if (!fresh || !Budget(kTeamQ)) return;
  const PlayerInfo p = ReadPlayer(player);
  char buf[200];
  std::snprintf(buf, sizeof(buf), "player new %08X teamq=%u idx=%d team=%d class=%08X lr=%08llX\n",
                player, result & 0xFFu, p.index, p.team, p.cls, lr);
  HookLog(buf);
}

// sub_83429560 (reaction dispatcher): r3 = victim, r4 = hitter.
void ReactionBegin(uint32_t victim, uint32_t hitter) {
  t_in_reaction = true;
  t_victim = victim;
  t_hitter = hitter;
  t_boosted = false;
  t_desc = 0;
}
void ReactionEnd(uint32_t applied) {
  if (Enabled() && t_desc && Budget(kReaction)) {
    char buf[200];
    std::snprintf(buf, sizeof(buf), "reaction-end tick=%lu victim=%08X hitter=%08X desc=%08X applied=%u boosted=%d\n",
                  (unsigned long)GetTickCount(), t_victim, t_hitter, t_desc, applied & 0xFFu, int(t_boosted));
    HookLog(buf);
  }
  t_in_reaction = false;
}
// sub_833B9028 (reaction choice from power): TEST hook. Returns the power the original sees.
double ReactionPowerIn(double power) {
  if (!Enabled() || !t_in_reaction) return power;
  const uint32_t me = rogue::BapPlayer();
  const float k = g_boost.load();
  if (!me || t_hitter != me || t_victim == me || k == 1.f) return power;
  t_boosted = true;
  return power * double(k);
}
void ReactionChosen(double power_in, double power_used, uint32_t desc, unsigned long long lr) {
  if (!Enabled()) return;
  g_hook_calls[kReaction].fetch_add(1, std::memory_order_relaxed);
  StartFlusher();
  if (t_in_reaction) t_desc = desc;
  if (!Budget(kReaction)) return;
  char buf[220];
  std::snprintf(buf, sizeof(buf), "reaction tick=%lu power=%.3f used=%.3f desc=%08X in_dispatch=%d victim=%08X hitter=%08X lr=%08llX\n",
                (unsigned long)GetTickCount(), power_in, power_used, desc, int(t_in_reaction),
                t_in_reaction ? t_victim : 0u, t_in_reaction ? t_hitter : 0u, lr);
  HookLog(buf);
}

void SetPowerTest(float factor) {
  g_boost.store(factor);
  char buf[96];
  std::snprintf(buf, sizeof(buf), "power test x%.2f (claimed player's hits only)\n", double(factor));
  HookLog(buf);
}
float PowerTest() { return g_boost.load(); }

// After a claim: dump the claimed player and every other known player once (1 KB each).
void DumpPlayersForClaim(uint32_t me) {
  if (!Enabled() || !me) return;
  PlayerEntry copy[kMaxPlayers];
  int n;
  {
    std::lock_guard<std::mutex> lk(g_pl_mu);
    n = g_pl_n;
    std::memcpy(copy, g_pl, sizeof(PlayerEntry) * size_t(n));
  }
  char b[96];
  std::snprintf(b, sizeof(b), "CLAIM tick=%lu me=%08X players=%d\n", (unsigned long)GetTickCount(), me, n);
  HookLog(b);
  DumpObject("me", me);
  for (int k = 0; k < n; ++k)
    if (copy[k].player != me) DumpObject("other", copy[k].player);
}

int HitEventCount() {
  std::lock_guard<std::mutex> lk(g_ev_mu);
  return g_ev_count;
}
bool HitEventGet(int i, HitEvent* out) {
  std::lock_guard<std::mutex> lk(g_ev_mu);
  if (!out || i < 0 || i >= g_ev_count) return false;
  *out = g_ev[(g_ev_head - 1 - i + 2 * kMaxEvents) % kMaxEvents];
  return true;
}
YouStats YouCounts() {
  std::lock_guard<std::mutex> lk(g_ev_mu);
  return g_you;
}
void YouReset() {
  std::lock_guard<std::mutex> lk(g_ev_mu);
  g_you = YouStats{};
  g_you_last_victim = 0;
  HookLog("ME counts reset\n");
}
unsigned long TickNow() { return (unsigned long)GetTickCount(); }

void Mark() {
  if (!Enabled()) return;
  char buf[96];
  std::snprintf(buf, sizeof(buf), "MARK tick=%lu\n", (unsigned long)GetTickCount());
  HookLog(buf);
}
}  // namespace rogue::probe

// Strong overrides of the generated functions (same pattern as src/diag_hooks.cpp).
// Pass-through: the original always runs, with the same registers. The hit contact, the reaction
// dispatcher and the reaction choice are hooked in rogue_hooks.cpp.
#define ROGUE_PROBE_HOOK(addr, idx)                                                     \
  REX_EXTERN(__imp__sub_##addr);                                                        \
  extern "C" REX_FUNC(sub_##addr) {                                                     \
    rogue::probe::HookHit(idx, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, \
                          (unsigned long long)ctx.lr);                                  \
    __imp__sub_##addr(ctx, base);                                                       \
  }
ROGUE_PROBE_HOOK(8357F9A0, 0)  // hit credit: r4 hitter, r5 victim (r7 = hitter again)
ROGUE_PROBE_HOOK(83589B38, 1)  // same caller, just before: r4 hitter, r5 victim, r7 strength 0..3 (round 3)
ROGUE_PROBE_HOOK(829380A0, 2)  // fall path: runs only when a skater really goes down
ROGUE_PROBE_HOOK(828CC300, 4)  // skating settings snapshot (TUN-12 cadence)

// Team query (round 5): result read after the original; collects all player objects.
REX_EXTERN(__imp__sub_834EA6D0);
extern "C" REX_FUNC(sub_834EA6D0) {
  const uint32_t player = ctx.r3.u32, arg = ctx.r4.u32;
  const unsigned long long lr = ctx.lr;
  __imp__sub_834EA6D0(ctx, base);
  if (rogue::probe::Enabled()) rogue::probe::TeamQueryResult(player, arg, ctx.r3.u32, lr);
}

// The reaction dispatcher sub_83429560 and the reaction choice sub_833B9028 are hooked in
// rogue_hooks.cpp (ROGUE_ADDR_REACTION_DISPATCH / _CHOICE); they call the Reaction* functions above.
