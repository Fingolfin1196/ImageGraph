#pragma once
#include "../internal/typing/Vips.hpp"
#include "Rectangle.hpp"
#include "SizedArray.hpp"
#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <memory>
#include <vector>

namespace ImageGraph {
/**
 * A rectangular region of an image.
 * @tparam T The base type of the image.
 */
template<typename T> struct Tile final {
  using dimension_t = std::size_t;
  using rectangle_t = Rectangle<dimension_t>;
  using point_t = Point<dimension_t>;
  using dimensions_t = RectangleDimensions<dimension_t>;
  using array_t = SizedArray<T>;
  using channels_t = std::size_t;
  using pixel_index_t = std::size_t;

private:
  const rectangle_t rectangle_;
  const channels_t channels_;
  array_t data_;

public:
  const rectangle_t& rectangle() const { return rectangle_; }
  const point_t& point() const { return rectangle_.point(); }
  const dimensions_t& dimensions() const { return rectangle_.dimensions(); }

  dimension_t left() const { return rectangle_.left(); }
  dimension_t top() const { return rectangle_.top(); }
  dimension_t width() const { return rectangle_.width(); }
  dimension_t height() const { return rectangle_.height(); }
  channels_t channels() const { return channels_; }
  dimension_t size() const { return data_.size(); }
  bool empty() const { return data_.empty(); }

  array_t& array() { return data_; }
  const array_t& array() const { return data_; }
  T* data() { return data_.data(); }
  const T* data() const { return data_.data(); }
  auto begin() const { return data_.begin(); }
  auto begin() { return data_.begin(); }
  auto end() const { return data_.end(); }
  auto end() { return data_.end(); }
  template<typename O> bool subsetOf(const Tile<O>& other) const { return rectangle_.subsetOf(other.rectangle()); }

  const T& operator[](pixel_index_t index) const { return data_[index]; }
  T& operator[](pixel_index_t index) { return data_[index]; }
  const T& operator()(std::size_t x, std::size_t y, std::size_t c) const {
    return data_[channels_ * (x + width() * y) + c];
  }
  T& operator()(std::size_t x, std::size_t y, std::size_t c) { return data_[channels_ * (x + width() * y) + c]; }

  const T& at(pixel_index_t index) const {
    DEBUG_ASSERT_S(std::out_of_range, index < data_.size(), index, " >= ", data_.size(), "!");
    return data_[index];
  }
  T& at(pixel_index_t index) {
    DEBUG_ASSERT_S(std::out_of_range, index < data_.size(), index, " >= ", data_.size(), "!");
    return data_[index];
  }
  const T& at(std::size_t x, std::size_t y, std::size_t c) const { return at(channels_ * (x + width() * y) + c); }
  T& at(std::size_t x, std::size_t y, std::size_t c) { return at(channels_ * (x + width() * y) + c); }

  void copyOverlap(const Tile& other) {
    DEBUG_ASSERT_S(std::invalid_argument, channels_ == other.channels_, "Cannot copy a tile with ", other.channels_,
                   " channels to a tile with ", channels_, " channels!");
    const std::size_t channels{channels_}, x_begin{std::max(left(), other.left())},
        y_begin{std::max(top(), other.top())}, x_end{std::min(left() + width(), other.left() + other.width())},
        y_end{std::min(top() + height(), other.top() + other.height())};
    for (std::size_t y{y_begin}; y < y_end; ++y)
      for (std::size_t x{x_begin}; x < x_end; ++x)
        for (std::size_t channel{0}; channel < channels; ++channel)
          data_[channels * ((y - top()) * width() + x - left()) + channel] =
              other.data_[channels * ((y - other.top()) * other.width() + x - other.left()) + channel];
  }
  void writeToFile(const std::string& path) const {
    VipsImage* image{vips_image_new_from_memory(data(), size() * sizeof(T), width(), height(), channels(),
                                                internal::band_format_v<T>)};
    vips_image_write_to_file(image, path.c_str(), nullptr);
    g_object_unref(image);
  }

  friend inline std::ostream& operator<<(std::ostream& stream, const Tile& tile) {
    for (std::size_t j{0}; j < tile.height(); ++j) {
      for (std::size_t i{0}; i < tile.width(); ++i) {
        stream << "[ ";
        for (std::size_t k{0}; k < tile.channels(); ++k) stream << tile(i, j, k) << " ";
        stream << "] ";
      }
      stream << std::endl;
    }
    return stream;
  }

  Tile(Rectangle<pixel_index_t> rectangle, channels_t channels)
      : rectangle_{rectangle}, channels_{channels}, data_(rectangle.size() * channels) {}
  Tile(Rectangle<pixel_index_t> rectangle, channels_t channels, SizedArray<T>&& data)
      : rectangle_{rectangle}, channels_{channels}, data_{std::move(data)} {
    DEBUG_ASSERT_S(std::invalid_argument, data_.size() == rectangle.size() * channels, "data.size() = ", data_.size(),
                   " != ", rectangle.size() * channels, " = rectangle.size() * channels!");
  }
  Tile(Tile&&) noexcept = default;
  ~Tile() = default;
};
} // namespace ImageGraph
