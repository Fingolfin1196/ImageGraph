#pragma once

#include "../../core/Rectangle.hpp"
#include "../Mathematics.hpp"
#include "../typing/NumberConversion.hpp"
#include <boost/coroutine2/coroutine.hpp>
#include <ostream>

namespace ImageGraph::internal {
enum class Direction : bool { X, Y };
static inline std::ostream& operator<<(std::ostream& stream, const Direction& direction) {
  return stream << (direction == Direction::X ? "X" : "Y");
}
constexpr static inline Direction operator!(const Direction& direction) {
  return direction == Direction::X ? Direction::Y : Direction::X;
}

/**
 * Mostly a C++ translation of https://github.com/Fingolfin1196/gilbert.git using a functor.
 * The source is licensed under the 2-Clause BSD License (SPDX short identifier: BSD-2-Clause),
 * a copy of which can be found under other-licenses/gilbert-bsd-2-clause.txt
 * relative to the base directory.
 */
template<Direction major_dir, typename int_t, typename functor_t> static inline std::enable_if_t<is_signed_v<int_t>>
gilbert2d_inner(int_t x, int_t y, int_t major, int_t minor, functor_t functor) {
  using namespace math;
  using uint_t = unsign_t<int_t>;
  static_assert(major_dir == Direction::X or major_dir == Direction::Y);

  const uint_t major_dim{abs_u(major)}, minor_dim{abs_u(minor)};
  const int_t major_step{sign(major)}, minor_step{sign(minor)};

  // Trivial row fill
  if (minor_dim == 1) {
    if constexpr (major_dir == Direction::X)
      for (uint_t i{0}; i < major_dim; ++i, x += major_step) functor(Point(x, y));
    else
      for (uint_t i{0}; i < major_dim; ++i, y += major_step) functor(Point(x, y));
    return;
  }

  // Trivial column fill
  if (major_dim == 1) {
    if constexpr (major_dir == Direction::Y)
      for (uint_t i{0}; i < minor_dim; ++i, x += minor_step) functor(Point(x, y));
    else
      for (uint_t i{0}; i < minor_dim; ++i, y += minor_step) functor(Point(x, y));
    return;
  }

  int_t major2{major / 2}, minor2{minor / 2};

  if (2 * major_dim > 3 * minor_dim) {
    // Prefer even steps
    if (abs(major2) % 2 and major_dim > 2) major2 += major_step;

    // Long case: Split in two parts only
    gilbert2d_inner<major_dir, int_t>(x, y, major2, minor, functor);
    if constexpr (major_dir == Direction::X)
      x += major2;
    else
      y += major2;
    gilbert2d_inner<major_dir, int_t>(x, y, major - major2, minor, functor);
  } else {
    // Prefer even steps.
    if (abs(minor2) % 2 and minor_dim > 2) minor2 += minor_step;

    // Standard case: One step up, one long horizontal, one step down
    gilbert2d_inner<not major_dir, int_t>(x, y, minor2, major2, functor);
    if constexpr (major_dir == Direction::X)
      y += minor2;
    else
      x += minor2;
    gilbert2d_inner<major_dir, int_t>(x, y, major, minor - minor2, functor);
    if constexpr (major_dir == Direction::X)
      x += major - major_step, y -= minor_step;
    else
      x -= minor_step, y += major - major_step;
    gilbert2d_inner<not major_dir, int_t>(x, y, -minor2, major2 - major, functor);
  }
}

template<typename int_t, typename functor_t>
static inline void gilbert2d(const int_t x, const int_t y, const int_t width, const int_t height, functor_t functor,
                             std::optional<Direction> direction = std::nullopt) {
  switch (direction ? *direction : (width >= height ? Direction::X : Direction::Y)) {
    case Direction::X: return gilbert2d_inner<Direction::X>(x, y, width, height, functor);
    case Direction::Y: return gilbert2d_inner<Direction::Y>(x, y, height, width, functor);
  }
}

template<typename int_t> using gilbert_coroutine_t = boost::coroutines2::coroutine<Point<int_t>>;
template<typename int_t> using gilbert_pull_t = typename gilbert_coroutine_t<int_t>::pull_type;
template<typename int_t> using gilbert_push_t = typename gilbert_coroutine_t<int_t>::push_type;

template<typename int_t>
static inline gilbert_pull_t<int_t> gilbert2d_generator(const int_t x, const int_t y, const int_t width,
                                                        const int_t height,
                                                        std::optional<Direction> direction = std::nullopt) {
  return gilbert_pull_t<int_t>([x, y, width, height, direction](gilbert_push_t<int_t>& yield) {
    gilbert2d(x, y, width, height, std::ref(yield), direction);
  });
}
} // namespace ImageGraph::internal