//
// Created by liu on 18-10-25.
//

#ifndef SRC_QUERY_MANAGEMENT_LOADTABLEQUERY_H_
#define SRC_QUERY_MANAGEMENT_LOADTABLEQUERY_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../Query.h"

class LoadTableQuery : public Query {
  // This query owns an immutable input/output path.
  const std::string fileName;  // NOLINT

public:
  // Read the input/output path owned by this Query.
  [[nodiscard]] auto getFileName() const -> const std::string & {
    return fileName;
  }

  explicit LoadTableQuery(std::string table, std::string fileName)
      : Query(std::move(table)), fileName(std::move(fileName)) {}

  [[nodiscard]] auto requiredResources() const
      -> std::vector<query::Resource> override {
    return {
        {.kind = query::ResourceKind::kFile,
         .name = fileName,
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

#endif  // SRC_QUERY_MANAGEMENT_LOADTABLEQUERY_H_
