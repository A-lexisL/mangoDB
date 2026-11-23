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
#include "../query/data/MaxQuery.h"
#include "../utils/uexception.h"
#include "ScanTask.h"
#include "Task.h"
#include "TaskResult.h"

namespace {

class MaxScanTask final : public ScanTask {
  std::vector<Table::FieldIndex> fields;
  std::vector<int> maxima;
  bool found = false;

public:
  MaxScanTask(const MaxQuery &query, Range range,
              std::vector<Table::FieldIndex> fields)
      : ScanTask(query, range, "MAX"), fields(std::move(fields)),
        maxima(this->fields.size()) {}

protected:
  void processRow(const Table::Object &row) override {
    for (std::size_t i = 0; i < fields.size(); ++i) {
      const auto value = row.get(fields.at(i));
      maxima.at(i) = found ? std::max(maxima.at(i), value) : value;
    }
    found = true;
  }
  auto finishPartial() -> TaskResult::Ptr override {
    if (!found) {
      return std::make_unique<MaxTaskResult>();
    }
    return std::make_unique<MaxTaskResult>(std::move(maxima));
  }
};

}  // namespace

auto MaxQuery::split(const SplitConfig &config) -> TaskPlan {
  const auto &operands = getOperands();
  const auto &targetTable = getTargetTable();
  constexpr const char *qname = "MAX";
  if (operands.empty()) {
    return TaskPlan::completed(std::make_unique<ErrorMsgResult>(
        qname, targetTable, "MAX requires at least one numeric field."));
  }
  try {
    auto &table = Database::getInstance().getTable(targetTable);
    const std::shared_lock lock(table.mutex());
    if (!initCondition(table)) {
      for (const auto &field : operands) {
        table.getFieldIndex(field);
      }
      return TaskPlan::completed(std::make_unique<NullQueryResult>());
    }
    std::vector<Table::FieldIndex> fields;
    fields.reserve(operands.size());
    std::ranges::transform(operands, std::back_inserter(fields),
                           [&table](const auto &field) -> Table::FieldIndex {
                             return table.getFieldIndex(field);
                           });
    return makeRangePlan(
        *this, table, config,
        [&](Task::Range range) -> std::unique_ptr<MaxScanTask> {
          return std::make_unique<MaxScanTask>(*this, range, fields);
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
