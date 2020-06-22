#include "VertexOrder.hpp"
#include "internal/Random.hpp"
#include <iostream>
#include <limits>
#include <pcg_random.hpp>

template<typename T> static inline void testInfinityOverlap(ImageGraph::internal::boost_geometry::point_t<T> c1,
                                                            ImageGraph::internal::boost_geometry::point_t<T> c2, T p,
                                                            const ImageGraph::internal::boost_geometry::box_t<T> box) {
  using namespace boost::geometry;
  using namespace ImageGraph::internal;
  using namespace ImageGraph::internal::boost_geometry;
  using ring_t = ring_t<T>;

  const T one_p{1 - p}, p_div{p / one_p}, root_p{std::sqrt(p)}, root_p_div{root_p / one_p},
      x_dif{get<0>(c1) - get<0>(c2)}, x_diff{std::abs(x_dif)}, y_dif{get<1>(c1) - get<1>(c2)}, y_diff{std::abs(y_dif)};
  const T cx_div{get<0>(c1) / one_p - get<0>(c2) * p_div}, cy_div{get<1>(c1) / one_p - get<1>(c2) * p_div},
      root_x_div{x_diff * root_p_div}, root_y_div{y_diff * root_p_div};
  const T alpha{cx_div + root_x_div}, beta{cx_div - root_x_div}, gamma{cy_div + root_y_div}, delta{cy_div - root_y_div};
  const T epsilon{x_dif * p_div}, zeta{y_dif * p_div}, eta{x_dif / one_p}, theta{y_dif / one_p}, iota{root_x_div},
      kappa{root_y_div};

  const T t_alpha{x_dif - y_dif}, t_beta{x_dif + y_dif}, t_gamma{2 * root_p * x_diff}, t_delta{2 * root_p * y_diff};
  std::array<point_t<T>, 17> raw_polygon{};
  size_t index{0};
  if (t_alpha * (-p * t_alpha - t_beta - t_gamma) <= 0) raw_polygon[index++] = {alpha, get<1>(c1) + epsilon + iota};
  if (t_alpha * (-p * t_alpha - t_beta + t_gamma) <= 0) raw_polygon[index++] = {beta, get<1>(c1) + epsilon - iota};
  if (t_alpha * (-p * t_alpha + t_beta + t_delta) <= 0) raw_polygon[index++] = {get<0>(c1) + zeta + kappa, gamma};
  if (t_alpha * (-p * t_alpha + t_beta - t_delta) <= 0) raw_polygon[index++] = {get<0>(c1) + zeta - kappa, delta};
  if (t_beta * (-t_alpha - p * t_beta - t_gamma) <= 0) raw_polygon[index++] = {alpha, get<1>(c1) - epsilon - iota};
  if (t_beta * (-t_alpha - p * t_beta + t_gamma) <= 0) raw_polygon[index++] = {beta, get<1>(c1) - epsilon + iota};
  if (t_beta * (t_alpha - p * t_beta - t_delta) <= 0) raw_polygon[index++] = {get<0>(c1) - zeta - kappa, gamma};
  if (t_beta * (t_alpha - p * t_beta + t_delta) <= 0) raw_polygon[index++] = {get<0>(c1) - zeta + kappa, delta};
  if (t_alpha * (t_alpha + p * t_beta + t_gamma) <= 0) raw_polygon[index++] = {alpha, get<1>(c2) + eta + iota};
  if (t_alpha * (t_alpha + p * t_beta - t_gamma) <= 0) raw_polygon[index++] = {beta, get<1>(c2) + eta - iota};
  if (t_alpha * (t_alpha - p * t_beta - t_delta) <= 0) raw_polygon[index++] = {get<0>(c2) + theta + kappa, gamma};
  if (t_alpha * (t_alpha - p * t_beta + t_delta) <= 0) raw_polygon[index++] = {get<0>(c2) + theta - kappa, delta};
  if (t_beta * (p * t_alpha + t_beta + t_gamma) <= 0) raw_polygon[index++] = {alpha, get<1>(c2) - eta - iota};
  if (t_beta * (p * t_alpha + t_beta - t_gamma) <= 0) raw_polygon[index++] = {beta, get<1>(c2) - eta + iota};
  if (t_beta * (-p * t_alpha + t_beta + t_delta) <= 0) raw_polygon[index++] = {get<0>(c2) - theta - kappa, gamma};
  if (t_beta * (-p * t_alpha + t_beta - t_delta) <= 0) raw_polygon[index++] = {get<0>(c2) - theta + kappa, delta};
  std::sort(raw_polygon.begin(), raw_polygon.begin() + index, VertexLess<T>(c1));
  // Close the polygon!
  raw_polygon[index++] = raw_polygon.front();
  // std::cout << "is_valid: " << std::boolalpha << is_valid(polygon) << std::endl;

  // std::cout << std::setprecision(std::numeric_limits<T>::digits10);
  // for (const auto& point : polygon) std::cout << point << " ";
  // std::cout << std::endl;
  // std::cout << area(polygon) << std::endl;

  ring_t polygon{clipRing<T>(raw_polygon.begin(), raw_polygon.begin() + index, box)};
  for (const auto& point : polygon) std::cout << point << " ";
  std::cout << std::endl;
  std::cout << "is_valid: " << std::boolalpha << is_valid(polygon) << std::endl;
  std::cout << area(polygon) << " : " << area(polygon) / area(box) << std::endl;
}

int main() {
  using num_t = float;
  std::cout << "digits10: " << std::numeric_limits<num_t>::digits10 << std::endl << std::endl;
  using namespace ImageGraph::internal;
  pcg64 generator{pcg_extras::seed_seq_from<std::random_device>()};
  const num_t min{0}, max{511};
  for (size_t i{0}; i < 1ul << 26; ++i)
    testInfinityOverlap<num_t>({random_real<num_t>(generator, min, max), random_real<num_t>(generator, min, max)},
                               {random_real<num_t>(generator, min, max), random_real<num_t>(generator, min, max)},
                               random_norm<num_t>(generator), {{min, min}, {max, max}});

  return 0;
}