// Legacy Rogue - ImGui panel inside the enhancements overlay (Guide/F1).
// Until lifecycle/stat hooks exist, the "Manual" section stands in for them.
#include <imgui.h>
#include <cstdio>
#include <vector>
#include "rogue/fx.h"
#include "rogue/perks.h"
#include "rogue/rogue_mod.h"
#include "rogue/run_state.h"

namespace rogue {
namespace {
bool g_shop_open = false;
std::vector<ShopOffer> g_offer;
GameStats g_manual;
char g_career[64] = "career-1";
int g_won = 1;
const char* RarityName(Rarity r) {
  static const char* n[] = {"Common", "Rare", "Epic", "Legendary"};
  return n[int(r)];
}
}  // namespace

void OpenShop() {
  if (!Run().run().active) return;
  g_offer = Run().RollShop();
  g_shop_open = !g_offer.empty();
}

void DrawPanel() {
  if (!ImGui::CollapsingHeader("Legacy Rogue (Be-A-Pro)")) return;
  auto& r = Run().run();
  auto& m = Run().meta();
  ImGui::Text("Legacy: %d   Best career score: %d", m.legacy, m.best_legacy_score);
  if (!r.active) {
    ImGui::InputText("Career name", g_career, sizeof(g_career));
    if (ImGui::Button("Start Legacy Rogue career")) {
      Run().StartCareer(g_career, uint32_t(ImGui::GetTime() * 1000));
      OpenShop();
    }
    return;
  }
  ImGui::Text("%s  |  Season %d, game %d  |  Hype %d", r.career_id.c_str(), r.season, r.games, r.hype);
  ImGui::Text("Notoriety %d/1000  [%s]  Hype x%.2f", r.notoriety, HeatTierName(Run().Tier()),
              Run().HypeMultiplier());
  ImGui::ProgressBar(r.notoriety / 1000.f, ImVec2(-1, 0));
  ImGui::Text("This game: %d knockdowns, %d crit slaps   Career score: %d", r.live.knockdowns,
              r.live.crit_slaps, Run().LegacyScore());
  ImGui::SeparatorText("Perks");
  for (auto& o : r.perks)
    if (auto* p = FindPerk(o.id.c_str()))
      ImGui::BulletText("%s x%d  %s%s", p->name, o.stacks, p->blurb, p->is_curse ? "  [CURSE]" : "");

  if (g_shop_open) {
    ImGui::SeparatorText(Run().PendingFreePick() ? "Perk Shop (free pick available!)" : "Perk Shop");
    for (size_t i = 0; i < g_offer.size(); ++i) {
      auto& o = g_offer[i];
      ImGui::PushID(int(i));
      char label[96];
      if (o.free) snprintf(label, sizeof(label), "%s (FREE)", o.perk->name);
      else if (o.price < 0) snprintf(label, sizeof(label), "%s (+%d Hype)", o.perk->name, -o.price);
      else snprintf(label, sizeof(label), "%s (%d Hype)", o.perk->name, o.price);
      ImGui::BeginDisabled(!o.free && o.price > r.hype);
      if (ImGui::Button(label, ImVec2(260, 0)) && Run().Buy(o)) {
        fx::Push(fx::Kind::kPerkBought, o.perk->name);
        g_offer.erase(g_offer.begin() + i);
        ImGui::EndDisabled(); ImGui::PopID();
        break;
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::TextDisabled("[%s] %s", RarityName(o.perk->rarity), o.perk->blurb);
      ImGui::PopID();
    }
    char rr[48]; snprintf(rr, sizeof(rr), "Reroll (%d Hype)", Run().RerollCost());
    if (ImGui::Button(rr) && Run().Reroll()) g_offer = Run().RollShop();
    ImGui::SameLine();
    if (ImGui::Button("Done shopping")) g_shop_open = false;
  } else if (ImGui::Button("Open Perk Shop")) {
    OpenShop();
  }

  ImGui::SeparatorText("Manual (until lifecycle/stat hooks are mapped)");
  if (ImGui::Button("BAP game started")) OnBapGameStart();
  ImGui::RadioButton("Won", &g_won, 1); ImGui::SameLine(); ImGui::RadioButton("Lost", &g_won, 0);
  ImGui::SetNextItemWidth(90); ImGui::InputInt("G", &g_manual.goals); ImGui::SameLine();
  ImGui::SetNextItemWidth(90); ImGui::InputInt("A", &g_manual.assists); ImGui::SameLine();
  ImGui::SetNextItemWidth(90); ImGui::InputInt("Hits", &g_manual.hits); ImGui::SameLine();
  ImGui::SetNextItemWidth(90); ImGui::InputInt("+/-", &g_manual.plus_minus);
  if (ImGui::Button("BAP game ended")) {
    g_manual.won = g_won == 1;
    OnBapGameEnd(g_manual);
    g_manual = GameStats{};
  }
  if (ImGui::Button("Test FX: crit")) fx::Push(fx::Kind::kCritSlap);
  ImGui::SameLine();
  if (ImGui::Button("Retire career")) Run().RetireCareer();
}
}  // namespace rogue
