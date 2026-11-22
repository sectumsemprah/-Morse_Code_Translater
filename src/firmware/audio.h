#pragma once
#include <cstdint>

namespace fw {

void audio_init();
void audio_tone(bool on);            // 回听音：按下即开、松开即关
void audio_error_beep();             // 非法组合的双短鸣
void audio_set_volume(uint8_t percent);
void audio_deinit();

}  // namespace fw
