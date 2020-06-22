#pragma once

#include "GeneralizedHilbert.hpp"
#include "Spiral.hpp"

namespace ImageGraph::internal {
template<typename int_t, typename functor_t>
static inline void gilbert_spiral(const int_t x_start, const int_t y_start, const int_t x_min, const int_t y_min,
                                  const int_t x_max, const int_t y_max, const unsign_t<int_t> tile_width,
                                  const unsign_t<int_t> tile_height, functor_t functor) {
  using uint_t = unsign_t<int_t>;

  if (x_max < x_min or y_max < y_min) return;
  assert(x_min <= x_start and x_start <= x_max);
  assert(y_min <= y_start and y_start <= y_max);

  const uint_t x_max0{static_cast<uint_t>(x_max - x_min)}, y_max0{static_cast<uint_t>(y_max - y_min)},
      width{x_max0 + 1}, height{y_max0 + 1};

  // The spiral always has its lower left corner at (x_min, y_min)!
  simple_spiral<uint_t>(
      (x_start - x_min) / tile_width, (y_start - y_min) / tile_height, 0, 0, x_max0 / tile_width, y_max0 / tile_height,
      [x_min, y_min, width, height, tile_width, tile_height, &functor](SpiralInfo<uint_t> info) {
        const auto& [side, x, y]{info};
        const uint_t current_width{std::min((x + 1) * tile_width, width) - x * tile_width},
            current_height{std::min((y + 1) * tile_height, height) - y * tile_height};
        switch (side) {
          case SpiralSide::RIGHT:
          case SpiralSide::TOP:
            return gilbert2d<int_t>(x_min + x * tile_width, y_min + y * tile_height, current_width, current_height,
                                    functor, side == SpiralSide::RIGHT ? Direction::Y : Direction::X);
          case SpiralSide::BOTTOM:
          case SpiralSide::LEFT:
            return gilbert2d<int_t>(x_min + x * tile_width + current_width - 1,
                                    y_min + y * tile_height + current_height - 1, -current_width, -current_height,
                                    functor, side == SpiralSide::BOTTOM ? Direction::X : Direction::Y);
        }
      });
}

template<typename int_t>
static inline gilbert_pull_t<int_t> gilbert_spiral_generator(const int_t x_start, const int_t y_start,
                                                             const int_t x_min, const int_t y_min, const int_t x_max,
                                                             const int_t y_max, const unsign_t<int_t> tile_width,
                                                             const unsign_t<int_t> tile_height) {
  return gilbert_pull_t<int_t>(
      [x_start, y_start, x_min, y_min, x_max, y_max, tile_width, tile_height](gilbert_push_t<int_t>& yield) {
        gilbert_spiral<int_t>(x_start, y_start, x_min, y_min, x_max, y_max, tile_width, tile_height, std::ref(yield));
      });
}
} // namespace ImageGraph::internal
