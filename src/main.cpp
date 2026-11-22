// StickS3 摩斯电码翻译器 —— 顶层装配
//
// 任务划分（本项目唯一需要考虑并发的地方）：
//   GPIO ISR / esp_timer  -> EventQueue            只打时间戳，不做判定
//   engine task (core1)    -> Engine 内核 + 回听音   高优先级，保证声音跟手
//   ui task   (core0)      -> 快照渲染 + NVS 落盘    低优先级，阻塞多久都不影响判定
// 正确性建立在"边界判定只使用事件自带的时间戳"之上，所以整屏重绘不会污染间隙测量。

#include <M5Unified.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "core/engine.h"
#include "core/events.h"
#include "firmware/audio.h"
#include "firmware/board.h"
#include "firmware/keyscan.h"
#include "firmware/link.h"
#include "firmware/settings.h"
#include "firmware/ui.h"

namespace {

morse::EventQueue g_queue;
morse::Engine g_engine;
SemaphoreHandle_t g_state_mux = nullptr;
morse::EngineState g_state;
esp_timer_handle_t g_tick_timer = nullptr;

void tone_hook(bool on, void*) { fw::audio_tone(on); }

void line_hook(const char* text, void*) { fw::link_print_line(text); }

void tick_cb(void*) {
  uint64_t ts = morse::port_now_us();
  morse::Event e;
  e.type = morse::EV_TICK;
  e.key = 0;
  e.down = 0;
  e.ui = 0;
  e.ts_us = ts;
  g_queue.push(e);
  fw::keyscan_reconcile(ts);
}

void engine_task(void*) {
  for (;;) {
    morse::Event e;
    int drained = 0;
    while (g_queue.pop(e)) {
      g_engine.handle(e);
      if (++drained >= 32) break;
    }
    g_engine.note_queue_overflow(g_queue.overflow());

    // 有积压就不睡眠继续排空，只在空闲时让出 CPU
    if (g_queue.empty()) {
      fw::link_poll(g_engine);
      if (g_queue.empty()) vTaskDelay(1);
    }

    xSemaphoreTake(g_state_mux, portMAX_DELAY);
    g_engine.snapshot(g_state);
    xSemaphoreGive(g_state_mux);
  }
}

void ui_task(void*) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(CFG_UI_PERIOD_MS));
    morse::EngineState local;
    xSemaphoreTake(g_state_mux, portMAX_DELAY);
    local = g_state;
    xSemaphoreGive(g_state_mux);
    fw::ui_render(local, (int)M5.Power.getBatteryLevel());
  }
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  cfg.internal_imu = false;  // 加速度计对本功能无用，省掉 I2C 轮询
  cfg.internal_spk = true;
  cfg.internal_mic = false;
  M5.begin(cfg);

  Serial.begin(115200);

  memset(&g_state, 0, sizeof(g_state));
  g_state_mux = xSemaphoreCreateMutex();

  morse::Config cfgm{120000, true, true, 60, CFG_SIDEZONE_TONE_HZ, false};
  fw::settings_load(&cfgm);
  g_engine.set_config(cfgm);

  morse::Engine::Hooks hooks;
  hooks.tone = tone_hook;
  hooks.line = line_hook;
  hooks.ctx = nullptr;
  g_engine.set_hooks(hooks);

  fw::audio_init();
  fw::audio_set_volume(cfgm.volume);
  fw::ui_init();
  fw::link_init();
  fw::keyscan_init(&g_queue);

  esp_timer_create_args_t ta = {};
  ta.callback = tick_cb;
  ta.arg = nullptr;
  ta.dispatch_method = ESP_TIMER_TASK;
  ta.name = "morse_tick";
  ESP_ERROR_CHECK(esp_timer_create(&ta, &g_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(g_tick_timer, 1000000 / CFG_TICK_HZ));

  xTaskCreatePinnedToCore(engine_task, "morse_eng", 4096, nullptr, CFG_ENGINE_TASK_PRIO,
                          nullptr, CFG_ENGINE_TASK_CORE);
  xTaskCreatePinnedToCore(ui_task, "morse_ui", 4096, nullptr, CFG_UI_TASK_PRIO, nullptr,
                          CFG_UI_TASK_CORE);

  fw::link_boot_banner(cfgm);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
  // 空闲时把待存的配置写进 NVS（在 UI 任务里做，见 settings_service_pending）
}
