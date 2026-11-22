#include "gesture.h"

namespace morse {

Gesture::Gesture(uint32_t hold_us, uint32_t multi_gap_us)
    : hold_us_(hold_us), multi_gap_us_(multi_gap_us) {}

void Gesture::on_key(bool down, uint64_t ts_us) {
  if (down) {
    down_ = true;
    down_ts_ = ts_us;
    long_fired_ = false;
    return;
  }
  if (!down_) return;
  down_ = false;
  up_ts_ = ts_us;

  if (!long_fired_ && ts_us - down_ts_ >= hold_us_) {
    pending_ = UI_CLEAR_LINE;
    taps_ = 0;
    long_fired_ = true;
  } else if (!long_fired_) {
    if (taps_ < 8) ++taps_;
  }
}

void Gesture::on_tick(uint64_t ts_us) {
  if (down_ || taps_ == 0) return;
  if (ts_us - up_ts_ < multi_gap_us_) return;
  resolve(ts_us);
}

void Gesture::resolve(uint64_t) {
  if (taps_ == 1)
    pending_ = UI_BACKSPACE;
  else if (taps_ == 2)
    pending_ = UI_TOGGLE_TRAINER;
  else if (taps_ == 3)
    pending_ = UI_TOGGLE_SIDETONE;
  else
    pending_ = UI_UNIT_DOWN;  // 4 击以上 = 减速校准入口
  taps_ = 0;
}

UiAction Gesture::take() {
  UiAction a = pending_;
  pending_ = UI_NONE;
  return a;
}

void Gesture::reset() {
  down_ = false;
  taps_ = 0;
  long_fired_ = false;
  pending_ = UI_NONE;
}

}  // namespace morse
