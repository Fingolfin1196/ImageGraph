#pragma once

#include "../Random.hpp"
#include "NumberTraits.hpp"
#include <optional>

namespace ImageGraph::internal {

// saturate_cast

template<typename OutType, typename InType> inline static constexpr OutType saturate_cast(InType input) {
  constexpr OutType min{std::numeric_limits<OutType>::min()}, max{std::numeric_limits<OutType>::max()};
  return input >= max ? max : (input <= min ? min : OutType(input));
}

// convert_normalized

namespace {
template<typename OutputType, typename InputType>
constexpr static inline bool convert_enable{(is_floating_point_v<OutputType> != is_integral_v<OutputType>)and(
    is_floating_point_v<InputType> != is_integral_v<InputType>)};

template<typename OutputType, typename InputType, typename Generator>
requires convert_enable<OutputType, InputType> constexpr static inline OutputType
convert_normalized_dithered(InputType input, Generator& gen) {
  if constexpr (std::is_same_v<InputType, OutputType>)
    return input;
  else if constexpr (is_floating_point_v<OutputType> and is_floating_point_v<InputType>)
    return saturate_cast<OutputType>(input);
  else if constexpr (is_floating_point_v<OutputType>) {
    constexpr OutputType p5{.5}, factor{white_point_v<OutputType> / OutputType(white_point_v<InputType>)};
    // Here, one needs to choose an interval which one quantized integer value x represents:
    // Is it [x, x + 1), [x - 0.5, x + 0.5), or even [x - 1, x)?
    // Only the first two option really make any sense, and for a long time I tended to prefer the former,
    // as it represents the interval that is mapped to that value when casting from a larger type,
    // but it has the unfortunate side effect of distorting values (i.e. brightening the image) when
    // performing two conversions in a row. As I now think that preserving the perceived brightness
    // of an image than trying to revert a cast that has possibly, but not certainly happened,
    // the second option is now implemented.
    return factor * (input + random_real<OutputType>(gen, -p5, p5));
  } else {
    using least_t = least_common_floating_point_t<OutputType, InputType>;
    constexpr least_t factor{least_t(white_point_v<OutputType>) / least_t(white_point_v<InputType>)};
    return saturate_cast<OutputType>(factor * input + random_norm<least_t>(gen));
  }
}

template<typename OutputType, typename InputType>
requires convert_enable<OutputType, InputType> constexpr static inline OutputType
convert_normalized_undithered(InputType input) {
  if constexpr (std::is_same_v<InputType, OutputType>)
    return input;
  else if constexpr (is_floating_point_v<OutputType> and is_floating_point_v<InputType>)
    return saturate_cast<OutputType>(input);
  else if constexpr (is_floating_point_v<OutputType>) {
    constexpr OutputType factor{white_point_v<OutputType> / OutputType(white_point_v<InputType>)};
    return factor * input;
  } else {
    using least_t = least_common_floating_point_t<OutputType, InputType>;
    constexpr least_t p5{.5}, factor{least_t(white_point_v<OutputType>) / least_t(white_point_v<InputType>)};
    return saturate_cast<OutputType>(factor * input + p5);
  }
}
} // namespace

template<bool dither, typename OutputType, typename InputType, typename... Args>
constexpr static inline OutputType convert_normalized(InputType input, Args&... args) {
  if constexpr (dither)
    return convert_normalized_dithered<OutputType, InputType>(input, args...);
  else
    return convert_normalized_undithered<OutputType, InputType>(input, args...);
}
} // namespace ImageGraph::internal