//
// Created by liu on 18-10-25.
//

#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../db/Database.h"
#include "../db/Table.h"
#include "../query/Query.h"
#include "../query/QueryResult.h"
#include "../query/data/UpdateQuery.h"
#include "../utils/formatter.h"
#include "../utils/uexception.h"
#include "ApplyTask.h"
#include "ScanTask.h"
#include "Task.h"
#include "TaskResult.h"
#include "WholeTask.h"

namespace {

// Caller holds the table's unique lock for the check and index update.
auto updateKey(const Table &table, const Table::Object::Ptr &row,
               const Table::KeyType &newKey) -> bool {
  if (!row) {
    return false;
  }
  // Keep the existing affected-row count for an unchanged key.
  if (row->key() == newKey) {
    return true;
  }
  if (table.ifKeyExist(newKey)) {
    return false;
  }
  row->setKey(newKey);
  return true;
}

class UpdateKeyTask final : public WholeTask {
public:
  using WholeTask::WholeTask;

  auto execute() -> TaskResult::Ptr override {
    constexpr const char *qname = "UPDATE";
    const auto &parameters = dynamic_cast<const UpdateQuery &>(getQuery());
    const auto &targetTable = getQuery().getTargetTable();
    try {
      auto &table = Database::getInstance().getTable(targetTable);
      const std::unique_lock lock(table.mutex());
      const auto row = table.findRow(*parameters.getKeyCondition());
      const int affectedRows =
          row && evalCondition(*row, parameters) &&
                  updateKey(table, row, parameters.getOperands().at(1))
              ? 1
              : 0;
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

class UpdateKeyCollectTask final : public ScanTask {
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

class UpdateKeyApplyTask final : public ApplyTask {
  Table::KeyType newKey;

public:
  explicit UpdateKeyApplyTask(const UpdateQuery &query)
      : ApplyTask(query, "UPDATE"), newKey(query.getOperands().at(1)) {}

protected:
  auto applyKey(Table &table, const Table::KeyType &key) -> bool override {
    return updateKey(table, table.findRow(key), newKey);
  }
};

auto makeKeyUpdatePlan(const UpdateQuery &query, Table &table,
                       const SplitConfig &config) -> TaskPlan {
  if (query.getKeyCondition() != nullptr) {
    TaskPlan plan;
    plan.tasks.push_back(std::make_unique<UpdateKeyTask>(query));
    return plan;
  }
  auto plan = makeRangePlan(
      query, table, config,
      [&](Task::Range range) -> std::unique_ptr<UpdateKeyCollectTask> {
        return std::make_unique<UpdateKeyCollectTask>(query, range, "UPDATE");
      });
  plan.finalTask = std::make_unique<UpdateKeyApplyTask>(query);
  return plan;
}

struct UpdateScanParameters {
  Table::FieldIndex field;
  Table::ValueType value;
};

class UpdateScanTask final : public ScanTask {
  UpdateScanParameters settings;
  int affectedRows = 0;

public:
  UpdateScanTask(const UpdateQuery &query, Range range,
                 UpdateScanParameters settings)
      : ScanTask(query, range, "UPDATE"), settings(settings) {}

protected:
  void processRow(const Table::Object &row) override {
    row.get(settings.field) = settings.value;
    ++affectedRows;
  }
  auto finishPartial() -> TaskResult::Ptr override {
    return std::make_unique<RecordCountTaskResult>(affectedRows);
  }
};

}  // namespace

auto UpdateQuery::split(const SplitConfig &config) -> TaskPlan {
  const auto &operands = getOperands();
  const auto &targetTable = getTargetTable();
  constexpr const char *qname = "UPDATE";
  if (operands.size() != 2) {
    return TaskPlan::completed(std::make_unique<ErrorMsgResult>(
        qname, targetTable.c_str(),
        "Invalid number of operands (? operands)."_f % operands.size()));
  }
  try {
    auto &table = Database::getInstance().getTable(targetTable);
    const std::shared_lock lock(table.mutex());
    if (!initCondition(table)) {
      return TaskPlan::completed(std::make_unique<RecordCountResult>(0));
    }
    if (operands.at(0) == "KEY") {
      return makeKeyUpdatePlan(*this, table, config);
    }
    constexpr int kDecimalBase = 10;
    const auto field = table.getFieldIndex(operands.at(0));
    const auto value = static_cast<Table::ValueType>(
        std::strtol(operands.at(1).c_str(), nullptr, kDecimalBase));
    return makeRangePlan(
        *this, table, config,
        [&](Task::Range range) -> std::unique_ptr<UpdateScanTask> {
          return std::make_unique<UpdateScanTask>(
              *this, range,
              UpdateScanParameters{.field = field, .value = value});
        });
  } catch (const TableNameNotFound &e) {
    return TaskPlan::completed(
        std::make_unique<ErrorMsgResult>(qname, targetTable, "No such table."));
  } catch (const IllFormedQueryCondition &e) {
    return TaskPlan::completed(
        std::make_unique<ErrorMsgResult>(qname, targetTable, e.what()));
  } catch (const std::invalid_argument &e) {
    // Cannot convert operand to string
    return TaskPlan::completed(std::make_unique<ErrorMsgResult>(
        qname, targetTable, "Unknown error '?'"_f % e.what()));
  } catch (const std::exception &e) {
    return TaskPlan::completed(std::make_unique<ErrorMsgResult>(
        qname, targetTable, "Unkonwn error '?'."_f % e.what()));
  }
}
