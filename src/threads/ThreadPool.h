#ifndef SRC_THREADS_THREADPOOL_H_
#define SRC_THREADS_THREADPOOL_H_

#include <cstddef>
#include <thread>
#include <vector>

#include "CompletionQueue.h"
#include "TaskQueue.h"

struct ThreadPoolConfig {
  std::size_t workerCount = 0;
};

class ThreadPool final {
public:
  ThreadPool(ThreadPoolConfig poolConfig, TaskQueue &tasks,
             CompletionQueue &completions);

  ThreadPool(const ThreadPool &) = delete;
  auto operator=(const ThreadPool &) -> ThreadPool & = delete;
  ThreadPool(ThreadPool &&) = delete;
  auto operator=(ThreadPool &&) -> ThreadPool & = delete;

  ~ThreadPool();

  void growTo(std::size_t workerCount);

  // The caller closes TaskQueue before joining on the normal path.
  void join();

  [[nodiscard]] auto workerCount() const -> std::size_t;

private:
  ThreadPoolConfig config;
  TaskQueue &taskQueue;
  CompletionQueue &completionQueue;
  std::vector<std::jthread> workers;
};

#endif  // SRC_THREADS_THREADPOOL_H_
