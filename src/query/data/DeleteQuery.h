#ifndef SRC_QUERY_DATA_DELETEQUERY_H_
#define SRC_QUERY_DATA_DELETEQUERY_H_

#include <string>

#include "../Query.h"

class DeleteQuery : public ComplexQuery {
public:
  using ComplexQuery::ComplexQuery;

  auto toString() -> std::string override;
  auto split(const SplitConfig &config) -> TaskPlan override;
};

#endif  // SRC_QUERY_DATA_DELETEQUERY_H_
