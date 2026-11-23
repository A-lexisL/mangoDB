#include "UpdateQuery.h"
#include <string>
//
// Created by liu on 18-10-25.
//

auto UpdateQuery::toString() -> std::string {
  return "QUERY = UPDATE " + getTargetTable() + "\"";
}
