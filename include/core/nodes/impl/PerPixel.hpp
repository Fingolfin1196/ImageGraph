#pragma once

#include "../LookUpTable.hpp"

namespace ImageGraph::nodes {
template<typename InputType, typename OutputType, template<typename In, typename Out> typename Callable>
struct PerPixelOutNode final : public TiledCachedOutputNode<OutputType>,
                               public MovingTimeLUTInputOutputNode<InputType, OutputType> {
  using least_float_t = internal::least_floating_point_t<OutputType>;
  using pcg_t = internal::pcg_fast_generator<least_float_t>;
  using call_t = Callable<InputType, OutputType>;
  using args_t = typename call_t::args_t;

private:
  const args_t attributes_;
  mutable std::optional<pcg_t> generator_;

protected:
  void computeRaw(const InputType* input, OutputType* output, const std::size_t size,
                  const bool is_lookup) const final {
    using namespace internal;
    if (this->generator_ and not is_lookup)
      for (size_t i{0}; i < size; ++i)
        output[i] = call_t::template compute<true>(input[i], attributes_, *this->generator_);
    else
      for (size_t i{0}; i < size; ++i) output[i] = call_t::template compute<false>(input[i], attributes_);
  }

  std::ostream& print(std::ostream& stream) const final {
    using namespace internal;
    stream << "[";
    call_t::nodeName(stream);
    stream << "<" << type_name<InputType>() << ", " << type_name<OutputType>()
           << ">(input=" << &this->typedInputNode() << ", ";
    if (call_t::argumentNames(stream, attributes_)) stream << ", ";
    stream << "dither=" << generator_.has_value();
    return stream << ") @ " << this << "]";
  }

public:
  template<typename... Args> PerPixelOutNode(OutputNode<InputType>& input, bool dither, Args&&... args)
      : OutNode(input.dimensions(), input.channels(), 1, internal::MemoryMode::ANY_MEMORY, typeid(OutputType)),
        InputOutNode<InputType>(input, false), attributes_{std::forward<Args>(args)...},
        generator_{dither ? std::optional<pcg_t>(pcg_t()) : std::optional<pcg_t>()} {}
};

namespace {
template<typename InputType, typename OutputType> struct ConvertCallable {
  using args_t = std::tuple<>;

  static inline std::ostream& nodeName(std::ostream& stream) { return stream << "ConvertNode"; }
  static inline bool argumentNames(std::ostream&, const args_t&) { return false; }

  template<bool Dither, typename... Args>
  static inline constexpr OutputType compute(InputType input, const args_t&, Args&... rands) {
    return internal::convert_normalized<Dither, OutputType>(input, rands...);
  }
};
} // namespace
template<typename InputType, typename OutputType> using ConvertNode =
    PerPixelOutNode<InputType, OutputType, ConvertCallable>;

namespace {
template<typename InputType, typename OutputType> struct LinearCallable {
  using least_float_t = internal::least_floating_point_t<OutputType>;
  struct args_t {
    least_float_t factor, constant;
    args_t(least_float_t factor, least_float_t constant) : factor{factor}, constant{constant} {}
  };

  static inline std::ostream& nodeName(std::ostream& stream) { return stream << "LinearNode"; }
  static inline bool argumentNames(std::ostream& stream, const args_t& args) {
    stream << "factor=" << args.factor << ", constant=" << args.constant;
    return true;
  }

  template<bool Dither, typename... Args>
  static inline constexpr OutputType compute(InputType input, const args_t& args, Args&... rands) {
    using namespace internal;
    return convert_normalized<Dither, OutputType>(
        args.factor * convert_normalized<Dither, least_float_t>(input, rands...) + args.constant, rands...);
  }
};
} // namespace
template<typename InputType, typename OutputType = internal::least_floating_point_t<InputType>> using LinearNode =
    PerPixelOutNode<InputType, OutputType, LinearCallable>;

namespace {
template<typename InputType, typename OutputType> struct GammaCallable {
  using least_float_t = internal::least_floating_point_t<OutputType>;
  struct args_t {
    least_float_t gamma;
    args_t(least_float_t gamma) : gamma{gamma} {}
  };

  static inline std::ostream& nodeName(std::ostream& stream) { return stream << "GammaNode"; }
  static inline bool argumentNames(std::ostream& stream, const args_t& args) {
    stream << "gamma=" << args.gamma;
    return true;
  }

  template<bool Dither, typename... Args>
  static inline constexpr OutputType compute(InputType input, const args_t& args, Args&... rands) {
    using namespace internal;
    return convert_normalized<Dither, OutputType>(
        std::pow(convert_normalized<Dither, least_float_t>(input, rands...), args.gamma), rands...);
  }
};
} // namespace
template<typename InputType, typename OutputType = internal::least_floating_point_t<InputType>> using GammaNode =
    PerPixelOutNode<InputType, OutputType, GammaCallable>;

namespace {
template<typename InputType, typename OutputType> struct ClampCallable {
  struct args_t {
    InputType min, max;
    args_t(InputType min, InputType max) : min{min}, max{max} {}
  };

  static inline std::ostream& nodeName(std::ostream& stream) { return stream << "ClampNode"; }
  static inline bool argumentNames(std::ostream& stream, const args_t& args) {
    stream << "min=" << args.min << ", max=" << args.max;
    return true;
  }

  template<bool Dither, typename... Args>
  static inline constexpr OutputType compute(InputType input, const args_t& args, Args&... rands) {
    using namespace internal;
    return convert_normalized<Dither, OutputType>(std::clamp(input, args.min, args.max), rands...);
  }
};
} // namespace
template<typename InputType, typename OutputType = InputType> using ClampNode =
    PerPixelOutNode<InputType, OutputType, ClampCallable>;
} // namespace ImageGraph::nodes
