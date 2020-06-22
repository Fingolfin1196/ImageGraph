#pragma once

#include <memory>

namespace ImageGraph::internal {
template<typename T> struct BorrowedPtr : public std::unique_ptr<T> {
  BorrowedPtr(T* t) : std::unique_ptr<T>(t) {}
  ~BorrowedPtr() { this->release(); }
};
} // namespace ImageGraph::internal