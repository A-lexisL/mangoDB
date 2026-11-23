#ifndef SRC_SCHEDULER_OUTPUTBUFFER_H_
#define SRC_SCHEDULER_OUTPUTBUFFER_H_

#include <cstddef>
#include <unordered_map>
#include <utility>

#include "../query/QueryResult.h"
#include "QueryTypes.h"

class OutputBuffer {
public:
  OutputBuffer() = default;
  OutputBuffer(const OutputBuffer &) = delete;
  auto operator=(const OutputBuffer &) -> OutputBuffer & = delete;
  OutputBuffer(OutputBuffer &&) = delete;
  auto operator=(OutputBuffer &&) -> OutputBuffer & = delete;
  ~OutputBuffer() = default;

  // Store a completed result under its Query ID.
  void accept(QueryId queryId, QueryResult::Ptr &&result) {
    resultBuffer.emplace(queryId, std::move(result));
  }

  // Print and remove consecutive results starting at the next Query ID.
  void flushConsecutive();

  // Return whether there are no buffered results.
  auto empty() const -> bool { return resultBuffer.empty(); }

private:
  std::size_t nextPrintableId = 1;
  std::unordered_map<QueryId, QueryResult::Ptr> resultBuffer;
};

#endif  // SRC_SCHEDULER_OUTPUTBUFFER_H_
