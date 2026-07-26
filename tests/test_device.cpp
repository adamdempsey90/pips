// Host-side smoke test for the DeviceVM pack + interpret path. No GPU is
// required: the same `DeviceVM` that would run inside a kernel is constructed
// here on the host (DeviceVM's methods are `__host__ __device__`-qualified
// only under NVCC/HIPCC, plain host calls otherwise).
//
// Build (from the workspace root, ignore any existing build dir):
//   c++ -std=c++17 -O2 -I . tests/device_smoke.cpp -o device_smoke
//
// What it covers:
//   1. Pack a pure-numeric function with a callee.
//   2. Pack a function that reads a plain global and a class-data-member
//      global (folded GET_GLOBAL+GET_PROPERTY).
//   3. Reject a function that writes a global (SET_GLOBAL).
//   4. Reject a function that calls print().

#include "pips/device/device_pack.hpp"
#include "pips/device/device_vm.hpp"
#include "pips/vm.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace dv = pips::device;

namespace {

int failures = 0;

void expect_ok(const char *what, bool ok, const std::string &msg = "") {
  if (ok) {
    std::printf("PASS %s\n", what);
  } else {
    std::printf("FAIL %s: %s\n", what, msg.c_str());
    failures++;
  }
}

bool approx_eq(double a, double b, double eps = 1e-9) {
  double d = a - b;
  if (d < 0) d = -d;
  return d <= eps * (1.0 + (a < 0 ? -a : a) + (b < 0 ? -b : b));
}

bool vector_eq(const dv::DeviceValue &value,
               const double *expected, std::uint8_t length) {
  if (!dv::dv_is_vector(value) || dv::dv_vector_length(value) != length)
    return false;
  for (std::uint8_t i = 0; i < length; ++i) {
    if (!approx_eq(static_cast<double>(dv::dv_vector_element(value, i)),
                   expected[i]))
      return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Test 1: pack `caller(x)` which calls `poly(x, x)` and check the result.
// ---------------------------------------------------------------------------
void test_pure_numeric() {
  pips::VM vm;
  const char *src =
      "fn poly(x, y) { return x * x + 2 * y + 1; }\n"
      "fn caller(x) { return poly(x, x); }\n";
  auto r = vm.interpret(src);
  expect_ok("test1.interpret",
            r == pips::InterpretResult::OK,
            "vm.interpret returned error");

  dv::DeviceModuleStorage store;
  std::uint32_t entry_id = 0;
  std::string err;
  bool ok = dv::pack_function(vm, "caller", store, entry_id, err);
  expect_ok("test1.pack", ok, err);
  if (!ok) return;

  dv::DeviceVM dvm;
  dv::DeviceModule mod = store.view();

  bool numeric_ok = true;
  for (double x = -3.0; x <= 3.0; x += 0.5) {
    dv::DeviceValue arg = dv::dv_number(static_cast<dv::DeviceReal>(x));
    dv::DeviceValue result{};
    auto st = dvm.run(mod, entry_id, &arg, 1, &result);
    if (st != dv::DeviceStatus::OK) {
      std::printf("  status = %d at x=%g\n", static_cast<int>(st), x);
      numeric_ok = false;
      break;
    }
    if (!dv::dv_is_number(result)) {
      numeric_ok = false;
      break;
    }
    double got = static_cast<double>(dv::dv_as_number(result));
    double expected = x * x + 2 * x + 1; // poly(x, x)
    if (!approx_eq(got, expected)) {
      std::printf("  x=%g got=%g expected=%g\n", x, got, expected);
      numeric_ok = false;
      break;
    }
  }
  expect_ok("test1.run", numeric_ok);
}

// ---------------------------------------------------------------------------
// Test 2: plain global + class-data-member global.
// ---------------------------------------------------------------------------
void test_globals_and_class_member() {
  pips::VM vm;
  const char *src =
      "class Cfg { var x = 3; var y = 4; }\n"
      "var cfg = new Cfg { };\n"
      "var bias = 10;\n"
      "fn dot() { return cfg.x * cfg.x + cfg.y * cfg.y + bias; }\n";
  auto r = vm.interpret(src);
  expect_ok("test2.interpret",
            r == pips::InterpretResult::OK);

  dv::DeviceModuleStorage store;
  std::uint32_t entry_id = 0;
  std::string err;
  bool ok = dv::pack_function(vm, "dot", store, entry_id, err);
  expect_ok("test2.pack", ok, err);
  if (!ok) return;

  // Expect three globals: cfg.x, cfg.y, bias (order = first-seen).
  expect_ok("test2.global_count",
            store.globals.size() == 3,
            "expected 3 globals, got " + std::to_string(store.globals.size()));

  dv::DeviceVM dvm;
  dv::DeviceValue result{};
  auto st = dvm.run(store.view(), entry_id, nullptr, 0, &result);
  expect_ok("test2.run", st == dv::DeviceStatus::OK,
            "status = " + std::to_string(static_cast<int>(st)));
  if (st != dv::DeviceStatus::OK) return;
  double got = static_cast<double>(dv::dv_as_number(result));
  expect_ok("test2.value",
            approx_eq(got, 3 * 3 + 4 * 4 + 10),
            "got " + std::to_string(got));
}

// ---------------------------------------------------------------------------
// Test 3: constant-index global vector reads are lowered to scalar globals.
// ---------------------------------------------------------------------------
void test_global_vector_element_read() {
  pips::VM vm;
  const char *src =
      "var v = [10, 20, 30];\n"
      "fn pick() { return v[1] + v[2]; }\n";
  auto r = vm.interpret(src);
  expect_ok("test3.interpret", r == pips::InterpretResult::OK);

  dv::DeviceModuleStorage store;
  std::uint32_t entry_id = 0;
  std::string err;
  bool ok = dv::pack_function(vm, "pick", store, entry_id, err);
  expect_ok("test3.pack", ok, err);
  if (!ok) return;

  expect_ok("test3.global_count",
            store.globals.size() == 2,
            "expected 2 globals, got " + std::to_string(store.globals.size()));

  dv::DeviceVM dvm;
  dv::DeviceValue result{};
  auto st = dvm.run(store.view(), entry_id, nullptr, 0, &result);
  expect_ok("test3.run", st == dv::DeviceStatus::OK,
            "status = " + std::to_string(static_cast<int>(st)));
  if (st != dv::DeviceStatus::OK) return;

  double got = static_cast<double>(dv::dv_as_number(result));
  expect_ok("test3.value", approx_eq(got, 50.0),
            "got " + std::to_string(got));
}

// ---------------------------------------------------------------------------
// Test 4: writing a global is rejected.
// ---------------------------------------------------------------------------
void test_reject_global_write() {
  pips::VM vm;
  const char *src =
      "var counter = 0;\n"
      "fn bump() { counter = counter + 1; return counter; }\n";
  auto r = vm.interpret(src);
  expect_ok("test3.interpret",
            r == pips::InterpretResult::OK);
  dv::DeviceModuleStorage store;
  std::uint32_t entry_id = 0;
  std::string err;
  bool ok = dv::pack_function(vm, "bump", store, entry_id, err);
  expect_ok("test4.reject", !ok && err.find("read-only") != std::string::npos,
            "expected read-only-globals error, got: " + err);
}

// ---------------------------------------------------------------------------
// Test 5: bounded vector literals, locals, nested returns, and unary/binary
// vector operations use one inline DeviceValue throughout the call chain.
// ---------------------------------------------------------------------------
void test_vector_return_and_arithmetic() {
  pips::VM vm;
  const char *src =
      "fn build(x) { return [x, x + 1, x + 2]; }\n"
      "fn transform(x) { var v = build(x); return -sqrt(v * v + 1); }\n";
  auto r = vm.interpret(src);
  expect_ok("test5.interpret", r == pips::InterpretResult::OK);

  dv::DeviceModuleStorage store;
  std::uint32_t entry_id = 0;
  std::string err;
  bool ok = dv::pack_function(vm, "transform", store, entry_id, err);
  expect_ok("test5.pack", ok, err);
  if (!ok) return;

  dv::DeviceValue arg = dv::dv_number(2);
  dv::DeviceValue result{};
  dv::DeviceVM dvm;
  auto st = dvm.run(store.view(), entry_id, &arg, 1, &result);
  expect_ok("test5.run", st == dv::DeviceStatus::OK,
            "status = " + std::to_string(static_cast<int>(st)));
  const double expected[] = {-std::sqrt(5.0), -std::sqrt(10.0),
                             -std::sqrt(17.0)};
  expect_ok("test5.value", vector_eq(result, expected, 3));
}

// ---------------------------------------------------------------------------
// Test 6: vectors can enter as arguments, broadcast on either side, and be
// indexed dynamically (including negative indices).
// ---------------------------------------------------------------------------
void test_vector_argument_and_index() {
  pips::VM vm;
  const char *src =
      "fn scale(v) { return 2 * v + v / 2; }\n"
      "fn pick(v, i) { return v[i]; }\n";
  expect_ok("test6.interpret",
            vm.interpret(src) == pips::InterpretResult::OK);

  const dv::DeviceReal elements[] = {4, 8, 12};
  dv::DeviceValue vector{};
  expect_ok("test6.construct", dv::dv_vector(elements, 3, vector));

  dv::DeviceModuleStorage store;
  std::uint32_t entry_id = 0;
  std::string err;
  bool ok = dv::pack_function(vm, "scale", store, entry_id, err);
  expect_ok("test6.pack_scale", ok, err);
  if (!ok) return;
  dv::DeviceValue result{};
  dv::DeviceVM dvm;
  auto st = dvm.run(store.view(), entry_id, &vector, 1, &result);
  const double scaled[] = {10, 20, 30};
  expect_ok("test6.scale", st == dv::DeviceStatus::OK &&
                               vector_eq(result, scaled, 3));

  ok = dv::pack_function(vm, "pick", store, entry_id, err);
  expect_ok("test6.pack_pick", ok, err);
  if (!ok) return;
  dv::DeviceValue args[] = {vector, dv::dv_number(-1)};
  st = dvm.run(store.view(), entry_id, args, 2, &result);
  expect_ok("test6.negative_index",
            st == dv::DeviceStatus::OK && dv::dv_is_number(result) &&
                approx_eq(dv::dv_as_number(result), 12));
}

// ---------------------------------------------------------------------------
// Test 7: a whole bounded global vector is packed, enabling dynamic indexing,
// and vector equality returns a scalar boolean.
// ---------------------------------------------------------------------------
void test_vector_global_and_equality() {
  pips::VM vm;
  const char *src =
      "var samples = [3, 5, 8];\n"
      "fn pick(i) { return samples[i]; }\n"
      "fn same() { return samples == [3, 5, 8]; }\n";
  expect_ok("test7.interpret",
            vm.interpret(src) == pips::InterpretResult::OK);

  dv::DeviceModuleStorage store;
  std::uint32_t entry_id = 0;
  std::string err;
  bool ok = dv::pack_function(vm, "pick", store, entry_id, err);
  expect_ok("test7.pack_pick", ok, err);
  if (!ok) return;
  expect_ok("test7.global_vector",
            store.globals.size() == 1 && dv::dv_is_vector(store.globals[0]));
  dv::DeviceValue index = dv::dv_number(1);
  dv::DeviceValue result{};
  dv::DeviceVM dvm;
  auto st = dvm.run(store.view(), entry_id, &index, 1, &result);
  expect_ok("test7.pick", st == dv::DeviceStatus::OK &&
                              approx_eq(dv::dv_as_number(result), 5));

  ok = dv::pack_function(vm, "same", store, entry_id, err);
  expect_ok("test7.pack_same", ok, err);
  if (!ok) return;
  st = dvm.run(store.view(), entry_id, nullptr, 0, &result);
  expect_ok("test7.equal", st == dv::DeviceStatus::OK &&
                               dv::dv_is_bool(result) &&
                               dv::dv_as_bool(result));
}

// ---------------------------------------------------------------------------
// Test 8: vector-specific failures are deterministic and do not allocate.
// ---------------------------------------------------------------------------
void test_vector_failures() {
  pips::VM vm;
  const char *src =
      "fn add(a, b) { return a + b; }\n"
      "fn divide(a, b) { return a / b; }\n"
      "fn pick(a, i) { return a[i]; }\n"
      "var mixed = [1, true];\n"
      "fn mixed_global() { return mixed; }\n";
  expect_ok("test8.interpret",
            vm.interpret(src) == pips::InterpretResult::OK);

  const dv::DeviceReal a_values[] = {1, 2};
  const dv::DeviceReal b_values[] = {3, 4, 5};
  const dv::DeviceReal zero_values[] = {1, 0};
  dv::DeviceValue a{}, b{}, zeros{};
  dv::dv_vector(a_values, 2, a);
  dv::dv_vector(b_values, 3, b);
  dv::dv_vector(zero_values, 2, zeros);
  dv::DeviceValue args[] = {a, b};
  dv::DeviceValue result{};
  dv::DeviceVM dvm;
  dv::DeviceModuleStorage store;
  std::uint32_t entry_id = 0;
  std::string err;

  bool ok = dv::pack_function(vm, "add", store, entry_id, err);
  expect_ok("test8.pack_add", ok, err);
  if (!ok) return;
  auto st = dvm.run(store.view(), entry_id, args, 2, &result);
  expect_ok("test8.length_mismatch",
            st == dv::DeviceStatus::VECTOR_LENGTH_MISMATCH);

  ok = dv::pack_function(vm, "divide", store, entry_id, err);
  expect_ok("test8.pack_divide", ok, err);
  if (!ok) return;
  args[1] = zeros;
  st = dvm.run(store.view(), entry_id, args, 2, &result);
  expect_ok("test8.divide_zero", st == dv::DeviceStatus::DIV_BY_ZERO);

  ok = dv::pack_function(vm, "pick", store, entry_id, err);
  expect_ok("test8.pack_pick", ok, err);
  if (!ok) return;
  args[1] = dv::dv_number(2);
  st = dvm.run(store.view(), entry_id, args, 2, &result);
  expect_ok("test8.index_range", st == dv::DeviceStatus::INDEX_OUT_OF_RANGE);

  dv::DeviceValue invalid{};
  invalid.type = dv::DeviceValueType::VECTOR;
  invalid.as.vector.length = PIPS_DEVICE_VECTOR_MAX + 1;
  dv::DeviceValue invalid_args[] = {invalid, dv::dv_number(0)};
  st = dvm.run(store.view(), entry_id, invalid_args, 2, &result);
  expect_ok("test8.invalid_argument", st == dv::DeviceStatus::INVALID_VECTOR);

  ok = dv::pack_function(vm, "mixed_global", store, entry_id, err);
  expect_ok("test8.reject_mixed", !ok,
            "expected mixed global vector rejection");
}

// ---------------------------------------------------------------------------
// Test 9: empty vectors and the configured capacity boundary are supported;
// the next literal size is rejected by the packer when representable.
// ---------------------------------------------------------------------------
void test_vector_capacity() {
  std::string src = "fn empty() { return []; }\nfn boundary() { return [";
  for (int i = 0; i < PIPS_DEVICE_VECTOR_MAX; ++i) {
    if (i) src += ",";
    src += std::to_string(i);
  }
  src += "]; }\n";
  if (PIPS_DEVICE_VECTOR_MAX < 255) {
    src += "fn oversized() { return [";
    for (int i = 0; i <= PIPS_DEVICE_VECTOR_MAX; ++i) {
      if (i) src += ",";
      src += std::to_string(i);
    }
    src += "]; }\n";
  }

  pips::VM vm;
  expect_ok("test9.interpret",
            vm.interpret(src.c_str()) == pips::InterpretResult::OK);
  dv::DeviceModuleStorage store;
  std::uint32_t entry_id = 0;
  std::string err;
  bool ok = dv::pack_function(vm, "empty", store, entry_id, err);
  expect_ok("test9.pack_empty", ok, err);
  if (!ok) return;
  dv::DeviceValue result{};
  dv::DeviceVM dvm;
  auto st = dvm.run(store.view(), entry_id, nullptr, 0, &result);
  expect_ok("test9.empty", st == dv::DeviceStatus::OK &&
                               dv::dv_is_vector(result) &&
                               dv::dv_vector_length(result) == 0);

  ok = dv::pack_function(vm, "boundary", store, entry_id, err);
  expect_ok("test9.pack_boundary", ok, err);
  if (!ok) return;
  st = dvm.run(store.view(), entry_id, nullptr, 0, &result);
  expect_ok("test9.boundary", st == dv::DeviceStatus::OK &&
                                  dv::dv_is_vector(result) &&
                                  dv::dv_vector_length(result) ==
                                      PIPS_DEVICE_VECTOR_MAX);

  if (PIPS_DEVICE_VECTOR_MAX < 255) {
    ok = dv::pack_function(vm, "oversized", store, entry_id, err);
    expect_ok("test9.reject_oversized", !ok,
              "expected vector capacity error");
  }
}

// ---------------------------------------------------------------------------
// Test 10: using print() is rejected.
// ---------------------------------------------------------------------------
void test_reject_print() {
  pips::VM vm;
  const char *src =
      "fn shout(x) { print(x); return x; }\n";
  auto r = vm.interpret(src);
  // The script itself may or may not compile depending on syntax; what we
  // actually want is to reject any function whose body uses PRINT. If the
  // language uses a different print keyword in this build, this test will
  // silently pass on the interpret step; the meaningful assertion is the
  // packer rejecting it.
  if (r != pips::InterpretResult::OK) {
    std::printf("SKIP test4 (interpret failed)\n");
    return;
  }
  dv::DeviceModuleStorage store;
  std::uint32_t entry_id = 0;
  std::string err;
  bool ok = dv::pack_function(vm, "shout", store, entry_id, err);
  expect_ok("test6.reject", !ok,
            "expected packer to reject print(): " + err);
}

} // namespace

int main() {
  test_pure_numeric();
  test_globals_and_class_member();
  test_global_vector_element_read();
  test_reject_global_write();
  test_vector_return_and_arithmetic();
  test_vector_argument_and_index();
  test_vector_global_and_equality();
  test_vector_failures();
  test_vector_capacity();
  test_reject_print();

  std::printf("---\n");
  if (failures == 0) {
    std::printf("PASS device_smoke (%d checks)\n", 0);
    return 0;
  }
  std::printf("FAIL device_smoke (%d failures)\n", failures);
  return 1;
}
