//
// Created by liu on 18-10-25.
//
#include "QuitQuery.h"
#include "../../tasks/Task.h"
#include "../Query.h"
#include "../QueryResult.h"
#include <memory>
#include <string>

auto QuitQuery::toString() -> std::string { return "QUERY = Quit"; }

auto QuitQuery::split(const SplitConfig & /*config*/) -> TaskPlan {
  return TaskPlan::completed(std::make_unique<NullQueryResult>());
}
