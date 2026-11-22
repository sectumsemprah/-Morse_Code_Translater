#include "morse_table.h"
#include <cstring>

namespace morse {

namespace {

struct Entry {
  const char* code;
  const char* text;
  Kind kind;
  Action action;
};

const Entry kTable[] = {
    {".-", "A", Kind::Letter, Action::None},
    {"-...", "B", Kind::Letter, Action::None},
    {"-.-.", "C", Kind::Letter, Action::None},
    {"-..", "D", Kind::Letter, Action::None},
    {".", "E", Kind::Letter, Action::None},
    {"..-.", "F", Kind::Letter, Action::None},
    {"--.", "G", Kind::Letter, Action::None},
    {"....", "H", Kind::Letter, Action::None},
    {"..", "I", Kind::Letter, Action::None},
    {".---", "J", Kind::Letter, Action::None},
    {"-.-", "K", Kind::Letter, Action::None},
    {".-..", "L", Kind::Letter, Action::None},
    {"--", "M", Kind::Letter, Action::None},
    {"-.", "N", Kind::Letter, Action::None},
    {"---", "O", Kind::Letter, Action::None},
    {".--.", "P", Kind::Letter, Action::None},
    {"--.-", "Q", Kind::Letter, Action::None},
    {".-.", "R", Kind::Letter, Action::None},
    {"...", "S", Kind::Letter, Action::None},
    {"-", "T", Kind::Letter, Action::None},
    {"..-", "U", Kind::Letter, Action::None},
    {"...-", "V", Kind::Letter, Action::None},
    {".--", "W", Kind::Letter, Action::None},
    {"-..-", "X", Kind::Letter, Action::None},
    {"-.--", "Y", Kind::Letter, Action::None},
    {"--..", "Z", Kind::Letter, Action::None},

    {"-----", "0", Kind::Digit, Action::None},
    {".----", "1", Kind::Digit, Action::None},
    {"..---", "2", Kind::Digit, Action::None},
    {"...--", "3", Kind::Digit, Action::None},
    {"....-", "4", Kind::Digit, Action::None},
    {".....", "5", Kind::Digit, Action::None},
    {"-....", "6", Kind::Digit, Action::None},
    {"--...", "7", Kind::Digit, Action::None},
    {"---..", "8", Kind::Digit, Action::None},
    {"----.", "9", Kind::Digit, Action::None},

    {".-.-.-", ".", Kind::Punct, Action::None},
    {"--..--", ",", Kind::Punct, Action::None},
    {"..--..", "?", Kind::Punct, Action::None},
    {"-..-.", "/", Kind::Punct, Action::None},
    {".--.-", "(", Kind::Punct, Action::None},
    {".--.-.", ")", Kind::Punct, Action::None},

    // prosign：用键入手势替代物理按键的确认/删除语义
    {"-...-", "BT", Kind::Action, Action::FieldSep},   // 字段分隔 -> 空格
    {".-.-.", "AR", Kind::Action, Action::CommitLine}, // 报文结束 -> 换行
    {"-.--.", "KN", Kind::Action, Action::Wait},       // 交给对方
    {"...--.", "SK", Kind::Action, Action::End},       // 通联结束
    {".......", "", Kind::Action, Action::Backspace},  // 标准错误信号 -> 退格
};

// 两处历史撞码，本表让位给更常用的：AR 与 "+" 同为 .-.-.，"=" 与 B 同为 -...，
// 因此不提供这两个标点，prosign 与字母表优先。

}  // namespace

Table::Table() : count_(0) {
  node_[0].text[0] = '\0';
  node_[0].kind = Kind::None;
  node_[0].action = Action::None;
  node_[0].child[0] = -1;
  node_[0].child[1] = -1;
  count_ = 1;

  for (size_t i = 0; i < sizeof(kTable) / sizeof(kTable[0]); ++i) {
    insert(kTable[i].code, kTable[i].text, kTable[i].kind, kTable[i].action);
  }
}

void Table::insert(const char* code, const char* text, Kind kind, Action action) {
  int cur = 0;
  for (const char* p = code; *p; ++p) {
    bool dash = (*p == '-');
    int nxt = node_[cur].child[dash ? 1 : 0];
    if (nxt < 0) {
      if (count_ >= kMaxNodes) return;
      nxt = count_++;
      Node& n = node_[nxt];
      n.text[0] = '\0';
      n.kind = Kind::None;
      n.action = Action::None;
      n.child[0] = -1;
      n.child[1] = -1;
      node_[cur].child[dash ? 1 : 0] = (int16_t)nxt;
    }
    cur = nxt;
  }
  Node& n = node_[cur];
  n.kind = kind;
  n.action = action;
  size_t len = strlen(text);
  if (len > sizeof(n.text) - 1) len = sizeof(n.text) - 1;
  memcpy(n.text, text, len);
  n.text[len] = '\0';
}

int Table::step(int node, bool dash) const {
  if (node < 0 || node >= count_) return -1;
  return node_[node].child[dash ? 1 : 0];
}

bool Table::value(int node, Kind* kind, Action* action, const char** text) const {
  if (node < 0 || node >= count_) return false;
  const Node& n = node_[node];
  if (n.kind == Kind::None) return false;
  if (kind) *kind = n.kind;
  if (action) *action = n.action;
  if (text) *text = n.text;
  return true;
}

void Table::collect(int node, char* out, size_t cap, size_t* n) const {
  if (node < 0 || node >= count_) return;
  const Node& nd = node_[node];
  if ((nd.kind == Kind::Letter || nd.kind == Kind::Digit) && nd.text[0] && *n + 1 < cap) {
    out[(*n)++] = nd.text[0];
    out[*n] = '\0';
  }
  collect(nd.child[0], out, cap, n);
  collect(nd.child[1], out, cap, n);
}

int Table::candidates(int node, char* out, size_t cap) const {
  if (cap == 0) return 0;
  out[0] = '\0';
  size_t n = 0;
  collect(node, out, cap, &n);
  return (int)n;
}

const char* Table::code_of(char c) const {
  for (size_t i = 0; i < sizeof(kTable) / sizeof(kTable[0]); ++i) {
    const Entry& e = kTable[i];
    if ((e.kind == Kind::Letter || e.kind == Kind::Digit) && e.text[0] == c) return e.code;
  }
  return nullptr;
}

}  // namespace morse
