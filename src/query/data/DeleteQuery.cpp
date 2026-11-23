#include "DeleteQuery.h"
#include <string>

auto DeleteQuery::toString() -> std::string {
  return "QUERY = DELETE \"" + getTargetTable() + "\"";
}
