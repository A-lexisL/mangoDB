//
// Created by liu on 18-10-25.
//

#ifndef SRC_QUERY_QUERY_H_
#define SRC_QUERY_QUERY_H_

#include "../db/Table.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace query {

// kTableNamespace conflicts with every table, including tables created later.
enum class ResourceKind : std::uint8_t { kTable, kFile, kTableNamespace };

enum class AccessMode : std::uint8_t { kRead, kWrite };

struct Resource {
  ResourceKind kind;
  std::string name;
  AccessMode mode = AccessMode::kWrite;
};

}  // namespace query

struct TableChange {
  std::string tableName;
  bool exists = false;
};

struct QueryCondition {
  std::string field;
  std::size_t fieldId = 0;
  std::string op;
  std::function<bool(const Table::ValueType &, const Table::ValueType &)> comp;
  std::string value;
  Table::ValueType valueParsed = 0;
};

struct SplitConfig {
  // Range size applies only when splitting is enabled.
  std::size_t rowsPerTask;
  bool enabled;
};
struct TaskPlan;

class Query {
private:
  std::string targetTable;
  std::size_t id = 0;

public:
  using Id = std::size_t;

  Query() = default;

  explicit Query(std::string targetTable)
      : targetTable(std::move(targetTable)) {}

  using Ptr = std::unique_ptr<Query>;

  void setId(Id queryId) { id = queryId; }

  [[nodiscard]] auto getId() const -> Id { return id; }

  [[nodiscard]] virtual auto requiredResources() const
      -> std::vector<query::Resource> {
    if (targetTable.empty()) {
      return {};
    }
    return {{.kind = query::ResourceKind::kTable, .name = targetTable}};
  }

  // Build execution units, or return an immediate result if no work is needed.
  virtual auto split(const SplitConfig &config) -> TaskPlan = 0;

  [[nodiscard]] auto getTargetTable() const -> const std::string & {
    return targetTable;
  }

  // Describe the catalog change associated with a successful operation.
  [[nodiscard]] virtual auto tableChange() const -> std::optional<TableChange> {
    return std::nullopt;
  }

  [[nodiscard]] virtual auto requestsExit() const -> bool { return false; }

  virtual auto toString() -> std::string = 0;

  Query(const Query &) = default;
  auto operator=(const Query &) -> Query & = default;
  Query(Query &&) = default;
  auto operator=(Query &&) -> Query & = default;
  virtual ~Query() = default;
};

class NopQuery : public Query {
public:
  [[nodiscard]] auto requiredResources() const
      -> std::vector<query::Resource> override {
    return {};
  }

  auto split(const SplitConfig &config) -> TaskPlan override;

  auto toString() -> std::string override { return "QUERY = NOOP"; }
};

class ComplexQuery : public Query {
private:
  /** The field names in the first () */
  std::vector<std::string> operands;
  /** The function used in where clause, excluding KEY */
  std::vector<QueryCondition> condition;
  /** The KEY equality condition, or nullptr if none. */
  std::unique_ptr<std::string> keyCondition;

protected:
  // Resolve field indices and extract KEY; return false for contradictory keys.
  auto initCondition(const Table &table) -> bool;

public:
  ComplexQuery(std::string targetTable, std::vector<std::string> operands,
               std::vector<QueryCondition> condition)
      : Query(std::move(targetTable)), operands(std::move(operands)),
        condition(std::move(condition)) {}

  [[nodiscard]] auto getKeyCondition() const -> const std::string * {
    return keyCondition.get();
  }

  /** Get operands in the query */
  [[nodiscard]] auto getOperands() const -> const std::vector<std::string> & {
    return operands;
  }

  // Read parsed conditions without modifying the Query.
  [[nodiscard]] auto getCondition() const
      -> const std::vector<QueryCondition> & {
    return condition;
  }
};

#endif  // SRC_QUERY_QUERY_H_
