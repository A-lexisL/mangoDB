#include "ThreadPool.h"

#include <algorithm>
#include <cstddef>
#include <thread>
#include <utility>

#include "../db/Database.h"
#include "CompletionQueue.h"
#include "QueryRunner.h"
#include "TaskQueue.h"

ThreadPool::ThreadPool(ThreadPoolConfig poolConfig, TaskQueue &tasks,
                       CompletionQueue &completions)
    : config(poolConfig), taskQueue(tasks), completionQueue(completions) {
  if (config.workerCount == 0) {
    config.workerCount = std::max(1U, std::thread::hardware_concurrency());
  }
  // Initialize the singleton on the control thread before workers can use it.
  Database::getInstance();
  growTo(config.workerCount);
}

void ThreadPool::growTo(std::size_t workerCount) {
  if (workerCount <= workers.size()) {
    return;
  }
  try {
    workers.reserve(workerCount);
    for (std::size_t index = workers.size(); index < workerCount; ++index) {
      workers.emplace_back([this]() -> void {
        while (auto task = taskQueue.waitPop()) {
          QueryRunner::run(std::move(*task), completionQueue);
        }
      });
    }
  } catch (...) {
    taskQueue.close();
    join();
    throw;
  }
  config.workerCount = workers.size();
}

ThreadPool::~ThreadPool() { join(); }

void ThreadPool::join() {
  for (auto &worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

auto ThreadPool::workerCount() const -> std::size_t {
  return config.workerCount;
}
