#pragma once
#include <cstdint>
#include "core/engine.h"

namespace fw {

void ui_init();
void ui_render(const morse::EngineState& s, int battery_percent);

}  // namespace fw
