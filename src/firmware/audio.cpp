#include "audio.h"
#include "board.h"
#include <M5GFX.h>

#if CFG_AUDIO
#include <M5Unified.h>
#endif

namespace fw {

#if CFG_AUDIO

namespace {
bool g_ready = false;
}

void audio_init() {
  M5.Speaker.begin();
  M5.Speaker.setVolume(120);
  g_ready = true;
}

void audio_deinit() {
  if (g_ready) M5.Speaker.end();
  g_ready = false;
}

void audio_set_volume(uint8_t percent) {
  if (percent > 100) percent = 100;
  M5.Speaker.setVolume((uint8_t)(percent * 255 / 100));
}

void audio_tone(bool on) {
  if (!g_ready) return;
  if (on)
    M5.Speaker.tone((float)CFG_SIDEZONE_TONE_HZ);  // duration 默认 UINT32_MAX = 持续到 stop
  else
    M5.Speaker.stop();
}

void audio_error_beep() {
  if (!g_ready) return;
  M5.Speaker.stop();
  M5.Speaker.tone(320.0f, 70);
}

#else
void audio_init() {}
void audio_deinit() {}
void audio_set_volume(uint8_t) {}
void audio_tone(bool) {}
void audio_error_beep() {}
#endif

}  // namespace fw
