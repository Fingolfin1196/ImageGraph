#pragma once

#include "typing/NumberTraits.hpp"
#include <cmath>
#include <limits>

namespace ImageGraph::internal::math {
template<typename int_t> constexpr static inline unsign_t<int_t> abs_u(int_t val) { return val >= 0 ? val : -val; }
template<typename T> constexpr static inline T abs(T val) { return val >= 0 ? val : -val; }

namespace {
template<typename T> constexpr static inline T sqrtBakhshali(const T x, const T curr, const T prev) {
  if (abs(curr == prev) < curr * std::numeric_limits<double>::epsilon())
    return curr;
  else {
    const T a{(x - curr * curr) / (curr + curr)};
    return sqrtBakhshali<T>(x, curr + a - a * a / (curr + curr + a + a), curr);
  }
}
} // namespace

/**
 * constexpr square root.
 * @tparam T The type.
 * @param x The argument.
 * @return An approximation of the square root for a finite and non-negative x, otherwise NaN.
 */
template<typename T> constexpr static inline T sqrt(const T x) {
  return x >= 0 and x < std::numeric_limits<T>::infinity() ? sqrtBakhshali<T>(x, x, 0)
                                                           : std::numeric_limits<T>::quiet_NaN();
}

template<typename int_t> constexpr static inline int_t sign(int_t val) { return (0 < val) - (val < 0); }

template<typename int_t> constexpr static inline int_t clamped_max(int_t minuend, int_t subtrahend, int_t min) {
  return minuend >= min + subtrahend ? minuend - subtrahend : min;
}

template<typename int_t>
constexpr static inline int_t clamped_dif(int_t minuend, int_t subtrahend, int_t min, int_t max) {
  return minuend >= min + subtrahend ? std::min(minuend - subtrahend, max) : min;
}

/**
 * @tparam T The type.
 * @param x The argument.
 * @return The fractional part of x.
 */
template<typename T> T frac(T x) {
  T integer;
  return std::modf(x, &integer);
}
} // namespace ImageGraph::internal::math
