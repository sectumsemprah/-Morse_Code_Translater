#pragma once
#include "core/engine.h"

namespace fw {

// 掉电保存的只有"人的习惯"（节奏、音量、回听音），不保存文本内容：
// 文本要么当场抄下来，要么用串口导出，存 flash 反而容易让人误以为没丢。
void settings_load(morse::Config* cfg);
void settings_save(const morse::Config& cfg);

// NVS 一次写可能有几十毫秒，绝不能发生在引擎任务里（那会让回听音变迟钝）。
// 所以引擎只登记待存内容，真正写入放在 UI 任务里做。
void settings_request_save(const morse::Config& cfg);
void settings_service_pending();

}  // namespace fw
