#include "hush.hpp"

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <wait.h>

#include <iostream>

pid_t fork_or_panic() {
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork failed");
    std::exit(EXIT_FAILURE);
  }
  return pid;
}

void Seq::run() {
  if (fork_or_panic() == 0) {
    first->run();
  } else {
    int status;
    wait(&status);
    next->run();
  }
}

void Exec::run() {
  if (argv[0] == nullptr)
    std::exit(EXIT_FAILURE);

  execvp(argv[0], (char *const *)argv);
  std::cerr << "\nError: exec failed for: " << argv[0] << std::endl;
  exit(EXIT_FAILURE);
}

void Back::run() {
  pid_t pid = fork_or_panic();
  if (pid == 0) {
    cmd->run();
  }
  // Do not wait. Let the child become an orphan process.
  exit(EXIT_SUCCESS);
}

void Redir::run() {
  close(fd);
  if (open(file, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) < 0) {
    perror("failed to open file for redirection");
    std::exit(EXIT_FAILURE);
  }
  cmd->run();
}

void Pipe::run() {
  int p[2];
  if (pipe(p) < 0) {
    perror("pipe failed");
    std::exit(EXIT_FAILURE);
  }

  pid_t pid = fork_or_panic();

  if (pid == 0) {
    // left cmd
    close(1);  // close STDOUT
    dup(p[1]); // now fd 1 is same as p[1]
    close(p[0]);
    close(p[1]);

    left->run();
  }

  pid_t pid2 = fork_or_panic();

  if (pid2 == 0) {
    // right cmd
    close(0);  // close STDIN
    dup(p[0]); // now fd 0 is same as p[0]
    close(p[0]);
    close(p[1]);

    right->run();
  }

  close(p[0]);
  close(p[1]);

  int status;
  wait(&status);
  wait(&status);

  exit(EXIT_SUCCESS);
}

void Hush::tokenize()
{
  for (size_t curr = 0; curr < m_cmd.size(); ++curr) {
    switch (m_cmd.at(curr)) {
    case ' ' :
    case '\n':
    case '\t':
    case '\v':
    case '\r': break;

    case '&': m_tokens.emplace_back(Token(Token::Type::AMPERSAND, curr)); break;
    case '|': m_tokens.emplace_back(Token(Token::Type::PIPE, curr)); break;
    case ';': m_tokens.emplace_back(Token(Token::Type::SEMICOLON, curr)); break;
    case '(': m_tokens.emplace_back(Token(Token::Type::LEFT_PAREN, curr)); break;
    case ')': m_tokens.emplace_back(Token(Token::Type::RIGHT_PAREN, curr)); break;
    case '<': m_tokens.emplace_back(Token(Token::Type::REDIR_INPUT, curr)); break;
    case '>':
      if (curr < m_cmd.size() - 1 && m_cmd.at(curr + 1) == '>') {
        m_tokens.emplace_back(Token(Token::Type::REDIR_OUTPUT_APPEND, curr));
	++curr;
      }
      else m_tokens.emplace_back(Token(Token::Type::REDIR_OUTPUT, curr));
      break;

    default:
      m_tokens.emplace_back(Token(Token::Type::ARGUMENT, curr));
      auto i = m_cmd.find_first_of(" \t\n;|&<>()", curr);
      if (i != std::string::npos) {
        if (std::isspace(m_cmd[i]))
          m_cmd[i] = '\0';
        else
          m_cmd.insert(m_cmd.begin() + i, '\0');
	curr = i;
      } else curr = m_cmd.size();
    }
  }
}

inline auto Hush::parse() const -> std::unique_ptr<Cmd>
{
  return std::unique_ptr<Cmd>(parse_seq(0, m_tokens.size()));
}

auto Hush::parse_seq(size_t start, size_t end) const -> Cmd *
{
  for (auto i = start; i < end; ++i) {
    if (m_tokens.at(i).type == Token::Type::SEMICOLON) {
      auto seq = new Seq();
      seq->first = parse_back(start, i);
      seq->next = parse_seq(i + 1, end);
      return seq;
    }
  }
  return parse_back(start, end);
}

auto Hush::parse_back(size_t start, size_t end) const -> Cmd *
{
  for (auto i = start; i < end; ++i) {
    if (m_tokens.at(i).type == Token::Type::AMPERSAND) {
      auto back = new Back();
      back->cmd = parse_pipe(start, i);
      return back;
    }
  }
  return parse_pipe(start, end);
}

auto Hush::parse_pipe(size_t start, size_t end) const -> Cmd *
{
  for (auto i = start; i < end; ++i) {
    if (m_tokens.at(i).type == Token::Type::PIPE) {
      auto pipe = new Pipe();
      pipe->left = parse_redir(start, i);
      pipe->right = parse_pipe(i + 1, end);
      return pipe;
    }
  }
  return parse_redir(start, end);
}

auto Hush::parse_redir(size_t start, size_t end) const -> Cmd *
{
  for (auto i = start; i < end; ++i) {

    if (m_tokens[i].type == Token::Type::REDIR_INPUT ||
	m_tokens[i].type == Token::Type::REDIR_OUTPUT ||
	m_tokens[i].type == Token::Type::REDIR_OUTPUT_APPEND) {

      auto redir = new Redir();
      // TODO: handle edge case
      redir->file = m_cmd.c_str() + m_tokens.at(i+1).position;

      switch (m_tokens[i].type) {
      case Token::Type::REDIR_INPUT:
	redir->fd = 0;
	redir->mode = O_RDONLY;
	break;
      case Token::Type::REDIR_OUTPUT:
	redir->fd = 1;
	redir->mode = O_WRONLY | O_CREAT | O_TRUNC;
	break;
      case Token::Type::REDIR_OUTPUT_APPEND:
	redir->fd = 1;
	redir->mode = O_WRONLY | O_CREAT | O_APPEND;
      default: break;
      }
      redir->cmd = parse_exec(start, i);
      return redir;
    }
  }
  return parse_exec(start, end);
}

auto Hush::parse_exec(size_t start, size_t end) const -> Cmd *
{
  auto size = end - start;
  if (size <= 0) {
    std::cerr << "Error: parse_exec, end - start = " << size << std::endl;
    std::exit(EXIT_FAILURE);
  }

  auto argv = new const char*[size + 1];
  argv[size] = nullptr;

  for (auto i = start, j = 0ul; i < end; ++i, ++j) {
    if (m_tokens[i].type != Token::Type::ARGUMENT) {
      std::cerr << "Error: unexpected token found at: " << m_tokens[i].position
                << std::endl;
      std::exit(EXIT_FAILURE);
    }
    argv[j] = m_cmd.c_str() + m_tokens[i].position;
  }

  auto exec_node = new Exec();
  exec_node->argv = argv;

  return exec_node;
}

void Hush::run(const std::string &cmd) {
  m_cmd = cmd;
  m_tokens.clear();

  tokenize();

  if (m_tokens[0].type == Token::Type::ARGUMENT &&
      !strcmp("cd", m_cmd.c_str() + m_tokens[0].position)) {
    if (m_tokens[1].type != Token::Type::ARGUMENT) {
      std::cerr << "syntax error" << std::endl;
    } else {
      auto dir = m_cmd.c_str() + m_tokens[1].position;
      if (chdir(dir) < 0) {
	perror("cd failed");
      }
    }
    return;
  }

  auto root = parse();

  if (fork_or_panic() == 0) {
    root->run();
    std::exit(EXIT_SUCCESS);
  } else {
    int status;
    wait(&status);
  }
}
