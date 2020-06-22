#pragma once
#include "../core/SizedArray.hpp"
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace ImageGraph::internal {
template<typename ID> class ThreadPool {
  struct ThreadTask {
    ID id;
    std::function<void()> function;
    ThreadTask(ID id, std::function<void()> function) : id{std::move(id)}, function{std::move(function)} {}
    void operator()() { function(); }
  };

  std::mutex mutex{};
  std::condition_variable thread_cv{}, control_cv{};

  SizedArray<std::thread> threads_;
  std::unique_ptr<ThreadTask> task_{};
  std::deque<ID> finished_{};
  bool finish_{false};

public:
  ThreadPool(const size_t size) : threads_{size} {
    for (size_t i{0}; i < size; ++i)
      threads_[i] = std::thread([this] {
        std::unique_ptr<ThreadTask> task;
        while (true) {
          {
            std::unique_lock lock{mutex};
            thread_cv.wait(lock, [this] { return finish_ or task_; });
            if (finish_) return;
            task = std::move(task_);
          }
          control_cv.notify_one();
          (*task)();
          {
            std::lock_guard lock{mutex};
            finished_.push_back(std::move(task->id));
          }
        }
      });
  }

  void execute(ID id, std::function<void()>&& function) {
    {
      std::lock_guard guard{mutex};
      assert(not task_);
      task_ = std::make_unique<ThreadTask>(std::move(id), std::move(function));
    }
    thread_cv.notify_one();
    {
      std::unique_lock lock{mutex};
      control_cv.wait(lock, [this] { return finish_ or not task_; });
    }
  }

  std::deque<ID> getFinished() {
    std::deque<ID> deque{};
    {
      std::lock_guard lock{mutex};
      std::swap(deque, finished_);
    }
    return deque;
  }

  void finish() {
    {
      std::lock_guard lock{mutex};
      finish_ = true;
    }
    thread_cv.notify_all();
    control_cv.notify_all();
    for (std::thread& thread : threads_) thread.join();
  }

  ~ThreadPool() { finish(); }
};
} // namespace ImageGraph::internal
