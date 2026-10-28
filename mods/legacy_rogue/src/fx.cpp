#include "rogue/fx.h"
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>  // PlaySoundW; link winmm
#include <rex/filesystem.h>

namespace rogue::fx {
namespace {
struct Style { const char* text; ImU32 color; float flash_alpha; float secs; const wchar_t* wav; };
Style StyleFor(Kind k) {
  switch (k) {
    case Kind::kCritSlap:   return {"CRITICAL!", IM_COL32(255, 200, 40, 255), 0.35f, 1.4f, L"crit.wav"};
    case Kind::kKnockdown:  return {"FLATTENED", IM_COL32(255, 80, 60, 255), 0.20f, 1.1f, L"knockdown.wav"};
    case Kind::kShockwave:  return {"SHOCKWAVE", IM_COL32(120, 200, 255, 255), 0.30f, 1.4f, L"shockwave.wav"};
    case Kind::kPerkBought: return {"PERK ACQUIRED", IM_COL32(180, 255, 140, 255), 0.f, 1.6f, L"perk.wav"};
    case Kind::kTierUp:     return {"NOTORIETY UP", IM_COL32(255, 120, 255, 255), 0.15f, 2.2f, L"tier.wav"};
    default:                return {"MILESTONE", IM_COL32(255, 255, 255, 255), 0.f, 2.0f, L"milestone.wav"};
  }
}
struct Live { Style s; std::string text; double t0; };
std::mutex g_mu;
std::deque<Live> g_q;
double Now() {
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}
}  // namespace

void Push(Kind k, const char* text) {
  Style s = StyleFor(k);
  auto wav = rex::filesystem::GetExecutableFolder() / "rogue" / "sfx" / s.wav;
  if (std::filesystem::exists(wav))  // single-voice; swap for XAudio2 later (FX-3)
    PlaySoundW(wav.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
  std::lock_guard<std::mutex> l(g_mu);
  if (g_q.size() > 8) g_q.pop_front();
  g_q.push_back({s, text ? text : s.text, Now()});
}

void Draw() {
  std::lock_guard<std::mutex> l(g_mu);
  if (g_q.empty()) return;
  auto* dl = ImGui::GetForegroundDrawList();
  const ImVec2 ds = ImGui::GetIO().DisplaySize;
  const double now = Now();
  float y = ds.y * 0.28f;
  for (auto it = g_q.begin(); it != g_q.end();) {
    const float age = float(now - it->t0), k = 1.f - age / it->s.secs;
    if (k <= 0.f) { it = g_q.erase(it); continue; }
    if (it->s.flash_alpha > 0.f && age < 0.15f)  // short full-screen flash
      dl->AddRectFilled({0, 0}, ds, (it->s.color & 0x00FFFFFF) |
                        (ImU32(255 * it->s.flash_alpha * (1.f - age / 0.15f)) << 24));
    const float scale = 2.6f + 1.2f * (1.f - k) * (age < 0.2f ? 1.f : 0.f);
    ImFont* f = ImGui::GetFont();
    const float sz = ImGui::GetFontSize() * scale;
    ImVec2 tsz = f->CalcTextSizeA(sz, FLT_MAX, 0.f, it->text.c_str());
    ImU32 col = (it->s.color & 0x00FFFFFF) | (ImU32(255 * std::min(1.f, k * 2.f)) << 24);
    dl->AddText(f, sz, {ds.x * 0.5f - tsz.x * 0.5f + 3, y + 3}, IM_COL32(0, 0, 0, 180 * k), it->text.c_str());
    dl->AddText(f, sz, {ds.x * 0.5f - tsz.x * 0.5f, y}, col, it->text.c_str());
    y += sz * 1.1f;
    ++it;
  }
}
}  // namespace rogue::fx
