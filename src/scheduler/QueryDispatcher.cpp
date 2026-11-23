#include "QueryDispatcher.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../query/Query.h"
#include "QueryTypes.h"

namespace {
void addResource(std::vector<query::Resource> &resources,
                 const query::Resource &resource) {
  const auto existing =
      std::ranges::find_if(resources, [&resource](const auto &access) -> bool {
        return access.kind == resource.kind && access.name == resource.name;
      });
  if (existing == resources.end()) {
    resources.push_back(resource);
  } else if (resource.mode == query::AccessMode::kWrite) {
    existing->mode = resource.mode;
  }
}
}  // namespace

auto QueryDispatcher::getTableNames() const -> std::vector<std::string> {
  const auto &tables = pendingByResource_.at(
      static_cast<std::size_t>(query::ResourceKind::kTable));
  std::vector<std::string> names;
  names.reserve(tables.size());
  std::ranges::transform(
      tables, std::back_inserter(names),
      [](const auto &entry) -> std::string { return entry.first; });
  return names;
}

void QueryDispatcher::addTableResource(const std::string &name) {
  auto &tables = pendingByResource_.at(
      static_cast<std::size_t>(query::ResourceKind::kTable));
  if (!tables.try_emplace(name).second) {
    return;
  }
  for (const auto &[queryId, mode] : namespaceAccesses_) {
    auto &resources = taskResources_.at(queryId).resources;
    const query::Resource resource{
        .kind = query::ResourceKind::kTable, .name = name, .mode = mode};
    addResource(resources, resource);
    indexAccess(queryId, resources.back());
  }
}

void QueryDispatcher::accept(Query::Ptr query) {
  const QueryId queryId = query->getId();
  taskResources_.try_emplace(queryId);
  indexPending(queryId, query->requiredResources());
  pending_.emplace(queryId, std::move(query));
  ready_ = selectReadyTasks({queryId});
}

void QueryDispatcher::indexAccess(QueryId queryId,
                                  const query::Resource &resource) {
  auto &groups = pendingByResource_
                     .at(static_cast<std::size_t>(resource.kind))[resource.name]
                     .groups;
  if (groups.empty() || resource.mode == query::AccessMode::kWrite ||
      groups.back().mode == query::AccessMode::kWrite) {
    groups.emplace_back(resource.mode);
  }
  auto &group = groups.back();
  ++group.unfinished;
  if (groups.size() > 1) {
    ++taskResources_.at(queryId).blockers;
    group.waiting.push_back(queryId);
  }
}

void QueryDispatcher::indexPending(
    QueryId queryId, const std::vector<query::Resource> &inputResources) {
  std::vector<query::Resource> normalized;
  for (const auto &resource : inputResources) {
    if (resource.kind == query::ResourceKind::kTableNamespace) {
      namespaceAccesses_[queryId] = resource.mode;
      const auto names = getTableNames();
      std::ranges::transform(names, std::back_inserter(normalized),
                             [&resource](const auto &name) -> query::Resource {
                               return query::Resource{
                                   .kind = query::ResourceKind::kTable,
                                   .name = name,
                                   .mode = resource.mode,
                               };
                             });
      continue;
    }
    if (resource.kind == query::ResourceKind::kTable) {
      addTableResource(resource.name);
    }
    addResource(normalized, resource);
  }
  auto &resources = taskResources_.at(queryId).resources;
  resources = std::move(normalized);
  for (const auto &resource : resources) {
    indexAccess(queryId, resource);
  }
}

auto QueryDispatcher::unindexPending(
    const std::vector<query::Resource> &resources) -> std::vector<QueryId> {
  std::vector<QueryId> candidates;
  for (const auto &resource : resources) {
    auto &byName =
        pendingByResource_.at(static_cast<std::size_t>(resource.kind));
    const auto indexed = byName.find(resource.name);
    auto &groups = indexed->second.groups;
    if (--groups.front().unfinished != 0) {
      continue;
    }
    groups.pop_front();
    if (!groups.empty()) {
      const auto &waiting = groups.front().waiting;
      std::ranges::copy_if(waiting, std::back_inserter(candidates),
                           [this](QueryId waitingId) -> bool {
                             return --taskResources_.at(waitingId).blockers ==
                                    0;
                           });
      groups.front().waiting.clear();
    } else if (!indexed->second.exists) {
      byName.erase(indexed);
    }
  }
  return candidates;
}

auto QueryDispatcher::takeReadyQueries() -> std::vector<Query::Ptr> {
  std::vector<Query::Ptr> ready;
  ready.swap(ready_);
  return ready;
}

auto QueryDispatcher::selectReadyTasks(std::vector<QueryId> candidates)
    -> std::vector<Query::Ptr> {
  std::vector<Query::Ptr> ready = std::move(ready_);
  std::ranges::sort(candidates);
  for (const QueryId queryId : candidates) {
    if (taskResources_.at(queryId).blockers != 0) {
      continue;
    }
    auto const selected = pending_.find(queryId);
    ready.push_back(std::move(selected->second));
    pending_.erase(selected);
  }
  return ready;
}

void QueryDispatcher::complete(QueryId queryId,
                               const std::optional<TableChange> &tableChange) {
  const auto task = taskResources_.find(queryId);
  if (task == taskResources_.end() || pending_.contains(queryId)) {
    return;
  }
  if (tableChange) {
    auto &tables = pendingByResource_.at(
        static_cast<std::size_t>(query::ResourceKind::kTable));
    tables[tableChange->tableName].exists = tableChange->exists;
  }
  const auto resources = std::move(task->second.resources);
  auto candidates = unindexPending(resources);
  taskResources_.erase(task);
  namespaceAccesses_.erase(queryId);
  ready_ = selectReadyTasks(std::move(candidates));
}

auto QueryDispatcher::drained() const -> bool { return taskResources_.empty(); }
