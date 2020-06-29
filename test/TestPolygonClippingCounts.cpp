#include "VertexOrder.hpp"
#include "internal/PCG.hpp"
#include "internal/Random.hpp"
#include <boost/range/irange.hpp>
#include <execution>
#include <iostream>
#include <utility>
#include <vector>

static std::size_t polygonClippingCountLoop(const std::size_t size) {
  using namespace ImageGraph::internal;
  using namespace ImageGraph::internal::boost_geometry;

  using num_t = double;
  using point_t = point_t<num_t>;
  using ring_t = ring_t<num_t>;
  using multi_polygon_t = multi_polygon_t<num_t>;

  std::size_t max_clipped{};
  pcg64 generator{pcg_extras::seed_seq_from<std::random_device>()};
  const box_t<num_t> rect{{.2, .2}, {.8, .8}};
  const std::size_t upper{1ul << 14ul};
  constexpr bool boosting{false};
  for (std::size_t i{0}; i < upper; ++i) {
    ring_t polygon{};
    polygon.reserve(size + 1);
    point_t center{0, 0};
    num_t factor_sum{0};
    for (std::size_t j{0}; j < size; ++j) {
      point_t p{random_norm<num_t>(generator), random_norm<num_t>(generator)};
      const num_t factor{random_norm<num_t>(generator)};
      factor_sum += factor;
      center += p * factor;
      polygon.emplace_back(p);
    }
    center /= factor_sum;
    std::sort(polygon.begin(), polygon.end(), VertexLess<num_t>(center));
    // Close the polygon!
    polygon.emplace_back(polygon.front());
    ring_t clipped;
    if constexpr (boosting) {
      multi_polygon_t multi{};
      boost::geometry::intersection(polygon, rect, multi);
      if (multi.size() != 1) continue;
      polygon_t<num_t> poly{std::move(multi[0])};
      if (not poly.inners().empty()) throw std::runtime_error("The resulting polygon should not have inner rings!");
      clipped = std::move(poly.outer());
    } else
      clipped = clipRing<num_t>(polygon, rect);
    max_clipped = std::max(max_clipped, clipped.size());
  }
  std::cout << size << ": " << max_clipped << std::endl;
  return max_clipped;
}

int main() {
  using pair_t = std::pair<std::size_t, std::size_t>;

  const std::size_t max{128}, min{3};
  std::vector<pair_t> clipped_counts(max - min + 1);
  auto range{boost::irange(min, max + 1)};
  std::transform(std::execution::par_unseq, range.begin(), range.end(), clipped_counts.begin(),
                 [](std::size_t size) -> pair_t {
                   return {size, polygonClippingCountLoop(size)};
                 });
  std::cout << std::endl;
  for (const auto& [size, max_clipped] : clipped_counts) std::cout << size << ": " << max_clipped << std::endl;

  return 0;
}
