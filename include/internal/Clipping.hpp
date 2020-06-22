#pragma once

#include <boost/geometry/geometry.hpp>
#include <deque>
#include <iostream>

namespace ImageGraph::internal {
namespace boost_geometry {
template<typename T> using point_t = boost::geometry::model::d2::point_xy<T>;
template<typename T> using box_t = boost::geometry::model::box<point_t<T>>;
template<typename T> using ring_t = boost::geometry::model::ring<point_t<T>>;
template<typename T> using polygon_t = boost::geometry::model::polygon<point_t<T>>;
template<typename T> using multi_polygon_t = boost::geometry::model::multi_polygon<polygon_t<T>>;

template<typename T> static inline point_t<T>& operator+=(point_t<T>& p1, const point_t<T>& p2) {
  using namespace boost::geometry;
  set<0>(p1, get<0>(p1) + get<0>(p2)), set<1>(p1, get<1>(p1) + get<1>(p2));
  return p1;
}
template<typename T> static inline point_t<T> operator+(point_t<T> p1, const point_t<T>& p2) {
  return p1 += p2;
}
template<typename T> static inline point_t<T>& operator*=(point_t<T>& p, const T& c) {
  using namespace boost::geometry;
  set<0>(p, get<0>(p) * c), set<1>(p, get<1>(p) * c);
  return p;
}
template<typename T> static inline point_t<T> operator*(point_t<T> p, const T& c) { return p *= c; }
template<typename T> static inline point_t<T>& operator/=(point_t<T>& p, const T& c) {
  using namespace boost::geometry;
  set<0>(p, get<0>(p) / c), set<1>(p, get<1>(p) / c);
  return p;
}
template<typename T> static inline point_t<T> operator/(point_t<T> p, const T& c) { return p /= c; }
template<typename T>
static inline std::ostream& operator<<(std::ostream& stream, const point_t<T>& p) {
  using namespace boost::geometry;
  return stream << "[" << get<0>(p) << ", " << get<1>(p) << "]";
}
} // namespace boost_geometry

namespace detail {
using out_code_t = uint8_t;
constexpr out_code_t INSIDE{0}, LEFT{1}, RIGHT{2}, BOTTOM{4}, TOP{8};

// Compute the bit code for a point (x, y) using the clip rectangle bounded diagonally by (xmin,
// ymin), and (xmax, ymax)
template<typename T> constexpr static inline out_code_t bitCode(boost_geometry::point_t<T> p,
                                                                boost_geometry::box_t<T> rect) {
  using namespace boost::geometry;
  return (get<0>(p) < get<min_corner, 0>(rect)
              ? LEFT
              : (get<0>(p) > get<max_corner, 0>(rect) ? RIGHT : INSIDE)) |
         (get<1>(p) < get<min_corner, 1>(rect)
              ? BOTTOM
              : (get<1>(p) > get<max_corner, 1>(rect) ? TOP : INSIDE));
}

/**
 * Intersects a segment against one of the 4 lines that make up the bounding box.
 */
template<typename T>
boost_geometry::point_t<T> intersect(boost_geometry::point_t<T> a, boost_geometry::point_t<T> b,
                                     out_code_t edge, boost_geometry::box_t<T> bounding_box) {
  using namespace boost::geometry;
  if (edge & TOP)
    return {get<0>(a) + (get<0>(b) - get<0>(a)) * (get<max_corner, 1>(bounding_box) - get<1>(a)) /
                            (get<1>(b) - get<1>(a)),
            T(get<max_corner, 1>(bounding_box))};
  else if (edge & BOTTOM)
    return {get<0>(a) + (get<0>(b) - get<0>(a)) * (get<min_corner, 1>(bounding_box) - get<1>(a)) /
                            (get<1>(b) - get<1>(a)),
            T(get<min_corner, 1>(bounding_box))};
  else if (edge & RIGHT)
    return {T(get<max_corner, 0>(bounding_box)),
            get<1>(a) + (get<1>(b) - get<1>(a)) * (get<max_corner, 0>(bounding_box) - get<0>(a)) /
                            (get<0>(b) - get<0>(a))};
  else if (edge & LEFT)
    return {T(get<min_corner, 0>(bounding_box)),
            get<1>(a) + (get<1>(b) - get<1>(a)) * (get<min_corner, 0>(bounding_box) - get<0>(a)) /
                            (get<0>(b) - get<0>(a))};
  throw std::invalid_argument("edge is not valid!");
}
} // namespace detail

/**
 * Sutherland-Hodgeman polygon clipping algorithm.
 */
template<typename T, typename It> boost_geometry::ring_t<T>
clipRing(It ring_begin, It ring_end, const boost_geometry::box_t<T>& bounding_box) {
  using namespace boost_geometry;

  std::deque<point_t<T>> inner{ring_begin, ring_end};
  if (inner.empty()) return {};
  inner.pop_front();
  // clip against each side of the clip rectangle
  for (detail::out_code_t edge{1}; edge <= 8; edge *= 2) {
    point_t<T> prev{inner.back()};
    bool prev_inside{not(detail::bitCode(prev, bounding_box) & edge)};

    const size_t size{inner.size()};
    for (size_t i{0}; i < size; ++i) {
      point_t<T> p{std::move(inner.front())};
      inner.pop_front();
      bool inside{not(detail::bitCode(p, bounding_box) & edge)};
      // Add the interection point with the clip window if the segment goes through the clip window.
      if (inside != prev_inside) inner.push_back(detail::intersect(prev, p, edge, bounding_box));
      // Add the point again if it is inside.
      if (inside) inner.push_back(p);
      prev = std::move(p);
      prev_inside = inside;
    }
    if (inner.empty()) break;
  }
  if (inner.size() > 1) inner.push_back(inner.front());
  return {inner.begin(), inner.end()};
}
template<typename T, typename Ring>
boost_geometry::ring_t<T> clipRing(const Ring& ring, const boost_geometry::box_t<T>& bounding_box) {
  return clipRing(ring.begin(), ring.end(), bounding_box);
}
} // namespace ImageGraph::internal
