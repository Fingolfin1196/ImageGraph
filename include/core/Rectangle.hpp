#pragma once

#include "../internal/Hashing.hpp"
#include <algorithm>
#include <cmath>
#include <ostream>
#include <stdexcept>
#include <type_traits>

namespace ImageGraph {
template<typename Type> class Point {
  Type x_, y_;

public:
  Type& x() { return x_; }
  const Type& x() const { return x_; }
  Type& y() { return y_; }
  const Type& y() const { return y_; }

  bool operator==(const Point& other) const { return x_ == other.x_ and y_ == other.y_; }
  bool operator!=(const Point& other) const { return not(*this == other); }

  Point& operator*=(const Type& factor) {
    x_ *= factor, y_ *= factor;
    return *this;
  }

  constexpr Point(Type x, Type y) : x_{x}, y_{y} {}
  constexpr Point() : Point(0, 0) {}
};

template<typename Type> class RectangleDimensions {
  Type width_, height_;

public:
  Type& width() { return width_; }
  const Type& width() const { return width_; }
  Type& height() { return height_; }
  const Type& height() const { return height_; }

  Type size() const { return width_ * height_; }
  bool empty() const { return not size(); }

  RectangleDimensions bound(const RectangleDimensions& other) const {
    return {std::max(width_, other.width_), std::max(height_, other.height_)};
  }

  bool operator==(const RectangleDimensions& other) const {
    return width_ == other.width_ and height_ == other.height_;
  }
  bool operator!=(const RectangleDimensions& other) const { return not(*this == other); }

  constexpr RectangleDimensions(Type width, Type height) : width_{width}, height_{height} {}
  constexpr RectangleDimensions(Type dim) : RectangleDimensions(dim, dim) {}
  constexpr RectangleDimensions() : RectangleDimensions(0) {}
};

/**
 * Counts from the top left corner.
 */
template<typename Type> struct Rectangle {
  using point_t = Point<Type>;
  using dimensions_t = RectangleDimensions<Type>;

private:
  point_t point_;
  dimensions_t dimensions_;

public:
  point_t& point() { return point_; }
  const point_t& point() const { return point_; }
  Type& left() { return point_.x(); }
  const Type& left() const { return point_.x(); }
  Type& top() { return point_.y(); }
  const Type& top() const { return point_.y(); }

  dimensions_t& dimensions() { return dimensions_; }
  const dimensions_t& dimensions() const { return dimensions_; }
  Type& width() { return dimensions_.width(); }
  const Type& width() const { return dimensions_.width(); }
  Type& height() { return dimensions_.height(); }
  const Type& height() const { return dimensions_.height(); }

  Type size() const { return dimensions_.size(); }
  bool empty() const { return dimensions_.empty(); }

  Rectangle& regularise() {
    if constexpr (std::is_signed_v<Type>) {
      constexpr Type _0{0};
      if (width() < _0) left() += width(), width() = -width();
      if (height() < _0) top() += height(), height() = -height();
    }
    return *this;
  }
  Rectangle& removeNegative() {
    constexpr Type _0{0};
    if constexpr (std::is_signed_v<Type>) {
      regularise();
      if (left() < _0) {
        width() = std::max(width() + left(), _0);
        left() = _0;
      }
      if (top() < _0) {
        height() = std::max(height() + top(), _0);
        top() = _0;
      }
    }
    return *this;
  }

  Rectangle& clip(const Rectangle& clipper) {
    const Type clipper_right{clipper.left() + clipper.width()}, clipper_bottom{clipper.top() + clipper.height()},
        new_left{std::clamp(left(), clipper.left(), clipper_right)},
        new_top{std::clamp(top(), clipper.top(), clipper_bottom)};
    left() = new_left, top() = new_top,
    width() = std::clamp(left() + width(), clipper.left(), clipper_right) - new_left,
    height() = std::clamp(top() + height(), clipper.top(), clipper_bottom) - new_top;
    return *this;
  }
  Rectangle& clip(RectangleDimensions<Type> dimensions) {
    constexpr Type _0{0};
    removeNegative();
    if (left() > dimensions.width())
      left() = dimensions.width(), width() = _0;
    else if (left() + width() > dimensions.width())
      width() = dimensions.width() - left();
    if (top() > dimensions.height())
      top() = dimensions.height(), height() = _0;
    else if (top() + height() > dimensions.height())
      height() = dimensions.height() - top();
    return *this;
  }
  bool subsetOf(const Rectangle& other) const {
    if constexpr (std::is_unsigned_v<Type>)
      return left() >= other.left() and top() >= other.top() and left() + width() <= other.left() + other.width() and
             top() + height() <= other.top() + other.height();
    else {
      Rectangle reg{other};
      reg.regularise();
      return left() >= reg.left() and top() >= reg.top() and left() + width() <= reg.left() + reg.width() and
             top() + height() <= reg.top() + reg.height();
    }
  }

  Type overlap(const Rectangle& other) const {
    constexpr Type _0{0};
    const Type min_right{std::min(left() + width(), other.left() + other.width())},
        max_left{std::max(left(), other.left())}, min_bottom{std::min(top() + height(), other.top() + other.height())},
        max_top{std::max(top(), other.top())};
    return (min_right > max_left ? min_right - max_left : _0) * (min_bottom > max_top ? min_bottom - max_top : _0);
  }
  Rectangle bound(const Rectangle& other) const {
    const Type min_left{std::min(left(), other.left())},
        max_right{std::max(left() + width(), other.left() + other.width())}, min_top{std::min(top(), other.top())},
        max_bottom{std::max(top() + height(), other.top() + other.height())};
    return {min_left, min_top, max_right - min_left, max_bottom - min_top};
  }

  Rectangle& extend(const Type e_left, const Type e_top, const Type e_right, const Type e_bottom) {
    static_assert(std::is_unsigned_v<Type> != std::is_signed_v<Type>);
    if constexpr (std::is_unsigned_v<Type>) {
      const Type right{left() + width()}, bottom{top() + height()};
      left() = left() > e_left ? left() - e_left : 0, top() = top() > e_top ? top() - e_top : 0;
      width() = right + e_right - left(), height() = bottom + e_bottom - top();
    }
    if constexpr (std::is_signed_v<Type>)
      left() = left() - e_left, top() = top() - e_top, width() = width() + e_left + e_right,
      height() = height() + e_top + e_bottom;
    return *this;
  }
  Rectangle& extend(Type extent) { return extend(extent, extent, extent, extent); }

  Rectangle& scale(Type scale_x, Type scale_y) {
    left() *= scale_x, width() *= scale_x, top() *= scale_y, height() *= scale_y;
    return regularise();
  }

  template<typename OutType>
  std::enable_if_t<std::is_floating_point_v<Type> and std::is_integral_v<OutType>, Rectangle<OutType>>
  boundingRectangle() const {
    Rectangle copy{*this};
    if constexpr (std::is_unsigned_v<OutType>) copy.removeNegative();
    const OutType left(copy.left()), top(copy.top());
    return {
        {left, top},
        {OutType(std::ceil(copy.left() + copy.width())) - left, OutType(std::ceil(copy.top() + copy.height())) - top}};
  }

  template<typename OutType>
  std::enable_if_t<std::is_integral_v<Type> and std::is_floating_point_v<OutType>, Rectangle<OutType>>
  toFloatingPoint() const {
    return {{OutType(left()), OutType(top())}, {OutType(width()), OutType(height())}};
  }

  friend std::ostream& operator<<(std::ostream& stream, const Rectangle& rectangle) {
    return stream << "[" << rectangle.left() << ", " << rectangle.top() << "; " << rectangle.width() << ", "
                  << rectangle.height() << "]";
  }

  bool operator==(const Rectangle& other) const {
    return left() == other.left() and top() == other.top() and width() == other.width() and height() == other.height();
  }
  bool operator!=(const Rectangle& other) const { return not(*this == other); }

  constexpr Rectangle(point_t point, dimensions_t dimensions)
      : point_{std::move(point)}, dimensions_{std::move(dimensions)} {}
  constexpr Rectangle(dimensions_t dimensions) : Rectangle({}, std::move(dimensions)) {}
};
} // namespace ImageGraph

namespace std {
template<typename T> struct hash<ImageGraph::Point<T>> {
  std::size_t operator()(const ImageGraph::Point<T>& dims) const {
    return ImageGraph::internal::hash_combine(std::hash<T>()(dims.x()), std::hash<T>()(dims.y()));
  }
};
template<typename T> struct hash<ImageGraph::RectangleDimensions<T>> {
  std::size_t operator()(const ImageGraph::RectangleDimensions<T>& dims) const {
    return ImageGraph::internal::hash_combine(std::hash<T>()(dims.width()), std::hash<T>()(dims.height()));
  }
};
template<typename T> struct hash<ImageGraph::Rectangle<T>> {
  std::size_t operator()(const ImageGraph::Rectangle<T>& rect) const {
    using namespace ImageGraph;
    return internal::hash_combine(std::hash<Point<T>>()(rect.point()),
                                  std::hash<RectangleDimensions<T>>()(rect.dimensions()));
  }
};
} // namespace std
