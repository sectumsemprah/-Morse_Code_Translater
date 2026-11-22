#include "textbuf.h"
#include <cstring>

namespace morse {

void TextBuf::push(char c) {
  if (len_ == kCap) {  // 丢最旧的 1/4，保留最近的上下文
    size_t drop = kCap / 4;
    memmove(buf_, buf_ + drop, len_ - drop);
    len_ -= drop;
  }
  buf_[len_++] = c;
}

size_t TextBuf::line_start() const {
  size_t i = len_;
  while (i > 0) {
    --i;
    if (buf_[i] == '\n') return i + 1;
  }
  return 0;
}

size_t TextBuf::line_len() const { return len_ - line_start(); }

bool TextBuf::pop() {
  if (len_ == 0) return false;
  --len_;
  return true;
}

void TextBuf::newline() {
  if (len_ > 0 && buf_[len_ - 1] == '\n') return;
  push('\n');
}

void TextBuf::clear() { len_ = 0; }

void TextBuf::line_tail(char* out, size_t cap) const {
  if (cap == 0) return;
  size_t start = line_start();
  size_t n = len_ - start;
  if (n > cap - 1) {
    start += n - (cap - 1);
    n = cap - 1;
  }
  memcpy(out, buf_ + start, n);
  out[n] = '\0';
}

void TextBuf::prev_line_tail(char* out, size_t cap) const {
  if (cap == 0) return;
  out[0] = '\0';
  size_t start = line_start();
  if (start == 0) return;
  size_t prev_end = start - 1;
  size_t i = prev_end;
  while (i > 0) {
    --i;
    if (buf_[i] == '\n') {
      ++i;
      break;
    }
  }
  size_t n = prev_end - i;
  if (n > cap - 1) {
    i += n - (cap - 1);
    n = cap - 1;
  }
  memcpy(out, buf_ + i, n);
  out[n] = '\0';
}

void TextBuf::full(char* out, size_t cap) const {
  if (cap == 0) return;
  size_t n = len_ < cap - 1 ? len_ : cap - 1;
  memcpy(out, buf_ + (len_ - n), n);
  out[n] = '\0';
}

}  // namespace morse
