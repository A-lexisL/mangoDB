#ifndef SRC_THREADS_COMPLETIONQUEUE_H_
#define SRC_THREADS_COMPLETIONQUEUE_H_

#include <condition_variable>
#include <mutex>
#include <utility>
#include <vector>

#include "../tasks/TaskResult.h"

class CompletionQueue {
public:
  using Result = TaskResult::Ptr;

  CompletionQueue() = default;
  CompletionQueue(const CompletionQueue &) = delete;
  auto operator=(const CompletionQueue &) -> CompletionQueue & = delete;
  CompletionQueue(CompletionQueue &&) = delete;
  auto operator=(CompletionQueue &&) -> CompletionQueue & = delete;
  ~CompletionQueue() = default;

  // Enqueue a completion and wake a waiting consumer.
  void push(Result &&completion) {
    {
      const std::scoped_lock lock(mutex_);
      dones_.push_back(std::move(completion));
    }
    cv_.notify_one();
  }

  // Take all queued completions without waiting; return empty if there are
  // none.
  auto takeAvailable() -> std::vector<Result> {
    const std::scoped_lock lock(mutex_);
    std::vector<Result> ready;
    ready.swap(dones_);
    return ready;
  }

  // Wait for at least one completion and take all queued completions.
  // Call only while a submitted Query can still produce a completion.
  auto waitTake() -> std::vector<Result> {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this]() -> bool { return !dones_.empty(); });
    std::vector<Result> ready;
    ready.swap(dones_);
    return ready;
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::vector<Result> dones_;
};

#endif  // SRC_THREADS_COMPLETIONQUEUE_H_
