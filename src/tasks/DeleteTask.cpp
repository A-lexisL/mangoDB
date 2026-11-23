#include <exception>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

#include "../db/Database.h"
#include "../db/Table.h"
#include "../query/Query.h"
#include "../query/QueryResult.h"
#include "../query/data/DeleteQuery.h"
#include "../utils/formatter.h"
#include "../utils/uexception.h"
#include "ApplyTask.h"
#include "ScanTask.h"
#include "Task.h"
#include "TaskResult.h"
#include "WholeTask.h"

namespace {

class DeleteKeyTask final : public WholeTask {
public:
  using WholeTask::WholeTask;

  auto execute() -> TaskResult::Ptr override {
    constexpr const char *qname = "DELETE";
    const auto &parameters = dynamic_cast<const DeleteQuery &>(getQuery());
    const auto &targetTable = getQuery().getTargetTable();
    try {
      auto &table = Database::getInstance().getTable(targetTable);
      const std::unique_lock lock(table.mutex());
      const bool matches = parameters.getCondition().empty() ||
                           evalConditionAtKey(table, parameters);
      const int affectedRows =
          matches && table.eraseByKey(*parameters.getKeyCondition()) ? 1 : 0;
      return std::make_unique<RecordCountTaskResult>(affectedRows);
    } catch (const TableNameNotFound &) {
      return std::make_unique<ErrorTaskResult>(qname, targetTable,
                                               "No such table.");
    } catch (const std::exception &error) {
      return std::make_unique<ErrorTaskResult>(
          qname, targetTable, "Unkonwn error '?'."_f % error.what());
    }
  }
};

class DeleteCollectTask final : public ScanTask {
  std::vector<Table::KeyType> keys;

public:
  using ScanTask::ScanTask;

protected:
  void processRow(const Table::Object &row) override {
    keys.push_back(row.key());
  }
  auto finishPartial() -> TaskResult::Ptr override {
    return std::make_unique<KeysTaskResult>(std::move(keys));
  }
};

class DeleteApplyTask final : public ApplyTask {
public:
  using ApplyTask::ApplyTask;

protected:
  auto applyKey(Table &table, const Table::KeyType &key) -> bool override {
    return table.eraseByKey(key);
  }
};

}  // namespace

auto DeleteQuery::split(const SplitConfig &config) -> TaskPlan {
  const auto &operands = getOperands();
  const auto &targetTable = getTargetTable();
  constexpr const char *qname = "DELETE";
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
      plan.tasks.push_back(std::make_unique<DeleteKeyTask>(*this));
      return plan;
    }
    plan = makeRangePlan(
        *this, table, config,
        [&](Task::Range range) -> std::unique_ptr<DeleteCollectTask> {
          return std::make_unique<DeleteCollectTask>(*this, range, qname);
        });
    plan.finalTask = std::make_unique<DeleteApplyTask>(*this, qname);
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
