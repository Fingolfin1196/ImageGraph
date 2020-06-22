#pragma once

#include "../../../internal/typing/NumberConversion.hpp"
#include "../MovingTime.hpp"
#include <boost/dynamic_bitset.hpp>

namespace ImageGraph::nodes {
template<typename OutputType, typename... InputTypes> struct ChannelCombinatorNode final
    : public TiledCachedOutputNode<OutputType>,
      public MovingTimeInputOutputNode<OutputType, InputTypes...> {
  using dimensions_t = Node::dimensions_t;
  using rectangle_t = Node::rectangle_t;
  using input_index_t = Node::input_index_t;

  using least_float_t = internal::least_floating_point_t<OutputType>;
  using pcg_t = internal::pcg_fast_generator<least_float_t>;
  using channel_array_t = SizedArray<std::optional<std::size_t>>;
  using channel_arrays_t = std::array<channel_array_t, sizeof...(InputTypes)>;

private:
  mutable std::optional<pcg_t> generator_;
  const channel_arrays_t channel_array_;

  struct MaximumCallable {
    constexpr static inline dimensions_t call(const dimensions_t d1, const dimensions_t d2) {
      return {std::max(d1.width(), d2.width()), std::max(d1.height(), d2.height())};
    }
  };
  struct DimensionsCallable {
    static inline dimensions_t call(const OutNode* node) { return node->dimensions(); }
  };

  template<bool Dither, std::size_t Index, typename... Args> struct CalcCallable {
    using element_t = std::tuple_element_t<Index, std::tuple<InputTypes...>>;

    static inline void call(const std::tuple<const Tile<InputTypes>&...>& inputs, Tile<OutputType>& output,
                            const channel_arrays_t& channel_arrays, Args&... args) {
      const Tile<element_t>& input{std::get<Index>(inputs)};
      const std::size_t channels{input.channels()};
      const channel_array_t& channel_array{channel_arrays[Index]};
      DEBUG_ASSERT(std::runtime_error, channel_array.size() == channels, "The channel numbers do not match!");

      std::vector<std::pair<std::size_t, std::size_t>> used_channels{};
      used_channels.reserve(channels);
      for (std::size_t i{0}; i < channels; ++i) {
        const auto& element{channel_array[i]};
        if (element) used_channels.emplace_back(i, *element);
      }

      const auto width{input.width()}, height{input.height()};
      for (std::size_t y{0}; y < height; ++y)
        for (std::size_t x{0}; x < width; ++x)
          for (const auto [input_channel, output_channel] : used_channels)
            output(x, y, output_channel) =
                internal::convert_normalized<Dither, OutputType>(input(x, y, input_channel), args...);
    }
  };
  template<std::size_t Index> using DitherCalcCallable = CalcCallable<true, Index, pcg_t>;
  template<std::size_t Index> using NoDitherCalcCallable = CalcCallable<false, Index>;

  template<std::size_t Index> struct InputTypeOutputCallable {
    static inline void call(std::ostream& stream) {
      using namespace internal;
      stream << ", " << type_name<std::tuple_element_t<Index, std::tuple<InputTypes...>>>();
    }
  };

  static inline std::size_t maxChannel(const channel_arrays_t& channel_arrays) {
    std::size_t max_channel{0};
    for (const auto& array : channel_arrays)
      for (const auto& ptr : array)
        if (ptr and *ptr > max_channel) max_channel = *ptr;
    return max_channel;
  }

protected:
  void computeImpl(std::tuple<const Tile<InputTypes>&...> inputs, Tile<OutputType>& output) const final {
    if (generator_)
      internal::ct::for_all<0, sizeof...(InputTypes), DitherCalcCallable>(
          std::cref(inputs), std::ref(output), std::cref(channel_array_), std::ref(*generator_));
    else
      internal::ct::for_all<0, sizeof...(InputTypes), NoDitherCalcCallable>(std::cref(inputs), std::ref(output),
                                                                            std::cref(channel_array_));
  }

  rectangle_t rawInputRegion(input_index_t, rectangle_t rectangle) const final { return rectangle; }

  std::ostream& print(std::ostream& stream) const override {
    using namespace internal;
    stream << "[ChannelCombinatorNode<" << type_name<OutputType>();
    internal::ct::for_all<0, sizeof...(InputTypes), InputTypeOutputCallable>(stream);
    stream << ">(channel_array={";
    for (auto it1{channel_array_.begin()}; it1 != channel_array_.end();) {
      stream << "{";
      const auto& channel_vector{*it1};
      for (auto it2{channel_vector.begin()}; it2 != channel_vector.end();) {
        const auto& ptr{*it2};
        if (ptr)
          stream << *ptr;
        else
          stream << "nullptr";
        if (++it2 != channel_vector.end()) stream << ", ";
      }
      stream << "}";
      if (++it1 != channel_array_.end()) stream << ", ";
    }
    return stream << "}) @ " << this << "]";
  }

public:
  ChannelCombinatorNode(std::tuple<OutputNode<InputTypes>*...>&& inputs, channel_arrays_t&& arrays, bool dither)
      : OutNode(internal::ct::transform_reduce<dimensions_t, MaximumCallable, DimensionsCallable>(inputs, 0),
                maxChannel(arrays) + 1, sizeof...(InputTypes), internal::MemoryMode::ANY_MEMORY, typeid(OutputType)),
        InputOutNode<InputTypes...>(std::move(inputs), false),
        generator_{dither ? std::optional<pcg_t>(pcg_t()) : std::optional<pcg_t>()}, channel_array_{std::move(arrays)} {
    DEBUG_ASSERT_P(
        std::invalid_argument,
        [this] {
          boost::dynamic_bitset<> channel_use{this->channels()};
          for (const auto& vector : channel_array_)
            for (const auto& ptr : vector)
              if (ptr and channel_use.test_set(*ptr)) return false;
          return true;
        },
        "Multiple inputs write to the same channel!");
  }
};
} // namespace ImageGraph::nodes
