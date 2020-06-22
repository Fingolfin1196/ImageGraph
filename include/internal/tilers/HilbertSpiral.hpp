#pragma once

#include "../TileRegion.hpp"
#include "../generators/HilbertSpiral.hpp"

namespace ImageGraph::internal {
class HilbertSpiralRegion final : public TileRegion {
  using int_t = long;
  using point_t = Point<std::size_t>;
  using dimensions_t = RectangleDimensions<std::size_t>;

  const dimensions_t node_, tile_, region_;
  std::optional<gilbert_pull_t<int_t>> curve_;

  static std::optional<gilbert_pull_t<int_t>> generateCurve(rectangle_t rect, dimensions_t tile,
                                                                   dimensions_t region, point_t centre) {
    if (rect.empty() or tile.empty() or region.empty()) return std::nullopt;

    const std::size_t tile_width{tile.width()}, tile_height{tile.height()}, right{rect.left() + rect.width() - 1},
        bottom{rect.top() + rect.height() - 1};
    return gilbert_spiral_generator<int_t>(centre.x() / tile_width, centre.y() / tile_height, rect.left() / tile_width,
                                           rect.top() / tile_height, right / tile_width, bottom / tile_height,
                                           region.width(), region.height());
  }

public:
  HilbertSpiralRegion(rectangle_t rectangle, point_t centre, dimensions_t node, dimensions_t tile, dimensions_t region)
      : TileRegion(rectangle), node_{node}, tile_{tile}, region_{region}, curve_{generateCurve(rectangle, tile, region,
                                                                                               centre)} {}

  bool remaining() const { return bool(curve_) and bool(*curve_); }
  rectangle_t next() {
    Point int_point{curve_.value().get()};
    curve_.value()();
    Point<std::size_t> point{int_point.x() * tile_.width(), int_point.y() * tile_.height()};
    rectangle_t rectangle{point, tile_};
    rectangle.clip(node_);
    return rectangle;
  }
};
} // namespace ImageGraph::internal