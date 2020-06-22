#include "internal/Cache.hpp"
#include <iostream>

int main() {
  using namespace ImageGraph::internal;

  OrderedMapCache<size_t, size_t> cache(4);
  std::cout << cache << std::endl;
  cache.put(0, 1);
  std::cout << cache << std::endl;
  cache.put(1, 1);
  std::cout << cache << std::endl;
  cache.get(0);
  std::cout << cache << std::endl;
  cache.put(2, 2);
  std::cout << cache << std::endl;
  cache.put(3, 6);
  std::cout << cache << std::endl;
  std::cout << cache.get(0).get() << std::endl;
  std::cout << cache << std::endl;
  cache.put(4, 24);
  std::cout << cache << std::endl;
  std::cout << cache.get(1).get() << std::endl;
  std::cout << cache << std::endl;
  cache.resize(8);
  std::cout << cache << std::endl;
  cache.put(5, 32);
  std::cout << cache << std::endl;
  cache.put(6, 37);
  std::cout << cache << std::endl;
  cache.put(7, 33);
  std::cout << cache << std::endl;
  cache.put(8, 44);
  std::cout << cache << std::endl;
  cache.put(1, 12);
  std::cout << cache << std::endl;

  return 0;
}
