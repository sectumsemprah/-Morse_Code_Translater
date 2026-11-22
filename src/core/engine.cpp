#include "engine.h"
#include <cstring>

namespace morse {

Engine::Engine()
    : table_(),
      est_(120000),
      keyer_(est_, *this),
      dec_(table_),
      trainer_(),
      gesture_(),
      cfg_{120000, true, true, 60, 620, false},
      hooks_{nullptr, nullptr, nullptr},
      last_ts_(0),
      holding_(false),
      hold_ts_(0),
      waiting_peer_(false),
      ended_(false),
      overflow_seen_(0) {}

Config Engine::config() const { return cfg_; }

void Engine::set_config(const Config& c) {
  cfg_ = c;
  est_.set_auto_mode(c.auto_unit);
  if (!c.auto_unit) est_.set_unit_us(c.unit_us);
  trainer_.set_enabled(c.trainer);
}

void Engine::handle(const Event& e) {
  switch (e.type) {
    case EV_KEY:
      if (e.key == KEY_MORSE)
        keyer_.on_key(e.key, e.down, e.ts_us);
      else
        gesture_.on_key(e.down, e.ts_us);
      break;
    case EV_TICK:
      // 边界判定用事件自带的时间戳，而不是"处理时刻"，主循环抖动因此不影响正确性
      keyer_.on_tick(e.ts_us);
      gesture_.on_tick(e.ts_us); {
        UiAction a = gesture_.take();
        if (a != UI_NONE) apply(a);
      }
      break;
    case EV_UI:
      apply((UiAction)e.ui);
      break;
    default:
      break;
  }
  if (e.ts_us > last_ts_) last_ts_ = e.ts_us;
}

void Engine::apply(UiAction a) {
  switch (a) {
    case UI_BACKSPACE:
      dec_.backspace();
      break;
    case UI_CLEAR_LINE:
      dec_.clear_line();
      keyer_.reset();
      break;
    case UI_TOGGLE_TRAINER:
      trainer_.set_enabled(!trainer_.enabled());
      trainer_.reroll(port_now_us());
      break;
    case UI_TOGGLE_SIDETONE:
      cfg_.sidetone = !cfg_.sidetone;
      break;
    case UI_UNIT_DOWN:
    case UI_UNIT_UP: {
      cfg_.auto_unit = false;
      est_.set_auto_mode(false);
      int32_t step = (int32_t)(est_.unit_us() / 10);
      est_.nudge(a == UI_UNIT_UP ? -step : step);  // 点数不变，改的是单位长度
      cfg_.unit_us = est_.unit_us();
      break;
    }
    case UI_NEW_LINE:
      dec_.new_line();
      break;
    case UI_DUMP_TEXT:
      if (hooks_.line) {
        char buf[TextBuf::kCap + 1];
        dec_.text().full(buf, sizeof(buf));
        hooks_.line(buf, hooks_.ctx);
      }
      break;
    default:
      break;
  }
}

void Engine::on_press(uint64_t ts_us) {
  holding_ = true;
  hold_ts_ = ts_us;
  dec_.clear_error();
  if (cfg_.sidetone && hooks_.tone) hooks_.tone(true, hooks_.ctx);
}

void Engine::on_element(bool dash, uint32_t dur_us) {
  est_.add_press(dur_us);
  dec_.element(dash);
}

void Engine::on_release(uint64_t ts_us, uint32_t dur_us) {
  (void)ts_us;
  (void)dur_us;
  holding_ = false;
  if (cfg_.sidetone && hooks_.tone) hooks_.tone(false, hooks_.ctx);
}

void Engine::on_intra_gap(uint32_t gap_us) { est_.add_intra_gap(gap_us); }

void Engine::on_symbol_boundary(uint64_t gap_us) {
  (void)gap_us;
  char emitted = 0;
  Action a = dec_.commit(&emitted);

  switch (a) {
    case Action::FieldSep:
      dec_.space();
      break;
    case Action::CommitLine:
      if (hooks_.line) {
        char buf[TextBuf::kCap + 1];
        dec_.text().full(buf, sizeof(buf));
        hooks_.line(buf, hooks_.ctx);
      }
      dec_.new_line();
      break;
    case Action::Wait:
      waiting_peer_ = true;
      ended_ = false;
      break;
    case Action::End:
      ended_ = true;
      waiting_peer_ = false;
      break;
    default:
      break;
  }

  if (trainer_.enabled() && emitted) {
    if (emitted == '?')
      trainer_.on_error();
    else
      trainer_.on_char(emitted);
  }
}

void Engine::on_word_boundary(uint64_t gap_us) {
  (void)gap_us;
  dec_.space();
  est_.commit_estimate();  // 一个单词之内判据恒定，单词之间才允许漂移
}

void Engine::on_idle_boundary(uint64_t gap_us) {
  (void)gap_us;
  dec_.reset_symbol();
  keyer_.reset();
}

void Engine::snapshot(EngineState& out) {
  dec_.text().line_tail(out.line, sizeof(out.line));
  dec_.text().prev_line_tail(out.prev, sizeof(out.prev));

  size_t n = dec_.pending_elements();
  if (n >= sizeof(out.code)) n = sizeof(out.code) - 1;
  memcpy(out.code, dec_.code(), n);
  out.code[n] = '\0';

  size_t c = strlen(dec_.candidates());
  if (c >= sizeof(out.candidates)) c = sizeof(out.candidates) - 1;
  memcpy(out.candidates, dec_.candidates(), c);
  out.candidates[c] = '\0';

  uint64_t hold = (holding_ && last_ts_ > hold_ts_) ? (last_ts_ - hold_ts_) : 0;
  out.hold_us = (uint32_t)hold;
  out.holding = holding_;
  out.dash_zone = holding_ && hold >= est_.dah_lo();
  out.prefix_valid = dec_.prefix_valid();
  out.last_error = dec_.last_error();
  out.unit_us = est_.unit_us();
  out.wpm_x10 = est_.wpm_x10();
  out.dit_hi_us = est_.dit_hi();
  out.dah_lo_us = est_.dah_lo();
  out.auto_unit = est_.auto_mode();
  out.chars = dec_.chars();
  out.errors = dec_.errors();
  out.hits = trainer_.hits();
  out.misses = trainer_.misses();
  out.streak = trainer_.streak();
  out.last_miss = trainer_.last_miss();
  out.target = trainer_.enabled() ? trainer_.target() : 0;
  out.sidetone = cfg_.sidetone;
  out.trainer = trainer_.enabled();
  out.waiting_peer = waiting_peer_;
  out.ended = ended_;
  out.queue_overflow = overflow_seen_;
}

}  // namespace morse
