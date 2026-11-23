#include "ScanTask.h"

#include <exception>
#include <memory>
#include <shared_mutex>
#include <string>
#include <utility>

#include "../db/Database.h"
#include "../db/Table.h"
#include "../query/Query.h"
#include "../utils/uexception.h"
#include "Task.h"
#include "TaskResult.h"

ScanTask::ScanTask(const ComplexQuery &query, Range range,
                   std::string queryName)
    : Task(query, std::move(range)), parameters(query),
      queryName(std::move(queryName)) {}

auto ScanTask::execute() -> TaskResult::Ptr {
  const auto &targetTable = getQuery().getTargetTable();
  try {
    auto &table = Database::getInstance().getTable(targetTable);
    // Numeric writers also require disjoint ranges and the parent Query's
    // exclusive reservation. This lock alone only stabilizes table structure.
    const std::shared_lock lock(table.mutex());
    forEachMatchingRow(
        table, parameters,
        [this](const Table::Object &row) -> void { processRow(row); });
    return finishPartial();
  } catch (const TableNameNotFound &) {
    return std::make_unique<ErrorTaskResult>(queryName, targetTable,
                                             "No such table.");
  } catch (const IllFormedQueryCondition &error) {
    return std::make_unique<ErrorTaskResult>(queryName, targetTable,
                                             error.what());
  } catch (const std::exception &error) {
    return std::make_unique<ErrorTaskResult>(queryName, targetTable,
                                             error.what());
  }
}
