#include "QueryRunner.h"

#include <exception>
#include <memory>
#include <utility>

#include "../query/Query.h"
#include "../query/QueryResult.h"
#include "../tasks/Task.h"
#include "../tasks/TaskResult.h"
#include "CompletionQueue.h"

auto QueryRunner::execute(Task::Ptr task) -> TaskResult::Ptr {
  TaskResult::Ptr result;
  try {
    result = task->execute();
  } catch (const std::exception &error) {
    result = std::make_unique<ErrorTaskResult>("EXECUTE", error.what());
  } catch (...) {
    result =
        std::make_unique<ErrorTaskResult>("EXECUTE", "Unknown query failure.");
  }
  result->queryId = task->queryId();
  result->taskId = task->taskId;
  return result;
}
void QueryRunner::run(Task::Ptr task, CompletionQueue &queue) {
  queue.push(execute(std::move(task)));
}

auto QueryRunner::executeQuery(Query &query) -> QueryResult::Ptr {
  const SplitConfig config{.rowsPerTask = 0, .enabled = false};
  auto plan = query.split(config);
  if (plan.result) {
    return std::move(plan.result);
  }
  auto result = execute(std::move(plan.tasks.front()));
  if (plan.finalTask && result->success()) {
    plan.finalTask->setInput(std::move(result));
    result = execute(std::move(plan.finalTask));
  }
  return transformResult(std::move(result));
}
