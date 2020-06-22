#pragma once

#include "../../../internal/GraphAdaptor.hpp"
#include "../../../internal/tilers/HilbertSpiral.hpp"
#include "../OutputNode.hpp"
#include "../SinkNode.hpp"
#include <optional>

namespace ImageGraph::nodes {
template<typename InputType> class SimpleSinkNode : public SinkNode {
  using shared_tile_t = std::shared_ptr<Tile<InputType>>;
  using shared_future_tile_t = std::shared_future<shared_tile_t>;

  const dimensions_t tile_dimensions_{32, 32}, region_dimensions_{2, 2};
  OutputNode<InputType>& input_;

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

    const SimpleSinkNode& node_;
    Tiler tiler_;
    std::vector<InputInfo> results_{};
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
      }
      node_.handleTile(tile);
    }
    void performFullImpl() final { node_.allTilesGenerated(); }

    std::ostream& print(std::ostream& stream) const final {
      return stream << "MergeTileTask(" << node() << "; " << region() << "; " << taskCounter() << ")";
    }

  public:
    MergeTileTask(const SimpleSinkNode& node, internal::GraphAdaptor& adaptor, point_t centre, dimensions_t tile,
                  dimensions_t region)
        : Task(adaptor, {{}, node.dimensions()}), node_{node}, tiler_{{{}, node.dimensions()},
                                                                      centre,
                                                                      node.dimensions(),
                                                                      tile,
                                                                      region} {}
  };

  template<typename Tiler> class MergeTileProtoTask final : public internal::ProtoSinkTask {
    const SimpleSinkNode& node_;
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

    MergeTileProtoTask(const SimpleSinkNode& node, point_t centre, dimensions_t tile, dimensions_t region)
        : ProtoSinkTask(), node_{node}, tiler_{{{}, node.dimensions()}, centre, node.dimensions(), tile, region} {}
  };

protected:
  /**
   * @brief Handle a computed tile. NOTE: This function may be called concurrently by multiple threads,
   * and it is the implementation’s responsibility to deal with that!
   */
  virtual void handleTile(shared_tile_t) const = 0;
  virtual void allTilesGenerated() const {}

public:
  SimpleSinkNode(OutputNode<InputType>& input)
      : SinkNode(input.dimensions(), input.channels(), 1, internal::MemoryMode::NO_MEMORY), input_{input} {}

  rectangle_t inputRegion(input_index_t, rectangle_t output_rectangle) const final { return output_rectangle; }
  OutNode& inputNode(input_index_t index) const final {
    DEBUG_ASSERT_S(std::invalid_argument, index == 0, "Input index ", index, " != 0!");
    return input_;
  }

  std::unique_ptr<internal::Task> task(internal::GraphAdaptor& adaptor) const final {
    return std::make_unique<MergeTileTask<internal::HilbertSpiralRegion>>(*this, adaptor, this->centralPoint(),
                                                                          tile_dimensions_, region_dimensions_);
  }

  std::unique_ptr<internal::ProtoSinkTask> protoTask() const final {
    return std::make_unique<MergeTileProtoTask<internal::HilbertSpiralRegion>>(*this, this->centralPoint(),
                                                                               tile_dimensions_, region_dimensions_);
  }

  point_t centralPoint() const final { return {this->width() / 2, this->height() / 2}; }

  std::ostream& print(std::ostream& stream) const final {
    return stream << "[SimpleSinkNode(input=" << &input_ << ") @ " << this << "]";
  }
};
} // namespace ImageGraph::nodes
