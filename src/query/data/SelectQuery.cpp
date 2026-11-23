#include "SelectQuery.h"
#include <string>

auto SelectQuery::toString() -> std::string {
  return "QUERY = SELECT, Table = \"" + getTargetTable() + "\"";
}
