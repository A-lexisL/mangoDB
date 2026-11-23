#ifndef SRC_SCHEDULER_QUERYDISPATCHER_H_
#define SRC_SCHEDULER_QUERYDISPATCHER_H_

#include <array>
#include <cstddef>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../query/Query.h"
#include "QueryTypes.h"

class QueryDispatcher {
private:
  class AccessGroup {
    friend class QueryDispatcher;
    query::AccessMode mode = query::AccessMode::kRead;
    std::size_t unfinished = 0;
    std::vector<QueryId> waiting;

  public:
    explicit AccessGroup(query::AccessMode mode) : mode(mode) {}
  };
  // Consecutive readers share one access group; each writer is its own group.

  class TaskState {
    friend class QueryDispatcher;
    std::vector<query::Resource> resources;
    std::size_t blockers = 0;
  };

  class ResourceState {
    friend class QueryDispatcher;
    std::deque<AccessGroup> groups;
    bool exists = false;
  };

  using PendingMap = std::unordered_map<std::string, ResourceState>;

  std::unordered_map<QueryId, TaskState> taskResources_;
  std::unordered_map<QueryId, Query::Ptr> pending_;
  std::vector<Query::Ptr> ready_;
  std::array<PendingMap, 2> pendingByResource_;
  std::map<QueryId, query::AccessMode> namespaceAccesses_;

  auto getTableNames() const -> std::vector<std::string>;
  void addTableResource(const std::string &name);
  void indexAccess(QueryId queryId, const query::Resource &resource);
  void indexPending(QueryId queryId,
                    const std::vector<query::Resource> &inputResources);
  auto unindexPending(const std::vector<query::Resource> &resources)
      -> std::vector<QueryId>;
  auto selectReadyTasks(std::vector<QueryId> candidates)
      -> std::vector<Query::Ptr>;

public:
  QueryDispatcher() = default;
  QueryDispatcher(const QueryDispatcher &) = delete;
  auto operator=(const QueryDispatcher &) -> QueryDispatcher & = delete;
  QueryDispatcher(QueryDispatcher &&) = delete;
  auto operator=(QueryDispatcher &&) -> QueryDispatcher & = delete;
  virtual ~QueryDispatcher() = default;

  void accept(Query::Ptr query);

  auto takeReadyQueries() -> std::vector<Query::Ptr>;

  void complete(QueryId queryId,
                const std::optional<TableChange> &tableChange = std::nullopt);

  auto pendingCount() const -> std::size_t { return pending_.size(); }

  auto readyCount() const -> std::size_t { return ready_.size(); }

  auto runningCount() const -> std::size_t {
    return taskResources_.size() - pendingCount() - readyCount();
  }

  auto outstandingCount() const -> std::size_t { return taskResources_.size(); }

  auto drained() const -> bool;
};

#endif  // SRC_SCHEDULER_QUERYDISPATCHER_H_
