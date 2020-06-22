#pragma once
#include "../MovingTime.hpp"

namespace ImageGraph::nodes {
template<typename InputType1, typename InputType2, typename OutputType,
         template<typename In1, typename In2, typename Out> typename Callable>
struct PerTwoPixelsOutNode final : public TiledCachedOutputNode<OutputType>,
                                   public MovingTimeInputOutputNode<OutputType, InputType1, InputType2> {
  using least_float_t = internal::least_floating_point_t<OutputType>;
  using pcg_t = internal::pcg_fast_generator<least_float_t>;
  using call_t = Callable<InputType1, InputType2, OutputType>;
  using args_t = typename call_t::args_t;

  using input_index_t = OutNode::input_index_t;
  using rectangle_t = OutNode::rectangle_t;

private:
  const args_t attributes_;
  mutable std::optional<pcg_t> generator_;

protected:
  void computeImpl(std::tuple<const Tile<InputType1>&, const Tile<InputType2>&> inputs,
                   Tile<OutputType>& output) const final {
    using namespace internal;
    const auto& in1{std::get<0>(inputs)};
    const auto& in2{std::get<1>(inputs)};
    assert(in1.rectangle() == in2.rectangle());
    assert(in1.size() == in2.size());
    std::size_t size{in1.size()};
    if (this->generator_)
      for (size_t i{0}; i < size; ++i)
        output[i] = call_t::template compute<true>(in1[i], in2[i], attributes_, *this->generator_);
    else
      for (size_t i{0}; i < size; ++i) output[i] = call_t::template compute<false>(in1[i], in2[i], attributes_);
  }

  std::ostream& print(std::ostream& stream) const final {
    using namespace internal;
    stream << "[";
    call_t::nodeName(stream);
    stream << "<" << type_name<InputType1>() << type_name<InputType2>() << ", " << type_name<OutputType>()
           << ">(input 1=" << &this->template typedInputNode<0>()
           << ", input 2=" << &this->template typedInputNode<1>() << ", ";
    if (call_t::argumentNames(stream, attributes_)) stream << ", ";
    stream << "dither=" << generator_.has_value();
    return stream << ") @ " << this << "]";
  }

  /**
   * @return The output does not need to be clipped.
   */
  rectangle_t rawInputRegion(input_index_t input, rectangle_t output_rectangle) const final {
    assert(input < 2);
    return output_rectangle;
  }

public:
  template<typename... Args>
  PerTwoPixelsOutNode(OutputNode<InputType1>& input1, OutputNode<InputType2>& input2, bool dither, Args&&... args)
      : OutNode(input1.dimensions().bound(input2.dimensions()), std::max(input1.channels(), input2.channels()), 1,
                internal::MemoryMode::ANY_MEMORY, typeid(OutputType)),
        InputOutNode<InputType1, InputType2>(std::make_tuple(&input1, &input2), false),
        attributes_{std::forward<Args>(args)...}, generator_{dither ? std::optional<pcg_t>(pcg_t())
                                                                    : std::optional<pcg_t>()} {}
};

namespace {
template<typename In1, typename In2, typename Out> struct AdditionCallable {
  using args_t = std::tuple<>;

  static inline std::ostream& nodeName(std::ostream& stream) { return stream << "AdditionNode"; }
  static inline bool argumentNames(std::ostream&, const args_t&) { return false; }

  template<bool Dither, typename... Args>
  static inline constexpr Out compute(In1 in1, In2 in2, const args_t&, Args&... rands) {
    using namespace internal;
    if constexpr (is_integral_v<Out>) {
      const Out v1{convert_normalized<Dither, Out>(in1, rands...)}, v2{convert_normalized<Dither, Out>(in2, rands...)};
      Out out;
      if (__builtin_add_overflow(v1, v2, &out)) {
        if constexpr (is_signed_v<Out>)
          return v1 < 0 and v2 < 0 ? std::numeric_limits<Out>::min() : std::numeric_limits<Out>::max();
        else
          return std::numeric_limits<Out>::max();
      } else
        return out;
    } else
      return convert_normalized<Dither, Out>(in1, rands...) + convert_normalized<Dither, Out>(in2, rands...);
  }
};
} // namespace
template<typename In1, typename In2, typename Out> using AdditionNode =
    PerTwoPixelsOutNode<In1, In2, Out, AdditionCallable>;

namespace {
template<typename In1, typename In2, typename Out> struct SubtractionCallable {
  using args_t = std::tuple<>;

  static inline std::ostream& nodeName(std::ostream& stream) { return stream << "SubtractionNode"; }
  static inline bool argumentNames(std::ostream&, const args_t&) { return false; }

  template<bool Dither, typename... Args>
  static inline constexpr Out compute(In1 in1, In2 in2, const args_t&, Args&... rands) {
    using namespace internal;
    if constexpr (is_integral_v<Out>) {
      const Out v1{convert_normalized<Dither, Out>(in1, rands...)}, v2{convert_normalized<Dither, Out>(in2, rands...)};
      Out out;
      if (__builtin_sub_overflow(v1, v2, &out)) {
        if constexpr (is_signed_v<Out>)
          return v1 < 0 and v2 > 0 ? std::numeric_limits<Out>::min() : std::numeric_limits<Out>::max();
        else
          return std::numeric_limits<Out>::min();
      } else
        return out;
    } else
      return convert_normalized<Dither, Out>(in1, rands...) - convert_normalized<Dither, Out>(in2, rands...);
  }
};
} // namespace
template<typename In1, typename In2, typename Out> using SubtractionNode =
    PerTwoPixelsOutNode<In1, In2, Out, SubtractionCallable>;

namespace {
template<typename In1, typename In2, typename Out> struct MultiplicationCallable {
  using least_float_t = internal::least_floating_point_t<Out>;
  using args_t = std::tuple<>;

  static inline std::ostream& nodeName(std::ostream& stream) { return stream << "MultiplicationNode"; }
  static inline bool argumentNames(std::ostream&, const args_t&) { return false; }

  template<bool Dither, typename... Args>
  static inline constexpr Out compute(In1 in1, In2 in2, const args_t&, Args&... rands) {
    using namespace internal;
    return convert_normalized<Dither, Out>(convert_normalized<Dither, least_float_t>(in1, rands...) *
                                               convert_normalized<Dither, least_float_t>(in2, rands...),
                                           rands...);
  }
};
} // namespace
template<typename In1, typename In2, typename Out> using MultiplicationNode =
    PerTwoPixelsOutNode<In1, In2, Out, MultiplicationCallable>;

namespace {
template<typename In1, typename In2, typename Out> struct DivisionCallable {
  using least_float_t = internal::least_floating_point_t<Out>;
  using args_t = std::tuple<>;

  static inline std::ostream& nodeName(std::ostream& stream) { return stream << "DivisionNode"; }
  static inline bool argumentNames(std::ostream&, const args_t&) { return false; }

  template<bool Dither, typename... Args>
  static inline constexpr Out compute(In1 in1, In2 in2, const args_t&, Args&... rands) {
    using namespace internal;
    return convert_normalized<Dither, Out>(convert_normalized<Dither, least_float_t>(in1, rands...) /
                                               convert_normalized<Dither, least_float_t>(in2, rands...),
                                           rands...);
  }
};
} // namespace
template<typename In1, typename In2, typename Out> using DivisionNode =
    PerTwoPixelsOutNode<In1, In2, Out, DivisionCallable>;
} // namespace ImageGraph::nodes
