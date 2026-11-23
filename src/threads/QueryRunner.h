#ifndef SRC_THREADS_QUERYRUNNER_H_
#define SRC_THREADS_QUERYRUNNER_H_

#include "../query/Query.h"
#include "../query/QueryResult.h"
#include "../tasks/Task.h"
#include "../tasks/TaskResult.h"
#include "CompletionQueue.h"

class QueryRunner final {
public:
  // Disable splitting and execute one whole Task on the calling thread.
  static auto executeQuery(Query &query) -> QueryResult::Ptr;

  // Execute one Task, attach identifiers, and convert escaped query failures.
  static auto execute(Task::Ptr task) -> TaskResult::Ptr;

  // Execute a Task and publish its owned result.
  static void run(Task::Ptr task, CompletionQueue &completionQueue);
};

#endif  // SRC_THREADS_QUERYRUNNER_H_
