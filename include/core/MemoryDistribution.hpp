#pragma once

#include "../internal/ProtoGraphAdaptor.hpp"
#include "../internal/Random.hpp"
#include "../internal/Typing.hpp"
#include "NodeGraph.hpp"
#include <numeric>

namespace ImageGraph {
struct MemoryDistribution {
  enum class MemoryAmount { ENOUGH_FOR_ALL, SUFFICIENT, TOO_LITTLE };

  using cost_t = double;
  using prob_t = double;

  using outs_t = NodeGraph::out_nodes_t;
  using sinks_t = NodeGraph::sink_nodes_t;
  using out_part_t = std::vector<const OutNode*>;

  using ProtoGraphAdaptor = internal::ProtoGraphAdaptor;

  struct NodeInformation {
    const OutNode& node;
    std::size_t byte_num;
    const std::size_t max_byte_num;
    const prob_t own_removal_prob, cum_removal_prob;
    NodeInformation(const OutNode& node, std::size_t byte_num, const std::size_t max_byte_num, prob_t own_removal_prob,
                    prob_t cum_removal_prob)
        : node{node}, byte_num{byte_num}, max_byte_num{max_byte_num}, own_removal_prob{own_removal_prob},
          cum_removal_prob{cum_removal_prob} {}
  };

  using info_t = std::vector<NodeInformation>;
  using argument_tuple_t = std::tuple<const std::size_t, const sinks_t&, info_t, out_part_t, const MemoryAmount>;

private:
  using pcg_t = internal::pcg_fast_generator<prob_t>;

  mutable ProtoGraphAdaptor adaptor_;
  const std::size_t memory_limit_;
  const sinks_t& sink_nodes_;
  info_t cache_nodes_;
  const out_part_t non_cache_nodes_;
  const MemoryAmount memory_amount_;
  mutable pcg_t generator_{};

  MemoryDistribution(ProtoGraphAdaptor adaptor, const std::size_t memory_limit, const sinks_t& sink_nodes,
                     info_t cache_nodes, out_part_t non_cache_nodes, const MemoryAmount memory_amount)
      : adaptor_{std::move(adaptor)}, memory_limit_{memory_limit}, sink_nodes_{sink_nodes},
        cache_nodes_{std::move(cache_nodes)}, non_cache_nodes_{std::move(non_cache_nodes)}, memory_amount_{
                                                                                                memory_amount} {}

  static ProtoGraphAdaptor generateAdaptor(const sinks_t& sink_nodes, info_t cache_nodes, out_part_t non_cache_nodes);

  MemoryDistribution(const std::size_t memory_limit, const sinks_t& sink_nodes, info_t cache_nodes,
                     out_part_t non_cache_nodes, const MemoryAmount memory_amount)
      : adaptor_{generateAdaptor(sink_nodes, cache_nodes, non_cache_nodes)}, memory_limit_{memory_limit},
        sink_nodes_{sink_nodes}, cache_nodes_{std::move(cache_nodes)}, non_cache_nodes_{std::move(non_cache_nodes)},
        memory_amount_{memory_amount} {}

  MemoryDistribution(argument_tuple_t tuple)
      : MemoryDistribution(std::get<0>(tuple), std::get<1>(tuple), std::move(std::get<2>(tuple)),
                           std::move(std::get<3>(tuple)), std::move(std::get<4>(tuple))) {}

  static argument_tuple_t generate_members(std::size_t memory_limit, const outs_t& out_nodes,
                                           const sinks_t& sink_nodes);

public:
  MemoryDistribution(const std::size_t memory_limit, const outs_t& out_nodes, const sinks_t& sink_nodes)
      : MemoryDistribution(generate_members(memory_limit, out_nodes, sink_nodes)) {}
  MemoryDistribution(MemoryDistribution&&) = default;
  MemoryDistribution(const MemoryDistribution&) = delete;

  const info_t& cacheNodes() const { return cache_nodes_; }
  const out_part_t& nonCacheNodes() const { return non_cache_nodes_; }
  MemoryAmount memoryAmount() const { return memory_amount_; }
  std::size_t memoryLimit() const { return memory_limit_; }
  const ProtoGraphAdaptor::out_data_map_t& outData() const { return adaptor_.outData(); }
  const ProtoGraphAdaptor::sink_data_map_t& sinkData() const { return adaptor_.sinkData(); }

  cost_t cost() const;

  MemoryDistribution random_neighbour() const;
};
} // namespace ImageGraph