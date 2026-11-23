#include "SumQuery.h"
#include <string>

auto SumQuery::toString() -> std::string {
  return "QUERY = SUM, Table = \"" + getTargetTable() + "\"";
}
