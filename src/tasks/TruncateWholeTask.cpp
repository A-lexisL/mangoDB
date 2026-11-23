#include "../query/Query.h"
#include "../query/management/TruncateQuery.h"
#include "Task.h"
#include "TaskResult.h"
#include "WholeTask.h"

#include <exception>
#include <memory>
#include <mutex>

#include "../db/Database.h"
#include "../db/Table.h"
#include "../utils/uexception.h"

namespace {

class TruncateWholeTask final : public WholeTask {
  static constexpr const char *qname = "TRUNCATE";

public:
  explicit TruncateWholeTask(const TruncateQuery &query) : WholeTask(query) {}

  auto execute() -> TaskResult::Ptr override;
};

auto TruncateWholeTask::execute() -> TaskResult::Ptr {
  const auto &targetTable = getQuery().getTargetTable();
  Database &database = Database::getInstance();
  try {
    auto &table = database.getTable(targetTable);
    const std::unique_lock lock(table.mutex());
    table.clear();
    return std::make_unique<SuccessTaskResult>();
  } catch (const TableNameNotFound &e) {
    return std::make_unique<ErrorTaskResult>(qname, targetTable,
                                             "No such table.");
  } catch (const std::exception &e) {
    return std::make_unique<ErrorTaskResult>(qname, e.what());
  }
}

}  // namespace

auto TruncateQuery::split(const SplitConfig & /*config*/) -> TaskPlan {
  TaskPlan plan;
  plan.tasks.push_back(std::make_unique<TruncateWholeTask>(*this));
  return plan;
}
