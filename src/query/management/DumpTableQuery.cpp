#include "DumpTableQuery.h"
#include <string>
//
// Created by liu on 18-10-25.
//

auto DumpTableQuery::toString() -> std::string {
  return "QUERY = Dump TABLE, FILE = \"" + this->fileName + "\"";
}
