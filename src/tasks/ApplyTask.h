#ifndef SRC_TASKS_APPLYTASK_H_
#define SRC_TASKS_APPLYTASK_H_

#include <string>
#include <vector>

#include "../db/Table.h"
#include "../query/Query.h"
#include "Task.h"
#include "TaskResult.h"

class ApplyTask : public FinalTask {
public:
  explicit ApplyTask(const ComplexQuery &query,
                     std::string queryName = "EXECUTE");

  void setInput(TaskResult::Ptr input) override;
  auto execute() -> TaskResult::Ptr override;

private:
  std::vector<Table::KeyType> keys;

protected:
  virtual auto applyKey(Table &table, const Table::KeyType &key) -> bool = 0;

private:
  std::string queryName;
};

#endif  // SRC_TASKS_APPLYTASK_H_
