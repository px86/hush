#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <sys/types.h>

struct Token {
  enum class Type {
    AMPERSAND,
    LEFT_PAREN,
    RIGHT_PAREN,
    PIPE,
    REDIR_INPUT,
    REDIR_OUTPUT,
    REDIR_OUTPUT_APPEND,
    SEMICOLON,
    ARGUMENT
  } type;

  size_t position;

  Token() = delete;
  Token(Token::Type type, size_t position) : type(type), position(position) {}
};


class Cmd {
public:
  virtual void run() = 0;
};

class Seq : public Cmd {
public:
  void run() override;
  Cmd *first;
  Cmd *next;
};

class Pipe : public Cmd {
public:
  void run() override;
  Cmd *left;
  Cmd *right;
};

class Redir : public Cmd {
public:
  void run() override;
  Cmd *cmd;
  const char *file;
  int fd;
  mode_t mode;
};

class Exec : public Cmd {
public:
  void run() override;
  const char **argv;
};

class Back : public Cmd {
public:
  void run() override;
  Cmd *cmd;
};

inline pid_t fork_or_panic();

class Hush {
public:
  Hush() = default;
  void run(const std::string &cmd);
private:
  void tokenize();
  auto parse() const -> Cmd *;

  auto parse_seq(size_t start, size_t end) const -> Cmd *;
  auto parse_pipe(size_t start, size_t end) const -> Cmd *;
  auto parse_redir(size_t start, size_t end) const -> Cmd *;
  auto parse_back(size_t start, size_t end) const -> Cmd *;
  auto parse_exec(size_t start, size_t end) const -> Cmd *;

  std::string m_cmd;
  std::vector<Token> m_tokens;
};
