//
// Created by liu on 18-10-25.
//

#include "../query/Query.h"
#include "../query/management/LoadTableQuery.h"
#include "Task.h"
#include "TaskResult.h"
#include "WholeTask.h"

#include <exception>
#include <fstream>
#include <memory>

#include "../db/Database.h"
#include "../db/Table.h"
#include "../utils/formatter.h"

namespace {

class LoadTableWholeTask final : public WholeTask {
  static constexpr const char *qname = "LOAD";

public:
  explicit LoadTableWholeTask(const LoadTableQuery &query) : WholeTask(query) {}

  auto execute() -> TaskResult::Ptr override;
};

auto LoadTableWholeTask::execute() -> TaskResult::Ptr {
  const auto &parameters = dynamic_cast<const LoadTableQuery &>(getQuery());
  const auto &fileName = parameters.getFileName();
  try {
    std::ifstream infile(fileName);
    if (!infile.is_open()) {
      return std::make_unique<ErrorTaskResult>(qname, "Cannot open file '?'"_f %
                                                          fileName);
    }
    const auto &table = Database::loadTableFromStream(infile, fileName);
    infile.close();
    auto result = std::make_unique<SuccessTaskResult>();
    result->setTableChange(
        TableChange{.tableName = table.name(), .exists = true});
    return result;
  } catch (const std::exception &e) {
    return std::make_unique<ErrorTaskResult>(qname, e.what());
  }
}

}  // namespace

auto LoadTableQuery::split(const SplitConfig & /*config*/) -> TaskPlan {
  TaskPlan plan;
  plan.tasks.push_back(std::make_unique<LoadTableWholeTask>(*this));
  return plan;
}
