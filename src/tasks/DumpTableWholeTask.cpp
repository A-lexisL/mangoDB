//
// Created by liu on 18-10-25.
//

#include "../query/Query.h"
#include "../query/management/DumpTableQuery.h"
#include "Task.h"
#include "TaskResult.h"
#include "WholeTask.h"

#include <exception>
#include <fstream>
#include <memory>

#include "../db/Database.h"
#include "../utils/formatter.h"

namespace {

class DumpTableWholeTask final : public WholeTask {
  static constexpr const char *qname = "DUMP";

public:
  explicit DumpTableWholeTask(const DumpTableQuery &query) : WholeTask(query) {}

  auto execute() -> TaskResult::Ptr override;
};

auto DumpTableWholeTask::execute() -> TaskResult::Ptr {
  const auto &parameters = dynamic_cast<const DumpTableQuery &>(getQuery());
  const auto &targetTable = getQuery().getTargetTable();
  const auto &fileName = parameters.getFileName();
  const auto &database = Database::getInstance();
  try {
    std::ofstream outfile(fileName);
    if (!outfile.is_open()) {
      return std::make_unique<ErrorTaskResult>(qname, "Cannot open file '?'"_f %
                                                          fileName);
    }
    outfile << database.getTable(targetTable);
    outfile.close();
    return std::make_unique<SuccessTaskResult>();
  } catch (const std::exception &e) {
    return std::make_unique<ErrorTaskResult>(qname, e.what());
  }
}

}  // namespace

auto DumpTableQuery::split(const SplitConfig & /*config*/) -> TaskPlan {
  TaskPlan plan;
  plan.tasks.push_back(std::make_unique<DumpTableWholeTask>(*this));
  return plan;
}
