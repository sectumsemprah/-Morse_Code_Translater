#pragma once
#include <cstdint>
#include <cstddef>

namespace morse {

// 单位时长 u（= 一个点的理想长度，微秒）与由它推出的一切判据。
//
// 判定一律用双阈值：落在"确信区"直接定论，落在两个确信区之间的犹豫区时
// 归到较近的一侧，这样手抖不会让同一个 − 一会儿判成 · 一会儿判成 −。
class UnitEstimator {
 public:
  static const size_t kWindow = 16;
  static const uint32_t kMinUnitUs = 40000;    // ≈30 WPM
  static const uint32_t kMaxUnitUs = 400000;   // ≈3 WPM

  explicit UnitEstimator(uint32_t init_us = 120000);

  uint32_t unit_us() const { return unit_us_; }
  void set_unit_us(uint32_t us);
  void nudge(int32_t delta_us);

  bool auto_mode() const { return auto_; }
  void set_auto_mode(bool on) { auto_ = on; }

  // 采样：按下时长是主证据，字符内间隙是备用证据
  void add_press(uint32_t dur_us);
  void add_intra_gap(uint32_t gap_us);

  // 只在"单词提交"时调用：一次单词内阈值保持恒定，否则同一字母前后判据不一致
  bool commit_estimate();

  uint32_t wpm_x10() const;   // PARIS 约定：wpm = 1200 / u_ms
  uint32_t samples() const { return (uint32_t)pn_; }

  // 判据（全部按 u 的倍数算，整数运算，避免浮点）
  uint32_t dit_hi() const { return unit_us_ * 16 / 10; }  // < 此为确信 ·
  uint32_t dah_lo() const { return unit_us_ * 24 / 10; }  // > 此为确信 −
  uint32_t press_mid() const { return unit_us_ * 2; }     // 犹豫区分界
  uint32_t char_lo() const { return unit_us_ * 2; }       // 间隙 >= 此为字母边界
  uint32_t word_lo() const { return unit_us_ * 5; }       // 间隙 >= 此为单词边界
  uint32_t idle_lo() const { return unit_us_ * 10; }      // 间隙 >= 此为走神复位

  bool classify_press(uint32_t dur_us) const {
    if (dur_us < dit_hi()) return false;
    if (dur_us > dah_lo()) return true;
    return dur_us >= press_mid();
  }

 private:
  static uint32_t mean(const uint32_t* v, size_t n);
  bool split_estimate(uint32_t* out) const;

  uint32_t press_[kWindow];
  uint32_t gap_[kWindow];
  size_t pn_, pi_, gn_, gi_;
  uint32_t unit_us_;
  bool auto_;
  uint32_t reject_streak_;
};

}  // namespace morse
