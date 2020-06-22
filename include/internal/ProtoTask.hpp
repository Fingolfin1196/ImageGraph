#pragma once

#include "../core/nodes/Node.hpp"
#include "Debugging.hpp"
#include <chrono>
#include <deque>
#include <memory>
#include <unordered_set>

namespace ImageGraph {
struct SinkNode;

namespace internal {
struct ProtoOutTask;
struct ProtoSinkTask;
struct ProtoGraphAdaptor;

struct ProtoTask {
  using rectangle_t = Node::rectangle_t;
  using duration_t = std::chrono::duration<double>;

private:
protected:
  virtual std::ostream& print(std::ostream& stream) const = 0;

public:
  virtual ~ProtoTask() = default;

  virtual rectangle_t region() const = 0;

  virtual duration_t singleTime() const = 0;
  virtual duration_t fullTime() const = 0;

  friend std::ostream& operator<<(std::ostream& stream, const ProtoTask& task) { return task.print(stream); }
};

struct ProtoOutTask : public ProtoTask {
  using performer_t = std::function<void(const OutNode&, rectangle_t)>;

  virtual const OutNode& node() const = 0;

  virtual void performRequiredTasks(performer_t) = 0;

  bool operator==(const ProtoOutTask& other) const {
    return this->region() == other.region() and &node() == &other.node();
  }
};

struct ProtoSinkTask : public ProtoTask {
protected:
  virtual std::pair<const OutNode&, rectangle_t> generateRequiredTask() = 0;

public:
  virtual const SinkNode& node() const = 0;

  virtual bool allGenerated() const = 0;
  std::pair<const OutNode&, rectangle_t> nextRequiredTask() { return generateRequiredTask(); }

  bool operator==(const ProtoSinkTask& other) const {
    return this->region() == other.region() and &node() == &other.node();
  }
};
} // namespace internal
} // namespace ImageGraph

namespace std {
template<> struct hash<ImageGraph::internal::ProtoOutTask> {
  std::size_t operator()(const ImageGraph::internal::ProtoOutTask& task) const {
    using namespace ImageGraph;
    using namespace ImageGraph::internal;
    return hash_combine(hash<const OutNode*>()(&task.node()), hash<ProtoTask::rectangle_t>()(task.region()));
  }
};
template<> struct hash<ImageGraph::internal::ProtoSinkTask> {
  std::size_t operator()(const ImageGraph::internal::ProtoSinkTask& task) const {
    using namespace ImageGraph;
    using namespace ImageGraph::internal;
    return hash_combine(hash<const SinkNode*>()(&task.node()), hash<ProtoTask::rectangle_t>()(task.region()));
  }
};
} // namespace std

namespace ImageGraph::internal {
namespace detail {
template<typename ProtoTask> struct ProtoTaskHash {
  using is_transparent = void;

  std::size_t operator()(const ProtoTask* task) const {
    if (not task) return 0;
    return std::hash<ProtoTask>()(*task);
  }
  std::size_t operator()(const std::unique_ptr<ProtoTask>& task) const { return (*this)(task.get()); }
};

template<typename ProtoTask> struct ProtoTaskEquals {
  using is_transparent = void;
  using unique_t = std::unique_ptr<ProtoTask>;

  bool operator()(const ProtoTask* t1, const ProtoTask* t2) const {
    if (not t1 or not t2) return not t1 and not t2;
    return *t1 == *t2;
  }
  bool operator()(const unique_t& t1, const unique_t& t2) const { return (*this)(t1.get(), t2.get()); }
  bool operator()(const unique_t& t1, const ProtoTask* t2) const { return (*this)(t1.get(), t2); }
  bool operator()(const ProtoTask* t1, const unique_t& t2) const { return (*this)(t1, t2.get()); }
};
} // namespace detail

using ProtoTaskSet = std::unordered_set<std::unique_ptr<ProtoTask>, detail::ProtoTaskHash<ProtoTask>,
                                        detail::ProtoTaskEquals<ProtoTask>>;
using ProtoOutTaskSet = std::unordered_set<std::unique_ptr<ProtoOutTask>, detail::ProtoTaskHash<ProtoOutTask>,
                                           detail::ProtoTaskEquals<ProtoOutTask>>;
using ProtoSinkTaskSet = std::unordered_set<std::unique_ptr<ProtoSinkTask>, detail::ProtoTaskHash<ProtoSinkTask>,
                                            detail::ProtoTaskEquals<ProtoSinkTask>>;
} // namespace ImageGraph::internal
