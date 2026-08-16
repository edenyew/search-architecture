#include "circuit_breaker.h"

namespace controller {

CircuitBreaker::CircuitBreaker(int failure_threshold, std::chrono::milliseconds reset_timeout) : failure_threshold_(failure_threshold), reset_timeout_(reset_timeout) {}

bool CircuitBreaker::AllowRequest() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (state_ == State::kClosed) {
    return true;
  }

  if (state_ == State::kOpen) {
    if (std::chrono::steady_clock::now() - opened_at_ >= reset_timeout_) {
      // Reset timeout elapsed — allow exactly one trial request through.
      state_ = State::kHalfOpen;
      return true;
    }
    return false;
  }

  // kHalfOpen: a trial is already in flight; don't let a second
  // request pile onto a dependency that might still be recovering.
  return false;
}

void CircuitBreaker::RecordSuccess() {
  std::lock_guard<std::mutex> lock(mutex_);
  consecutive_failures_ = 0;
  state_ = State::kClosed;
}

void CircuitBreaker::RecordFailure() {
  std::lock_guard<std::mutex> lock(mutex_);
  ++consecutive_failures_;

  if (state_ == State::kHalfOpen) {
    // Trial call failed — still not recovered, go back to Open and
    // restart the timeout.
    state_ = State::kOpen;
    opened_at_ = std::chrono::steady_clock::now();
    return;
  }

  if (consecutive_failures_ >= failure_threshold_) {
    state_ = State::kOpen;
    opened_at_ = std::chrono::steady_clock::now();
  }
}

std::string CircuitBreaker::StateName() {
  std::lock_guard<std::mutex> lock(mutex_);
  switch (state_) {
    case State::kClosed:
      return "closed";
    case State::kOpen:
      return "open";
    case State::kHalfOpen:
      return "half_open";
  }
  return "unknown";
}

}  // namespace controller
