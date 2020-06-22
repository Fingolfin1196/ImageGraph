#pragma once

#include "../core/Tile.hpp"
#include <vector>

namespace ImageGraph::internal {
struct TileRegion {
  using rectangle_t = Rectangle<std::size_t>;

private:
  const rectangle_t rectangle_;

public:
  rectangle_t rectangle() const { return rectangle_; }

  TileRegion(rectangle_t rectangle) : rectangle_{rectangle} {}
};
} // namespace ImageGraph::internal
