//
// Created by liu on 18-10-25.
//

#ifndef SRC_QUERY_MANAGEMENT_QUITQUERY_H_
#define SRC_QUERY_MANAGEMENT_QUITQUERY_H_

#include <string>

#include "../Query.h"

class QuitQuery : public Query {
public:
  QuitQuery() = default;

  // Construct one complete-operation Task.
  auto split(const SplitConfig &config) -> TaskPlan override;

  [[nodiscard]] auto requestsExit() const -> bool override { return true; }

  auto toString() -> std::string override;
};

#endif  // SRC_QUERY_MANAGEMENT_QUITQUERY_H_
