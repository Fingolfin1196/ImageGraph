#pragma once

namespace ImageGraph {
struct NodeGraph;
struct Optimizer {
  virtual ~Optimizer() = default;

  virtual void operator()(NodeGraph&) const = 0;
};
} // namespace ImageGraph
