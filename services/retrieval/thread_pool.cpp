#include "thread_pool.h"

namespace retrieval {

ThreadPool::ThreadPool(size_t num_threads, size_t max_queue_size) : max_queue_size_(max_queue_size) {
  for (size_t i = 0; i < num_threads; ++i) {
    workers_.emplace_back([this]() { WorkerLoop(); });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  cv_.notify_all();
  for (std::thread& worker : workers_) {
    worker.join();
  }
}

void ThreadPool::WorkerLoop() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this]() { return stopping_ || !queue_.empty(); });
      if (stopping_ && queue_.empty()) {
        return;
      }
      task = std::move(queue_.front());
      queue_.pop();
    }
    task();
  }
}

}  // namespace retrieval
