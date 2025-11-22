#ifndef PIPS_SCANNER_HPP_
#define PIPS_SCANNER_HPP_

//===========================================================================
// Much of this code is based on the clox language from the book
// "Crafting Interpreters" by Robert Nystrom
// https://craftinginterpreters.com/contents.html which is available at
// https://github.com/munificent/craftinginterpreters under the MIT License.
// The code was adapted for C++ and simplified in many ways.
//===========================================================================
#include <cctype>
#include <cstring>

namespace pips {

enum class TokenType : unsigned int {
  // one char:
  LEFT_PAREN,
  RIGHT_PAREN,
  LEFT_BRACE,
  RIGHT_BRACE,
  COMMA,
  DOT,
  MINUS,
  PLUS,
  SEMICOLON,
  MOD,
  // one or two chars:
  SLASH,
  SLASH_SLASH,
  STAR,
  STAR_STAR,
  BANG,
  BANG_EQUAL,
  EQUAL,
  EQUAL_EQUAL,
  GREATER,
  GREATER_EQUAL,
  LESS,
  LESS_EQUAL,
  QUESTION,
  COLON,
  // literals:
  IDENTIFIER,
  STRING,
  NUMBER,
  // special constants
  PI, // MAX, MIN, FUZZ, TINY, INF ?
  // Keywords,
  AND,
  CLASS,
  ELSE,
  FALSE,
  FOR,
  FUN,
  IF,
  NIL,
  OR,
  PRINT,
  NEWLINE,
  RETURN,
  SUPER,
  THIS,
  TRUE,
  VAR,
  WHILE,

  // Special unary functions (almost all of <cmath>)
  EXP,
  SIN,
  COS,
  TAN,
  ABS,
  LOG,
  LOG10,
  SIGN,
  SQRT,
  ACOS,
  ASIN,
  ATAN,
  CEIL,
  FLOOR,
  // LOG2, sinh, cosh, tanh, erf, tgamma, round,

  // special binary functions
  ATAN2,
  MIN,
  MAX,

  ERROR,
  END
};

struct Token {
  TokenType type;
  const char *start;
  int length;
  int line;

  Token() {
    length = 0;
    line = 0;
  }
  Token(const char *msg, int line_) : type(TokenType::ERROR), start(msg), line(line_) {
    length = static_cast<int>(std::strlen(msg));
  }
  Token(TokenType type_, const char *start_, const char *current_, int line_)
      : type(type_), start(start_), line(line_) {
    length = static_cast<int>(current_ - start_);
  }
  void copy(char *buff) {
    const size_t len =
        std::min(static_cast<size_t>(length), static_cast<size_t>(STRING_MAX));
    std::memcpy(buff, start, len);
    buff[len] = '\0';
  }
};

struct Scanner {
  const char *start;
  const char *current;
  int line;

  Scanner() = default;
  void init(const char *source) {
    start = source;
    current = source;
    line = 1;
  }
  Scanner(const char *source) { init(source); }

  ~Scanner() = default;

  char advance() {
    // const char c = current[0];
    current++;
    return current[-1];
  }
  char peek() { return *current; }
  char peekNext() {
    if (*current == '\0') return '\0';
    return current[1];
  }
  bool match(char expected) {
    if ((*current == '\0') || (*current != expected)) return false;
    current++;
    return true;
  }
  Token string() {
    while ((peek() != '"') && (*current != '\0')) {
      if (peek() == '\n') line++;
      advance();
    }
    if (*current == '\0') return Token("Unterminated string.", line);
    advance(); // past the "
    return Token(TokenType::STRING, start, current, line);
  }

  void skipWhitespace() {
    for (;;) {
      switch (peek()) {
      case ' ':
      case '\r':
      case '\t':
        advance();
        break;
      case '\n': {
        line++;
        advance();
        break;
      }
        // disallow // comments for integer divide
        // case '/': {
        //  if (peekNext() == '/') {
        //    while (peek() != '\n' && !(*current == '\0')) advance();
        //  }
        //  else {
        //    return;
        //  }
        //  break;
        //}
      case '#': {
        while (peek() != '\n' && !(*current == '\0'))
          advance();
        break;
      }
      default:
        return;
      }
    }
  }
  inline bool matchKeyword(const int st, const int len, const char *rest) {
    return (current - start == st + len && std::memcmp(start + st, rest, len) == 0);
  }

  TokenType checkKeyword(const int st, const int len, const char *rest, TokenType type) {
    if (matchKeyword(st, len, rest)) return type;
    return TokenType::IDENTIFIER;
  }

  TokenType identifierType() {
    switch (start[0]) {
    case 'a': {
      if (current - start > 1) {
        switch (start[1]) {
        case 'n':
          return checkKeyword(2, 1, "d", TokenType::AND);
        case 'b':
          return checkKeyword(2, 1, "s", TokenType::ABS);
        case 'c':
          return checkKeyword(2, 2, "os", TokenType::ACOS);
        case 's':
          return checkKeyword(2, 2, "in", TokenType::ASIN);
        case 't': {
          if (matchKeyword(2, 3, "an2"))
            return TokenType::ATAN2;
          else if (matchKeyword(2, 2, "an"))
            return TokenType::ATAN;
        } 
       }
      }
      break;
    }
    case 'c': {
      if (current - start > 1) {
        switch (start[1]) {
        case 'l':
          return checkKeyword(2, 3, "ass", TokenType::CLASS);
        case 'o':
          return checkKeyword(2, 1, "s", TokenType::COS);
        case 'e':
          return checkKeyword(2, 2, "il", TokenType::CEIL);
        }
      }
      break;
    }
    case 'e': {
      if (current - start > 1) {
        switch (start[1]) {
        case 'l':
          return checkKeyword(2, 2, "se", TokenType::ELSE);
        case 'x':
          return checkKeyword(2, 1, "p", TokenType::EXP);
        }
      }
      break;
    }
    case 'f': {
      if (current - start > 1) {
        switch (start[1]) {
        case 'a':
          return checkKeyword(2, 3, "lse", TokenType::FALSE);
        case 'o':
          return checkKeyword(2, 1, "r", TokenType::FOR);
        case 'u':
          return checkKeyword(2, 1, "n", TokenType::FUN);
        case 'l':
          return checkKeyword(2, 3, "oor", TokenType::FLOOR);
        }
      }
      break;
    }
    case 'i':
      return checkKeyword(1, 1, "f", TokenType::IF);
    case 'l': {
      if (current - start > 1) {
        switch (start[1]) {
        case 'o': {
          if (matchKeyword(2, 3, "g10"))
            return TokenType::LOG10;
          else if (matchKeyword(2, 1, "g"))
            return TokenType::LOG;
          else
            return TokenType::IDENTIFIER;
        }
        }
      }
      break;
    }
    case 'm': {
      if (current - start > 1) {
        switch (start[1]) {
        case 'i':
          return checkKeyword(2, 1, "n", TokenType::MIN);
        case 'a':
          return checkKeyword(2, 1, "x", TokenType::MAX);
        }
      }
      break;
    }
    case 'n':
      return checkKeyword(1, 2, "il", TokenType::NIL);
    case 'o':
      return checkKeyword(1, 1, "r", TokenType::OR);
    case 'p': {
      if (current - start > 1) {
        switch (start[1]) {
        case 'r':
          return checkKeyword(2, 3, "int", TokenType::PRINT);
        case 'i':
          return (current - start > 2) ? TokenType::IDENTIFIER : TokenType::PI;
        }
      }
      break;
    }
    case 'r':
      return checkKeyword(1, 5, "eturn", TokenType::RETURN);
    case 's': {
      if (current - start > 1) {
        switch (start[1]) {
        case 'u':
          return checkKeyword(2, 3, "per", TokenType::SUPER);
        case 'i': {
          if (matchKeyword(2, 2, "gn"))
            return TokenType::SIGN;
          else if (matchKeyword(2, 1, "n"))
          return TokenType::SIN;

        }
        case 'q':
          return checkKeyword(2, 2, "rt", TokenType::SQRT);
        }
      }
      break;
    }
    case 't': {
      if (current - start > 1) {
        switch (start[1]) {
        case 'a':
          return checkKeyword(2, 1, "n", TokenType::TAN);
        case 'h':
          return checkKeyword(2, 2, "is", TokenType::THIS);
        case 'r':
          return checkKeyword(2, 2, "ue", TokenType::TRUE);
        }
      }
      break;
    }
#ifndef NO_VAR_DECL
    case 'v': return checkKeyword(1, 2, "ar", TokenType::VAR);
#endif
    case 'w':
      return checkKeyword(1, 4, "hile", TokenType::WHILE);
      // default: return TokenType::STRING;
    }
    return TokenType::IDENTIFIER;
  }
  Token identifier() {
    while (std::isalpha(peek()) || std::isdigit(peek()) || peek() == '_' ||
           peek() == '[' || peek() == ']')
      advance();

    if ((peek() == '.') &&
        ((std::isalpha(peekNext()) || std::isdigit(peekNext()) || peekNext() == '_' ||
          peekNext() == '[' || peekNext() == ']'))) {
      advance(); // consume '.'
      identifier();
    }

    auto itype = identifierType();
    auto tok = Token(itype, start, current, line);
    return tok;
  }
  Token number() {
    while (std::isdigit(peek()))
      advance();
    if (peek() == '.') {
      advance(); // consume .
      while (std::isdigit(peek()))
        advance();
    }
    if ((peek() == 'e') || (peek() == 'E') || (peek() == 'd') || (peek() == 'D')) {
      advance(); // consume e/E/d/D
      if ((peek() == '+') || (peek() == '-')) {
        advance(); // consume +/-
      }
      while (std::isdigit(peek()))
        advance();

      if (peek() == '.' && std::isdigit(peekNext())) {
        return Token("Cannot have decimal powers!", line);
        advance(); // consume .
      }
    }
    return Token(TokenType::NUMBER, start, current, line);
  }
  Token scanToken() {
    skipWhitespace();
    start = current;
    if (*current == '\0') {
      return Token(TokenType::END, start, current, line);
    }
    char c = advance();
    if (std::isalpha(c) || c == '_') return identifier();
    if (std::isdigit(c)) return number();
    if ((c == '.') && std::isdigit(peek())) return number();

    switch (c) {
    case '(':
      return Token(TokenType::LEFT_PAREN, start, current, line);
    case ')':
      return Token(TokenType::RIGHT_PAREN, start, current, line);
    case '{':
      return Token(TokenType::LEFT_BRACE, start, current, line);
    case '}':
      return Token(TokenType::RIGHT_BRACE, start, current, line);
    case ';':
      return Token(TokenType::SEMICOLON, start, current, line);
    case ',':
      return Token(TokenType::COMMA, start, current, line);
    case '.':
      return Token(TokenType::DOT, start, current, line);
    case '-':
      return Token(TokenType::MINUS, start, current, line);
    case '+':
      return Token(TokenType::PLUS, start, current, line);
    case '%':
      return Token(TokenType::MOD, start, current, line);
    case '?':
      return Token(TokenType::QUESTION, start, current, line);
    case ':':
      return Token(TokenType::COLON, start, current, line);
    case '*':
      return Token(match('*') ? TokenType::STAR_STAR : TokenType::STAR, start, current,
                   line);
    case '/':
      return Token(match('/') ? TokenType::SLASH_SLASH : TokenType::SLASH, start, current,
                   line);
    case '!':
      return Token(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG, start, current,
                   line);
    case '=':
      return Token(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL, start, current,
                   line);
    case '<':
      return Token(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS, start, current,
                   line);
    case '>':
      return Token(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER, start,
                   current, line);
    case '"':
      return string();
    }
    return Token("Unexpected character!", line);
  }
};
} // namespace pips
#endif // PIPS_SCANNER_HPP_
