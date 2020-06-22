#pragma once

#include "../LookUpTable.hpp"
#include "../MovingTime.hpp"
#include "../OptimizedOutputNode.hpp"

namespace ImageGraph {
namespace nodes {
template<typename OutputType, typename InputType>
requires internal::is_luttable_v<InputType> struct LUTCombinatorNode final
    : public TiledCachedOutputNode<OutputType>,
      public OptimizedOutputNode<OutputType>,
      public MovingTimeInputOutputNode<OutputType, InputType> {
  using dimensions_t = Node::dimensions_t;
  using rectangle_t = Node::rectangle_t;
  using input_index_t = Node::input_index_t;
  using duration_t = OutNode::duration_t;

private:
  mutable internal::NumberGenerator<InputType,
                                    internal::pcg_fast_generator<internal::least_floating_point_t<InputType>>>
      generator_{};
  const SizedArray<OutputType> l_u_t_;

  static inline SizedArray<OutputType> computeLUT(LUTInputOutNode<InputType>& first_node,
                                                  const LUTOutputNode<OutputType>& last_node) {
    first_node.setLUT();
    SizedArray<OutputType> l_u_t{last_node.computeLUT()};
    first_node.clearLUT();
    return l_u_t;
  }

protected:
  void computeImpl(std::tuple<const Tile<InputType>&> inputs, Tile<OutputType>& output) const final {
    const Tile<InputType>& input{std::get<0>(inputs)};
    const size_t input_size{input.size()}, output_size{output.size()};
    DEBUG_ASSERT_S(std::runtime_error, input_size == output_size, "The input tile has size ", input_size,
                   " which differs from the size ", output_size, " of the output tile!");
    const auto& l_u_t{this->l_u_t_};
    for (size_t i{0}; i < output_size; ++i) output[i] = l_u_t[input[i] - std::numeric_limits<InputType>::min()];
  }

  rectangle_t rawInputRegion(input_index_t, rectangle_t output_rectangle) const final { return output_rectangle; }

  std::ostream& print(std::ostream& stream) const override {
    return this->printChildren(stream << "[LUTCombinatorNode(children={") << "}) @ " << this << "]";
  }

public:
  /**
   * @param children Should include both first_node and last_node!
   */
  LUTCombinatorNode(LUTInputOutNode<InputType>& first_node, std::unordered_set<OutNode*> children,
                    LUTOutputNode<OutputType>& last_node)
      : OutNode(first_node.typedInputNode().dimensions(), first_node.typedInputNode().channels(), 1,
                internal::MemoryMode::ANY_MEMORY, typeid(OutputType)),
        OptimizedOutNode(std::move(children)), InputOutNode<InputType>(first_node.typedInputNode(), true),
        OptimizedOutputNode<OutputType>(last_node), l_u_t_{computeLUT(first_node, last_node)} {}
};
} // namespace nodes
namespace internal {
struct lut_combinator_tag : public node_tag {};
template<> struct node_traits<lut_combinator_tag> {
  template<typename T> static inline constexpr bool is_input_type_supported{is_luttable_v<T>};
  template<typename Out, typename... In> struct are_types_supported {};
  template<typename Out, typename In> struct are_types_supported<Out, In> {
    static inline constexpr bool value{is_input_type_supported<In>};
  };
};
} // namespace internal
} // namespace ImageGraph
