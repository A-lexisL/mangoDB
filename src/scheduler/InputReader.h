#ifndef SRC_SCHEDULER_INPUTREADER_H_
#define SRC_SCHEDULER_INPUTREADER_H_

#include <cstddef>
#include <istream>
#include <memory>

#include "../query/QueryBuilders.h"
#include "../query/QueryParser.h"
#include "QueryTypes.h"

class InputReader {
public:
  // Read from the supplied stream buffer and register the Query builders.
  explicit InputReader(const std::istream &input) : is_(input.rdbuf()) {
    parser.registerQueryBuilder(std::make_unique<QueryBuilder(Debug)>());
    parser.registerQueryBuilder(std::make_unique<QueryBuilder(ManageTable)>());
    parser.registerQueryBuilder(std::make_unique<QueryBuilder(Complex)>());
  }
  InputReader(const InputReader &) = delete;
  auto operator=(const InputReader &) -> InputReader & = delete;
  InputReader(InputReader &&) = delete;
  auto operator=(InputReader &&) -> InputReader & = delete;
  ~InputReader() = default;
  // Parse up to queryLimit Queries and report whether EOF or QUIT closed input.
  auto readQueries(std::size_t queryLimit) -> InputBatch;

private:
  std::size_t IdCounter = 0;
  std::istream is_;
  QueryParser parser;
};

#endif  // SRC_SCHEDULER_INPUTREADER_H_
