//
// Created by liu on 18-10-25.
//

#include "QueryBuilders.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "../db/Database.h"
#include "../utils/formatter.h"
#include "../utils/uexception.h"
#include "Query.h"
#include "QueryParser.h"
#include "data/DeleteQuery.h"
#include "data/DuplicateQuery.h"
#include "data/InsertQuery.h"
#include "data/MaxQuery.h"
#include "data/SelectQuery.h"
#include "data/SubQuery.h"
#include "data/SumQuery.h"
#include "data/SwapQuery.h"
#include "data/UpdateQuery.h"
#include "management/CopyTableQuery.h"
#include "management/DropTableQuery.h"
#include "management/DumpTableQuery.h"
#include "management/ListTableQuery.h"
#include "management/LoadTableQuery.h"
#include "management/PrintTableQuery.h"
#include "management/QuitQuery.h"
#include "management/TruncateQuery.h"

namespace {
using TokenIterator = std::vector<std::string>::const_iterator;

auto advanceToken(TokenIterator current, const TokenIterator end,
                  const char *message) -> TokenIterator {
  ++current;
  if (current == end) {
    throw IllFormedQuery(message);
  }
  return current;
}
}  // namespace

// Prints out debugging information.
// Does no real work
auto FakeQueryBuilder::tryExtractQuery(TokenizedQueryString &query)
    -> Query::Ptr {
  std::cerr << "Query string: \n" << query.rawQeuryString << "\n";
  std::cerr << "Tokens:\n";
  constexpr int kTokenWidth = 10;
  constexpr int kColumns = 5;
  int count = 0;
  for (const auto &tok : query.token) {
    std::cerr << std::setw(kTokenWidth) << "\"" << tok << "\"";
    count = (count + 1) % kColumns;
    if (count == kColumns - 1) {
      std::cerr << '\n';
    }
  }
  if (count != kColumns - 1) {
    std::cerr << '\n';
  }
  return getNextBuilder().tryExtractQuery(query);
}

auto ManageTableQueryBuilder::tryExtractQuery(TokenizedQueryString &query)
    -> Query::Ptr {
  if (query.token.size() == 2) {
    if (query.token.front() == "LOAD") {
      auto &database = Database::getInstance();
      auto const tableName = database.getFileTableName(query.token.at(1));
      return std::make_unique<LoadTableQuery>(tableName, query.token.at(1));
    }
    if (query.token.front() == "DROP") {
      return std::make_unique<DropTableQuery>(query.token.at(1));
    }
    if (query.token.front() == "TRUNCATE") {
      return std::make_unique<TruncateQuery>(query.token.at(1));
    }
  }
  if (query.token.size() == 3) {
    if (query.token.front() == "DUMP") {
      auto &database = Database::getInstance();
      database.updateFileTableName(query.token.at(2), query.token.at(1));
      return std::make_unique<DumpTableQuery>(query.token.at(1),
                                              query.token.at(2));
    }
    if (query.token.front() == "COPYTABLE") {
      return std::make_unique<CopyTableQuery>(query.token.at(1),
                                              query.token.at(2));
    }
  }
  return getNextBuilder().tryExtractQuery(query);
}

auto DebugQueryBuilder::tryExtractQuery(TokenizedQueryString &query)
    -> Query::Ptr {
  if (query.token.size() == 1) {
    if (query.token.front() == "LIST") {
      return std::make_unique<ListTableQuery>();
    }
    if (query.token.front() == "QUIT") {
      return std::make_unique<QuitQuery>();
    }
  }
  if ((query.token.size() == 2) && (query.token.front() == "SHOWTABLE")) {
    return std::make_unique<PrintTableQuery>(query.token.at(1));
  }

  return BasicQueryBuilder::tryExtractQuery(query);
}

void ComplexQueryBuilder::parseToken(TokenizedQueryString &query) {
  // Treats forms like:
  //
  // $OPER$ ( arg1 arg2 ... )
  // FROM table
  // WHERE ( KEY = $STR$ ) ( $field$ $OP$ $int$ ) ...
  //
  // The "WHERE" clause can be ommitted
  // The args of OPER clause can be ommitted

  auto iterator = query.token.cbegin();
  const auto end = query.token.cend();
  iterator += 1;  // Take to args;
  if (iterator == query.token.end()) {
    throw IllFormedQuery("Missing FROM clause");
  }
  if (*iterator != "FROM") {
    if (*iterator != "(") {
      throw IllFormedQuery("Ill-formed operand.");
    }
    iterator = advanceToken(iterator, end, "Ill-formed operand");
    while (*iterator != ")") {
      this->operandToken.push_back(*iterator);
      iterator = advanceToken(iterator, end, "Ill-formed operand");
    }
    iterator = advanceToken(iterator, end, "Missing FROM clause");
    if (*iterator != "FROM") {
      throw IllFormedQuery("Missing FROM clause");
    }
  }
  iterator = advanceToken(iterator, end, "Missing targed table");
  this->targetTable = *iterator;
  ++iterator;
  if (iterator == end) {  // the "WHERE" clause is ommitted
    return;
  }
  parseConditions(iterator, end);
}

void ComplexQueryBuilder::parseConditions(TokenIterator iterator,
                                          const TokenIterator end) {
  if (*iterator != "WHERE") {
    // Hmmm, C++11 style Raw-string literal
    // Reference:
    // http://en.cppreference.com/w/cpp/language/string_literal
    throw IllFormedQuery(R"(Expecting "WHERE", found "?".)"_f % *iterator);
  }
  ++iterator;
  while (iterator != end) {
    if (*iterator != "(") {
      throw IllFormedQuery("Ill-formed query condition");
    }
    QueryCondition cond;
    iterator = advanceToken(iterator, end, "Missing field in condition");
    cond.field = *iterator;
    iterator = advanceToken(iterator, end, "Missing operator in condition");
    cond.op = *iterator;
    iterator = advanceToken(iterator, end, "Missing  in condition");
    cond.value = *iterator;
    iterator = advanceToken(iterator, end, "Ill-formed query condition");
    if (*iterator != ")") {
      throw IllFormedQuery("Ill-formed query condition");
    }
    this->conditionToken.push_back(cond);
    ++iterator;
  }
}

auto ComplexQueryBuilder::tryExtractQuery(TokenizedQueryString &query)
    -> Query::Ptr {
  try {
    this->parseToken(query);
  } catch (const IllFormedQuery &e) {
    std::cerr << e.what() << '\n';
    return getNextBuilder().tryExtractQuery(query);
  }
  std::string const operation = query.token.front();
  if (operation == "INSERT") {
    return std::make_unique<InsertQuery>(this->targetTable, this->operandToken,
                                         this->conditionToken);
  }
  if (operation == "SELECT") {
    return std::make_unique<SelectQuery>(this->targetTable, this->operandToken,
                                         this->conditionToken);
  }
  if (operation == "SUB") {
    return std::make_unique<SubQuery>(this->targetTable, this->operandToken,
                                      this->conditionToken);
  }
  if (operation == "SWAP") {
    return std::make_unique<SwapQuery>(this->targetTable, this->operandToken,
                                       this->conditionToken);
  }
  if (operation == "UPDATE") {
    return std::make_unique<UpdateQuery>(this->targetTable, this->operandToken,
                                         this->conditionToken);
  }
  if (operation == "DUPLICATE") {
    return std::make_unique<DuplicateQuery>(
        this->targetTable, this->operandToken, this->conditionToken);
  }
  if (operation == "DELETE") {
    return std::make_unique<DeleteQuery>(this->targetTable, this->operandToken,
                                         this->conditionToken);
  }
  if (operation == "SUM") {
    return std::make_unique<SumQuery>(this->targetTable, this->operandToken,
                                      this->conditionToken);
  }
  if (operation == "MAX") {
    return std::make_unique<MaxQuery>(this->targetTable, this->operandToken,
                                      this->conditionToken);
  }
  constexpr std::array<const char *, 3> pendingOperations = {
      "COUNT",
      "MIN",
      "ADD",
  };
  if (std::ranges::find(pendingOperations, operation) !=
      pendingOperations.end()) {
    return std::make_unique<NopQuery>();  // Not implemented
  }
  printDebug(query);
  return getNextBuilder().tryExtractQuery(query);
}

void ComplexQueryBuilder::printDebug(const TokenizedQueryString &query) const {
  std::cerr << "Complicated query found!" << '\n';
  std::cerr << "Operation = " << query.token.front() << '\n';
  std::cerr << "    Operands : ";
  for (const auto &oprand : this->operandToken) {
    std::cerr << oprand << " ";
  }
  std::cerr << '\n';
  std::cerr << "Target Table = " << this->targetTable << '\n';
  if (this->conditionToken.empty()) {
    std::cerr << "No WHERE clause specified." << '\n';
  } else {
    std::cerr << "Conditions = ";
  }
  for (const auto &cond : this->conditionToken) {
    std::cerr << cond.field << cond.op << cond.value << " ";
  }
  std::cerr << '\n';
}

void ComplexQueryBuilder::clear() {
  this->conditionToken.clear();
  this->targetTable = "";
  this->operandToken.clear();
  getNextBuilder().clear();
}
