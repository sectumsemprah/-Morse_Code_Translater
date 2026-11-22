#pragma once
#include <cstdint>
#include "timing.h"

namespace morse {

// 键入层的输出契约。直键(iambic 将来也实现同一接口即可复用解码/界面/音频)。
class KeyerSink {
 public:
  virtual ~KeyerSink() {}
  virtual void on_press(uint64_t ts_us) = 0;
  virtual void on_element(bool dash, uint32_t dur_us) = 0;
  virtual void on_release(uint64_t ts_us, uint32_t dur_us) = 0;
  virtual void on_intra_gap(uint32_t gap_us) = 0;
  virtual void on_symbol_boundary(uint64_t gap_us) = 0;  // ≥ char_lo：提交当前符号串
  virtual void on_word_boundary(uint64_t gap_us) = 0;    // ≥ word_lo：补一个空格
  virtual void on_idle_boundary(uint64_t gap_us) = 0;    // ≥ idle_lo：用户走神了
};

// 直键（straight key）：按住时长定 ·/−，松开后的静默长度定字符/单词边界。
//
// 关键设计：边界用"截止期 + 阶段号"判定，且判定所用的时间戳来自事件本身，
// 而不是处理时刻。因此主循环被整屏重绘阻塞十几毫秒也不会误判间隙，
// 只会让出字变慢一点——这正面解决了本项目最大的坑。
class StraightKey {
 public:
  static const uint32_t kMinPulseUs = 4000;  // 短于此的按下视为触点抖动

  StraightKey(const UnitEstimator& u, KeyerSink& sink) : u_(u), sink_(sink) {}

  void on_key(uint8_t key, bool down, uint64_t ts_us);
  void on_tick(uint64_t ts_us);
  void reset();

  bool holding() const { return hold_key_ != 0; }
  uint8_t hold_key() const { return hold_key_; }
  uint64_t hold_start_us() const { return hold_ts_; }

 private:
  enum Stage : uint8_t { kNone = 0, kSymbolDone = 1, kWordDone = 2, kIdleDone = 3 };

  void settle(uint64_t gap_us);

  const UnitEstimator& u_;
  KeyerSink& sink_;
  uint8_t hold_key_ = 0;
  uint64_t hold_ts_ = 0;
  uint64_t release_ts_ = 0;
  uint8_t stage_ = kNone;
};

}  // namespace morse
