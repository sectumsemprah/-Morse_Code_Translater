#pragma once
#include <cstdint>
#include <cstddef>

namespace morse {

// 定长环形文本缓冲。摩斯打出来的是纯 ASCII（字母/数字/标点），
// 不需要 UTF-8 与字库，这也是"只做拉丁字母和数字"省掉的最大一块复杂度。
class TextBuf {
 public:
  static const size_t kCap = 768;

  void push(char c);
  bool pop();  // 当前行空时会吃掉换行符，跨行退格符合直觉
  void newline();
  void clear();

  size_t size() const { return len_; }
  bool empty() const { return len_ == 0; }
  char last_char() const { return len_ ? buf_[len_ - 1] : 0; }

  // 当前行（最后一个 '\n' 之后）的末尾 max 个字符，供窄屏左截断显示
  void line_tail(char* out, size_t cap) const;
  void prev_line_tail(char* out, size_t cap) const;
  size_t line_len() const;
  void full(char* out, size_t cap) const;

 private:
  size_t line_start() const;

  char buf_[kCap];
  size_t len_ = 0;
};

}  // namespace morse
