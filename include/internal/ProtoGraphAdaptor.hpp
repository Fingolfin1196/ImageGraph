#pragma once

#include "../core/nodes/OutNode.hpp"
#include "generators/RelevanceChoice.hpp"
#include <absl/container/flat_hash_map.h>

namespace ImageGraph::internal {
struct ProtoGraphAdaptor {
  using duration_t = std::chrono::duration<double>;
  using durrep_t = duration_t::rep;
  using rectangle_t = Rectangle<std::size_t>;
  using out_tasks_t = std::deque<ProtoOutTask*>;
  using sink_tasks_t = std::deque<ProtoSinkTask*>;
  using relevance_t = SinkNode::relevance_t;

  struct OutData {
    std::unique_ptr<ProtoCache<rectangle_t>> cache;
    std::size_t computations{0}, requests{0};
    durrep_t duration{};
    OutData(std::unique_ptr<ProtoCache<rectangle_t>> cache) : cache{std::move(cache)} {}
  };
  struct SinkData {
    durrep_t duration{};
    relevance_t relevance;
    SinkData(relevance_t relevance) : relevance{relevance} {}
  };

  using out_data_map_t = absl::flat_hash_map<const OutNode*, OutData>;
  using sink_data_map_t = absl::flat_hash_map<const SinkNode*, SinkData>;

private:
  ProtoSinkTaskSet sink_tasks_{};
  ProtoTaskRelevanceChoiceGenerator chooser_{};
  out_data_map_t out_data_{};
  sink_data_map_t sink_data_{};

  durrep_t outRequest(ProtoOutTask& task, OutData& data);
  durrep_t sinkRequest(ProtoSinkTask& task);
  durrep_t sinkPerformable(ProtoSinkTask& task);

public:
  bool empty() const { return sink_tasks_.empty(); }

  durrep_t frontRequestableNextRequiredTask() {
    if (chooser_) return sinkRequest(*chooser_.generate());
    throw std::runtime_error("Invalid request!");
  }

  durrep_t addOutNode(const OutNode& node, std::size_t capacity);
  durrep_t addSinkTask(const SinkNode& node);

  ProtoGraphAdaptor() = default;
  ProtoGraphAdaptor(const ProtoGraphAdaptor&) = delete;
  ProtoGraphAdaptor(ProtoGraphAdaptor&&) = default;

  void printData() const {
    for (const auto& [out_node, out_data] : out_data_)
      std::cout << *out_node << ": " << out_data.computations << " / " << out_data.requests << std::endl;
  }
  const out_data_map_t& outData() const { return out_data_; }
  const sink_data_map_t& sinkData() const { return sink_data_; }
};
} // namespace ImageGraph::internal
