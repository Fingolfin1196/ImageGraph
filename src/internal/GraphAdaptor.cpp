#include "../../include/internal/GraphAdaptor.hpp"
#include "internal/BorrowedPtr.hpp"

using namespace ImageGraph::internal;

void GraphAdaptor::taskModified(Task& task, const TaskMode mode) {
  assert(isIn(set_, BorrowedPtr(&task)));
  switch (mode) {
    case TaskMode::OUT_REQUESTABLE: {
      assert(isIn(out_requestable_, &task));

      const bool all_performed{task.allSinglePerformed()}, all_generated{task.allGenerated()};
      if (all_performed or all_generated)
        out_requestable_.erase(std::find(out_requestable_.begin(), out_requestable_.end(), &task));
      if (all_performed)
        performable_.push_back(&task);
      else if (all_generated)
        requested_.push_back(&task);
      break;
    }
    case TaskMode::SINK_REQUESTABLE: {
      assert(chooser_.contains(task));

      const bool all_performed{task.allSinglePerformed()}, all_generated{task.allGenerated()};
      if (all_performed or all_generated) chooser_.eraseSinkTask(task);
      if (all_performed)
        performable_.push_back(&task);
      else if (all_generated)
        requested_.push_back(&task);
      break;
    }
    case TaskMode::REQUESTED: {
      assert(isIn(requested_, &task));
      if (task.allSinglePerformed()) {
        requested_.erase(std::find(requested_.begin(), requested_.end(), &task));
        performable_.push_back(&task);
      }
      break;
    }
    case TaskMode::PERFORMABLE: throw std::invalid_argument("The modified task is performable!");
  }
}

void GraphAdaptor::singlePerformed(Task& task) {
  assert(not isIn(performable_, &task));

  task.singlePerformed();
  if (task.allSinglePerformed()) {
    assert(not isIn(out_requestable_, &task));

    auto it{std::find(requested_.begin(), requested_.end(), &task)};
    assert(it != requested_.end());

    requested_.erase(it);
    performable_.push_back(&task);
  }
}

void GraphAdaptor::finished(Task& task) {
  assert(not isIn(out_requestable_, &task) and not isIn(requested_, &task) and not isIn(performable_, &task));
  BorrowedPtr ptr{&task};
  set_.erase(ptr);
}
