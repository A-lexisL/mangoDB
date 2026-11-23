#ifndef SRC_TASKS_TASKRESULT_H_
#define SRC_TASKS_TASKRESULT_H_

#include "../query/Query.h"
#include "../query/QueryResult.h"
#include "../query/ResultRow.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class TaskResult {
  friend class QueryRunner;
  friend class ThreadPoolManager;

public:
  using Ptr = std::unique_ptr<TaskResult>;

private:
  Query::Id queryId = 0;
  std::size_t taskId = 0;
  std::optional<TableChange> tableChange;

public:
  void setTableChange(std::optional<TableChange> change) {
    tableChange = std::move(change);
  }

  TaskResult() = default;
  TaskResult(const TaskResult &) = default;
  auto operator=(const TaskResult &) -> TaskResult & = default;
  TaskResult(TaskResult &&) = default;
  auto operator=(TaskResult &&) -> TaskResult & = default;
  virtual ~TaskResult() = default;
  // Report whether this Task completed successfully.
  [[nodiscard]] virtual auto success() const -> bool { return true; }
  // Consume a compatible partial result from the same Query execution phase.
  virtual void combine(TaskResult &other) = 0;
};

class ErrorTaskResult final : public TaskResult {
  friend auto transformResult(TaskResult::Ptr result) -> QueryResult::Ptr;
  std::string queryName;
  std::optional<std::string> tableName;
  std::string message;

public:
  ErrorTaskResult(std::string queryName, std::string message);
  ErrorTaskResult(std::string queryName, std::string tableName,
                  std::string message);
  [[nodiscard]] auto success() const -> bool override { return false; }
  void combine(TaskResult & /*other*/) override {}
};

struct SuccessTaskResult final : TaskResult {
  void combine(TaskResult & /*other*/) override {}
};

class DisplayTaskResult final : public TaskResult {
  friend auto transformResult(TaskResult::Ptr result) -> QueryResult::Ptr;
  std::string text;

public:
  explicit DisplayTaskResult(std::string text);
  void combine(TaskResult &other) override;
};

class SumTaskResult final : public TaskResult {
  friend auto transformResult(TaskResult::Ptr result) -> QueryResult::Ptr;
  std::vector<int> values;

public:
  explicit SumTaskResult(std::vector<int> values) : values(std::move(values)) {}
  void combine(TaskResult &other) override;
};

class MaxTaskResult final : public TaskResult {
  friend auto transformResult(TaskResult::Ptr result) -> QueryResult::Ptr;
  std::optional<std::vector<int>> values;

public:
  MaxTaskResult() = default;
  explicit MaxTaskResult(std::vector<int> values) : values(std::move(values)) {}
  void combine(TaskResult &other) override;
};

class SelectTaskResult final : public TaskResult {
  friend auto transformResult(TaskResult::Ptr result) -> QueryResult::Ptr;
  std::vector<ResultRow> rows;

public:
  explicit SelectTaskResult(std::vector<ResultRow> rows)
      : rows(std::move(rows)) {}
  void combine(TaskResult &other) override;
};

class RecordCountTaskResult final : public TaskResult {
  friend auto transformResult(TaskResult::Ptr result) -> QueryResult::Ptr;
  int affectedRows;

public:
  explicit RecordCountTaskResult(int count) : affectedRows(count) {}
  void combine(TaskResult &other) override;
};

class KeysTaskResult final : public TaskResult {
  friend class ApplyTask;
  std::vector<std::string> keys;

public:
  explicit KeysTaskResult(std::vector<std::string> keys)
      : keys(std::move(keys)) {}
  void combine(TaskResult &other) override;
};

// Consume one terminal result and construct its output representation.
auto transformResult(TaskResult::Ptr result) -> QueryResult::Ptr;

#endif  // SRC_TASKS_TASKRESULT_H_
