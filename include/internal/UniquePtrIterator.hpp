#pragma once

#include <memory>

namespace ImageGraph::internal {
namespace __detail {
template<bool Const, typename T> using cond_const_t = std::conditional_t<Const, const T, T>;

template<typename T, bool Const, typename Derived> struct UniquePtrIteratorBase {
  using data_t = cond_const_t<Const, std::unique_ptr<T[]>>;

protected:
  data_t& data_;
  std::size_t index_;

public:
  using difference_type = std::ptrdiff_t;
  using value_type = T;
  using pointer = cond_const_t<Const, T*>;
  using reference = cond_const_t<Const, T&>;
  using iterator_category = std::random_access_iterator_tag;
  UniquePtrIteratorBase(data_t& data, std::size_t index) : data_{data}, index_{index} {}
  friend Derived& operator++(Derived& derived) {
    ++derived.index_;
    return derived;
  }
  Derived operator+(difference_type offset) const { return {data_, index_ + offset}; }
  Derived operator-(difference_type offset) const { return {data_, index_ - offset}; }
  difference_type operator-(const Derived& other) const {
    return difference_type(index_) - difference_type(other.index_);
  }
  bool operator!=(const Derived& other) const { return data_ != other.data_ or index_ != other.index_; }
  const T& operator*() const { return data_[index_]; }
};
} // namespace __detail

template<typename T, bool Const> struct UniquePtrIterator final {};
template<typename T> struct UniquePtrIterator<T, true> final
    : public __detail::UniquePtrIteratorBase<T, true, UniquePtrIterator<T, true>> {
  using base_t = __detail::UniquePtrIteratorBase<T, true, UniquePtrIterator<T, true>>;

  UniquePtrIterator(typename base_t::data_t& data, std::size_t index) : base_t(data, index) {}
};
template<typename T> struct UniquePtrIterator<T, false> final
    : public __detail::UniquePtrIteratorBase<T, false, UniquePtrIterator<T, false>> {
  using base_t = __detail::UniquePtrIteratorBase<T, false, UniquePtrIterator<T, false>>;

  UniquePtrIterator(typename base_t::data_t& data, std::size_t index) : base_t(data, index) {}
  T& operator*() { return this->data_[this->index_]; }
};
} // namespace ImageGraph::internal
