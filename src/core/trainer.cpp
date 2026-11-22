#include "trainer.h"

namespace morse {

namespace {
const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
}

Trainer::Trainer()
    : enabled_(false), target_('K'), state_(0x9e3779b9u), hits_(0), misses_(0), streak_(0), last_miss_(false) {}

uint32_t Trainer::next_rand() {
  // xorshift32：几条形指令就够，不需要真随机
  uint32_t x = state_;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state_ = x ? x : 0x9e3779b9u;
  return state_;
}

char Trainer::pick(uint8_t idx) {
  if (idx >= kCount) idx %= kCount;
  return kAlphabet[idx];
}

void Trainer::set_enabled(bool on) {
  enabled_ = on;
  hits_ = misses_ = streak_ = 0;
  last_miss_ = false;
  if (on) target_ = pick((uint8_t)(next_rand() % kCount));
}

void Trainer::reroll(uint64_t seed) {
  state_ = (uint32_t)(seed ^ (seed >> 32)) | 1u;
  target_ = pick((uint8_t)(next_rand() % kCount));
}

void Trainer::on_char(char c) {
  if (!enabled_) return;
  if (c == target_) {
    ++hits_;
    ++streak_;
    last_miss_ = false;
    target_ = pick((uint8_t)(next_rand() % kCount));
  } else {
    ++misses_;
    streak_ = 0;
    last_miss_ = true;
  }
}

void Trainer::on_error() {
  if (!enabled_) return;
  ++misses_;
  streak_ = 0;
  last_miss_ = true;
}

}  // namespace morse
