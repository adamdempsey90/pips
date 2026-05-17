#ifndef PIPS_OBJECT_HPP_
#define PIPS_OBJECT_HPP_

#include "value.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pips {

struct ClassDef {
  std::string name;
  std::vector<std::string> fields;
  std::unordered_set<std::string> methods;
  bool hasCtor = false;
};

struct Instance {
  ClassDef *classDef = nullptr;
  std::unordered_map<std::string, Value> fields;
};

} // namespace pips

#endif // PIPS_OBJECT_HPP_
