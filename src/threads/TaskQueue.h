#ifndef SRC_THREADS_TASKQUEUE_H_
#define SRC_THREADS_TASKQUEUE_H_

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

#include "../tasks/Task.h"

class TaskQueue {
public:
  using Task = Task::Ptr;

  TaskQueue() = default;
  TaskQueue(const TaskQueue &) = delete;
  auto operator=(const TaskQueue &) -> TaskQueue & = delete;
  TaskQueue(TaskQueue &&) = delete;
  auto operator=(TaskQueue &&) -> TaskQueue & = delete;
  ~TaskQueue() = default;

  // Enqueue a Task and wake a worker; return false without moving if closed.
  auto push(Task &&task) -> bool {
    {
      const std::scoped_lock lock(mutex_);
      if (closed_) {
        return false;
      }
      tasks_.push_back(std::move(task));
    }
    cv_.notify_one();
    return true;
  }

  // Wait for a Task; return empty only when the closed queue has been drained.
  auto waitPop() -> std::optional<Task> {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this]() -> bool { return closed_ || !tasks_.empty(); });
    if (tasks_.empty()) {
      return std::nullopt;
    }
    Task task = std::move(tasks_.front());
    tasks_.pop_front();
    return std::optional<Task>{std::move(task)};
  }

  // Reject future pushes and wake all workers so they can drain and exit.
  void close() {
    {
      const std::scoped_lock lock(mutex_);
      closed_ = true;
    }
    cv_.notify_all();
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<Task> tasks_;
  bool closed_ = false;
};

#endif  // SRC_THREADS_TASKQUEUE_H_
