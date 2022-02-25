#include "hush.hpp"
#include "linereader.hpp"

#include <iostream>

int main(int argc, char **argv) {
  auto lr = LineReader(".history");
  Hush hush;
  while (lr) {
    auto cmd = lr.readline("> ");
    if (cmd.empty()) continue;
    hush.run(cmd);
  }
  return 0;
}
