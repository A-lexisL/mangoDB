//
// Created by liu on 18-10-26.
//

#include "QueryResult.h"

#include <ostream>
#include <string>
#include <utility>
#include <vector>

auto operator<<(std::ostream &output, const QueryResult &table)
    -> std::ostream & {
  return table.output(output);
}

DisplayQueryResult::DisplayQueryResult(std::string text)
    : text(std::move(text)) {}

void DisplayQueryResult::outputBeforeId(std::ostream &output) const {
  output << text;
}

AggregateQueryResult::AggregateQueryResult(std::vector<int> results)
    : values(std::move(results)) {}

auto AggregateQueryResult::output(std::ostream &output) const
    -> std::ostream & {
  output << "ANSWER = ( ";
  for (const auto value : values) {
    output << value << ' ';
  }
  return output << ")\n";
}

SelectQueryResult::SelectQueryResult(std::vector<Row> rows)
    : rows(std::move(rows)) {}

auto SelectQueryResult::output(std::ostream &output) const -> std::ostream & {
  for (const auto &row : rows) {
    output << "( " << row.key;
    for (const auto value : row.values) {
      output << ' ' << value;
    }
    output << " )\n";
  }
  return output;
}
