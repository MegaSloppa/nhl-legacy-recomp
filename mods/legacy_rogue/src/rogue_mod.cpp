#include "rogue/rogue_mod.h"
#include <cstdlib>
#include "rogue/fx.h"
#include "rogue/perks.h"
#include "rogue/reactions.h"
#include "rogue/run_state.h"
#include "rogue/tunable_patch.h"

namespace rogue {
namespace {
nhl::ui::ITunableStore* g_store = nullptr;
TunablePatcher* g_patcher = nullptr;
bool g_in_game = false;
const bool g_enabled = [] { const char* e = std::getenv("NHL_ROGUE"); return e && *e == '1'; }();
constexpr uint32_t kReactionLayer = 0xFFFF0000u;

uint32_t LayerId(size_t perk_idx, int stack) { return uint32_t(perk_idx * 16 + stack); }

// Persistent layers: owned perks (non-scoped) + the league's heat reaction.
void ApplyLayers() {
  if (!g_patcher || !g_patcher->Ready()) return;
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

TunablePatcher* Patcher() { return g_patcher; }
bool InBapGame() { return g_enabled && g_in_game && Run().run().active; }

void Init(nhl::ui::ITunableStore* store) {
  if (!g_enabled) return;
  g_store = store;
  g_patcher = new TunablePatcher(store);
  Run().Load();
}

void Tick() {
  if (!g_enabled || !g_store) return;
  if (!g_patcher->Ready()) {
    g_store->RequestBuild();  // idempotent; scan runs on a worker
    if (g_patcher->Index() && g_in_game) ApplyLayers();
  }
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
