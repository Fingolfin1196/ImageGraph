#pragma once

#include "CachedOutputNode.hpp"
#include "InputOutputNode.hpp"

namespace ImageGraph {
template<typename OutputType> struct TiledOutputNode : virtual public OutputNode<OutputType> {
  using dimensions_t = Node::dimensions_t;
  using rectangle_t = Node::rectangle_t;

private:
  const dimensions_t tile_dimensions_{32, 32};

public:
  dimensions_t tileDimensions() const { return tile_dimensions_; }
  bool isTile(rectangle_t region) const {
    if (region.left() % tile_dimensions_.width() or region.top() % tile_dimensions_.height()) return false;
    return region == rectangle_t(region.point(), tile_dimensions_).clip(this->dimensions());
  }
};

template<typename OutputType, typename... InputTypes> struct TiledInputOutputNode
    : virtual public InputOutputNode<OutputType, InputTypes...>,
      virtual public TiledOutputNode<OutputType> {
  using parent_t = InputOutputNode<OutputType, InputTypes...>;
  using rectangle_t = Node::rectangle_t;
  template<typename T> using shared_tile_t = std::shared_ptr<Tile<T>>;

private:
  std::unique_ptr<internal::ProtoOutTask> protoTask(rectangle_t region) const final {
    if (this->isTile(region))
      return std::make_unique<typename parent_t::ComputeTileProtoTask>(*this, std::move(region));
    else
      return std::make_unique<typename parent_t::template TilingProtoTask<internal::HilbertRegion>>(
          *this, std::move(region), this->tileDimensions());
  }

  std::unique_ptr<internal::TypedTask<shared_tile_t<OutputType>>> typedTask(internal::GraphAdaptor& adaptor,
                                                                            rectangle_t region) const final {
    if (this->isTile(region))
      return std::make_unique<typename parent_t::ComputeTileTask>(*this, adaptor, std::move(region));
    else
      return std::make_unique<typename parent_t::template TilingTask<internal::HilbertRegion>>(
          *this, adaptor, std::move(region), this->tileDimensions());
  }
};

template<typename OutputType, template<typename, typename> typename Cache = internal::OrderedMapCache>
struct TiledCachedOutputNode : virtual public CachedOutputNode<OutputType, Cache>,
                               virtual public TiledOutputNode<OutputType> {
  bool isCacheable(Node::rectangle_t region) const final { return this->isTile(region); }
  std::size_t cacheSizeFromBytes(std::size_t byte_num) const final {
    const auto node_dim{this->dimensions()}, tile_dim{this->tileDimensions()};
    const auto node_width{node_dim.width()}, tile_width{tile_dim.width()}, node_height{node_dim.height()},
        tile_height{tile_dim.height()}, node_channels{this->channels()}, node_bytes{this->elementBytes()};
    const std::size_t tile_num_x{node_width / tile_width}, tile_num_y{node_height / tile_height},
        num_of_full_tiles{tile_num_x * tile_num_y}, bytes_per_pixel{node_bytes * node_channels},
        bytes_per_full_tile{bytes_per_pixel * tile_dim.size()};

    if (byte_num <= num_of_full_tiles * bytes_per_full_tile) return byte_num / bytes_per_full_tile;
    byte_num -= num_of_full_tiles * bytes_per_full_tile;
    std::size_t tile_num{num_of_full_tiles};

    const auto [larger_add, larger_other, larger_other_num, smaller_add, smaller_other, smaller_other_num]{
        node_width % tile_width >= node_height % tile_height
            ? std::make_tuple(node_width % tile_width, node_height, node_height / tile_height,
                              node_height % tile_height, node_width, node_width / tile_width)
            : std::make_tuple(node_height % tile_height, node_width, node_width / tile_width, node_width % tile_width,
                              node_height, node_height / tile_height)};

    if (byte_num <= larger_add * larger_other * larger_other_num * bytes_per_pixel)
      return tile_num + byte_num / (larger_add * larger_other * bytes_per_pixel);
    byte_num -= larger_add * larger_other * larger_other_num * bytes_per_pixel;
    tile_num += larger_other_num;

    DEBUG_ASSERT_D(std::runtime_error,
                   byte_num <
                       (smaller_add * smaller_other * smaller_other_num + larger_add * smaller_add) * bytes_per_pixel);
    return tile_num + byte_num / (smaller_add * smaller_other * bytes_per_pixel);
  }
};
} // namespace ImageGraph