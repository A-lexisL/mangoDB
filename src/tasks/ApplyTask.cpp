#include "ApplyTask.h"

#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "../db/Database.h"
#include "../db/Table.h"
#include "../query/Query.h"
#include "../utils/formatter.h"
#include "../utils/uexception.h"
#include "Task.h"
#include "TaskResult.h"

ApplyTask::ApplyTask(const ComplexQuery &query, std::string queryName)
    : FinalTask(query), queryName(std::move(queryName)) {}

void ApplyTask::setInput(TaskResult::Ptr input) {
  keys = std::move(dynamic_cast<KeysTaskResult &>(*input).keys);
}

auto ApplyTask::execute() -> TaskResult::Ptr {
  const auto &targetTable = getQuery().getTargetTable();
  try {
    auto &table = Database::getInstance().getTable(targetTable);
    const std::unique_lock lock(table.mutex());
    int affectedRows = 0;
    for (const auto &key : keys) {
      if (applyKey(table, key)) {
        ++affectedRows;
      }
    }
    return std::make_unique<RecordCountTaskResult>(affectedRows);
  } catch (const TableNameNotFound &) {
    return std::make_unique<ErrorTaskResult>(queryName, targetTable,
                                             "No such table.");
  } catch (const IllFormedQueryCondition &error) {
    return std::make_unique<ErrorTaskResult>(queryName, targetTable,
                                             error.what());
  } catch (const std::exception &error) {
    return std::make_unique<ErrorTaskResult>(
        queryName, targetTable, "Unkonwn error '?'."_f % error.what());
  }
}
