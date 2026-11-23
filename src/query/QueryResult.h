//
// Created by liu on 18-10-25.
//

#ifndef SRC_QUERY_QUERYRESULT_H_
#define SRC_QUERY_QUERYRESULT_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "../utils/formatter.h"
#include "ResultRow.h"

class QueryResult {
public:
  using Ptr = std::unique_ptr<QueryResult>;

  virtual auto success() -> bool = 0;

  virtual auto display() -> bool = 0;

  // LIST and SHOWTABLE historically print their snapshot before the query ID.
  virtual void outputBeforeId(std::ostream & /*output*/) const {}

  QueryResult() = default;
  QueryResult(const QueryResult &) = default;
  auto operator=(const QueryResult &) -> QueryResult & = default;
  QueryResult(QueryResult &&) = default;
  auto operator=(QueryResult &&) -> QueryResult & = default;
  virtual ~QueryResult() = default;

  friend auto operator<<(std::ostream &output, const QueryResult &table)
      -> std::ostream &;

protected:
  virtual auto output(std::ostream &output) const -> std::ostream & = 0;
};

class FailedQueryResult : public QueryResult {
public:
  auto success() -> bool override { return false; }

  auto display() -> bool override { return false; }
};

class SucceededQueryResult : public QueryResult {
public:
  auto success() -> bool override { return true; }
};

class NullQueryResult : public SucceededQueryResult {
public:
  auto display() -> bool override { return false; }

protected:
  auto output(std::ostream &output) const -> std::ostream & override {
    return output;
  }
};

class ErrorMsgResult : public FailedQueryResult {
  std::string msg;

public:
  ErrorMsgResult(const char *qname, const std::string &msg) {
    this->msg = R"(Query "?" failed : ?)"_f % qname % msg;
  }

  ErrorMsgResult(const char *qname, const std::string &table,
                 const std::string &msg) {
    this->msg = R"(Query "?" failed in Table "?" : ?)"_f % qname % table % msg;
  }

protected:
  auto output(std::ostream &output) const -> std::ostream & override {
    return output << msg << "\n";
  }
};

class DisplayQueryResult final : public SucceededQueryResult {
  std::string text;

public:
  explicit DisplayQueryResult(std::string text);

  auto display() -> bool override { return false; }
  void outputBeforeId(std::ostream &output) const override;

protected:
  auto output(std::ostream &output) const -> std::ostream & override {
    return output;
  }
};

class AggregateQueryResult final : public SucceededQueryResult {
  std::vector<int> values;

public:
  explicit AggregateQueryResult(std::vector<int> results);

  auto display() -> bool override { return true; }

protected:
  auto output(std::ostream &output) const -> std::ostream & override;
};

class SelectQueryResult final : public SucceededQueryResult {
public:
  using Row = ResultRow;

private:
  std::vector<Row> rows;

public:
  explicit SelectQueryResult(std::vector<Row> rows);

  auto display() -> bool override { return true; }

protected:
  auto output(std::ostream &output) const -> std::ostream & override;
};

class RecordCountResult : public SucceededQueryResult {
  // The result is an immutable snapshot of the completed query.

  // NOLINTNEXTLINE
  const int affectedRows;

public:
  auto display() -> bool override { return true; }

  explicit RecordCountResult(int count) : affectedRows(count) {}

protected:
  auto output(std::ostream &output) const -> std::ostream & override {
    return output << "Affected ? rows."_f % affectedRows << "\n";
  }
};

#endif  // SRC_QUERY_QUERYRESULT_H_
