#ifndef PIPS_READLINE_HPP_
#define PIPS_READLINE_HPP_

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <vector>
#ifdef __unix__
#include <termios.h>
#include <unistd.h>
#endif

namespace pips {

#ifdef __unix__
enum class Key : char {
  CTRL_A   = 1,   // move to start of line
  CTRL_C   = 3,   // cancel / KeyboardInterrupt
  CTRL_D   = 4,   // EOF (when line is empty)
  CTRL_E   = 5,   // move to end of line
  CTRL_H   = 8,   // backspace (alternate)
  NEWLINE  = '\n',
  CR       = '\r',
  ESCAPE   = '\x1b',
  BACKSPACE = 127,
  PRINTABLE_START = 32,
};
#endif  // __unix__

// Minimal line editor with history
inline std::optional<std::string> pips_readline(const char *prompt,
                                                std::vector<std::string> &history) {
#ifdef __unix__
  printf("%s", prompt);
  fflush(stdout);

  if (!isatty(STDIN_FILENO)) {
    char buf[1024];
    if (!fgets(buf, sizeof(buf), stdin)) return std::nullopt;
    return std::string(buf);
  }

  struct termios orig, raw;
  tcgetattr(STDIN_FILENO, &orig);
  raw = orig;
  raw.c_lflag &= ~(ECHO | ICANON | ISIG); 
  raw.c_cc[VMIN]  = 1;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  auto restore = [&] { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig); };

  std::string line;
  int cursor   = 0;
  int hist_pos = static_cast<int>(history.size());
  std::string saved;  // stash current line when navigating history

  auto redraw = [&] {
    printf("\r\033[K%s%s", prompt, line.c_str());
    int behind = static_cast<int>(line.size()) - cursor;
    if (behind > 0) printf("\033[%dD", behind);
    fflush(stdout);
  };

  for (;;) {
    char c;
    ssize_t n;
    do { n = ::read(STDIN_FILENO, &c, 1); } while (n == -1 && errno == EINTR);
    if (n <= 0) { restore(); return std::nullopt; }

    if (c == static_cast<char>(Key::NEWLINE) || c == static_cast<char>(Key::CR)) {
      printf("\n"); fflush(stdout);
      restore();
      return line + '\n';
    }
    if (c == static_cast<char>(Key::CTRL_C)) {  // cancel current line, re-prompt
      printf("\nKeyboardInterrupt\n"); fflush(stdout);
      restore();
      return std::string("\n");
    }
    if (c == static_cast<char>(Key::CTRL_D)) {  // EOF only if line is empty
      if (line.empty()) { printf("\n"); fflush(stdout); restore(); return std::nullopt; }
      continue;
    }
    if (c == static_cast<char>(Key::BACKSPACE) || c == static_cast<char>(Key::CTRL_H)) {
      if (cursor > 0) { line.erase(--cursor, 1); redraw(); }
      continue;
    }
    if (c == static_cast<char>(Key::CTRL_A)) { cursor = 0; redraw(); continue; }                // move to start
    if (c == static_cast<char>(Key::CTRL_E)) { cursor = (int)line.size(); redraw(); continue; } // move to end
    if (c == static_cast<char>(Key::ESCAPE)) {  // Escape sequence
      struct termios tmp = raw;
      tmp.c_cc[VMIN]  = 0;
      tmp.c_cc[VTIME] = 1;  // 100 ms
      tcsetattr(STDIN_FILENO, TCSANOW, &tmp);
      char seq[2] = {0, 0};
      ::read(STDIN_FILENO, &seq[0], 1);
      ::read(STDIN_FILENO, &seq[1], 1);
      tcsetattr(STDIN_FILENO, TCSANOW, &raw);

      if (seq[0] == '[') {
        switch (seq[1]) {
        case 'A':  // Up
          if (hist_pos > 0) {
            if (hist_pos == (int)history.size()) saved = line;
            line   = history[--hist_pos];
            cursor = (int)line.size();
            redraw();
          }
          break;
        case 'B':  // Down
          if (hist_pos < (int)history.size()) {
            ++hist_pos;
            line   = (hist_pos == (int)history.size()) ? saved : history[hist_pos];
            cursor = (int)line.size();
            redraw();
          }
          break;
        case 'C':  // Right
          if (cursor < (int)line.size()) { ++cursor; printf("\033[C"); fflush(stdout); }
          break;
        case 'D':  // Left
          if (cursor > 0) { --cursor; printf("\033[D"); fflush(stdout); }
          break;
        }
      }
      continue;
    }
    if (c >= static_cast<char>(Key::PRINTABLE_START)) {  // printable character
      line.insert(cursor++, 1, c);
      redraw();
    }
  }
#else
  printf("%s", prompt);
  fflush(stdout);
  char buf[1024];
  if (!fgets(buf, sizeof(buf), stdin)) return std::nullopt;
  return std::string(buf);
#endif
}

} // namespace pips-
#endif // PIPS_READLINE_HPP_
