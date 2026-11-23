//
// Created by liu on 18-10-25.
//

#include "../query/Query.h"
#include "../query/data/InsertQuery.h"
#include "Task.h"
#include "TaskResult.h"
#include "WholeTask.h"

#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../db/Database.h"
#include "../db/Table.h"
#include "../utils/formatter.h"
#include "../utils/uexception.h"

namespace {

class InsertWholeTask final : public WholeTask {
  static constexpr const char *qname = "INSERT";

public:
  explicit InsertWholeTask(const InsertQuery &query) : WholeTask(query) {}

  auto execute() -> TaskResult::Ptr override;
};

auto InsertWholeTask::execute() -> TaskResult::Ptr {
  const auto &parameters = dynamic_cast<const InsertQuery &>(getQuery());
  const auto &operands = parameters.getOperands();
  const auto &targetTable = getQuery().getTargetTable();
  constexpr int kDecimalBase = 10;
  if (operands.empty()) {
    return std::make_unique<ErrorTaskResult>(qname, targetTable.c_str(),
                                             "No operand (0 operands).");
  }
  Database &database = Database::getInstance();
  try {
    auto &table = database.getTable(targetTable);
    const auto &key = operands.front();
    std::vector<Table::ValueType> data;
    data.reserve(operands.size() - 1);
    for (auto iterator = ++operands.begin(); iterator != operands.end();
         ++iterator) {
      data.emplace_back(std::strtol(iterator->c_str(), nullptr, kDecimalBase));
    }
    const std::unique_lock lock(table.mutex());
    table.insertByIndex(key, std::move(data));
    return std::make_unique<SuccessTaskResult>();
  } catch (const TableNameNotFound &e) {
    return std::make_unique<ErrorTaskResult>(qname, targetTable,
                                             "No such table.");
  } catch (const IllFormedQueryCondition &e) {
    return std::make_unique<ErrorTaskResult>(qname, targetTable, e.what());
  } catch (const std::invalid_argument &e) {
    // Cannot convert operand to string
    return std::make_unique<ErrorTaskResult>(qname, targetTable,
                                             "Unknown error '?'"_f % e.what());
  } catch (const std::exception &e) {
    return std::make_unique<ErrorTaskResult>(qname, targetTable,
                                             "Unkonwn error '?'."_f % e.what());
  }
}

}  // namespace

auto InsertQuery::split(const SplitConfig & /*config*/) -> TaskPlan {
  TaskPlan plan;
  plan.tasks.push_back(std::make_unique<InsertWholeTask>(*this));
  return plan;
}
