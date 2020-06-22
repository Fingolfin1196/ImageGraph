#pragma once

#include "../internal/Debugging.hpp"
#include "../internal/UniquePtrIterator.hpp"

namespace ImageGraph {
template<typename T> class SizedArray {
  std::unique_ptr<T[]> data_;
  const std::size_t size_;

  static inline std::unique_ptr<T[]> fromInitializerList(const std::initializer_list<T>& list) {
    std::unique_ptr<T[]> values{std::make_unique<T[]>(std::size(list))};
    std::size_t index{0};
    for (auto it{std::begin(list)}; it != std::end(list); ++it) values[index++] = *it;
    return std::move(values);
  }

public:
  using iterator = internal::UniquePtrIterator<T, false>;
  using const_iterator = internal::UniquePtrIterator<T, true>;

  const_iterator begin() const { return {data_, 0}; }
  iterator begin() { return {data_, 0}; }
  const_iterator end() const { return {data_, size_}; }
  iterator end() { return {data_, size_}; }

  std::size_t size() const { return size_; }
  bool empty() const { return not size_; }

  const T* data() const { return data_.get(); }
  T* data() { return data_.get(); }
  const T& operator[](std::size_t index) const { return data_[index]; }
  T& operator[](std::size_t index) { return data_[index]; }
  const T& at(std::size_t index) const {
    DEBUG_ASSERT_S(std::out_of_range, index < size_, index, " >= ", size_, "!");
    return (*this)[index];
  }
  T& at(std::size_t index) {
    DEBUG_ASSERT_S(std::out_of_range, index < size_, index, " >= ", size_, "!");
    return (*this)[index];
  }

  SizedArray(std::initializer_list<T> list) : data_{fromInitializerList(list)}, size_{list.size()} {}
  SizedArray(std::unique_ptr<T[]>&& data, std::size_t size) : data_{std::move(data)}, size_{size} {}
  explicit SizedArray(std::size_t size) : data_{std::make_unique<T[]>(size)}, size_{size} {}
  SizedArray(SizedArray&&) noexcept = default;
};
} // namespace ImageGraph
