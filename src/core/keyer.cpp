#include "keyer.h"

namespace morse {

void StraightKey::on_key(uint8_t key, bool down, uint64_t ts_us) {
  if (down) {
    if (hold_key_) return;  // 已有一键按下，忽略第二下（iambic 才需要这里做处理）

    if (release_ts_) {
      // 间隙到此成为定局：以真实间隙长度做最终判定，晚到的 tick 不再改变结果
      uint64_t gap = ts_us - release_ts_;
      if (stage_ == kNone && gap < u_.char_lo()) sink_.on_intra_gap((uint32_t)gap);
      settle(gap);
      release_ts_ = 0;
    }

    hold_key_ = key ? key : 1;
    hold_ts_ = ts_us;
    stage_ = kNone;
    sink_.on_press(ts_us);
    return;
  }

  if (!hold_key_) return;  // 抖动造成的假下降沿
  uint32_t dur = (uint32_t)(ts_us - hold_ts_);
  if (dur < kMinPulseUs) return;  // 触点回弹：当作仍按住，不产生元素，也不结束按下
  hold_key_ = 0;
  release_ts_ = ts_us;

  bool dash = u_.classify_press(dur);
  sink_.on_element(dash, dur);
  sink_.on_release(ts_us, dur);
}

void StraightKey::on_tick(uint64_t ts_us) {
  if (hold_key_ || !release_ts_) return;
  if (stage_ == kIdleDone) return;
  settle(ts_us - release_ts_);
}

void StraightKey::settle(uint64_t gap_us) {
  if (stage_ == kNone && gap_us >= u_.char_lo()) {
    sink_.on_symbol_boundary(gap_us);
    stage_ = kSymbolDone;
  }
  if (stage_ == kSymbolDone && gap_us >= u_.word_lo()) {
    sink_.on_word_boundary(gap_us);
    stage_ = kWordDone;
  }
  if (stage_ == kWordDone && gap_us >= u_.idle_lo()) {
    sink_.on_idle_boundary(gap_us);
    stage_ = kIdleDone;
  }
}

void StraightKey::reset() {
  hold_key_ = 0;
  hold_ts_ = 0;
  release_ts_ = 0;
  stage_ = kNone;
}

}  // namespace morse
