#pragma once

#include <chrono>
#include <optional>

class Timer {
public:
  Timer() : _start(Clock::now()) {}

  void stop() { _end = Clock::now(); }

  double seconds() { return milliseconds() / 1000; }

  double milliseconds() {
    return static_cast<double>(elapsed<std::chrono::milliseconds>());
  }

  double nanoseconds() {
    return static_cast<double>(elapsed<std::chrono::nanoseconds>());
  }

  template <typename Duration> int64_t elapsed() {
    auto end = _end.value_or(Clock::now());
    auto val = std::chrono::duration_cast<Duration>(end - _start).count();
    _start = Clock::now();
    return val;
  }

private:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  TimePoint _start;
  std::optional<TimePoint> _end;
};
