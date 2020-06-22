#pragma once

#include "OutNode.hpp"
#include <unordered_set>

namespace ImageGraph {
class OptimizedOutNode : public virtual OutNode {
  const std::unordered_set<OutNode*> children_;

public:
  OptimizedOutNode(std::unordered_set<OutNode*> children) : children_{std::move(children)} {
    for (auto& child : children_) child->addParent(this);
  }
  ~OptimizedOutNode() {
    for (auto& child : children_) child->removeLastParent(this);
  }

  const std::unordered_set<OutNode*>& children() const { return children_; }

protected:
  std::ostream& printChildren(std::ostream& stream) const {
    for (auto it{children_.begin()}; it != children_.end();) {
      stream << **it;
      if (++it != children_.end()) stream << ", ";
    }
    return stream;
  }

  std::ostream& print(std::ostream& stream) const override {
    return printChildren(stream << "[OptimizedOutNode {") << "} @ " << this << "]";
  }
};
} // namespace ImageGraph
