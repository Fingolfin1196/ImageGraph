#include "../../include/internal/ProtoGraphAdaptor.hpp"
#include "internal/BorrowedPtr.hpp"

using namespace ImageGraph::internal;

ProtoGraphAdaptor::durrep_t ProtoGraphAdaptor::outRequest(ProtoOutTask& task, OutData& data) {
  durrep_t own_time{}, dep_time{};
  // TODO This could depend on the input index!
  const durrep_t single_time{std::chrono::duration_cast<duration_t>(task.singleTime()).count()};

  // Time to perform dependencies
  task.performRequiredTasks([this, &task, &own_time, &dep_time, single_time](const OutNode& node, rectangle_t region) {
    OutData& data{out_data_.at(&node)};
    ++data.requests;
    if (node.memoryMode() != MemoryMode::ANY_MEMORY or not data.cache->contains(region)) {
      ++data.computations;
      std::unique_ptr<ProtoOutTask> new_task{node.protoTask(region)};
      assert(new_task);
      ProtoOutTask& ref{*new_task};
      dep_time += outRequest(ref, data);
    }
    own_time += single_time;
  });

  // Time to perform the task itself.
  const OutNode& node{task.node()};
  const auto region{task.region()};
  if (node.memoryMode() == MemoryMode::ANY_MEMORY and node.isCacheable(region)) data.cache->put(region);
  own_time += std::chrono::duration_cast<duration_t>(task.fullTime()).count();
  data.duration += own_time;

  return own_time + dep_time;
}

ProtoGraphAdaptor::durrep_t ProtoGraphAdaptor::sinkRequest(ProtoSinkTask& task) {
  durrep_t time{};
  auto [node, region]{task.nextRequiredTask()};

  OutData& data{out_data_.at(&node)};
  ++data.requests;
  if (node.memoryMode() != MemoryMode::ANY_MEMORY or not data.cache->contains(region)) {
    ++data.computations;

    std::unique_ptr<ProtoOutTask> new_task{node.protoTask(region)};
    assert(new_task);
    ProtoOutTask& ref{*new_task};
    time += outRequest(ref, data);
  }

  const durrep_t single_time{std::chrono::duration_cast<duration_t>(task.singleTime()).count()};
  SinkData& sink_datum{sink_data_.at(&task.node())};
  if (task.allGenerated()) {
    chooser_.eraseSinkTask(task);
    time += sinkPerformable(task);
  }
  sink_datum.duration += time;
  return time;
}

ProtoGraphAdaptor::durrep_t ProtoGraphAdaptor::sinkPerformable(ProtoSinkTask& task) {
  assert(not chooser_.contains(task));

  const auto time{std::chrono::duration_cast<duration_t>(task.fullTime()).count()};

  BorrowedPtr ptr{&task};
  sink_tasks_.erase(ptr);

  return time;
}

ProtoGraphAdaptor::durrep_t ProtoGraphAdaptor::addOutNode(const OutNode& node, std::size_t capacity) {
  auto proto_cache{node.createProtoCache()};
  if (node.memoryMode() == MemoryMode::ANY_MEMORY) proto_cache->resize(capacity);
  out_data_.emplace(&node, OutData(std::move(proto_cache)));
  return 0;
}

ProtoGraphAdaptor::durrep_t ProtoGraphAdaptor::addSinkTask(const SinkNode& node) {
  const auto relevance{node.relevance()};

  std::unique_ptr<ProtoSinkTask> task{node.protoTask()};
  ProtoSinkTask& ref{*task};

  sink_tasks_.emplace(std::move(task));
  auto it{sink_data_.emplace(&node, SinkData(relevance)).first};

  if (ref.allGenerated()) {
    const auto time{sinkPerformable(ref)};
    it->second.duration += time;
    return time;
  } else {
    chooser_.addSinkTask(ref, relevance);
    return 0;
  }
}
