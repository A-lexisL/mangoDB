#include "Task.h"

#include <algorithm>

#include "../db/Table.h"
#include "../query/Query.h"

auto Task::evalConditionAtKey(Table &table, const ComplexQuery &parameters)
    -> bool {
  const auto *keyCondition = parameters.getKeyCondition();
  if (keyCondition == nullptr) {
    return false;
  }
  const auto row = table.findRow(*keyCondition);
  return row && evalCondition(*row, parameters);
}

auto Task::evalCondition(const Table::Object &object,
                         const ComplexQuery &parameters) -> bool {
  const auto &resolvedConditions = parameters.getCondition();
  return std::ranges::all_of(
      resolvedConditions, [&object](const QueryCondition &cond) -> bool {
        return cond.comp(object.get(cond.fieldId), cond.valueParsed);
      });
}
