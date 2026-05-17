#ifndef PIPS_FUNCTION_HPP_
#define PIPS_FUNCTION_HPP_

#include "chunk.hpp"
#include <cstdint>
#include <string>

namespace pips {

#ifndef FRAMES_MAX
#define FRAMES_MAX 64
#endif


struct Function {
  std::string name;
  int arity = 0;
  Chunk chunk;

  Function() = default;
  Function(std::string name_, int arity_ = 0)
      : name(std::move(name_)), arity(arity_) {}
};

// CallFrames save the caller's state
struct CallFrame {
  // what function is begin called
  Function *function = nullptr;

  // caller state
  Chunk *chunk = nullptr;
  std::uint8_t *ip = nullptr;
  Value *slots = nullptr;
};

} // namespace pips

#endif // PIPS_FUNCTION_HPP_
