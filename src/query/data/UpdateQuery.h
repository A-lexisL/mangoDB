//
// Created by liu on 18-10-25.
//

#ifndef SRC_QUERY_DATA_UPDATEQUERY_H_
#define SRC_QUERY_DATA_UPDATEQUERY_H_

#include <string>

#include "../Query.h"

class UpdateQuery : public ComplexQuery {
public:
  using ComplexQuery::ComplexQuery;

  auto toString() -> std::string override;

  // Prepare conditions and construct this operation's execution units.
  auto split(const SplitConfig &config) -> TaskPlan override;
};

#endif  // SRC_QUERY_DATA_UPDATEQUERY_H_
