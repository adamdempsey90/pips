#include "pips/vm.hpp"

#include <cstdio>
#include <initializer_list>
#include <string>

namespace {

using namespace pips;

int failures = 0;

void expect(bool condition, const char *name) {
  if (condition) {
    std::printf("PASS %s\n", name);
  } else {
    std::printf("FAIL %s\n", name);
    ++failures;
  }
}

const pips::Value &global(const pips::VM &vm, const char *name) {
  return vm.globals.at(name);
}

bool is_number(const pips::Value &value, double expected) {
  return IS_NUMBER(value) && static_cast<double>(AS_NUMBER(value)) == expected;
}

bool is_string(const pips::Value &value, const char *expected) {
  return IS_STRING(value) && AS_STD_STRING(value) == expected;
}

bool string_vector(const pips::Value &value,
                   std::initializer_list<const char *> expected) {
  if (!IS_VECTOR(value)) return false;
  const auto &elements = AS_VECTOR(value)->elements;
  if (elements.size() != expected.size()) return false;
  std::size_t i = 0;
  for (const char *item : expected) {
    if (!is_string(elements[i++], item)) return false;
  }
  return true;
}

void test_vector_methods() {
  pips::VM vm;
  const char *source =
      "var v = [1, \"two\"];\n"
      "var alias = v;\n"
      "var pushed = v.push(true);\n"
      "var after_push = alias.len();\n"
      "var removed = v.pop();\n"
      "var after_pop = alias.size();\n"
      "var literal_len = ([1, 2] + [3, 4]).len();\n"
      "var joined = [1, \"two\", true, nil].join(\" | \" );\n"
      "var empty_join = [].join(\",\");\n"
      "var compact_join = [1, 2, 3].join(\"\");\n"
      "class Counter { fn len() { return 99; } fn push(x) { return x; } "
      "fn join(s) { return s; } }\n"
      "var counter = new Counter { };\n"
      "var class_len = counter.len();\n"
      "fn class_push() { var c = new Counter { }; return c.push(7); }\n"
      "var class_push_result = class_push();\n"
      "var class_join = counter.join(\"class\");\n";
  expect(vm.interpret(source) == pips::InterpretResult::OK,
         "vector.interpret");
  expect(IS_NIL(global(vm, "pushed")), "vector.push_returns_nil");
  expect(is_number(global(vm, "after_push"), 3), "vector.push_mutates_alias");
  expect(IS_BOOL(global(vm, "removed")) && AS_BOOL(global(vm, "removed")),
         "vector.pop_returns_value");
  expect(is_number(global(vm, "after_pop"), 2), "vector.pop_mutates_alias");
  expect(is_number(global(vm, "literal_len"), 2),
         "vector.arbitrary_expression_len");
    expect(is_string(global(vm, "joined"), "1 | two | true | nil"),
      "vector.join_mixed_values");
    expect(is_string(global(vm, "empty_join"), ""), "vector.join_empty");
    expect(is_string(global(vm, "compact_join"), "123"),
      "vector.join_empty_separator");
  expect(is_number(global(vm, "class_len"), 99),
         "vector.class_method_collision");
    expect(is_number(global(vm, "class_push_result"), 7),
      "vector.mutator_name_class_method");
    expect(is_string(global(vm, "class_join"), "class"),
      "vector.join_name_class_method");
}

void test_string_methods() {
  pips::VM vm;
  const char *source =
      "var s = \"  AbA  \";\n"
      "var n = s.len();\n"
      "var n2 = s.size();\n"
      "var lower = s.lower();\n"
      "var upper = s.upper();\n"
      "var stripped = s.strip();\n"
      "var left = \"xyvalueyx\".lstrip(\"xy\");\n"
      "var right = \"xyvalueyx\".rstrip(\"xy\");\n"
      "var starts = s.starts_with(\"  A\");\n"
      "var ends = s.ends_with(\"  \" );\n"
      "var has = s.contains(\"Ab\");\n"
      "var replaced = \"one two one\".replace(\"one\", \"1\");\n"
      "var words = \"  one\t two  three \".split();\n"
      "var fields = \"a,,b,\".split(\",\");\n";
  expect(vm.interpret(source) == pips::InterpretResult::OK,
         "string.interpret");
  expect(is_number(global(vm, "n"), 7) && is_number(global(vm, "n2"), 7),
         "string.length_aliases");
  expect(is_string(global(vm, "lower"), "  aba  "), "string.lower");
  expect(is_string(global(vm, "upper"), "  ABA  "), "string.upper");
  expect(is_string(global(vm, "stripped"), "AbA"), "string.strip");
  expect(is_string(global(vm, "left"), "valueyx"), "string.lstrip_chars");
  expect(is_string(global(vm, "right"), "xyvalue"), "string.rstrip_chars");
  expect(IS_BOOL(global(vm, "starts")) && AS_BOOL(global(vm, "starts")) &&
             IS_BOOL(global(vm, "ends")) && AS_BOOL(global(vm, "ends")) &&
             IS_BOOL(global(vm, "has")) && AS_BOOL(global(vm, "has")),
         "string.predicates");
  expect(is_string(global(vm, "replaced"), "1 two 1"), "string.replace");
  expect(string_vector(global(vm, "words"), {"one", "two", "three"}),
         "string.split_whitespace");
  expect(string_vector(global(vm, "fields"), {"a", "", "b", ""}),
         "string.split_separator");
}

void test_method_errors() {
  {
    pips::VM vm;
    expect(vm.interpret("var v = []; v.pop();") ==
               pips::InterpretResult::RUNTIME_ERROR,
           "errors.empty_pop");
  }
  {
    pips::VM vm;
    expect(vm.interpret("var v = []; v.push();") ==
               pips::InterpretResult::RUNTIME_ERROR,
           "errors.push_arity");
  }
  {
    pips::VM vm;
    expect(vm.interpret("[1, 2].join(1);") ==
               pips::InterpretResult::RUNTIME_ERROR,
           "errors.join_separator_type");
  }
  {
    pips::VM vm;
    expect(vm.interpret("\"a\".replace(\"\", \"x\");") ==
               pips::InterpretResult::RUNTIME_ERROR,
           "errors.empty_replace_search");
  }
  {
    pips::VM vm;
    expect(vm.interpret("\"a\".split(\"\");") ==
               pips::InterpretResult::RUNTIME_ERROR,
           "errors.empty_split_separator");
  }
  {
    pips::VM vm;
    expect(vm.interpret("(1).len();") == pips::InterpretResult::RUNTIME_ERROR,
           "errors.unsupported_receiver");
  }
}

} // namespace

int main() {
  test_vector_methods();
  test_string_methods();
  test_method_errors();
  std::printf("---\n");
  if (failures == 0) {
    std::printf("PASS value_methods\n");
    return 0;
  }
  std::printf("FAIL value_methods (%d failures)\n", failures);
  return 1;
}
