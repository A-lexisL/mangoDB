//
// Created by liu on 18-10-25.
//

#include "../query/Query.h"
#include "../query/management/ListTableQuery.h"
#include "Task.h"
#include "TaskResult.h"
#include "WholeTask.h"

#include <memory>
#include <sstream>

#include "../db/Database.h"

namespace {

class ListTableWholeTask final : public WholeTask {
public:
  explicit ListTableWholeTask(const ListTableQuery &query) : WholeTask(query) {}

  auto execute() -> TaskResult::Ptr override;
};

auto ListTableWholeTask::execute() -> TaskResult::Ptr {
  Database &database = Database::getInstance();
  std::ostringstream output;
  database.printAllTable(output);
  return std::make_unique<DisplayTaskResult>(output.str());
}

}  // namespace

auto ListTableQuery::split(const SplitConfig & /*config*/) -> TaskPlan {
  TaskPlan plan;
  plan.tasks.push_back(std::make_unique<ListTableWholeTask>(*this));
  return plan;
}
