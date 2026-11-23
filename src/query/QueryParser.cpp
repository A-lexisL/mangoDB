#include "QueryParser.h"

#include <sstream>
#include <string>
#include <utility>

#include "../utils/uexception.h"
#include "Query.h"
#include "QueryBuilders.h"

QueryParser::QueryParser() : first(nullptr) {}

auto QueryParser::parseQuery(const std::string &queryString) -> Query::Ptr {
  if (first == nullptr) {
    throw QueryBuilderMatchFailed(queryString);
  }
  auto tokens = tokenizeQueryString(queryString);
  if (tokens.token.empty()) {
    throw QueryBuilderMatchFailed("");
  }
  first->clear();
  return first->tryExtractQuery(tokens);
}

void QueryParser::registerQueryBuilder(QueryBuilder::Ptr &&qBuilder) {
  if (first == nullptr) {
    first = std::move(qBuilder);
    last = first.get();
    last->setNext(FailedQueryBuilder::getDefault());
  } else {
    QueryBuilder *temp = last;
    last = qBuilder.get();
    temp->setNext(std::move(qBuilder));
    last->setNext(FailedQueryBuilder::getDefault());
  }
}

auto QueryParser::tokenizeQueryString(const std::string &queryString)
    -> TokenizedQueryString {
  TokenizedQueryString tokens;
  tokens.rawQeuryString = queryString;
  std::stringstream stream;
  stream.str(queryString);
  std::string tStr;
  while (stream >> tStr) {
    tokens.token.push_back(tStr);
  }
  return tokens;
}
