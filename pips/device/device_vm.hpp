#ifndef PIPS_DEVICE_DEVICE_VM_HPP_
#define PIPS_DEVICE_DEVICE_VM_HPP_

#include "device_common.hpp"
#include "device_chunk.hpp"
#include "device_opcode.hpp"
#include "device_status.hpp"
#include "device_value.hpp"

#include <cmath>
#include <cstdint>

// Per-thread stack sizing
#ifndef PIPS_DEVICE_STACK_MAX
#define PIPS_DEVICE_STACK_MAX 64
#endif
#ifndef PIPS_DEVICE_FRAMES_MAX
#define PIPS_DEVICE_FRAMES_MAX 16
#endif

namespace pips {
namespace device {

struct DeviceFrame {
  std::uint32_t func_id;
  std::uint32_t ip;         
  std::uint32_t frame_base; 
};

// A simple bytecode VM that executes per-thread on device
struct DeviceVM {
  DeviceValue stack[PIPS_DEVICE_STACK_MAX];
  DeviceFrame frames[PIPS_DEVICE_FRAMES_MAX];
  std::int32_t sp = 0; 
  std::int32_t fp = 0; 

  PIPS_DEVICE_HOST_INLINE bool push(const DeviceValue &v) {
    if (sp >= PIPS_DEVICE_STACK_MAX) return false;
    stack[sp++] = v;
    return true;
  }
  PIPS_DEVICE_HOST_INLINE DeviceValue pop() { return stack[--sp]; }
  PIPS_DEVICE_HOST_INLINE const DeviceValue &peek(int back) const {
    return stack[sp - 1 - back];
  }

  // Execute a packed function
  PIPS_DEVICE_HOST DeviceStatus run(const DeviceModule &module,
                                    std::uint32_t entry_id,
                                    const DeviceValue *args,
                                    std::uint32_t argc,
                                    DeviceValue *out_result) {
    sp = 0;
    fp = 0;
    if (entry_id >= module.function_count) return DeviceStatus::BAD_FUNCTION_ID;

    const DeviceFunction &entry = module.functions[entry_id];
    if (argc != entry.arity) return DeviceStatus::ARITY_MISMATCH;
    for (std::uint32_t i = 0; i < argc; ++i) {
      if (!dv_vector_is_valid(args[i])) return DeviceStatus::INVALID_VECTOR;
      if (!push(args[i])) return DeviceStatus::STACK_OVERFLOW;
    }

    if (fp >= PIPS_DEVICE_FRAMES_MAX) return DeviceStatus::FRAME_OVERFLOW;
    frames[fp].func_id = entry_id;
    frames[fp].ip = entry.code_offset;
    frames[fp].frame_base = 0;
    fp++;

    DeviceStatus st = dispatch(module, out_result);
    return st;
  }

private:
  PIPS_DEVICE_HOST_INLINE static DeviceStatus
  scalar_arith(DeviceOpCode op, DeviceReal a, DeviceReal b, DeviceReal &out) {
    switch (op) {
    case DeviceOpCode::ADD: out = a + b; return DeviceStatus::OK;
    case DeviceOpCode::SUB: out = a - b; return DeviceStatus::OK;
    case DeviceOpCode::MUL: out = a * b; return DeviceStatus::OK;
    case DeviceOpCode::DIV:
      if (b == 0) return DeviceStatus::DIV_BY_ZERO;
      out = a / b;
      return DeviceStatus::OK;
    case DeviceOpCode::MOD: {
      long long ib = static_cast<long long>(b);
      if (ib == 0) return DeviceStatus::DIV_BY_ZERO;
      out = static_cast<DeviceReal>(static_cast<long long>(a) % ib);
      return DeviceStatus::OK;
    }
    case DeviceOpCode::POW: out = std::pow(a, b); return DeviceStatus::OK;
    default: return DeviceStatus::BAD_OPCODE;
    }
  }

  PIPS_DEVICE_HOST_INLINE static DeviceStatus
  vector_arith(DeviceOpCode op, const DeviceValue &a, const DeviceValue &b,
               DeviceValue &out) {
    const bool av = dv_is_vector(a);
    const bool bv = dv_is_vector(b);
    if ((!av && !dv_is_number(a)) || (!bv && !dv_is_number(b)))
      return DeviceStatus::TYPE_ERROR;
    if (!dv_vector_is_valid(a) || !dv_vector_is_valid(b))
      return DeviceStatus::INVALID_VECTOR;

    if (!av && !bv) {
      DeviceReal result = 0;
      DeviceStatus st = scalar_arith(op, a.as.n, b.as.n, result);
      if (st == DeviceStatus::OK) out = dv_number(result);
      return st;
    }

    std::uint8_t length = av ? a.as.vector.length : b.as.vector.length;
    if (av && bv && a.as.vector.length != b.as.vector.length)
      return DeviceStatus::VECTOR_LENGTH_MISMATCH;

    DeviceValue result{};
    result.type = DeviceValueType::VECTOR;
    result.as.vector.length = length;
    for (std::uint8_t i = 0; i < length; ++i) {
      DeviceReal lhs = av ? a.as.vector.elements[i] : a.as.n;
      DeviceReal rhs = bv ? b.as.vector.elements[i] : b.as.n;
      DeviceStatus st = scalar_arith(
          op, lhs, rhs, result.as.vector.elements[i]);
      if (st != DeviceStatus::OK) return st;
    }
    out = result;
    return DeviceStatus::OK;
  }

  PIPS_DEVICE_HOST DeviceStatus dispatch(const DeviceModule &module,
                                         DeviceValue *out_result) {
    using OC = DeviceOpCode;

    DeviceFrame *frame = &frames[fp - 1];
    const DeviceFunction *func = &module.functions[frame->func_id];
    const std::uint8_t *code = module.code;
    const DeviceValue *fconsts = module.constants + func->const_offset;
    std::uint32_t ip = frame->ip;
    std::uint32_t code_end = func->code_offset + func->code_size;

    while (true) {
      if (ip >= code_end) return DeviceStatus::BAD_OPCODE;
      auto op = static_cast<OC>(code[ip++]);

      switch (op) {
      case OC::CONSTANT: {
        if (ip >= code_end) return DeviceStatus::BAD_OPCODE;
        std::uint8_t idx = code[ip++];
        if (idx >= func->const_count) return DeviceStatus::BAD_CONST_ID;
        if (!push(fconsts[idx])) return DeviceStatus::STACK_OVERFLOW;
        break;
      }
      case OC::NIL:
        if (!push(dv_nil())) return DeviceStatus::STACK_OVERFLOW;
        break;
      case OC::TRUE:
        if (!push(dv_bool(true))) return DeviceStatus::STACK_OVERFLOW;
        break;
      case OC::FALSE:
        if (!push(dv_bool(false))) return DeviceStatus::STACK_OVERFLOW;
        break;

      case OC::NEGATE: {
        if (sp < 1) return DeviceStatus::STACK_UNDERFLOW;
        DeviceValue &top = stack[sp - 1];
        if (dv_is_number(top)) {
          top.as.n = -top.as.n;
        } else if (dv_is_vector(top)) {
          if (!dv_vector_is_valid(top)) return DeviceStatus::INVALID_VECTOR;
          for (std::uint8_t i = 0; i < top.as.vector.length; ++i)
            top.as.vector.elements[i] = -top.as.vector.elements[i];
        } else {
          return DeviceStatus::TYPE_ERROR;
        }
        break;
      }
      case OC::UPLUS: {
        if (sp < 1) return DeviceStatus::STACK_UNDERFLOW;
        if (!dv_is_number(stack[sp - 1]) && !dv_is_vector(stack[sp - 1]))
          return DeviceStatus::TYPE_ERROR;
        if (!dv_vector_is_valid(stack[sp - 1]))
          return DeviceStatus::INVALID_VECTOR;
        break;
      }
      case OC::NOT: {
        if (sp < 1) return DeviceStatus::STACK_UNDERFLOW;
        bool b = dv_is_falsey(stack[sp - 1]);
        stack[sp - 1] = dv_bool(b);
        break;
      }

#define DV_BIN_NUM(EXPR)                                                       \
  do {                                                                         \
    if (sp < 2) return DeviceStatus::STACK_UNDERFLOW;                          \
    if (!dv_is_number(stack[sp - 1]) || !dv_is_number(stack[sp - 2]))          \
      return DeviceStatus::TYPE_ERROR;                                         \
    DeviceReal b = stack[sp - 1].as.n;                                         \
    DeviceReal a = stack[sp - 2].as.n;                                         \
    sp -= 2;                                                                   \
    (void)b;                                                                   \
    (void)a;                                                                   \
    if (!push(dv_number(EXPR))) return DeviceStatus::STACK_OVERFLOW;           \
  } while (0)

      case OC::ADD:
      case OC::SUB:
      case OC::MUL:
      case OC::DIV:
      case OC::MOD:
      case OC::POW: {
        if (sp < 2) return DeviceStatus::STACK_UNDERFLOW;
        DeviceValue result{};
        DeviceStatus st = vector_arith(op, stack[sp - 2], stack[sp - 1],
                                       result);
        if (st != DeviceStatus::OK) return st;
        sp -= 2;
        if (!push(result)) return DeviceStatus::STACK_OVERFLOW;
        break;
      }
      case OC::INTDIV: {
        if (sp < 2) return DeviceStatus::STACK_UNDERFLOW;
        if (!dv_is_number(stack[sp - 1]) || !dv_is_number(stack[sp - 2]))
          return DeviceStatus::TYPE_ERROR;
        DeviceReal b = stack[sp - 1].as.n;
        DeviceReal a = stack[sp - 2].as.n;
        if (b == 0) return DeviceStatus::DIV_BY_ZERO;
        sp -= 2;
        DeviceReal r = static_cast<DeviceReal>(static_cast<long long>(a / b));
        if (!push(dv_number(r))) return DeviceStatus::STACK_OVERFLOW;
        break;
      }
      case OC::EQUAL: {
        if (sp < 2) return DeviceStatus::STACK_UNDERFLOW;
        DeviceValue b = stack[--sp];
        DeviceValue a = stack[--sp];
        bool eq = false;
        if (a.type == b.type) {
          switch (a.type) {
          case DeviceValueType::NIL: eq = true; break;
          case DeviceValueType::BOOL: eq = (a.as.b == b.as.b); break;
          case DeviceValueType::NUMBER: eq = (a.as.n == b.as.n); break;
          case DeviceValueType::VECTOR:
            if (!dv_vector_is_valid(a) || !dv_vector_is_valid(b))
              return DeviceStatus::INVALID_VECTOR;
            eq = a.as.vector.length == b.as.vector.length;
            for (std::uint8_t i = 0; eq && i < a.as.vector.length; ++i)
              eq = a.as.vector.elements[i] == b.as.vector.elements[i];
            break;
          }
        }
        if (!push(dv_bool(eq))) return DeviceStatus::STACK_OVERFLOW;
        break;
      }
      case OC::GREATER: {
        if (sp < 2) return DeviceStatus::STACK_UNDERFLOW;
        if (!dv_is_number(stack[sp - 1]) || !dv_is_number(stack[sp - 2]))
          return DeviceStatus::TYPE_ERROR;
        DeviceReal b = stack[sp - 1].as.n;
        DeviceReal a = stack[sp - 2].as.n;
        sp -= 2;
        if (!push(dv_bool(a > b))) return DeviceStatus::STACK_OVERFLOW;
        break;
      }
      case OC::LESS: {
        if (sp < 2) return DeviceStatus::STACK_UNDERFLOW;
        if (!dv_is_number(stack[sp - 1]) || !dv_is_number(stack[sp - 2]))
          return DeviceStatus::TYPE_ERROR;
        DeviceReal b = stack[sp - 1].as.n;
        DeviceReal a = stack[sp - 2].as.n;
        sp -= 2;
        if (!push(dv_bool(a < b))) return DeviceStatus::STACK_OVERFLOW;
        break;
      }

#define DV_BIN_INT(EXPR)                                                       \
  do {                                                                         \
    if (sp < 2) return DeviceStatus::STACK_UNDERFLOW;                          \
    if (!dv_is_number(stack[sp - 1]) || !dv_is_number(stack[sp - 2]))          \
      return DeviceStatus::TYPE_ERROR;                                         \
    long long b = static_cast<long long>(stack[sp - 1].as.n);                  \
    long long a = static_cast<long long>(stack[sp - 2].as.n);                  \
    sp -= 2;                                                                   \
    (void)a;                                                                   \
    (void)b;                                                                   \
    if (!push(dv_number(static_cast<DeviceReal>(EXPR))))                       \
      return DeviceStatus::STACK_OVERFLOW;                                     \
  } while (0)

      case OC::XOR: DV_BIN_INT(a ^ b); break;
      case OC::BOR: DV_BIN_INT(a | b); break;
      case OC::BAND: DV_BIN_INT(a & b); break;
      case OC::LSHIFT: DV_BIN_INT(a << b); break;
      case OC::RSHIFT: DV_BIN_INT(a >> b); break;
      case OC::BNOT: {
        if (sp < 1) return DeviceStatus::STACK_UNDERFLOW;
        if (!dv_is_number(stack[sp - 1])) return DeviceStatus::TYPE_ERROR;
        long long a = static_cast<long long>(stack[sp - 1].as.n);
        stack[sp - 1] = dv_number(static_cast<DeviceReal>(~a));
        break;
      }

#define DV_UN_NUM(EXPR)                                                        \
  do {                                                                         \
    if (sp < 1) return DeviceStatus::STACK_UNDERFLOW;                          \
    DeviceValue &top = stack[sp - 1];                                          \
    if (dv_is_number(top)) {                                                   \
      DeviceReal a = top.as.n;                                                 \
      (void)a;                                                                 \
      top = dv_number(EXPR);                                                   \
    } else if (dv_is_vector(top)) {                                            \
      if (!dv_vector_is_valid(top)) return DeviceStatus::INVALID_VECTOR;       \
      for (std::uint8_t i = 0; i < top.as.vector.length; ++i) {                \
        DeviceReal a = top.as.vector.elements[i];                              \
        (void)a;                                                               \
        top.as.vector.elements[i] = (EXPR);                                    \
      }                                                                        \
    } else {                                                                   \
      return DeviceStatus::TYPE_ERROR;                                         \
    }                                                                          \
  } while (0)

      case OC::EXP: DV_UN_NUM(std::exp(a)); break;
      case OC::SIN: DV_UN_NUM(std::sin(a)); break;
      case OC::COS: DV_UN_NUM(std::cos(a)); break;
      case OC::TAN: DV_UN_NUM(std::tan(a)); break;
      case OC::ABS: DV_UN_NUM(a < 0 ? -a : a); break;
      case OC::LOG: DV_UN_NUM(std::log(a)); break;
      case OC::LOG10: DV_UN_NUM(std::log10(a)); break;
      case OC::SIGN: DV_UN_NUM(a < 0 ? -1 : (a > 0 ? 1 : 0)); break;
      case OC::SQRT: DV_UN_NUM(std::sqrt(a)); break;
      case OC::ACOS: DV_UN_NUM(std::acos(a)); break;
      case OC::ASIN: DV_UN_NUM(std::asin(a)); break;
      case OC::ATAN: DV_UN_NUM(std::atan(a)); break;
      case OC::CEIL: DV_UN_NUM(std::ceil(a)); break;
      case OC::FLOOR: DV_UN_NUM(std::floor(a)); break;
      case OC::ATAN2: DV_BIN_NUM(std::atan2(a, b)); break;
      case OC::MIN: DV_BIN_NUM(a < b ? a : b); break;
      case OC::MAX: DV_BIN_NUM(a > b ? a : b); break;

#undef DV_BIN_NUM
#undef DV_BIN_INT
#undef DV_UN_NUM

      case OC::POP:
        if (sp < 1) return DeviceStatus::STACK_UNDERFLOW;
        sp--;
        break;

      case OC::GET_LOCAL: {
        if (ip >= code_end) return DeviceStatus::BAD_OPCODE;
        std::uint8_t slot = code[ip++];
        std::uint32_t idx = frame->frame_base + slot;
        if (idx >= static_cast<std::uint32_t>(sp))
          return DeviceStatus::BAD_LOCAL_SLOT;
        if (!push(stack[idx])) return DeviceStatus::STACK_OVERFLOW;
        break;
      }
      case OC::SET_LOCAL: {
        if (ip >= code_end) return DeviceStatus::BAD_OPCODE;
        std::uint8_t slot = code[ip++];
        std::uint32_t idx = frame->frame_base + slot;
        if (sp < 1) return DeviceStatus::STACK_UNDERFLOW;
        if (idx >= static_cast<std::uint32_t>(sp))
          return DeviceStatus::BAD_LOCAL_SLOT;
        stack[idx] = stack[sp - 1];
        break;
      }

      case OC::GET_GLOBAL_ID: {
        if (ip + 1 >= code_end) return DeviceStatus::BAD_OPCODE;
        std::uint16_t gid = static_cast<std::uint16_t>((code[ip] << 8) | code[ip + 1]);
        ip += 2;
        if (gid >= module.global_count) return DeviceStatus::BAD_GLOBAL_ID;
        if (!push(module.globals[gid])) return DeviceStatus::STACK_OVERFLOW;
        break;
      }

      case OC::JUMP_IF_FALSE: {
        if (ip + 1 >= code_end) return DeviceStatus::BAD_OPCODE;
        std::uint16_t off = static_cast<std::uint16_t>((code[ip] << 8) | code[ip + 1]);
        ip += 2;
        if (sp < 1) return DeviceStatus::STACK_UNDERFLOW;
        if (dv_is_falsey(stack[sp - 1])) ip += off;
        break;
      }
      case OC::JUMP: {
        if (ip + 1 >= code_end) return DeviceStatus::BAD_OPCODE;
        std::uint16_t off = static_cast<std::uint16_t>((code[ip] << 8) | code[ip + 1]);
        ip += 2;
        ip += off;
        break;
      }
      case OC::LOOP: {
        if (ip + 1 >= code_end) return DeviceStatus::BAD_OPCODE;
        std::uint16_t off = static_cast<std::uint16_t>((code[ip] << 8) | code[ip + 1]);
        ip += 2;
        ip -= off;
        break;
      }

      case OC::CALL_ID: {
        if (ip + 2 >= code_end) return DeviceStatus::BAD_OPCODE;
        std::uint16_t fid = static_cast<std::uint16_t>((code[ip] << 8) | code[ip + 1]);
        ip += 2;
        std::uint8_t argc = code[ip++];
        if (fid >= module.function_count) return DeviceStatus::BAD_FUNCTION_ID;
        const DeviceFunction &callee = module.functions[fid];
        if (argc != callee.arity) return DeviceStatus::ARITY_MISMATCH;
        if (fp >= PIPS_DEVICE_FRAMES_MAX) return DeviceStatus::FRAME_OVERFLOW;

        frame->ip = ip;
        frames[fp].func_id = fid;
        frames[fp].ip = callee.code_offset;
        frames[fp].frame_base = static_cast<std::uint32_t>(sp) - argc;
        fp++;

        frame = &frames[fp - 1];
        func = &callee;
        fconsts = module.constants + func->const_offset;
        ip = frame->ip;
        code_end = func->code_offset + func->code_size;
        break;
      }
      case OC::BUILD_VECTOR: {
        if (ip >= code_end) return DeviceStatus::BAD_OPCODE;
        std::uint8_t count = code[ip++];
        if (count > PIPS_DEVICE_VECTOR_MAX)
          return DeviceStatus::INVALID_VECTOR;
        if (sp < count) return DeviceStatus::STACK_UNDERFLOW;
        DeviceValue result{};
        result.type = DeviceValueType::VECTOR;
        result.as.vector.length = count;
        std::int32_t first = sp - count;
        for (std::uint8_t i = 0; i < count; ++i) {
          if (!dv_is_number(stack[first + i])) return DeviceStatus::TYPE_ERROR;
          result.as.vector.elements[i] = stack[first + i].as.n;
        }
        sp = first;
        if (!push(result)) return DeviceStatus::STACK_OVERFLOW;
        break;
      }
      case OC::GET_INDEX: {
        if (sp < 2) return DeviceStatus::STACK_UNDERFLOW;
        DeviceValue index = stack[sp - 1];
        DeviceValue vector = stack[sp - 2];
        if (!dv_is_vector(vector)) return DeviceStatus::TYPE_ERROR;
        if (!dv_vector_is_valid(vector)) return DeviceStatus::INVALID_VECTOR;
        long long raw = 0;
        if (dv_is_number(index))
          raw = static_cast<long long>(index.as.n);
        else if (dv_is_bool(index))
          raw = index.as.b ? 1 : 0;
        else
          return DeviceStatus::TYPE_ERROR;
        long long normalized = raw;
        if (normalized < 0) normalized += vector.as.vector.length;
        if (normalized < 0 || normalized >= vector.as.vector.length)
          return DeviceStatus::INDEX_OUT_OF_RANGE;
        sp -= 2;
        if (!push(dv_number(vector.as.vector.elements[normalized])))
          return DeviceStatus::STACK_OVERFLOW;
        break;
      }
      case OC::RETURN: {
        if (sp < 1) return DeviceStatus::STACK_UNDERFLOW;
        DeviceValue result = stack[--sp];
        sp = static_cast<std::int32_t>(frame->frame_base);
        fp--;
        if (fp == 0) {
          if (out_result) *out_result = result;
          return DeviceStatus::OK;
        }
        if (!push(result)) return DeviceStatus::STACK_OVERFLOW;
        frame = &frames[fp - 1];
        func = &module.functions[frame->func_id];
        fconsts = module.constants + func->const_offset;
        ip = frame->ip;
        code_end = func->code_offset + func->code_size;
        break;
      }

      default:
        return DeviceStatus::BAD_OPCODE;
      }
    }
  }
};

} // namespace device
} // namespace pips

#endif // PIPS_DEVICE_DEVICE_VM_HPP_
