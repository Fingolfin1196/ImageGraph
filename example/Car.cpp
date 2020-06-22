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
  auto& downsizer{graph.createOutNode<BlockResizeNode<uint16_t, float32_t>>(loader, .25f, .25f, true)};
  auto& converter{graph.createOutNode<ConvertNode<float32_t, uint8_t>>(downsizer, true)};
  auto& down_sinker{graph.createSinkNode<FileSinkNode<uint8_t>>(converter, "img/CarLargeEdited1.tif")};
  // const int mask_size{17};
  // SizedArray<float32_t> mask{2 * mask_size + 1};
  // for (int i{-mask_size}; i <= mask_size; ++i) mask[i + mask_size] = 1.f / (2 * mask_size + 1);
  // auto& convolver{graph.createOutNode<DirectedConvolutionNode<float32_t, float32_t>>(downsizer, std::move(mask),
  //                                                                                    ConvolutionDirection::Y, 0,
  //                                                                                    true)};
  auto& convolver{graph.createOutNode<GaussianBlurNode<float32_t, uint8_t>>(downsizer, 16.f, .01f, true)};
  // auto& adder{graph.createOutNode<AdditionNode<uint8_t, uint8_t, uint8_t>>(converter, convolver, true)};
  // auto& channeller{graph.createOutNode<ChannelCombinatorNode<uint8_t, uint8_t>>(
  //     std::make_tuple(&convolver), std::array{SizedArray<std::optional<std::size_t>>{0, std::nullopt, std::nullopt}},
  //     true)};
  auto& sinker{graph.createSinkNode<FileSinkNode<uint8_t>>(convolver, "img/CarLargeEdited2.tif")};

  // graph.optimize();
  std::thread finisher{[&graph] {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(2s);
    graph.finish();
    graph.compute(54'000'000);
  }};

  graph.compute(54'000'000);
  finisher.join();

  return 0;
}
