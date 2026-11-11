// Copyright (C) 2026 Ihor Dolinskyi
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/stopwatch.hpp"

#include "core/time_utils.hpp"

#include <format>
#include <utility>

namespace sw::core {

void Stopwatch::start(TimePoint now) noexcept {
  if (state_ == StopwatchState::Running) {
    return;
  }
  start_time_ = now;
  state_ = StopwatchState::Running;
}

void Stopwatch::pause(TimePoint now) noexcept {
  if (state_ != StopwatchState::Running) {
    return;
  }
  if (now > start_time_) {
    accumulated_ += (now - start_time_);
  }
  state_ = StopwatchState::Paused;
}

void Stopwatch::toggle(TimePoint now) noexcept {
  if (state_ == StopwatchState::Running) {
    pause(now);
  } else {
    start(now);
  }
}

void Stopwatch::reset() noexcept {
  state_ = StopwatchState::Idle;
  start_time_ = TimePoint{};
  accumulated_ = std::chrono::nanoseconds{0};
  last_lap_total_ns_ = std::chrono::nanoseconds{0};
  laps_.clear();
}

std::chrono::nanoseconds Stopwatch::elapsed_ns(TimePoint now) const noexcept {
  if (state_ == StopwatchState::Running && now > start_time_) {
    return accumulated_ + (now - start_time_);
  }
  return accumulated_;
}

std::chrono::milliseconds Stopwatch::elapsed(TimePoint now) const noexcept {
  return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_ns(now));
}

std::chrono::milliseconds Stopwatch::current_lap_elapsed(TimePoint now) const noexcept {
  const auto total_ns = elapsed_ns(now);
  if (total_ns <= last_lap_total_ns_) {
    return std::chrono::milliseconds{0};
  }
  return std::chrono::duration_cast<std::chrono::milliseconds>(total_ns - last_lap_total_ns_);
}

std::optional<LapRecord> Stopwatch::lap(std::string note, TimePoint now) {
  if (state_ == StopwatchState::Idle) {
    return std::nullopt;
  }

  const auto current_total_ns = elapsed_ns(now);
  const auto split_ns = (current_total_ns > last_lap_total_ns_)
                          ? (current_total_ns - last_lap_total_ns_)
                          : std::chrono::nanoseconds{0};

  const auto split_ms = std::chrono::duration_cast<std::chrono::milliseconds>(split_ns);
  const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_total_ns);

  std::chrono::milliseconds delta_ms{0};
  if (!laps_.empty()) {
    delta_ms = split_ms - laps_.back().split;
  }

  last_lap_total_ns_ = current_total_ns;

  if (note.empty()) {
    note = std::format("Коло #{}", laps_.size() + 1U);
  }

  LapRecord record{
    .number = laps_.size() + 1U,
    .split = split_ms,
    .total = total_ms,
    .delta = delta_ms,
    .note = std::move(note),
  };

  laps_.push_back(record);
  return record;
}

bool Stopwatch::set_lap_note(std::size_t lap_number, std::string note) {
  if (lap_number == 0U || lap_number > laps_.size()) {
    return false;
  }
  laps_[lap_number - 1U].note = std::move(note);
  return true;
}

std::optional<LapStatistics> Stopwatch::statistics() const noexcept {
  if (laps_.empty()) {
    return std::nullopt;
  }

  LapStatistics stats{};
  stats.count = laps_.size();
  stats.fastest_lap_number = laps_.front().number;
  stats.slowest_lap_number = laps_.front().number;
  stats.fastest_split = laps_.front().split;
  stats.slowest_split = laps_.front().split;

  std::int64_t sum_ms = 0;
  for (const auto &item : laps_) {
    sum_ms += item.split.count();
    if (item.split < stats.fastest_split) {
      stats.fastest_split = item.split;
      stats.fastest_lap_number = item.number;
    }
    if (item.split > stats.slowest_split) {
      stats.slowest_split = item.split;
      stats.slowest_lap_number = item.number;
    }
  }

  stats.total_laps_time = std::chrono::milliseconds{sum_ms};
  stats.average_split = std::chrono::milliseconds{sum_ms / static_cast<std::int64_t>(laps_.size())};
  return stats;
}

std::string Stopwatch::export_csv() const {
  std::string out = "Number,Split,Total,Delta,Note\n";
  for (const auto &item : laps_) {
    std::string escaped_note;
    escaped_note.reserve(item.note.size() + 4U);
    for (const char ch : item.note) {
      if (ch == '"') {
        escaped_note += "\"\"";
      } else {
        escaped_note.push_back(ch);
      }
    }
    out += std::format("{},{},{},{},\"{}\"\n", item.number, format_digital(item.split),
                       format_digital(item.total), format_delta(item.delta), escaped_note);
  }
  return out;
}

std::string Stopwatch::export_markdown() const {
  std::string out = "| № | Час кола | Загальний | Δ | Примітка |\n";
  out += "|---:|:---:|:---:|:---:|:---|\n";
  for (const auto &item : laps_) {
    out +=
      std::format("| {} | `{}` | `{}` | `{}` | {} |\n", item.number, format_digital(item.split),
                  format_digital(item.total), format_delta(item.delta), item.note);
  }
  if (const auto stats = statistics(); stats.has_value()) {
    out += std::format("\nКіл: {}. Найкраще #{} ({}), найгірше #{} ({}), середнє {}.\n",
                       stats->count, stats->fastest_lap_number,
                       format_digital(stats->fastest_split), stats->slowest_lap_number,
                       format_digital(stats->slowest_split), format_digital(stats->average_split));
  }
  return out;
}

} // namespace sw::core
