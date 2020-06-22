#pragma once
#include <sstream>

namespace ImageGraph::internal {
namespace __detail {
template<typename Arg, typename... Args> struct ArgsPrinter {
  static inline std::ostream& print(std::ostream& stream, Arg&& arg, Args&&... args) {
    return ArgsPrinter<Args...>::print(stream << arg, std::forward<Args>(args)...);
  }
};
template<typename Arg> struct ArgsPrinter<Arg> {
  static inline std::ostream& print(std::ostream& stream, Arg&& arg) { return stream << arg; }
};
template<typename... Args> static inline std::ostream& printArgs(std::ostream& stream, Args&&... args) {
  return ArgsPrinter<Args...>::print(stream, std::forward<Args>(args)...);
}

template<typename Exception, typename Test, typename... Args>
static inline void __debug_prevent(const Test& test, Args&&... args) {
  if (test) throw Exception(std::forward<Args>(args)...);
}
template<typename Exception, typename Test, typename... Args>
static inline void __debug_prevent_stream(const Test& test, Args&&... args) {
  if (test) {
    std::stringstream stream{};
    __detail::printArgs<Args...>(stream, std::forward<Args>(args)...);
    throw Exception(stream.str());
  }
}

// DEBUG_PREVENT
template<typename Exception, typename Test, typename... Args>
static inline void debug_prevent(const Test& test, Args&&... args) {
  __debug_prevent<Exception>(test, std::forward<Args>(args)...);
}
template<typename Exception, typename Test, typename... Args>
static inline void debug_prevent(Test&& test, Args&&... args) {
  __debug_prevent<Exception>(test, std::forward<Args>(args)...);
}
template<typename Exception, typename Predicate, typename... Args>
static inline void debug_prevent_predicate(Predicate test, Args&&... args) {
  __debug_prevent<Exception>(test(), std::forward<Args>(args)...);
}

template<typename Exception, typename Test, typename... Args>
static inline void debug_prevent_stream(const Test& test, Args&&... args) {
  __debug_prevent_stream<Exception>(test, std::forward<Args>(args)...);
}
template<typename Exception, typename Test, typename... Args>
static inline void debug_prevent_stream(Test&& test, Args&&... args) {
  __debug_prevent_stream<Exception>(test, std::forward<Args>(args)...);
}
template<typename Exception, typename Predicate, typename... Args>
static inline void debug_prevent_predicate_stream(Predicate test, Args&&... args) {
  __debug_prevent_stream<Exception>(test(), std::forward<Args>(args)...);
}

// DEBUG_ASSERT
template<typename Exception, typename Test, typename... Args>
static inline void debug_assert(const Test& test, Args&&... args) {
  __debug_prevent<Exception>(not test, std::forward<Args>(args)...);
}
template<typename Exception, typename Test, typename... Args>
static inline void debug_assert(Test&& test, Args&&... args) {
  __debug_prevent<Exception>(not test, std::forward<Args>(args)...);
}
template<typename Exception, typename Predicate, typename... Args>
static inline void debug_assert_predicate(Predicate test, Args&&... args) {
  __debug_prevent<Exception>(not test(), std::forward<Args>(args)...);
}

template<typename Exception, typename Test, typename... Args>
static inline void debug_assert_stream(const Test& test, Args&&... args) {
  __debug_prevent_stream<Exception>(not test, std::forward<Args>(args)...);
}
template<typename Exception, typename Test, typename... Args>
static inline void debug_assert_stream(Test&& test, Args&&... args) {
  __debug_prevent_stream<Exception>(not test, std::forward<Args>(args)...);
}

template<typename Exception, typename Predicate, typename... Args>
static inline void debug_assert_predicate_stream(Predicate test, Args&&... args) {
  __debug_prevent_stream<Exception>(not test(), std::forward<Args>(args)...);
}
} // namespace __detail

#ifndef NDEBUG
#define DEBUG_ASSERT(TYPE, ...) ImageGraph::internal::__detail::debug_assert<TYPE>(__VA_ARGS__)
#define DEBUG_ASSERT_P(TYPE, ...) ImageGraph::internal::__detail::debug_assert_predicate<TYPE>(__VA_ARGS__)
#define DEBUG_ASSERT_S(TYPE, ...) ImageGraph::internal::__detail::debug_assert_stream<TYPE>(__VA_ARGS__)
#define DEBUG_ASSERT_PS(TYPE, ...) ImageGraph::internal::__detail::debug_assert_predicate_stream<TYPE>(__VA_ARGS__)
#define DEBUG_ASSERT_D(TYPE, COND) \
  ImageGraph::internal::__detail::debug_assert_stream<TYPE>(COND, #COND, " is not true!")

#define DEBUG_PREVENT(TYPE, ...) ImageGraph::internal::__detail::debug_prevent<TYPE>(__VA_ARGS__)
#define DEBUG_PREVENT_P(TYPE, ...) ImageGraph::internal::__detail::debug_prevent_predicate<TYPE>(__VA_ARGS__)
#define DEBUG_PREVENT_S(TYPE, ...) ImageGraph::internal::__detail::debug_prevent_stream<TYPE>(__VA_ARGS__)
#define DEBUG_PREVENT_PS(TYPE, ...) ImageGraph::internal::__detail::debug_prevent_predicate_stream<TYPE>(__VA_ARGS__)
#define DEBUG_PREVENT_D(TYPE, COND) \
  ImageGraph::internal::__detail::debug_prevent_stream<TYPE>(COND, #COND, " should not have happened!")
#else
#define DEBUG_ASSERT(TYPE, ...)
#define DEBUG_ASSERT_P(TYPE, ...)
#define DEBUG_ASSERT_S(TYPE, ...)
#define DEBUG_ASSERT_PS(TYPE, ...)
#define DEBUG_ASSERT_D(TYPE, COND)

#define DEBUG_PREVENT(TYPE, ...)
#define DEBUG_PREVENT_P(TYPE, ...)
#define DEBUG_PREVENT_S(TYPE, ...)
#define DEBUG_PREVENT_PS(TYPE, ...)
#define DEBUG_PREVENT_D(TYPE, COND)
#endif
} // namespace ImageGraph::internal