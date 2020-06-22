#pragma once

#include "../../internal/ProtoCache.hpp"
#include "../../internal/ProtoTask.hpp"
#include "../../internal/Task.hpp"
#include "Node.hpp"
#include <iostream>
#include <stack>

namespace ImageGraph {
class OptimizedOutNode;
struct OutNode : public Node {
  struct ParentPair {
    /**
     * WARNING This might be dangerous, but it should not be!
     * The parent should always register and unregister itself (and nobody else),
     * which ensures that this information is always valid, I hope.
     */
    OptimizedOutNode* parent;
    bool is_output;

    ParentPair(OptimizedOutNode* parent, bool is_output) : parent{parent}, is_output{is_output} {}
  };

  using successor_count_t = std::size_t;
  using parents_t = std::stack<ParentPair, std::vector<ParentPair>>;
  using duration_t = std::chrono::duration<double, std::nano>;
  using proto_cache_t = internal::ProtoCache<rectangle_t>;
  using probability_t = double;

private:
  const std::type_info& output_type_;
  parents_t parents_{};
  successor_count_t successor_count_;
  probability_t change_probability_{0};

protected:
  /**
   * The computation time for a tile of the given dimensions.
   */
  virtual duration_t tileDuration(rectangle_t region) const = 0;
  virtual void updateTileDuration(duration_t duration, rectangle_t region) const = 0;

  /**
   * @return The output does not need to be clipped.
   */
  virtual rectangle_t rawInputRegion(input_index_t input, rectangle_t output_rectangle) const = 0;

public:
  OutNode(dimensions_t dimensions, channels_t channels, input_count_t inputs, internal::MemoryMode mode,
          const std::type_info& type, successor_count_t count = 0)
      : Node(dimensions, channels, inputs, mode), output_type_{type}, successor_count_{count} {}
  virtual ~OutNode() {
    if (not parents_.empty()) std::cerr << "There is a parent at destruction!" << std::endl;
  }

  const std::type_info& outputType() const { return output_type_; }
  rectangle_t inputRegion(input_index_t input, rectangle_t output_rectangle) const final {
    return rectangle_t(rawInputRegion(input, output_rectangle)).clip(inputNode(input).dimensions());
  }

  const OutNode& outputNode() const {
    if (parents_.empty()) return *this;

    auto& [top_parent, top_output]{parents_.top()};
    if (top_output) return (const OutNode&)(*top_parent);

    throw std::runtime_error("There is a parent, but it is not representing this node!");
  }
  OutNode& outputNode() {
    if (parents_.empty()) return *this;

    auto& [top_parent, top_output]{parents_.top()};
    if (top_output) return (OutNode&)(*top_parent);

    throw std::runtime_error("There is a parent, but it is not representing this node!");
  }

  virtual std::unique_ptr<internal::Task> task(internal::GraphAdaptor& adaptor, rectangle_t region) const = 0;

  virtual std::unique_ptr<internal::ProtoOutTask> protoTask(rectangle_t region) const = 0;

  const ParentPair& topParent() const { return parents_.top(); }
  bool hasParents() const { return not parents_.empty(); }
  void addParent(OptimizedOutNode* parent) { parents_.emplace(parent, false); }
  void setParentOutput(const OptimizedOutNode* parent) {
    auto& top_parent{parents_.top()};
    DEBUG_ASSERT(std::invalid_argument, top_parent.parent == parent, "The given node is not the topmost parent!");
    top_parent.is_output = true;
  }
  void removeLastParent(const OptimizedOutNode* parent) {
    DEBUG_ASSERT(std::invalid_argument, parents_.top().parent == parent, "The given node is not the topmost parent!");
    parents_.pop();
  }

  virtual std::size_t elementBytes() const = 0;
  std::size_t fullByteNumber() const { return elementBytes() * dimensions().size() * channels(); }
  virtual std::size_t cacheSizeFromBytes(std::size_t byte_num) const = 0;

  virtual probability_t changeProbability() const { return change_probability_; }
  virtual void setChangeProbability(probability_t probability) { change_probability_ = probability; }

  virtual void setCacheSize(std::size_t size) const = 0;
  virtual void setCacheBytes(std::size_t bytes) const { setCacheSize(cacheSizeFromBytes(bytes)); }
  virtual bool isCacheable(rectangle_t) const { return false; }
  /**
   * @return A ProtoCache that has the same contents as the cache.
   */
  virtual std::unique_ptr<proto_cache_t> createProtoCache() const = 0;

  successor_count_t successorCount() const { return successor_count_; }
  void addSuccessor() { ++successor_count_; }
  void removeSuccessor() { --successor_count_; }

protected:
  std::ostream& print(std::ostream& stream) const override { return stream << "[OutNode @ " << this << "]"; }
};
} // namespace ImageGraph
