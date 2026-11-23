#include <algorithm>
#include <cstddef>
#include <exception>
#include <iterator>
#include <memory>
#include <shared_mutex>
#include <utility>
#include <vector>

#include "../db/Database.h"
#include "../db/Table.h"
#include "../query/Query.h"
#include "../query/QueryResult.h"
#include "../query/ResultRow.h"
#include "../query/data/SelectQuery.h"
#include "../utils/uexception.h"
#include "ScanTask.h"
#include "Task.h"
#include "TaskResult.h"

namespace {

class SelectScanTask final : public ScanTask {
  std::vector<Table::FieldIndex> fields;
  std::vector<ResultRow> rows;

public:
  SelectScanTask(const SelectQuery &query, Range range,
                 std::vector<Table::FieldIndex> fields)
      : ScanTask(query, range, "SELECT"), fields(std::move(fields)) {}

protected:
  void processRow(const Table::Object &row) override {
    std::vector<int> values;
    values.reserve(fields.size());
    std::ranges::transform(
        fields, std::back_inserter(values),
        [&row](const auto field) -> auto { return row.get(field); });
    rows.push_back({.key = row.key(), .values = std::move(values)});
  }
  auto finishPartial() -> TaskResult::Ptr override {
    return std::make_unique<SelectTaskResult>(std::move(rows));
  }
};

}  // namespace

auto SelectQuery::split(const SplitConfig &config) -> TaskPlan {
  const auto &operands = getOperands();
  const auto &targetTable = getTargetTable();
  constexpr const char *qname = "SELECT";
  if (operands.empty() || operands.front() != "KEY") {
    return TaskPlan::completed(std::make_unique<ErrorMsgResult>(
        qname, targetTable, "SELECT requires KEY as its first field."));
  }
  try {
    auto &table = Database::getInstance().getTable(targetTable);
    const std::shared_lock lock(table.mutex());
    if (!initCondition(table)) {
      for (std::size_t i = 1; i < operands.size(); ++i) {
        table.getFieldIndex(operands.at(i));
      }
      return TaskPlan::completed(std::make_unique<SelectQueryResult>(
          std::vector<SelectQueryResult::Row>{}));
    }
    std::vector<Table::FieldIndex> fields;
    fields.reserve(operands.size() - 1);
    for (std::size_t i = 1; i < operands.size(); ++i) {
      fields.push_back(table.getFieldIndex(operands.at(i)));
    }
    return makeRangePlan(
        *this, table, config,
        [&](Task::Range range) -> std::unique_ptr<SelectScanTask> {
          return std::make_unique<SelectScanTask>(*this, range, fields);
        });
  } catch (const TableNameNotFound &) {
    return TaskPlan::completed(
        std::make_unique<ErrorMsgResult>(qname, targetTable, "No such table."));
  } catch (const IllFormedQueryCondition &error) {
    return TaskPlan::completed(
        std::make_unique<ErrorMsgResult>(qname, targetTable, error.what()));
  } catch (const std::exception &error) {
    return TaskPlan::completed(
        std::make_unique<ErrorMsgResult>(qname, targetTable, error.what()));
  }
}
