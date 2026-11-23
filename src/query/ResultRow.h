#ifndef SRC_QUERY_RESULTROW_H_
#define SRC_QUERY_RESULTROW_H_

#include <string>
#include <vector>

// Own selected values independently of table storage and execution lifetime.
struct ResultRow {
  std::string key;
  std::vector<int> values;
};

#endif  // SRC_QUERY_RESULTROW_H_
