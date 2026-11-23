#include "DropTableQuery.h"
#include <string>
//
// Created by liu on 18-10-25.
//

auto DropTableQuery::toString() -> std::string {
  return "QUERY = DROP, Table = \"" + getTargetTable() + "\"";
}
