#pragma once

#include "InputOutputNode.hpp"
#include "MovingTime.hpp"

namespace ImageGraph {
struct LUTOutNode : virtual OutNode {
protected:
  rectangle_t rawInputRegion(input_index_t, rectangle_t output_rectangle) const final { return output_rectangle; }
};

template<typename OutputType> struct LUTOutputNode : virtual public LUTOutNode, virtual public OutputNode<OutputType> {
  virtual SizedArray<OutputType> computeLUT() const = 0;
};

template<typename InputType> class LUTInputOutNode : virtual public LUTOutNode, virtual public InputOutNode<InputType> {
protected:
  std::optional<SizedArray<InputType>> l_u_t_{};

public:
  void setLUT() {
    const size_t size(std::numeric_limits<InputType>::max() - std::numeric_limits<InputType>::min() + 1);
    SizedArray<InputType> l_u_t(size);
    InputType* l_u_t_data{l_u_t.data()};
    for (size_t i{0}; i < size; ++i) l_u_t_data[i] = i - std::numeric_limits<InputType>::min();
    l_u_t_.emplace(std::move(l_u_t));
  }
  void clearLUT() { l_u_t_.reset(); }
};

template<typename InputType, typename OutputType> struct LUTInputOutputNode
    : virtual public LUTOutputNode<OutputType>,
      virtual public LUTInputOutNode<InputType>,
      virtual public TiledInputOutputNode<OutputType, InputType> {
protected:
  /**
   * “input” and “output” are guaranteed to be both have the size “size”.
   * Additionally, they are allowed to be aliased to save on memory!
   */
  virtual void computeRaw(const InputType* input, OutputType* output, const std::size_t size,
                          const bool is_lookup) const = 0;

public:
  SizedArray<OutputType> computeLUT() const final {
    using input_tile_t = SizedArray<InputType>;
    using output_tile_t = SizedArray<OutputType>;
    if (this->l_u_t_) {
      const input_tile_t& l_u_t{*this->l_u_t_};
      const std::size_t size{l_u_t.size()};
      output_tile_t output(size);
      computeRaw(l_u_t.data(), output.data(), size, true);
      return std::move(output);
    } else {
      const auto& predecessor{dynamic_cast<const LUTOutputNode<InputType>&>(this->typedInputNode())};
      input_tile_t l_u_t{std::move(predecessor.computeLUT())};
      const std::size_t size{l_u_t.size()};
      if constexpr (std::is_same_v<InputType, OutputType>) {
        computeRaw(l_u_t.data(), l_u_t.data(), size, true);
        return std::move(l_u_t);
      } else {
        output_tile_t output(size);
        computeRaw(l_u_t.data(), output.data(), size, true);
        return std::move(output);
      }
    }
  }
};

template<typename InputType, typename OutputType> class MovingTimeLUTInputOutputNode
    : virtual public LUTInputOutputNode<InputType, OutputType>,
      virtual public MovingTimeInputOutputNode<OutputType, InputType> {
protected:
  void computeImpl(std::tuple<const Tile<InputType>&> inputs, Tile<OutputType>& output) const final {
    const Tile<InputType>& input{std::get<0>(inputs)};
    const size_t input_size{input.size()}, output_size{output.size()};
    // TODO Ensure this before calling!
    DEBUG_ASSERT_S(std::runtime_error, input_size == output_size, "The input tile has size ", input_size,
                   " which differs from the size ", output_size, " of the output tile!");
    this->computeRaw(input.data(), output.data(), output_size, false);
  }
};
} // namespace ImageGraph
