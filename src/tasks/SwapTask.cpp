#include <exception>
#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../db/Database.h"
#include "../db/Table.h"
#include "../query/Query.h"
#include "../query/QueryResult.h"
#include "../query/data/SwapQuery.h"
#include "../utils/formatter.h"
#include "../utils/uexception.h"
#include "ScanTask.h"
#include "Task.h"
#include "TaskResult.h"

namespace {

class SwapScanTask final : public ScanTask {
  Table::FieldIndex firstField;
  Table::FieldIndex secondField;
  int affectedRows = 0;

public:
  SwapScanTask(const SwapQuery &query, Range range,
               std::pair<Table::FieldIndex, Table::FieldIndex> fields)
      : ScanTask(query, range, "SWAP"), firstField(fields.first),
        secondField(fields.second) {}

protected:
  void processRow(const Table::Object &row) override {
    if (firstField != secondField) {
      std::swap(row.get(firstField), row.get(secondField));
    }
    ++affectedRows;
  }
  auto finishPartial() -> TaskResult::Ptr override {
    return std::make_unique<RecordCountTaskResult>(affectedRows);
  }
};

}  // namespace

auto SwapQuery::split(const SplitConfig &config) -> TaskPlan {
  const auto &operands = getOperands();
  const auto &targetTable = getTargetTable();
  constexpr const char *qname = "SWAP";
  if (operands.size() != 2) {
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
    const auto firstField = table.getFieldIndex(operands.at(0));
    const auto secondField = table.getFieldIndex(operands.at(1));
    return makeRangePlan(
        *this, table, config,
        [&](Task::Range range) -> std::unique_ptr<SwapScanTask> {
          return std::make_unique<SwapScanTask>(
              *this, range, std::pair{firstField, secondField});
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
