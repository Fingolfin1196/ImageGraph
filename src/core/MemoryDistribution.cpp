#include "../../include/core/MemoryDistribution.hpp"
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <boost/random/beta_distribution.hpp>

using namespace ImageGraph;
using namespace ImageGraph::internal;

ProtoGraphAdaptor MemoryDistribution::generateAdaptor(const sinks_t& sink_nodes, info_t cache_nodes,
                                                      out_part_t non_cache_nodes) {
  ProtoGraphAdaptor adaptor{};
  for (auto& sink : sink_nodes) adaptor.addSinkTask(*sink);
  for (auto& out : cache_nodes) adaptor.addOutNode(out.node, out.node.cacheSizeFromBytes(out.byte_num));
  for (auto& out : non_cache_nodes) adaptor.addOutNode(*out, 0);
  return std::move(adaptor);
}

using prob_t = MemoryDistribution::prob_t;
struct Probs {
  prob_t own_prob, cum_prob;
  Probs(prob_t own_prob, prob_t cum_prob) : own_prob{own_prob}, cum_prob{cum_prob} {}
};
using prob_map_t = absl::flat_hash_map<const OutNode*, Probs>;
using inputs_t = absl::flat_hash_set<const OutNode*>;

void inputs(const OutNode& node, inputs_t& input_set) {
  const std::size_t input_count{node.inputCount()};
  for (std::size_t i{0}; i < input_count; ++i) {
    auto& input{node.inputNode(i)};
    if (input_set.insert(&input).second) inputs(input, input_set);
  }
}

Probs computeProbs(const OutNode& node, prob_map_t& probabilities) {
  auto it{probabilities.find(&node)};
  if (it != probabilities.end()) return it->second;

  const std::size_t input_count{node.inputCount()};
  const prob_t base{node.changeProbability()};
  assert(0 <= base and base <= 1);
  prob_t prob{1. - base};

  inputs_t input_set{};
  inputs(node, input_set);
  for (const auto* input : input_set) prob *= 1. - computeProbs(*input, probabilities).own_prob;

  Probs probs{base, 1. - prob};
  probabilities.emplace(&node, probs);
  return probs;
}

MemoryDistribution::argument_tuple_t
MemoryDistribution::generate_members(std::size_t memory_limit, const outs_t& out_nodes, const sinks_t& sink_nodes) {
  info_t cache_nodes{};
  out_part_t non_cache_nodes{};
  std::size_t important_bytes{0}, unimportant_bytes{0};
  bool enough_bytes{true};

  prob_map_t probabilities{};

  for (const auto& ptr : out_nodes) {
    const OutNode& node{*ptr};
    switch (node.memoryMode()) {
      case MemoryMode::NO_MEMORY: {
        non_cache_nodes.push_back(&node);
        break;
      }
      case MemoryMode::ANY_MEMORY: {
        const std::size_t bytes{node.fullByteNumber()};
        Probs probs{computeProbs(node, probabilities)};
        cache_nodes.emplace_back(node, 0, bytes, probs.own_prob, probs.cum_prob);
        if (node.isCacheImportant())
          important_bytes += bytes;
        else
          unimportant_bytes += bytes;
        break;
      }
      case MemoryMode::FULL_MEMORY: {
        const std::size_t bytes{node.fullByteNumber()};
        if (bytes <= memory_limit)
          memory_limit -= bytes;
        else
          memory_limit = 0, enough_bytes = false;
        non_cache_nodes.push_back(&node);
        break;
      }
    }
  }
  const MemoryAmount amount{enough_bytes
                                ? (memory_limit >= important_bytes + unimportant_bytes ? MemoryAmount::ENOUGH_FOR_ALL
                                                                                       : MemoryAmount::SUFFICIENT)
                                : MemoryAmount ::TOO_LITTLE};
  const std::size_t size{cache_nodes.size()};
  using portion_t = long double;
  if (amount == MemoryAmount::ENOUGH_FOR_ALL) {
    for (auto& information : cache_nodes) information.byte_num = information.max_byte_num;
  } else if (memory_limit >= important_bytes) {
    std::size_t remaining_unimportant{memory_limit - important_bytes};
    for (auto& information : cache_nodes) {
      if (information.node.isCacheImportant())
        information.byte_num = information.max_byte_num;
      else {
        const std::size_t max_byte_num{information.max_byte_num};
        const portion_t portion{portion_t(max_byte_num) / portion_t(unimportant_bytes)};
        const std::size_t byte_num(portion * remaining_unimportant);
        assert(byte_num <= remaining_unimportant);
        information.byte_num = byte_num;
        unimportant_bytes -= max_byte_num, remaining_unimportant -= byte_num;
      }
    }
  } else if (memory_limit > 0) {
    std::size_t remaining_important{memory_limit};
    for (auto& information : cache_nodes) {
      if (information.node.isCacheImportant()) {
        const std::size_t max_byte_num{information.max_byte_num};
        const portion_t portion{portion_t(max_byte_num) / portion_t(important_bytes)};
        const std::size_t byte_num(portion * remaining_important);
        assert(byte_num <= remaining_important);
        information.byte_num = byte_num;
        important_bytes -= max_byte_num, remaining_important -= byte_num;
      }
    }
  }
  return {std::move(memory_limit), sink_nodes, std::move(cache_nodes), std::move(non_cache_nodes),
          MemoryAmount(amount)};
}

MemoryDistribution::cost_t MemoryDistribution::cost() const {
  // Perform the time computations
  cost_t raw_cost{0};
  while (not adaptor_.empty()) raw_cost += adaptor_.frontRequestableNextRequiredTask();
  assert(raw_cost >= 0);

  // Compute the time cost
  double cost{0}, cumulative{0};
  const auto& sink_data{adaptor_.sinkData()};
  for (const auto& datum : sink_data) {
    assert(datum.second.relevance >= 0);
    cumulative += datum.second.relevance;
    cost += datum.second.relevance * datum.second.duration;
  }

  // Compute the memory loss (based on the removal probabilities)
  prob_t wasted{0}, full{0};
  for (const auto& info : cache_nodes_) {
    const prob_t prob{info.cum_removal_prob};
    assert(0 <= prob and prob <= 1);
    const std::size_t cache_size{info.byte_num};
    full += cache_size;
    wasted += prob * cache_size;
  }
  wasted /= full;

  if (cumulative == 0) return 0;
  cost *= cost_t(sink_data.size()) / cumulative;
  std::cout << "raw cost vs. actual cost: " << raw_cost << " / " << cost << " with " << wasted << " wasted"
            << std::endl;
  assert(0 <= wasted and wasted <= 1);
  return (1. + wasted) * cost;
}

MemoryDistribution MemoryDistribution::random_neighbour() const {
  struct NodeProbability {
    const prob_t probability;
    const std::size_t index;
    NodeProbability(const prob_t probability, const std::size_t index) : probability{probability}, index{index} {}
  };

  constexpr prob_t _0{0}, _1{1};
  assert(adaptor_.empty());
  const prob_t eps{1e-2}, one_eps{_1 - eps};
  const auto& out_data{adaptor_.outData()};
  const std::size_t node_num{cache_nodes_.size()};

  prob_t cumulative{0};
  std::vector<NodeProbability> choices{};
  choices.reserve(cache_nodes_.size());
  for (std::size_t i{0}; i < node_num; ++i) {
    const auto& info{cache_nodes_.at(i)};
    assert(info.byte_num <= info.max_byte_num);
    const auto& out_datum{out_data.at(&info.node)};
    assert(out_datum.computations <= out_datum.requests);
    if (not out_datum.requests) continue;
    const prob_t memory_portion{prob_t(info.byte_num) / prob_t(info.max_byte_num)},
        non_hit_portion{prob_t(out_datum.computations) / prob_t(out_datum.requests)};
    const prob_t prob{memory_portion * (eps + one_eps * non_hit_portion)};
    if (prob) {
      cumulative += prob;
      choices.emplace_back(cumulative, i);
    }
  }
  auto from_it{std::upper_bound(choices.begin(), choices.end(), random_real(generator_, _0, cumulative),
                                [](prob_t p, const NodeProbability& np) { return p < np.probability; })};
  assert(from_it != choices.end());
  const NodeProbability from{std::move(*from_it)};

  cumulative = 0;
  choices.clear();
  for (std::size_t i{0}; i < node_num; ++i) {
    if (i == from.index) continue;
    const auto& info{cache_nodes_.at(i)};
    assert(info.byte_num <= info.max_byte_num);
    const auto& out_datum{out_data.at(&info.node)};
    assert(out_datum.computations <= out_datum.requests);
    if (not out_datum.requests) continue;
    const prob_t memory_portion{prob_t(info.max_byte_num - info.byte_num) / prob_t(info.max_byte_num)},
        hit_portion{prob_t(out_datum.requests - out_datum.computations) / prob_t(out_datum.requests)};
    const prob_t prob{memory_portion * (eps + one_eps * hit_portion)};
    if (prob) {
      cumulative += prob;
      choices.emplace_back(cumulative, i);
    }
  }
  auto to_it{std::upper_bound(choices.begin(), choices.end(), random_real(generator_, _0, cumulative),
                              [](prob_t p, const NodeProbability& np) { return p < np.probability; })};
  assert(to_it != choices.end());
  const NodeProbability to{std::move(*to_it)};

  auto new_cache_nodes{cache_nodes_};
  auto &from_info{new_cache_nodes.at(from.index)}, &to_info{new_cache_nodes.at(to.index)};
  const std::size_t max_bytes{std::min(from_info.byte_num, to_info.max_byte_num - to_info.byte_num)};
  assert(max_bytes >= 1);
  auto moved_bytes{size_t(std::ceil(boost::random::beta_distribution(2., 4.)(generator_) * max_bytes))};
  from_info.byte_num -= moved_bytes, to_info.byte_num += moved_bytes;
  return MemoryDistribution(memory_limit_, sink_nodes_, std::move(new_cache_nodes), non_cache_nodes_, memory_amount_);
}
