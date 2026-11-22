#include "link.h"
#include "board.h"
#include "audio.h"
#include "settings.h"
#include <Arduino.h>
#include <stdlib.h>

namespace fw {

namespace {

char g_line[64];
size_t g_len = 0;

int arg_of(const char* s, const char** rest) {
  while (*s && *s != ' ') ++s;
  if (*s == ' ') ++s;
  if (rest) *rest = s;
  return atoi(s);
}

void help() {
  Serial.print(
      "cmd: u <ms>  unit(rhythm)\r\n"
      "     a       auto unit on/off\r\n"
      "     t       trainer on/off\r\n"
      "     s       sidetone on/off\r\n"
      "     v <%%>   volume\r\n"
      "     e/c/n   backspace / clear line / new line\r\n"
      "     d       dump text\r\n"
      "     w       stats\r\n");
}

void handle(morse::Engine& eng, const char* cmd) {
  const char* rest = nullptr;
  int v = arg_of(cmd, &rest);
  morse::Config cfg = eng.config();

  switch (cmd[0]) {
    case 'u':
      cfg.auto_unit = false;
      cfg.unit_us = (uint32_t)(v < 0 ? 0 : v) * 1000u;
      eng.set_config(cfg);
      settings_request_save(cfg);
      Serial.printf("unit=%luus auto=0\r\n", (unsigned long)eng.config().unit_us);
      break;
    case 'a':
      cfg.auto_unit = !cfg.auto_unit;
      eng.set_config(cfg);
      settings_request_save(cfg);
      Serial.printf("auto=%d\r\n", (int)cfg.auto_unit);
      break;
    case 's':
      cfg.sidetone = !cfg.sidetone;
      eng.set_config(cfg);
      settings_request_save(cfg);
      Serial.printf("sidetone=%d\r\n", (int)cfg.sidetone);
      break;
    case 'v':
      cfg.volume = (uint8_t)(v < 0 ? 0 : (v > 100 ? 100 : v));
      eng.set_config(cfg);
      audio_set_volume(cfg.volume);
      settings_request_save(cfg);
      Serial.printf("vol=%u\r\n", cfg.volume);
      break;
    case 't':
      eng.apply(morse::UI_TOGGLE_TRAINER);
      Serial.printf("trainer=%d\r\n", (int)eng.config().trainer);
      break;
    case 'e':
      eng.apply(morse::UI_BACKSPACE);
      break;
    case 'c':
      eng.apply(morse::UI_CLEAR_LINE);
      break;
    case 'n':
      eng.apply(morse::UI_NEW_LINE);
      break;
    case 'd':
      eng.apply(morse::UI_DUMP_TEXT);
      break;
    case 'w': {
      morse::EngineState s;
      eng.snapshot(s);
      Serial.printf("u=%luus wpm=%.1f chars=%lu err=%lu hit=%lu miss=%lu ovr=%lu\r\n",
                    (unsigned long)s.unit_us, (double)s.wpm_x10 / 10.0,
                    (unsigned long)s.chars, (unsigned long)s.errors,
                    (unsigned long)s.hits, (unsigned long)s.misses,
                    (unsigned long)s.queue_overflow);
      break;
    }
    default:
      help();
      break;
  }
}

}  // namespace

void link_init() {}

void link_poll(morse::Engine& eng) {
#if CFG_SERIAL_LOG
  int c;
  while ((c = Serial.read()) >= 0) {
    if (c == '\r' || c == '\n') {
      if (g_len) {
        g_line[g_len] = '\0';
        handle(eng, g_line);
        g_len = 0;
      }
    } else if (g_len + 1 < sizeof(g_line)) {
      g_line[g_len++] = (char)c;
    }
  }
#else
  (void)eng;
#endif
}

void link_print_line(const char* text) {
#if CFG_SERIAL_LOG
  Serial.printf("[LINE] %s\r\n", text);
#else
  (void)text;
#endif
}

void link_boot_banner(const morse::Config& cfg) {
#if CFG_SERIAL_LOG
  Serial.printf("\r\nStickS3 Morse  unit=%luus auto=%d tone=%d\r\n",
                (unsigned long)cfg.unit_us, (int)cfg.auto_unit, (int)cfg.sidetone);
  help();
#else
  (void)cfg;
#endif
}

}  // namespace fw
