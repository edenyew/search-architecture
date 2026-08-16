#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace retrieval {

// A fixed-size worker pool with a bounded task queue. Unlike
// std::async(std::launch::async), which spawns a fresh OS thread per
// call with no limit, this caps concurrent work to num_threads and
// queues at most max_queue_size pending tasks beyond that.
//
// Submit() returns nullopt immediately if the queue is already full,
// instead of blocking or growing unbounded — callers use that as a
// load-shedding signal (reject the request) rather than silently
// accepting more work than the service can keep up with.
class ThreadPool {
 public:
  ThreadPool(size_t num_threads, size_t max_queue_size);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  template <typename F>
  auto Submit(F&& f) -> std::optional<std::future<std::invoke_result_t<F>>> {
    using ReturnType = std::invoke_result_t<F>;
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(f));
    std::future<ReturnType> future = task->get_future();

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stopping_ || queue_.size() >= max_queue_size_) {
        return std::nullopt;
      }
      queue_.emplace([task]() { (*task)(); });
    }
    cv_.notify_one();
    return future;
  }

 private:
  void WorkerLoop();

  size_t max_queue_size_;
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> queue_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stopping_ = false;
};

}  // namespace retrieval
