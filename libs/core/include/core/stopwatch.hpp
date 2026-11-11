// Copyright (C) 2026 Ihor Dolinskyi
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace sw::core {

enum class StopwatchState {
  Idle,
  Running,
  Paused,
};

struct LapRecord {
  std::size_t number{0};
  std::chrono::milliseconds split{0};
  std::chrono::milliseconds total{0};
  std::chrono::milliseconds delta{0};
  std::string note{};
};

struct LapStatistics {
  std::size_t count{0};
  std::size_t fastest_lap_number{0};
  std::size_t slowest_lap_number{0};
  std::chrono::milliseconds fastest_split{0};
  std::chrono::milliseconds slowest_split{0};
  std::chrono::milliseconds average_split{0};
  std::chrono::milliseconds total_laps_time{0};
};

class Stopwatch {
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  Stopwatch() = default;

  void start(TimePoint now = Clock::now()) noexcept;
  void pause(TimePoint now = Clock::now()) noexcept;
  void toggle(TimePoint now = Clock::now()) noexcept;
  void reset() noexcept;

  std::optional<LapRecord> lap(std::string note = {}, TimePoint now = Clock::now());
  bool set_lap_note(std::size_t lap_number, std::string note);

  [[nodiscard]] StopwatchState state() const noexcept { return state_; }
  [[nodiscard]] bool is_running() const noexcept { return state_ == StopwatchState::Running; }

  [[nodiscard]] std::chrono::nanoseconds elapsed_ns(TimePoint now = Clock::now()) const noexcept;
  [[nodiscard]] std::chrono::milliseconds elapsed(TimePoint now = Clock::now()) const noexcept;
  [[nodiscard]] std::chrono::milliseconds
  current_lap_elapsed(TimePoint now = Clock::now()) const noexcept;

  [[nodiscard]] const std::vector<LapRecord> &laps() const noexcept { return laps_; }
  [[nodiscard]] std::optional<LapStatistics> statistics() const noexcept;

  [[nodiscard]] std::string export_csv() const;
  [[nodiscard]] std::string export_markdown() const;

private:
  StopwatchState state_{StopwatchState::Idle};
  TimePoint start_time_{};
  std::chrono::nanoseconds accumulated_{0};
  std::chrono::nanoseconds last_lap_total_ns_{0};
  std::vector<LapRecord> laps_{};
};

} // namespace sw::core
