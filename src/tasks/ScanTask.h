#ifndef SRC_TASKS_SCANTASK_H_
#define SRC_TASKS_SCANTASK_H_

#include <string>

#include "../db/Table.h"
#include "../query/Query.h"
#include "Task.h"
#include "TaskResult.h"

class ScanTask : public Task {
public:
  ScanTask(const ComplexQuery &query, Range range,
           std::string queryName = "EXECUTE");

  auto execute() -> TaskResult::Ptr override;

private:
  const ComplexQuery &parameters;

protected:
  virtual void processRow(const Table::Object &row) = 0;
  virtual auto finishPartial() -> TaskResult::Ptr = 0;

private:
  std::string queryName;
};

#endif  // SRC_TASKS_SCANTASK_H_
