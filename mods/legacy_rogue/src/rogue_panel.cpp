// Legacy Rogue - ImGui panel inside the enhancements overlay (Guide/F1).
// Until lifecycle/stat hooks exist, the "Manual" section stands in for them.
// The overlay itself belongs to the parent project; only THIS section is gated on
// NHL_ROGUE=1 (00-START-HERE rule 1).
#include <imgui.h>
#include <cstdio>
#include <vector>
#include "rogue/fx.h"
#include "rogue/perks.h"
#include "rogue/probe.h"
#include "rogue/rogue_mod.h"
#include "rogue/run_state.h"
#include "rogue/tweak_index.h"
#include "rogue/reactions.h"
#include "rogue/tunable_patch.h"
#include <cstring>

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
// Offers are rolled once per shop visit. After a free pick is spent the remaining
// offers must show their real price again (BUG-1: they stayed FREE, free went negative).
void RefreshFreeFlags() {
  for (auto& o : g_offer) o.free = Run().PendingFreePick() && !o.perk->is_curse;
}
// Live readout of every tunable the owned perks + Notoriety reaction touch: stock -> live value
// in game memory. During a BAP game the live values should differ; after "BAP game ended" they
// should be back to stock. Verifies the whole perk write path without judging gameplay feel.
void DrawPerkEffects() {
  if (!TweakIndex::Get().Ready()) return;
  std::vector<const char*> names;
  auto add = [&](const std::vector<Mod>& mods) {
    for (auto& m : mods) {
      bool dup = false;
      for (auto* n : names) dup = dup || !std::strcmp(n, m.tunable);
      if (!dup) names.push_back(m.tunable);
    }
  };
  for (auto& o : Run().run().perks)
    if (auto* p = FindPerk(o.id.c_str())) add(p->mods);
  add(ReactionMods(Run().Tier()));
  if (names.empty()) return;
  if (!ImGui::TreeNode("Active perk effects (stock -> live)")) return;
  for (auto* n : names) {
    const TweakRef* r = TweakIndex::Get().Find(n);
    if (!r || !ScalarTag(r->tag)) { ImGui::TextDisabled("%s: not found / unsupported", n); continue; }
    const float s = BitsToF(r->stock, r->tag), v = BitsToF(TweakIndex::Get().Read(*r), r->tag);
    if (s != v) ImGui::Text("%s  %.3f -> %.3f", n, s, v);
    else ImGui::TextDisabled("%s  %.3f (stock)", n, s);
  }
  ImGui::TreePop();
}

// RE-1 round 6: recent contacts as the hit watch sees them (newest first), the ME counters and
// the power test.
void PlayerTag(char* out, size_t n, uint32_t va, bool me, bool goalie, bool autom) {
  if (me && autom) std::snprintf(out, n, "ME*");
  else if (me) std::snprintf(out, n, "ME");
  else if (autom) std::snprintf(out, n, "AUTO");
  else if (goalie) std::snprintf(out, n, "G%04X", unsigned(va & 0xFFFFu));
  else std::snprintf(out, n, "%04X", unsigned(va & 0xFFFFu));
}
void DrawHitWatch() {
  ImGui::SeparatorText("Hit watch (RE-1 round 7)");
  ImGui::TextWrapped("Every body contact shows on screen as hitter -> victim. ME = the player you "
                     "claimed (Manual section: That was me). AUTO = the automatic marker being tested; ME* = "
                     "both agree. G = goalie.");
  const probe::YouStats y = probe::YouCounts();
  ImGui::Text("ME so far: %d hits, %d knockdowns of skaters, knocked down %d times", y.hits,
              y.knockdowns, y.knocked_down);
  ImGui::SameLine();
  if (ImGui::SmallButton("Reset counts")) probe::YouReset();
  ImGui::Text("AUTO marker vs claim: agree %d, missed you %d, marked someone else %d  |  "
              "no claim: %d contacts with an AUTO player", y.auto_agree, y.auto_missed,
              y.auto_wrong, y.auto_seen);
  ImGui::TextUnformatted("Power test (ME's hits only):");
  const float k = probe::PowerTest();
  const float opts[] = {1.f, 1.15f, 1.3f, 2.f};
  const char* labels[] = {"off", "x1.15", "x1.3", "x2"};
  for (int i = 0; i < 4; ++i) {
    ImGui::SameLine();
    if (ImGui::RadioButton(labels[i], k == opts[i])) probe::SetPowerTest(opts[i]);
  }
  const int count = probe::HitEventCount();
  if (!count) { ImGui::TextDisabled("No contacts yet."); return; }
  const unsigned long now = probe::TickNow();
  for (int i = 0; i < count; ++i) {
    probe::HitEvent e;
    if (!probe::HitEventGet(i, &e)) break;
    char hn[16], vn[16], st[8];
    PlayerTag(hn, sizeof(hn), e.hitter, e.hitter_me, e.hitter_goalie, e.hitter_auto);
    PlayerTag(vn, sizeof(vn), e.victim, e.victim_me, e.victim_goalie, e.victim_auto);
    if (e.strength < 0) std::snprintf(st, sizeof(st), "-");
    else std::snprintf(st, sizeof(st), "%d", e.strength);
    ImGui::Text("%5.0f s ago  %5s -> %-5s  %-9s  power %.2f%s  str %s  credit %s  fall %s%s",
                double(now - e.tick) / 1000.0, hn, vn, e.knockdown ? "KNOCKDOWN" : "hit",
                double(e.power), e.boosted ? " (boosted)" : "", st, e.credited ? "yes" : "no",
                e.fall ? "yes" : "no", e.counted ? "  [counted]" : "");
  }
}

void DrawDiagnostics() {
  if (!probe::Enabled()) return;
  ImGui::SeparatorText("Diagnostics (NHL_ROGUE_PROBE=1)");
  ImGui::TextWrapped("TUN-4 tunable probe. Read-only. Each button appends one snapshot to "
                     "rogue_probe.txt next to the exe.");
  ImGui::BeginDisabled(probe::Busy());
  if (ImGui::Button("Snapshot: menu")) probe::RunAsync("menu");
  ImGui::SameLine();
  if (ImGui::Button("Snapshot: in match")) probe::RunAsync("in match");
  ImGui::SameLine();
  if (ImGui::Button("Snapshot: after edit")) probe::RunAsync("after edit (GameSpeedLevel=20)");
  if (ImGui::Button("Dump tweak catalog (TUN-9)")) probe::CatalogAsync();
  ImGui::SameLine();
  if (ImGui::Button("Mark log")) probe::Mark();
  ImGui::EndDisabled();
  ImGui::TextDisabled("%s", probe::Status());
  DrawHitWatch();

  ImGui::SeparatorText("Write test (TUN-10 hip checks)");
  ImGui::TextWrapped("Writes the field the game's code reads (or every game-style table entry). "
                     "Restore, or restart the game, to undo. Every write is logged to rogue_probe.txt.");
  for (int i = 0; i < probe::WriteTestCount(); ++i) {
    ImGui::PushID(1000 + i);
    ImGui::TextUnformatted(probe::WriteTestName(i));
    for (int k = 0; k < 3; ++k) {
      ImGui::PushID(k);
      if (ImGui::SmallButton(probe::WriteTestPresetLabel(i, k))) probe::WriteTestApply(i, k);
      ImGui::PopID();
      ImGui::SameLine();
    }
    if (ImGui::SmallButton("Restore")) probe::WriteTestRestore(i);
    char d[192];
    probe::WriteTestDescribe(i, d, sizeof(d));
    ImGui::TextDisabled("%s", d);
    ImGui::PopID();
  }
  if (ImGui::Button("Hip check combo (guess)")) probe::WriteTestHipCombo();
  ImGui::SameLine();
  if (ImGui::Button("Restore all")) probe::WriteTestRestoreAll();
}
}  // namespace

void OpenShop() {
  if (!Run().run().active) return;
  g_offer = Run().RollShop();
  g_shop_open = !g_offer.empty();
}

void DrawPanel() {
  if (!Enabled()) return;  // BUG-2: hide the whole section without NHL_ROGUE=1
  if (!ImGui::CollapsingHeader("Legacy Rogue (Be-A-Pro)")) return;
  auto& r = Run().run();
  auto& m = Run().meta();
  ImGui::Text("Legacy: %d   Best career score: %d", m.legacy, m.best_legacy_score);
  DrawDiagnostics();
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
  DrawPerkEffects();
  ImGui::SeparatorText("Perks");
  if (TweakIndex::Get().Ready())
    ImGui::TextDisabled("Tunable index ready: %u settings", unsigned(TweakIndex::Get().Count()));
  else
    ImGui::TextDisabled("Tunable index not built yet (builds ~20 s after boot)");
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
        RefreshFreeFlags();
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
  // RE-2 is not solved: you tell the mod which player is yours, once per game. During the game,
  // right after a check: "I hit" if your pro delivered the latest contact, "I got hit" if he took it.
  ImGui::BeginDisabled(!InBapGame());
  if (ImGui::Button("That was me: I hit") && LastContactHitter()) {
    SetBapPlayer(LastContactHitter());
    probe::DumpPlayersForClaim(BapPlayer());
  }
  ImGui::SameLine();
  if (ImGui::Button("That was me: I got hit") && LastContactVictim()) {
    SetBapPlayer(LastContactVictim());
    probe::DumpPlayersForClaim(BapPlayer());
  }
  ImGui::EndDisabled();
  if (BapPlayer())
    ImGui::Text("Your pro: %04X  |  this game: %d hits, %d knockdowns (counted by the mod)",
                unsigned(BapPlayer() & 0xFFFFu), r.live.hits, r.live.knockdowns);
  else if (InBapGame())
    ImGui::TextDisabled("Your pro is not claimed yet: make a check, then press That was me.");
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
