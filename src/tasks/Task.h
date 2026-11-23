#ifndef SRC_TASKS_TASK_H_
#define SRC_TASKS_TASK_H_

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "../db/Table.h"
#include "../query/Query.h"
#include "../query/QueryResult.h"
#include "TaskResult.h"

class Task {
  friend class QueryRunner;
  friend class ThreadPoolManager;

public:
  using Ptr = std::unique_ptr<Task>;
  using Range = std::pair<Table::Iterator, Table::Iterator>;
  explicit Task(const Query &query, Range range = {})
      : query(query), range(std::move(range)) {}
  Task(const Task &) = delete;
  Task(Task &&) = delete;
  auto operator=(const Task &) -> Task & = delete;
  auto operator=(Task &&) -> Task & = delete;
  virtual ~Task() = default;
  // Execute one unit of work; the runner attaches completion identifiers.
  virtual auto execute() -> TaskResult::Ptr = 0;
  [[nodiscard]] auto queryId() const -> Query::Id { return query.getId(); }

private:
  std::size_t taskId = 0;
  const Query &query;
  Range range;

protected:
  [[nodiscard]] auto getQuery() const -> const Query & { return query; }

  // Evaluate the parsed KEY lookup and the remaining resolved conditions.
  static auto evalConditionAtKey(Table &table, const ComplexQuery &parameters)
      -> bool;
  // Evaluate one row using the Query's prepared conditions.
  static auto evalCondition(const Table::Object &object,
                            const ComplexQuery &parameters) -> bool;
  // The action may update a row but must not add or remove rows.
  template <class Action>
  void forEachMatchingRow(Table &table, const ComplexQuery &parameters,
                          const Action &action) {
    const auto *keyCondition = parameters.getKeyCondition();
    if (keyCondition) {
      const auto row = table.findRow(*keyCondition);
      if (row && evalCondition(*row, parameters)) {
        action(*row);
      }
      return;
    }
    for (auto iterator = range.first; iterator != range.second; ++iterator) {
      const auto row = *iterator;
      if (evalCondition(row, parameters)) {
        action(row);
      }
    }
  }
};

class FinalTask : public Task {
public:
  using Task::Task;
  // Consume one combined normal-task result before submission to a worker.
  virtual void setInput(TaskResult::Ptr input) = 0;
};

// A completed result bypasses task submission; otherwise tasks are nonempty.
struct TaskPlan {
  std::vector<Task::Ptr> tasks;
  std::unique_ptr<FinalTask> finalTask;
  QueryResult::Ptr result;

  static auto completed(QueryResult::Ptr result) -> TaskPlan {
    return {.tasks = {}, .finalTask = {}, .result = std::move(result)};
  }
};

// Caller holds the table lock and the parent Query's resource reservation.
// KEY lookups and unsplit execution produce one Task, even on an empty table.
template <class Factory>
auto makeRangePlan(const ComplexQuery &query, Table &table,
                   const SplitConfig &config, const Factory &makeTask)
    -> TaskPlan {
  TaskPlan plan;
  auto first = table.begin();
  auto remaining = table.size();
  const auto maxRows =
      static_cast<std::size_t>(std::numeric_limits<int>::max());
  const auto rowsPerTask = config.enabled && config.rowsPerTask != 0 &&
                                   query.getKeyCondition() == nullptr
                               ? std::min(config.rowsPerTask, maxRows)
                               : remaining;
  while (plan.tasks.empty() || remaining != 0) {
    auto last = table.end();
    if (remaining > rowsPerTask) {
      last = first + static_cast<int>(rowsPerTask);
    }
    plan.tasks.push_back(makeTask(Task::Range{first, last}));
    first = last;
    remaining -= std::min(rowsPerTask, remaining);
  }
  return plan;
}

#endif  // SRC_TASKS_TASK_H_
