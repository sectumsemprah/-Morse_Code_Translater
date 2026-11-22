#include "ui.h"
#include "board.h"
#include "settings.h"
#include <M5Unified.h>
#include <cstring>

namespace fw {

namespace {

// 240x135 横屏的行布局，按 AsciiFont8x16 的 8x16 单元格排；换字体只改这几行。
const int32_t CW = 8;
const int32_t CH = 16;
const int32_t Y_STATUS = 0;
const int32_t Y_CODE = 16;
const int32_t Y_CAND = 40;
const int32_t Y_LINE = 56;
const int32_t Y_PREV = 88;
const int32_t Y_HINT = 104;
const int32_t Y_WARN = 119;

const int32_t BAR_X = 136;
const int32_t BAR_W = 100;
const int32_t BAR_H = 12;
const int32_t BAR_Y = Y_CODE + 4;

const uint16_t C_BG = m5gfx::color565(0, 0, 0);
const uint16_t C_TEXT = m5gfx::color565(235, 235, 235);
const uint16_t C_DIM = m5gfx::color565(105, 105, 105);
const uint16_t C_DIT = m5gfx::color565(80, 220, 120);
const uint16_t C_DAH = m5gfx::color565(90, 160, 255);
const uint16_t C_AMBER = m5gfx::color565(255, 190, 60);
const uint16_t C_ERR = m5gfx::color565(255, 90, 90);
const uint16_t C_TRACK = m5gfx::color565(28, 28, 38);

const int kRows = 6;
char g_cache[kRows][48];
uint32_t g_idle_frames = 0;
uint8_t g_brightness = 190;

// 只重绘内容变过的那一行：整屏刷新既慢又会闪，135 高的窄屏尤其明显。
void draw_row(int row, int32_t y, const char* text, uint16_t color, int32_t size) {
  if (strcmp(g_cache[row], text) == 0) return;
  strncpy(g_cache[row], text, sizeof(g_cache[0]) - 1);
  g_cache[row][sizeof(g_cache[0]) - 1] = '\0';

  int32_t w = (int32_t)strlen(text) * CW * size + 4;
  if (w > M5.Display.width()) w = M5.Display.width();
  M5.Display.fillRect(0, y, w, CH * size, C_BG);
  M5.Display.setTextColor(color);
  M5.Display.setTextSize(size);
  M5.Display.drawString(text, 0, y);
  M5.Display.setTextSize(1);
}

void tail_to(char* dst, size_t dst_cap, const char* src, size_t max_chars) {
  size_t n = strlen(src);
  if (n > max_chars) src += n - max_chars;
  snprintf(dst, dst_cap, "%s", src);
}

}  // namespace

void ui_init() {
  M5.Display.setColorDepth(16);
  M5.Display.setRotation(1);  // 240x135
  M5.Display.fillScreen(C_BG);
  M5.Display.setFont(&m5gfx::fonts::AsciiFont8x16);
  for (int i = 0; i < kRows; ++i) g_cache[i][0] = '\0';
  M5.Display.setBrightness(g_brightness);
}

void ui_render(const morse::EngineState& s, int battery_percent) {
  settings_service_pending();

  char buf[48];
  snprintf(buf, sizeof(buf), "%c u%lu %.1fw%s B%d", s.trainer ? 'T' : 'M',
           (unsigned long)(s.unit_us / 1000), (double)s.wpm_x10 / 10.0,
           s.auto_unit ? "*" : "", battery_percent);
  draw_row(0, Y_STATUS, buf,
           (battery_percent >= 0 && battery_percent < 20) ? C_ERR : C_DIM, 1);

  // --- 符号区：点/划按判据比例画，长度直接对应时长 ---
  int32_t cx = 8;
  const int32_t cy = Y_CODE + 12;
  for (const char* p = s.code; *p; ++p) {
    if (cx > BAR_X - 34) break;
    if (*p == '.') {
      M5.Display.fillRect(cx - 2, cy - 6, 16, 12, C_BG);
      M5.Display.fillCircle(cx + 6, cy, 6, s.prefix_valid ? C_TEXT : C_ERR);
      cx += 24;
    } else {
      M5.Display.fillRect(cx - 2, cy - 6, 16, 12, C_BG);
      M5.Display.fillRect(cx, cy - 5, 26, 10, s.prefix_valid ? C_TEXT : C_ERR);
      cx += 34;
    }
  }
  if (cx < BAR_X - 4) M5.Display.fillRect(cx, Y_CODE, BAR_X - cx - 4, 24, C_BG);

  // --- 进度条：把这一下"会被判成什么"实时画出来 ---
  uint32_t scale = s.unit_us * 4;
  M5.Display.fillRect(BAR_X, BAR_Y, BAR_W, BAR_H, C_TRACK);
  if (s.holding && scale) {
    uint32_t hold = s.hold_us > scale ? scale : s.hold_us;
    int32_t w = (int32_t)((uint64_t)hold * BAR_W / scale);
    uint16_t col = s.last_error ? C_ERR : (s.dash_zone ? C_DAH : (s.hold_us >= s.dit_hi_us ? C_AMBER : C_DIT));
    M5.Display.fillRect(BAR_X, BAR_Y, w > 0 ? w : 1, BAR_H, col);
  }
  if (scale) {  // 判据刻度，调参时一眼看出阈值落在哪
    int32_t x1 = BAR_X + (int32_t)((uint64_t)s.dit_hi_us * BAR_W / scale);
    int32_t x2 = BAR_X + (int32_t)((uint64_t)s.dah_lo_us * BAR_W / scale);
    M5.Display.fillRect(x1, BAR_Y - 2, 1, BAR_H + 4, C_DIT);
    M5.Display.fillRect(x2, BAR_Y - 2, 1, BAR_H + 4, C_DAH);
  }

  if (s.target)
    snprintf(buf, sizeof(buf), "TGT %c  %lu/%lu", s.target, (unsigned long)s.hits,
             (unsigned long)s.misses);
  else if (s.code[0] && s.candidates[0])
    snprintf(buf, sizeof(buf), "%.29s?", s.candidates);  // 空闲时不刷全码表
  else
    buf[0] = '\0';
  draw_row(1, Y_CAND, buf, s.target ? C_AMBER : C_DIM, 1);

  char shown[40];
  tail_to(shown, sizeof(shown), s.line, (size_t)(M5.Display.width() / (CW * 2) - 1));
  draw_row(2, Y_LINE, shown, s.last_error ? C_ERR : C_TEXT, 2);

  tail_to(shown, sizeof(shown), s.prev, (size_t)(M5.Display.width() / CW - 1));
  draw_row(3, Y_PREV, shown, C_DIM, 1);

  snprintf(buf, sizeof(buf), "%luc %lue K1 key K2 gest", (unsigned long)s.chars,
           (unsigned long)s.errors);
  draw_row(4, Y_HINT, buf, C_DIM, 1);

  buf[0] = '\0';
  uint16_t wcol = C_AMBER;
  if (s.last_error) {
    strcpy(buf, "ERR  7dots=back");
    wcol = C_ERR;
  } else if (s.last_miss) {
    strcpy(buf, "MISS");
    wcol = C_ERR;
  } else if (s.queue_overflow) {
    snprintf(buf, sizeof(buf), "OVF%lu", (unsigned long)s.queue_overflow);
    wcol = C_ERR;
  } else if (s.ended) {
    strcpy(buf, "SK end");
  } else if (s.waiting_peer) {
    strcpy(buf, "KN wait");
  }
  draw_row(5, Y_WARN, buf, wcol, 1);

  bool quiet = !s.holding && !s.last_error && !s.last_miss && !s.waiting_peer &&
               !s.ended && s.queue_overflow == 0 && s.code[0] == '\0';
  if (quiet) {
    ++g_idle_frames;
  } else {
    g_idle_frames = 0;
  }
  uint8_t want = g_idle_frames > (CFG_BACKLIGHT_AUTO_DIM_S * 30u) ? 40 : 190;
  if (want != g_brightness) {
    g_brightness = want;
    M5.Display.setBrightness(g_brightness);
  }
}

}  // namespace fw
