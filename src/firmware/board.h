#pragma once
#include <cstdint>

// StickS3 引脚与全局配置。来源：docs.m5stack.com StickS3 PinMap。
//   LCD  ST7789P3  MOSI G39  SCK G40  RS G45  CS G41  RST G21  BL G38
//   KEY  KEY1 = G11  KEY2 = G12          （侧面电源键走 M5PM1，不可编程）
//   音频 ES8311 + AW8737 + 8Ω1W 喇叭，MEMS 麦克风
//   IR   TX G46  RX G42

#define PIN_KEY_MORSE 11  // 摩斯键（直键）
#define PIN_KEY_AUX 12    // 副键：退格 / 清行 / 切换模式

#define CFG_TICK_HZ 500        // 时基：2ms 一次，远小于最快手速的 1u，够精细
#define CFG_KEY_DEBOUNCE_US 3000
#define CFG_ENGINE_TASK_PRIO 6
#define CFG_ENGINE_TASK_CORE 1
#define CFG_UI_TASK_PRIO 2
#define CFG_UI_TASK_CORE 0
#define CFG_UI_PERIOD_MS 33

#define CFG_BACKLIGHT_AUTO_DIM_S 60  // 无敲击多久把背光压到 20%
#define CFG_BATTERY_WARN_MV 3200

#define CFG_AUDIO 1
#define CFG_SERIAL_LOG 1
#define CFG_SIDEZONE_TONE_HZ 620
