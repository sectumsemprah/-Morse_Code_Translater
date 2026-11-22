#include "timing.h"

namespace morse {

UnitEstimator::UnitEstimator(uint32_t init_us)
    : pn_(0), pi_(0), gn_(0), gi_(0), auto_(true), reject_streak_(0) {
  unit_us_ = init_us;
  if (unit_us_ < kMinUnitUs) unit_us_ = kMinUnitUs;
  if (unit_us_ > kMaxUnitUs) unit_us_ = kMaxUnitUs;
}

void UnitEstimator::set_unit_us(uint32_t us) {
  if (us < kMinUnitUs) us = kMinUnitUs;
  if (us > kMaxUnitUs) us = kMaxUnitUs;
  unit_us_ = us;
  pn_ = gn_ = 0;
}

void UnitEstimator::nudge(int32_t delta_us) {
  int32_t v = (int32_t)unit_us_ + delta_us;
  if (v < (int32_t)kMinUnitUs) v = (int32_t)kMinUnitUs;
  if (v > (int32_t)kMaxUnitUs) v = (int32_t)kMaxUnitUs;
  unit_us_ = (uint32_t)v;
}

void UnitEstimator::add_press(uint32_t dur_us) {
  if (!auto_) return;
  if (dur_us < 3000 || dur_us > kMaxUnitUs * 6) return;  // 去抖残留 / 长按手势不当样本
  press_[pi_] = dur_us;
  pi_ = (pi_ + 1) % kWindow;
  if (pn_ < kWindow) ++pn_;
}

void UnitEstimator::add_intra_gap(uint32_t gap_us) {
  if (!auto_) return;
  if (gap_us < 3000 || gap_us > kMaxUnitUs * 3) return;
  gap_[gi_] = gap_us;
  gi_ = (gi_ + 1) % kWindow;
  if (gn_ < kWindow) ++gn_;
}

uint32_t UnitEstimator::mean(const uint32_t* v, size_t n) {
  uint64_t s = 0;
  for (size_t i = 0; i < n; ++i) s += v[i];
  return n ? (uint32_t)(s / n) : 0;
}

// 一维两类切分：排序后找相邻样本间的最大落差，落差两侧分别当作 ·簇 与 −簇。
// 只有 −簇/·簇 落在 [2.2, 4.2] 这个合理区间才接受，防止"只打了点"时估出荒谬值。
bool UnitEstimator::split_estimate(uint32_t* out) const {
  if (pn_ < 6) return false;
  uint32_t s[kWindow];
  size_t n = 0;
  for (size_t i = 0; i < kWindow; ++i) {
    if (i < pn_) s[n++] = press_[i];
  }
  for (size_t i = 1; i < n; ++i) {  // 插入排序，n<=16
    uint32_t key = s[i];
    size_t j = i;
    while (j > 0 && s[j - 1] > key) {
      s[j] = s[j - 1];
      --j;
    }
    s[j] = key;
  }

  size_t best = 0;
  uint32_t best_gap = 0;
  for (size_t i = 2; i + 1 < n; ++i) {
    uint32_t g = s[i] - s[i - 1];
    if (g > best_gap) {
      best_gap = g;
      best = i;
    }
  }
  if (best == 0) return false;

  uint32_t low = mean(s, best);
  uint32_t high = mean(s + best, n - best);
  if (low == 0) return false;
  uint32_t ratio10 = high * 10 / low;  // 期望 ≈30
  if (ratio10 < 22 || ratio10 > 42) return false;
  *out = low;
  return true;
}

bool UnitEstimator::commit_estimate() {
  if (!auto_) return false;

  uint32_t cand = 0;
  bool got = split_estimate(&cand);
  if (!got && gn_ >= 4) {
    cand = mean(gap_, gn_);  // 备用证据：字符内间隙 ≈ 1u
    got = true;
  }

  pn_ = pi_ = 0;
  gn_ = gi_ = 0;

  if (!got || cand < kMinUnitUs || cand > kMaxUnitUs) {
    if (++reject_streak_ > 8) reject_streak_ = 8;
    return false;
  }
  reject_streak_ = 0;
  unit_us_ = (unit_us_ + cand) / 2;
  return true;
}

uint32_t UnitEstimator::wpm_x10() const {
  // wpm = 1200 / u_ms，而 u_ms = u_us/1000，故 wpm*10 = 1.2e7 / u_us
  if (unit_us_ == 0) return 0;
  return (uint32_t)(12000000ull / unit_us_);
}

}  // namespace morse
