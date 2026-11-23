#include "SubQuery.h"
#include <string>

auto SubQuery::toString() -> std::string {
  return "QUERY = SUB " + getTargetTable() + "\"";
}
