#include <exception>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

#include "../db/Database.h"
#include "../db/Table.h"
#include "../query/Query.h"
#include "../query/QueryResult.h"
#include "../query/data/DuplicateQuery.h"
#include "../utils/formatter.h"
#include "../utils/uexception.h"
#include "ApplyTask.h"
#include "ScanTask.h"
#include "Task.h"
#include "TaskResult.h"
#include "WholeTask.h"

namespace {

void duplicateRow(Table &table, const Table::Object &row,
                  const Table::KeyType &copyKey) {
  std::vector<Table::ValueType> datum;
  datum.reserve(table.field().size());
  for (Table::FieldIndex fieldIndex = 0; fieldIndex < table.field().size();
       ++fieldIndex) {
    datum.push_back(row.get(fieldIndex));
  }
  table.insertByIndex(copyKey, std::move(datum));
}

class DuplicateKeyTask final : public WholeTask {
public:
  using WholeTask::WholeTask;

  auto execute() -> TaskResult::Ptr override {
    constexpr const char *qname = "DUPLICATE";
    const auto &parameters = dynamic_cast<const DuplicateQuery &>(getQuery());
    const auto &targetTable = getQuery().getTargetTable();
    try {
      auto &table = Database::getInstance().getTable(targetTable);
      const std::unique_lock lock(table.mutex());
      const auto &key = *parameters.getKeyCondition();
      const auto row = table.findRow(key);
      if (!row || (!parameters.getCondition().empty() &&
                   !evalCondition(*row, parameters))) {
        return std::make_unique<RecordCountTaskResult>(0);
      }
      const auto copyKey = key + "_copy";
      if (table.ifKeyExist(copyKey)) {
        return std::make_unique<RecordCountTaskResult>(0);
      }
      duplicateRow(table, *row, copyKey);
      return std::make_unique<RecordCountTaskResult>(1);
    } catch (const TableNameNotFound &) {
      return std::make_unique<ErrorTaskResult>(qname, targetTable,
                                               "No such table.");
    } catch (const std::exception &error) {
      return std::make_unique<ErrorTaskResult>(
          qname, targetTable, "Unkonwn error '?'."_f % error.what());
    }
  }
};

class DuplicateCollectTask final : public ScanTask {
  const Table &table;
  std::vector<Table::KeyType> keys;

public:
  DuplicateCollectTask(const DuplicateQuery &query, Range range,
                       const Table &table)
      : ScanTask(query, range, "DUPLICATE"), table(table) {}

protected:
  void processRow(const Table::Object &row) override {
    if (!table.ifKeyExist(row.key() + "_copy")) {
      keys.push_back(row.key());
    }
  }
  auto finishPartial() -> TaskResult::Ptr override {
    return std::make_unique<KeysTaskResult>(std::move(keys));
  }
};

class DuplicateApplyTask final : public ApplyTask {
public:
  using ApplyTask::ApplyTask;

protected:
  auto applyKey(Table &table, const Table::KeyType &key) -> bool override {
    const auto row = table.findRow(key);
    duplicateRow(table, *row, key + "_copy");
    return true;
  }
};

}  // namespace

auto DuplicateQuery::split(const SplitConfig &config) -> TaskPlan {
  const auto &operands = getOperands();
  const auto &targetTable = getTargetTable();
  constexpr const char *qname = "DUPLICATE";
  if (!operands.empty()) {
    return TaskPlan::completed(std::make_unique<ErrorMsgResult>(
        qname, targetTable.c_str(),
        "Too much operand (? operands)."_f % operands.size()));
  }
  try {
    auto &table = Database::getInstance().getTable(targetTable);
    const std::shared_lock lock(table.mutex());
    if (!initCondition(table)) {
      return TaskPlan::completed(std::make_unique<RecordCountResult>(0));
    }
    TaskPlan plan;
    if (getKeyCondition() != nullptr) {
      plan.tasks.push_back(std::make_unique<DuplicateKeyTask>(*this));
      return plan;
    }
    plan = makeRangePlan(
        *this, table, config,
        [&](Task::Range range) -> std::unique_ptr<DuplicateCollectTask> {
          return std::make_unique<DuplicateCollectTask>(*this, range, table);
        });
    plan.finalTask = std::make_unique<DuplicateApplyTask>(*this, qname);
    return plan;
  } catch (const TableNameNotFound &e) {
    return TaskPlan::completed(
        std::make_unique<ErrorMsgResult>(qname, targetTable, "No such table."));
  } catch (const IllFormedQueryCondition &e) {
    return TaskPlan::completed(
        std::make_unique<ErrorMsgResult>(qname, targetTable, e.what()));
  } catch (const std::exception &e) {
    return TaskPlan::completed(std::make_unique<ErrorMsgResult>(
        qname, targetTable, "Unkonwn error '?'."_f % e.what()));
  }
}
