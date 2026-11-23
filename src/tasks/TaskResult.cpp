#include "TaskResult.h"
#include "../query/QueryResult.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

ErrorTaskResult::ErrorTaskResult(std::string queryName, std::string message)
    : queryName(std::move(queryName)), message(std::move(message)) {}

ErrorTaskResult::ErrorTaskResult(std::string queryName, std::string tableName,
                                 std::string message)
    : queryName(std::move(queryName)), tableName(std::move(tableName)),
      message(std::move(message)) {}

DisplayTaskResult::DisplayTaskResult(std::string text)
    : text(std::move(text)) {}

void DisplayTaskResult::combine(TaskResult &other) {
  text += dynamic_cast<DisplayTaskResult &>(other).text;
}

void SumTaskResult::combine(TaskResult &other) {
  const auto &rhs = dynamic_cast<const SumTaskResult &>(other);
  for (std::size_t i = 0; i < values.size(); ++i) {
    values.at(i) += rhs.values.at(i);
  }
}

void MaxTaskResult::combine(TaskResult &other) {
  auto &rhs = dynamic_cast<MaxTaskResult &>(other);
  if (!rhs.values) {
    return;
  }
  if (!values) {
    values = std::move(rhs.values);
    return;
  }
  for (std::size_t i = 0; i < values->size(); ++i) {
    values->at(i) = std::max(values->at(i), rhs.values->at(i));
  }
}

void SelectTaskResult::combine(TaskResult &other) {
  auto &rhs = dynamic_cast<SelectTaskResult &>(other);
  rows.insert(rows.end(), std::make_move_iterator(rhs.rows.begin()),
              std::make_move_iterator(rhs.rows.end()));
}

void RecordCountTaskResult::combine(TaskResult &other) {
  affectedRows +=
      dynamic_cast<const RecordCountTaskResult &>(other).affectedRows;
}

void KeysTaskResult::combine(TaskResult &other) {
  auto &rhs = dynamic_cast<KeysTaskResult &>(other);
  keys.insert(keys.end(), std::make_move_iterator(rhs.keys.begin()),
              std::make_move_iterator(rhs.keys.end()));
}

auto transformResult(TaskResult::Ptr result) -> QueryResult::Ptr {
  if (const auto *error = dynamic_cast<const ErrorTaskResult *>(result.get())) {
    if (error->tableName) {
      return std::make_unique<ErrorMsgResult>(
          error->queryName.c_str(), *error->tableName, error->message);
    }
    return std::make_unique<ErrorMsgResult>(error->queryName.c_str(),
                                            error->message);
  }
  if (dynamic_cast<const SuccessTaskResult *>(result.get()) != nullptr) {
    return std::make_unique<NullQueryResult>();
  }
  if (auto *display = dynamic_cast<DisplayTaskResult *>(result.get())) {
    return std::make_unique<DisplayQueryResult>(std::move(display->text));
  }
  if (auto *sum = dynamic_cast<SumTaskResult *>(result.get())) {
    return std::make_unique<AggregateQueryResult>(std::move(sum->values));
  }
  if (auto *maximum = dynamic_cast<MaxTaskResult *>(result.get())) {
    if (!maximum->values) {
      return std::make_unique<NullQueryResult>();
    }
    return std::make_unique<AggregateQueryResult>(std::move(*maximum->values));
  }
  if (auto *selected = dynamic_cast<SelectTaskResult *>(result.get())) {
    std::ranges::sort(selected->rows,
                      [](const auto &left, const auto &right) -> bool {
                        return left.key < right.key;
                      });
    return std::make_unique<SelectQueryResult>(std::move(selected->rows));
  }
  if (const auto *count =
          dynamic_cast<const RecordCountTaskResult *>(result.get())) {
    return std::make_unique<RecordCountResult>(count->affectedRows);
  }
  throw std::logic_error("Task result requires a final task before output.");
}
