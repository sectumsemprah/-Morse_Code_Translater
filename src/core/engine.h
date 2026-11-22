#pragma once
#include <cstdint>
#include "decoder.h"
#include "events.h"
#include "gesture.h"
#include "keyer.h"
#include "morse_table.h"
#include "timing.h"
#include "trainer.h"

namespace morse {

enum KeyId : uint8_t { KEY_MORSE = 0, KEY_AUX = 1 };

struct Config {
  uint32_t unit_us;
  bool auto_unit;
  bool sidetone;
  uint8_t volume;      // 0..100
  uint16_t tone_hz;
  bool trainer;
};

struct EngineState {
  char code[12];        // 当前未提交的符号串，".-.." 形式
  char candidates[32];  // 该前缀还可能的字符
  char line[40];        // 当前行末尾
  char prev[40];        // 上一行
  char target;          // 练习模式题目
  uint32_t unit_us;
  uint32_t wpm_x10;
  uint32_t dit_hi_us;
  uint32_t dah_lo_us;
  uint32_t hold_us;     // 本次按下已持续时长（未按下为 0）
  uint32_t chars;
  uint32_t errors;
  uint32_t hits;
  uint32_t misses;
  uint32_t streak;
  uint32_t queue_overflow;
  bool holding;
  bool prefix_valid;
  bool last_error;
  bool last_miss;
  bool auto_unit;
  bool sidetone;
  bool trainer;
  bool waiting_peer;
  bool ended;
  bool dash_zone;  // 当前按住时长已进入"确信是划"区
};

// 内核对外只暴露一个门面：喂事件、取快照。UI / 音频 / 串口都只读快照或收钩子，
// 不碰状态机，这样核心逻辑能在 PC 上用 g++ 单独跑仿真测试。
class Engine : public KeyerSink {
 public:
  struct Hooks {
    void (*tone)(bool on, void* ctx);
    void (*line)(const char* text, void* ctx);  // AR / dump 时导出整行
    void* ctx;
  };

  Engine();

  void handle(const Event& e);
  void apply(UiAction a);
  void snapshot(EngineState& out);

  void set_hooks(const Hooks& h) { hooks_ = h; }
  void note_queue_overflow(uint32_t v) { overflow_seen_ = v; }
  Config config() const;
  void set_config(const Config& c);

  const Table& table() const { return table_; }
  Decoder& decoder() { return dec_; }
  UnitEstimator& unit() { return est_; }

  // KeyerSink
  void on_press(uint64_t ts_us) override;
  void on_element(bool dash, uint32_t dur_us) override;
  void on_release(uint64_t ts_us, uint32_t dur_us) override;
  void on_intra_gap(uint32_t gap_us) override;
  void on_symbol_boundary(uint64_t gap_us) override;
  void on_word_boundary(uint64_t gap_us) override;
  void on_idle_boundary(uint64_t gap_us) override;

 private:
  Table table_;
  UnitEstimator est_;
  StraightKey keyer_;
  Decoder dec_;
  Trainer trainer_;
  Gesture gesture_;
  Config cfg_;
  Hooks hooks_;
  uint64_t last_ts_;
  bool holding_;
  uint64_t hold_ts_;
  bool waiting_peer_;
  bool ended_;
  uint32_t overflow_seen_;
};

}  // namespace morse
