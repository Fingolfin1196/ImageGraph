#pragma once

#include "../../../internal/GraphAdaptor.hpp"
#include "../../../internal/Typing.hpp"
#include "../../../internal/tilers/HilbertSpiral.hpp"
#include "../../../internal/typing/Vips.hpp"
#include "../../Tile.hpp"
#include "../OutputNode.hpp"
#include "../SinkNode.hpp"

namespace ImageGraph::nodes {
template<typename InputType> class FileSinkNode final : public SinkNode {
  using shared_tile_t = std::shared_ptr<Tile<InputType>>;
  using shared_future_tile_t = std::shared_future<shared_tile_t>;

  const dimensions_t tile_dimensions_{32, 32}, region_dimensions_{2, 2};
  OutputNode<InputType>& input_;
  const std::string out_path_;

  /**
   * This class is meant for merging all tiles of the input.
   */
  template<typename Tiler> class MergeTileTask final : public internal::Task {
    struct InputInfo {
      rectangle_t rectangle;
      shared_future_tile_t future;
      InputInfo(rectangle_t rectangle, shared_future_tile_t future)
          : rectangle{std::move(rectangle)}, future{std::move(future)} {}
    };

    const FileSinkNode& node_;
    Tiler tiler_;
    std::vector<InputInfo> results_{};
    std::optional<Tile<InputType>> output_;
    std::mutex mutex_{};

    const Node& node() const final { return node_; }
    bool allGenerated() const final { return not tiler_.remaining(); }
    std::optional<RequiredTaskInfo> generateRequiredTaskImpl() final {
      rectangle_t rect{tiler_.next()};
      auto result{adaptor_.generateRegion<InputType>(*this, node_.input_, rect)};
      {
        std::lock_guard guard{mutex_};
        results_.emplace_back(rect, std::move(result.future_tile));
      }
      if (result.finished) return std::make_optional<RequiredTaskInfo>(node_.input_, rect);
      return std::nullopt;
    }

    /**
     * CAUTION This is only safe if the tiles do not overlap!
     */
    void performSingleImpl(const Node& node, rectangle_t rectangle) final {
      DEBUG_ASSERT(std::invalid_argument, &node == &node_.input_, "The given node is not the stored node!");
      std::shared_ptr<Tile<InputType>> tile;
      {
        std::lock_guard guard{mutex_};
        auto iterator{std::find_if(results_.begin(), results_.end(),
                                   [rectangle](const InputInfo& input) { return input.rectangle == rectangle; })};
        DEBUG_ASSERT(std::invalid_argument, iterator != results_.end(), "The ID has not been stored!");
        tile = iterator->future.get();
        results_.erase(iterator);

        if (not output_) output_.emplace(region(), node_.channels());
      }
      output_->copyOverlap(*tile);
    }
    void performFullImpl() final { output_.value().writeToFile(node_.out_path_); }

    std::ostream& print(std::ostream& stream) const final {
      return stream << "MergeTileTask(" << node() << "; " << region() << "; " << taskCounter() << ")";
    }

  public:
    MergeTileTask(const FileSinkNode& node, internal::GraphAdaptor& adaptor, point_t centre, dimensions_t tile,
                  dimensions_t region)
        : Task(adaptor, {{}, node.dimensions()}), node_{node}, tiler_{{{}, node.dimensions()},
                                                                      centre,
                                                                      node.dimensions(),
                                                                      tile,
                                                                      region} {}
  };

  template<typename Tiler> class MergeTileProtoTask final : public internal::ProtoSinkTask {
    const FileSinkNode& node_;
    Tiler tiler_;

    rectangle_t region() const final { return tiler_.rectangle(); }
    const SinkNode& node() const final { return node_; }

    bool allGenerated() const final { return not tiler_.remaining(); }

    std::pair<const OutNode&, rectangle_t> generateRequiredTask() final { return {node_.input_, tiler_.next()}; }

    std::ostream& print(std::ostream& stream) const final {
      return stream << "MergeTileProtoTask(" << node_ << "; " << region() << ")";
    }

  public:
    duration_t singleTime() const final { return {}; }
    /**
     * @return Nothing, which is not realistic, but irrelevant for the relative values.
     */
    duration_t fullTime() const final { return {}; }

    MergeTileProtoTask(const FileSinkNode& node, point_t centre, dimensions_t tile, dimensions_t region)
        : ProtoSinkTask(), node_{node}, tiler_{{{}, node.dimensions()}, centre, node.dimensions(), tile, region} {}
  };

public:
  FileSinkNode(OutputNode<InputType>& input, std::string out_path)
      : SinkNode(input.dimensions(), input.channels(), 1, internal::MemoryMode::FULL_MEMORY), input_{input},
        out_path_{std::move(out_path)} {}

  rectangle_t inputRegion(input_index_t, rectangle_t output_rectangle) const final { return output_rectangle; }
  OutNode& inputNode(input_index_t) const final { return input_; }
  /**
   * CAUTION: This currently only works with non-overlapping regions,
   * since the writes to the output are not synchronised.
   */
  std::unique_ptr<internal::Task> task(internal::GraphAdaptor& adaptor) const final {
    return std::make_unique<MergeTileTask<internal::HilbertSpiralRegion>>(*this, adaptor, this->centralPoint(),
                                                                          tile_dimensions_, region_dimensions_);
  }

  std::unique_ptr<internal::ProtoSinkTask> protoTask() const final {
    return std::make_unique<MergeTileProtoTask<internal::HilbertSpiralRegion>>(*this, this->centralPoint(),
                                                                               tile_dimensions_, region_dimensions_);
  }

  // TODO Replace with an actual value!
  relevance_t relevance() const final { return 1.; }
  point_t centralPoint() const final { return {this->width() / 2, this->height() / 2}; }

  std::ostream& print(std::ostream& stream) const final {
    return stream << "[FileSinkNode<" << internal::type_name<InputType>() << ">(input=" << &input_ << ", path=\""
                  << out_path_ << "\") @ " << this << "]";
  }
};
} // namespace ImageGraph::nodes
