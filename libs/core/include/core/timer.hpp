// Copyright (C) 2026 Ihor Dolinskyi
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstddef>
#include <span>
#include <string_view>

namespace sw::core {

enum class TimerState {
  Idle,
  Running,
  Paused,
  Finished,
};

struct TimerPreset {
  std::string_view name{};
  std::chrono::milliseconds duration{0};
};

class CountdownTimer {
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  explicit CountdownTimer(std::chrono::milliseconds target = std::chrono::minutes{5}) noexcept;

  void set_target(std::chrono::milliseconds target) noexcept;
  [[nodiscard]] std::chrono::milliseconds target() const noexcept;
  [[nodiscard]] std::chrono::milliseconds active_target() const noexcept;

  void start(TimePoint now = Clock::now()) noexcept;
  void pause(TimePoint now = Clock::now()) noexcept;
  void toggle(TimePoint now = Clock::now()) noexcept;
  void reset() noexcept;
  void restart(TimePoint now = Clock::now()) noexcept;

  void add_time(std::chrono::milliseconds delta, TimePoint now = Clock::now()) noexcept;

  // True when this call ends a cycle.
  bool update(TimePoint now = Clock::now()) noexcept;

  [[nodiscard]] TimerState state() const noexcept { return state_; }
  [[nodiscard]] bool is_running() const noexcept { return state_ == TimerState::Running; }
  [[nodiscard]] bool is_finished() const noexcept { return state_ == TimerState::Finished; }
  [[nodiscard]] bool is_overtime_running() const noexcept {
    return state_ == TimerState::Finished && overtime_ticking_;
  }

  [[nodiscard]] std::chrono::milliseconds remaining(TimePoint now = Clock::now()) const noexcept;
  [[nodiscard]] std::chrono::milliseconds elapsed(TimePoint now = Clock::now()) const noexcept;
  [[nodiscard]] std::chrono::milliseconds overtime(TimePoint now = Clock::now()) const noexcept;
  [[nodiscard]] double progress(TimePoint now = Clock::now()) const noexcept;

  void set_auto_repeat(bool enabled) noexcept { auto_repeat_ = enabled; }
  [[nodiscard]] bool auto_repeat() const noexcept { return auto_repeat_; }
  [[nodiscard]] std::size_t completed_cycles() const noexcept { return completed_cycles_; }
  void reset_cycles() noexcept { completed_cycles_ = 0U; }

  [[nodiscard]] static std::span<const TimerPreset> default_presets() noexcept;

private:
  [[nodiscard]] std::chrono::nanoseconds raw_elapsed_ns(TimePoint now) const noexcept;

  TimerState state_{TimerState::Idle};
  std::chrono::nanoseconds configured_target_ns_{std::chrono::minutes{5}};
  std::chrono::nanoseconds active_target_ns_{std::chrono::minutes{5}};
  TimePoint start_time_{};
  std::chrono::nanoseconds accumulated_ns_{0};
  bool overtime_ticking_{false};
  bool auto_repeat_{false};
  std::size_t completed_cycles_{0U};
};

} // namespace sw::core
