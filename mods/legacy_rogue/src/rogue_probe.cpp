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
// Read-only: never writes guest memory. Same committed-memory walk as the parent
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
    case 2: std::snprintf(buf, sizeof(buf), "Done in %u ms: appended to rogue_probe.txt", g_last_ms.load()); return buf;
    case 3: return "Failed: guest memory not available or rogue_probe.txt could not be opened";
    default: return "Idle";
  }
}
}  // namespace rogue::probe
