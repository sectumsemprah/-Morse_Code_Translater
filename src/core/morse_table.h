#pragma once
#include <cstdint>
#include <cstddef>

namespace morse {

enum class Kind : uint8_t { None, Letter, Digit, Punct, Action };
enum class Action : uint8_t { None, FieldSep, CommitLine, Wait, End, Backspace };

struct Node {
  char text[5];
  Kind kind;
  Action action;
  int16_t child[2];  // [0]=dot, [1]=dash, -1 = 不存在
};

// 二叉 trie：· 走左，− 走右。除了查表，还能回答"当前前缀还可能是哪些字符"，
// 界面用它做实时候选提示。
class Table {
 public:
  Table();

  static int root() { return 0; }
  // 返回新的节点号，-1 表示这条前缀不存在
  int step(int node, bool dash) const;
  bool value(int node, Kind* kind, Action* action, const char** text) const;
  // 收集 node 子树下所有可达的字母/数字，写入 out（NUL 结尾），返回个数
  int candidates(int node, char* out, size_t cap) const;
  // 反查：'H' -> "...."，找不到返回 nullptr（练习模式出题用）
  const char* code_of(char c) const;
  int node_count() const { return count_; }

 private:
  void insert(const char* code, const char* text, Kind kind, Action action);
  void collect(int node, char* out, size_t cap, size_t* n) const;

  static const int kMaxNodes = 256;
  Node node_[kMaxNodes];
  int count_;
};

}  // namespace morse
