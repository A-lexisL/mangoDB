//
// Created by liu on 18-10-25.
//

#ifndef SRC_QUERY_MANAGEMENT_LISTTABLEQUERY_H_
#define SRC_QUERY_MANAGEMENT_LISTTABLEQUERY_H_

#include <string>
#include <vector>

#include "../Query.h"

class ListTableQuery : public Query {
public:
  [[nodiscard]] auto requiredResources() const
      -> std::vector<query::Resource> override {
    return {
        {.kind = query::ResourceKind::kTableNamespace,
         .name = {},
         .mode = query::AccessMode::kRead},
    };
  }

  // Construct one complete-operation Task.
  auto split(const SplitConfig &config) -> TaskPlan override;

  auto toString() -> std::string override;
};

#endif  // SRC_QUERY_MANAGEMENT_LISTTABLEQUERY_H_
