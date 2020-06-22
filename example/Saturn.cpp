#include "core/NodeGraph.hpp"
#include "core/nodes/impl/FileSink.hpp"
#include "core/nodes/impl/PerPixel.hpp"
#include "core/nodes/impl/Vips.hpp"
#include "core/optimizers/LUTOptimizer.hpp"
#include <vips/vips8>

int main() {
  if (VIPS_INIT("ImageGraph")) vips_error_exit(nullptr);

  using namespace ImageGraph;
  using namespace ImageGraph::optimizers;
  using namespace ImageGraph::nodes;

  NodeGraph graph{};
  graph.createOptimizer<LUTOptimizer<internal::default_numbers_t>>();

  auto& loader{graph.createOutNode<LoadNode<uint16_t>>("img/Saturn.png")};
  auto& lineariser{graph.createOutNode<LinearNode<uint16_t, float32_t>>(loader, true, 11.f, -.02f)};
  auto& gammiser{graph.createOutNode<GammaNode<float32_t, float32_t>>(lineariser, false, 1.f)};
  auto& converter{graph.createOutNode<ConvertNode<float32_t, uint8_t>>(gammiser, true)};
  auto& sinker{graph.createSinkNode<FileSinkNode<uint8_t>>(converter, "img/SaturnOut.png")};

  // graph.optimize();
  graph.compute(54'000'000);

  return 0;
}
