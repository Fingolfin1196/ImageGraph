#pragma once

#include "../../internal/Cache.hpp"
#include "../Tile.hpp"
#include "OutNode.hpp"
#include <memory>

namespace ImageGraph {
template<typename OutputType> struct OutputNode : virtual public OutNode {
  using tile_t = Tile<OutputType>;
  using shared_tile_t = std::shared_ptr<tile_t>;

public:
  const OutputNode<OutputType>& typedOutputNode() const {
    return dynamic_cast<const OutputNode<OutputType>&>(outputNode());
  }
  OutputNode<OutputType>& typedOutputNode() { return dynamic_cast<OutputNode<OutputType>&>(outputNode()); }

  std::size_t elementBytes() const final { return sizeof(OutputType); }

  virtual shared_tile_t cacheGet(const rectangle_t&) const { return nullptr; }
  virtual shared_tile_t cacheGetSynchronized(const rectangle_t&) const { return nullptr; }
  virtual void cachePut(const rectangle_t&, shared_tile_t) const {}
  virtual void cachePutSynchronized(const rectangle_t&, shared_tile_t) const {}

  virtual std::unique_ptr<internal::TypedTask<shared_tile_t>> typedTask(internal::GraphAdaptor& adaptor,
                                                                        rectangle_t region) const = 0;

  std::unique_ptr<internal::Task> task(internal::GraphAdaptor& adaptor, rectangle_t region) const final {
    return typedTask(adaptor, region);
  }
};
} // namespace ImageGraph
