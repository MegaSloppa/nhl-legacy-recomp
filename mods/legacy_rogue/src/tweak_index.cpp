// Legacy Rogue - TweakIndex implementation (see include/rogue/tweak_index.h).
#include "generated/default/nhllegacy_init.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <thread>

#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include "rogue/tweak_index.h"

namespace rogue {
namespace {
constexpr uint32_t kImgBase = 0x82000000u;
constexpr uint32_t kImgSize = 0x1EA0000u;
constexpr std::size_t kMinComplete = 5000;  // a full index has ~10,583 objects

uint32_t Be(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
void StoreBe(uint8_t* p, uint32_t b) {
  p[0] = uint8_t(b >> 24); p[1] = uint8_t(b >> 16); p[2] = uint8_t(b >> 8); p[3] = uint8_t(b);
}
bool Readable(const uint8_t* p, std::size_t n) {
  MEMORY_BASIC_INFORMATION mbi{};
  if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
  if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
  return p + n <= static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
}
uint8_t* VBase() {
  auto* ks = rex::runtime::current_kernel_state();
  return (ks && ks->memory()) ? ks->memory()->virtual_membase() : nullptr;
}
uint8_t* Host(uint8_t* vb, uint32_t va) { return vb + va + REX_PHYS_HOST_OFFSET(va); }
bool IsTweakName(const uint8_t* s, uint32_t max) {
  if (s[0] != 'g') return false;
  uint32_t i = 1;
  for (; i < max && s[i]; ++i) {
    const uint8_t ch = s[i];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_'))
      return false;
  }
  return i > 2 && i < max;
}
}  // namespace

TweakIndex& TweakIndex::Get() {
  static TweakIndex t;
  return t;
}

void TweakIndex::RequestBuild() {
  const unsigned now = unsigned(GetTickCount());
  if (state_.load() != 0 || now - last_try_.load() < 5000u) return;
  int expected = 0;
  if (!state_.compare_exchange_strong(expected, 1)) return;
  last_try_.store(now);
  std::thread([this] { Build(); }).detach();
}

void TweakIndex::Build() {
  uint8_t* vb = VBase();
  if (!vb) { state_.store(0); return; }
  const uint8_t* img = vb + kImgBase;
  std::unordered_map<std::string, TweakRef> m;
  m.reserve(16384);
  for (uint32_t o = 8; o + 40 <= kImgSize; o += 4) {
    const uint32_t np = Be(img + o);
    if (np < kImgBase + 1 || np >= kImgBase + kImgSize - 2) continue;
    if (img[np - kImgBase] != 'g') continue;
    const uint32_t tag = Be(img + o + 8);
    if (tag == 0 || tag > 63) continue;
    const uint32_t vt = Be(img + o - 8);
    if (vt < kImgBase || vt >= kImgBase + kImgSize) continue;
    if (!IsTweakName(img + (np - kImgBase), std::min<uint32_t>(96, kImgBase + kImgSize - np))) continue;
    const uint32_t heap = Be(img + o + 36);
    if (heap < 0x40000000u) continue;
    const uint8_t* h = Host(vb, heap);
    if (!Readable(h, 40) || Be(h + 4) != np) continue;
    TweakRef r;
    r.n_va = kImgBase + o;
    r.heap_va = heap;
    r.tag = tag;
    r.stock = Be(img + o + 32);
    m.emplace(reinterpret_cast<const char*>(img + (np - kImgBase)), r);  // first wins
  }
  if (m.size() < kMinComplete) { state_.store(0); return; }  // registration not done yet
  map_ = std::move(m);
  state_.store(2, std::memory_order_release);
}

const TweakRef* TweakIndex::Find(const char* name) const {
  if (!Ready()) return nullptr;
  auto it = map_.find(name);
  return it == map_.end() ? nullptr : &it->second;
}

uint32_t TweakIndex::Read(const TweakRef& r) const {
  uint8_t* vb = VBase();
  return vb ? Be(vb + r.n_va + 32) : r.stock;
}

void TweakIndex::Write(const TweakRef& r, uint32_t bits) {
  uint8_t* vb = VBase();
  if (!vb) return;
  StoreBe(vb + r.n_va + 32, bits);            // image object, raw view (as scanned)
  StoreBe(Host(vb, r.heap_va + 36), bits);    // heap copy, translated view
}

}  // namespace rogue
