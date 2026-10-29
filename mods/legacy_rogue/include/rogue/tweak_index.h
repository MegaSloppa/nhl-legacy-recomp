// Legacy Rogue - live tweak object index (TUN-6). Replaces the parent ITunableStore as the
// perk write path: that store edits N+8 (the TYPE TAG). Proven layout (docs/re-findings.md):
// N = address of the name pointer of an image tweak object; N+8 tag (4 float, 2 int,
// 1 bool, 12 indexed slider, 13 game-style table), N+32 LIVE VALUE (CONFIRMED TUN-5b),
// N+36 pointer H to a heap copy whose live value is at H+36.
// The index is built once on a worker thread by scanning the image (~0.2 s), only after
// tweak registration (callers wait ~20 s after boot or until a BAP game starts).
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace rogue {

struct TweakRef {
  uint32_t n_va = 0;     // name-pointer address in the image object
  uint32_t heap_va = 0;  // heap copy base H
  uint32_t tag = 0;      // type tag (N+8)
  uint32_t stock = 0;    // N+32 bits when the index was built
};

class TweakIndex {
 public:
  static TweakIndex& Get();
  void RequestBuild();  // idempotent, async, retries at most every 5 s until complete
  bool Ready() const { return state_.load(std::memory_order_acquire) == 2; }
  std::size_t Count() const { return Ready() ? map_.size() : 0; }
  const TweakRef* Find(const char* name) const;
  uint32_t Read(const TweakRef& r) const;          // live N+32
  void Write(const TweakRef& r, uint32_t bits);    // N+32 and H+36

 private:
  void Build();
  std::atomic<int> state_{0};  // 0 idle, 1 building, 2 ready
  std::atomic<unsigned> last_try_{0};
  std::unordered_map<std::string, TweakRef> map_;
};

}  // namespace rogue
