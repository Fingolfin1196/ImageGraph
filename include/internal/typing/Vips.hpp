#pragma once

#include "../../core/Definitions.hpp"
#include <optional>
#include <vips/vips8>

namespace ImageGraph::internal {
template<typename T> struct band_format {};
template<> struct band_format<uint8_t> { constexpr static VipsBandFormat value{VipsBandFormat::VIPS_FORMAT_UCHAR}; };
template<> struct band_format<uint16_t> { constexpr static VipsBandFormat value{VipsBandFormat::VIPS_FORMAT_USHORT}; };
template<> struct band_format<uint32_t> { constexpr static VipsBandFormat value{VipsBandFormat::VIPS_FORMAT_UINT}; };
template<> struct band_format<int8_t> { constexpr static VipsBandFormat value{VipsBandFormat::VIPS_FORMAT_CHAR}; };
template<> struct band_format<int16_t> { constexpr static VipsBandFormat value{VipsBandFormat::VIPS_FORMAT_SHORT}; };
template<> struct band_format<int32_t> { constexpr static VipsBandFormat value{VipsBandFormat::VIPS_FORMAT_INT}; };
template<> struct band_format<float32_t> { constexpr static VipsBandFormat value{VipsBandFormat::VIPS_FORMAT_FLOAT}; };
template<> struct band_format<float64_t> { constexpr static VipsBandFormat value{VipsBandFormat::VIPS_FORMAT_DOUBLE}; };
template<typename T> inline constexpr VipsBandFormat band_format_v = band_format<T>::value;
} // namespace ImageGraph::internal

static inline std::ostream& operator<<(std::ostream& stream, VipsBandFormat format) {
  switch (format) {
    case VIPS_FORMAT_NOTSET: return stream << "VIPS_FORMAT_NOTSET";
    case VIPS_FORMAT_UCHAR: return stream << "VIPS_FORMAT_UCHAR";
    case VIPS_FORMAT_CHAR: return stream << "VIPS_FORMAT_CHAR";
    case VIPS_FORMAT_USHORT: return stream << "VIPS_FORMAT_USHORT";
    case VIPS_FORMAT_SHORT: return stream << "VIPS_FORMAT_SHORT";
    case VIPS_FORMAT_UINT: return stream << "VIPS_FORMAT_UINT";
    case VIPS_FORMAT_INT: return stream << "VIPS_FORMAT_INT";
    case VIPS_FORMAT_FLOAT: return stream << "VIPS_FORMAT_FLOAT";
    case VIPS_FORMAT_COMPLEX: return stream << "VIPS_FORMAT_COMPLEX";
    case VIPS_FORMAT_DOUBLE: return stream << "VIPS_FORMAT_DOUBLE";
    case VIPS_FORMAT_DPCOMPLEX: return stream << "VIPS_FORMAT_DPCOMPLEX";
    case VIPS_FORMAT_LAST: return stream << "VIPS_FORMAT_LAST";
  }
  return stream;
}

namespace ImageGraph {
template<typename Perform, typename Return, typename... Args>
static inline std::optional<Return> perform_from_vips(VipsBandFormat format, Args&&... args) {
  switch (format) {
    case VIPS_FORMAT_UCHAR: return Perform::template perform<uint8_t>(std::forward<Args>(args)...);
    case VIPS_FORMAT_USHORT: return Perform::template perform<uint16_t>(std::forward<Args>(args)...);
    case VIPS_FORMAT_UINT: return Perform::template perform<uint32_t>(std::forward<Args>(args)...);
    case VIPS_FORMAT_CHAR: return Perform::template perform<int8_t>(std::forward<Args>(args)...);
    case VIPS_FORMAT_SHORT: return Perform::template perform<int16_t>(std::forward<Args>(args)...);
    case VIPS_FORMAT_INT: return Perform::template perform<int32_t>(std::forward<Args>(args)...);
    case VIPS_FORMAT_FLOAT: return Perform::template perform<float32_t>(std::forward<Args>(args)...);
    case VIPS_FORMAT_DOUBLE: return Perform::template perform<float64_t>(std::forward<Args>(args)...);
    default: return std::nullopt;
  }
}
} // namespace ImageGraph
