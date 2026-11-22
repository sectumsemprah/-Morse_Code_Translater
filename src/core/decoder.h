#pragma once
#include <cstdint>
#include "morse_table.h"
#include "textbuf.h"

namespace morse {

class Decoder {
 public:
  static const int kMaxElements = 8;

  explicit Decoder(const Table& t) : t_(t) { reset_symbol(); }

  void element(bool dash);
  // 提交当前符号串。emitted 返回本次真正落到文本上的字符（没有则为 0），
  // 返回值是该序列携带的 prosign 动作。
  Action commit(char* emitted);
  void space();
  void backspace();
  void clear_line();
  void new_line();
  void reset_symbol();

  const char* code() const { return code_; }        // ".-.." 形式，界面自行画点/划
  int pending_elements() const { return len_; }
  bool prefix_valid() const { return node_ >= 0; }
  const char* candidates() const;                  // 当前前缀还可能是哪些字符
  bool last_error() const { return error_; }
  void clear_error() { error_ = false; }

  TextBuf& text() { return text_; }
  const TextBuf& text() const { return text_; }
  uint32_t chars() const { return chars_; }
  uint32_t errors() const { return errors_; }

 private:
  const Table& t_;
  TextBuf text_;
  char code_[kMaxElements + 2];
  int len_;
  int node_;
  mutable char cand_[32];
  bool error_;
  uint32_t chars_;
  uint32_t errors_;
};

}  // namespace morse
