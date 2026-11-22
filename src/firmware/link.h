#pragma once
#include "core/engine.h"

namespace fw {

void link_init();

// 在引擎任务里调用：单线程访问 Engine，命令处理不需要任何加锁。
void link_poll(morse::Engine& eng);
void link_print_line(const char* text);
void link_boot_banner(const morse::Config& cfg);

}  // namespace fw
