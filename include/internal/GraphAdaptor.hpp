#pragma once

#include "../core/Rectangle.hpp"
#include "../core/Tile.hpp"
#include "../core/nodes/OutputNode.hpp"
#include "Task.hpp"
#include "generators/RelevanceChoice.hpp"
#include <future>

namespace ImageGraph::internal {
struct GraphAdaptor {
  using rectangle_t = Rectangle<std::size_t>;
  template<typename T> using shared_tile_t = std::shared_ptr<Tile<T>>;
  template<typename T> using shared_future_tile_t = std::shared_future<shared_tile_t<T>>;

  struct TaskDependency {
    Task& task;
    const Node& dependency;
    rectangle_t rectangle;
    TaskDependency(Task& task, const Node& dependency, rectangle_t rectangle)
        : task{task}, dependency{dependency}, rectangle{std::move(rectangle)} {}
  };
  using tasks_t = std::deque<Task*>;
  using task_dependencies_t = std::deque<TaskDependency>;

  enum class TaskMode { OUT_REQUESTABLE, SINK_REQUESTABLE, REQUESTED, PERFORMABLE };

private:
  TaskSet set_{};
  tasks_t out_requestable_{}, requested_{}, performable_{};
  task_dependencies_t finished_{};
  TaskRelevanceChoiceGenerator chooser_{};

  template<typename C, typename T> static bool isIn(const C& container, const T& value) {
    return std::find(std::cbegin(container), std::cend(container), value) != std::cend(container);
  }

  void taskModified(Task& task, const TaskMode mode);

public:
  template<typename T> struct GeneratedTile {
    shared_future_tile_t<T> future_tile;
    const bool finished;
  };

  class TaskWrapper {
    friend struct GraphAdaptor;

    GraphAdaptor& adaptor;
    Task& task;
    TaskMode mode;
    TaskWrapper(GraphAdaptor& adaptor, Task& task, TaskMode mode) : adaptor{adaptor}, task{task}, mode{mode} {}

  public:
    void nextRequiredTask() { task.nextRequiredTask(); }
    ~TaskWrapper() { adaptor.taskModified(task, mode); }
  };

  bool empty() const { return set_.empty(); }
  bool emptyPerformable() const { return performable_.empty(); }
  bool emptyRequestable() const { return out_requestable_.empty() and chooser_.empty(); }

  TaskWrapper frontRequestable() {
    if (not out_requestable_.empty())
      return {*this, *out_requestable_.front(), TaskMode::OUT_REQUESTABLE};
    else if (chooser_)
      return {*this, *chooser_.generate(), TaskMode::SINK_REQUESTABLE};
    throw std::runtime_error("Invalid request!");
  }
  Task* extractPerformable() {
    Task* task{performable_.front()};
    performable_.pop_front();
    return task;
  }

  void singlePerformed(Task& task);
  void finished(Task& task);

  /**
   * @tparam T The output type of the node.
   * @param caller The calling task.
   * @param node The input node of which the given region shall be computed.
   * @param region The input region to be computed.
   * @return A std::shared_future representing the given region of the given node.
   *         This can come from a cache, another task or a new task.
   */
  template<typename T> GeneratedTile<T> generateRegion(Task& caller, const OutputNode<T>& node, rectangle_t region) {
    if (node.memoryMode() == MemoryMode::ANY_MEMORY) {
      shared_tile_t<T> cache_tile{node.cacheGetSynchronized(region)};
      if (cache_tile) {
        std::promise<shared_tile_t<T>> promise{};
        promise.set_value(cache_tile);
        return {promise.get_future().share(), true};
      }
    }

    std::unique_ptr<TypedTask<shared_tile_t<T>>> task{node.typedTask(*this, region)};
    DEBUG_ASSERT(std::runtime_error, task, "The task is nullptr!");

    TypedTask<shared_tile_t<T>>* ptr{task.get()};
    auto [it, inserted]{set_.emplace(std::move(task))};
    if (inserted) {
      ptr->addDependant(caller);

      if (ptr->allGenerated())
        performable_.push_back(ptr);
      else
        out_requestable_.push_front(ptr);
      return {ptr->future(), false};
    } else {
      Task& old{**it};
      old.addDependant(caller);
      return {dynamic_cast<TypedTask<shared_tile_t<T>>&>(old).future(), false};
    }
  }

  void addSinkTask(const SinkNode& node) {
    std::unique_ptr<Task> task{node.task(*this)};
    Task& ref{*task};
    set_.emplace(std::move(task));
    if (ref.allGenerated())
      performable_.push_back(&ref);
    else
      chooser_.addSinkTask(ref, node.relevance());
  }

  void addSingleFinished(Task& task, const Node& dependency, rectangle_t rectangle) {
    finished_.emplace_back(task, dependency, rectangle);
  }
  task_dependencies_t getSingleFinished() {
    task_dependencies_t deque{};
    std::swap(deque, finished_);
    return deque;
  }

  GraphAdaptor() = default;
  GraphAdaptor(const GraphAdaptor&) = delete;
  GraphAdaptor(GraphAdaptor&&) = default;

  friend std::ostream& operator<<(std::ostream& stream, const GraphAdaptor& a) {
    constexpr auto delimiter(
        "****************************************************************************************************");
    stream << delimiter << std::endl;
    for (const auto& task : a.set_) {
      stream << '[' << (a.isIn(a.out_requestable_, task.get()) or a.chooser_.contains(*task) ? 'X' : ' ') << "]["
             << (a.isIn(a.requested_, task.get()) ? 'X' : ' ') << "]["
             << (a.isIn(a.performable_, task.get()) ? 'X' : ' ') << "] " << *task << std::endl;
      for (const auto& dependant : task->dependants()) stream << "          " << *dependant << std::endl;
    }
    return stream << delimiter;
  }
};
} // namespace ImageGraph::internal
