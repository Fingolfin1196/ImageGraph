#include "internal/Clipping.hpp"

template<typename T> class VertexLess {
  using point_t = ImageGraph::internal::boost_geometry::point_t<T>;
  point_t center;

public:
  explicit VertexLess(point_t center) : center{std::move(center)} {}

  bool operator()(const point_t& a, const point_t& b) const {
    using namespace boost::geometry;
    if (get<0>(a) >= get<0>(center) and get<0>(b) < get<0>(center)) return true;
    if (get<0>(a) < get<0>(center) and get<0>(b) >= get<0>(center)) return false;
    if (get<0>(a) == get<0>(center) and get<0>(b) == get<0>(center)) {
      if (get<1>(a) >= get<1>(center) or get<1>(b) >= get<1>(center)) return get<1>(a) > get<1>(b);
      return get<1>(b) > get<1>(a);
    }
    // Compute the cross product of vectors (center -> a) × (center -> b)
    const T det{(get<0>(a) - get<0>(center)) * (get<1>(b) - get<1>(center)) -
                (get<0>(b) - get<0>(center)) * (get<1>(a) - get<1>(center))};
    if (det != 0) return det < 0;
    // Points a and b are on the same line from the center:
    // Check which point is closer to the center
    return (get<0>(a) - get<0>(center)) * (get<0>(a) - get<0>(center)) +
               (get<1>(a) - get<1>(center)) * (get<1>(a) - get<1>(center)) >
           (get<0>(b) - get<0>(center)) * (get<0>(b) - get<0>(center)) +
               (get<1>(b) - get<1>(center)) * (get<1>(b) - get<1>(center));
  }
};
