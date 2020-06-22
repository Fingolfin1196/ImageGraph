#pragma once

#include "../Mathematics.hpp"
#include <boost/coroutine2/coroutine.hpp>

namespace ImageGraph::internal {
enum class SpiralSide { RIGHT, BOTTOM, LEFT, TOP };
template<typename int_t> struct SpiralInfo {
  SpiralSide side;
  int_t x, y;
  SpiralInfo(SpiralSide side, int_t x, int_t y) : side{side}, x{x}, y{y} {}
};

template<typename int_t, typename functor_t>
static inline void simple_spiral(const int_t x_start, const int_t y_start, const int_t x_min, const int_t y_min,
                                 const int_t x_max, const int_t y_max, functor_t functor) {
  using namespace math;
  constexpr int_t _0{0};

  assert(x_min <= x_start and x_start <= x_max);
  assert(y_min <= y_start and y_start <= y_max);

  functor(SpiralInfo(SpiralSide::TOP, x_start, y_start));
  const int_t maximum{std::max(std::max(std::max(x_start - x_min, x_max - x_start), _0),
                               std::max(std::max(y_start - y_min, y_max - y_start), _0))};
  for (int_t i{1}; i <= maximum; ++i) {
    const int_t x2{x_start + i}, y2{y_start + i};
    const bool right1{x2 <= x_max}, bottom{y2 <= y_max}, left{x_start >= x_min + i}, top{y_start >= y_min + i},
        right2{x2 + 1 <= x_max};
    const int_t x1c{clamped_dif<int_t>(x_start + left, i, x_min, x_max)},
        y1c{y_start >= y_min + i ? y_start - i : y_min}, y2c{std::min(y2, y_max)};
    if (right1)
      for (int_t y{y_start}; y <= y2c; ++y) functor(SpiralInfo(SpiralSide::RIGHT, x2, y));
    if (bottom)
      for (int_t x{clamped_dif<int_t>(x2, right1, x_min, x_max)};; --x) {
        functor(SpiralInfo(SpiralSide::BOTTOM, x, y2));
        if (x == x1c) break;
      }
    if (left) {
      const int_t x1{x_start - i};
      for (int_t y{y2c};; --y) {
        functor(SpiralInfo(SpiralSide::LEFT, x1, y));
        if (y == y1c) break;
      }
    }
    if (top) {
      const int_t y1{y_start - i}, x2c{std::min(x2 + 1 - right2, x_max)};
      for (int_t x{x1c}; x <= x2c; ++x) functor(SpiralInfo(SpiralSide::TOP, x, y1));
    }
    if (right2)
      for (int_t y{y1c}; y < y_start; ++y) functor(SpiralInfo(SpiralSide::RIGHT, x2 + 1, y));
  }
}

template<typename int_t> using spiral_coroutine_t = boost::coroutines2::coroutine<SpiralInfo<int_t>>;
template<typename int_t> using spiral_pull_t = typename spiral_coroutine_t<int_t>::pull_type;
template<typename int_t> using spiral_push_t = typename spiral_coroutine_t<int_t>::push_type;

template<typename int_t> static inline spiral_pull_t<int_t> spiral_generator(const int_t x_start, const int_t y_start,
                                                                             const int_t x_base, const int_t y_base,
                                                                             const int_t width, const int_t height) {
  const int_t x_added{x_base + width - 1}, y_added{y_base + height - 1};
  return spiral_pull_t<int_t>(
      [x_base, x_added, y_base, y_added, width, height, x_start, y_start](spiral_push_t<int_t>& yield) {
        simple_spiral<int_t>(x_start, y_start, std::min(x_base, x_added), std::min(y_base, y_added),
                             std::max(x_base, x_added), std::max(y_base, y_added), std::ref(yield));
      });
}
} // namespace ImageGraph::internal