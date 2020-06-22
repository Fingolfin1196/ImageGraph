#include "internal/Task.hpp"
#include "internal/GraphAdaptor.hpp"

using namespace ImageGraph::internal;

void Task::nextRequiredTask() {
  DEBUG_PREVENT(std::runtime_error, allGenerated(), "All tasks have been created!");
  auto finished{generateRequiredTaskImpl()};
  ++task_counter_;
  if (finished) adaptor_.addSingleFinished(*this, finished->node, finished->rectangle);
}