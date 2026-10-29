#include "rogue/rogue_mod.h"
#include <chrono>
#include <cstdlib>
#include "rogue/fx.h"
#include "rogue/perks.h"
#include "rogue/reactions.h"
#include "rogue/run_state.h"
#include "rogue/tunable_patch.h"

namespace rogue {
namespace {
TunablePatcher* g_patcher = nullptr;
bool g_in_game = false;
bool g_pending_apply = false;  // BAP game started before the tweak index was ready
const bool g_enabled = [] { const char* e = std::getenv("NHL_ROGUE"); return e && *e == '1'; }();
constexpr uint32_t kReactionLayer = 0xFFFF0000u;
std::chrono::steady_clock::time_point g_t0;

uint32_t LayerId(size_t perk_idx, int stack) { return uint32_t(perk_idx * 16 + stack); }

// Persistent layers: owned perks (non-scoped) + the league's heat reaction.
void ApplyLayers() {
  if (!g_patcher) return;
  if (!g_patcher->Ready()) { g_pending_apply = true; g_patcher->Index(); return; }
  g_pending_apply = false;
  g_patcher->RevertAll();
  auto& run = Run().run();
  for (size_t i = 0; i < run.perks.size(); ++i) {
    auto* p = FindPerk(run.perks[i].id.c_str());
    if (!p || p->scope == Scope::kBapScoped) continue;
    for (int s = 0; s < run.perks[i].stacks; ++s) g_patcher->AddLayer(LayerId(i, s), p->mods);
  }
  g_patcher->AddLayer(kReactionLayer, ReactionMods(Run().Tier()));
}
}  // namespace

bool Enabled() { return g_enabled; }
TunablePatcher* Patcher() { return g_patcher; }
bool InBapGame() { return g_enabled && g_in_game && Run().run().active; }

// The parent store is no longer used for perks (it edits type tags, see tweak_index.h); the
// parameter stays so the parent wiring in nhllegacy_app.h does not change.
void Init(nhl::ui::ITunableStore* /*store*/) {
  if (!g_enabled) return;
  g_patcher = new TunablePatcher();
  g_t0 = std::chrono::steady_clock::now();
  Run().Load();
}

void Tick() {
  if (!g_enabled || !g_patcher) return;
  if (!g_patcher->Ready()) {
    // Build only after tweak registration (~12 s after boot) or when a game needs it.
    if (g_pending_apply || std::chrono::steady_clock::now() - g_t0 > std::chrono::seconds(20))
      g_patcher->Index();
    return;
  }
  if (g_pending_apply && g_in_game) ApplyLayers();
}

void OnBapGameStart() {
  if (!g_enabled || !Run().run().active) return;
  g_in_game = true;
  Run().run().live = GameStats{};
  ApplyLayers();
}

void OnBapGameEnd(const GameStats& final_stats) {
  if (!g_enabled) return;
  g_in_game = false;
  g_pending_apply = false;
  if (g_patcher) g_patcher->RevertAll();  // never leak perks into other modes
  GameStats g = final_stats;
  g.knockdowns = Run().run().live.knockdowns;  // hook-counted
  g.crit_slaps = Run().run().live.crit_slaps;
  const HeatTier before = Run().Tier();
  Run().OnGameFinished(g);
  if (Run().Tier() > before) fx::Push(fx::Kind::kTierUp, HeatTierName(Run().Tier()));
  OpenShop();
}
}  // namespace rogue
