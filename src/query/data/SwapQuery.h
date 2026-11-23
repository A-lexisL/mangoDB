#ifndef SRC_QUERY_DATA_SWAPQUERY_H_
#define SRC_QUERY_DATA_SWAPQUERY_H_

#include <string>

#include "../Query.h"

class SwapQuery : public ComplexQuery {
public:
  using ComplexQuery::ComplexQuery;

  auto toString() -> std::string override;

  // Prepare conditions and construct this operation's execution units.
  auto split(const SplitConfig &config) -> TaskPlan override;
};

#endif  // SRC_QUERY_DATA_SWAPQUERY_H_
