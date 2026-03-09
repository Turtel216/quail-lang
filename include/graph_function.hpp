#pragma once
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ff {
namespace sem {

using function = std::string;
using groupId = std::size_t;

class Group {
public:
  std::set<function> members;
};

class FunctionGraph {
private:
  struct GroupData {
    std::set<function> functions;
    std::set<groupId> adjacencyList;
    std::size_t indegree;
  };

  using edge = std::pair<function, function>;
  using groupEdge = std::pair<groupId, groupId>;

  std::map<function, std::set<function>> adjacencyList;
  std::set<edge> edges;

  std::set<edge> computeTransitiveEdges();
  void createGroups(const std::set<edge> &, std::map<function, groupId> &,
                    std::map<groupId, std::shared_ptr<GroupData>> &);
  void createEdges(std::map<function, groupId> &,
                   std::map<groupId, std::shared_ptr<GroupData>> &);
  std::vector<std::unique_ptr<Group>>
  generateOrder(std::map<function, groupId> &,
                std::map<groupId, std::shared_ptr<GroupData>> &);

public:
  std::set<function> &addFunction(const function &f);
  void addEdge(const function &from, const function &to);
  std::vector<std::unique_ptr<Group>> computeOrder();
};

} // namespace sem
} // namespace ff
