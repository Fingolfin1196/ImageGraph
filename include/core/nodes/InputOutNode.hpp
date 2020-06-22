#pragma once

#include "../../internal/CompileTime.hpp"
#include "../../internal/Typing.hpp"
#include "OutputNode.hpp"

namespace ImageGraph {
template<typename... InputTypes> struct InputOutNode : virtual public OutNode {
  using inputs_t = std::tuple<OutputNode<InputTypes>*...>;

private:
  const inputs_t inputs_;
  const bool replaces_successor_;

  struct IncrementInput {
    template<input_index_t Input> constexpr static inline void call(const inputs_t& inputs) {
      std::get<Input>(inputs)->addSuccessor();
    }
  };
  struct DecrementInput {
    template<input_index_t Input> constexpr static inline void call(const inputs_t& inputs) {
      std::get<Input>(inputs)->removeSuccessor();
    }
  };

protected:
  const inputs_t& inputs() const { return inputs_; }

public:
  explicit InputOutNode(inputs_t inputs, bool replaces_successor)
      : inputs_{std::move(inputs)}, replaces_successor_{std::move(replaces_successor)} {
    if (not replaces_successor_) internal::ct::for_all<0, sizeof...(InputTypes), IncrementInput>(inputs_);
  }
  virtual ~InputOutNode() {
    if (not replaces_successor_) internal::ct::for_all<0, sizeof...(InputTypes), DecrementInput>(inputs_);
  }

  OutNode& inputNode(input_index_t index) const final { return *internal::dynamic_get_v<OutNode>(inputs_, index); }
  template<input_index_t index>
  std::tuple_element_t<index, std::tuple<OutputNode<InputTypes>&...>> typedInputNode() const {
    return *std::get<index>(inputs_);
  }
};
template<typename InputType> struct InputOutNode<InputType> : virtual public OutNode {
  using input_t = OutputNode<InputType>;
  using inputs_t = std::tuple<input_t*>;

private:
  const inputs_t inputs_;
  const bool replaces_successor_;

protected:
  const inputs_t& inputs() const { return inputs_; }

public:
  explicit InputOutNode(inputs_t inputs, bool replaces_successor)
      : inputs_{std::move(inputs)}, replaces_successor_{std::move(replaces_successor)} {
    if (not replaces_successor_) std::get<0>(inputs_)->addSuccessor();
  }
  explicit InputOutNode(input_t& input, bool replaces_successor)
      : InputOutNode(std::make_tuple<input_t*>(&input), replaces_successor) {}
  virtual ~InputOutNode() {
    if (not replaces_successor_) std::get<0>(inputs_)->removeSuccessor();
  }

  OutNode& inputNode(input_index_t index) const final {
    DEBUG_ASSERT_S(std::out_of_range, index == 0, "Input index ", index, " != 0!");
    return *std::get<0>(inputs_);
  }
  template<input_index_t index> std::enable_if_t<index == 0, input_t&> typedInputNode() const {
    return *std::get<0>(inputs_);
  }
  input_t& typedInputNode() const { return *std::get<0>(inputs_); }
};
template<> struct InputOutNode<> : virtual public OutNode {
  using inputs_t = std::tuple<>;

private:
  const inputs_t inputs_{};

protected:
  const inputs_t& inputs() const { return inputs_; }

public:
  virtual ~InputOutNode() = default;

  OutNode& inputNode(input_index_t) const final { throw std::invalid_argument("There are no inputs!"); }
};
} // namespace ImageGraph
