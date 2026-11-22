#pragma once
#include <cstdint>
#include "events.h"

namespace morse {

// 副键（辅助键）手势机：单击=退格，双击=练习模式，三击=回听音开关，
// 长按=清行。单击要等一抖间隙才能确认，所以退格有 ~kMultiGapMs 的固有延迟，
// 换来的是不用第三个物理键。
class Gesture {
 public:
  Gesture(uint32_t hold_us = 600000, uint32_t multi_gap_us = 450000);

  void on_key(bool down, uint64_t ts_us);
  void on_tick(uint64_t ts_us);
  UiAction take();      // 取出并清空待发动作
  void reset();

 private:
  void resolve(uint64_t ts_us);

  const uint32_t hold_us_;
  const uint32_t multi_gap_us_;
  bool down_ = false;
  uint64_t down_ts_ = 0;
  uint64_t up_ts_ = 0;
  uint8_t taps_ = 0;
  bool long_fired_ = false;
  UiAction pending_ = UI_NONE;
};

}  // namespace morse
