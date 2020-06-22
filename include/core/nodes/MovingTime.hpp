#pragma once

#include "../../internal/Random.hpp"
#include "../../internal/typing/NumberTraits.hpp"
#include "CachedOutputNode.hpp"
#include "TiledInputOutputNode.hpp"

namespace ImageGraph {
struct MovingTimeOutNode : virtual public OutNode {
  using factor_t = double;

private:
  mutable internal::OrderedMapCache<dimensions_t, duration_t> cache_;
  const factor_t factor_;

protected:
  virtual duration_t computeDuration(dimensions_t dimensions) const = 0;
  /**
   * This function is synchronized, i.e. it locks a mutex!
   * @param duration The new duration to update the value with.
   * @param region The region to which the duration belongs.
   */
  void updateTileDuration(duration_t duration, rectangle_t region) const final {
    auto dimensions{region.dimensions()};
    std::lock_guard lock{cache_.mutex()};
    auto ptr{cache_.get(dimensions)};
    if (ptr)
      *ptr = factor_ * duration + (1. - factor_) * *ptr;
    else
      cache_.put(dimensions, std::move(duration));
  }
  /**
   * This function is not synchronized!
   * @param region The region to compute the computation time of.
   * @return The computation time.
   */
  duration_t tileDuration(rectangle_t region) const final {
    auto dimensions{region.dimensions()};
    auto ptr{cache_.get(dimensions)};
    if (ptr)
      return *ptr;
    else {
      auto duration{computeDuration(dimensions)};
      cache_.put(dimensions, duration_t(duration));
      return duration;
    }
  }

public:
  MovingTimeOutNode(std::size_t cache_size = 8, factor_t factor = 1e-2) : cache_{cache_size}, factor_{factor} {}
};

template<typename OutputType, typename... InputTypes> struct MovingTimeInputOutputNode
    : virtual public MovingTimeOutNode,
      virtual public TiledInputOutputNode<OutputType, InputTypes...> {
  using rectangle_t = Node::rectangle_t;
  using dimensions_t = OutNode::dimensions_t;
  using duration_t = OutNode::duration_t;

private:
  struct DurationInputsCallable {
    template<std::size_t Index> using element_t = std::tuple_element_t<Index, std::tuple<InputTypes...>>;
    template<std::size_t Index> static inline Tile<element_t<Index>> call(const MovingTimeInputOutputNode& node,
                                                                          const rectangle_t output_rectangle) {
      using namespace internal;
      using generator_t =
          NumberGenerator<element_t<Index>, pcg_fast_generator<least_floating_point_t<element_t<Index>>>>;
      Tile<element_t<Index>> tile{node.inputRegion(Index, output_rectangle), node.inputNode(Index).channels()};
      std::generate(tile.begin(), tile.end(), generator_t());
      return std::move(tile);
    }
  };

protected:
  std::tuple<Tile<InputTypes>...> durationInputs(rectangle_t rectangle) const {
    return internal::ct::map<0, sizeof...(InputTypes), DurationInputsCallable>(std::cref(*this), rectangle);
  }
  virtual void computeImpl(std::tuple<const Tile<InputTypes>&...> inputs, Tile<OutputType>& output) const = 0;

  virtual rectangle_t durationRectangle(dimensions_t dimensions) const { return {dimensions}; }

  duration_t computeDuration(dimensions_t dimensions) const final {
    using namespace std::chrono;

    rectangle_t rectangle{durationRectangle(dimensions)};
    std::tuple<Tile<InputTypes>...> raw_inputs{durationInputs(rectangle)};
    std::tuple<const Tile<InputTypes>&...> inputs{raw_inputs};
    Tile<OutputType> output{rectangle, this->channels()};

    const auto start_time{steady_clock::now()};
    computeImpl(std::move(inputs), output);
    return duration_cast<duration_t>(steady_clock::now() - start_time);
  }

public:
  void compute(std::tuple<const Tile<InputTypes>&...> inputs, Tile<OutputType>& output) const final {
    computeImpl(std::move(inputs), output);
  }
};
} // namespace ImageGraph
