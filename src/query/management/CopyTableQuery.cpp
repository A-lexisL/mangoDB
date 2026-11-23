#include "CopyTableQuery.h"
#include <string>

auto CopyTableQuery::toString() -> std::string {
  return "QUERY = COPYTABLE, table = \"" + this->original + "\" newtable = \"" +
         getTargetTable() + "\"";
}
