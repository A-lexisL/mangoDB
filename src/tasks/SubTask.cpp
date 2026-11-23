#include <algorithm>
#include <cstddef>
#include <exception>
#include <iterator>
#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../db/Database.h"
#include "../db/Table.h"
#include "../query/Query.h"
#include "../query/QueryResult.h"
#include "../query/data/SubQuery.h"
#include "../utils/formatter.h"
#include "../utils/uexception.h"
#include "ScanTask.h"
#include "Task.h"
#include "TaskResult.h"

namespace {

class SubScanTask final : public ScanTask {
  std::vector<Table::FieldIndex> fields;
  int affectedRows = 0;

public:
  SubScanTask(const SubQuery &query, Range range,
              std::vector<Table::FieldIndex> fields)
      : ScanTask(query, range, "SUB"), fields(std::move(fields)) {}

protected:
  void processRow(const Table::Object &row) override {
    auto value = row.get(fields.front());
    for (std::size_t i = 1; i + 1 < fields.size(); ++i) {
      value -= row.get(fields.at(i));
    }
    row.get(fields.back()) = value;
    ++affectedRows;
  }
  auto finishPartial() -> TaskResult::Ptr override {
    return std::make_unique<RecordCountTaskResult>(affectedRows);
  }
};

}  // namespace

auto SubQuery::split(const SplitConfig &config) -> TaskPlan {
  const auto &operands = getOperands();
  const auto &targetTable = getTargetTable();
  constexpr const char *qname = "SUB";
  if (operands.size() < 2) {
    return TaskPlan::completed(std::make_unique<ErrorMsgResult>(
        qname, targetTable.c_str(),
        "Invalid number of operands (? operands)."_f % operands.size()));
  }
  try {
    auto &table = Database::getInstance().getTable(targetTable);
    const std::shared_lock lock(table.mutex());
    if (!initCondition(table)) {
      for (const auto &field : operands) {
        table.getFieldIndex(field);
      }
      return TaskPlan::completed(std::make_unique<RecordCountResult>(0));
    }
    std::vector<Table::FieldIndex> fields;
    fields.reserve(operands.size());
    std::ranges::transform(operands, std::back_inserter(fields),
                           [&table](const auto &field) -> Table::FieldIndex {
                             return table.getFieldIndex(field);
                           });
    return makeRangePlan(
        *this, table, config,
        [&](Task::Range range) -> std::unique_ptr<SubScanTask> {
          return std::make_unique<SubScanTask>(*this, range, fields);
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
