//
// Created by liu on 18-10-23.
//

#include "Database.h"

#include <algorithm>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../utils/formatter.h"
#include "../utils/uexception.h"
#include "Table.h"

void Database::testDuplicate(const std::string &tableName) {
  const std::scoped_lock lock(mapMutex);
  auto const iterator = this->tables.find(tableName);
  if (iterator != this->tables.end()) {
    throw DuplicatedTableName("Error when inserting table \"" + tableName +
                              "\". Name already exists.");
  }
}

auto Database::registerTable(Table::Ptr &&table) -> Table & {
  auto const name = table->name();
  const std::scoped_lock lock(mapMutex);
  auto [position, inserted] = tables.try_emplace(name, std::move(table));
  if (!inserted) {
    throw DuplicatedTableName("Error when inserting table \"" + name +
                              "\". Name already exists.");
  }
  return *position->second;
}

auto Database::getTable(const std::string &tableName) -> Table & {
  const std::scoped_lock lock(mapMutex);
  auto const iterator = this->tables.find(tableName);
  if (iterator == this->tables.end()) {
    throw TableNameNotFound("Error accesing table \"" + tableName +
                            "\". Table not found.");
  }
  return *iterator->second;
}

auto Database::getTable(const std::string &tableName) const -> const Table & {
  const std::scoped_lock lock(mapMutex);
  auto const iterator = this->tables.find(tableName);
  if (iterator == this->tables.end()) {
    throw TableNameNotFound("Error accesing table \"" + tableName +
                            "\". Table not found.");
  }
  return *iterator->second;
}

void Database::dropTable(const std::string &tableName) {
  Table::Ptr removed;
  {
    const std::scoped_lock lock(mapMutex);
    auto const iterator = tables.find(tableName);
    if (iterator == tables.end()) {
      throw TableNameNotFound("Error when trying to drop table \"" + tableName +
                              "\". Table not found.");
    }
    removed = std::move(iterator->second);
    tables.erase(iterator);
  }
  {
    // Release the table lock before destroying the table and its mutex.
    const std::unique_lock lock(removed->mutex());
  }
}

void Database::printAllTable() { printAllTable(std::cout); }

void Database::printAllTable(std::ostream &output) {
  struct Summary {
    std::string name;
    Table::SizeType fields;
    Table::SizeType rows;
  };
  std::vector<const Table *> snapshot;
  {
    const std::scoped_lock lock(mapMutex);
    snapshot.reserve(tables.size());
    std::ranges::transform(
        tables, std::back_inserter(snapshot),
        [](const auto &entry) -> const Table * { return entry.second.get(); });
  }
  // LIST's namespace reservation keeps these tables alive during the snapshot.
  std::vector<Summary> summaries;
  summaries.reserve(snapshot.size());
  for (const auto *table : snapshot) {
    const std::shared_lock lock(table->mutex());
    summaries.push_back({.name = table->name(),
                         .fields = table->field().size() + 1,
                         .rows = table->size()});
  }
  const int width = 15;
  output << "Database overview:" << '\n';
  output << "=========================" << '\n';
  output << std::setw(width) << "Table name";
  output << std::setw(width) << "# of fields";
  output << std::setw(width) << "# of entries" << '\n';
  for (const auto &table : summaries) {
    output << std::setw(width) << table.name;
    output << std::setw(width) << table.fields;
    output << std::setw(width) << table.rows << '\n';
  }
  output << "Total " << summaries.size() << " tables." << '\n';
  output << "=========================" << '\n';
}

auto Database::getInstance() -> Database & {
  static Database instance;
  return instance;
}

void Database::updateFileTableName(const std::string &fileName,
                                   const std::string &tableName) {
  const std::scoped_lock lock(mapMutex);
  fileTableNameMap[fileName] = tableName;
}

auto Database::getFileTableName(const std::string &fileName) -> std::string {
  {
    const std::scoped_lock lock(mapMutex);
    auto const iterator = fileTableNameMap.find(fileName);
    if (iterator != fileTableNameMap.end()) {
      return iterator->second;
    }
  }
  std::ifstream infile(fileName);
  if (!infile.is_open()) {
    return "";
  }
  std::string tableName;
  infile >> tableName;
  infile.close();
  const std::scoped_lock lock(mapMutex);
  return fileTableNameMap.try_emplace(fileName, tableName).first->second;
}

auto Database::loadTableFromStream(std::istream &input,
                                   const std::string &source) -> Table & {
  auto &database = Database::getInstance();
  std::string const errString =
      !source.empty() ? R"(Invalid table (from "?") format: )"_f % source
                      : "Invalid table format: ";

  std::string tableName;
  Table::SizeType fieldCount = 0;
  std::deque<Table::KeyType> fields;

  std::string line;
  std::stringstream sstream;
  if (!std::getline(input, line)) {
    throw LoadFromStreamException(errString +
                                  "Failed to read table metadata line.");
  }

  sstream.str(line);
  sstream >> tableName >> fieldCount;
  if (!sstream) {
    throw LoadFromStreamException(errString +
                                  "Failed to parse table metadata.");
  }

  // throw error if tableName duplicates
  database.testDuplicate(tableName);

  if (!std::getline(input, line)) {
    throw LoadFromStreamException(errString + "Failed to load field names.");
  }

  sstream.clear();
  sstream.str(line);
  for (Table::SizeType i = 0; i < fieldCount; ++i) {
    std::string field;
    if (!(sstream >> field)) {
      throw LoadFromStreamException(errString + "Failed to load field names.");
    }
    fields.emplace_back(std::move(field));
  }

  if (fields.front() != "KEY") {
    throw LoadFromStreamException(errString + "Missing or invalid KEY field.");
  }

  fields.erase(fields.begin());  // Remove leading key
  auto table = std::make_unique<Table>(tableName, fields);

  Table::SizeType lineCount = 2;
  while (std::getline(input, line)) {
    if (line.empty()) {
      break;  // Read to an empty line
    }
    lineCount++;
    sstream.clear();
    sstream.str(line);
    std::string key;
    if (!(sstream >> key)) {
      throw LoadFromStreamException(errString +
                                    "Missing or invalid KEY field.");
    }
    std::vector<Table::ValueType> tuple;
    tuple.reserve(fieldCount - 1);
    for (Table::SizeType i = 1; i < fieldCount; ++i) {
      Table::ValueType value = 0;
      if (!(sstream >> value)) {
        throw LoadFromStreamException(errString + "Invalid row on LINE " +
                                      std::to_string(lineCount));
      }
      tuple.emplace_back(value);
    }
    table->insertByIndex(key, std::move(tuple));
  }

  return database.registerTable(std::move(table));
}
