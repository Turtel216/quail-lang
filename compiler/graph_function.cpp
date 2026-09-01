#include "graph_function.hpp"
#include <queue>

namespace ff {
namespace sem {
std::set<FunctionGraph::edge> FunctionGraph::computeTransitiveEdges() {
  std::set<edge> transitiveEdges;
  transitiveEdges.insert(edges.begin(), edges.end());

  for (auto &connector : this->adjacencyList) {
    for (auto &from : this->adjacencyList) {
      edge toConnector{from.first, connector.first};
      for (auto &to : this->adjacencyList) {
        edge fullJump{from.first, to.first};
        if (transitiveEdges.find(fullJump) != transitiveEdges.end())
          continue;

        edge fromConnector{connector.first, to.first};
        if (transitiveEdges.find(toConnector) != transitiveEdges.end() &&
            transitiveEdges.find(fromConnector) != transitiveEdges.end())
          transitiveEdges.insert(std::move(fullJump));
      }
    }
  }

  return transitiveEdges;
}

void FunctionGraph::createGroups(
    const std::set<edge> &transitiveEdges,
    std::map<function, groupId> &groupIds,
    std::map<groupId, std::shared_ptr<GroupData>> &groupDataMap) {
  groupId idCounter = 0;
  for (auto &vertex : this->adjacencyList) {
    if (groupIds.find(vertex.first) != groupIds.end())
      continue;

    std::shared_ptr<GroupData> newGroup(new GroupData);
    newGroup->functions.insert(vertex.first);
    groupDataMap[idCounter] = newGroup;
    groupIds[vertex.first] = idCounter;

    for (auto &otherVertex : this->adjacencyList) {
      if (transitiveEdges.find({vertex.first, otherVertex.first}) !=
              transitiveEdges.end() &&
          transitiveEdges.find({otherVertex.first, vertex.first}) !=
              transitiveEdges.end()) {
        groupIds[otherVertex.first] = idCounter;
        newGroup->functions.insert(otherVertex.first);
      }
    }
    idCounter++;
  }
}

void FunctionGraph::createEdges(
    std::map<function, groupId> &groupIds,
    std::map<groupId, std::shared_ptr<GroupData>> &groupDataMap) {
  std::set<std::pair<groupId, groupId>> groupEdges;
  for (auto &vertex : this->adjacencyList) {
    auto vertexId = groupIds[vertex.first];
    auto &vertexData = groupDataMap[vertexId];
    for (auto &otherVertex : vertex.second) {
      auto otherId = groupIds[otherVertex];
      if (vertexId == otherId)
        continue;
      if (groupEdges.find({vertexId, otherId}) != groupEdges.end())
        continue;
      groupEdges.insert({vertexId, otherId});
      vertexData->adjacencyList.insert(otherId);
      groupDataMap[otherId]->indegree++;
    }
  }
}

std::vector<std::unique_ptr<Group>> FunctionGraph::generateOrder(
    std::map<function, groupId> &,
    std::map<groupId, std::shared_ptr<GroupData>> &groupDataMap) {
  std::queue<groupId> idQueue;
  std::vector<std::unique_ptr<Group>> output;

  for (auto &group : groupDataMap) {
    if (group.second->indegree == 0)
      idQueue.push(group.first);
  }

  while (!idQueue.empty()) {
    auto newId = idQueue.front();
    auto &groupData = groupDataMap[newId];
    std::unique_ptr<Group> outputGroup(new Group);
    outputGroup->members = std::move(groupData->functions);
    idQueue.pop();

    for (auto &adjacentGroup : groupData->adjacencyList) {
      if (--groupDataMap[adjacentGroup]->indegree == 0)
        idQueue.push(adjacentGroup);
    }

    output.push_back(std::move(outputGroup));
  }

  return output;
}

std::vector<std::unique_ptr<Group>> FunctionGraph::computeOrder() {
  std::set<edge> transitiveEdges = computeTransitiveEdges();
  std::map<function, groupId> groupIds;
  std::map<groupId, std::shared_ptr<GroupData>> groupDataMap;

  createGroups(transitiveEdges, groupIds, groupDataMap);
  createEdges(groupIds, groupDataMap);
  return generateOrder(groupIds, groupDataMap);
}

std::set<function> &FunctionGraph::addFunction(const function &f) {
  auto adjacencyListIt = adjacencyList.find(f);
  if (adjacencyListIt != adjacencyList.end()) {
    return adjacencyListIt->second;
  } else {
    return adjacencyList[f] = {};
  }
}

void FunctionGraph::addEdge(const function &from, const function &to) {
  addFunction(from).insert(to);
  edges.insert({from, to});
}
} // namespace sem
} // namespace ff
