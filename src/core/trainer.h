#pragma once
#include <cstdint>

namespace morse {

// 练习模式：出一个小写字母/数字题，等解码结果比对。
// 这一模式同时是最好用的时序调参工具——命中率掉下来就说明 u 估歪了。
class Trainer {
 public:
  Trainer();

  void set_enabled(bool on);
  bool enabled() const { return enabled_; }
  char target() const { return target_; }

  void on_char(char c);
  void on_error();
  void reroll(uint64_t seed);

  uint32_t hits() const { return hits_; }
  uint32_t misses() const { return misses_; }
  uint32_t streak() const { return streak_; }
  bool last_miss() const { return last_miss_; }

 private:
  uint32_t next_rand();
  char pick(uint8_t idx);

  static constexpr uint8_t kCount = 36;
  bool enabled_;
  char target_;
  uint32_t state_;
  uint32_t hits_;
  uint32_t misses_;
  uint32_t streak_;
  bool last_miss_;
};

}  // namespace morse
