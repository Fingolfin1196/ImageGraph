#pragma once

#include "Mathematics.hpp"
#include "Random.hpp"
#include <iomanip>
#include <memory>
#include "PCG.hpp"
#include <random>
#include <set>

namespace ImageGraph::internal {
template<typename Solution> class Annealer {
  mutable pcg64 random_number_generator_{pcg_extras::seed_seq_from<std::random_device>()};

public:
  using cost_t = double;

  struct SolutionInfo {
    Solution solution;
    cost_t cost;
  };

  /**
   * @param cost_x The cost of x.
   * @param cost_y The cost of y.
   * @param temperature The temperature.
   * @return The value of the metropolis acceptance function, as described in the lecture.
   */
  static constexpr cost_t metropolis(cost_t cost_x, cost_t cost_y, cost_t temperature) {
    return cost_y <= cost_x ? 1 : std::exp(-(cost_y - cost_x) / temperature);
  }

  SolutionInfo perform(Solution init, std::size_t end_iterations, cost_t initial_temp, cost_t beta, const std::size_t) {
    using shared_t = std::shared_ptr<Solution>;
    shared_t x{std::make_shared<Solution>(std::move(init))};
    cost_t cost_x{x->cost()};
    std::cout << std::setprecision(std::numeric_limits<cost_t>::digits10);
    std::cout << "initial cost: " << cost_x << std::endl;

    shared_t optimum{x};
    cost_t cost_optimum{cost_x};
    cost_t t{initial_temp};

    std::size_t optimum_kept_counter{0};
    while (optimum_kept_counter <= end_iterations) {
      shared_t y{std::make_shared<Solution>(x->random_neighbour())};
      cost_t cost_y{y->cost()};
      std::cout << "y cost " << cost_y << ": ";
      const cost_t metropolis_value{metropolis(cost_x, cost_y, t)},
          random_value{random_norm<cost_t>(random_number_generator_)};
      if (metropolis_value >= random_value) {
        x = std::move(y), cost_x = cost_y;
        std::cout << "accepted!" << std::endl;
      } else
        std::cout << "rejected!" << std::endl;
      t *= beta;
      if (cost_optimum > cost_x)
        optimum = x, cost_optimum = cost_x, optimum_kept_counter = 0;
      else
        ++optimum_kept_counter;
    }

    return {std::move(*optimum), cost_optimum};
  }
};
} // namespace ImageGraph::internal