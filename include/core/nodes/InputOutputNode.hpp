#pragma once

#include "../../internal/GraphAdaptor.hpp"
#include "../../internal/ProtoGraphAdaptor.hpp"
#include "../../internal/tilers/Hilbert.hpp"
#include "InputOutNode.hpp"
#include "OutputNode.hpp"

namespace ImageGraph {
template<typename OutputType, typename... InputTypes>
struct InputOutputNode : virtual public OutputNode<OutputType>, virtual public InputOutNode<InputTypes...> {
  using dimensions_t = Node::dimensions_t;
  using rectangle_t = Node::rectangle_t;
  template<typename T> using shared_tile_t = std::shared_ptr<Tile<T>>;
  template<typename T> using shared_future_tile_t = std::shared_future<std::shared_ptr<Tile<T>>>;

protected:
  /**
   * This class is meant for computing a single tile, which the caller has to ensure.
   */
  class ComputeTileTask final : public internal::TypedTask<shared_tile_t<OutputType>> {
    const InputOutputNode& node_;
    rectangle_t region_;
    std::tuple<shared_future_tile_t<InputTypes>...> results_{};
    size_t counter_{0};

    const Node& node() const final { return node_; }
    bool allGenerated() const final {
      DEBUG_ASSERT(std::runtime_error, counter_ <= sizeof...(InputTypes), "The counter is too large!");
      return counter_ == sizeof...(InputTypes);
    }

    template<std::size_t Index> struct RequiredTaskGenerator {
      using input_t = std::tuple_element_t<Index, std::tuple<InputTypes...>>;

      static inline bool call(internal::Task& task, const InputOutputNode& node, internal::GraphAdaptor& adaptor,
                              rectangle_t region, std::tuple<shared_future_tile_t<InputTypes>...>& tuple) {
        auto result{adaptor.generateRegion<input_t>(task, node.template typedInputNode<Index>(),
                                                    node.inputRegion(Index, region))};
        DEBUG_PREVENT(std::runtime_error, result.finished and not result.future_tile.get(),
                      "The present future tile is nullptr!");
        std::get<Index>(tuple) = std::move(result.future_tile);
        return result.finished;
      }
    };
    std::optional<internal::Task::RequiredTaskInfo> generateRequiredTaskImpl() final {
      if (internal::ct::template_find<0, sizeof...(InputTypes), RequiredTaskGenerator, bool>(
              counter_++, *this, node_, this->adaptor_, region_, results_)
              .value())
        return std::make_optional<internal::Task::RequiredTaskInfo>(node_, this->region());
      return std::nullopt;
    }

    struct FutureRemover {
      template<std::size_t Index> using input_t = std::tuple_element_t<Index, std::tuple<InputTypes...>>;

      template<std::size_t Index>
      static inline Tile<input_t<Index>>& call(std::tuple<shared_future_tile_t<InputTypes>...>& tuple) {
        shared_tile_t<input_t<Index>> tile{std::get<Index>(tuple).get()};
        DEBUG_ASSERT(std::runtime_error, tile, "The tile is nullptr!");
        return *tile;
      }
    };
    void performSingleImpl(const Node&, rectangle_t) final {}
    void performFullImpl() final {
      using namespace std::chrono;

      auto output{std::make_shared<Tile<OutputType>>(region_, node_.channels())};
      {
        const auto start_time{steady_clock::now()};
        node_.compute(internal::ct::ref_map<0, sizeof...(InputTypes), FutureRemover>(results_), *output);
        node_.updateTileDuration(duration_cast<OutNode::duration_t>(steady_clock::now() - start_time), this->region());
      }
      node_.cachePutSynchronized(this->region(), output);
      DEBUG_ASSERT(std::runtime_error, output, "The output is nullptr!");
      this->setPromise(std::move(output));
    }

    std::ostream& print(std::ostream& stream) const final {
      return stream << "ComputeTileTask(" << this->node() << "; " << this->region() << "; " << this->taskCounter()
                    << ")";
    }

  public:
    ComputeTileTask(const InputOutputNode& node, internal::GraphAdaptor& adaptor, rectangle_t region)
        : internal::TypedTask<shared_tile_t<OutputType>>(adaptor, region), node_{node}, region_{region} {}
  };

  /**
   * This class is meant for splitting a region into separate tiles.
   */
  template<typename Tiler> class TilingTask final : public internal::TypedTask<shared_tile_t<OutputType>> {
    struct InputInfo {
      rectangle_t rectangle;
      shared_future_tile_t<OutputType> future;
      InputInfo(rectangle_t rectangle, shared_future_tile_t<OutputType> future)
          : rectangle{std::move(rectangle)}, future{std::move(future)} {}
    };

    const InputOutputNode& node_;
    Tiler tiler_;
    std::vector<InputInfo> results_{};
    std::unique_ptr<Tile<OutputType>> output_{};
    std::mutex mutex_{};

    const Node& node() const final { return node_; }
    bool allGenerated() const final { return not tiler_.remaining(); }
    std::optional<internal::Task::RequiredTaskInfo> generateRequiredTaskImpl() final {
      rectangle_t rect{tiler_.next()};
      auto result{this->adaptor_.template generateRegion<OutputType>(*this, node_, rect)};
      {
        std::lock_guard guard{mutex_};
        results_.emplace_back(rect, std::move(result.future_tile));
      }
      if (result.finished) return std::make_optional<internal::Task::RequiredTaskInfo>(node_, rect);
      return std::nullopt;
    }

    /**
     * CAUTION This is only safe if the tiles do not overlap!
     */
    void performSingleImpl(const Node& node, rectangle_t rectangle) final {
      DEBUG_ASSERT(std::invalid_argument, &node == &node_, "The given node is not the stored node!");
      std::shared_ptr<Tile<OutputType>> tile;
      {
        std::lock_guard guard{mutex_};
        auto iterator{std::find_if(results_.begin(), results_.end(),
                                   [rectangle](const InputInfo& input) { return input.rectangle == rectangle; })};
        DEBUG_ASSERT(std::invalid_argument, iterator != results_.end(), "The ID has not been stored!");
        tile = iterator->future.get();
        results_.erase(iterator);

        if (not output_) output_ = std::make_unique<Tile<OutputType>>(tiler_.rectangle(), node_.channels());
      }
      output_->copyOverlap(*tile);
    }
    void performFullImpl() final {
      DEBUG_ASSERT(std::runtime_error, output_, "The output is nullptr!");
      this->setPromise(std::move(output_));
    }

    std::ostream& print(std::ostream& stream) const final {
      return stream << "TilingTask(" << this->node() << "; " << this->region() << "; " << this->taskCounter() << ")";
    }

  public:
    /**
     * @param node The node to tile. For the caller, this is usually <code>*this</code>.
     * @param adaptor A reference to the GraphAdaptor that handles region requests.
     * @param region The region to be tiled. The actual tiling is determined by adaptor.
     */
    TilingTask(const InputOutputNode& node, internal::GraphAdaptor& adaptor, rectangle_t region, dimensions_t tile)
        : internal::TypedTask<shared_tile_t<OutputType>>(adaptor, region), node_{node}, tiler_{region,
                                                                                               node.dimensions(),
                                                                                               tile} {}
  };

  class ComputeTileProtoTask final : public internal::ProtoOutTask {
    const InputOutputNode& node_;
    rectangle_t region_;

    rectangle_t region() const final { return region_; }
    const OutNode& node() const final { return node_; }

    void performRequiredTasks(internal::ProtoOutTask::performer_t action) final {
      for (std::size_t i{0}; i < sizeof...(InputTypes); ++i) action(node_.inputNode(i), node_.inputRegion(i, region_));
    }

    std::ostream& print(std::ostream& stream) const final {
      return stream << "ComputeTileProtoTask(" << node_ << "; " << region() << ")";
    }

  public:
    duration_t singleTime() const final { return {}; }
    duration_t fullTime() const final { return node_.tileDuration(region_); }

    ComputeTileProtoTask(const InputOutputNode& node, rectangle_t region) : node_{node}, region_{region} {}
  };

  template<typename Tiler> class TilingProtoTask final : public internal::ProtoOutTask {
    const InputOutputNode& node_;
    rectangle_t region_;
    dimensions_t tile_dimensions_;

    rectangle_t region() const final { return region_; }
    const OutNode& node() const final { return node_; }

    void performRequiredTasks(internal::ProtoOutTask::performer_t action) final {
      Tiler::perform(region_, node_.dimensions(), tile_dimensions_,
                     [this, &action](rectangle_t r) { action(node_, r); });
    }

    std::ostream& print(std::ostream& stream) const final {
      return stream << "TilingProtoTask(" << node_ << "; " << region() << ")";
    }

  public:
    duration_t singleTime() const final { return {}; }
    duration_t fullTime() const final { return {}; }

    /**
     * @param node The node to tile. For the caller, this is usually <code>*this</code>.
     * @param adaptor A reference to the GraphAdaptor that handles region requests.
     * @param region The region to be tiled. The actual tiling is determined by adaptor.
     */
    TilingProtoTask(const InputOutputNode& node, rectangle_t region, dimensions_t tile)
        : node_{node}, region_{std::move(region)}, tile_dimensions_{tile} {}
  };

  virtual void compute(std::tuple<const Tile<InputTypes>&...> inputs, Tile<OutputType>& output) const = 0;
};
} // namespace ImageGraph
