#pragma once

#include "Debugging.hpp"
#include "LRUCache.hpp"
#include <mutex>

namespace ImageGraph::internal {
template<typename E> struct ProtoCache {
  using element_t = E;

  virtual std::size_t capacity() const = 0;
  virtual std::size_t size() const = 0;
  virtual void resize(std::size_t capacity) = 0;

  virtual bool contains(const E& element) = 0;
  virtual void put(E element) = 0;

  virtual ~ProtoCache() = default;
};

template<typename E> class OrderedMapProtoCache final : public ProtoCache<E> {
  using set_t = LRUSet<E>;

  set_t set_;

public:
  using element_t = E;

  explicit OrderedMapProtoCache(size_t capacity) : set_{capacity} {}

  std::size_t capacity() const final { return set_.capacity(); }
  std::size_t size() const final { return set_.size(); }
  void resize(std::size_t capacity) final { set_.recapacitate(capacity); }

  bool contains(const E& element) final { return set_.exists(element); }

  void put(E element) final { set_.insert(std::move(element)); }

  friend std::ostream& operator<<(std::ostream& stream, const OrderedMapProtoCache& cache) {
    stream << "[";
    for (auto it{cache.map_.begin()}; it != cache.map_.end();) {
      stream << "(" << it->first << ": " << *it->second << ")";
      if (++it != cache.map_.end()) stream << ", ";
    }
    stream << "]";
    return stream;
  }
};
} // namespace ImageGraph::internal
