#ifndef SRC_SCHEDULER_QUERYTYPES_H_
#define SRC_SCHEDULER_QUERYTYPES_H_

#include <optional>
#include <vector>

#include "../query/Query.h"
#include "../query/QueryResult.h"

using QueryId = Query::Id;

struct InputBatch {
  std::vector<Query::Ptr> queries;
  bool inputClosed = false;
};

struct QueryDone {
  QueryId id = 0;
  QueryResult::Ptr result;
  std::optional<TableChange> tableChange = std::nullopt;
};

#endif  // SRC_SCHEDULER_QUERYTYPES_H_
