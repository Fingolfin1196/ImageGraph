#pragma once
#include "../../internal/ProtoTask.hpp"
#include "../../internal/Task.hpp"
#include "Node.hpp"

namespace ImageGraph {
struct SinkNode : public Node {
  using pixel_index_t = std::size_t;
  using relevance_t = double;
  using point_t = Point<std::size_t>;

  SinkNode(dimensions_t dimensions, channels_t channels, input_count_t input_count, internal::MemoryMode mode)
      : Node(dimensions, channels, input_count, mode) {}
  virtual ~SinkNode() = default;

  virtual std::unique_ptr<internal::Task> task(internal::GraphAdaptor& adaptor) const = 0;
  virtual std::unique_ptr<internal::ProtoSinkTask> protoTask() const = 0;

  virtual relevance_t relevance() const = 0;
  virtual point_t centralPoint() const = 0;

protected:
  std::ostream& print(std::ostream& stream) const override { return stream << "[SinkNode @ " << this << "]"; }
};
} // namespace ImageGraph
