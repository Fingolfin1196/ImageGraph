#pragma once

#include "../../../internal/Mathematics.hpp"
#include "../MovingTime.hpp"
#include <exception>

namespace ImageGraph::nodes {
enum class ConvolutionDirection { X, Y };
static inline std::ostream& operator<<(std::ostream& stream, const ConvolutionDirection& direction) {
  switch (direction) {
    case ConvolutionDirection::X: return stream << "X";
    case ConvolutionDirection::Y: return stream << "Y";
  }
}

template<typename InputType, typename OutputType = internal::least_floating_point_t<InputType>>
struct DirectedConvolutionNode final : public TiledCachedOutputNode<OutputType>,
                                       public MovingTimeInputOutputNode<OutputType, InputType> {
  using dimensions_t = Node::dimensions_t;
  using rectangle_t = Node::rectangle_t;
  using input_index_t = Node::input_index_t;
  using least_float_t = internal::least_floating_point_t<OutputType>;
  using pcg_t = internal::pcg_fast_generator<least_float_t>;

  template<ConvolutionDirection Direction> constexpr static inline rectangle_t
  inputRegion(const rectangle_t rectangle, const size_t mask_size, const size_t offset) {
    if constexpr (Direction == ConvolutionDirection::Y)
      return {{rectangle.left(), rectangle.top() > offset ? rectangle.top() - offset : 0},
              {rectangle.width(), rectangle.height() + mask_size - 1}};
    else
      return {{rectangle.left() > offset ? rectangle.left() - offset : 0, rectangle.top()},
              {rectangle.width() + mask_size - 1, rectangle.height()}};
  }
  template<ConvolutionDirection Direction> constexpr static inline rectangle_t
  inputRegion(const rectangle_t rectangle, const size_t mask_size, const size_t offset, const dimensions_t dimensions) {
    rectangle_t output{inputRegion<Direction>(rectangle, mask_size, offset)};
    output.clip(dimensions);
    return output;
  }

  /**
   * Computes the output of a one-dimensional convolution.
   * @tparam Direction The direction in which the convolution is supposed to proceed.
   * @tparam Dither Whether or not to dither when converting between types.
   * @tparam Args Additional arguments for dithering.
   *              This is empty if Dither == false, otherwise a random number generator.
   * @param in_tile The input tile.
   *                Preconditions: in_tile.channels() == out_tile.channels() and out_tile.subsetOf(in_tile)
   * @param out_tile The output tile into which the result will be written.
   *                 Preconditions: in_tile.channels() == out_tile.channels() and out_tile.subsetOf(in_tile)
   * @param kernel The kernel by which to compute the convolution.
   *               Precondition: not kernel.empty()
   * @param center The position of the central pixel of the convolution within the kernel.
   *               Precondition: 0 <= center < kernel.size()
   * @param random_args Additional arguments for the RNG, see above.
   */
  template<ConvolutionDirection Direction, bool Dither, typename... Args>
  static inline void compute(const Tile<InputType>& in_tile, Tile<OutputType>& out_tile,
                             const SizedArray<least_float_t>& kernel, const size_t center, Args&&... random_args) {
    using namespace internal::math;
    constexpr least_float_t _0{0}, _1{1};

    DEBUG_PREVENT(std::invalid_argument, kernel.empty(), "A convolution with an empty kernel is meaningless!");
    const std::size_t kernel_size{kernel.size()};
    DEBUG_ASSERT(std::invalid_argument, center < kernel_size, "The center has to be in [0, kernel.size())!");
    DEBUG_ASSERT(std::invalid_argument, out_tile.subsetOf(in_tile), "The output has to be a subset of the input!");
    const std::size_t channels{in_tile.channels()};
    DEBUG_ASSERT(std::runtime_error, channels == out_tile.channels(), "The tiles have a different number of channels!");

    /* Here, two cases are distinguished:
     * If the OutputType is floating point, the results are stored directly into the output tile,
     * otherwise a temporary tile of least_float_t is created.
     */
    std::unique_ptr<Tile<least_float_t>> work_ptr{};
    Tile<least_float_t>* work_tile_ptr;
    if constexpr (std::is_same_v<OutputType, least_float_t>) {
      work_tile_ptr = &out_tile;
    } else {
      work_ptr = std::make_unique<Tile<least_float_t>>(out_tile.rectangle(), channels);
      work_tile_ptr = work_ptr.get();
    }
    Tile<least_float_t>& work_tile{*work_tile_ptr};

    const std::size_t from_center{kernel_size - center};
    const std::size_t in_height{in_tile.height()}, in_width{in_tile.width()}, out_height{out_tile.height()},
        out_width{out_tile.width()};
    // The index offsets in the x and y axes between the output and input.
    const std::size_t x_offset{out_tile.left() - in_tile.left()}, y_offset{out_tile.top() - in_tile.top()};

    if constexpr (Direction == ConvolutionDirection::Y) {
      for (std::size_t out_y{0}; out_y < out_height; ++out_y) {
        const std::size_t kernel_offset{clamped_max<std::size_t>(center, out_y + y_offset, 0)},
            y_begin{clamped_max<std::size_t>(out_y + y_offset, center, 0)},
            y_end{std::min<std::size_t>(out_y + y_offset + from_center, in_height)};
        least_float_t norm{_0};
        for (size_t in_y{y_begin}, i{kernel_offset}; in_y < y_end; ++in_y, ++i) norm += kernel.at(i);

        for (std::size_t out_x{0}; out_x < out_width; ++out_x) {
          const std::size_t in_x{out_x + x_offset};
          SizedArray<least_float_t> together(channels);
          for (size_t in_y{y_begin}, i{kernel_offset}; in_y < y_end; ++in_y, ++i) {
            const auto kern{kernel.at(i)};
            for (size_t channel{0}; channel < channels; ++channel)
              together.at(channel) += kern * internal::convert_normalized<Dither, least_float_t>(
                                                 in_tile.at(in_x, in_y, channel), std::forward<Args>(random_args)...);
          }
          for (size_t channel{0}; channel < channels; ++channel)
            work_tile.at(out_x, out_y, channel) = together.at(channel) / norm;
        }
      }
    } else {
      for (std::size_t out_y{0}; out_y < out_height; ++out_y) {
        const std::size_t in_y{out_y + y_offset};
        for (std::size_t out_x{0}; out_x < out_width; ++out_x) {
          const std::size_t kernel_offset{clamped_max<std::size_t>(center, out_x + x_offset, 0)},
              x_begin{clamped_max<std::size_t>(out_x + x_offset, center, 0)},
              x_end{std::min<std::size_t>(out_x + x_offset + from_center, in_width)};

          least_float_t norm{_0};
          SizedArray<least_float_t> together(channels);
          for (size_t in_x{x_begin}, i{kernel_offset}; in_x < x_end; ++in_x, ++i) {
            const auto kern{kernel.at(i)};
            norm += kern;
            for (size_t channel{0}; channel < channels; ++channel)
              together.at(channel) += kern * internal::convert_normalized<Dither, least_float_t>(
                                                 in_tile.at(in_x, in_y, channel), std::forward<Args>(random_args)...);
          }
          for (size_t channel{0}; channel < channels; ++channel)
            work_tile.at(out_x, out_y, channel) = together.at(channel) / norm;
        }
      }
    }
    if constexpr (not std::is_same_v<OutputType, least_float_t>) {
      for (size_t y{0}; y < out_height; ++y)
        for (size_t x{0}; x < out_width; ++x)
          for (size_t channel{0}; channel < channels; ++channel)
            out_tile(x, y, channel) = internal::convert_normalized<Dither, OutputType>(
                work_tile(x, y, channel), std::forward<Args>(random_args)...);
    }
  }

private:
  ConvolutionDirection direction_;
  const SizedArray<least_float_t> mask_;
  const size_t offset_;
  mutable std::optional<pcg_t> generator_;

protected:
  void computeImpl(std::tuple<const Tile<InputType>&> inputs, Tile<OutputType>& output) const final {
    switch (direction_) {
      case ConvolutionDirection::Y: {
        if (generator_)
          return compute<ConvolutionDirection::Y, true>(std::get<0>(inputs), output, mask_, offset_, *generator_);
        else
          return compute<ConvolutionDirection::Y, false>(std::get<0>(inputs), output, mask_, offset_);
      }
      case ConvolutionDirection::X: {
        if (generator_)
          return compute<ConvolutionDirection::X, true>(std::get<0>(inputs), output, mask_, offset_, *generator_);
        else
          return compute<ConvolutionDirection::X, false>(std::get<0>(inputs), output, mask_, offset_);
      }
    }
  }

  rectangle_t rawInputRegion(input_index_t, rectangle_t output_rectangle) const final {
    switch (direction_) {
      case ConvolutionDirection::Y:
        return inputRegion<ConvolutionDirection::Y>(output_rectangle, mask_.size(), offset_);
      case ConvolutionDirection::X:
        return inputRegion<ConvolutionDirection::X>(output_rectangle, mask_.size(), offset_);
    }
  }

  std::ostream& print(std::ostream& stream) const override {
    using namespace internal;
    return stream << "[DirectedConvolutionNode"
                  << "<" << type_name<InputType>() << ", " << type_name<OutputType>() << ">"
                  << "(direction=" << direction_ << ", mask.size=" << mask_.size() << ", offset=" << offset_
                  << ") @ " << this << "]";
  }

public:
  DirectedConvolutionNode(OutputNode<InputType>& input, SizedArray<least_float_t>&& mask,
                          ConvolutionDirection direction, size_t offset, bool dither)
      : OutNode(input.dimensions(), input.channels(), 1, internal::MemoryMode::ANY_MEMORY, typeid(OutputType)),
        InputOutNode<InputType>(input, false), direction_{direction}, mask_{std::move(mask)}, offset_{offset},
        generator_{dither ? std::optional<pcg_t>(pcg_t()) : std::optional<pcg_t>()} {
    DEBUG_ASSERT(std::invalid_argument, offset_ < mask_.size(), "The offset is not smaller than the mask size!");
  }
};
} // namespace ImageGraph::nodes
