#include "decoder.h"

namespace morse {

void Decoder::reset_symbol() {
  len_ = 0;
  node_ = Table::root();
  code_[0] = '\0';
}

void Decoder::element(bool dash) {
  if (len_ >= kMaxElements) {  // 超过任何合法字符的长度，保留缓冲让用户能删
    error_ = true;
    return;
  }
  code_[len_++] = dash ? '-' : '.';
  code_[len_] = '\0';
  node_ = t_.step(node_, dash);
}

Action Decoder::commit(char* emitted) {
  if (emitted) *emitted = 0;
  if (len_ == 0) return Action::None;

  Kind k = Kind::None;
  Action a = Action::None;
  const char* txt = "";
  bool valid = (node_ >= 0 && t_.value(node_, &k, &a, &txt));
  reset_symbol();  // 先清符号，否则下面 Backspace 会误当成"丢弃未打完的字符"

  if (!valid) {
    text_.push('?');  // 非法组合：显式报错比静默丢弃好，学习者需要知道敲错了
    if (emitted) *emitted = '?';
    error_ = true;
    ++errors_;
    a = Action::None;
  } else if (k == Kind::Action) {
    if (a == Action::Backspace) {
      text_.pop();
      error_ = false;
    }
  } else {
    for (const char* p = txt; *p; ++p) text_.push(*p);
    ++chars_;
    if (emitted && txt[0] && !txt[1]) *emitted = txt[0];
  }

  return a;
}

void Decoder::space() {
  char last = text_.last_char();
  if (last != ' ' && last != '\n' && last != 0) text_.push(' ');
}

void Decoder::backspace() {
  if (len_ > 0) {
    reset_symbol();  // 先扔掉没提交的部分，符合"打错了立刻纠正"的直觉
    return;
  }
  text_.pop();
  error_ = false;
}

void Decoder::clear_line() {
  reset_symbol();
  while (text_.line_len() > 0) text_.pop();
  error_ = false;
}

void Decoder::new_line() {
  reset_symbol();
  text_.newline();
}

const char* Decoder::candidates() const {
  if (node_ < 0) {
    cand_[0] = '\0';
    return cand_;
  }
  t_.candidates(node_, cand_, sizeof(cand_));
  return cand_;
}

}  // namespace morse
