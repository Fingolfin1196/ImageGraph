#pragma once

#include <cstddef>
#include <utility>

namespace ImageGraph::internal {
namespace detail {
template<typename T1, typename T2, typename... T>
inline constexpr std::size_t hash_combine(T1 h1, T2 h2, T&&... hashes) {
  return hash_combine<T...>(hash_combine<T1, T2>(h1, h2), std::forward<T>(hashes)...);
}

/**
 * A direct copy of boost::hash_combine, apart from the implicit hashing.
 */
template<> inline constexpr std::size_t hash_combine<std::size_t, std::size_t>(std::size_t h1,
                                                                               std::size_t h2) {
  return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
}
} // namespace detail

template<typename... T> inline constexpr std::size_t hash_combine(T&&... hashes) {
  return detail::hash_combine<T...>(std::forward<T>(hashes)...);
}
} // namespace ImageGraph::internal