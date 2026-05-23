#ifndef PIPS_DEVICE_DEVICE_PACK_HPP_
#define PIPS_DEVICE_DEVICE_PACK_HPP_

// Host-side packer that converts a `pips::Function` (and its transitively
// called functions, restricted to the numeric+bool subset) into a flat
// `DeviceModule` suitable for upload to a device. See doc/embedding.md.

#include "../chunk.hpp"
#include "device_chunk.hpp"
#include "device_opcode.hpp"
#include "device_value.hpp"
#include "../function.hpp"
#include "../object.hpp"
#include "../value.hpp"

#include <cstdint>
#include <deque>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace pips {

struct VM; // forward decl; defined in vm.hpp

namespace device {

namespace detail {

// Number of operand bytes following the given host opcode. Used both for
// scanning unknown / skipped ops and for laying out the remapped bytecode.
inline int host_operand_size(OpCode op) {
  switch (op) {
  case OpCode::CONSTANT:
  case OpCode::DEFINE_GLOBAL:
  case OpCode::GET_GLOBAL:
  case OpCode::SET_GLOBAL:
  case OpCode::GET_LOCAL:
  case OpCode::SET_LOCAL:
  case OpCode::NEW_INSTANCE:
  case OpCode::GET_PROPERTY:
  case OpCode::SET_PROPERTY:
    return 1;
  case OpCode::GET_OUTER:
  case OpCode::SET_OUTER:
  case OpCode::JUMP_IF_FALSE:
  case OpCode::JUMP:
  case OpCode::LOOP:
  case OpCode::CALL:
  case OpCode::CALL_METHOD:
    return 2;
  default:
    return 0;
  }
}

// Convert a host pips::Value to a DeviceValue if it is in the device subset
// (nil / bool / number). Returns false otherwise.
inline bool host_to_device_value(const Value &v, DeviceValue &out) {
  switch (v.type) {
  case ValueType::NIL: out = dv_nil(); return true;
  case ValueType::BOOL: out = dv_bool(v.as.boolean); return true;
  case ValueType::NUMBER:
    out = dv_number(static_cast<DeviceReal>(v.as.number));
    return true;
  default:
    return false;
  }
}

} // namespace detail

// Pack the function named `entry` (which must already have been registered
// on `vm` via `vm.interpret(...)`) plus every function transitively called
// from it into a packed device module. The packer:
//
//   * walks the host bytecode of each visited function;
//   * rejects any opcode outside the numeric/bool subset
//     (strings, vectors, classes-as-objects, attrs, env, print, etc.);
//   * rewrites `CALL <name>` into `CALL_ID <u16 func_id>`;
//   * collects globals that are read by the bytecode (via `GET_GLOBAL`)
//     and snapshots their current values from `vm.globals`;
//   * recognises `GET_GLOBAL <ClassName>` immediately followed by
//     `GET_PROPERTY <member>` and folds it into a single
//     `GET_GLOBAL_ID <slot for ClassName.member>`; the value is pulled
//     from the class instance currently held in `vm.globals[ClassName]`.
//
// On success returns true, writes the entry function's id into `out_entry_id`
// and fills `out`. On failure returns false and writes a human-readable
// reason into `out_error`.
inline bool pack_function(const VM &vm,
                          const std::string &entry,
                          DeviceModuleStorage &out,
                          std::uint32_t &out_entry_id,
                          std::string &out_error);

// ---------------------------------------------------------------------------
// Implementation.
// ---------------------------------------------------------------------------

namespace detail {

struct Packer {
  const VM &vm;
  std::string error;

  // First-seen ordering for visited functions (host name -> device id).
  std::unordered_map<std::string, std::uint32_t> func_ids;
  std::vector<std::string> func_order; // device id -> host name

  // First-seen ordering for globals (slot name -> id).
  // For class members the slot name is "Class.member".
  std::unordered_map<std::string, std::uint16_t> global_ids;
  std::vector<std::string> global_names;
  std::vector<DeviceValue> global_values;

  // Output bytecode + constants, one rewritten block per function.
  struct PackedFn {
    std::vector<std::uint8_t> code;
    std::vector<DeviceValue> constants;
    std::uint8_t arity = 0;
    std::uint8_t max_stack = 0;
  };
  std::vector<PackedFn> packed;

  Packer(const VM &v) : vm(v) {}

  template <typename... Args> bool fail(const char *fmt, Args... args) {
    char buf[256];
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
    std::snprintf(buf, sizeof(buf), fmt, args...);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    error = buf;
    return false;
  }

  // Look up a name in vm.globals (definition in vm.hpp; this is implemented
  // inline below by the public entry point because it can't see VM here).
  const Value *find_global(const std::string &name) const;
  const Function *find_function(const std::string &name) const;
  const std::unordered_map<std::string, ClassDef> &class_table() const;

  std::uint16_t intern_global(const std::string &slot_name,
                              const DeviceValue &val) {
    auto it = global_ids.find(slot_name);
    if (it != global_ids.end()) return it->second;
    std::uint16_t id = static_cast<std::uint16_t>(global_values.size());
    global_ids.emplace(slot_name, id);
    global_names.push_back(slot_name);
    global_values.push_back(val);
    return id;
  }

  bool resolve_plain_global(const std::string &name, std::uint16_t &out_id) {
    const Value *gv = find_global(name);
    if (!gv) return fail("Global '%s' not found.", name.c_str());
    DeviceValue dv;
    if (!host_to_device_value(*gv, dv))
      return fail("Global '%s' is not a device-safe value (nil/bool/number).",
                  name.c_str());
    out_id = intern_global(name, dv);
    return true;
  }

  bool resolve_class_member(const std::string &cls, const std::string &member,
                            std::uint16_t &out_id) {
    std::string slot = cls + "." + member;
    auto it = global_ids.find(slot);
    if (it != global_ids.end()) {
      out_id = it->second;
      return true;
    }
    const Value *gv = find_global(cls);
    if (!gv) return fail("Class instance '%s' not found.", cls.c_str());
    if (gv->type != ValueType::INSTANCE)
      return fail("'%s' is not a class instance.", cls.c_str());
    Instance *inst = gv->as.instance;
    auto fit = inst->fields.find(member);
    if (fit == inst->fields.end())
      return fail("Class '%s' has no member '%s'.", cls.c_str(),
                  member.c_str());
    DeviceValue dv;
    if (!host_to_device_value(fit->second, dv))
      return fail("Class member '%s.%s' is not a device-safe value.",
                  cls.c_str(), member.c_str());
    out_id = intern_global(slot, dv);
    return true;
  }

  // Visit a function name: ensure it is enrolled in func_ids, return its id.
  bool enroll(const std::string &name, std::uint32_t &out_id) {
    auto it = func_ids.find(name);
    if (it != func_ids.end()) {
      out_id = it->second;
      return true;
    }
    if (!find_function(name))
      return fail("Function '%s' is not defined.", name.c_str());
    std::uint32_t id = static_cast<std::uint32_t>(func_order.size());
    func_ids.emplace(name, id);
    func_order.push_back(name);
    out_id = id;
    return true;
  }

  // ---- Per-function rewrite pass ----
  //
  // Translates one host Chunk into a PackedFn. Folds CALL/GET_GLOBAL/
  // GET_GLOBAL+GET_PROPERTY operands, rejects banned opcodes, builds the
  // per-function constant pool (numeric/bool/nil only), and remaps jump
  // offsets through an old->new offset table.
  bool rewrite(const Function &fn, PackedFn &out_fn,
               std::deque<std::string> &worklist) {
    using OC = DeviceOpCode;
    const Chunk &c = fn.chunk;
    const std::vector<std::uint8_t> &code = c.code;
    const std::vector<Value> &consts = c.constants;

    // -- Pass A: scan opcode by opcode, compute new offsets, validate.
    // We record old_offset -> new_offset at instruction boundaries.
    std::vector<std::uint32_t> old_to_new(code.size() + 1, 0);
    // For each opcode in source order, also record the "decision" we made
    // so Pass B can emit without redoing all the lookups.
    enum class Action : std::uint8_t {
      EMIT_TRIVIAL,   // 1 byte opcode, no operand
      EMIT_CONST,     // CONSTANT with new local idx
      EMIT_LOCAL,     // GET_LOCAL/SET_LOCAL, copy slot byte
      EMIT_GLOBAL_ID, // GET_GLOBAL plain → GET_GLOBAL_ID
      FOLD_CLASSMEMBER, // GET_GLOBAL + GET_PROPERTY fold; this entry is the
                        // first instruction; the second is SKIPPED.
      SKIP,             // operand-only marker (the GET_PROPERTY half of a fold)
      EMIT_CALL_ID,
      EMIT_JUMP,
      EMIT_LOOP,
    };
    struct Step {
      std::uint32_t old_ip;
      Action action;
      DeviceOpCode out_op;        // for trivial / local / jump
      std::uint16_t operand16;    // global id, function id, or raw
      std::uint8_t operand_extra; // argc, local slot, new const idx
      std::uint32_t old_target;   // for JUMP/LOOP (absolute old target)
    };
    std::vector<Step> steps;
    steps.reserve(code.size());

    // Per-function constant pool (rebuilt with only numeric/bool/nil) and a
    // map from old constant idx to new idx.
    std::unordered_map<int, std::uint8_t> const_remap;

    auto get_or_add_const = [&](int old_idx, std::uint8_t &new_idx) -> bool {
      auto it = const_remap.find(old_idx);
      if (it != const_remap.end()) { new_idx = it->second; return true; }
      if (old_idx < 0 || static_cast<size_t>(old_idx) >= consts.size())
        return fail("Bad constant index %d in function '%s'.", old_idx,
                    fn.name.c_str());
      DeviceValue dv;
      if (!host_to_device_value(consts[old_idx], dv))
        return fail("Function '%s' references a non-device-safe constant.",
                    fn.name.c_str());
      if (out_fn.constants.size() >= 255)
        return fail("Function '%s' exceeds 255 device-safe constants.",
                    fn.name.c_str());
      new_idx = static_cast<std::uint8_t>(out_fn.constants.size());
      out_fn.constants.push_back(dv);
      const_remap.emplace(old_idx, new_idx);
      return true;
    };

    std::uint32_t new_offset = 0;
    std::size_t i = 0;
    while (i < code.size()) {
      old_to_new[i] = new_offset;
      OpCode op = static_cast<OpCode>(code[i]);
      int opsize = 1 + host_operand_size(op);
      if (i + static_cast<size_t>(opsize) > code.size())
        return fail("Truncated opcode in function '%s'.", fn.name.c_str());

      Step s{};
      s.old_ip = static_cast<std::uint32_t>(i);

      // Default mapping: host opcode -> device opcode for trivially-mapped
      // ops; banned ops produce a clear error below.
      auto map_simple = [&](DeviceOpCode dop) {
        s.action = Action::EMIT_TRIVIAL;
        s.out_op = dop;
      };

      switch (op) {
      case OpCode::NIL: map_simple(OC::NIL); break;
      case OpCode::TRUE: map_simple(OC::TRUE); break;
      case OpCode::FALSE: map_simple(OC::FALSE); break;
      case OpCode::NEGATE: map_simple(OC::NEGATE); break;
      case OpCode::UPLUS: map_simple(OC::UPLUS); break;
      case OpCode::NOT: map_simple(OC::NOT); break;
      case OpCode::ADD: map_simple(OC::ADD); break;
      case OpCode::SUBTRACT: map_simple(OC::SUB); break;
      case OpCode::MULTIPLY: map_simple(OC::MUL); break;
      case OpCode::DIVIDE: map_simple(OC::DIV); break;
      case OpCode::INTDIVIDE: map_simple(OC::INTDIV); break;
      case OpCode::MOD: map_simple(OC::MOD); break;
      case OpCode::POW: map_simple(OC::POW); break;
      case OpCode::EQUAL: map_simple(OC::EQUAL); break;
      case OpCode::GREATER: map_simple(OC::GREATER); break;
      case OpCode::LESS: map_simple(OC::LESS); break;
      case OpCode::XOR: map_simple(OC::XOR); break;
      case OpCode::BOR: map_simple(OC::BOR); break;
      case OpCode::BAND: map_simple(OC::BAND); break;
      case OpCode::BNOT: map_simple(OC::BNOT); break;
      case OpCode::LSHIFT: map_simple(OC::LSHIFT); break;
      case OpCode::RSHIFT: map_simple(OC::RSHIFT); break;
      case OpCode::EXP: map_simple(OC::EXP); break;
      case OpCode::SIN: map_simple(OC::SIN); break;
      case OpCode::COS: map_simple(OC::COS); break;
      case OpCode::TAN: map_simple(OC::TAN); break;
      case OpCode::ABS: map_simple(OC::ABS); break;
      case OpCode::LOG: map_simple(OC::LOG); break;
      case OpCode::LOG10: map_simple(OC::LOG10); break;
      case OpCode::SIGN: map_simple(OC::SIGN); break;
      case OpCode::SQRT: map_simple(OC::SQRT); break;
      case OpCode::ACOS: map_simple(OC::ACOS); break;
      case OpCode::ASIN: map_simple(OC::ASIN); break;
      case OpCode::ATAN: map_simple(OC::ATAN); break;
      case OpCode::CEIL: map_simple(OC::CEIL); break;
      case OpCode::FLOOR: map_simple(OC::FLOOR); break;
      case OpCode::ATAN2: map_simple(OC::ATAN2); break;
      case OpCode::MIN: map_simple(OC::MIN); break;
      case OpCode::MAX: map_simple(OC::MAX); break;
      case OpCode::POP: map_simple(OC::POP); break;
      case OpCode::RETURN: map_simple(OC::RETURN); break;

      case OpCode::CONSTANT: {
        std::uint8_t new_idx = 0;
        if (!get_or_add_const(code[i + 1], new_idx)) return false;
        s.action = Action::EMIT_CONST;
        s.out_op = OC::CONSTANT;
        s.operand_extra = new_idx;
        break;
      }
      case OpCode::GET_LOCAL: {
        s.action = Action::EMIT_LOCAL;
        s.out_op = OC::GET_LOCAL;
        s.operand_extra = code[i + 1];
        break;
      }
      case OpCode::SET_LOCAL: {
        s.action = Action::EMIT_LOCAL;
        s.out_op = OC::SET_LOCAL;
        s.operand_extra = code[i + 1];
        break;
      }
      case OpCode::GET_GLOBAL: {
        const Value &nv = consts[code[i + 1]];
        if (nv.type != ValueType::STRING)
          return fail("GET_GLOBAL operand is not a string in '%s'.",
                      fn.name.c_str());
        std::string name = nv.as.string->str;
        // Look ahead: is the next op GET_PROPERTY?  If so, and the global
        // resolves to an INSTANCE, fold into a single class-member slot.
        bool folded = false;
        if (i + 2 < code.size() &&
            static_cast<OpCode>(code[i + 2]) == OpCode::GET_PROPERTY) {
          const Value *gv = find_global(name);
          if (gv && gv->type == ValueType::INSTANCE) {
            // peek member name from the GET_PROPERTY operand
            if (i + 3 >= code.size())
              return fail("Truncated GET_PROPERTY in '%s'.", fn.name.c_str());
            std::uint8_t mc = code[i + 3];
            if (mc >= consts.size() ||
                consts[mc].type != ValueType::STRING)
              return fail("GET_PROPERTY operand is not a string in '%s'.",
                          fn.name.c_str());
            std::string member = consts[mc].as.string->str;
            std::uint16_t gid = 0;
            if (!resolve_class_member(name, member, gid)) return false;
            s.action = Action::FOLD_CLASSMEMBER;
            s.out_op = OC::GET_GLOBAL_ID;
            s.operand16 = gid;
            folded = true;
          }
        }
        if (!folded) {
          std::uint16_t gid = 0;
          if (!resolve_plain_global(name, gid)) return false;
          s.action = Action::EMIT_GLOBAL_ID;
          s.out_op = OC::GET_GLOBAL_ID;
          s.operand16 = gid;
        }
        break;
      }
      case OpCode::CALL: {
        std::uint8_t name_idx = code[i + 1];
        std::uint8_t argc = code[i + 2];
        if (name_idx >= consts.size() ||
            consts[name_idx].type != ValueType::STRING)
          return fail("CALL operand is not a string in '%s'.",
                      fn.name.c_str());
        std::string callee = consts[name_idx].as.string->str;
        std::uint32_t fid = 0;
        if (!enroll(callee, fid)) return false;
        if (fid > 0xFFFF) return fail("Too many functions (>65535).");
        // Always enqueue; the outer pack() loop dedups via packed[fid].code.
        worklist.push_back(callee);
        s.action = Action::EMIT_CALL_ID;
        s.out_op = OC::CALL_ID;
        s.operand16 = static_cast<std::uint16_t>(fid);
        s.operand_extra = argc;
        break;
      }
      case OpCode::JUMP_IF_FALSE:
      case OpCode::JUMP: {
        std::uint16_t off = static_cast<std::uint16_t>(
            (code[i + 1] << 8) | code[i + 2]);
        s.action = Action::EMIT_JUMP;
        s.out_op = (op == OpCode::JUMP) ? OC::JUMP : OC::JUMP_IF_FALSE;
        s.old_target =
            static_cast<std::uint32_t>(i) + 3 + static_cast<std::uint32_t>(off);
        break;
      }
      case OpCode::LOOP: {
        std::uint16_t off = static_cast<std::uint16_t>(
            (code[i + 1] << 8) | code[i + 2]);
        s.action = Action::EMIT_LOOP;
        s.out_op = OC::LOOP;
        s.old_target =
            static_cast<std::uint32_t>(i) + 3 - static_cast<std::uint32_t>(off);
        break;
      }

      // ---- Banned ops -----------------------------------------------------
      case OpCode::DEFINE_GLOBAL:
      case OpCode::SET_GLOBAL:
        return fail("Function '%s' writes a global; globals are read-only "
                    "on the device.",
                    fn.name.c_str());
      case OpCode::GET_PROPERTY:
        // Only allowed as the tail of a GET_GLOBAL+GET_PROPERTY fold; if we
        // see one here unfolded, the receiver wasn't a class-instance global.
        return fail("Function '%s' uses GET_PROPERTY outside of a "
                    "ClassName.member read; per-instance properties are not "
                    "supported on the device.",
                    fn.name.c_str());
      case OpCode::SET_PROPERTY:
      case OpCode::NEW_INSTANCE:
      case OpCode::CALL_METHOD:
      case OpCode::GET_ATTR:
      case OpCode::SET_ATTR:
      case OpCode::HAS_ATTR:
      case OpCode::STR:
      case OpCode::BUILD_VECTOR:
      case OpCode::GET_INDEX:
      case OpCode::SET_INDEX:
      case OpCode::GET_SLICE:
      case OpCode::SET_SLICE:
      case OpCode::DUP:
      case OpCode::PRINT:
      case OpCode::ENV:
      case OpCode::LIST:
      case OpCode::LIST_GLOBALS:
      case OpCode::LIST_LOCALS:
      case OpCode::LIST_STACK:
      case OpCode::LIST_FUNC:
      case OpCode::NEWLINE:
      case OpCode::GET_OUTER:
      case OpCode::SET_OUTER:
        return fail("Function '%s' uses opcode %d which is not supported on "
                    "the device.",
                    fn.name.c_str(), static_cast<int>(op));
      }

      // Output size for this step.
      std::uint32_t out_size = 0;
      switch (s.action) {
      case Action::EMIT_TRIVIAL: out_size = 1; break;
      case Action::EMIT_CONST: out_size = 2; break;
      case Action::EMIT_LOCAL: out_size = 2; break;
      case Action::EMIT_GLOBAL_ID:
      case Action::FOLD_CLASSMEMBER:
        out_size = 3; break;
      case Action::EMIT_CALL_ID: out_size = 4; break;
      case Action::EMIT_JUMP:
      case Action::EMIT_LOOP: out_size = 3; break;
      case Action::SKIP: out_size = 0; break;
      }

      steps.push_back(s);
      new_offset += out_size;
      i += opsize;

      // If we folded, mark the following GET_PROPERTY as SKIP. Its offset
      // still needs to map to *something* sensible for any jump that may
      // (in theory) target it -- map it to the position immediately after
      // the folded instruction.
      if (s.action == Action::FOLD_CLASSMEMBER) {
        std::uint32_t skipped_old_ip = static_cast<std::uint32_t>(i);
        old_to_new[skipped_old_ip] = new_offset;
        Step skip{};
        skip.old_ip = skipped_old_ip;
        skip.action = Action::SKIP;
        steps.push_back(skip);
        i += 2; // GET_PROPERTY + operand byte
      }
    }
    old_to_new[code.size()] = new_offset;

    // -- Pass B: emit bytes, resolving jumps via old_to_new.
    out_fn.code.reserve(new_offset);
    for (const Step &s : steps) {
      switch (s.action) {
      case Action::SKIP: break;
      case Action::EMIT_TRIVIAL:
        out_fn.code.push_back(static_cast<std::uint8_t>(s.out_op));
        break;
      case Action::EMIT_CONST:
        out_fn.code.push_back(static_cast<std::uint8_t>(s.out_op));
        out_fn.code.push_back(s.operand_extra);
        break;
      case Action::EMIT_LOCAL:
        out_fn.code.push_back(static_cast<std::uint8_t>(s.out_op));
        out_fn.code.push_back(s.operand_extra);
        break;
      case Action::EMIT_GLOBAL_ID:
      case Action::FOLD_CLASSMEMBER:
        out_fn.code.push_back(static_cast<std::uint8_t>(s.out_op));
        out_fn.code.push_back(static_cast<std::uint8_t>(s.operand16 >> 8));
        out_fn.code.push_back(static_cast<std::uint8_t>(s.operand16 & 0xFF));
        break;
      case Action::EMIT_CALL_ID:
        out_fn.code.push_back(static_cast<std::uint8_t>(s.out_op));
        out_fn.code.push_back(static_cast<std::uint8_t>(s.operand16 >> 8));
        out_fn.code.push_back(static_cast<std::uint8_t>(s.operand16 & 0xFF));
        out_fn.code.push_back(s.operand_extra);
        break;
      case Action::EMIT_JUMP: {
        std::uint32_t new_ip = old_to_new[s.old_ip];
        if (s.old_target > code.size())
          return fail("Jump target out of range in '%s'.", fn.name.c_str());
        std::uint32_t new_target = old_to_new[s.old_target];
        if (new_target < new_ip + 3)
          return fail("Forward jump went backwards after rewrite in '%s'.",
                      fn.name.c_str());
        std::uint32_t off = new_target - (new_ip + 3);
        if (off > 0xFFFF)
          return fail("Jump offset >65535 in '%s'.", fn.name.c_str());
        out_fn.code.push_back(static_cast<std::uint8_t>(s.out_op));
        out_fn.code.push_back(static_cast<std::uint8_t>(off >> 8));
        out_fn.code.push_back(static_cast<std::uint8_t>(off & 0xFF));
        break;
      }
      case Action::EMIT_LOOP: {
        std::uint32_t new_ip = old_to_new[s.old_ip];
        if (s.old_target > code.size())
          return fail("Loop target out of range in '%s'.", fn.name.c_str());
        std::uint32_t new_target = old_to_new[s.old_target];
        if (new_ip + 3 < new_target)
          return fail("Backward loop went forwards after rewrite in '%s'.",
                      fn.name.c_str());
        std::uint32_t off = (new_ip + 3) - new_target;
        if (off > 0xFFFF)
          return fail("Loop offset >65535 in '%s'.", fn.name.c_str());
        out_fn.code.push_back(static_cast<std::uint8_t>(s.out_op));
        out_fn.code.push_back(static_cast<std::uint8_t>(off >> 8));
        out_fn.code.push_back(static_cast<std::uint8_t>(off & 0xFF));
        break;
      }
      }
    }

    out_fn.arity = static_cast<std::uint8_t>(fn.arity);
    // Conservative max_stack estimate: scan the rewritten bytecode and
    // track stack delta. This is the same heuristic used in many small
    // VMs and is good enough for v1.
    int cur = static_cast<int>(fn.arity);
    int hi = cur;
    std::size_t pi = 0;
    while (pi < out_fn.code.size()) {
      DeviceOpCode dop = static_cast<DeviceOpCode>(out_fn.code[pi]);
      int delta = 0;
      int sz = 1;
      switch (dop) {
      case DeviceOpCode::CONSTANT: delta = +1; sz = 2; break;
      case DeviceOpCode::NIL: case DeviceOpCode::TRUE:
      case DeviceOpCode::FALSE: delta = +1; break;
      case DeviceOpCode::NEGATE: case DeviceOpCode::UPLUS:
      case DeviceOpCode::NOT: case DeviceOpCode::BNOT:
      case DeviceOpCode::EXP: case DeviceOpCode::SIN:
      case DeviceOpCode::COS: case DeviceOpCode::TAN:
      case DeviceOpCode::ABS: case DeviceOpCode::LOG:
      case DeviceOpCode::LOG10: case DeviceOpCode::SIGN:
      case DeviceOpCode::SQRT: case DeviceOpCode::ACOS:
      case DeviceOpCode::ASIN: case DeviceOpCode::ATAN:
      case DeviceOpCode::CEIL: case DeviceOpCode::FLOOR:
        delta = 0; break;
      case DeviceOpCode::ADD: case DeviceOpCode::SUB:
      case DeviceOpCode::MUL: case DeviceOpCode::DIV:
      case DeviceOpCode::INTDIV: case DeviceOpCode::MOD:
      case DeviceOpCode::POW: case DeviceOpCode::EQUAL:
      case DeviceOpCode::GREATER: case DeviceOpCode::LESS:
      case DeviceOpCode::XOR: case DeviceOpCode::BOR:
      case DeviceOpCode::BAND: case DeviceOpCode::LSHIFT:
      case DeviceOpCode::RSHIFT: case DeviceOpCode::ATAN2:
      case DeviceOpCode::MIN: case DeviceOpCode::MAX:
        delta = -1; break;
      case DeviceOpCode::POP: delta = -1; break;
      case DeviceOpCode::GET_LOCAL: delta = +1; sz = 2; break;
      case DeviceOpCode::SET_LOCAL: delta = 0; sz = 2; break;
      case DeviceOpCode::GET_GLOBAL_ID: delta = +1; sz = 3; break;
      case DeviceOpCode::JUMP_IF_FALSE: delta = 0; sz = 3; break;
      case DeviceOpCode::JUMP: delta = 0; sz = 3; break;
      case DeviceOpCode::LOOP: delta = 0; sz = 3; break;
      case DeviceOpCode::CALL_ID: {
        // Pops argc args, pushes one result.
        std::uint8_t argc = out_fn.code[pi + 3];
        delta = -static_cast<int>(argc) + 1;
        sz = 4;
        break;
      }
      case DeviceOpCode::RETURN: delta = -1; break;
      }
      cur += delta;
      if (cur > hi) hi = cur;
      if (cur < 0) cur = 0; // be permissive on the heuristic
      pi += sz;
    }
    if (hi > 255) hi = 255;
    out_fn.max_stack = static_cast<std::uint8_t>(hi);
    return true;
  }

  // ---- Driver ----
  bool pack(const std::string &entry, std::uint32_t &out_entry_id) {
    if (!find_function(entry))
      return fail("Entry function '%s' is not defined.", entry.c_str());
    std::uint32_t entry_id = 0;
    if (!enroll(entry, entry_id)) return false;
    out_entry_id = entry_id;

    std::deque<std::string> worklist;
    worklist.push_back(entry);
    while (!worklist.empty()) {
      std::string name = worklist.front();
      worklist.pop_front();
      auto fit = func_ids.find(name);
      if (fit == func_ids.end()) continue;
      std::uint32_t id = fit->second;
      if (id < packed.size() && !packed[id].code.empty()) continue;
      if (id >= packed.size()) packed.resize(id + 1);
      const Function *fn = find_function(name);
      if (!fn) return fail("Function '%s' is not defined.", name.c_str());
      PackedFn pf;
      if (!rewrite(*fn, pf, worklist)) return false;
      if (pf.code.empty()) {
        // empty function body shouldn't happen, but guard against it
        pf.code.push_back(static_cast<std::uint8_t>(DeviceOpCode::NIL));
        pf.code.push_back(static_cast<std::uint8_t>(DeviceOpCode::RETURN));
      }
      packed[id] = std::move(pf);
    }

    // Any function that was enrolled but never rewritten (because no caller
    // actually reached it) should not exist; the worklist seeded by enroll()
    // ensures every enrolled function is rewritten. Sanity check:
    for (std::size_t k = 0; k < packed.size(); ++k) {
      if (packed[k].code.empty())
        return fail("Internal: function '%s' enrolled but not packed.",
                    func_order[k].c_str());
    }
    return true;
  }

  void emit_into(DeviceModuleStorage &out) {
    out.code.clear();
    out.constants.clear();
    out.functions.clear();
    out.function_names.clear();
    out.globals = global_values;
    out.global_names = global_names;

    out.functions.resize(packed.size());
    out.function_names = func_order;
    for (std::size_t i = 0; i < packed.size(); ++i) {
      DeviceFunction df{};
      df.code_offset = static_cast<std::uint32_t>(out.code.size());
      df.code_size = static_cast<std::uint32_t>(packed[i].code.size());
      df.const_offset = static_cast<std::uint32_t>(out.constants.size());
      df.const_count = static_cast<std::uint16_t>(packed[i].constants.size());
      df.arity = packed[i].arity;
      df.max_stack = packed[i].max_stack;
      out.functions[i] = df;
      out.code.insert(out.code.end(), packed[i].code.begin(),
                      packed[i].code.end());
      out.constants.insert(out.constants.end(), packed[i].constants.begin(),
                           packed[i].constants.end());
    }
  }
};

} // namespace detail

} // namespace device
} // namespace pips

// ---------------------------------------------------------------------------
// Implementations that need to see the full `VM` type. They are defined here
// in the device_pack.hpp header so users only have to include this header
// (and naturally vm.hpp must be visible first, since pack_function takes a
// const VM&). We use the function-template / inline approach to keep the
// header-only structure.
// ---------------------------------------------------------------------------

#include "../vm.hpp"

namespace pips {
namespace device {

namespace detail {

inline const Value *Packer::find_global(const std::string &name) const {
  auto it = vm.globals.find(name);
  if (it == vm.globals.end()) return nullptr;
  return &it->second;
}
inline const Function *Packer::find_function(const std::string &name) const {
  auto it = vm.functions.find(name);
  if (it == vm.functions.end()) return nullptr;
  return &it->second;
}
inline const std::unordered_map<std::string, ClassDef> &
Packer::class_table() const {
  return vm.classes;
}

} // namespace detail

inline bool pack_function(const VM &vm, const std::string &entry,
                          DeviceModuleStorage &out,
                          std::uint32_t &out_entry_id,
                          std::string &out_error) {
  detail::Packer p(vm);
  if (!p.pack(entry, out_entry_id)) {
    out_error = p.error;
    return false;
  }
  p.emit_into(out);
  return true;
}

} // namespace device
} // namespace pips

#endif // PIPS_DEVICE_DEVICE_PACK_HPP_
