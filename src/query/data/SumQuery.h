#ifndef SRC_QUERY_DATA_SUMQUERY_H_
#define SRC_QUERY_DATA_SUMQUERY_H_

#include <string>
#include <vector>

#include "../Query.h"

class SumQuery : public ComplexQuery {
public:
  using ComplexQuery::ComplexQuery;

  [[nodiscard]] auto requiredResources() const
      -> std::vector<query::Resource> override {
    return {
        {.kind = query::ResourceKind::kTable,
         .name = getTargetTable(),
         .mode = query::AccessMode::kRead},
    };
  }

  auto toString() -> std::string override;

  // Prepare conditions and construct this operation's execution units.
  auto split(const SplitConfig &config) -> TaskPlan override;
};

#endif  // SRC_QUERY_DATA_SUMQUERY_H_
