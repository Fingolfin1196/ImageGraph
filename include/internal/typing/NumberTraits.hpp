#pragma once

#include "../../core/Definitions.hpp"
#include <cstdint>
#include <limits>
#include <type_traits>

namespace ImageGraph::internal {

// is_floating_point and is_integral.

template<typename T> struct is_floating_point { static inline constexpr bool value{std::is_floating_point_v<T>}; };
template<typename T> static inline constexpr bool is_floating_point_v = is_floating_point<T>::value;

template<typename T> struct is_integral { static inline constexpr bool value{std::is_integral_v<T>}; };
template<typename T> static inline constexpr bool is_integral_v = is_integral<T>::value;

// is_signed

template<typename T> struct is_signed : public std::is_signed<T> {};
template<class T> inline constexpr bool is_signed_v = is_signed<T>::value;

// unsign

template<typename T> struct unsign { using type = std::make_unsigned_t<T>; };
template<class T> using unsign_t = typename unsign<T>::type;

// common_type.

template<typename T1, typename T2> struct common_type { using type = typename std::common_type_t<T1, T2>; };
template<typename T1, typename T2> using common_type_t = typename common_type<T1, T2>::type;

// least_floating_point.

template<typename T> struct least_floating_point {};
template<> struct least_floating_point<uint8_t> { using type = float32_t; };
template<> struct least_floating_point<uint16_t> { using type = float32_t; };
template<> struct least_floating_point<uint32_t> { using type = float64_t; };
template<> struct least_floating_point<int8_t> { using type = float32_t; };
template<> struct least_floating_point<int16_t> { using type = float32_t; };
template<> struct least_floating_point<int32_t> { using type = float64_t; };
template<> struct least_floating_point<float32_t> { using type = float32_t; };
template<> struct least_floating_point<float64_t> { using type = float64_t; };
template<typename T> using least_floating_point_t = typename least_floating_point<T>::type;

template<typename T1, typename T2> using least_common_floating_point_t =
    common_type_t<least_floating_point_t<T1>, least_floating_point_t<T2>>;

// white_point.

template<typename T> struct white_point {};
template<> struct white_point<uint8_t> { constexpr static uint8_t value{std::numeric_limits<uint8_t>::max()}; };
template<> struct white_point<uint16_t> { constexpr static uint16_t value{std::numeric_limits<uint16_t>::max()}; };
template<> struct white_point<uint32_t> { constexpr static uint32_t value{std::numeric_limits<uint32_t>::max()}; };
template<> struct white_point<int8_t> { constexpr static int8_t value{std::numeric_limits<int8_t>::max()}; };
template<> struct white_point<int16_t> { constexpr static int16_t value{std::numeric_limits<int16_t>::max()}; };
template<> struct white_point<int32_t> { constexpr static int32_t value{std::numeric_limits<int32_t>::max()}; };
template<> struct white_point<float32_t> { constexpr static float32_t value{1.f}; };
template<> struct white_point<float64_t> { constexpr static float64_t value{1.}; };
template<typename T> inline constexpr T white_point_v = white_point<T>::value;

// is_luttable.

template<typename T> struct is_luttable { constexpr static bool value{false}; };
template<> struct is_luttable<uint8_t> { constexpr static bool value{true}; };
template<> struct is_luttable<int8_t> { constexpr static bool value{true}; };
template<> struct is_luttable<uint16_t> { constexpr static bool value{true}; };
template<> struct is_luttable<int16_t> { constexpr static bool value{true}; };
template<typename T> static inline constexpr bool is_luttable_v = is_luttable<T>::value;
} // namespace ImageGraph::internal