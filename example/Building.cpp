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

  auto& loader{graph.createOutNode<LoadNode<float32_t>>("img/PabellonSunset.exr")};
  auto& lineariser{graph.createOutNode<LinearNode<float32_t, float32_t>>(loader, true, 3.f, 0.f)};
  auto& converter1{graph.createOutNode<ConvertNode<float32_t, uint8_t>>(lineariser, true)};
  auto& sinker1{graph.createSinkNode<FileSinkNode<uint8_t>>(converter1, "img/PabellonSunsetConv.png")};
  auto& gammiser{graph.createOutNode<GammaNode<float32_t, float32_t>>(lineariser, true, .7f)};
  auto& converter2{graph.createOutNode<ConvertNode<float32_t, uint8_t>>(gammiser, true)};
  auto& sinker2{graph.createSinkNode<FileSinkNode<uint8_t>>(converter2, "img/PabellonSunsetEdited.png")};

  // graph.optimize();
  graph.compute(54'000'000);

  return 0;
}
