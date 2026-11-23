#include "../tasks/Task.h"
#include "QueryResult.h"
//
// Created by liu on 18-10-25.
//

#include "Query.h"

#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "../db/Table.h"
#include "../utils/formatter.h"
#include "../utils/uexception.h"

auto ComplexQuery::initCondition(const Table &table) -> bool {
  constexpr int kDecimalBase = 10;
  const std::unordered_map<std::string, int> opmap{
      {">", '>'}, {"<", '<'}, {"=", '='}, {">=", 'g'}, {"<=", 'l'},
  };
  for (auto cond_it = condition.begin(); cond_it != condition.end();) {
    if (cond_it->field == "KEY") {
      if (cond_it->op != "=") {
        throw IllFormedQueryCondition("Can only compare equivalence on KEY");
      }
      if (!keyCondition) {
        keyCondition = std::make_unique<std::string>(cond_it->value);
      } else if (*keyCondition != cond_it->value) {
        return false;
      }
      cond_it = condition.erase(cond_it);
    } else {
      cond_it->fieldId = table.getFieldIndex(cond_it->field);
      cond_it->valueParsed = static_cast<Table::ValueType>(
          std::strtol(cond_it->value.c_str(), nullptr, kDecimalBase));
      int operation = 0;
      try {
        operation = opmap.at(cond_it->op);
      } catch (const std::out_of_range &e) {
        throw IllFormedQueryCondition(
            R"("?" is not a valid condition operator.)"_f % cond_it->op);
      }
      switch (operation) {
      case '>':
        cond_it->comp = std::greater<>();
        break;
      case '<':
        cond_it->comp = std::less<>();
        break;
      case '=':
        cond_it->comp = std::equal_to<>();
        break;
      case 'g':
        cond_it->comp = std::greater_equal<>();
        break;
      case 'l':
        cond_it->comp = std::less_equal<>();
        break;
      default:
        throw std::logic_error("Unknown condition operator");
      }
      ++cond_it;
    }
  }
  return true;
}

auto NopQuery::split(const SplitConfig & /*config*/) -> TaskPlan {
  return TaskPlan::completed(std::make_unique<NullQueryResult>());
}
