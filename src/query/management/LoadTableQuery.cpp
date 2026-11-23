#include "LoadTableQuery.h"
#include <string>
//
// Created by liu on 18-10-25.
//

auto LoadTableQuery::toString() -> std::string {
  return "QUERY = Load TABLE, FILE = \"" + this->fileName + "\"";
}
