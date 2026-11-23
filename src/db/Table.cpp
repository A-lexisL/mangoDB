//
// Created by liu on 18-10-23.
//

#include "Table.h"

#include <iomanip>
#include <iostream>
#include <shared_mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../utils/formatter.h"
#include "../utils/uexception.h"

auto Table::getFieldIndex(const Table::FieldNameType &field) const
    -> Table::FieldIndex {
  try {
    return this->fieldMap.at(field);
  } catch (const std::out_of_range &e) {
    throw TableFieldNotFound(R"(Field name "?" doesn't exists.)"_f % field);
  }
}

auto Table::eraseByKey(const KeyType &key) -> bool {
  auto const keyMapIt = keyMap.find(key);
  if (keyMapIt == keyMap.end()) {
    return false;
  }
  const SizeType rowIndex = keyMapIt->second;
  if (!swap_row_in_data(rowIndex, data.size() - 1)) {
    return false;
  }
  pop_back();
  return true;
}

auto Table::ifKeyExist(const KeyType &key) const -> bool {
  return this->keyMap.contains(key);
}

void Table::insertByIndex(const KeyType &key, std::vector<ValueType> &&data) {
  if (this->keyMap.contains(key)) {
    std::string const err = "In Table \"" + this->tableName + "\" : Key \"" +
                            key + "\" already exists!";
    throw ConflictingKey(err);
  }
  this->keyMap.emplace(key, this->data.size());
  this->data.emplace_back(key, std::move(data));
}

auto Table::findRow(const Table::KeyType &key) -> Table::Object::Ptr {
  auto const iterator = keyMap.find(key);
  if (iterator == keyMap.end()) {
    // not found
    return nullptr;
  }
  return createProxy(
      data.begin() + static_cast<std::vector<Table::Datum>::difference_type>(
                         iterator->second),
      this);
}

auto operator<<(std::ostream &output, const Table &table) -> std::ostream & {
  const int width = 10;
  std::stringstream buffer;
  {
    const std::shared_lock lock(table.dataMutex);
    buffer << table.tableName << "\t" << (table.fields.size() + 1) << "\n";
    buffer << std::setw(width) << "KEY";
    for (const auto &field : table.fields) {
      buffer << ' ' << std::setw(width) << field;
    }
    buffer << "\n";
    auto const numFields = table.fields.size();
    for (const auto &datum : table.data) {
      buffer << std::setw(width) << datum.key;
      for (Table::SizeType i = 0; i < numFields; ++i) {
        buffer << ' ' << std::setw(width) << datum.datum.at(i);
      }
      buffer << "\n";
    }
  }
  return output << buffer.str();
}
