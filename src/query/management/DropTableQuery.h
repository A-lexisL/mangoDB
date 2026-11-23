//
// Created by liu on 18-10-25.
//

#ifndef SRC_QUERY_MANAGEMENT_DROPTABLEQUERY_H_
#define SRC_QUERY_MANAGEMENT_DROPTABLEQUERY_H_

#include <optional>
#include <string>

#include "../Query.h"

class DropTableQuery : public Query {
public:
  using Query::Query;

  // Construct one complete-operation Task.
  auto split(const SplitConfig &config) -> TaskPlan override;

  [[nodiscard]] auto tableChange() const
      -> std::optional<TableChange> override {
    return TableChange{.tableName = getTargetTable(), .exists = false};
  }

  auto toString() -> std::string override;
};

#endif  // SRC_QUERY_MANAGEMENT_DROPTABLEQUERY_H_
