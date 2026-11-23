#ifndef SRC_QUERY_MANAGEMENT_TRUNCATEQUERY_H_
#define SRC_QUERY_MANAGEMENT_TRUNCATEQUERY_H_

#include <string>

#include "../Query.h"

class TruncateQuery : public Query {
public:
  using Query::Query;

  // Construct one complete-operation Task.
  auto split(const SplitConfig &config) -> TaskPlan override;

  auto toString() -> std::string override;
};

#endif  // SRC_QUERY_MANAGEMENT_TRUNCATEQUERY_H_
