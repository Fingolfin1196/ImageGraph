#include "core/NodeGraph.hpp"
#include "core/MemoryDistribution.hpp"
#include "core/SizedArray.hpp"
#include "internal/Annealer.hpp"
#include "internal/GraphAdaptor.hpp"
#include "internal/ProtoGraphAdaptor.hpp"
#include "internal/ThreadPool.hpp"
#include "internal/generators/RelevanceChoice.hpp"

using namespace ImageGraph;
using namespace internal;

NodeGraph::~NodeGraph() {
  finish();
  for (auto& node : out_nodes_)
    while (node->hasParents()) eraseOutNode(*node->topParent().parent);
  while (not out_nodes_.empty())
    for (auto it{out_nodes_.begin()}; it != out_nodes_.end();)
      if (not(*it)->successorCount())
        it = out_nodes_.erase(it);
      else
        ++it;
}

MemoryDistribution NodeGraph::optimizeMemoryDistribution(std::size_t memory_limit) const {
  MemoryDistribution distribution{memory_limit, out_nodes_, sink_nodes_};
  switch (distribution.memoryAmount()) {
    case MemoryDistribution::MemoryAmount::ENOUGH_FOR_ALL: {
      std::cout << "There is enough memory for everyone!" << std::endl;
      break;
    }
    case MemoryDistribution::MemoryAmount::SUFFICIENT: {
      if (not distribution.memoryLimit())
        std::cout << "The memory precisely suffices for the necessary parts!" << std::endl;
      else {
        if (distribution.cacheNodes().size() <= 1) {
          std::cout << "Fewer than 2 nodes, nothing to distribute!" << std::endl;
          return distribution;
        }
        auto result{Annealer<MemoryDistribution>().perform(std::move(distribution), 4, 0.5, 0.95, 0)};
        std::cout << "result:" << std::endl;
        std::cout << "out nodes:" << std::endl;
        for (const auto& info : result.solution.outData())
          std::cout << *info.first << ": " << info.second.computations << " / " << info.second.requests
                    << " with cache size "
                    << (info.second.cache ? info.second.cache->size() : 0) * info.first->elementBytes() << " of "
                    << info.first->fullByteNumber() << " at " << info.second.duration << "s" << std::endl;
        std::cout << "sink nodes:" << std::endl;
        for (const auto& info : result.solution.sinkData())
          std::cout << *info.first << ": " << info.second.relevance << " at " << info.second.duration << "s"
                    << std::endl;
        std::cout << "cache nodes:" << std::endl;
        for (const auto& info : result.solution.cacheNodes())
          std::cout << info.node << ": " << info.byte_num << " / " << info.max_byte_num << " with probabilities "
                    << info.own_removal_prob << " / " << info.cum_removal_prob << std::endl;
        std::cout << "result cost: " << result.cost << std::endl;
        return std::move(result.solution);
      }
      break;
    }
    case MemoryDistribution::MemoryAmount::TOO_LITTLE: {
      std::cout << "There is too little memory for even the necessary parts!" << std::endl;
      break;
    }
  }
  return std::move(distribution);
}

bool NodeGraph::handleFinished(GraphAdaptor& adaptor, pool_deque_t finished, pool_t& pool) {
  if (finished.empty()) return false;
  while (not finished.empty()) {
    PoolID pool_id{std::move(finished.front())};
    finished.pop_front();
    Task& task{pool_id.task};

    if (pool_id.dependency)
      adaptor.singlePerformed(task);
    else {
      const Node& node{task.node()};
      auto rectangle{task.region()};
      for (Task* dependant : task.dependants())
        pool.execute({*dependant, true}, [dependant, &node, rectangle] { dependant->performSingle(node, rectangle); });
      adaptor.finished(task);
    }
  }
  return true;
}
bool NodeGraph::performSingle(task_dependency_deque_t finished, pool_t& pool) {
  if (finished.empty()) return false;
  while (not finished.empty()) {
    GraphAdaptor::TaskDependency& dependency{finished.front()};
    pool.execute({dependency.task, true},
                 [dependency] { dependency.task.performSingle(dependency.dependency, dependency.rectangle); });
    finished.pop_front();
  }
  return true;
}

void NodeGraph::finish() {
  std::unique_lock lock{mutex_};
  assert(run_ != RunState::STOP_RUNNING);
  if (run_ == RunState::RUNNING) {
    run_ = RunState::STOP_RUNNING;
    compute_finished_.wait(lock, [this] { return run_ == RunState::NOT_RUNNING; });
  }
}

void NodeGraph::compute(MemoryDistribution distribution, std::optional<size_t> opt_thread_num) {
  struct RunManager {
    RunState& run;
    std::mutex& mutex;
    std::condition_variable& cond;
    RunManager(RunState& run, std::mutex& mutex, std::condition_variable& cond) : run{run}, mutex{mutex}, cond{cond} {
      std::lock_guard lock{mutex};
      assert(run == RunState::NOT_RUNNING);
      run = RunState::RUNNING;
    }
    ~RunManager() {
      {
        std::lock_guard lock{mutex};
        assert(run != RunState::NOT_RUNNING);
        run = RunState::NOT_RUNNING;
      }
      cond.notify_all();
    }
    bool check() {
      std::lock_guard lock{mutex};
      assert(run != RunState::NOT_RUNNING);
      return run == RunState::RUNNING;
    }
  };

  const size_t thread_num{opt_thread_num ? *opt_thread_num : std::thread::hardware_concurrency()};
  RunManager finish{run_, mutex_, compute_finished_};
  GraphAdaptor adaptor{};
  pool_t pool{thread_num};

  for (auto& sink : sink_nodes_) adaptor.addSinkTask(*sink);
  for (const auto& info : distribution.cacheNodes()) info.node.setCacheBytes(info.byte_num);

  while (not adaptor.empty() and finish.check()) {
    while (adaptor.emptyPerformable() and finish.check()) {
      if (performSingle(adaptor.getSingleFinished(), pool) or handleFinished(adaptor, pool.getFinished(), pool))
        continue;
      if (not adaptor.emptyRequestable()) {
        adaptor.frontRequestable().nextRequiredTask();
        continue;
      }
      break;
    }
    while (not adaptor.emptyPerformable() and finish.check()) {
      Task& task{*adaptor.extractPerformable()};
      pool.execute({task, false}, [&task] { task.performFull(); });
    }

    if (finish.check()) {
      performSingle(adaptor.getSingleFinished(), pool);
      handleFinished(adaptor, pool.getFinished(), pool);
    }
  }
}
void NodeGraph::compute(std::size_t memory_limit, std::optional<size_t> opt_thread_num) {
  compute(optimizeMemoryDistribution(memory_limit), std::move(opt_thread_num));
}

using duration_t = NodeGraph::duration_t;

duration_t NodeGraph::computationDuration(std::size_t size) {
  ProtoGraphAdaptor adaptor{};
  duration_t::rep time{0};
  for (auto& sink : sink_nodes_) time += adaptor.addSinkTask(*sink);
  for (auto& out : out_nodes_) time += adaptor.addOutNode(*out, size);

  while (not adaptor.empty()) time += adaptor.frontRequestableNextRequiredTask();
  return duration_t{time};
}
