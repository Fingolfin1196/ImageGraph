#pragma once

#include "../internal/BorrowedPtr.hpp"
#include "../internal/GraphAdaptor.hpp"
#include "../internal/ProtoCache.hpp"
#include "Optimizer.hpp"
#include "nodes/OptimizedOutNode.hpp"
#include "nodes/SinkNode.hpp"
#include <unordered_set>

namespace ImageGraph {
struct MemoryDistribution;

namespace internal {
struct Task;
}

struct NodeGraph {
  using out_nodes_t = std::unordered_set<std::unique_ptr<OutNode>>;
  using sink_nodes_t = std::unordered_set<std::unique_ptr<SinkNode>>;
  using optimizers_t = std::vector<std::unique_ptr<Optimizer>>;
  using duration_t = std::chrono::duration<double>;

private:
  struct PoolID {
    internal::Task& task;
    bool dependency;
    PoolID(internal::Task& task, bool dependency) : task{task}, dependency{dependency} {}
  };

  using pool_t = internal::ThreadPool<PoolID>;
  using pool_deque_t = std::deque<PoolID>;
  using task_deque_t = std::deque<internal::Task*>;
  using task_dependency_deque_t = std::deque<internal::GraphAdaptor::TaskDependency>;

  bool handleFinished(internal::GraphAdaptor& adaptor, pool_deque_t finished, pool_t& pool);
  bool performSingle(task_dependency_deque_t finished, pool_t& pool);

  enum class RunState { NOT_RUNNING, STOP_RUNNING, RUNNING };

  out_nodes_t out_nodes_{};
  sink_nodes_t sink_nodes_{};
  optimizers_t optimizers_{};
  std::mutex mutex_{};
  std::condition_variable compute_finished_{};
  RunState run_{false};

public:
  NodeGraph() = default;
  ~NodeGraph();

  const out_nodes_t& outNodes() const { return out_nodes_; }
  const sink_nodes_t& sinkNodes() const { return sink_nodes_; }

  void addOutNode(std::unique_ptr<OutNode>&& ptr) { out_nodes_.insert(std::move(ptr)); }
  void addSinkNode(std::unique_ptr<SinkNode>&& ptr) { sink_nodes_.insert(std::move(ptr)); }

  template<typename NodeType, typename... Args> NodeType& createOutNode(Args&&... args) {
    auto ptr{std::make_unique<NodeType, Args...>(std::forward<Args>(args)...)};
    auto& ref{*ptr};
    addOutNode(std::move(ptr));
    return ref;
  }
  template<typename NodeType, typename... Args> NodeType& createSinkNode(Args&&... args) {
    auto ptr{std::make_unique<NodeType, Args...>(std::forward<Args>(args)...)};
    auto& ref{*ptr};
    addSinkNode(std::move(ptr));
    return ref;
  }

  size_t eraseOutNode(OutNode& ref) {
    while (ref.hasParents()) eraseOutNode(*ref.topParent().parent);
    internal::BorrowedPtr<OutNode> ptr{&ref};
    return out_nodes_.erase(ptr);
  }
  size_t eraseSinkNode(SinkNode& ref) {
    internal::BorrowedPtr<SinkNode> ptr{&ref};
    return sink_nodes_.erase(ptr);
  }

  void addOptimizer(std::unique_ptr<Optimizer>&& optimizer) { optimizers_.push_back(std::move(optimizer)); }
  template<typename Optimizer, typename... Args> void createOptimizer(Args&&... args) {
    addOptimizer(std::make_unique<Optimizer>(std::forward<Args>(args)...));
  }

  void optimize() {
    for (const auto& optimizer : optimizers_) (*optimizer)(*this);
  }

  void finish();

  duration_t computationDuration(std::size_t size);
  MemoryDistribution optimizeMemoryDistribution(std::size_t memory_limit) const;
  void compute(MemoryDistribution distribution, std::optional<size_t> opt_thread_num = std::nullopt);
  void compute(std::size_t memory_limit, std::optional<size_t> opt_thread_num = std::nullopt);

  friend std::ostream& operator<<(std::ostream& stream, const NodeGraph& graph) {
    stream << "********************************************************************************\n";
    stream << "* NodeGraph @ " << &graph << "\n";
    stream << "********************************************************************************\n";
    stream << "* out_nodes:\n";
    for (const auto& node : graph.out_nodes_) {
      stream << "* ";
      if (node)
        stream << *node;
      else
        stream << "nullptr";
      stream << "\n";
    }
    stream << "********************************************************************************\n";
    stream << "* sink_nodes:\n";
    for (const auto& node : graph.sink_nodes_) {
      stream << "* ";
      if (node)
        stream << *node;
      else
        stream << "nullptr";
      stream << "\n";
    }
    stream << "********************************************************************************\n";
    stream << "* optimizers: " << graph.optimizers_.size() << "\n";
    stream << "********************************************************************************";
    stream.flush();
    return stream;
  }
};
} // namespace ImageGraph
