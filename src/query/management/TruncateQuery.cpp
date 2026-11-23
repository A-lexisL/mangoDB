#include "TruncateQuery.h"
#include <string>

auto TruncateQuery::toString() -> std::string {
  return "QUERY = TRUNCATE, Table = \"" + getTargetTable() + "\"";
}
