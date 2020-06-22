#pragma once

#include "../../../internal/Mathematics.hpp"
#include "DirectedConvolution.hpp"

namespace ImageGraph::nodes {
template<typename InputType, typename OutputType = internal::least_floating_point_t<InputType>>
struct GaussianBlurNode final : public TiledCachedOutputNode<OutputType>,
                                public MovingTimeInputOutputNode<OutputType, InputType> {
  using dimensions_t = Node::dimensions_t;
  using rectangle_t = Node::rectangle_t;
  using input_index_t = Node::input_index_t;
  using duration_t = OutNode::duration_t;

  using least_float_t = internal::least_floating_point_t<OutputType>;
  using pcg_t = internal::pcg_fast_generator<least_float_t>;

private:
  constexpr static least_float_t SQRT_TWO{internal::math::sqrt<least_float_t>(2)};

  const least_float_t sigma_, minimum_amplitude_;
  const size_t mask_size_;
  const SizedArray<least_float_t> mask_;
  mutable std::optional<pcg_t> generator_;

  constexpr static inline size_t maskSize(least_float_t sigma, least_float_t minimum_amplitude) {
    return std::ceil(SQRT_TWO * sigma * std::sqrt(-std::log(minimum_amplitude)));
  }

  static inline SizedArray<least_float_t> calcMask(least_float_t sigma, size_t mask_size) {
    constexpr least_float_t _2{2};

    const size_t full_size{2 * mask_size + 1};
    auto mask_data{std::make_unique<least_float_t[]>(full_size)};
    least_float_t sum{0};
    const least_float_t sigma_squared{_2 * sigma * sigma};
    for (size_t i{1}; i <= mask_size; ++i) {
      const auto value{std::exp(-least_float_t(i * i) / sigma_squared)};
      mask_data[mask_size - i] = value;
      sum += value;
    }
    mask_data[mask_size] = 1;
    sum = 2 * sum + 1;

    for (size_t i{0}; i <= mask_size; ++i) mask_data[i] /= sum;
    for (size_t i{0}; i < mask_size; ++i) mask_data[full_size - i - 1] = mask_data[i];

    return {std::move(mask_data), full_size};
  }

protected:
  void computeImpl(std::tuple<const Tile<InputType>&> inputs, Tile<OutputType>& output) const final {
    using x_convolution_t = DirectedConvolutionNode<InputType, least_float_t>;
    using y_convolution_t = DirectedConvolutionNode<least_float_t, OutputType>;
    constexpr auto X{ConvolutionDirection::X}, Y{ConvolutionDirection::Y};

    const auto input_rectangle_y{
        y_convolution_t::template inputRegion<Y>(output.rectangle(), mask_.size(), mask_size_, this->dimensions())};

    const Tile<InputType>& input_tile_x(std::get<0>(inputs));
    Tile<least_float_t> input_tile_y(input_rectangle_y, this->channels());

    if (generator_) {
      x_convolution_t::template compute<X, true>(input_tile_x, input_tile_y, mask_, mask_size_, *generator_);
      y_convolution_t::template compute<Y, true>(input_tile_y, output, mask_, mask_size_, *generator_);
    } else {
      x_convolution_t::template compute<X, false>(input_tile_x, input_tile_y, mask_, mask_size_);
      y_convolution_t::template compute<Y, false>(input_tile_y, output, mask_, mask_size_);
    }
  }

  rectangle_t rawInputRegion(input_index_t, rectangle_t output_rectangle) const final {
    using x_convolution_t = DirectedConvolutionNode<InputType, least_float_t>;
    using y_convolution_t = DirectedConvolutionNode<least_float_t, OutputType>;
    constexpr auto X{ConvolutionDirection::X}, Y{ConvolutionDirection::Y};

    const auto dimensions{this->dimensions()};
    return x_convolution_t::template inputRegion<X>(
        y_convolution_t::template inputRegion<Y>(output_rectangle, mask_.size(), mask_size_, dimensions), mask_.size(),
        mask_size_, dimensions);
  }

  std::ostream& print(std::ostream& stream) const override {
    using namespace internal;
    return stream << "[GaussianBlurNode<" << type_name<InputType>() << ", " << type_name<OutputType>()
                  << ">(sigma=" << sigma_ << ", minimum_amplitude=" << minimum_amplitude_
                  << ", mask_size=" << mask_size_ << ", mask.size=" << mask_.size() << ") @ " << this << "]";
  }

public:
  bool isCacheImportant() const final { return true; }

  GaussianBlurNode(OutputNode<InputType>& input, least_float_t sigma, least_float_t minimum_amplitude, bool dither)
      : OutNode(input.dimensions(), input.channels(), 1, internal::MemoryMode::ANY_MEMORY, typeid(OutputType)),
        InputOutNode<InputType>(input, false), sigma_{sigma}, minimum_amplitude_{minimum_amplitude},
        mask_size_{maskSize(sigma, minimum_amplitude)}, mask_{calcMask(sigma, mask_size_)},
        generator_{dither ? std::optional<pcg_t>(pcg_t()) : std::optional<pcg_t>()} {}
};
} // namespace ImageGraph::nodes
