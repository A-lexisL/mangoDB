//
// Created by liu on 18-10-25.
//

#ifndef SRC_QUERY_QUERYBUILDERS_H_
#define SRC_QUERY_QUERYBUILDERS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../utils/uexception.h"
#include "Query.h"
#include "QueryParser.h"

#define QueryBuilder(name) name##QueryBuilder

// These macros declare builder classes; a constexpr function cannot replace
// them.

// NOLINTNEXTLINE
#define QueryBuilderClass(name)                                                \
  class QueryBuilder(name) : public QueryBuilder {                             \
  public:                                                                      \
    auto tryExtractQuery(TokenizedQueryString &query) -> Query::Ptr override;  \
  }

// NOLINTNEXTLINE
#define BasicQueryBuilderClass(name)                                           \
  class QueryBuilder(name) : public BasicQueryBuilder {                        \
  public:                                                                      \
    auto tryExtractQuery(TokenizedQueryString &query) -> Query::Ptr override;  \
  }

// NOLINTNEXTLINE
#define ComplexQueryBuilderClass(name)                                         \
  class QueryBuilder(name) : public ComplexQueryBuilder {                      \
  public:                                                                      \
    auto tryExtractQuery(TokenizedQueryString &query) -> Query::Ptr override;  \
  }

class FailedQueryBuilder : public QueryBuilder {
public:
  static auto getDefault() -> QueryBuilder::Ptr {
    return std::make_unique<FailedQueryBuilder>();
  }

  auto tryExtractQuery(TokenizedQueryString &query) -> Query::Ptr final {
    throw QueryBuilderMatchFailed(query.rawQeuryString);
  }

  // The terminal builder deliberately ignores successors.

  // NOLINTNEXTLINE
  void setNext(QueryBuilder::Ptr && /*builder*/) final {}

  void clear() override {}

  FailedQueryBuilder() = default;
  FailedQueryBuilder(const FailedQueryBuilder &) = default;
  auto operator=(const FailedQueryBuilder &) -> FailedQueryBuilder & = default;
  FailedQueryBuilder(FailedQueryBuilder &&) = default;
  auto operator=(FailedQueryBuilder &&) -> FailedQueryBuilder & = default;
  ~FailedQueryBuilder() override = default;
};

class BasicQueryBuilder : public QueryBuilder {
private:
  QueryBuilder::Ptr nextBuilder;

protected:
  [[nodiscard]] auto getNextBuilder() const -> QueryBuilder & {
    return *nextBuilder;
  }

public:
  void setNext(Ptr &&builder) override { nextBuilder = std::move(builder); }

  auto tryExtractQuery(TokenizedQueryString &query) -> Query::Ptr override {
    return nextBuilder->tryExtractQuery(query);
  }

  BasicQueryBuilder() : nextBuilder(FailedQueryBuilder::getDefault()) {}

  void clear() override { nextBuilder->clear(); }

  BasicQueryBuilder(const BasicQueryBuilder &) = delete;
  auto operator=(const BasicQueryBuilder &) -> BasicQueryBuilder & = delete;
  BasicQueryBuilder(BasicQueryBuilder &&) = delete;
  auto operator=(BasicQueryBuilder &&) -> BasicQueryBuilder & = delete;
  ~BasicQueryBuilder() override = default;
};

class ComplexQueryBuilder : public BasicQueryBuilder {
  using TokenIterator = std::vector<std::string>::const_iterator;

  void parseConditions(TokenIterator iterator, TokenIterator end);
  void printDebug(const TokenizedQueryString &query) const;

private:
  std::string targetTable;
  std::vector<std::string> operandToken;
  std::vector<QueryCondition> conditionToken;

protected:
  virtual void parseToken(TokenizedQueryString &query);

public:
  void clear() override;

  // Used as a debugging function.
  // Prints the parsed information
  auto tryExtractQuery(TokenizedQueryString &query) -> Query::Ptr override;
};

// Transparant builder
// It does not modify or extract anything
// It prints current tokenized string
// Use to examine the queries and tokenizer
BasicQueryBuilderClass(Fake);

// Debug commands / Utils
BasicQueryBuilderClass(Debug);

// Load, dump, truncate and delete table
BasicQueryBuilderClass(ManageTable);

// ComplexQueryBuilderClass(UpdateTable);
// ComplexQueryBuilderClass(Insert);
// ComplexQueryBuilderClass(Delete);

#endif  // SRC_QUERY_QUERYBUILDERS_H_
