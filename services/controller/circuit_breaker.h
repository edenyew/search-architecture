#pragma once

#include <chrono>
#include <mutex>
#include <string>

namespace controller {

// A three-state circuit breaker (Closed/Open/Half-Open) for one
// downstream dependency. Trips to Open after failure_threshold
// consecutive failures, short-circuiting further calls (no attempt
// made at all) until reset_timeout elapses, then allows exactly one
// trial call through (Half-Open) to test whether the dependency has
// recovered.
class CircuitBreaker {
 public:
  CircuitBreaker(int failure_threshold, std::chrono::milliseconds reset_timeout);

  // Returns true if a call should be attempted right now. Returns
  // false if the breaker is Open (or a Half-Open trial is already in
  // flight) — the caller should fail fast without attempting the call.
  bool AllowRequest();

  void RecordSuccess();
  void RecordFailure();

  std::string StateName();

 private:
  enum class State { kClosed, kOpen, kHalfOpen };

  const int failure_threshold_;
  const std::chrono::milliseconds reset_timeout_;

  std::mutex mutex_;
  State state_ = State::kClosed;
  int consecutive_failures_ = 0;
  std::chrono::steady_clock::time_point opened_at_;
};

}  // namespace controller
