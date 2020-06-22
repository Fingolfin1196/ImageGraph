#include "internal/generators/GeneralizedHilbert.hpp"
#include "internal/generators/HilbertSpiral.hpp"
#include <iostream>

using namespace ImageGraph;
using namespace ImageGraph::internal;

using int_t = long long;
using point_t = Point<long long>;

int main() {
  std::optional<point_t> prev{};
  gilbert2d<int_t>(0, 0, 4, 4, [&prev](point_t point) {
    if (prev)
      std::cout << "\\draw[->] (" << prev->x() << "," << prev->y() << ") to (" << point.x() << "," << point.y() << ");"
                << std::endl;
    prev.emplace(std::move(point));
  });
  std::cout << std::endl;

  prev.reset();
  const int_t size{4}, count{1};
  gilbert_spiral<int_t>(count * size, count * size, 0, 0, (2 * count + 1) * size - 1, (2 * count + 1) * size - 1, size,
                        size, [&prev](point_t point) {
                          if (prev)
                            std::cout << "\\draw[->] (" << prev->x() << "," << prev->y() << ") to (" << point.x() << ","
                                      << point.y() << ");" << std::endl;
                          prev.emplace(std::move(point));
                        });
}