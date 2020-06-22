#include "core/MemoryDistribution.hpp"
#include "core/NodeGraph.hpp"
#include "core/nodes/impl/ChannelCombinator.hpp"
#include "core/nodes/impl/FileSink.hpp"
#include "core/nodes/impl/GaussianBlur.hpp"
#include "core/nodes/impl/PerPixel.hpp"
#include "core/nodes/impl/PerTwoPixels.hpp"
#include "core/nodes/impl/Resize.hpp"
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

  auto& loader{graph.createOutNode<LoadNode<uint16_t>>("img/CarLarge.png")};
  OutputNode<float32_t>* convolver{
      &graph.createOutNode<GaussianBlurNode<uint16_t, float32_t>>(loader, 16.f, .01f, true)};
  for (auto i{0}; i < 2; ++i)
    convolver = &graph.createOutNode<GaussianBlurNode<float32_t, float32_t>>(*convolver, 16.f, .01f, true);
  auto& out_convolver{graph.createOutNode<GaussianBlurNode<float32_t, uint8_t>>(*convolver, 16.f, .01f, true)};
  auto& sinker{graph.createSinkNode<FileSinkNode<uint8_t>>(out_convolver, "img/CarLargeEdited2.tif")};

  // graph.optimize();
  graph.optimizeMemoryDistribution(54'000'000);

  return 0;
}
