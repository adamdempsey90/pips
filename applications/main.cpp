// RK4 ODE solver that uses a pips script to define the system.
//
// The script must define:
//   var time_range = <vector>;    // time points to step through (>= 2 elements)
//   var dimension  = <number>;    // number of state variables
//   fn initial_condition()        // returns a vector of length `dimension`
//   fn rhs(t, y)                  // returns a vector of length `dimension`
//
// Output: CSV to stdout with columns  t, y0, y1, ...
//
// Usage:
//   ./applications/ode_solver <script.pips>

#include <pips/vm.hpp>
#include <pips/utils.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace pips;

// ---------------------------------------------------------------------------
// Helpers: convert between host std::vector<double> and pips VectorObject
// ---------------------------------------------------------------------------

static std::vector<double> fromPipsVec(const Value &v,
                                       const char *ctx) {
  if (!IS_VECTOR(v)) {
    std::fprintf(stderr, "error: %s must be a vector\n", ctx);
    std::exit(1);
  }
  VectorObject *vo = AS_VECTOR(v);
  std::vector<double> out;
  out.reserve(vo->elements.size());
  for (const auto &e : vo->elements) {
    if (!IS_NUMBER(e)) {
      std::fprintf(stderr, "error: %s contains a non-numeric element\n", ctx);
      std::exit(1);
    }
    out.push_back(static_cast<double>(AS_NUMBER(e)));
  }
  return out;
}

static Value toPipsVec(VM &vm, const std::vector<double> &v) {
  VectorObject *vo = vm.newVector();
  vo->elements.reserve(v.size());
  for (double x : v) {
    vo->elements.push_back(NUMBER_VAL(static_cast<Real>(x)));
  }
  return VECTOR_VAL(vo);
}

// ---------------------------------------------------------------------------
// Host-side vector arithmetic for the RK4 stages
// ---------------------------------------------------------------------------

static std::vector<double> vadd(const std::vector<double> &a,
                                const std::vector<double> &b) {
  std::vector<double> out(a.size());
  for (std::size_t i = 0; i < a.size(); ++i) out[i] = a[i] + b[i];
  return out;
}

static std::vector<double> vscale(double s, const std::vector<double> &a) {
  std::vector<double> out(a.size());
  for (std::size_t i = 0; i < a.size(); ++i) out[i] = s * a[i];
  return out;
}

// ---------------------------------------------------------------------------
// Call the pips 'rhs' function
// ---------------------------------------------------------------------------

static std::vector<double> callRhs(VM &vm, double t,
                                   const std::vector<double> &y) {
  Value tv  = NUMBER_VAL(static_cast<Real>(t));
  Value yv  = toPipsVec(vm, y);
  Value res = vm.call("rhs", {tv, yv});
  return fromPipsVec(res, "rhs() return value");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::fprintf(stderr, "Usage: ode_solver <script.pips>\n");
    return 1;
  }

  // ------------------------------------------------------------------
  // 1. Load and interpret the script.
  // ------------------------------------------------------------------
  VM vm;

  char *source = Utils::readFile(argv[1]);
  InterpretResult ir = vm.interpret(source);
  std::free(source);
  if (ir == InterpretResult::COMPILE_ERROR) {
    std::fprintf(stderr, "error: script failed to compile\n");
    return 65;
  }
  if (ir == InterpretResult::RUNTIME_ERROR) {
    std::fprintf(stderr, "error: script raised a runtime error\n");
    return 70;
  }

  // ------------------------------------------------------------------
  // 2. Extract time_range and dimension from globals.
  // ------------------------------------------------------------------
  auto it_tr = vm.globals.find(Utils::getKey("time_range"));
  if (it_tr == vm.globals.end()) {
    std::fprintf(stderr, "error: script must define 'time_range'\n");
    return 1;
  }
  std::vector<double> time_range = fromPipsVec(it_tr->second, "time_range");
  if (time_range.size() < 2) {
    std::fprintf(stderr,
                 "error: 'time_range' must contain at least 2 elements\n");
    return 1;
  }

  auto it_dim = vm.globals.find(Utils::getKey("dimension"));
  if (it_dim == vm.globals.end() || !IS_NUMBER(it_dim->second)) {
    std::fprintf(stderr,
                 "error: script must define 'dimension' as a number\n");
    return 1;
  }
  int dim = static_cast<int>(AS_NUMBER(it_dim->second));
  if (dim <= 0) {
    std::fprintf(stderr, "error: 'dimension' must be a positive integer\n");
    return 1;
  }

  // ------------------------------------------------------------------
  // 3. Get the initial condition.
  // ------------------------------------------------------------------
  Value ic = vm.call("initial_condition", {});
  std::vector<double> y = fromPipsVec(ic, "initial_condition() return value");
  if (static_cast<int>(y.size()) != dim) {
    std::fprintf(
        stderr,
        "error: initial_condition() returned %zu elements, expected %d\n",
        y.size(), static_cast<int>(dim));
    return 1;
  }

  // ------------------------------------------------------------------
  // 4. Print CSV header and initial state.
  // ------------------------------------------------------------------
  std::printf("t");
  for (int i = 0; i < dim; ++i) std::printf(",y%d", i);
  std::printf("\n");

  std::printf("%.16g", time_range[0]);
  for (double yi : y) std::printf(",%.16g", yi);
  std::printf("\n");

  // ------------------------------------------------------------------
  // 5. RK4 time loop.
  // ------------------------------------------------------------------
  for (std::size_t step = 0; step + 1 < time_range.size(); ++step) {
    double t      = time_range[step];
    double t_next = time_range[step + 1];
    double dt     = t_next - t;

    // k1 = rhs(t,        y)
    auto k1 = callRhs(vm, t,            y);
    // k2 = rhs(t + dt/2, y + (dt/2)*k1)
    auto k2 = callRhs(vm, t + 0.5 * dt, vadd(y, vscale(0.5 * dt, k1)));
    // k3 = rhs(t + dt/2, y + (dt/2)*k2)
    auto k3 = callRhs(vm, t + 0.5 * dt, vadd(y, vscale(0.5 * dt, k2)));
    // k4 = rhs(t + dt,   y + dt*k3)
    auto k4 = callRhs(vm, t_next,        vadd(y, vscale(dt, k3)));

    // y_new = y + (dt/6) * (k1 + 2*k2 + 2*k3 + k4)
    for (int i = 0; i < dim; ++i) {
      y[i] += (dt / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }

    std::printf("%.16g", t_next);
    for (double yi : y) std::printf(",%.16g", yi);
    std::printf("\n");
  }

  return 0;
}
