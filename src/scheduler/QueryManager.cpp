#include "QueryManager.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include "../query/Query.h"
#include "../query/QueryResult.h"
#include "../threads/ThreadPoolManager.h"
#include "InputReader.h"
#include "OutputBuffer.h"
#include "QueryDispatcher.h"
#include "QueryTypes.h"

void QueryManager::readBatch() {
  auto batch = reader->readQueries(batchQueryLimit);
  inputClosed = batch.inputClosed;
  for (auto &query : batch.queries) {
    dispatcher->accept(std::move(query));
  }
}

void QueryManager::processCompletions(bool wait) {
  auto completed = pool.getQueryDone(wait);
  for (auto &completion : completed) {
    if (!completion.result) {
      completion.result = std::make_unique<ErrorMsgResult>(
          "EXECUTE", "Query completed without a result.");
    }
    dispatcher->complete(completion.id, completion.result->success()
                                            ? completion.tableChange
                                            : std::nullopt);
    output->accept(completion.id, std::move(completion.result));
  }
}

void QueryManager::dispatchReady() {
  for (auto &query : dispatcher->takeReadyQueries()) {
    pool.pushQuery(std::move(query));
  }
}

void QueryManager::flushOutput() { output->flushConsecutive(); }

void QueryManager::run() {
  // Drain every accepted Query before the pool manager is destroyed.
  while (!inputClosed || !dispatcher->drained()) {
    // Bound all outstanding Queries, including those awaiting dependencies.
    if (!inputClosed && dispatcher->outstandingCount() < queryHighWaterMark) {
      readBatch();
    }
    processCompletions(false);
    dispatchReady();
    flushOutput();
    // Input is closed or throttled, and all ready Queries are dispatched.
    // Wait for progress only while dispatched Queries remain unfinished.
    if ((inputClosed || dispatcher->outstandingCount() >= queryHighWaterMark) &&
        dispatcher->readyCount() == 0 && dispatcher->runningCount() > 0) {
      processCompletions(true);
    }
  }
  // All Query phases have finished, so no final Task remains to be submitted.
  pool.shutdown();
  flushOutput();
  if (!output->empty()) {
    throw std::logic_error("Query output contains missing query IDs.");
  }
}
