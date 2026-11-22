#include "keyscan.h"
#include "board.h"
#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_timer.h>

#ifndef CFG_KEY_ACTIVE_LOW
#define CFG_KEY_ACTIVE_LOW 1  // StickS3 按键按下接地，内部上拉
#endif

namespace fw {

namespace {

morse::EventQueue* g_queue = nullptr;
volatile uint64_t g_last_up[2] = {0, 0};
volatile uint8_t g_down[2] = {0, 0};
const gpio_num_t kPins[2] = {(gpio_num_t)PIN_KEY_MORSE, (gpio_num_t)PIN_KEY_AUX};
volatile uint32_t g_irq = 0;

inline uint8_t read_down(uint8_t i) {
  uint8_t lv = (uint8_t)gpio_get_level(kPins[i]);
  return CFG_KEY_ACTIVE_LOW ? (lv == 0) : (lv == 1);
}

void emit(uint8_t i, uint8_t down, uint64_t ts) {
  g_down[i] = down;
  if (!down) g_last_up[i] = ts;
  morse::Event e;
  e.type = morse::EV_KEY;
  e.key = i;
  e.down = down;
  e.ui = 0;
  e.ts_us = ts;
  if (g_queue) g_queue->push(e);
}

void key_isr(void*) {
  uint64_t ts = (uint64_t)esp_timer_get_time();
  ++g_irq;
  for (uint8_t i = 0; i < 2; ++i) {
    uint8_t down = read_down(i);
    if (down == g_down[i]) continue;
    // 释放后窗口内的再按下判为抖动；释放边沿永远放行，绝不让键卡住
    if (down && ts - g_last_up[i] < CFG_KEY_DEBOUNCE_US) continue;
    emit(i, down, ts);
  }
}

}  // namespace

void keyscan_init(morse::EventQueue* q) {
  g_queue = q;
  for (uint8_t i = 0; i < 2; ++i) {
    gpio_reset_pin(kPins[i]);
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << (uint8_t)kPins[i];
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_ANYEDGE;
    gpio_config(&cfg);
    g_down[i] = read_down(i);
  }
  attachInterruptArg(digitalPinToInterrupt(PIN_KEY_MORSE), key_isr, nullptr, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(PIN_KEY_AUX), key_isr, nullptr, CHANGE);
}

void keyscan_reconcile(uint64_t ts_us) {
  for (uint8_t i = 0; i < 2; ++i) {
    uint8_t down = read_down(i);
    if (down && !g_down[i] && ts_us - g_last_up[i] >= CFG_KEY_DEBOUNCE_US) {
      emit(i, 1, ts_us);
    } else if (!down && g_down[i]) {
      emit(i, 0, ts_us);
    }
  }
}

uint32_t keyscan_irq_count() { return g_irq; }

}  // namespace fw
