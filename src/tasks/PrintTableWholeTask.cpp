//
// Created by liu on 18-10-25.
//

#include "../query/Query.h"
#include "../query/management/PrintTableQuery.h"
#include "Task.h"
#include "TaskResult.h"
#include "WholeTask.h"

#include <memory>
#include <sstream>

#include "../db/Database.h"
#include "../db/Table.h"
#include "../utils/uexception.h"

namespace {

class PrintTableWholeTask final : public WholeTask {
  static constexpr const char *qname = "SHOWTABLE";

public:
  explicit PrintTableWholeTask(const PrintTableQuery &query)
      : WholeTask(query) {}

  auto execute() -> TaskResult::Ptr override;
};

auto PrintTableWholeTask::execute() -> TaskResult::Ptr {
  const auto &targetTable = getQuery().getTargetTable();
  const Database &database = Database::getInstance();
  try {
    const auto &table = database.getTable(targetTable);
    std::ostringstream output;
    output << "================\nTABLE = " << table << "================\n\n";
    return std::make_unique<DisplayTaskResult>(output.str());
  } catch (const TableNameNotFound &e) {
    return std::make_unique<ErrorTaskResult>(qname, targetTable,
                                             "No such table.");
  }
}

}  // namespace

auto PrintTableQuery::split(const SplitConfig & /*config*/) -> TaskPlan {
  TaskPlan plan;
  plan.tasks.push_back(std::make_unique<PrintTableWholeTask>(*this));
  return plan;
}
