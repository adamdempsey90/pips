#ifndef PIPS_CALCULATOR_COMPILER_HPP_
#define PIPS_CALCULATOR_COMPILER_HPP_

//===========================================================================
// Much of this code is based on the clox language from the book
// "Crafting Interpreters" by Robert Nystrom
// https://craftinginterpreters.com/contents.html which is available at
// https://github.com/munificent/craftinginterpreters under the MIT License.
// The code was adapted for C++ and simplified in many ways.
//===========================================================================

#include <array>
#include <cmath>
#include <tuple>
#include <unordered_map>

#include "chunk.hpp"
#include "function.hpp"
#include "object.hpp"
#include "scanner.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "value.hpp"

namespace pips {
// #define DEBUG_PRINT_CODE

enum class Precedence : unsigned int {
  NONE = 0,
  ASSIGNMENT = 1,
  TERNARY = 2,
  OR = 3,
  XOR = 4,
  AND = 5,
  EQUALITY = 6,
  COMPARISON = 7,
  TERM = 8,
  FACTOR = 9,
  UNARY = 10,
  POWER = 11,
  CALL = 12,
  PRIMARY = 13
};

struct Parser {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;

  Scanner *scanner;

  Parser(Scanner *scanner_) : scanner(scanner_) {
    hadError = false;
    panicMode = false;
  }
  Parser() = default;
  ~Parser() = default;
  void init(Scanner *scanner_) {
    hadError = false;
    panicMode = false;
    scanner = scanner_;
  }
  void errorAt(Token &token, const char *msg) {
    if (panicMode)
      return;
    panicMode = true;
    std::fprintf(stderr, "[line %d] Error", token.line);
    if (token.type == TokenType::END) {
      std::fprintf(stderr, " at end");
    } else if (token.type == TokenType::ERROR) {

    } else {
      std::fprintf(stderr, " at '%.*s'", token.length, token.start);
    }
    std::fprintf(stderr, ": %s\n", msg);
    hadError = true;
  }
  void error(const char *msg) { errorAt(previous, msg); }
  void errorAtCurrent(const char *msg) { errorAt(current, msg); }

  void advance() {
    previous = current;
    for (;;) {
      current = scanner->scanToken();
      if (current.type != TokenType::ERROR)
        break;
      errorAtCurrent(current.start);
    }
  }
  void consume(TokenType type, const char *msg) {
    if (current.type == type) {
      advance();
      return;
    }
    errorAtCurrent(msg);
  }
};

struct VM;
inline StringObject *vmNewString(VM *vm, std::string s);

struct Local {
  Token name;
  int depth;
};

enum class FunctionType { SCRIPT, FUNCTION };

// Per-function compilation context
struct CompilerState {
  Function *function = nullptr;
  FunctionType type = FunctionType::SCRIPT;
  Local locals[UINT8_MAX + 1];
  int localCount = 0;
  int scopeDepth = 0;
  CompilerState *enclosing = nullptr;
};

struct Compiler {

  // scanner maybe needs to be a unique_ptr?
  Scanner scanner;
  Parser parser;
  VM *pvm;
  CompilerState *current = nullptr;
  std::unordered_map<std::string, Function> *fnTable = nullptr;
  std::unordered_map<std::string, ClassDef> *classTable = nullptr;
  ClassDef *currentClass = nullptr;
  bool inClassInit = false;
  char end_line = ';';

  // clang-format off
  std::array<Precedence, 14> prec_array{
      Precedence::NONE,  Precedence::ASSIGNMENT, Precedence::TERNARY, 
      Precedence::OR, Precedence::XOR, Precedence::AND,   
      Precedence::EQUALITY,   Precedence::COMPARISON,
      Precedence::TERM,  Precedence::FACTOR,     Precedence::POWER,
      Precedence::UNARY, Precedence::CALL,       Precedence::PRIMARY};
  std::array<void (Compiler::*)(bool), 100> prefix_rules{&Compiler::grouping, // LEFT_PAREN
                                                        nullptr,          // RIGHT_PAREN
                                                        nullptr,          // LEFT_BRACE
                                                        nullptr,          // RIGHT_BRACE
                                                        nullptr,          // COMMA
                                                        nullptr,          // DOT
                                                        &Compiler::unary, // MINUS
                                                        &Compiler::unary, // PLUS
                                                        nullptr,          // SEMICOLON
                                                        nullptr,          // MOD
                                                        nullptr,          // SLASH
                                                        nullptr,          // SLASH_SLASH
                                                        nullptr,          // STAR
                                                        nullptr,          // STAR_STAR
                                                        &Compiler::unary, // BANG
                                                        nullptr,          // BANG_EQUAL
                                                        nullptr,          // EQUAL
                                                        nullptr,          // EQUAL_EQUAL
                                                        nullptr,          // GREATER
                                                        nullptr,          // GREATER_EQUAL
                                                        nullptr,          // LESS
                                                        nullptr,          // LESS_EQUAL
                                                        nullptr,          // QUESTION
                                                        nullptr,          // COLON
                                                        &Compiler::variable, // IDENTIFIER
                                                        &Compiler::string,   // STRING
                                                        &Compiler::number,   // NUMBER
                                                        &Compiler::getPI,    // PI
                                                        nullptr,             // AND
                                                        nullptr,             // CLASS
                                                        nullptr,             // ELSE
                                                        &Compiler::literal,  // FALSE
                                                        nullptr,             // FOR
                                                        nullptr,             // FUN
                                                        nullptr,             // IF
                                                        &Compiler::literal,  // NIL
                                                        nullptr,             // OR
                                                        nullptr,             // XOR
                                                        nullptr,             // BOR
                                                        nullptr,             // BAND
                                                        &Compiler::unary,    // BNOT
                                                        nullptr,             // LSHIFT
                                                        nullptr,             // RSHIFT
                                                        nullptr,             // PRINT
                                                        nullptr,             // LIST
                                                        nullptr,             // GLOBALS
                                                        nullptr,             // LOCALS
                                                        nullptr,             // STACK
                                                        nullptr,             // LIST-FUNC
                                                        nullptr,             // NEWLINE
                                                        nullptr,             // RETURN
                                                        nullptr,             // SUPER
                                                        &Compiler::thisExpr, // THIS
                                                        &Compiler::literal,  // TRUE
                                                        nullptr,             // VAR
                                                        nullptr,             // WHILE
                                                        &Compiler::exp,      // EXP
                                                        &Compiler::sin,      // SIN
                                                        &Compiler::cos,      // COS
                                                        &Compiler::tan,      // TAN
                                                        &Compiler::abs,      // ABS
                                                        &Compiler::log,      // LOG
                                                        &Compiler::log10,    // LOG10
                                                        &Compiler::sign,     // SIGN
                                                        &Compiler::sqrt,     // SQRT
                                                        &Compiler::acos,     // ACOS
                                                        &Compiler::asin,     // ASIN
                                                        &Compiler::atan,     // ATAN
                                                        &Compiler::ceil,     // CEIL
                                                        &Compiler::floor,    // FLOOR
                                                        &Compiler::env,      // ENV
                                                        &Compiler::atan2,    // ATAN2
                                                        &Compiler::min,      // MIN
                                                        &Compiler::max,           // MAX
                                                        &Compiler::rangeBuiltin,      // RANGE
                                                        &Compiler::linspaceBuiltin,   // LINSPACE
                                                        &Compiler::logspaceBuiltin,   // LOGSPACE
                                                        &Compiler::log10spaceBuiltin, // LOG10SPACE
                                                        &Compiler::zerosBuiltin,      // ZEROS
                                                        &Compiler::onesBuiltin,       // ONES
                                                        &Compiler::preInc,   // PLUS_PLUS
                                                        &Compiler::preDec,   // MINUS_MINUS
                                                        nullptr,             // PLUS_EQUAL
                                                        nullptr,             // MINUS_EQUAL
                                                        nullptr,             // STAR_EQUAL
                                                        nullptr,             // SLASH_EQUAL
                                                        nullptr,             // MOD_EQUAL
                                                        nullptr,             // BOR_EQUAL
                                                        nullptr,             // BAND_EQUAL
                                                        nullptr,             // LSHIFT_EQUAL
                                                        nullptr,             // RSHIFT_EQUAL
                                                        &Compiler::setAttr,  // SETATTR
                                                        &Compiler::getAttr,  // GETATTR
                                                        &Compiler::hasAttr, // HASATTR
                                                        &Compiler::str,      // STR
                                                        &Compiler::newExpr,  // NEW
                                                        &Compiler::vectorLiteral, // LEFT_BRACKET
                                                        nullptr,             // RIGHT_BRACKET
                                                        nullptr,             // ERROR
                                                        nullptr};            // END

  std::array<void (Compiler::*)(bool), 100> infix_rules{nullptr,           // LEFT_PAREN
                                                       nullptr,           // RIGHT_PAREN
                                                       nullptr,           // LEFT_BRACE
                                                       nullptr,           // RIGHT_BRACE
                                                       nullptr,           // COMMA
                                                       &Compiler::dot,    // DOT
                                                       &Compiler::binary, // MINUS
                                                       &Compiler::binary, // PLUS
                                                       nullptr,           // SEMICOLON
                                                       &Compiler::binary, // MOD
                                                       &Compiler::binary, // SLASH
                                                       &Compiler::binary, // SLASH_SLASH
                                                       &Compiler::binary, // STAR
                                                       &Compiler::binary, // STAR_STAR
                                                       nullptr,           // BANG
                                                       &Compiler::binary, // BANG_EQUAL
                                                       nullptr,           // EQUAL
                                                       &Compiler::binary, // EQUAL_EQUAL
                                                       &Compiler::binary, // GREATER
                                                       &Compiler::binary, // GREATER_EQUAL
                                                       &Compiler::binary, // LESS
                                                       &Compiler::binary, // LESS_EQUAL
                                                       &Compiler::ternary,// QUESTION
                                                       nullptr,           // COLON
                                                       nullptr,           // IDENTIFIER
                                                       nullptr,           // STRING
                                                       nullptr,           // NUMBER
                                                       nullptr,           // PI
                                                       &Compiler::and_,   // AND
                                                       nullptr,           // CLASS
                                                       nullptr,           // ELSE
                                                       nullptr,           // FALSE
                                                       nullptr,           // FOR
                                                       nullptr,           // FUN
                                                       nullptr,           // IF
                                                       nullptr,           // NIL
                                                       &Compiler::or_,    // OR
                                                       &Compiler::binary, // XOR
                                                       &Compiler::binary, // BOR
                                                       &Compiler::binary, // BAND
                                                       nullptr,           // BNOT
                                                       &Compiler::binary, // LSHIFT
                                                       &Compiler::binary, // RSHIFT
                                                       nullptr,           // PRINT
                                                       nullptr,           // LIST
                                                       nullptr,           // GLOBALS
                                                       nullptr,           // LOCALS
                                                       nullptr,           // STACK
                                                       nullptr,           // LISTFUNC
                                                       nullptr,           // NEWLINE
                                                       nullptr,           // RETURN
                                                       nullptr,           // SUPER
                                                       nullptr,           // THIS
                                                       nullptr,           // TRUE
                                                       nullptr,           // VAR
                                                       nullptr,           // WHILE
                                                       nullptr,           // EXP
                                                       nullptr,           // SIN
                                                       nullptr,           // COS
                                                       nullptr,           // TAN
                                                       nullptr,           // ABS
                                                       nullptr,           // LOG
                                                       nullptr,           // LOG10
                                                       nullptr,           // SIGN
                                                       nullptr,           // SQRT
                                                       nullptr,           // ACOS
                                                       nullptr,           // ASIN
                                                       nullptr,           // ATAN
                                                       nullptr,           // CEIL
                                                       nullptr,           // FLOOR
                                                       nullptr,           // ENV
                                                       nullptr,           // ATAN2
                                                       nullptr,           // MIN
                                                       nullptr,           // MAX
                                                       nullptr,           // RANGE
                                                       nullptr,           // LINSPACE
                                                       nullptr,           // LOGSPACE
                                                       nullptr,           // LOG10SPACE
                                                       nullptr,           // ZEROS
                                                       nullptr,           // ONES
                                                       nullptr,           // PLUS_PLUS
                                                       nullptr,           // MINUS_MINUS
                                                       nullptr,           // PLUS_EQUAL
                                                       nullptr,           // MINUS_EQUAL
                                                       nullptr,           // STAR_EQUAL
                                                       nullptr,           // SLASH_EQUAL
                                                       nullptr,           // MOD_EQUAL
                                                       nullptr,           // BOR_EQUAL
                                                       nullptr,           // BAND_EQUAL
                                                       nullptr,           // LSHIFT_EQUAL
                                                       nullptr,           // RSHIFT_EQUAL
                                                       nullptr,           // SETATTR
                                                       nullptr,           // GETATTR
                                                       nullptr,           // HASATTR
                                                       nullptr,           // STR
                                                       nullptr,           // NEW
                                                       &Compiler::subscript, // LEFT_BRACKET
                                                       nullptr,           // RIGHT_BRACKET
                                                       nullptr,           // ERROR
                                                       nullptr};          // END

  std::array<Precedence, 100> prec_rules{Precedence::NONE,       // LEFT_PAREN
                                        Precedence::NONE,       // RIGHT_PAREN
                                        Precedence::NONE,       // LEFT_BRACE
                                        Precedence::NONE,       // RIGHT_BRACE
                                        Precedence::NONE,       // COMMA
                                        Precedence::CALL,       // DOT
                                        Precedence::TERM,       // MINUS
                                        Precedence::TERM,       // PLUS
                                        Precedence::NONE,       // SEMICOLON
                                        Precedence::FACTOR,     // MOD
                                        Precedence::FACTOR,     // SLASH
                                        Precedence::FACTOR,     // SLASH_SLASH
                                        Precedence::FACTOR,     // STAR
                                        Precedence::POWER,      // STAR_STAR
                                        Precedence::NONE,       // BANG
                                        Precedence::EQUALITY,   // BANG_EQUAL
                                        Precedence::NONE,       // EQUAL
                                        Precedence::EQUALITY,   // EQUAL_EQUAL
                                        Precedence::COMPARISON, // GREATER
                                        Precedence::COMPARISON, // GREATER_EQUAL
                                        Precedence::COMPARISON, // LESS
                                        Precedence::COMPARISON, // LESS_EQUAL
                                        Precedence::TERNARY,    // QUESTION
                                        Precedence::NONE,       // COLON
                                        Precedence::NONE,       // IDENTIFIER
                                        Precedence::NONE,       // STRING
                                        Precedence::NONE,       // NUMBER
                                        Precedence::NONE,       // PI
                                        Precedence::AND,        // AND
                                        Precedence::NONE,       // CLASS
                                        Precedence::NONE,       // ELSE
                                        Precedence::NONE,       // FALSE
                                        Precedence::NONE,       // FOR
                                        Precedence::NONE,       // FUN
                                        Precedence::NONE,       // IF
                                        Precedence::NONE,       // NIL
                                        Precedence::OR,         // OR
                                        Precedence::XOR,        // XOR
                                        Precedence::OR,         // BOR
                                        Precedence::AND,        // BAND
                                        Precedence::NONE,       // BNOT
                                        Precedence::TERM,       // LSHIFT
                                        Precedence::TERM,       // RSHIFT
                                        Precedence::NONE,       // PRINT
                                        Precedence::NONE,       // LIST
                                        Precedence::NONE,       // GLOBALS
                                        Precedence::NONE,       // LOCALS
                                        Precedence::NONE,       // STACK
                                        Precedence::NONE,       // LISTFUNC
                                        Precedence::NONE,       // NEWLINE  
                                        Precedence::NONE,       // RETURN
                                        Precedence::NONE,       // SUPER
                                        Precedence::NONE,       // THIS
                                        Precedence::NONE,       // TRUE
                                        Precedence::NONE,       // VAR
                                        Precedence::NONE,       // WHILE
                                        Precedence::NONE,       // EXP
                                        Precedence::NONE,       // SIN
                                        Precedence::NONE,       // COS
                                        Precedence::NONE,       // TAN
                                        Precedence::NONE,       // ABS
                                        Precedence::NONE,       // LOG
                                        Precedence::NONE,       // LOG10
                                        Precedence::NONE,       // SIGN
                                        Precedence::NONE,       // SQRT
                                        Precedence::NONE,       // ACOS
                                        Precedence::NONE,       // ASIN
                                        Precedence::NONE,       // ATAN
                                        Precedence::NONE,       // CEIL
                                        Precedence::NONE,       // FLOOR
                                        Precedence::NONE,       // ENV
                                        Precedence::NONE,       // ATAN2
                                        Precedence::NONE,       // MIN
                                        Precedence::NONE,       // MAX                                       
                                        Precedence::NONE,       // RANGE
                                        Precedence::NONE,       // LINSPACE
                                        Precedence::NONE,       // LOGSPACE
                                        Precedence::NONE,       // LOG10SPACE
                                        Precedence::NONE,       // ZEROS
                                        Precedence::NONE,       // ONES
                                        Precedence::NONE,       // PLUS_PLUS
                                        Precedence::NONE,       // MINUS_MINUS
                                        Precedence::NONE,       // PLUS_EQUAL
                                        Precedence::NONE,       // MINUS_EQUAL
                                        Precedence::NONE,       // STAR_EQUAL
                                        Precedence::NONE,       // SLASH_EQUAL
                                        Precedence::NONE,       // MOD_EQUAL
                                        Precedence::NONE,       // BOR_EQUAL
                                        Precedence::NONE,       // BAND_EQUAL
                                        Precedence::NONE,       // LSHIFT_EQUAL
                                        Precedence::NONE,       // RSHIFT_EQUAL
                                        Precedence::NONE,       // SETATTR
                                        Precedence::NONE,       // GETATTR
                                        Precedence::NONE,       // HASATTR
                                        Precedence::NONE,       // STR
                                        Precedence::NONE,       // NEW
                                        Precedence::CALL,       // LEFT_BRACKET
                                        Precedence::NONE,       // RIGHT_BRACKET
                                        Precedence::NONE,       // ERROR
                                        Precedence::NONE};      // END
  // clang-format on
  Compiler() = default;
  Compiler(VM *vm_, char end_line = '\n')
      : pvm(vm_), parser(nullptr), end_line(end_line) {};

  Compiler(VM *vm_, const char *source, char end_line = '\n')
      : scanner(source), parser(&scanner), pvm(vm_), end_line(end_line) {}
  ~Compiler() = default;

  void init(const char *source) {
    scanner.init(source);
    parser.init(&scanner);
  }
  void set_current(CompilerState *curr) { current = curr; }
  Chunk *currentChunk() { return &current->function->chunk; }

  void emitByte(std::uint8_t byte) {
    currentChunk()->write(byte, parser.previous.line);
  }
  void emitBytes(std::uint8_t byte1, std::uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
  }
  std::uint8_t makeConstant(Value val) {
    auto constant = currentChunk()->addConstant(val);
    if (constant > Utils::Big<std::uint8_t>()) {
      parser.error("Too many constants in one chunk.");
      return 0;
    }
    return static_cast<std::uint8_t>(constant);
  }
  void emitReturn() {
    // Default return value is nil.
    emitByte(OpCode::NIL);
    emitByte(OpCode::RETURN);
  }
  void emitConstant(Value val) {
    emitBytes(OpCode::CONSTANT, makeConstant(val));
  }

  void parsePrecedence(Precedence precedence) {
    parser.advance();
    const auto &[prefix, infix, prec_] = getRule(parser.previous.type);
    if (prefix == nullptr) {
      parser.error("Expect expression");
      return;
    }
    bool canAssign = Utils::to_underlying(precedence) <=
                     Utils::to_underlying(Precedence::ASSIGNMENT);
    (this->*prefix)(canAssign);

    const auto &[_f1, _f2, prec2_] = getRule(parser.current.type);
    Precedence prec = prec2_;
    while (Utils::to_underlying(precedence) <= Utils::to_underlying(prec)) {
      // Inside "new C {...}" initializers, a DOT that begins on a new line
      // belongs to the next ".field = expr" initializer, not to the current
      // expression. Stop here so the init loop can pick it up.
      if (inClassInit && parser.current.type == TokenType::DOT &&
          parser.current.line > parser.previous.line) {
        break;
      }
      parser.advance();
      const auto &[_f1, infix, _prec] = getRule(parser.previous.type);
      (this->*infix)(canAssign);
      const auto &[_f3, _f4, next_prec] = getRule(parser.current.type);
      prec = next_prec;
    }
    if (canAssign && match(TokenType::EQUAL)) {
      parser.error("Invalid assignmnt target.");
    }
  }
  std::tuple<void (Compiler::*)(bool), void (Compiler::*)(bool), Precedence>
  getRule(TokenType type) {
    const auto idx = Utils::to_underlying(type);
    return {prefix_rules[idx], infix_rules[idx], prec_rules[idx]};
  }
  void expression() { parsePrecedence(Precedence::ASSIGNMENT); }
  void addLocal(Token name) {
    if (current->localCount == UINT8_MAX + 1) {
      parser.error("Too many local variables in function");
      return;
    }
    Local *local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = current->scopeDepth;
  }
  bool identifiersEqual(Token *a, Token *b) {
    if (a->length != b->length)
      return false;
    return std::memcmp(a->start, b->start, a->length) == 0;
  }
  void declareVariable() {
    if (current->scopeDepth == 0)
      return;
    Token *name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--) {
      Local *local = &current->locals[i];
      if (local->depth != -1 && local->depth < current->scopeDepth) {
        break;
      }
      if (identifiersEqual(name, &local->name)) {
        parser.error("Already a variable with this name in this scope.");
        return;
      }
    }
    addLocal(*name);
  }
  void varDeclarationNoVar() {
    // TODO:
    // This does not handle scope correctly
    // Related to the different handling of a variable declaration and a
    // statement if (some condition) {
    //   return statement();
    //}
    declareVariable();
    std::uint8_t global =
        (current->scopeDepth > 0) ? 0 : identifierConstant(&parser.previous);
    if (match(TokenType::EQUAL)) {
      expression();
    } else {
      emitByte(OpCode::NIL);
    }
    if (end_line == ';' && parser.previous.type != TokenType::RIGHT_BRACE)
      parser.consume(TokenType::SEMICOLON,
                     "Expect ';' after variable declaration.");
    else
      (void)match(TokenType::SEMICOLON);
    defineVariable(global);
  }
  void varDeclaration() {
    auto global = parseVariable("Expect variable name.");
    if (match(TokenType::EQUAL)) {
      expression();
    } else {
      emitByte(OpCode::NIL);
    }
    if (end_line == ';' && parser.previous.type != TokenType::RIGHT_BRACE)
      parser.consume(TokenType::SEMICOLON,
                     "Expect ';' after variable declaration.");
    else
      (void)match(TokenType::SEMICOLON);
    defineVariable(global);
  }

  std::uint8_t identifierConstant(Token *name) {
    std::string s(name->start, name->length);
    return makeConstant(STRING_VAL(vmNewString(pvm, std::move(s))));
  }
  std::uint8_t parseVariable(const char *msg) {
    parser.consume(TokenType::IDENTIFIER, msg);
    declareVariable();
    if (current->scopeDepth > 0)
      return 0;
    return identifierConstant(&parser.previous);
  }
  void markInitialized() {
    current->locals[current->localCount - 1].depth = current->scopeDepth;
  }
  void defineVariable(std::uint8_t global) {
    if (current->scopeDepth > 0) {
      markInitialized();
      return;
    }
    emitBytes(OpCode::DEFINE_GLOBAL, global);
  }
  int resolveLocal(CompilerState *comp, Token *name) {
    for (int i = comp->localCount - 1; i >= 0; i--) {
      Local *local = &comp->locals[i];
      if (identifiersEqual(name, &local->name)) {
        if (local->depth == -1) {
          parser.error("Can't read local variable in its own initializer.");
        }
        return i;
      }
    }
    return -1;
  }
  // Walk enclosing function scopes looking for `name` as a local
  int resolveOuter(CompilerState *comp, Token *name,
                   std::uint8_t &outNameConst) {
    for (CompilerState *s = comp->enclosing; s != nullptr; s = s->enclosing) {
      if (s->type != FunctionType::FUNCTION)
        continue;
      int slot = resolveLocal(s, name);
      if (slot != -1) {
        outNameConst = makeConstant(
            STRING_VAL(vmNewString(pvm, s->function->name)));
        return slot;
      }
    }
    return -1;
  }
  void namedVariable(Token name, bool canAssign) {
    int localArg = resolveLocal(current, &name);
    int outerSlot = -1;
    std::uint8_t outerNameConst = 0;
    std::uint8_t globalArg = 0;
    enum Kind { LOCAL, OUTER, GLOBAL } kind;
    if (localArg != -1) {
      kind = LOCAL;
    } else if ((outerSlot = resolveOuter(current, &name, outerNameConst)) !=
               -1) {
      kind = OUTER;
    } else {
      kind = GLOBAL;
      globalArg = identifierConstant(&name);
    }
    auto emitGet = [&]() {
      if (kind == LOCAL) {
        emitBytes(OpCode::GET_LOCAL, (std::uint8_t)localArg);
      } else if (kind == OUTER) {
        emitByte(OpCode::GET_OUTER);
        emitByte(outerNameConst);
        emitByte((std::uint8_t)outerSlot);
      } else {
        emitBytes(OpCode::GET_GLOBAL, globalArg);
      }
    };
    auto emitSet = [&]() {
      if (kind == LOCAL) {
        emitBytes(OpCode::SET_LOCAL, (std::uint8_t)localArg);
      } else if (kind == OUTER) {
        emitByte(OpCode::SET_OUTER);
        emitByte(outerNameConst);
        emitByte((std::uint8_t)outerSlot);
      } else {
        emitBytes(OpCode::SET_GLOBAL, globalArg);
      }
    };
    if (canAssign && match(TokenType::EQUAL)) {
      expression();
      emitSet();
      return;
    }
    if (canAssign) {
      // Compound assignment
      auto compound = [&](OpCode op) {
        emitGet();
        expression();
        emitByte(op);
        emitSet();
      };
      // Postfix increment/decrement
      auto incDec = [&](OpCode op) {
        emitGet();
        emitConstant(NUMBER_VAL(1.0));
        emitByte(op);
        emitSet();
      };
      if (match(TokenType::PLUS_EQUAL))   { compound(OpCode::ADD);      return; }
      if (match(TokenType::MINUS_EQUAL))  { compound(OpCode::SUBTRACT); return; }
      if (match(TokenType::STAR_EQUAL))   { compound(OpCode::MULTIPLY); return; }
      if (match(TokenType::SLASH_EQUAL))  { compound(OpCode::DIVIDE);   return; }
      if (match(TokenType::MOD_EQUAL))    { compound(OpCode::MOD);      return; }
      if (match(TokenType::BOR_EQUAL))    { compound(OpCode::BOR);      return; }
      if (match(TokenType::BAND_EQUAL))   { compound(OpCode::BAND);     return; }
      if (match(TokenType::LSHIFT_EQUAL)) { compound(OpCode::LSHIFT);   return; }
      if (match(TokenType::RSHIFT_EQUAL)) { compound(OpCode::RSHIFT);   return; }
      if (match(TokenType::PLUS_PLUS))    { incDec(OpCode::ADD);        return; }
      if (match(TokenType::MINUS_MINUS))  { incDec(OpCode::SUBTRACT);   return; }
    }
    emitGet();
  }
  // Prefix ++x / --x
  void preIncDec(OpCode op) {
    parser.consume(TokenType::IDENTIFIER,
                   "Expect variable name after '++' or '--'.");
    Token name = parser.previous;
    int localArg = resolveLocal(current, &name);
    int outerSlot = -1;
    std::uint8_t outerNameConst = 0;
    if (localArg != -1) {
      emitBytes(OpCode::GET_LOCAL, (std::uint8_t)localArg);
      emitConstant(NUMBER_VAL(1.0));
      emitByte(op);
      emitBytes(OpCode::SET_LOCAL, (std::uint8_t)localArg);
    } else if ((outerSlot = resolveOuter(current, &name, outerNameConst)) !=
               -1) {
      emitByte(OpCode::GET_OUTER);
      emitByte(outerNameConst);
      emitByte((std::uint8_t)outerSlot);
      emitConstant(NUMBER_VAL(1.0));
      emitByte(op);
      emitByte(OpCode::SET_OUTER);
      emitByte(outerNameConst);
      emitByte((std::uint8_t)outerSlot);
    } else {
      std::uint8_t arg = identifierConstant(&name);
      emitBytes(OpCode::GET_GLOBAL, arg);
      emitConstant(NUMBER_VAL(1.0));
      emitByte(op);
      emitBytes(OpCode::SET_GLOBAL, arg);
    }
  }
  void preInc(bool /*canAssign*/) { preIncDec(OpCode::ADD); }
  void preDec(bool /*canAssign*/) { preIncDec(OpCode::SUBTRACT); }
  void and_(bool tmp_) {
    int endJump = emitJump(OpCode::JUMP_IF_FALSE);
    emitByte(OpCode::POP);
    parsePrecedence(Precedence::AND);
    patchJump(endJump);
  }
  void or_(bool tmp) {
    int elseJump = emitJump(OpCode::JUMP_IF_FALSE);
    int endJump = emitJump(OpCode::JUMP);

    patchJump(elseJump);
    emitByte(OpCode::POP);

    parsePrecedence(Precedence::OR);
    patchJump(endJump);
  }
  void variable(bool canAssign) {
    Token name = parser.previous;
    if (currentClass && resolveLocal(current, &name) == -1) {
      std::string nstr(name.start, name.length);
      // Bare method call: name(...)
      if (currentClass->methods.count(nstr) &&
          check(TokenType::LEFT_PAREN)) {
        parser.advance(); // consume '('
        emitBytes(OpCode::GET_LOCAL, 0);
        std::uint8_t mname = identifierConstant(&name);
        std::uint8_t argc = argumentList();
        emitByte(OpCode::CALL_METHOD);
        emitByte(mname);
        emitByte(argc);
        return;
      }
      // Bare field reference: name, name = value
      bool isField = false;
      for (const auto &f : currentClass->fields) {
        if (f == nstr) { isField = true; break; }
      }
      if (isField) {
        emitBytes(OpCode::GET_LOCAL, 0);
        std::uint8_t nc = identifierConstant(&name);
        if (canAssign && match(TokenType::EQUAL)) {
          expression();
          emitByte(OpCode::SET_PROPERTY);
          emitByte(nc);
          return;
        }
        emitByte(OpCode::GET_PROPERTY);
        emitByte(nc);
        return;
      }
    }
    // look for name(...) as the sign for a function call
    if (check(TokenType::LEFT_PAREN) && resolveLocal(current, &name) == -1) {
      parser.advance(); // consume '('
      std::uint8_t nameConst = identifierConstant(&name);
      std::uint8_t argc = argumentList();
      emitByte(OpCode::CALL);
      emitByte(nameConst);
      emitByte(argc);
      return;
    }
    namedVariable(name, canAssign);
  }

  void thisExpr(bool /*canAssign*/) {
    if (!currentClass) {
      parser.error("'this' is only valid inside a method.");
      return;
    }
    emitBytes(OpCode::GET_LOCAL, 0);
  }

  void dot(bool canAssign) {
    parser.consume(TokenType::IDENTIFIER, "Expect property name after '.'.");
    Token nameTok = parser.previous;
    std::uint8_t nameConst = identifierConstant(&nameTok);
    if (check(TokenType::LEFT_PAREN)) {
      parser.advance(); // '('
      std::uint8_t argc = argumentList();
      emitByte(OpCode::CALL_METHOD);
      emitByte(nameConst);
      emitByte(argc);
      return;
    }
    if (canAssign && match(TokenType::EQUAL)) {
      expression();
      emitByte(OpCode::SET_PROPERTY);
      emitByte(nameConst);
      return;
    }
    emitByte(OpCode::GET_PROPERTY);
    emitByte(nameConst);
  }

  void newExpr(bool /*canAssign*/) {
    parser.consume(TokenType::IDENTIFIER, "Expect class name after 'new'.");
    Token classTok = parser.previous;
    std::string className(classTok.start, classTok.length);
    std::uint8_t classConst = identifierConstant(&classTok);

    // Look up class for field-name validation.
    ClassDef *cls = nullptr;
    if (classTable) {
      auto it = classTable->find(className);
      if (it == classTable->end()) {
        parser.error("Unknown class in 'new'.");
      } else {
        cls = &it->second;
      }
    }

    parser.consume(TokenType::LEFT_BRACE,
                   "Expect '{' after class name in 'new'.");

    bool savedInit = inClassInit;
    inClassInit = true;

    // Allocate
    emitByte(OpCode::NEW_INSTANCE);
    emitByte(classConst);

    // Default field initializers
    emitByte(OpCode::DUP);
    std::uint8_t initConst = makeConstant(
        STRING_VAL(vmNewString(pvm, std::string("__init_fields"))));
    emitByte(OpCode::CALL_METHOD);
    emitByte(initConst);
    emitByte(0);
    emitByte(OpCode::POP);

    // Brace-initializer block: { .field = expr; ... }
    while (!check(TokenType::RIGHT_BRACE) && !check(TokenType::END) &&
           !parser.hadError) {
      parser.consume(TokenType::DOT,
                     "Expect '.field = expr' initializer.");
      parser.consume(TokenType::IDENTIFIER, "Expect field name after '.'.");
      Token fieldTok = parser.previous;
      std::string fieldName(fieldTok.start, fieldTok.length);

      if (cls) {
        bool found = false;
        for (const auto &f : cls->fields) {
          if (f == fieldName) { found = true; break; }
        }
        if (!found) parser.error("Unknown field in class initializer.");
      }

      parser.consume(TokenType::EQUAL, "Expect '=' after field name.");

      std::uint8_t fieldConst = identifierConstant(&fieldTok);
      emitByte(OpCode::DUP);          // keep instance on stack
      expression();
      emitByte(OpCode::SET_PROPERTY);
      emitByte(fieldConst);
      emitByte(OpCode::POP);          // discard value pushed by SET_PROPERTY

      if (check(TokenType::SEMICOLON) || check(TokenType::COMMA))
        parser.advance();
    }
    parser.consume(TokenType::RIGHT_BRACE,
                   "Expect '}' after class initializers.");
    inClassInit = savedInit;
  }

  std::uint8_t argumentList() {
    std::uint8_t argc = 0;
    if (!check(TokenType::RIGHT_PAREN)) {
      do {
        expression();
        if (argc == 255) {
          parser.error("Can't have more than 255 arguments.");
        }
        argc++;
      } while (match(TokenType::COMMA));
    }
    parser.consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments.");
    return argc;
  }
  void number(bool tmp_) {
    Real value = static_cast<Real>(std::strtod(parser.previous.start, NULL));
    emitConstant(NUMBER_VAL(value));
  }
  void getPI(bool tmp_) { emitConstant(NUMBER_VAL(std::acos(-1.0L))); }
  void exp(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::EXP);
  }
  void sin(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::SIN);
  }
  void cos(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::COS);
  }
  void tan(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::TAN);
  }
  void abs(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::ABS);
  }
  void log(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::LOG);
  }
  void log10(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::LOG10);
  }
  void sign(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::SIGN);
  }
  void sqrt(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::SQRT);
  }
  void acos(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::ACOS);
  }
  void asin(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::ASIN);
  }
  void atan(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::ATAN);
  }
  void binary_consume() {
    parser.consume(TokenType::LEFT_PAREN, "Expect '(' after 'atan'.");
    expression();
    parser.consume(TokenType::COMMA, "Expect ',' between arguments to atan.");
    expression();
    parser.consume(TokenType::RIGHT_PAREN,
                   "Expect ')' after arguments to atan.");
  }
  void atan2(bool tmp_) {
    binary_consume();
    emitByte(OpCode::ATAN2);
  }
  void min(bool tmp_) {
    binary_consume();
    emitByte(OpCode::MIN);
  }
  void max(bool tmp_) {
    binary_consume();
    emitByte(OpCode::MAX);
  }
  void rangeBuiltin(bool /*tmp_*/) {
    parser.consume(TokenType::LEFT_PAREN, "Expect '(' after 'range'.");
    std::uint8_t count = 0;
    if (!check(TokenType::RIGHT_PAREN)) {
      do {
        if (count == 3) {
          parser.error("range() takes at most 3 arguments.");
        }
        expression();
        count++;
      } while (match(TokenType::COMMA));
    }
    parser.consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments to range.");
    if (count == 0) {
      parser.error("range() requires at least 1 argument.");
    }
    emitByte(OpCode::RANGE);
    emitByte(count);
  }
  void linspaceBuiltin(bool /*tmp_*/) {
    parser.consume(TokenType::LEFT_PAREN, "Expect '(' after 'linspace'.");
    expression();
    parser.consume(TokenType::COMMA, "Expect ',' in linspace(start, stop, count).");
    expression();
    parser.consume(TokenType::COMMA, "Expect ',' in linspace(start, stop, count).");
    expression();
    parser.consume(TokenType::RIGHT_PAREN, "Expect ')' after linspace arguments.");
    emitByte(OpCode::LINSPACE);
  }
  void logspaceBuiltin(bool /*tmp_*/) {
    parser.consume(TokenType::LEFT_PAREN, "Expect '(' after 'logspace'.");
    expression();
    parser.consume(TokenType::COMMA, "Expect ',' in logspace(start, stop, count).");
    expression();
    parser.consume(TokenType::COMMA, "Expect ',' in logspace(start, stop, count).");
    expression();
    parser.consume(TokenType::RIGHT_PAREN, "Expect ')' after logspace arguments.");
    emitByte(OpCode::LOGSPACE);
  }
  void log10spaceBuiltin(bool /*tmp_*/) {
    parser.consume(TokenType::LEFT_PAREN, "Expect '(' after 'log10space'.");
    expression();
    parser.consume(TokenType::COMMA, "Expect ',' in log10space(start, stop, count).");
    expression();
    parser.consume(TokenType::COMMA, "Expect ',' in log10space(start, stop, count).");
    expression();
    parser.consume(TokenType::RIGHT_PAREN, "Expect ')' after log10space arguments.");
    emitByte(OpCode::LOG10SPACE);
  }
  void zerosBuiltin(bool /*tmp_*/) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::ZEROS);
  }
  void onesBuiltin(bool /*tmp_*/) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::ONES);
  }
  void setAttr(bool tmp_) {
    parser.consume(TokenType::LEFT_PAREN, "Expect '(' after 'setattr'.");
    expression();
    parser.consume(TokenType::COMMA, "Expect ',' between arguments to setattr.");
    expression();
    parser.consume(TokenType::COMMA, "Expect ',' between arguments to setattr.");
    expression();
    parser.consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments to setattr.");
    emitByte(OpCode::SET_ATTR);
  }
  void getAttr(bool tmp_) {
    binary_consume();
    emitByte(OpCode::GET_ATTR);
  }
  void hasAttr(bool tmp_) {
    binary_consume();
    emitByte(OpCode::HAS_ATTR);
  }
  void str(bool tmp_) {
    parser.consume(TokenType::LEFT_PAREN, "Expect '(' after 'str'.");
    expression();
    parser.consume(TokenType::RIGHT_PAREN, "Expect ')' after argument to str.");
    emitByte(OpCode::STR);
  }
  void ceil(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::CEIL);
  }
  void floor(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::FLOOR);
  }
  void env(bool tmp_) {
    parsePrecedence(Precedence::UNARY);
    emitByte(OpCode::ENV);
  }
  void grouping(bool tmp_) {
    expression();
    parser.consume(TokenType::RIGHT_PAREN, "Expect ')' after expression");
  }
  void vectorLiteral(bool /*canAssign*/) {
    // '[' already consumed by parsePrecedence.
    std::uint8_t count = 0;
    if (!check(TokenType::RIGHT_BRACKET)) {
      do {
        if (count == 255) {
          parser.error("Can't have more than 255 elements in a vector literal.");
        }
        expression();
        count++;
      } while (match(TokenType::COMMA));
    }
    parser.consume(TokenType::RIGHT_BRACKET,
                   "Expect ']' after vector elements.");
    emitByte(OpCode::BUILD_VECTOR);
    emitByte(count);
  }
  void subscript(bool canAssign) {
    // '[' already consumed. Parse either `i]` or `[a]:b]` style slice.
    bool isSlice = false;
    if (check(TokenType::COLON)) {
      // [: ...]  -> omitted start
      emitByte(OpCode::NIL);
      isSlice = true;
      parser.advance(); // consume ':'
      // Parse end (or omitted)
      if (check(TokenType::RIGHT_BRACKET)) {
        emitByte(OpCode::NIL);
      } else {
        expression();
      }
    } else {
      expression(); // first expression (index or slice start)
      if (match(TokenType::COLON)) {
        isSlice = true;
        if (check(TokenType::RIGHT_BRACKET)) {
          emitByte(OpCode::NIL);
        } else {
          expression();
        }
      }
    }
    parser.consume(TokenType::RIGHT_BRACKET,
                   "Expect ']' after subscript.");
    if (canAssign && match(TokenType::EQUAL)) {
      expression();
      emitByte(isSlice ? OpCode::SET_SLICE : OpCode::SET_INDEX);
      return;
    }
    emitByte(isSlice ? OpCode::GET_SLICE : OpCode::GET_INDEX);
  }
  void unary(bool tmp_) {
    auto op_type = parser.previous.type;
    parsePrecedence(Precedence::UNARY);
    switch (op_type) {
    case TokenType::MINUS:
      emitByte(OpCode::NEGATE);
      break;
    case TokenType::PLUS:
      emitByte(OpCode::UPLUS);
      break;
    case TokenType::BNOT:
      emitByte(OpCode::BNOT);
      break;
    case TokenType::BANG:
      emitByte(OpCode::NOT);
      break;
    default:
      break;
    }
    return;
  }
  void binary(bool tmp_) {
    TokenType op_type = parser.previous.type;
    const auto &[_f1, _f2, precedence] = getRule(op_type);

    Precedence next_prec = prec_array[Utils::to_underlying(precedence) + 1];
    parsePrecedence(next_prec);
    switch (op_type) {
    case TokenType::PLUS:
      emitByte(OpCode::ADD);
      break;
    case TokenType::BANG_EQUAL:
      emitBytes(OpCode::EQUAL, OpCode::NOT);
      break;
    case TokenType::EQUAL_EQUAL:
      emitByte(OpCode::EQUAL);
      break;
    case TokenType::GREATER:
      emitByte(OpCode::GREATER);
      break;
    case TokenType::GREATER_EQUAL:
      emitBytes(OpCode::LESS, OpCode::NOT);
      break;
    case TokenType::LESS:
      emitByte(OpCode::LESS);
      break;
    case TokenType::LESS_EQUAL:
      emitBytes(OpCode::GREATER, OpCode::NOT);
      break;
    case TokenType::MINUS:
      emitByte(OpCode::SUBTRACT);
      break;
    case TokenType::MOD:
      emitByte(OpCode::MOD);
      break;
    case TokenType::STAR:
      emitByte(OpCode::MULTIPLY);
      break;
    case TokenType::STAR_STAR:
      emitByte(OpCode::POW);
      break;
    case TokenType::SLASH:
      emitByte(OpCode::DIVIDE);
      break;
    case TokenType::SLASH_SLASH:
      emitByte(OpCode::INTDIVIDE);
      break;
    case TokenType::XOR:
      emitByte(OpCode::XOR);
      break;
    case TokenType::BOR:
      emitByte(OpCode::BOR);
      break;
    case TokenType::BAND:
      emitByte(OpCode::BAND);
      break;
    case TokenType::LSHIFT:
      emitByte(OpCode::LSHIFT);
      break;
    case TokenType::RSHIFT:
      emitByte(OpCode::RSHIFT);
      break;
    default:
      return;
    }
  }
  void ternary(bool tmp_) {
    int thenJump = emitJump(OpCode::JUMP_IF_FALSE);
    emitByte(OpCode::POP);

    parsePrecedence(Precedence::TERNARY);

    int elseJump = emitJump(OpCode::JUMP);
    patchJump(thenJump);
    emitByte(OpCode::POP);

    parser.consume(TokenType::COLON,
                   "Expect ':' after true expression in ternary operator.");

    parsePrecedence(Precedence::TERNARY);

    patchJump(elseJump);
  }
  void string(bool tmp_) {
    // The +1 and -2 remove the leading and trailing "
    // If we supported strings without the need of " " we would remove that and
    // possibly make this the default case of the keyword switch?
    std::string s(parser.previous.start + 1, parser.previous.length - 2);
    emitConstant(STRING_VAL(vmNewString(pvm, std::move(s))));
  }

  void literal(bool tmp_) {
    switch (parser.previous.type) {
    case TokenType::FALSE:
      emitByte(OpCode::FALSE);
      break;
    case TokenType::NIL:
      emitByte(OpCode::NIL);
      break;
    case TokenType::TRUE:
      emitByte(OpCode::TRUE);
      break;
    default:
      return;
    }
  }
  void synchronize() {
    parser.panicMode = false;
    while (parser.current.type != TokenType::END) {
      if (parser.previous.type == TokenType::SEMICOLON)
        return;
      switch (parser.current.type) {
      case TokenType::CLASS:
      case TokenType::FUN:
      case TokenType::VAR:
      case TokenType::FOR:
      case TokenType::IF:
      case TokenType::WHILE:
      case TokenType::PRINT:
      case TokenType::LIST:
      case TokenType::GLOBALS:
      case TokenType::LOCALS:
      case TokenType::STACK:
      case TokenType::LISTFUNC:
      case TokenType::RETURN:
        return;
      default:; // Do nothing
      }
      parser.advance();
    }
  }
  void listStatement() {
    // dump the current list of variables
    emitByte(OpCode::LIST);
    if (end_line == ';')
      parser.consume(TokenType::SEMICOLON, "Expect ';' after statement.");
  }
  void globalsStatement() {
    emitByte(OpCode::LIST_GLOBALS);
    if (end_line == ';')
      parser.consume(TokenType::SEMICOLON, "Expect ';' after statement.");
  }
  void localsStatement() {
    emitByte(OpCode::LIST_LOCALS);
    if (end_line == ';')
      parser.consume(TokenType::SEMICOLON, "Expect ';' after statement.");
  }
  void stackStatement() {
    emitByte(OpCode::LIST_STACK);
    if (end_line == ';')
      parser.consume(TokenType::SEMICOLON, "Expect ';' after statement.");
  }
  void listfuncStatement() {
    emitByte(OpCode::LIST_FUNC);
    if (end_line == ';')
      parser.consume(TokenType::SEMICOLON, "Expect ';' after statement.");
  }
  void printStatement() {
    parser.consume(TokenType::LEFT_PAREN, "Expect '(' after 'print'.");
    // keep processing expression until no more commas
    do {
      expression();
      emitByte(OpCode::PRINT);
    } while (match(TokenType::COMMA));
    emitByte(OpCode::NEWLINE);
    parser.consume(TokenType::RIGHT_PAREN, "Expect ')' after value.");
    if (end_line == ';')
      parser.consume(TokenType::SEMICOLON, "Expect ';' after statement.");
  }
  void expressionStatement() {
    expression();
    if (end_line == ';') {
      if (parser.previous.type != TokenType::RIGHT_BRACE)
        parser.consume(TokenType::SEMICOLON, "Expect ';' after value.");
      else
        (void)match(TokenType::SEMICOLON);
    }
    emitByte(OpCode::POP);
  }
  int emitJump(std::uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->code.size() - 2;
  }
  void patchJump(int offset) {
    const auto count = currentChunk()->code.size();
    int jump = count - offset - 2;
    if (jump > UINT16_MAX) {
      parser.error("Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
  }
  void emitLoop(int loopStart) {
    emitByte(OpCode::LOOP);
    int offset = currentChunk()->code.size() - loopStart + 2;
    if (offset > UINT16_MAX)
      parser.error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
  }
  void ifStatement() {
    parser.consume(TokenType::LEFT_PAREN, "Expect  '(' after 'if'.");
    expression();
    parser.consume(TokenType::RIGHT_PAREN, "Expect  ')' after condition.");

    int thenJump = emitJump(OpCode::JUMP_IF_FALSE);
    emitByte(OpCode::POP);
    statement();

    int elseJump = emitJump(OpCode::JUMP);
    patchJump(thenJump);
    emitByte(OpCode::POP);

    if (match(TokenType::ELSE))
      statement();
    patchJump(elseJump);
  }
  void whileStatement() {

    int loopStart = currentChunk()->code.size();

    parser.consume(TokenType::LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    parser.consume(TokenType::RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OpCode::JUMP_IF_FALSE);
    emitByte(OpCode::POP);
    statement();
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OpCode::POP);
  }

  void forStatement() {
    beginScope();
    parser.consume(TokenType::LEFT_PAREN, "Expect '(' after 'for'.");
    // Inside the for-clauses ';' is always the separator
    char saved_end_line = end_line;
    end_line = ';';

    if (match(TokenType::SEMICOLON)) {
      // no initializer
    } else if (match(TokenType::VAR)) {
      varDeclaration();
    } else {
      expressionStatement();
    }

    int loopStart = currentChunk()->code.size();
    int exitJump = -1;
    if (!match(TokenType::SEMICOLON)) {
      expression();
      parser.consume(TokenType::SEMICOLON, "Expect ';' after loop condition.");

      exitJump = emitJump(OpCode::JUMP_IF_FALSE);
      emitByte(OpCode::POP);
    }

    if (!match(TokenType::RIGHT_PAREN)) {
      int bodyJump = emitJump(OpCode::JUMP);
      int incrementStart = currentChunk()->code.size();
      expression();
      emitByte(OpCode::POP);
      parser.consume(TokenType::RIGHT_PAREN, "Expect ')' after for clauses.");
      emitLoop(loopStart);
      loopStart = incrementStart;
      patchJump(bodyJump);
    }
    end_line = saved_end_line;
    statement();
    emitLoop(loopStart);
    if (exitJump != -1) {
      patchJump(exitJump);
      emitByte(OpCode::POP);
    }
    endScope();
  }

  void endCompiler() {
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
      currentChunk()->disassemble("Code");
    }
#endif
  }
  bool check(TokenType type) { return parser.current.type == type; }
  bool match(TokenType type) {
    if (!check(type))
      return false;
    parser.advance();
    return true;
  }
  void block() {
    while (!check(TokenType::RIGHT_BRACE) && !check(TokenType::END)) {
      declaration();
    }
    parser.consume(TokenType::RIGHT_BRACE, "Expect '}' after block.");
  }
  void beginScope() { current->scopeDepth++; }
  void endScope() {
    current->scopeDepth--;
    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth >
               current->scopeDepth) {
      emitByte(OpCode::POP);
      current->localCount--;
    }
  }
  void statement() {
    if (match(TokenType::PRINT)) {
      printStatement();
    } else if (match(TokenType::LIST)) {
      listStatement();
    } else if (match(TokenType::GLOBALS)) {
      globalsStatement();
    } else if (match(TokenType::LOCALS)) {
      localsStatement();
    } else if (match(TokenType::STACK)) {
      stackStatement();
    } else if (match(TokenType::LISTFUNC)) {
      listfuncStatement();
    } else if (match(TokenType::LEFT_BRACE)) {
      beginScope();
      block();
      endScope();
    } else if (match(TokenType::IF)) {
      ifStatement();
    } else if (match(TokenType::WHILE)) {
      whileStatement();
    } else if (match(TokenType::FOR)) {
      forStatement();
    } else if (match(TokenType::RETURN)) {
      returnStatement();
    } else {
      expressionStatement();
    }
  }
  void returnStatement() {
    if (current->type == FunctionType::SCRIPT) {
      parser.error("Can't return from top-level code.");
    }
    if (check(TokenType::SEMICOLON) ||
        (end_line != ';' && check(TokenType::END))) {
      emitReturn();
    } else {
      expression();
      if (end_line == ';')
        parser.consume(TokenType::SEMICOLON, "Expect ';' after return value.");
      emitByte(OpCode::RETURN);
      return;
    }
    if (end_line == ';')
      parser.consume(TokenType::SEMICOLON, "Expect ';' after return.");
  }

  void function(FunctionType type) {
    Token nameTok = parser.previous;
    std::string fnName(nameTok.start, nameTok.length);
    Function *fn = &(*fnTable)[fnName];
    fn->name = fnName;
    fn->arity = 0;
    fn->chunk = Chunk();

    CompilerState state;
    state.function = fn;
    state.type = type;
    state.enclosing = current;
    state.localCount = 0;
    state.scopeDepth = 0;
    current = &state;

    beginScope();
    parser.consume(TokenType::LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TokenType::RIGHT_PAREN)) {
      do {
        current->function->arity++;
        if (current->function->arity > 255) {
          parser.error("Can't have more than 255 parameters.");
        }
        std::uint8_t constant = parseVariable("Expect parameter name.");
        defineVariable(constant);
      } while (match(TokenType::COMMA));
    }
    parser.consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");
    parser.consume(TokenType::LEFT_BRACE, "Expect '{' before function body.");
    block();
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError)
      currentChunk()->disassemble(fn->name.c_str());
#endif
    current = state.enclosing;
  }
  void funDeclaration() {
    parser.consume(TokenType::IDENTIFIER, "Expect function name.");
    function(FunctionType::FUNCTION);
  }

  void compileMethod(ClassDef *cls, bool /*isCtor*/) {
    // Caller already consumed the IDENTIFIER for the method name.
    Token nameTok = parser.previous;
    std::string methodName(nameTok.start, nameTok.length);
    std::string qname = cls->name + "::" + methodName;
    Function *fn = &(*fnTable)[qname];
    fn->name = qname;
    fn->arity = 0;
    fn->chunk = Chunk();

    CompilerState state;
    state.function = fn;
    state.type = FunctionType::FUNCTION;
    state.enclosing = current;
    state.localCount = 0;
    state.scopeDepth = 0;
    current = &state;

    beginScope();
    // Reserve slot 0 as the synthetic `this` local.
    Local &thisLocal = current->locals[current->localCount++];
    thisLocal.name.start = "this";
    thisLocal.name.length = 4;
    thisLocal.name.line = parser.previous.line;
    thisLocal.depth = current->scopeDepth;

    parser.consume(TokenType::LEFT_PAREN, "Expect '(' after method name.");
    if (!check(TokenType::RIGHT_PAREN)) {
      do {
        current->function->arity++;
        if (current->function->arity > 254) {
          parser.error("Can't have more than 254 parameters.");
        }
        std::uint8_t constant = parseVariable("Expect parameter name.");
        defineVariable(constant);
      } while (match(TokenType::COMMA));
    }
    parser.consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");
    parser.consume(TokenType::LEFT_BRACE, "Expect '{' before method body.");
    block();
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError)
      currentChunk()->disassemble(fn->name.c_str());
#endif
    current = state.enclosing;
  }

  void classDeclaration() {
    parser.consume(TokenType::IDENTIFIER, "Expect class name.");
    Token classNameTok = parser.previous;
    std::string className(classNameTok.start, classNameTok.length);

    if (current->type != FunctionType::SCRIPT) {
      parser.error("Classes can only be declared at script scope.");
      return;
    }
    if (!classTable) {
      parser.error("No class table available.");
      return;
    }

    ClassDef &cls = (*classTable)[className];
    cls.name = className;
    cls.fields.clear();
    cls.methods.clear();
    cls.hasCtor = false;

    // Synthesize the __init_fields function for default-value initialization.
    std::string initQname = className + "::__init_fields";
    Function *initFn = &(*fnTable)[initQname];
    initFn->name = initQname;
    initFn->arity = 0;
    initFn->chunk = Chunk();

    CompilerState initState;
    initState.function = initFn;
    initState.type = FunctionType::FUNCTION;
    initState.enclosing = current;
    initState.localCount = 0;
    initState.scopeDepth = 1;
    // Slot 0 of __init_fields is `this`.
    initState.locals[0].name.start = "this";
    initState.locals[0].name.length = 4;
    initState.locals[0].name.line = parser.previous.line;
    initState.locals[0].depth = 1;
    initState.localCount = 1;

    CompilerState *outerState = current;
    ClassDef *savedClass = currentClass;
    currentClass = &cls;

    parser.consume(TokenType::LEFT_BRACE, "Expect '{' before class body.");

    bool seenMethod = false;
    while (!check(TokenType::RIGHT_BRACE) && !check(TokenType::END) &&
           !parser.hadError) {
      if (match(TokenType::VAR)) {
        if (seenMethod) {
          parser.error("Field declarations must come before method "
                       "declarations in a class body.");
          break;
        }
        parser.consume(TokenType::IDENTIFIER, "Expect field name.");
        Token fieldTok = parser.previous;
        std::string fieldName(fieldTok.start, fieldTok.length);
        cls.fields.push_back(fieldName);

        if (match(TokenType::EQUAL)) {
          // Emit field-default code into __init_fields:
          current = &initState;
          std::uint8_t nameConst = identifierConstant(&fieldTok);
          emitBytes(OpCode::GET_LOCAL, 0);
          expression();
          emitByte(OpCode::SET_PROPERTY);
          emitByte(nameConst);
          emitByte(OpCode::POP);
          current = outerState;
        }
        // Optional terminator inside class body.
        if (check(TokenType::SEMICOLON))
          parser.advance();
      } else if (match(TokenType::FUN)) {
        seenMethod = true;
        parser.consume(TokenType::IDENTIFIER, "Expect method name.");
        Token methodTok = parser.previous;
        std::string methodName(methodTok.start, methodTok.length);
        if (methodName == className) {
          parser.error("Constructors are not supported; use 'new C { .field = expr }' instead.");
        }
        cls.methods.insert(methodName);
        compileMethod(&cls, false);
      } else {
        parser.errorAtCurrent("Expect 'var' or 'fn' in class body.");
        parser.advance();
      }
    }
    parser.consume(TokenType::RIGHT_BRACE, "Expect '}' after class body.");

    // Close __init_fields with `return nil`.
    current = &initState;
    emitByte(OpCode::NIL);
    emitByte(OpCode::RETURN);
    current = outerState;

    currentClass = savedClass;
  }
  void declaration() {
    if (match(TokenType::CLASS)) {
      classDeclaration();
    } else if (match(TokenType::FUN)) {
      funDeclaration();
    } else if (match(TokenType::VAR)) {
      varDeclaration();
    }
#ifdef NO_VAR_DECL
    // This should sometimes go down the statement path
    // if we are updating a variable that was declared in a lower scope
    else if (match(TokenType::IDENTIFIER)) {
      varDeclarationNoVar();
    }
#endif
    else {
      statement();
    }
    if (parser.panicMode)
      synchronize();
  }
  bool compile(Function *fn,
               std::unordered_map<std::string, Function> *fns,
               std::unordered_map<std::string, ClassDef> *classes = nullptr) {
    fnTable = fns;
    classTable = classes;
    CompilerState rootState;
    rootState.function = fn;
    rootState.type = FunctionType::SCRIPT;
    rootState.enclosing = nullptr;
    current = &rootState;

    parser.advance();
    while (!match(TokenType::END)) {
      declaration();
    }
    endCompiler();
    current = rootState.enclosing;
    return !parser.hadError;
  }
};

} // namespace pips

#endif // PIPS_COMPILER_HPP_
