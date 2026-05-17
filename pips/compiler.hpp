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
#include "scanner.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "value.hpp"
#include "vm.hpp"

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
  // function table
  std::unordered_map<std::string, Function> *fnTable = nullptr;
  char end_line = ';';

  // clang-format off
  std::array<Precedence, 14> prec_array{
      Precedence::NONE,  Precedence::ASSIGNMENT, Precedence::TERNARY, 
      Precedence::OR, Precedence::XOR, Precedence::AND,   
      Precedence::EQUALITY,   Precedence::COMPARISON,
      Precedence::TERM,  Precedence::FACTOR,     Precedence::POWER,
      Precedence::UNARY, Precedence::CALL,       Precedence::PRIMARY};
  std::array<void (Compiler::*)(bool), 87> prefix_rules{&Compiler::grouping, // LEFT_PAREN
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
                                                        nullptr,             // THIS
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
                                                        &Compiler::max,      // MAX
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
                                                        nullptr,             // ERROR
                                                        nullptr};            // END

  std::array<void (Compiler::*)(bool), 87> infix_rules{nullptr,           // LEFT_PAREN
                                                       nullptr,           // RIGHT_PAREN
                                                       nullptr,           // LEFT_BRACE
                                                       nullptr,           // RIGHT_BRACE
                                                       nullptr,           // COMMA
                                                       nullptr,           // DOT
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
                                                       nullptr,           // ERROR
                                                       nullptr};          // END

  std::array<Precedence, 87> prec_rules{Precedence::NONE,       // LEFT_PAREN
                                        Precedence::NONE,       // RIGHT_PAREN
                                        Precedence::NONE,       // LEFT_BRACE
                                        Precedence::NONE,       // RIGHT_BRACE
                                        Precedence::NONE,       // COMMA
                                        Precedence::NONE,       // DOT
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
    if (end_line == ';')
      parser.consume(TokenType::SEMICOLON,
                     "Expect ';' after variable declaration.");
    defineVariable(global);
  }
  void varDeclaration() {
    auto global = parseVariable("Expect variable name.");
    if (match(TokenType::EQUAL)) {
      expression();
    } else {
      emitByte(OpCode::NIL);
    }
    if (end_line == ';')
      parser.consume(TokenType::SEMICOLON,
                     "Expect ';' after variable declaration.");
    defineVariable(global);
  }

  std::uint8_t identifierConstant(Token *name) {
    Value val;
    val.type = ValueType::STRING;
    name->copy(val.as.str);
    return makeConstant(val);
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
  void namedVariable(Token name, bool canAssign) {
    std::uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
      getOp = OpCode::GET_LOCAL;
      setOp = OpCode::SET_LOCAL;
    } else {
      arg = identifierConstant(&name);
      getOp = OpCode::GET_GLOBAL;
      setOp = OpCode::SET_GLOBAL;
    }
    if (canAssign && match(TokenType::EQUAL)) {
      expression();
      emitBytes(setOp, (std::uint8_t)arg);
      return;
    }
    if (canAssign) {
      // Compound assignment
      auto compound = [&](OpCode op) {
        emitBytes(getOp, (std::uint8_t)arg);
        expression();
        emitByte(op);
        emitBytes(setOp, (std::uint8_t)arg);
      };
      // Postfix increment/decrement
      auto incDec = [&](OpCode op) {
        emitBytes(getOp, (std::uint8_t)arg);
        emitConstant(NUMBER_VAL(1.0));
        emitByte(op);
        emitBytes(setOp, (std::uint8_t)arg);
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
    emitBytes(getOp, (std::uint8_t)arg);
  }
  // Prefix ++x / --x
  void preIncDec(OpCode op) {
    parser.consume(TokenType::IDENTIFIER,
                   "Expect variable name after '++' or '--'.");
    Token name = parser.previous;
    std::uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
      getOp = OpCode::GET_LOCAL;
      setOp = OpCode::SET_LOCAL;
    } else {
      arg = identifierConstant(&name);
      getOp = OpCode::GET_GLOBAL;
      setOp = OpCode::SET_GLOBAL;
    }
    emitBytes(getOp, (std::uint8_t)arg);
    emitConstant(NUMBER_VAL(1.0));
    emitByte(op);
    emitBytes(setOp, (std::uint8_t)arg);
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
    Value val;
    val.type = ValueType::STRING;
    std::memcpy(val.as.str, parser.previous.start + 1,
                parser.previous.length - 2);
    val.as.str[parser.previous.length - 2] = '\0';
    emitConstant(val);
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
      parser.consume(TokenType::SEMICOLON, "Expect ';' after value.");
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
  void declaration() {
    if (match(TokenType::FUN)) {
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
               std::unordered_map<std::string, Function> *fns) {
    fnTable = fns;
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
