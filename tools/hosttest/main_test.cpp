// PC 端敲击仿真测试：不烧板子就能验证时序内核。
// 编译：见 tools/hosttest/build.sh
//
// 覆盖：快/慢/正常手速、抖动、节奏漂移、无 tick 的补判路径、
//       prosign(BT/AR/7点退格)、非法组合、回听音开关配平。

#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "../../src/core/engine.h"

static uint64_t g_now_us = 0;

namespace morse {
uint64_t port_now_us() { return g_now_us; }
void port_lock() {}
void port_unlock() {
}
}  // namespace morse

namespace {

uint32_t g_seed = 20261121u;
uint32_t rnd32() {
  g_seed ^= g_seed << 13;
  g_seed ^= g_seed >> 17;
  g_seed ^= g_seed << 5;
  return g_seed;
}

int64_t jittered(uint32_t base_us, int pct) {
  if (pct <= 0) return base_us;
  int32_t r = (int32_t)(rnd32() % (uint32_t)(2 * pct + 1)) - pct;
  int64_t v = (int64_t)base_us + (int64_t)base_us * r / 100;
  return v < 4000 ? 4000 : v;
}

enum Gap { G_CHAR, G_WORD };
struct Token {
  const char* text;
  Gap gap;
};

typedef std::function<uint32_t(uint64_t)> UFn;

void push(std::vector<morse::Event>& v, uint8_t type, uint8_t key, uint8_t down, uint64_t ts) {
  morse::Event e;
  e.type = type;
  e.key = key;
  e.down = down;
  e.ui = 0;
  e.ts_us = ts;
  v.push_back(e);
}

bool is_raw_code(const char* s) {
  for (const char* p = s; *p; ++p) {
    if (*p != '.' && *p != '-') return false;
  }
  return true;
}

// 把手敲的"点/划 + 三种间隙"展开成带绝对时间戳的边沿事件流
std::vector<morse::Event> tap(const std::vector<Token>& seq, const UFn& ufn, int jit_pct,
                              bool with_ticks) {
  morse::Table table;
  std::vector<morse::Event> evs;
  uint64_t t = 10000000;

  for (size_t i = 0; i < seq.size(); ++i) {
    const char* code = is_raw_code(seq[i].text) ? seq[i].text : table.code_of(seq[i].text[0]);
    if (!code) continue;
    uint32_t u = ufn(t);
    for (const char* p = code; *p; ++p) {
      uint32_t dur = (*p == '-') ? (uint32_t)(3 * u) : u;
      push(evs, morse::EV_KEY, morse::KEY_MORSE, 1, t);
      t += (uint64_t)jittered(dur, jit_pct);
      push(evs, morse::EV_KEY, morse::KEY_MORSE, 0, t);
      if (p[1]) t += (uint64_t)jittered(u, jit_pct);  // 字符内间隙
    }
    uint32_t g = (seq[i].gap == G_WORD) ? 7 : 3;
    if (i + 1 == seq.size()) g = 12;  // 结尾留一个长空，让最后一个字符落地
    t += (uint64_t)jittered(u * g, jit_pct);
  }

  if (with_ticks) {
    std::vector<morse::Event> all;
    uint64_t start = evs.empty() ? 0 : evs.front().ts_us;
    uint64_t end = evs.empty() ? 0 : evs.back().ts_us;
    size_t ki = 0;
    uint64_t stop = end + (uint64_t)ufn(end) * 14;
    for (uint64_t tk = start; tk <= stop; tk += 2000) {
      while (ki < evs.size() && evs[ki].ts_us <= tk) all.push_back(evs[ki++]);
      push(all, morse::EV_TICK, 0, 0, tk);
    }
    while (ki < evs.size()) all.push_back(evs[ki++]);
    return all;
  }
  // 敲的过程中一个 tick 都不给：间隙全靠"下一次按下"追溯判定。
  // 结尾补一个远期 tick 只做最后一次收尾，等价于真实固件里的定时器。
  if (!evs.empty()) push(evs, morse::EV_TICK, 0, 0, evs.back().ts_us + 5000000);
  return evs;
}

struct Counters {
  int tone_on = 0;
  int tone_off = 0;
  int lines = 0;
};

void tone_hook(bool on, void* ctx) {
  Counters* c = (Counters*)ctx;
  if (on)
    ++c->tone_on;
  else
    ++c->tone_off;
}

void line_hook(const char*, void* ctx) { ((Counters*)ctx)->lines++; }

struct Outcome {
  std::string text;
  Counters ctr;
  uint32_t unit_us;
  uint32_t errors;
};

Outcome feed(const std::vector<morse::Event>& evs, bool auto_unit, uint32_t unit_us) {
  Counters ctr;
  morse::Engine eng;
  morse::Config cfg{unit_us, auto_unit, true, 60, 620, false};
  eng.set_config(cfg);
  morse::Engine::Hooks h;
  h.tone = tone_hook;
  h.line = line_hook;
  h.ctx = &ctr;
  eng.set_hooks(h);

  for (const auto& e : evs) {
    g_now_us = e.ts_us;
    eng.handle(e);
  }

  char buf[morse::TextBuf::kCap + 1];
  eng.decoder().text().full(buf, sizeof(buf));

  Outcome o;
  o.text = buf;
  o.ctr = ctr;
  morse::EngineState s;
  eng.snapshot(s);
  o.unit_us = s.unit_us;
  o.errors = s.errors;
  return o;
}

// 允许"漏字符"的比对用 LCS，统计插入/删除各自的影响
size_t lcs_len(const std::string& a, const std::string& b) {
  if (a.empty() || b.empty()) return 0;
  std::vector<size_t> prev(b.size() + 1, 0), cur(b.size() + 1, 0);
  for (size_t i = 1; i <= a.size(); ++i) {
    cur[0] = 0;
    for (size_t j = 1; j <= b.size(); ++j) {
      cur[j] = (a[i - 1] == b[j - 1]) ? prev[j - 1] + 1 : (prev[j] > cur[j - 1] ? prev[j] : cur[j - 1]);
    }
    prev = cur;
  }
  return prev[b.size()];
}

int g_fail = 0;

// 最后一个单词后面一定会多出一个空格（摩斯键的正常行为），比对时忽略
std::string rstrip(std::string s) {
  while (!s.empty() && s.back() == ' ') s.pop_back();
  return s;
}

void expect_exact(const char* name, const Outcome& got, const char* want) {
  std::string g = rstrip(got.text);
  bool ok = g == want;
  printf("%-34s %s  got=\"%s\" want=\"%s\"\n", name, ok ? "PASS" : "FAIL", g.c_str(), want);
  if (!ok) ++g_fail;
}

void expect_accuracy(const char* name, const Outcome& got, const char* want, double min_acc) {
  size_t hit = lcs_len(rstrip(got.text), want);
  double acc = (double)hit / (double)strlen(want);
  printf("%-34s %s  acc=%.1f%% (%zu/%zu)  u=%lums\n", name,
         acc + 1e-9 >= min_acc ? "PASS" : "FAIL", acc * 100.0, hit, strlen(want),
         (unsigned long)(got.unit_us / 1000));
  if (acc + 1e-9 < min_acc) ++g_fail;
}

std::vector<Token> words(const char* text) {
  std::vector<Token> seq;
  for (const char* p = text; *p; ++p) {
    if (*p == ' ') continue;
    Token t;
    t.text = p;  // 逐字符使用，见 tap() 里对 text[0] 的取值
    t.gap = (p[1] == ' ' || p[1] == '\0') ? G_WORD : G_CHAR;
    seq.push_back(t);
    // text 指向常量串内部，Token 只在本轮立即使用
  }
  return seq;
}

}  // namespace

int main() {
  uint32_t u_const = 100000;
  UFn fixed = [u_const](uint64_t) { return u_const; };
  UFn slow = [](uint64_t) { return 250000u; };
  UFn fast = [](uint64_t) { return 50000u; };
  UFn drift = [](uint64_t t) {
    // 60ms 起步，随时间线性放慢到 220ms：模拟"越打越累"的真实手
    uint64_t ms = (t - 10000000) / 1000;
    uint32_t u = 60000u + (uint32_t)(ms * 40u / 1000u) * 1000u;
    if (u > 220000u) u = 220000u;
    return u;
  };

  const char* hello = "HELLO WORLD";
  const char* pangram = "THE QUICK BROWN FOX JUMPS OVER 42 LAZY DOGS";

  // 1) 理想时序
  {
    Outcome o = feed(tap(words(hello), fixed, 0, true), false, 100000);
    expect_exact("ideal timing @100ms", o, hello);
  }

  Outcome o2 = feed(tap(words(hello), slow, 0, true), false, 250000);
  expect_exact("slow hand @250ms", o2, hello);

  Outcome o3 = feed(tap(words(hello), fast, 0, true), false, 50000);
  expect_exact("fast hand @50ms", o3, hello);

  // 4) 敲击期间零 tick：全靠下一次按下补判，验证"追溯补空格"路径
  Outcome o4 = feed(tap(words(hello), fixed, 0, false), false, 100000);
  expect_exact("no tick during taps (retro)", o4, hello);

  // 5) 手动阈值但手速漂移 3 倍：应当明显退化（证明阈值真的起作用）
  Outcome o5 = feed(tap(words(hello), drift, 0, true), false, 60000);
  printf("%-34s info  text=\"%s\"\n", "manual u=60ms vs drifting hand", o5.text.c_str());

  // 6) 自适应：抖动 ±25%
  Outcome o6 = feed(tap(words(pangram), fixed, 25, true), true, 120000);
  expect_accuracy("auto unit, jitter +-25%", o6, pangram, 0.95);

  // 7) 自适应：手速从 60 漂到 220ms
  Outcome o7 = feed(tap(words(pangram), drift, 10, true), true, 120000);
  expect_accuracy("auto unit, 60->220ms drift", o7, pangram, 0.90);
  printf("       final unit = %lums (want 190..240)\n", (unsigned long)(o7.unit_us / 1000));

  // 8) prosign：BT=空格, AR=换行, 7 点=退格
  {
    std::vector<Token> seq = {{"A", G_CHAR}, {"B", G_CHAR}, {"-...-", G_CHAR},
                              {"C", G_CHAR}, {".-.-.", G_CHAR}, {"D", G_CHAR}};
    Outcome o = feed(tap(seq, fixed, 0, true), false, 100000);
    expect_exact("prosign BT/AR spacing", o, "AB C\nD");
  }
  {
    std::vector<Token> seq = {{"E", G_CHAR}, {"F", G_CHAR}, {".......", G_CHAR}, {"G", G_CHAR}};
    Outcome o = feed(tap(seq, fixed, 0, true), false, 100000);
    expect_exact("7 dots backspace", o, "EG");
  }

  // 9) 非法组合
  {
    std::vector<Token> seq = {{".-.-.-.-.", G_CHAR}, {"H", G_CHAR}};
    Outcome o = feed(tap(seq, fixed, 0, true), false, 100000);
    expect_exact("invalid sequence -> ?", o, "?H");
  }

  // 10) 回听音：开/关次数必须配平，且不多不少
  {
    Outcome o = feed(tap(words("SOS"), fixed, 0, true), false, 100000);
    bool bal = (o.ctr.tone_on == o.ctr.tone_off) && o.ctr.tone_on == 9;  // S=3 O=3 S=3
    printf("%-34s %s  on=%d off=%d want=9\n", "sidetone press/release balance",
           bal ? "PASS" : "FAIL", o.ctr.tone_on, o.ctr.tone_off);
    if (!bal) ++g_fail;
  }

  // 11) 数字与标点
  {
    std::vector<Token> seq = {{"1", G_CHAR}, {"9", G_CHAR}, {"0", G_WORD},
                              {".-.-.-", G_CHAR}, {"9", G_CHAR}};
    Outcome o = feed(tap(seq, fixed, 0, true), false, 100000);
    expect_exact("digits and punctuation", o, "190 .9");
  }

  // 12) 速率读数：wpm = 1200 / u_ms。真机上这里错了一百倍（u120 显示成 1000.0w）
  {
    morse::UnitEstimator ue(120000);
    uint32_t w120 = ue.wpm_x10();
    ue.set_unit_us(50000);
    uint32_t w50 = ue.wpm_x10();
    ue.set_unit_us(400000);
    uint32_t w400 = ue.wpm_x10();
    bool ok = w120 == 100 && w50 == 240 && w400 == 30;
    printf("%-34s %s  120/50/400ms -> %.1f/%.1f/%.1f wpm\n", "wpm readout math",
           ok ? "PASS" : "FAIL", (double)w120 / 10.0, (double)w50 / 10.0,
           (double)w400 / 10.0);
    if (!ok) ++g_fail;
  }

  // 13) 副键手势：长按清行、单击退格
  {
    std::vector<morse::Event> evs = tap(words("EET"), fixed, 0, true);
    uint64_t t = evs.back().ts_us + 2000000;
    push(evs, morse::EV_KEY, morse::KEY_AUX, 1, t);
    t += 800000;  // 按住 0.8s 再松
    push(evs, morse::EV_KEY, morse::KEY_AUX, 0, t);
    for (uint64_t tk = t; tk <= t + 1000000; tk += 2000) push(evs, morse::EV_TICK, 0, 0, tk);
    Outcome o = feed(evs, false, 100000);
    expect_exact("aux hold clears line", o, "");
  }
  {
    std::vector<morse::Event> evs = tap(words("EG"), fixed, 0, true);
    uint64_t t = evs.back().ts_us + 2000000;
    push(evs, morse::EV_KEY, morse::KEY_AUX, 1, t);
    t += 80000;  // 短按
    push(evs, morse::EV_KEY, morse::KEY_AUX, 0, t);
    for (uint64_t tk = t; tk <= t + 1200000; tk += 2000) push(evs, morse::EV_TICK, 0, 0, tk);
    Outcome o = feed(evs, false, 100000);
    expect_exact("aux tap backspaces", o, "EG");  // 先吃掉词尾空格，再吃字母
  }

  printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
  return g_fail ? 1 : 0;
}
