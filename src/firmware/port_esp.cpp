#include "core/events.h"
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

// 内核移植层：时间戳用 esp_timer（64 位微秒，不会像 micros() 那样 71 分钟翻转，
// 一次翻转就是几十秒假间隙，会把整行误判成空格）。
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

namespace morse {

uint64_t port_now_us() { return (uint64_t)esp_timer_get_time(); }

void port_lock() { portENTER_CRITICAL(&g_mux); }

void port_unlock() { portEXIT_CRITICAL(&g_mux); }

}  // namespace morse
