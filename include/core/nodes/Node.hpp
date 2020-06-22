#pragma once

#include "../../internal/MemoryMode.hpp"
#include "../Rectangle.hpp"
#include <chrono>

namespace ImageGraph {
struct OutNode;
struct Node {
  using dimension_t = std::size_t;
  using dimensions_t = RectangleDimensions<dimension_t>;
  using rectangle_t = Rectangle<dimension_t>;
  using channels_t = std::size_t;
  using input_count_t = std::size_t;
  using input_index_t = std::size_t;
  using probability_t = double;

private:
  const dimensions_t dimensions_;
  const channels_t channels_;
  const input_count_t input_count_;
  const internal::MemoryMode memory_mode_;

protected:
  virtual std::ostream& print(std::ostream& stream) const { return stream << "[RawNode @ " << this << "]"; }

public:
  const dimensions_t& dimensions() const { return dimensions_; }
  dimension_t width() const { return dimensions_.width(); }
  dimension_t height() const { return dimensions_.height(); }

  channels_t channels() const { return channels_; }
  input_count_t inputCount() const { return input_count_; }
  internal::MemoryMode memoryMode() const { return memory_mode_; }

  virtual OutNode& inputNode(input_index_t index) const = 0;
  virtual rectangle_t inputRegion(input_index_t index, rectangle_t out_rect) const = 0;
  // TODO Replace with a useful value!
  virtual probability_t removalProbability() const { return .5; }
  virtual bool isCacheImportant() const { return false; }

  friend std::ostream& operator<<(std::ostream& s, const Node& node) { return node.print(s); }

  Node(dimensions_t dimensions, channels_t channels, input_count_t inputs, internal::MemoryMode mode)
      : dimensions_{dimensions}, channels_{channels}, input_count_{inputs}, memory_mode_{mode} {}
  virtual ~Node() = default;
};
} // namespace ImageGraph
