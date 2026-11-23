#include "ThreadPoolManager.h"
#include "../query/Query.h"
#include "../scheduler/QueryTypes.h"
#include "../tasks/Task.h"
#include "../tasks/TaskResult.h"
#include "CompletionQueue.h"
#include "TaskQueue.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

ThreadPoolManager::ThreadPoolManager(ThreadPoolManagerConfig config,
                                     TaskQueue &tasks,
                                     CompletionQueue &completions)
    : config(config), tasks(tasks), completions(completions),
      pool({1}, tasks, completions) {}
ThreadPoolManager::~ThreadPoolManager() { shutdown(); }
void ThreadPoolManager::shutdown() { tasks.close(); }
auto ThreadPoolManager::splitQuery(Query &query) const -> TaskPlan {
  return query.split(config.split);
}
void ThreadPoolManager::adjustThreadPoolSize() {
  pool.growTo(std::min(config.workerLimit, remainingTasks));
}
void ThreadPoolManager::pushQuery(Query::Ptr query) {
  auto plan = splitQuery(*query);
  const auto queryId = query->getId();
  // QueryResult already exists
  if (plan.result) {
    completed.push_back({.id = queryId,
                         .result = std::move(plan.result),
                         .tableChange = std::nullopt});
    return;
  }
  executions.try_emplace(queryId, std::move(query), std::move(plan.finalTask),
                         plan.tasks.size());
  remainingTasks += plan.tasks.size();
  adjustThreadPoolSize();
  // push tasks to TaskQueue
  for (std::size_t i = 0; i < plan.tasks.size(); ++i) {
    plan.tasks.at(i)->taskId = i;
    tasks.push(std::move(plan.tasks.at(i)));
  }
}
void ThreadPoolManager::applyFinalTask(QueryExecution &state,
                                       TaskResult::Ptr input) {
  state.finalTask->setInput(std::move(input));
  state.finalTask->taskId = 0;
  state.results.clear();
  state.results.resize(1);
  state.remainingTasks = 1;
  ++remainingTasks;
  adjustThreadPoolSize();
  // push the final Task to TaskQueue
  tasks.push(std::move(state.finalTask));
}
auto ThreadPoolManager::combineResults(std::vector<TaskResult::Ptr> results)
    -> TaskResult::Ptr {
  const auto failure = std::ranges::find_if(
      results, [](const auto &result) -> bool { return !result->success(); });
  if (failure != results.end()) {
    return std::move(*failure);
  }
  auto combined = std::move(results.front());
  for (std::size_t i = 1; i < results.size(); ++i) {
    combined->combine(*results.at(i));
  }
  return combined;
}
auto ThreadPoolManager::getQueryDone(bool wait) -> std::vector<QueryDone> {
  std::vector<QueryDone> done;
  done.swap(completed);
  auto results = wait && done.empty() && remainingTasks > 0
                     ? completions.waitTake()
                     : completions.takeAvailable();
  for (auto &result : results) {
    auto const position = executions.find(result->queryId);
    auto &state = position->second;
    state.results.at(result->taskId) = std::move(result);
    --remainingTasks;
    // the query still has running tasks
    if (--state.remainingTasks != 0) {
      continue;
    }
    auto combined = combineResults(std::move(state.results));
    const bool failed = !combined->success();
    if (state.finalTask && !failed) {
      applyFinalTask(state, std::move(combined));
    } else {
      auto change = failed ? std::nullopt : std::move(combined->tableChange);
      done.push_back({.id = state.query->getId(),
                      .result = transformResult(std::move(combined)),
                      .tableChange = std::move(change)});
      executions.erase(position);
    }
  }
  return done;
}
