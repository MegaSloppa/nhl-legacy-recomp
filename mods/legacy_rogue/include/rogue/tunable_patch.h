// Legacy Rogue - name-keyed, layered tunable patching on the LIVE value (TUN-6).
//
// Perks never write raw values. Each active perk contributes a Mod (mul/add/set) to a
// tunable; the composer recomputes  value = set ?: (stock * PROD(mul) + SUM(add))  and writes
// it to the live value (N+32 + heap copy, via TweakIndex). Removing a perk recomputes from
// stock, so stacking and end-of-run revert are exact. Only float (tag 4) and int (tag 2)
// tunables are supported; anything else is skipped.
// (Before 2026-10-29 this went through the parent ITunableStore, which edits the TYPE TAG.)
//
// Two scopes:
//   * Persistent: active for the whole Be-A-Pro game (Apply/Revert at game start/end).
//   * Scoped:     ScopedTunable RAII inside a guest hook (e.g. only while the BAP
//                 player's check is being resolved) - this is how we make GLOBAL
//                 tunables behave PER-PLAYER. Valid only if the hook and the
//                 consumer run on the same guest thread (verify per hook).
// NOTE: game code does not clamp to a tunable's own min/max (TUN-5b), so stacking can go far
// beyond vanilla. Keep perk numbers sane on purpose.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include "rogue/tweak_index.h"

namespace rogue {

enum class ModOp : uint8_t { kMul, kAdd, kSet };

struct Mod {
  const char* tunable;  // gXxx name (see the TUN-9 catalog / tweak_readers)
  ModOp op;
  float value;          // ints are carried as float and rounded on write
};

inline bool ScalarTag(uint32_t tag) { return tag == 4u || tag == 2u; }
inline float BitsToF(uint32_t b, uint32_t tag) {
  if (tag == 4u) { float f; std::memcpy(&f, &b, 4); return f; }
  return float(int32_t(b));
}
inline uint32_t FToBits(float v, uint32_t tag) {
  if (tag == 4u) { uint32_t b; std::memcpy(&b, &v, 4); return b; }
  return uint32_t(int32_t(std::lround(v)));
}

class TunablePatcher {
 public:
  // Kicks the index build; true once it is ready.
  bool Index() { auto& t = TweakIndex::Get(); t.RequestBuild(); return t.Ready(); }
  bool Ready() const { return TweakIndex::Get().Ready(); }
  bool Has(const char* name) const {
    const TweakRef* r = TweakIndex::Get().Find(name);
    return r && ScalarTag(r->tag);
  }

  // layer = perk instance id. Re-adding a layer replaces it.
  void AddLayer(uint32_t layer, const std::vector<Mod>& mods) {
    layers_[layer] = mods;
    for (auto& m : mods) Recompute(m.tunable);
  }
  void RemoveLayer(uint32_t layer) {
    auto it = layers_.find(layer);
    if (it == layers_.end()) return;
    auto mods = it->second;
    layers_.erase(it);
    for (auto& m : mods) Recompute(m.tunable);
  }
  void RevertAll() {
    auto copy = layers_;
    layers_.clear();
    for (auto& [id, mods] : copy) for (auto& m : mods) Recompute(m.tunable);
  }

 private:
  void Recompute(const char* name) {
    TweakIndex& t = TweakIndex::Get();
    const TweakRef* r = t.Find(name);
    if (!r || !ScalarTag(r->tag)) return;  // unknown or unsupported type: skip
    bool any = false, has_set = false;
    float mul = 1.f, add = 0.f, set = 0.f;
    for (auto& [id, mods] : layers_)
      for (auto& m : mods) {
        if (std::strcmp(m.tunable, name) != 0) continue;
        any = true;
        if (m.op == ModOp::kMul) mul *= m.value;
        else if (m.op == ModOp::kAdd) add += m.value;
        else { has_set = true; set = m.value; }
      }
    if (!any) { t.Write(*r, r->stock); return; }
    const float stock = BitsToF(r->stock, r->tag);
    t.Write(*r, FToBits(has_set ? set : stock * mul + add, r->tag));
  }

  std::unordered_map<uint32_t, std::vector<Mod>> layers_;
};

// RAII override of one tunable (scale of its CURRENT value) for the duration of a guest call.
class ScopedTunable {
 public:
  ScopedTunable(TunablePatcher&, const char* name, float scale) {
    TweakIndex& t = TweakIndex::Get();
    r_ = t.Find(name);
    if (!r_ || !ScalarTag(r_->tag)) { r_ = nullptr; return; }
    saved_ = t.Read(*r_);
    t.Write(*r_, FToBits(BitsToF(saved_, r_->tag) * scale, r_->tag));
  }
  ~ScopedTunable() { if (r_) TweakIndex::Get().Write(*r_, saved_); }
  ScopedTunable(const ScopedTunable&) = delete;
  ScopedTunable& operator=(const ScopedTunable&) = delete;

 private:
  const TweakRef* r_ = nullptr;
  uint32_t saved_ = 0;
};

}  // namespace rogue
