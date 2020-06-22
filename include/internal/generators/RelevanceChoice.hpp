#pragma once

#include "../../core/nodes/SinkNode.hpp"
#include <algorithm>
#include <vector>

namespace ImageGraph::internal {
template<typename T, typename Op> struct RelevanceChoiceGenerator {
  using relevance_t = float;

  class OutputInfo {
    T* task_;
    relevance_t relevance;
    std::size_t generations{0};
    relevance_t relative_count{0};

  public:
    OutputInfo(T& task, relevance_t relevance) : task_{&task}, relevance{relevance} {}

    void nextRequiredTask() { task_->nextRequiredTask(); }
    bool allGenerated() const { return task_->allGenerated(); }
    T& task() const { return *task_; }

    OutputInfo& operator++() {
      relative_count = ++generations / relevance;
      return *this;
    }
    bool operator<(const OutputInfo& other) const {
      return relative_count < other.relative_count or
             (relative_count == other.relative_count and relevance < other.relevance);
    }
  };
  using info_t = std::deque<OutputInfo>;

private:
  info_t info_{};

public:
  RelevanceChoiceGenerator() {}

  void addSinkTask(T& task, relevance_t relevance) {
    if (not task.allGenerated()) info_.emplace_back(task, relevance);
  }

  bool contains(const T& task) const {
    return std::find_if(info_.begin(), info_.end(), [&task](const OutputInfo& info) { return info.task() == task; }) !=
           info_.end();
  }

  void eraseSinkTask(const T& task) {
    info_.erase(
        std::find_if(info_.begin(), info_.end(), [&task](const OutputInfo& info) { return info.task() == task; }));
  }

  /**
   * @return Not nullptr if the returned task has generated all required tasks, otherwise nullptr.
   */
  auto generate() {
    DEBUG_PREVENT(std::runtime_error,
                  std::any_of(info_.begin(), info_.end(), [](const OutputInfo& info) { return info.allGenerated(); }),
                  "There are tasks that cannot generate more dependencies!");

    auto it{std::min_element(info_.begin(), info_.end())};
    DEBUG_ASSERT(std::runtime_error, it != info_.end(), "There is no minimum!");

    auto& info{*it};
    ++info;
    return Op::call(info.task());
  }

  bool empty() const { return info_.empty(); }
  operator bool() const { return info_.size() > 0; }
};

namespace {
struct TaskChooserOp {
  static inline Task* call(Task& task) { return &task; }
};

struct ProtoTaskChooserOp {
  static inline ProtoSinkTask* call(ProtoSinkTask& task) { return &task; }
};
} // namespace

using TaskRelevanceChoiceGenerator = RelevanceChoiceGenerator<Task, TaskChooserOp>;
using ProtoTaskRelevanceChoiceGenerator = RelevanceChoiceGenerator<ProtoSinkTask, ProtoTaskChooserOp>;
} // namespace ImageGraph::internal
