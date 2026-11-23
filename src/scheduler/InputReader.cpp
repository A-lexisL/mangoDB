#include "InputReader.h"

#include <cstddef>
#include <cstdio>
#include <exception>
#include <iostream>
#include <string>
#include <utility>

#include "../query/Query.h"
#include "QueryTypes.h"

namespace {
auto extractQueryString(std::istream &input) -> std::string {
  std::string buf;
  while (true) {
    const int character = input.get();
    if (character == ';') {
      return buf;
    }
    if (character == EOF) {
      throw std::ios_base::failure("End of input");
    }
    buf.push_back(static_cast<char>(character));
  }
}

}  // namespace

auto InputReader::readQueries(std::size_t queryLimit) -> InputBatch {
  InputBatch inputBatch;
  inputBatch.inputClosed = false;
  while (inputBatch.queries.size() < queryLimit) {
    if (!is_) {
      inputBatch.inputClosed = true;
      return inputBatch;
    }
    try {
      const std::string queryStr = extractQueryString(is_);
      Query::Ptr query = parser.parseQuery(queryStr);
      if (query->requestsExit()) {
        inputBatch.inputClosed = true;
        return inputBatch;
      }
      query->setId(++IdCounter);
      inputBatch.queries.push_back(std::move(query));
    } catch (const std::ios_base::failure &e) {
      // End of input
      inputBatch.inputClosed = true;
      return inputBatch;
    } catch (const std::exception &e) {
      std::cout.flush();
      std::cerr << e.what() << '\n';
    }
  }
  return inputBatch;
}
