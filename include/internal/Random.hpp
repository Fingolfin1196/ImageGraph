#pragma once

#include <random>

namespace ImageGraph::internal {
template<typename N, typename G> static inline N random_integer(G& generator, N min, N max) {
  return std::uniform_int_distribution<N>(min, max)(generator);
}

template<typename N, typename G> static inline N random_real(G& generator, N min, N max) {
  return std::uniform_real_distribution<N>(min, max)(generator);
}

template<typename N, typename G> static inline N random_norm(G& generator) {
  return random_real<N>(generator, 0, 1);
}

template<typename T,
         std::enable_if_t<std::is_floating_point_v<T> != std::is_integral_v<T>, bool> = true>
using distribution_t =
    std::conditional_t<std::is_floating_point_v<T>, std::uniform_real_distribution<T>,
                       std::uniform_int_distribution<T>>;

template<typename T, typename G,
         std::enable_if_t<std::is_floating_point_v<T> != std::is_integral_v<T>, bool> = true>
class NumberGenerator {
  distribution_t<T> distribution_{};
  G generator_{};

public:
  T operator()() { return distribution_(generator_); }
};
} // namespace ImageGraph::internal
