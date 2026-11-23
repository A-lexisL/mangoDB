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
#include "../query/data/SumQuery.h"
#include "../utils/uexception.h"
#include "ScanTask.h"
#include "Task.h"
#include "TaskResult.h"

namespace {

class SumScanTask final : public ScanTask {
  std::vector<Table::FieldIndex> fields;
  std::vector<int> sums;

public:
  SumScanTask(const SumQuery &query, Range range,
              std::vector<Table::FieldIndex> fields)
      : ScanTask(query, range, "SUM"), fields(std::move(fields)),
        sums(this->fields.size(), 0) {}

protected:
  void processRow(const Table::Object &row) override {
    for (std::size_t i = 0; i < fields.size(); ++i) {
      sums.at(i) += row.get(fields.at(i));
    }
  }
  auto finishPartial() -> TaskResult::Ptr override {
    return std::make_unique<SumTaskResult>(std::move(sums));
  }
};

}  // namespace

auto SumQuery::split(const SplitConfig &config) -> TaskPlan {
  const auto &operands = getOperands();
  const auto &targetTable = getTargetTable();
  constexpr const char *qname = "SUM";
  if (operands.empty()) {
    return TaskPlan::completed(std::make_unique<ErrorMsgResult>(
        qname, targetTable, "SUM requires at least one numeric field."));
  }
  try {
    auto &table = Database::getInstance().getTable(targetTable);
    const std::shared_lock lock(table.mutex());
    if (!initCondition(table)) {
      for (const auto &field : operands) {
        table.getFieldIndex(field);
      }
      return TaskPlan::completed(std::make_unique<AggregateQueryResult>(
          std::vector<int>(operands.size(), 0)));
    }
    std::vector<Table::FieldIndex> fields;
    fields.reserve(operands.size());
    std::ranges::transform(operands, std::back_inserter(fields),
                           [&table](const auto &field) -> Table::FieldIndex {
                             return table.getFieldIndex(field);
                           });
    return makeRangePlan(
        *this, table, config,
        [&](Task::Range range) -> std::unique_ptr<SumScanTask> {
          return std::make_unique<SumScanTask>(*this, range, fields);
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
