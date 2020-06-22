#pragma once

#include "LRUCache.hpp"
#include "ProtoCache.hpp"
#include <boost/circular_buffer.hpp>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace ImageGraph::internal {
template<typename K, typename V, typename Data, typename Proto> struct Cache {
  using key_type = K;
  using mapped_type = V;

protected:
  std::mutex mutex_{};
  Data data_;

public:
  std::size_t capacity() const { return data_.capacity(); }
  std::size_t size() const { return data_.size(); }
  void resize(std::size_t capacity) { data_.recapacitate(capacity); }

  std::shared_ptr<V> get(const K& key) { return data_.at(key); }
  std::shared_ptr<V> getSynchronized(const K& key) {
    std::shared_ptr<V> output;
    {
      std::lock_guard lock{mutex_};
      output = get(key);
    }
    return output;
  }

  void put(const K& key, std::shared_ptr<V> ptr) { data_.insert(key, ptr); }
  void put(const K& key, V&& value) { put(key, std::make_shared<V>(std::move(value))); }

  void putSynchronized(const K& key, std::shared_ptr<V> ptr) {
    std::lock_guard lock{mutex_};
    put(key, std::move(ptr));
  }
  void putSynchronized(const K& key, V&& value) { putSynchronized(key, std::make_shared<V>(std::move(value))); }

  const std::mutex& mutex() const { return mutex_; }
  std::mutex& mutex() { return mutex_; }

  std::unique_ptr<Proto> toProtoCache() const {
    Proto proto_cache{capacity()};
    for (const auto& [key, value] : data_) proto_cache.put(key);
    return std::make_unique<Proto>(std::move(proto_cache));
  }

  explicit Cache(size_t capacity) : data_{capacity} {}
  explicit Cache() : Cache(0) {}

  friend std::ostream& operator<<(std::ostream& stream, const Cache& cache) {
    stream << "[";
    for (auto it{cache.data_.begin()}; it != cache.data_.end();) {
      stream << "(" << it->first << ": " << *it->second << ")";
      if (++it != cache.data_.end()) stream << ", ";
    }
    stream << "]";
    return stream;
  }
};

template<typename K, typename V> using OrderedMapCache = Cache<K, V, LRUMap<K, V>, OrderedMapProtoCache<K>>;
} // namespace ImageGraph::internal
