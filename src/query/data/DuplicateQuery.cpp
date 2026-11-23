#include "DuplicateQuery.h"
#include <string>

auto DuplicateQuery::toString() -> std::string {
  return "QUERY = DUPLICATE \"" + getTargetTable() + "\"";
}
