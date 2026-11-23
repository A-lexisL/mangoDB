//
// Created by liu on 18-10-25.
//

#include "../query/Query.h"
#include "../query/management/DropTableQuery.h"
#include "Task.h"
#include "TaskResult.h"
#include "WholeTask.h"

#include <exception>
#include <memory>

#include "../db/Database.h"
#include "../utils/uexception.h"

namespace {

class DropTableWholeTask final : public WholeTask {
  static constexpr const char *qname = "DROP";

public:
  explicit DropTableWholeTask(const DropTableQuery &query) : WholeTask(query) {}

  auto execute() -> TaskResult::Ptr override;
};

auto DropTableWholeTask::execute() -> TaskResult::Ptr {
  const auto &targetTable = getQuery().getTargetTable();
  Database &database = Database::getInstance();
  try {
    database.dropTable(targetTable);
    auto result = std::make_unique<SuccessTaskResult>();
    result->setTableChange(getQuery().tableChange());
    return result;
  } catch (const TableNameNotFound &e) {
    return std::make_unique<ErrorTaskResult>(qname, targetTable,
                                             "No such table.");
  } catch (const std::exception &e) {
    return std::make_unique<ErrorTaskResult>(qname, e.what());
  }
}

}  // namespace

auto DropTableQuery::split(const SplitConfig & /*config*/) -> TaskPlan {
  TaskPlan plan;
  plan.tasks.push_back(std::make_unique<DropTableWholeTask>(*this));
  return plan;
}
