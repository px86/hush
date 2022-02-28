#pragma once

#include <string>
#include <vector>
#include <memory>

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
  virtual ~Cmd() {}
};

class Seq : public Cmd {
public:
  Cmd *first;
  Cmd *next;

  void run();
  ~Seq() {
    delete first;
    delete next;
  }
};

class Pipe : public Cmd {
public:
  Cmd *left;
  Cmd *right;

  void run();
  ~Pipe() {
    delete left;
    delete right;
  }
};

class Redir : public Cmd {
public:
  Cmd *cmd;
  const char *file;
  int fd;
  mode_t mode;

  void run();
  ~Redir() { delete cmd; }
};

class Exec : public Cmd {
public:
  const char **argv;

  void run();
  ~Exec() { delete[] argv; }
};

class Back : public Cmd {
public:
  Cmd *cmd;

  void run();
  ~Back() { delete cmd; }
};

inline pid_t fork_or_panic();

class Hush {
public:
  Hush() = default;
  void run(const std::string &cmd);
private:
  void tokenize();
  auto parse() const -> std::unique_ptr<Cmd>;

  auto parse_seq(size_t start, size_t end) const -> Cmd *;
  auto parse_pipe(size_t start, size_t end) const -> Cmd *;
  auto parse_redir(size_t start, size_t end) const -> Cmd *;
  auto parse_back(size_t start, size_t end) const -> Cmd *;
  auto parse_exec(size_t start, size_t end) const -> Cmd *;

  std::string m_cmd;
  std::vector<Token> m_tokens;
};
