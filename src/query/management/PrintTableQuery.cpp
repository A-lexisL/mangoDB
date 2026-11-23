#include "PrintTableQuery.h"
#include <string>
//
// Created by liu on 18-10-25.
//

auto PrintTableQuery::toString() -> std::string {
  return "QUERY = SHOWTABLE, Table = \"" + getTargetTable() + "\"";
}
