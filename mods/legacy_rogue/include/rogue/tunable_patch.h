// Legacy Rogue - name-keyed, layered tunable patching on top of the existing
// nhl::ui::ITunableStore (src/tunable_runtime.h). No changes to the store needed.
//
// Perks never write raw values. Each active perk contributes a Mod (mul/add/set)
// to a tunable; the composer recomputes  value = set ?: (stock * PROD(mul) + SUM(add))
// and writes it once. Removing a perk recomputes from stock, so stacking and
// end-of-run revert are always exact.
//
// Two scopes:
//   * Persistent: active for the whole Be-A-Pro game (Apply/Revert at game start/end).
//   * Scoped:     ScopedTunable RAII inside a guest hook (e.g. only while the BAP
//                 player's check is being resolved) - this is how we make GLOBAL
//                 tunables behave PER-PLAYER. Valid only if the hook and the
//                 consumer run on the same guest thread (verify per hook).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include "renderer/core/nhl_tunable_store.h"

namespace rogue {

enum class ModOp : uint8_t { kMul, kAdd, kSet };

struct Mod {
  const char* tunable;  // gXxx name (join key, see docs/tunables_schema.tsv)
  ModOp op;
  float value;          // ints/bools are carried as float and rounded on write
};

class TunablePatcher {
 public:
  explicit TunablePatcher(nhl::ui::ITunableStore* store) : store_(store) {}

  // Call once the store reports kReady (build is lazy, ~12 s after boot).
  bool Index() {
    if (!store_ || store_->GetState() != nhl::ui::ITunableStore::State::kReady)
      return false;
    by_name_.clear();
    for (std::size_t i = 0; i < store_->Count(); ++i)
      by_name_[store_->Info(i).name] = i;
    return true;
  }
  bool Ready() const { return !by_name_.empty(); }
  bool Has(const char* name) const { return by_name_.count(name) != 0; }

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

  bool Lookup(const char* name, std::size_t& idx) const {
    auto it = by_name_.find(name);
    if (it == by_name_.end()) return false;
    idx = it->second;
    return true;
  }
  nhl::ui::ITunableStore* store() const { return store_; }

  static float BitsToF(uint32_t b, nhl::ui::TunableType t) {
    if (t == nhl::ui::TunableType::kFloat) { float f; std::memcpy(&f, &b, 4); return f; }
    return float(int32_t(b));
  }
  static uint32_t FToBits(float v, nhl::ui::TunableType t) {
    if (t == nhl::ui::TunableType::kFloat) { uint32_t b; std::memcpy(&b, &v, 4); return b; }
    if (t == nhl::ui::TunableType::kBool) return v != 0.f ? 1u : 0u;
    return uint32_t(int32_t(std::lround(v)));
  }

 private:
  void Recompute(const char* name) {
    std::size_t i;
    if (!Lookup(name, i)) return;  // unknown on this binary: skip (log in caller)
    auto e = store_->Info(i);
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
    if (!any) { store_->Reset(i); return; }
    float stock = BitsToF(e.captured_bits, e.type);
    store_->Set(i, FToBits(has_set ? set : stock * mul + add, e.type));
  }

  nhl::ui::ITunableStore* store_;
  std::unordered_map<std::string, std::size_t> by_name_;
  std::unordered_map<uint32_t, std::vector<Mod>> layers_;
};

// RAII override of one tunable for the duration of a guest call.
class ScopedTunable {
 public:
  ScopedTunable(TunablePatcher& p, const char* name, float scale) : p_(p) {
    if (!p_.Lookup(name, idx_)) return;
    auto* s = p_.store();
    auto e = s->Info(idx_);
    saved_ = s->Get(idx_);
    s->Set(idx_, TunablePatcher::FToBits(TunablePatcher::BitsToF(saved_, e.type) * scale, e.type));
    active_ = true;
  }
  ~ScopedTunable() { if (active_) p_.store()->Set(idx_, saved_); }
 private:
  TunablePatcher& p_;
  std::size_t idx_ = 0;
  uint32_t saved_ = 0;
  bool active_ = false;
};

}  // namespace rogue
