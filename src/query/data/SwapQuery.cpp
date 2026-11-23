#include "SwapQuery.h"
#include <string>

auto SwapQuery::toString() -> std::string {
  return "QUERY = SWAP " + getTargetTable() + "\"";
}
