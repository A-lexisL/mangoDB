#ifndef SRC_TASKS_WHOLETASK_H_
#define SRC_TASKS_WHOLETASK_H_
#include "Task.h"

// Execution family for one complete, unsplit operation.
class WholeTask : public Task {
public:
  using Task::Task;
};
#endif  // SRC_TASKS_WHOLETASK_H_
