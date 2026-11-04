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
int g_box_seen = 0;  // RE-5: last box score copied into the result fields
// GameStateName moved to rogue_mod.cpp (rogue_mod.h), BUG-3.
// BUG-3: start / end history, newest first.
void DrawLifecycleLog(const std::vector<LifecycleEvent>& log) {
  if (log.empty()) { ImGui::TextDisabled("(empty)"); return; }
  for (auto it = log.rbegin(); it != log.rend(); ++it)
    ImGui::Text("%7.1f s  %-38s  %-11s match %04X  flag %d  pro %04X", double(it->t), it->what,
                GameStateName(it->state), unsigned(it->match & 0xFFFFu), int(it->bap), unsigned(it->pro & 0xFFFFu));
}
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

// Recent contacts as the hit watch sees them (newest first): hitter -> victim, power, outcome. Kept for
// perk tuning (Heavy Shoulders / Bulldozer floors). 2026-11-04: the round 6-8 extras (AUTO marker,
// marker scan, power test) left the menu; their backends stay off.
void PlayerTag(char* out, size_t n, uint32_t va, bool me, bool goalie) {
  if (me) std::snprintf(out, n, "ME");
  else if (goalie) std::snprintf(out, n, "G%04X", unsigned(va & 0xFFFFu));
  else std::snprintf(out, n, "%04X", unsigned(va & 0xFFFFu));
}
void DrawHitWatch() {
  const probe::YouStats y = probe::YouCounts();
  ImGui::Text("ME (claimed pro) so far: %d hits, %d knockdowns of skaters, knocked down %d times", y.hits,
              y.knockdowns, y.knocked_down);
  ImGui::SameLine();
  if (ImGui::SmallButton("Reset counts")) probe::YouReset();
  const int count = probe::HitEventCount();
  if (!count) { ImGui::TextDisabled("No contacts yet."); return; }
  const unsigned long now = probe::TickNow();
  for (int i = 0; i < count; ++i) {
    probe::HitEvent e;
    if (!probe::HitEventGet(i, &e)) break;
    char hn[16], vn[16];
    PlayerTag(hn, sizeof(hn), e.hitter, e.hitter_me, e.hitter_goalie);
    PlayerTag(vn, sizeof(vn), e.victim, e.victim_me, e.victim_goalie);
    ImGui::Text("%5.0f s ago  %5s -> %-5s  %-9s  power %.2f  fall %s%s", double(now - e.tick) / 1000.0, hn, vn,
                e.knockdown ? "KNOCKDOWN" : "hit", double(e.power), e.fall ? "yes" : "no",
                e.counted ? "  [counted]" : "");
  }
}

// 2026-11-04 trim: removed from the menu (backends kept where live hooks call them, all default off):
// TUN-4 snapshots and the TUN-10 write tests (code removed), penalty immunity test (now the perk
// Untouchable), contact floor test (now Heavy Shoulders / Bulldozer), shockwave tests (parked).
void DrawDiagnostics() {
  if (!probe::Enabled()) return;
  ImGui::SeparatorText("Diagnostics (NHL_ROGUE_PROBE=1)");
  ImGui::TextWrapped("Stat finder (RE-5): watches game memory during a Be-A-Pro game and writes "
                     "rogue_stats_probe.txt next to the exe at game over.");
  ImGui::TextDisabled("%s", probe::StatStatus());
  if (ImGui::Button("Write stat report now")) probe::StatWriteNow();
  ImGui::SameLine();
  if (ImGui::Button("Mark log")) probe::Mark();
  ImGui::SameLine();
  ImGui::BeginDisabled(probe::Busy());
  if (ImGui::Button("Dump tweak catalog (TUN-9)")) probe::CatalogAsync();
  ImGui::EndDisabled();
  ImGui::TextDisabled("Catalog: %s", probe::Status());
  if (ImGui::TreeNode("Recent contacts (hit watch)")) {
    DrawHitWatch();
    ImGui::TreePop();
  }
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

  ImGui::SeparatorText("This game");
  {
    const GuestGame g = PollGuestGame();
    if (!g.valid)
      ImGui::TextDisabled("Game state: not readable yet");
    else
      ImGui::Text("Game state: %s, %s%s", GameStateName(g.state),
                  g.match ? (g.bap ? "Be-A-Pro match" : "match (not Be-A-Pro)") : "no match",
                  InBapGame() ? (AutoStarted() ? "  |  game started automatically" : "  |  game started by hand") : "");
    if (!AutoLifecycle()) ImGui::TextDisabled("Automatic start / end is off (NHL_ROGUE_AUTO=0).");
  }
  if (AwaitingResult()) {
    ImGui::TextColored(ImVec4(1.f, 0.8f, 0.2f, 1.f),
                       "Game over detected, perks are off. Set Won / Lost and your line, then press BAP game ended.");
    if (LastGameOverReason()[0]) ImGui::TextDisabled("Why: %s", LastGameOverReason());
    // BUG-3: if the game is in fact still going on (the mod also does this by itself at the next faceoff).
    if (ImGui::Button("Not over: resume this game (perks back on, counters kept)")) ResumeBapGame();
  }
  if (LifecycleResumes() > 0)
    ImGui::TextColored(ImVec4(1.f, 0.5f, 0.2f, 1.f),
                       "A false game over was undone %d time(s) this session. Please screenshot the start / end log.",
                       LifecycleResumes());
  if (ImGui::TreeNode("Start / end log")) {
    ImGui::TextDisabled("Newest first. Times = seconds since the game was launched.");
    DrawLifecycleLog(LifecycleLog());
    if (!LifecycleLogAtGameOver().empty() && ImGui::TreeNode("At the last game over")) {
      DrawLifecycleLog(LifecycleLogAtGameOver());
      ImGui::TreePop();
    }
    ImGui::TreePop();
  }
  if (!InBapGame() && ImGui::Button("BAP game started (only if it did not start by itself)")) {
    OnBapGameStart();
    probe::ResetPlayers();  // round 8: no player objects from an earlier game
  }
  // Round 9: the pro is found automatically (game memory). The claim buttons override it, for the
  // case the automatic pro is ever wrong: right after a check, "I hit" / "I got hit".
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
  const uint32_t auto_pro = InBapGame() ? AutoBapPlayer() : 0u;
  if (BapPlayer())
    ImGui::Text("Your pro (claimed by hand): %04X%s  |  this game: %d hits, %d knockdowns (counted by the mod)",
                unsigned(BapPlayer() & 0xFFFFu),
                auto_pro ? (auto_pro == BapPlayer() ? ", automatic agrees" : ", AUTOMATIC DIFFERS") : "",
                r.live.hits, r.live.knockdowns);
  else if (auto_pro)
    ImGui::Text("Your pro (automatic): %04X  |  this game: %d hits, %d knockdowns (counted by the mod)",
                unsigned(auto_pro & 0xFFFFu), r.live.hits, r.live.knockdowns);
  else if (InBapGame())
    ImGui::TextDisabled("Your pro is not found yet: make a check, then press That was me.");
  else if (AwaitingResult())
    ImGui::Text("Last game: %d hits, %d knockdowns (counted by the mod)", r.live.hits, r.live.knockdowns);
  // Round 28: Jet Skates sprint button.
  if (Run().Stacks("jets") > 0) {
    int n = 0;
    const char* const* names = SprintButtonNames(&n);
    const unsigned pad = PadButtonsNow();
    ImGui::TextUnformatted("Jet Skates sprint button (the one you sprint with):");
    for (int i = 0; i < n; ++i) {
      if (i) ImGui::SameLine();
      char lb[40];
      std::snprintf(lb, sizeof(lb), "%s##sprint%d", names[i], i);
      if (ImGui::RadioButton(lb, SprintButton() == i)) SetSprintButton(i);
    }
    if (pad == 0xFFFFFFFFu)
      ImGui::TextColored(ImVec4(1.f, 0.6f, 0.2f, 1.f), "No XInput controller found: Jet Skates never triggers.");
    else
      ImGui::TextDisabled("Sprint button held: %s", SprintHeld() ? "YES (Jet Skates on)" : "no");
  }
  // Round 25: pro-only perks (shots / speed boost) - are the hooks seeing your pro?
  {
    bool any = false;
    for (auto& o : r.perks)
      if (auto* pd = FindPerk(o.id.c_str())) any = any || pd->scope == Scope::kProTunable || !std::strcmp(pd->id, "jets");
    if (any && InBapGame()) {
      const char* names[3] = {"shots", "wrist shots", "ratings"};
      char line[256];
      int w = std::snprintf(line, sizeof(line), "Your-only perks:");
      for (int i = 0; i < 3 && w > 0 && w < int(sizeof(line)); ++i) {
        unsigned calls = 0, mine = 0;
        int reg = 0;
        ProTunableStats(i, &calls, &mine, &reg);
        w += std::snprintf(line + w, sizeof(line) - size_t(w), "  %s %u calls, yours %u%s%s", names[i], calls, mine,
                           reg ? " (r" : "", reg ? (reg == 3 ? "3)" : reg == 4 ? "4)" : reg == 5 ? "5)" : "6+)") : "");
      }
      ImGui::TextDisabled("%s", line);
    }
  }
  // RE-5 round 21: the game's own box score (addresses.h, OBSERVED in one game; check it).
  if (InBapGame()) {
    const BoxScore b = ReadBoxScore();
    if (!b.valid) ImGui::TextDisabled("%s", BoxScoreStatus());
    if (b.valid)
      ImGui::TextDisabled("Game box score: %d G, %d A, %+d, %d SOG, %d hits, %d PIM, TOI %d:%02d  |  score %d-%d, shots %d-%d%s",
                          b.goals, b.assists, b.plus_minus, b.shots, b.hits, b.pim, b.toi / 60, b.toi % 60, b.team_gf, b.team_ga,
                          b.team_shots, b.opp_shots, b.identified ? "" : "  (your record: still a guess)");
  }
  if (AwaitingResult() && BoxScoreSerial() != g_box_seen) {
    g_box_seen = BoxScoreSerial();
    const BoxScore& b = LastBoxScore();
    g_manual.goals = b.goals;
    g_manual.assists = b.assists;
    g_manual.hits = b.hits;
    g_manual.plus_minus = b.plus_minus;
    g_manual.shots = b.shots;
    g_manual.team_gf = b.team_gf;
    g_manual.team_ga = b.team_ga;
    if (b.team_gf != b.team_ga) g_won = b.team_gf > b.team_ga ? 1 : 0;
  }
  if (AwaitingResult() && LastBoxScore().valid) {
    const BoxScore& b = LastBoxScore();
    ImGui::TextColored(ImVec4(0.5f, 1.f, 0.5f, 1.f), "Filled in from the game: %d G, %d A, %+d, %d hits; score %d-%d%s. Check, then press BAP game ended.",
                       b.goals, b.assists, b.plus_minus, b.hits, b.team_gf, b.team_ga,
                       b.team_gf == b.team_ga ? " (tied: shootout, set Won / Lost yourself)" : "");
    if (!b.identified) ImGui::TextColored(ImVec4(1.f, 0.6f, 0.2f, 1.f), "Your record was guessed, not identified: check the numbers carefully.");
  }
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
