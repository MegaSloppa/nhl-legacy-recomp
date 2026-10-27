// Legacy Rogue - feedback effects for perk procs ("CRITICAL!", hit flashes, sfx).
//
// Tier A (today, host-side, no RE): ImGui foreground popups + full-screen flash +
//   WAV playback from <exe>/rogue/sfx/*.wav. Needs a per-frame draw call that runs
//   even when the overlay window is CLOSED (see docs/current-status.md, task FX-1).
//   NOTE: the color-grade cvars (present_grade_*) are currently a no-op in the
//   rebuilt runtime (src/present_grade_compat.cpp), so they cannot be used for flashes.
// Tier B (needs RE): trigger the game's own effects - crowd roar, goal horn, camera
//   shake, replay/highlight flag, glass-break/particle - by calling guest functions.
// Tier C (experiment): hit-stop / slow-mo by briefly changing game speed tunables.
#pragma once
#include <cstdint>

namespace rogue::fx {

enum class Kind : uint8_t { kCritSlap, kKnockdown, kShockwave, kPerkBought, kTierUp, kMilestone };

// Queue an effect. Thread-safe (hooks run on guest threads, drawing on render thread).
void Push(Kind k, const char* text = nullptr);

// Call every frame on the render thread inside an active ImGui frame.
void Draw();

}  // namespace rogue::fx
