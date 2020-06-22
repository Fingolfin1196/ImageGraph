#pragma once

#include "../NodeGraph.hpp"
#include "../nodes/impl/LUTCombinator.hpp"
#include <tuple>

namespace ImageGraph::optimizers {
template<typename SupportedTypes> class LUTOptimizer : public Optimizer {
  struct Selector {
    template<typename T> constexpr static inline bool is_supported{
        internal::node_traits<internal::lut_combinator_tag>::is_input_type_supported<T>};
  };

  using luttable_t = typename SupportedTypes::template selected_t<Selector>;
  using sink_nodes_t = NodeGraph::sink_nodes_t;
  using l_u_t_nodes_t = std::unordered_set<std::shared_ptr<LUTOutNode>>;
  using optimized_t = std::unordered_set<std::unique_ptr<OptimizedOutNode>>;

  struct CurrentDFSPath {
    /**
     * Stores the nodes in reverse order, i.e. the first node in the chain comes last.
     */
    std::vector<LUTOutNode*> nodes;

    CurrentDFSPath(std::vector<LUTOutNode*>&& nodes) : nodes{std::move(nodes)} {}
  };

  struct CallHelper {
    template<typename OutputType, typename InputType>
    static inline std::unique_ptr<OptimizedOutNode> perform(CurrentDFSPath& path) {
      return std::make_unique<nodes::LUTCombinatorNode<OutputType, InputType>>(
          dynamic_cast<LUTInputOutNode<InputType>&>(*path.nodes.back()),
          std::unordered_set<OutNode*>(path.nodes.begin(), path.nodes.end()),
          dynamic_cast<LUTOutputNode<OutputType>&>(*path.nodes.front()));
    }
  };

  struct LuttableCallable {
    template<typename InputType> constexpr static inline bool perform() { return internal::is_luttable_v<InputType>; }
  };

  /**
   * @return true if the path has been continued by node, false if not. If false, the path has to be
   * added to an optimizer by the caller!
   */
  inline bool dfs_inner(OutNode& node, optimized_t& optimized_set, CurrentDFSPath* path) const {
    auto l_u_t{dynamic_cast<LUTOutNode*>(&node)};
    if (l_u_t) {
      DEBUG_ASSERT(std::runtime_error, l_u_t->inputCount() == 1,
                   "A LUT node has to have an output and precisely one input!");

      // A path can either be continued or newly constructed
      CurrentDFSPath* new_path;
      bool continued;
      if (path and node.successorCount() == 1) {
        // The path exists and can be continued.
        new_path = path;
        new_path->nodes.emplace_back(l_u_t);
        continued = true;
      } else {
        // The path does not exists, but a new one can be created.
        new_path = new CurrentDFSPath({l_u_t});
        continued = false;
      }
      if (!dfs_inner(node.inputNode(0).outputNode(), optimized_set, new_path)) {
        auto l_u_t_path{*new_path};
        std::cout << "Working on path with length " << l_u_t_path.nodes.size() << ":" << std::endl;
        while (not l_u_t_path.nodes.empty() and not SupportedTypes::template perform<LuttableCallable, bool>(
                                                        l_u_t_path.nodes.back()->inputNode(0).outputType())
                                                        .value()) {
          l_u_t_path.nodes.pop_back();
          std::cout << "Cut!" << std::endl;
        }
        if (!l_u_t_path.nodes.empty()) {
          std::cout << "Remaining: " << l_u_t_path.nodes.size() << std::endl;
          optimized_set.emplace(
              internal::TypeListList<SupportedTypes, luttable_t>::template perform<CallHelper,
                                                                                   std::unique_ptr<OptimizedOutNode>>(
                  std::tie(l_u_t_path.nodes.front()->outputType(), l_u_t_path.nodes.back()->inputNode(0).outputType()),
                  l_u_t_path)
                  .value());
        } else
          std::cout << "All cut!" << std::endl;
      }
      if (continued)
        new_path->nodes.pop_back();
      else
        delete new_path;
      return continued;
    } else {
      // The node is not a LUT node at all, i.e. any potential path cannot be continued and an empty
      // path is handed down.
      const size_t input_count{node.inputCount()};
      for (size_t i{0}; i < input_count; ++i) dfs_inner(node.inputNode(i).outputNode(), optimized_set, {});
      return false;
    }
  }
  inline optimized_t dfs(const sink_nodes_t& sink_nodes) const {
    optimized_t optimized_set{};
    for (const auto& node : sink_nodes) {
      CurrentDFSPath path{{}};
      const auto input_count{node->inputCount()};
      for (size_t i{0}; i < input_count; ++i) dfs_inner(node->inputNode(i), optimized_set, &path);
      std::cout << "path length: " << path.nodes.size() << std::endl;
    }
    return optimized_set;
  }

public:
  void operator()(NodeGraph& graph) const final {
    const sink_nodes_t& sink_nodes{graph.sinkNodes()};

    auto optimized_set{dfs(sink_nodes)};

    std::cout << "sink_nodes: {" << std::endl;
    for (const auto& node : sink_nodes) std::cout << "  " << *node << std::endl;
    std::cout << "}" << std::endl;

    std::cout << "optimized_set: {" << std::endl;
    for (const auto& node : optimized_set) std::cout << "  " << *node << std::endl;
    std::cout << "}" << std::endl;

    while (not optimized_set.empty()) graph.addOutNode(std::move(optimized_set.extract(optimized_set.begin()).value()));

    std::cout << "graph.outNodes(): {" << std::endl;
    const auto& out_nodes{graph.outNodes()};
    for (const auto& node : out_nodes)
      std::cout << "  [" << *node << ", " << node->successorCount() << "]" << std::endl;
    std::cout << "}" << std::endl;
  }
};
} // namespace ImageGraph::optimizers
