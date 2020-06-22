#pragma once

#include "OutputNode.hpp"
namespace ImageGraph {
/**
 * The MemoryMode ANY_MEMORY is expected!
 * @tparam OutputType The output type of the node.
 * @tparam Cache The cache type.
 */
template<typename OutputType, template<typename, typename> typename Cache = internal::OrderedMapCache>
struct CachedOutputNode : public virtual OutputNode<OutputType> {
  using rectangle_t = Node::rectangle_t;
  using tile_t = Tile<OutputType>;
  using shared_tile_t = typename OutputNode<OutputType>::shared_tile_t;
  using cache_t = Cache<rectangle_t, tile_t>;

private:
  mutable cache_t cache_{};

public:
  void setCacheSize(std::size_t size) const final { cache_.resize(size); }
  shared_tile_t cacheGet(const rectangle_t& rectangle) const final { return cache_.get(rectangle); }
  shared_tile_t cacheGetSynchronized(const rectangle_t& rectangle) const final {
    return cache_.getSynchronized(rectangle);
  }
  void cachePut(const rectangle_t& rectangle, shared_tile_t tile) const final {
    cache_.put(rectangle, std::move(tile));
  }
  void cachePutSynchronized(const rectangle_t& rectangle, shared_tile_t tile) const final {
    cache_.putSynchronized(rectangle, std::move(tile));
  }

  std::unique_ptr<OutNode::proto_cache_t> createProtoCache() const override { return cache_.toProtoCache(); }
};
} // namespace ImageGraph