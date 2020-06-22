#pragma once

#include "../TileRegion.hpp"
#include "../generators/GeneralizedHilbert.hpp"

namespace ImageGraph::internal {
class HilbertRegion final : public TileRegion {
  using int_t = long;
  using dimensions_t = RectangleDimensions<std::size_t>;

  const dimensions_t node_, tile_;
  std::optional<gilbert_pull_t<int_t>> curve_;

  static std::optional<gilbert_pull_t<int_t>> generateGenerator(rectangle_t rect, dimensions_t tile) {
    if (rect.empty() or tile.empty()) return std::nullopt;
    const std::size_t tile_width{tile.width()}, tile_height{tile.height()}, left{rect.left() / tile_width},
        right{(rect.left() + rect.width() - 1) / tile_width}, top{rect.top() / tile_height},
        bottom{(rect.top() + rect.height() - 1) / tile_height};
    return gilbert2d_generator<int_t>(left, top, right - left + 1, bottom - top + 1);
  }

public:
  HilbertRegion(rectangle_t rectangle, dimensions_t node, dimensions_t tile)
      : TileRegion(rectangle), node_{node}, tile_{tile}, curve_{generateGenerator(rectangle, tile)} {}

  template<typename F> static inline void perform(rectangle_t rect, dimensions_t node, dimensions_t tile, F functor) {
    if (rect.empty() or tile.empty()) return;
    const std::size_t tile_width{tile.width()}, tile_height{tile.height()}, left{rect.left() / tile_width},
        right{(rect.left() + rect.width() - 1) / tile_width}, top{rect.top() / tile_height},
        bottom{(rect.top() + rect.height() - 1) / tile_height};
    gilbert2d<int_t>(left, top, right - left + 1, bottom - top + 1, [&node, &tile, &functor](Point<int_t> p) {
      functor(rectangle_t(Point<std::size_t>(p.x() * tile.width(), p.y() * tile.height()), tile).clip(node));
    });
  }

  bool remaining() const { return bool(curve_) and bool(*curve_); }
  rectangle_t next() {
    Point p{curve_.value().get()};
    curve_.value()();
    return rectangle_t(Point<std::size_t>(p.x() * tile_.width(), p.y() * tile_.height()), tile_).clip(node_);
  }
};
} // namespace ImageGraph::internal