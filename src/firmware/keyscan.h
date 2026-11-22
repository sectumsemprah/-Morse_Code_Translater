#pragma once
#include <cstdint>
#include "core/events.h"

namespace fw {

void keyscan_init(morse::EventQueue* q);

// 电平同步兜底：中断抖动可能让记录状态与真实引脚脱节，
// 用 2ms 时基补发一次漏掉的按下边沿，避免整个按键被吞掉。
void keyscan_reconcile(uint64_t ts_us);

uint32_t keyscan_irq_count();

}  // namespace fw
