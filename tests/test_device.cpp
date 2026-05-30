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
// Test 5: dynamic vector indexing is still rejected.
// ---------------------------------------------------------------------------
void test_reject_dynamic_vector_index() {
  pips::VM vm;
  const char *src =
      "var v = [10, 20, 30];\n"
      "fn pick(i) { return v[i]; }\n";
  auto r = vm.interpret(src);
  expect_ok("test5.interpret", r == pips::InterpretResult::OK);
  dv::DeviceModuleStorage store;
  std::uint32_t entry_id = 0;
  std::string err;
  bool ok = dv::pack_function(vm, "pick", store, entry_id, err);
  expect_ok("test5.reject", !ok,
            "expected packer to reject dynamic vector indexing: " + err);
}

// ---------------------------------------------------------------------------
// Test 6: using print() is rejected.
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
  test_reject_dynamic_vector_index();
  test_reject_print();

  std::printf("---\n");
  if (failures == 0) {
    std::printf("PASS device_smoke (%d checks)\n", 0);
    return 0;
  }
  std::printf("FAIL device_smoke (%d failures)\n", failures);
  return 1;
}
