#pragma once

#include "../core/Rectangle.hpp"
#include "../core/nodes/Node.hpp"
#include "ThreadPool.hpp"
#include <boost/container_hash/hash.hpp>
#include <future>
#include <unordered_set>

namespace ImageGraph {
struct Node;
}
namespace ImageGraph::internal {
struct GraphAdaptor;

struct Task {
  using rectangle_t = Rectangle<std::size_t>;
  struct RequiredTaskInfo {
    const Node& node;
    rectangle_t rectangle;
    RequiredTaskInfo(const Node& node, rectangle_t rectangle) : node{node}, rectangle{std::move(rectangle)} {}
  };

private:
  std::deque<Task*> dependants_{};
  std::size_t task_counter_{0};

protected:
  GraphAdaptor& adaptor_;
  const rectangle_t region_;

  std::size_t taskCounter() const { return task_counter_; }

  virtual void performSingleImpl(const Node& node, rectangle_t rectangle) = 0;
  virtual void performFullImpl() = 0;
  /**
   * CAUTION This function has to call adaptor.generateRegion precisely once!
   */
  virtual std::optional<RequiredTaskInfo> generateRequiredTaskImpl() = 0;

  virtual std::ostream& print(std::ostream& stream) const = 0;

public:
  /**
   * CAUTION This must always return the same node!
   */
  virtual const Node& node() const = 0;
  rectangle_t region() const { return region_; }
  const GraphAdaptor& adaptor() const { return adaptor_; }

  void addDependant(Task& task) { dependants_.push_back(&task); }
  std::deque<Task*>& dependants() { return dependants_; }
  const std::deque<Task*>& dependants() const { return dependants_; }

  virtual bool allGenerated() const = 0;
  virtual bool allSinglePerformed() const { return allGenerated() and not task_counter_; }

  /**
   * Calls adaptor.generateRegion precisely once to generate the next required task.
   * CAUTION: This function has to be called sequentially!
   * @throws std::runtime_error If not remaining()!
   */
  void nextRequiredTask();

  /**
   * This function can perform some computations when a required task is finished.
   */
  void performSingle(const Node& node, rectangle_t rectangle) { performSingleImpl(node, std::move(rectangle)); }

  /**
   * This function can perform some computation when all required tasks are finished.
   */
  void performFull() {
    DEBUG_ASSERT(std::runtime_error, allSinglePerformed(), "There are remaining or unfinished tasks!");
    performFullImpl();
  }

  /**
   * CAUTION Has to be called sequentially!
   */
  void singlePerformed() { --task_counter_; }

  Task(GraphAdaptor& adaptor, rectangle_t region) : adaptor_{adaptor}, region_{region} {}
  virtual ~Task() = default;

  bool operator==(const Task& other) const {
    return &adaptor_ == &other.adaptor_ and region_ == other.region_ and &node() == &other.node();
  }

  friend std::ostream& operator<<(std::ostream& stream, const Task& task) { return task.print(stream); }
};

template<typename Out> class TypedTask : public Task {
  std::promise<Out> promise_{};
  std::shared_future<Out> future_{promise_.get_future().share()};

protected:
  void setPromise(Out&& value) { promise_.set_value(std::move(value)); }

public:
  TypedTask(GraphAdaptor& adaptor, rectangle_t region) : Task(adaptor, region) {}

  std::shared_future<Out> future() const { return future_; }
};
} // namespace ImageGraph::internal

namespace std {
template<> struct hash<ImageGraph::internal::Task> {
  std::size_t operator()(const ImageGraph::internal::Task& task) const {
    using namespace ImageGraph;
    using namespace ImageGraph::internal;
    return hash_combine(hash<const Node*>()(&task.node()), hash<Task::rectangle_t>()(task.region()),
                        hash<const GraphAdaptor*>()(&task.adaptor()));
  }
};
} // namespace std

namespace ImageGraph::internal {
namespace detail {
struct TaskHash {
  using is_transparent = void;

  std::size_t operator()(const Task* task) const {
    if (not task) return 0;
    return std::hash<Task>()(*task);
  }
  std::size_t operator()(const std::unique_ptr<Task>& task) const { return (*this)(task.get()); }
};

struct TaskEquals {
  using is_transparent = void;
  using unique_t = std::unique_ptr<Task>;

  bool operator()(const Task* t1, const Task* t2) const {
    if (not t1 or not t2) return not t1 and not t2;
    return *t1 == *t2;
  }
  bool operator()(const unique_t& t1, const unique_t& t2) const { return (*this)(t1.get(), t2.get()); }
  bool operator()(const unique_t& t1, const Task* t2) const { return (*this)(t1.get(), t2); }
  bool operator()(const Task* t1, const unique_t& t2) const { return (*this)(t1, t2.get()); }
};
} // namespace detail

using TaskSet = std::unordered_set<std::unique_ptr<Task>, detail::TaskHash, detail::TaskEquals>;
} // namespace ImageGraph::internal
