#include "core/Rectangle.hpp"
#include "core/Tile.hpp"
#include "internal/BicubicInterpolator.hpp"
#include <iostream>

int main() {
  using namespace ImageGraph;
  using namespace ImageGraph::internal;

  const std::size_t channels{3};

  Rectangle<std::size_t> in_rect{{3, 3}};
  Tile<float> in_tile{in_rect, channels};
  for (std::size_t j{0}; j < in_tile.height(); ++j)
    for (std::size_t i{0}; i < in_tile.width(); ++i)
      for (std::size_t k{0}; k < in_tile.channels(); ++k) in_tile(i, j, k) = (i * j + i + j) % (k + channels);
  std::cout << in_tile << std::endl;

  BicubicInterpolator<float, float, Tile> interpolator{in_tile, [](float value) { return value; }};

  Rectangle<std::size_t> out_rect{{6, 6}};
  Tile<float> out_tile{out_rect, channels};
  for (std::size_t j{0}; j < out_tile.height(); ++j)
    for (std::size_t i{0}; i < out_tile.width(); ++i)
      for (std::size_t k{0}; k < out_tile.channels(); ++k) out_tile(i, j, k) = interpolator.evaluate(.5 * i, .5 * j, k);
  std::cout << out_tile << std::endl;

  return 0;
}
