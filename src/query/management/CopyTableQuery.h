#ifndef SRC_QUERY_MANAGEMENT_COPYTABLEQUERY_H_
#define SRC_QUERY_MANAGEMENT_COPYTABLEQUERY_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../Query.h"

class CopyTableQuery : public Query {
  std::string original;

public:
  // Read the source table name owned by this Query.
  [[nodiscard]] auto getOriginal() const -> const std::string & {
    return original;
  }

  CopyTableQuery(std::string original_table, std::string newtable)
      : Query(std::move(newtable)), original(std::move(original_table)) {}

  [[nodiscard]] auto requiredResources() const
      -> std::vector<query::Resource> override {
    return {
        {.kind = query::ResourceKind::kTable,
         .name = original,
         .mode = query::AccessMode::kRead},
        {.kind = query::ResourceKind::kTable,
         .name = getTargetTable(),
         .mode = query::AccessMode::kWrite},
    };
  }

  // Construct one complete-operation Task.
  auto split(const SplitConfig &config) -> TaskPlan override;

  [[nodiscard]] auto tableChange() const
      -> std::optional<TableChange> override {
    return TableChange{.tableName = getTargetTable(), .exists = true};
  }

  auto toString() -> std::string override;
};

#endif  // SRC_QUERY_MANAGEMENT_COPYTABLEQUERY_H_
