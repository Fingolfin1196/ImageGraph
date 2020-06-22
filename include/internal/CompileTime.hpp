#pragma once

#include <cstddef>
#include <optional>
#include <tuple>
#include <utility>

namespace ImageGraph::internal::ct {
namespace detail {
template<std::size_t Begin, std::size_t Current, typename Callable, typename... Args> struct for_aller {
  constexpr static inline void call(Args&&... args) {
    Callable::template call<Current>(args...);
    for_aller<Begin, Current - 1, Callable, Args...>::call(std::forward<Args>(args)...);
  }
};
template<std::size_t Begin, typename Callable, typename... Args> struct for_aller<Begin, Begin, Callable, Args...> {
  constexpr static inline void call(Args&&... args) { Callable::template call<Begin>(std::forward<Args>(args)...); }
};
} // namespace detail

template<std::size_t Begin, std::size_t End, typename Callable, typename... Args>
constexpr static inline void for_all(Args&&... args) {
  if constexpr (Begin < End) detail::for_aller<Begin, End - 1, Callable, Args...>::call(std::forward<Args>(args)...);
}

namespace detail {
template<typename T, typename BinaryCallable, typename UnaryCallable, std::size_t Index, typename... Args>
struct transform_reducer {
  template<typename... TupleTypes>
  constexpr static inline T calc(const std::tuple<TupleTypes...>& tuple, Args&&... args) {
    return BinaryCallable::call(
        transform_reducer<T, BinaryCallable, UnaryCallable, Index - 1, TupleTypes...>(tuple, args...),
        UnaryCallable::call(std::get<Index>(tuple)));
  }
};
template<typename T, typename BinaryCallable, typename UnaryCallable, typename... Args>
struct transform_reducer<T, BinaryCallable, UnaryCallable, 0, Args...> {
  template<typename... TupleTypes>
  constexpr static inline T calc(const std::tuple<TupleTypes...>& tuple, Args&&... args) {
    return UnaryCallable::call(std::get<0>(tuple), std::forward<Args>(args)...);
  }
};
} // namespace detail

template<typename T, typename BinaryCallable, typename UnaryCallable, typename TupleType, typename... Args>
constexpr static inline T transform_reduce(const TupleType& tuple, T init, Args&&... args) {
  if constexpr (not std::tuple_size_v<TupleType>) return init;
  return BinaryCallable::call(
      init, detail::transform_reducer<T, BinaryCallable, UnaryCallable, std::tuple_size_v<TupleType> - 1,
                                      Args...>::template calc<>(tuple, std::forward<Args>(args)...));
}

namespace {
template<std::size_t Offset, typename Callable, typename IndexSequence> struct Mapper {};
template<std::size_t Offset, typename Callable, std::size_t... Indices>
struct Mapper<Offset, Callable, std::index_sequence<Indices...>> {
  template<typename... Args> constexpr static inline auto map(Args&&... args) {
    return std::make_tuple(Callable::template call<Offset + Indices>(args...)...);
  }
  template<typename... Args> constexpr static inline auto ref_map(Args&&... args) {
    return std::forward_as_tuple(Callable::template call<Offset + Indices>(args...)...);
  }
};
} // namespace

template<std::size_t Begin, std::size_t End, typename Callable, typename... Args>
constexpr static inline auto map(Args&&... args) {
  static_assert(Begin <= End);
  return Mapper<Begin, Callable, std::make_index_sequence<End - Begin>>::template map<Args...>(
      std::forward<Args>(args)...);
}

template<std::size_t Begin, std::size_t End, typename Callable, typename... Args>
constexpr static inline auto ref_map(Args&&... args) {
  static_assert(Begin <= End);
  return Mapper<Begin, Callable, std::make_index_sequence<End - Begin>>::template ref_map<Args...>(
      std::forward<Args>(args)...);
}

namespace detail {
template<std::size_t Current, std::size_t End, template<std::size_t> typename Callable, typename T, typename... Args>
constexpr static inline std::conditional_t<std::is_void_v<T>, void, std::optional<T>> template_find(std::size_t index,
                                                                                                    Args&&... args) {
  if (index == Current) return Callable<Current>::call(std::forward<Args>(args)...);
  if constexpr (Current + 1 < End)
    return template_find<Current + 1, End, Callable, T, Args...>(index, std::forward<Args>(args)...);
  else if constexpr (not std::is_void_v<T>)
    return {};
}
} // namespace detail

template<std::size_t Begin, std::size_t End, template<std::size_t> typename Callable, typename T, typename... Args>
constexpr static inline std::conditional_t<std::is_void_v<T>, void, std::optional<T>> template_find(std::size_t index,
                                                                                                    Args&&... args) {
  if constexpr (Begin >= End) {
    if constexpr (not std::is_void_v<T>) return {};
  } else {
    if (index < Begin or index >= End) {
      if constexpr (not std::is_void_v<T>) return {};
    } else
      return detail::template_find<Begin, End, Callable, T, Args...>(index, std::forward<Args>(args)...);
  }
}

template<std::size_t Begin, std::size_t End, template<std::size_t> typename TestCallable, typename T, typename... Args>
constexpr static inline std::conditional_t<std::is_void_v<T>, void, std::optional<T>> find_first(Args&&... args) {
  if constexpr (Begin >= End) {
    if constexpr (not std::is_void_v<T>) return {};
  } else {
    if (TestCallable<Begin>::test(args...))
      return TestCallable<Begin>::call(std::forward<Args>(args)...);
    else
      return find_first<Begin + 1, End, TestCallable, T, Args...>(std::forward<Args>(args)...);
  }
}
} // namespace ImageGraph::internal::ct
