#pragma once

#include "OptimizedOutNode.hpp"
#include "OutputNode.hpp"

namespace ImageGraph {
template<typename OutputType> class OptimizedOutputNode : public virtual OptimizedOutNode,
                                                          public virtual OutputNode<OutputType> {
private:
  OutputNode<OutputType>& output_child_;

public:
  OptimizedOutputNode(OutputNode<OutputType>& output_child) : output_child_{output_child} {
    output_child_.setParentOutput(this);
  }
};
} // namespace ImageGraph
