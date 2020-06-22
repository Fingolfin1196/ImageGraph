#pragma once
#include "../../../internal/BicubicInterpolator.hpp"
#include "../../../internal/typing/NumberConversion.hpp"
#include "../../../internal/typing/NumberTraits.hpp"
#include "../MovingTime.hpp"
#include <boost/math/constants/constants.hpp>
#include <boost/math/special_functions/sinc.hpp>
#include <optional>
#include <type_traits>

namespace ImageGraph::nodes {
template<typename InputType, typename OutputType, typename Callable>
struct ResizeNode : public TiledCachedOutputNode<OutputType>, public MovingTimeInputOutputNode<OutputType, InputType> {
  using rectangle_t = Node::rectangle_t;
  using input_index_t = Node::input_index_t;

  using least_float_t = internal::least_floating_point_t<OutputType>;
  using pcg_t = internal::pcg_fast_generator<least_float_t>;
  using args_t = typename Callable::args_t;

private:
  const args_t attributes_;
  const least_float_t factor_x_, factor_y_;
  const std::size_t extension_;
  mutable std::optional<pcg_t> generator_;

protected:
  void computeImpl(std::tuple<const Tile<InputType>&> inputs, Tile<OutputType>& output) const final {
    if (generator_.has_value())
      return Callable::template compute<true, pcg_t>(std::get<0>(inputs), output, *this, *generator_);
    else
      return Callable::template compute<false>(std::get<0>(inputs), output, *this);
  }

  rectangle_t rawInputRegion(input_index_t, rectangle_t out_rect) const final {
    constexpr least_float_t _1{1};

    return out_rect.template toFloatingPoint<least_float_t>()
        .scale(_1 / factor_x_, _1 / factor_y_)
        .template boundingRectangle<std::size_t>()
        .extend(extension_);
  }

public:
  bool isCacheImportant() const final { return true; }

  std::ostream& print(std::ostream& stream) const override {
    using namespace ImageGraph::internal;

    stream << "[";
    Callable::nodeName(stream);
    stream << "<" << type_name<InputType>() << ", " << type_name<OutputType>() << ">(factor_x=" << factor_x_
           << ", factor_y=" << factor_y_ << ", extension=" << extension_ << ", ";
    if (Callable::argumentNames(stream, attributes_)) stream << ", ";
    stream << "dither=" << generator_.has_value();
    stream << ") @ " << this << "]";
    return stream;
  }

  least_float_t factorX() const { return factor_x_; }
  least_float_t factorY() const { return factor_y_; }
  const args_t& attributes() const { return attributes_; }

  template<typename... Args> ResizeNode(OutputNode<InputType>& input, least_float_t factor_x_, least_float_t factor_y_,
                                        bool dither, Args&&... args)
      : OutNode({std::size_t(std::ceil(factor_x_ * input.width())), std::size_t(std::ceil(factor_y_ * input.height()))},
                input.channels(), 1, internal::MemoryMode::ANY_MEMORY, typeid(OutputType)),
        InputOutNode<InputType>(input, false), attributes_{std::forward<Args>(args)...}, factor_x_{factor_x_},
        factor_y_{factor_y_}, extension_{Callable::extension_(attributes_)}, generator_{
                                                                                 dither ? std::optional<pcg_t>(pcg_t())
                                                                                        : std::optional<pcg_t>()} {}
};

namespace {
template<typename InputType, typename OutputType> struct NearestNeighbourComputer {
  using node_t = ResizeNode<InputType, OutputType, NearestNeighbourComputer>;
  using least_float_t = internal::least_floating_point_t<OutputType>;

  using args_t = std::tuple<>;

  static inline std::ostream& nodeName(std::ostream& stream) { return stream << "NearestNeighbourResizeNode"; }
  static inline bool argumentNames(std::ostream&, const args_t&) { return false; }

  static inline std::size_t extension_(const std::tuple<>&) { return 0; }

  template<bool dither, typename... Args>
  static void compute(const Tile<InputType>& in_tile, Tile<OutputType>& out_tile, const node_t& node, Args&... args) {
    constexpr least_float_t _1{1}, p5{.5};

    const auto channels{node.channels()};
    const least_float_t inv_x{_1 / node.factorX()}, inv_y{_1 / node.factorY()};
    const auto in_rect{in_tile.rectangle()}, out_rect{out_tile.rectangle()};

    const std::size_t in_right{in_rect.left() + in_rect.width()}, in_bottom{in_rect.top() + in_rect.height()};
    for (std::size_t y{0}; y < out_rect.height(); ++y)
      for (std::size_t x{0}; x < out_rect.width(); ++x) {
        const std::size_t input_x{
            std::clamp(std::size_t(inv_x * (out_rect.left() + x + p5)), in_rect.left(), in_right)},
            input_y{std::clamp(std::size_t(inv_y * (out_rect.top() + y + p5)), in_rect.top(), in_bottom)};
        for (std::size_t channel{0}; channel < channels; ++channel)
          out_tile(x, y, channel) = internal::convert_normalized<dither, OutputType>(
              in_tile(input_x - in_rect.left(), input_y - in_rect.top(), channel), args...);
      }
  }
};
} // namespace
template<typename InputType, typename OutputType = InputType> using NearestNeighbourResizeNode =
    ResizeNode<InputType, OutputType, NearestNeighbourComputer<InputType, OutputType>>;

namespace {
template<typename InputType, typename OutputType> struct BilinearComputer {
  using node_t = ResizeNode<InputType, OutputType, BilinearComputer>;
  using least_float_t = internal::least_floating_point_t<OutputType>;

  using args_t = std::tuple<>;

  static inline std::ostream& nodeName(std::ostream& stream) { return stream << "BilinearResizeNode"; }
  static inline bool argumentNames(std::ostream&, const args_t&) { return false; }

  static inline std::size_t extension_(const std::tuple<>&) { return 1; }

  template<bool dither, typename... Args>
  static void compute(const Tile<InputType>& in_tile, Tile<OutputType>& out_tile, const node_t& node, Args&... args) {
    using namespace internal;
    constexpr least_float_t _0{0}, _1{1}, p5{.5};

    const std::size_t channels{node.channels()};
    const least_float_t inv_x{_1 / node.factorX()}, inv_y{_1 / node.factorY()};
    const Rectangle in_rect{in_tile.rectangle()}, out_rect{out_tile.rectangle()};

    const std::size_t in_past_right{in_rect.left() + in_rect.width()}, in_past_bottom{in_rect.top() + in_rect.height()},
        in_right{in_past_right > 0 ? in_past_right - 1 : 0}, in_bottom{in_past_bottom > 0 ? in_past_bottom - 1 : 0};
    for (std::size_t y{0}; y < out_rect.height(); ++y)
      for (std::size_t x{0}; x < out_rect.width(); ++x) {
        const least_float_t input_x{std::max(_0, inv_x * (out_rect.left() + x + p5) - p5)},
            input_y{std::max(_0, inv_y * (out_rect.top() + y + p5) - p5)}, x_mix{math::frac(input_x)},
            y_mix{math::frac(input_y)};
        const std::size_t low_x{std::clamp(std::size_t(input_x), in_rect.left(), in_right)},
            high_x{std::clamp(std::size_t(std::ceil(input_x)), in_rect.left(), in_right)},
            low_y{std::clamp(std::size_t(input_y), in_rect.top(), in_bottom)},
            high_y{std::clamp(std::size_t(std::ceil(input_y)), in_rect.top(), in_bottom)};
        for (std::size_t channel{0}; channel < channels; ++channel)
          out_tile(x, y, channel) = convert_normalized<dither, OutputType>(
              (_1 - x_mix) *
                      ((_1 - y_mix) * convert_normalized<dither, least_float_t>(
                                          in_tile(low_x - in_rect.left(), low_y - in_rect.top(), channel), args...) +
                       y_mix * convert_normalized<dither, least_float_t>(
                                   in_tile(low_x - in_rect.left(), high_y - in_rect.top(), channel), args...)) +
                  x_mix *
                      ((_1 - y_mix) * convert_normalized<dither, least_float_t>(
                                          in_tile(high_x - in_rect.left(), low_y - in_rect.top(), channel), args...) +
                       y_mix * convert_normalized<dither, least_float_t>(
                                   in_tile(high_x - in_rect.left(), high_y - in_rect.top(), channel), args...)),
              args...);
      }
  }
};
} // namespace
template<typename InputType, typename OutputType = internal::least_floating_point_t<InputType>>
using BilinearResizeNode = ResizeNode<InputType, OutputType, BilinearComputer<InputType, OutputType>>;

namespace {
template<typename InputType, typename OutputType> struct BicubicComputer {
  using node_t = ResizeNode<InputType, OutputType, BicubicComputer>;
  using least_float_t = internal::least_floating_point_t<OutputType>;

  using args_t = std::tuple<>;

  static inline std::ostream& nodeName(std::ostream& stream) { return stream << "BicubicResizeNode"; }
  static inline bool argumentNames(std::ostream&, const args_t&) { return false; }

  static inline std::size_t extension_(const std::tuple<>&) { return 2; }

  template<bool dither, typename... Args>
  static void compute(const Tile<InputType>& in_tile, Tile<OutputType>& out_tile, const node_t& node, Args&... args) {
    using namespace internal;
    constexpr least_float_t _1{1}, p5{.5};

    const std::size_t channels{node.channels()};
    const least_float_t inv_x{_1 / node.factorX()}, inv_y{_1 / node.factorY()};
    const Rectangle in_rect{in_tile.rectangle()}, out_rect{out_tile.rectangle()};

    internal::BicubicInterpolator<InputType, least_float_t, Tile> interpolator{
        in_tile,
        [&args...](InputType input) { return convert_normalized<dither, least_float_t, InputType>(input, args...); }};

    for (std::size_t y{0}; y < out_rect.height(); ++y)
      for (std::size_t x{0}; x < out_rect.width(); ++x)
        for (std::size_t channel{0}; channel < channels; ++channel)
          out_tile(x, y, channel) = convert_normalized<dither, OutputType>(
              interpolator.evaluate(inv_x * (x + out_rect.left() + p5) - p5 - in_rect.left(),
                                    inv_y * (y + out_rect.top() + p5) - p5 - in_rect.top(), channel),
              args...);
  }
};
} // namespace
template<typename InputType, typename OutputType = internal::least_floating_point_t<InputType>>
using BicubicResizeNode = ResizeNode<InputType, OutputType, BicubicComputer<InputType, OutputType>>;

namespace {
template<typename InputType, typename OutputType> struct LanczosComputer {
  using node_t = ResizeNode<InputType, OutputType, LanczosComputer>;
  using least_float_t = internal::least_floating_point_t<OutputType>;

  struct AContainer {
    const std::size_t a;
    AContainer(std::size_t a) : a{a} {}
  };
  using args_t = AContainer;

  static inline std::ostream& nodeName(std::ostream& stream) { return stream << "LanczosResizeNode"; }
  static inline bool argumentNames(std::ostream& stream, const args_t& attributes) {
    stream << "a=" << attributes.a;
    return true;
  }

  static inline std::size_t extension_(const AContainer& c) { return c.a; }

  template<bool dither, typename... Args>
  static void compute(const Tile<InputType>& in_tile, Tile<OutputType>& out_tile, const node_t& node, Args&... args) {
    using namespace boost::math;
    using namespace internal;
    constexpr least_float_t _0{0}, _1{1}, p5{.5};

    const std::size_t channels{node.channels()};
    const least_float_t inv_x{_1 / node.factorX()}, inv_y{_1 / node.factorY()};
    const Rectangle in_rect{in_tile.rectangle()}, out_rect{out_tile.rectangle()};
    const auto a{node.attributes().a};

    const std::size_t in_past_right{in_rect.left() + in_rect.width()}, in_past_bottom{in_rect.top() + in_rect.height()},
        in_right{in_past_right > 0 ? in_past_right - 1 : 0}, in_bottom{in_past_bottom > 0 ? in_past_bottom - 1 : 0};
    std::vector<least_float_t> values(channels);
    for (std::size_t y{0}; y < out_rect.height(); ++y)
      for (std::size_t x{0}; x < out_rect.width(); ++x) {
        const least_float_t input_x{std::max(_0, inv_x * (out_rect.left() + x + p5) - p5)},
            input_y{std::max(_0, inv_y * (out_rect.top() + y + p5) - p5)};
        const std::size_t low_x{std::clamp(std::size_t(std::max(input_x - a, _0)), in_rect.left(), in_right)},
            high_x{std::clamp(std::size_t(std::ceil(input_x)) + a, in_rect.left(), in_right)},
            low_y{std::clamp(std::size_t(std::max(input_y - a, _0)), in_rect.top(), in_bottom)},
            high_y{std::clamp(std::size_t(std::ceil(input_y)) + a, in_rect.top(), in_bottom)};

        std::fill(values.begin(), values.end(), _0);
        least_float_t weight_sum{_0};
        for (std::size_t y_i{low_y}; y_i <= high_y; ++y_i)
          for (std::size_t x_i{low_x}; x_i <= high_x; ++x_i) {
            const std::size_t effective_x{x_i - in_rect.left()}, effective_y{y_i - in_rect.top()};
            const least_float_t weight{sinc_pi<least_float_t>(constants::pi<least_float_t>() * (input_x - x_i)) *
                                       sinc_pi<least_float_t>(constants::pi<least_float_t>() * (input_x - x_i) / a) *
                                       sinc_pi<least_float_t>(constants::pi<least_float_t>() * (input_y - y_i)) *
                                       sinc_pi<least_float_t>(constants::pi<least_float_t>() * (input_y - y_i) / a)};
            weight_sum += weight;
            for (std::size_t channel{0}; channel < channels; ++channel)
              values[channel] += weight * convert_normalized<dither, least_float_t>(
                                              in_tile(effective_x, effective_y, channel), args...);
          }
        for (std::size_t channel{0}; channel < channels; ++channel)
          out_tile(x, y, channel) = convert_normalized<dither, OutputType>(
              weight_sum ? values[channel] / weight_sum : values[channel], args...);
      }
  }
};
} // namespace
template<typename InputType, typename OutputType = internal::least_floating_point_t<InputType>>
using LanczosResizeNode = ResizeNode<InputType, OutputType, LanczosComputer<InputType, OutputType>>;

namespace {
template<typename InputType, typename OutputType> struct BlockComputer {
  using node_t = ResizeNode<InputType, OutputType, BlockComputer>;
  using least_float_t = internal::least_floating_point_t<OutputType>;

  using args_t = std::tuple<>;

  static inline std::ostream& nodeName(std::ostream& stream) { return stream << "BlockResizeNode"; }
  static inline bool argumentNames(std::ostream&, const args_t&) { return false; }

  static inline std::size_t extension_(const std::tuple<>&) { return 0; }

  template<bool dither, typename... Args>
  static void compute(const Tile<InputType>& in_tile, Tile<OutputType>& out_tile, const node_t& node, Args&... args) {
    using namespace internal;
    constexpr least_float_t _0{0}, _1{1};

    const std::size_t channels{node.channels()};
    const least_float_t inv_x{_1 / node.factorX()}, inv_y{_1 / node.factorY()};
    const Rectangle in_rect{in_tile.rectangle()}, out_rect{out_tile.rectangle()};

    std::vector<least_float_t> values(channels);
    for (std::size_t y{0}; y < out_rect.height(); ++y)
      for (std::size_t x{0}; x < out_rect.width(); ++x) {
        auto pixel_rect{
            Rectangle<least_float_t>({least_float_t(out_rect.left() + x), least_float_t(out_rect.top() + y)}, {_1, _1})
                .scale(inv_x, inv_y)};
        auto pixel_bound{pixel_rect.template boundingRectangle<std::size_t>().clip(in_rect)};

        std::fill(values.begin(), values.end(), _0);
        least_float_t area{0};
        for (std::size_t y_i{0}; y_i < pixel_bound.height(); ++y_i)
          for (std::size_t x_i{0}; x_i < pixel_bound.width(); ++x_i) {
            Rectangle<least_float_t> inner_pixel_rect{
                {least_float_t(x_i + pixel_bound.left()), least_float_t(y_i + pixel_bound.top())}, {_1, _1}};
            const least_float_t overlap{pixel_rect.overlap(inner_pixel_rect)};
            const std::size_t effective_y{y_i + pixel_bound.top() - in_rect.top()},
                effective_x{x_i + pixel_bound.left() - in_rect.left()};
            area += overlap;
            for (std::size_t channel{0}; channel < channels; ++channel)
              values[channel] += overlap * internal::convert_normalized<dither, least_float_t>(
                                               in_tile(effective_x, effective_y, channel), args...);
          }
        for (std::size_t channel{0}; channel < channels; ++channel)
          out_tile(x, y, channel) =
              convert_normalized<dither, OutputType>(area ? values[channel] / area : values[channel], args...);
      }
  }
};
} // namespace
template<typename InputType, typename OutputType = internal::least_floating_point_t<InputType>> using BlockResizeNode =
    ResizeNode<InputType, OutputType, BlockComputer<InputType, OutputType>>;
} // namespace ImageGraph::nodes
