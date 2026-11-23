#include "../query/Query.h"
#include "../query/management/CopyTableQuery.h"
#include "Task.h"
#include "TaskResult.h"
#include "WholeTask.h"

#include <exception>
#include <memory>
#include <utility>

#include "../db/Database.h"
#include "../db/Table.h"
#include "../utils/uexception.h"

namespace {

class CopyTableWholeTask final : public WholeTask {
  static constexpr const char *qname = "COPYTABLE";

public:
  explicit CopyTableWholeTask(const CopyTableQuery &query) : WholeTask(query) {}

  auto execute() -> TaskResult::Ptr override;
};

auto CopyTableWholeTask::execute() -> TaskResult::Ptr {
  const auto &parameters = dynamic_cast<const CopyTableQuery &>(getQuery());
  const auto &targetTable = getQuery().getTargetTable();
  const auto &original = parameters.getOriginal();
  Database &database = Database::getInstance();
  try {
    database.testDuplicate(targetTable);
  } catch (const DuplicatedTableName &e) {
    return std::make_unique<ErrorTaskResult>(qname, e.what());
  }
  try {
    auto table =
        std::make_unique<Table>(targetTable, database.getTable(original));
    database.registerTable(std::move(table));
    auto result = std::make_unique<SuccessTaskResult>();
    result->setTableChange(getQuery().tableChange());
    return result;
  } catch (const TableNameNotFound &e) {
    return std::make_unique<ErrorTaskResult>(qname, original, "No such table.");
  } catch (const std::exception &e) {
    return std::make_unique<ErrorTaskResult>(qname, e.what());
  }
}

}  // namespace

auto CopyTableQuery::split(const SplitConfig & /*config*/) -> TaskPlan {
  TaskPlan plan;
  plan.tasks.push_back(std::make_unique<CopyTableWholeTask>(*this));
  return plan;
}
