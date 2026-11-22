#pragma once
#include <cstdint>
#include <cstddef>

namespace morse {

// 由移植层实现：ESP32 上分别是 esp_timer_get_time() 与临界区，
// 主机测试里是普通函数。内核只通过这两个钩子接触外界。
uint64_t port_now_us();
void port_lock();
void port_unlock();

enum EventType : uint8_t {
  EV_NONE = 0,
  EV_KEY,    // 按键电平变化：key/down 有效
  EV_TICK,   // 周期性时基，ts_us 用于间隙的截止期判定
  EV_UI,     // 界面/手势动作，ui 字段为 UiAction
};

enum UiAction : uint8_t {
  UI_NONE = 0,
  UI_BACKSPACE,
  UI_CLEAR_LINE,
  UI_TOGGLE_TRAINER,
  UI_TOGGLE_SIDETONE,
  UI_UNIT_DOWN,
  UI_UNIT_UP,
  UI_NEW_LINE,
  UI_DUMP_TEXT,
};

struct Event {
  uint8_t type;
  uint8_t key;
  uint8_t down;
  uint8_t ui;
  uint64_t ts_us;
};

// push/pop 都在 port_lock 保护下，因为有两个生产者（GPIO ISR + esp_timer 任务）
// 和一个消费者（引擎任务）。容量 64 足够吸收一次整屏重绘带来的积压。
class EventQueue {
 public:
  static const size_t kCap = 64;

  bool push(const Event& e) {
    port_lock();
    bool ok = count_ < kCap;
    if (ok) {
      buf_[head_] = e;
      head_ = (head_ + 1) % kCap;
      ++count_;
    } else {
      ++overflow_;
    }
    port_unlock();
    return ok;
  }

  bool pop(Event& e) {
    port_lock();
    bool ok = count_ > 0;
    if (ok) {
      e = buf_[tail_];
      tail_ = (tail_ + 1) % kCap;
      --count_;
    }
    port_unlock();
    return ok;
  }

  bool empty() const { return count_ == 0; }
  uint32_t overflow() const { return overflow_; }
  void reset() {
    port_lock();
    head_ = tail_ = count_ = 0;
    overflow_ = 0;
    port_unlock();
  }

 private:
  Event buf_[kCap];
  volatile size_t head_ = 0;
  volatile size_t tail_ = 0;
  volatile size_t count_ = 0;
  volatile uint32_t overflow_ = 0;
};

}  // namespace morse
