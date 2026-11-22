#include "settings.h"
#include <Preferences.h>

namespace fw {

namespace {
volatile bool g_pending = false;
morse::Config g_pending_cfg;
}  // namespace

void settings_request_save(const morse::Config& cfg) {
  g_pending_cfg = cfg;
  g_pending = true;
}

void settings_service_pending() {
  if (!g_pending) return;
  g_pending = false;
  settings_save(g_pending_cfg);
}

void settings_load(morse::Config* cfg) {
  Preferences p;
  if (!p.begin("morse", true)) return;
  cfg->unit_us = p.getUInt("u", cfg->unit_us);
  cfg->auto_unit = p.getBool("au", cfg->auto_unit);
  cfg->sidetone = p.getBool("st", cfg->sidetone);
  cfg->volume = p.getUChar("vol", cfg->volume);
  cfg->tone_hz = p.getUShort("hz", cfg->tone_hz);
  cfg->trainer = false;  // 练习模式每次开机默认关，避免拿旧题目迷惑使用者
  p.end();
}

void settings_save(const morse::Config& cfg) {
  Preferences p;
  if (!p.begin("morse", false)) return;
  p.putUInt("u", cfg.unit_us);
  p.putBool("au", cfg.auto_unit);
  p.putBool("st", cfg.sidetone);
  p.putUChar("vol", cfg.volume);
  p.putUShort("hz", cfg.tone_hz);
  p.end();
}

}  // namespace fw
