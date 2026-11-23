#include "InsertQuery.h"
#include <string>
//
// Created by liu on 18-10-25.
//

auto InsertQuery::toString() -> std::string {
  return "QUERY = INSERT " + getTargetTable() + "\"";
}
