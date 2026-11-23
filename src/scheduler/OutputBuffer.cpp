#include "OutputBuffer.h"

#include <cstddef>
#include <iostream>

#include "../query/QueryResult.h"

namespace {
void printResult(const std::size_t queryId, const QueryResult::Ptr &result) {
  result->outputBeforeId(std::cout);
  std::cout << queryId << "\n";
  if (result->success()) {
    if (result->display()) {
      std::cout << *result;
    }
  } else {
    std::cout.flush();
    std::cerr << "QUERY FAILED:\n\t" << *result;
  }
}
}  // namespace

void OutputBuffer::flushConsecutive() {
  while (resultBuffer.contains(nextPrintableId)) {
    printResult(nextPrintableId, resultBuffer[nextPrintableId]);
    resultBuffer.erase(nextPrintableId);
    ++nextPrintableId;
  }
}
