#ifndef SRC_THREADS_THREADPOOLMANAGER_H_
#define SRC_THREADS_THREADPOOLMANAGER_H_
#include "../query/Query.h"
#include "../scheduler/QueryTypes.h"
#include "../tasks/Task.h"
#include "../tasks/TaskResult.h"
#include "CompletionQueue.h"
#include "TaskQueue.h"
#include "ThreadPool.h"
#include <cstddef>
#include <map>
#include <memory>
#include <utility>
#include <vector>

struct ThreadPoolManagerConfig {
  std::size_t workerLimit;
  SplitConfig split;
};

class ThreadPoolManager {
public:
  ThreadPoolManager(ThreadPoolManagerConfig config, TaskQueue &tasks,
                    CompletionQueue &completions);
  ~ThreadPoolManager();
  ThreadPoolManager(const ThreadPoolManager &) = delete;
  auto operator=(const ThreadPoolManager &) -> ThreadPoolManager & = delete;
  ThreadPoolManager(ThreadPoolManager &&) = delete;
  auto operator=(ThreadPoolManager &&) -> ThreadPoolManager & = delete;
  // Own a ready Query, split it, and submit its normal Tasks.
  void pushQuery(Query::Ptr query);
  // Process TaskResults and return completed Queries. When requested, wait for
  // an outstanding Task only if no completed Query is already buffered.
  auto getQueryDone(bool wait = false) -> std::vector<QueryDone>;
  void shutdown();

private:
  class QueryExecution {
    friend class ThreadPoolManager;
    Query::Ptr query;
    std::unique_ptr<FinalTask> finalTask;
    std::size_t remainingTasks;
    // Indexed by Task ID within the current execution phase.
    std::vector<TaskResult::Ptr> results;

  public:
    QueryExecution(Query::Ptr query, std::unique_ptr<FinalTask> finalTask,
                   std::size_t taskCount)
        : query(std::move(query)), finalTask(std::move(finalTask)),
          remainingTasks(taskCount), results(taskCount) {}
  };
  ThreadPoolManagerConfig config;
  std::map<QueryId, QueryExecution> executions;
  // Tasks across all Queries whose completions have not been consumed.
  std::size_t remainingTasks = 0;
  std::vector<QueryDone> completed;
  TaskQueue &tasks;
  CompletionQueue &completions;
  ThreadPool pool;
  static auto combineResults(std::vector<TaskResult::Ptr> results)
      -> TaskResult::Ptr;
  auto splitQuery(Query &query) const -> TaskPlan;
  void adjustThreadPoolSize();
  void applyFinalTask(QueryExecution &state, TaskResult::Ptr input);
};
#endif  // SRC_THREADS_THREADPOOLMANAGER_H_
