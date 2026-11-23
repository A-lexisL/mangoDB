#ifndef SRC_SCHEDULER_QUERYMANAGER_H_
#define SRC_SCHEDULER_QUERYMANAGER_H_

#include <cstddef>
#include <memory>
#include <utility>

#include "InputReader.h"
#include "OutputBuffer.h"
#include "QueryDispatcher.h"

class ThreadPoolManager;

class QueryManager {
private:
  std::unique_ptr<InputReader> reader;
  std::unique_ptr<QueryDispatcher> dispatcher;
  std::unique_ptr<OutputBuffer> output;
  ThreadPoolManager &pool;
  std::size_t batchQueryLimit;
  std::size_t queryHighWaterMark;
  bool inputClosed = false;

  // Read a batch and pass its Queries to the Dispatcher.
  void readBatch();

  // Pass ready Queries to ThreadPoolManager.
  void dispatchReady();

  // Collect completed Queries, optionally waiting for task progress, then
  // release reservations and buffer results.
  void processCompletions(bool wait);

  // Print the consecutive results available in OutputBuffer.
  void flushOutput();

public:
  struct Limits {
    std::size_t batchQueryLimit;
    std::size_t queryHighWaterMark;
  };

  QueryManager(std::unique_ptr<InputReader> reader,
               std::unique_ptr<QueryDispatcher> dispatcher,
               std::unique_ptr<OutputBuffer> output, ThreadPoolManager &pool,
               Limits limits)
      : reader(std::move(reader)), dispatcher(std::move(dispatcher)),
        output(std::move(output)), pool(pool),
        batchQueryLimit(limits.batchQueryLimit),
        queryHighWaterMark(limits.queryHighWaterMark) {}

  QueryManager(const QueryManager &) = delete;
  auto operator=(const QueryManager &) -> QueryManager & = delete;
  QueryManager(QueryManager &&) = delete;
  auto operator=(QueryManager &&) -> QueryManager & = delete;
  ~QueryManager() = default;

  // Coordinate input, dispatch, completion processing, and output until
  // drained.
  void run();
};

#endif  // SRC_SCHEDULER_QUERYMANAGER_H_
