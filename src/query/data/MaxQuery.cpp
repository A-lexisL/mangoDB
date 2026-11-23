#include "MaxQuery.h"
#include <string>

auto MaxQuery::toString() -> std::string {
  return "QUERY = MAX, Table = \"" + getTargetTable() + "\"";
}
