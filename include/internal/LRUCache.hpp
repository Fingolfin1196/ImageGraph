#pragma once

#include <absl/container/flat_hash_map.h>
#include <cassert>
#include <list>
#include <memory>

namespace {
template<typename K, typename V> using PairContainer = std::list<std::pair<K, std::shared_ptr<V>>>;
}

template<typename K, typename V,
         typename LookupContainer = absl::flat_hash_map<K, typename PairContainer<K, V>::iterator>>
class LRUMap {
  LookupContainer lookup_{};
  PairContainer<K, V> values_{};
  std::size_t capacity_{};

  void remove_over(std::size_t limit) {
    while (size() > limit) {
      lookup_.erase(values_.front().first);
      values_.pop_front();
    }
  }

public:
  LRUMap(std::size_t capacity) : capacity_{capacity} {}
  std::size_t size() const {
    assert(lookup_.size() == values_.size());
    return values_.size();
  }
  std::size_t capacity() const { return capacity_; }
  void recapacitate(std::size_t capacity) {
    if (capacity_ == capacity) return;
    capacity_ = capacity;
    if (not capacity)
      lookup_.clear(), values_.clear();
    else
      remove_over(capacity_);
  }

  void insert(const K& key, std::shared_ptr<V> value) {
    assert(lookup_.find(key) == lookup_.end());
    if (capacity_ == 0) {
      assert(lookup_.empty() and values_.empty());
      return;
    }
    remove_over(capacity_ - 1);
    auto it{values_.emplace(values_.end(), key, std::move(value))};
    lookup_.emplace(std::move(key), it);
  }
  std::shared_ptr<V> at(const K& key) {
    auto look_it{lookup_.find(key)};
    if (look_it != lookup_.end()) {
      values_.splice(values_.end(), values_, look_it->second);
      return look_it->second->second;
    }
    return nullptr;
  }

  auto begin() { return values_.rbegin(); }
  auto begin() const { return values_.rbegin(); }
  auto end() { return values_.rend(); }
  auto end() const { return values_.rend(); }
};

template<typename V, typename LookupContainer = absl::flat_hash_map<V, typename std::list<V>::iterator>> class LRUSet {
  LookupContainer lookup_{};
  std::list<V> values_{};
  std::size_t capacity_{};

  void remove_over(std::size_t limit) {
    while (size() > limit) {
      lookup_.erase(values_.front());
      values_.pop_front();
    }
  }

public:
  LRUSet(std::size_t capacity) : capacity_{capacity} {}
  std::size_t size() const {
    assert(lookup_.size() == values_.size());
    return values_.size();
  }
  std::size_t capacity() const { return capacity_; }
  void recapacitate(std::size_t capacity) {
    if (capacity_ == capacity) return;
    capacity_ = capacity;
    if (not capacity)
      lookup_.clear(), values_.clear();
    else
      remove_over(capacity_);
  }

  void insert(V value) {
    assert(lookup_.find(value) == lookup_.end());
    if (capacity_ == 0) {
      assert(lookup_.empty() and values_.empty());
      return;
    }
    remove_over(capacity_ - 1);
    auto it{values_.emplace(values_.end(), value)};
    lookup_.emplace(std::move(value), it);
  }
  bool exists(const V& value) {
    auto look_it{lookup_.find(value)};
    if (look_it != lookup_.end()) {
      values_.splice(values_.end(), values_, look_it->second);
      return true;
    }
    return false;
  }
};
