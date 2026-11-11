// Copyright (C) 2026 Ihor Dolinskyi
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/timer.hpp"

#include <algorithm>
#include <array>

namespace sw::core {

namespace {

constexpr std::array<TimerPreset, 6> kDefaultPresets = {{
  {.name = "Помодоро (25 хв)", .duration = std::chrono::minutes{25}},
  {.name = "Коротка перерва (5 хв)", .duration = std::chrono::minutes{5}},
  {.name = "Довга перерва (15 хв)", .duration = std::chrono::minutes{15}},
  {.name = "Чай (3 хв)", .duration = std::chrono::minutes{3}},
  {.name = "Спринт (10 хв)", .duration = std::chrono::minutes{10}},
  {.name = "Розминка (1 хв)", .duration = std::chrono::minutes{1}},
}};

} // namespace

CountdownTimer::CountdownTimer(std::chrono::milliseconds target) noexcept { set_target(target); }

void CountdownTimer::set_target(std::chrono::milliseconds target) noexcept {
  const auto clamped = std::max(target, std::chrono::milliseconds{0});
  configured_target_ns_ = std::chrono::duration_cast<std::chrono::nanoseconds>(clamped);
  reset();
}

std::chrono::milliseconds CountdownTimer::target() const noexcept {
  return std::chrono::duration_cast<std::chrono::milliseconds>(configured_target_ns_);
}

std::chrono::milliseconds CountdownTimer::active_target() const noexcept {
  return std::chrono::duration_cast<std::chrono::milliseconds>(active_target_ns_);
}

void CountdownTimer::start(TimePoint now) noexcept {
  if (state_ == TimerState::Running) {
    return;
  }
  if (state_ == TimerState::Finished) {
    reset();
  }
  if (active_target_ns_.count() <= 0) {
    state_ = TimerState::Finished;
    overtime_ticking_ = false;
    return;
  }
  start_time_ = now;
  state_ = TimerState::Running;
  overtime_ticking_ = false;
}

void CountdownTimer::pause(TimePoint now) noexcept {
  if (state_ == TimerState::Running) {
    if (now > start_time_) {
      accumulated_ns_ += (now - start_time_);
    }
    state_ = TimerState::Paused;
  } else if (state_ == TimerState::Finished && overtime_ticking_) {
    if (now > start_time_) {
      accumulated_ns_ += (now - start_time_);
    }
    overtime_ticking_ = false;
  }
}

void CountdownTimer::toggle(TimePoint now) noexcept {
  if (state_ == TimerState::Running || (state_ == TimerState::Finished && overtime_ticking_)) {
    pause(now);
  } else {
    start(now);
  }
}

void CountdownTimer::reset() noexcept {
  state_ = TimerState::Idle;
  active_target_ns_ = configured_target_ns_;
  start_time_ = TimePoint{};
  accumulated_ns_ = std::chrono::nanoseconds{0};
  overtime_ticking_ = false;
}

void CountdownTimer::restart(TimePoint now) noexcept {
  reset();
  start(now);
}

void CountdownTimer::add_time(std::chrono::milliseconds delta, TimePoint now) noexcept {
  const auto delta_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(delta);
  if (state_ == TimerState::Idle) {
    configured_target_ns_ = std::max(std::chrono::nanoseconds{0}, configured_target_ns_ + delta_ns);
    active_target_ns_ = configured_target_ns_;
    return;
  }

  active_target_ns_ = std::max(std::chrono::nanoseconds{0}, active_target_ns_ + delta_ns);

  const auto current_elapsed = raw_elapsed_ns(now);
  if (state_ == TimerState::Finished && current_elapsed < active_target_ns_) {
    if (overtime_ticking_) {
      state_ = TimerState::Running;
      overtime_ticking_ = false;
    } else {
      state_ = TimerState::Paused;
    }
  } else if (state_ == TimerState::Running && current_elapsed >= active_target_ns_) {
    (void)update(now);
  }
}

bool CountdownTimer::update(TimePoint now) noexcept {
  if (state_ != TimerState::Running) {
    return false;
  }

  const auto current_elapsed = raw_elapsed_ns(now);
  if (current_elapsed < active_target_ns_) {
    return false;
  }

  if (auto_repeat_ && active_target_ns_.count() > 0) {
    const auto cycles =
      static_cast<std::size_t>(current_elapsed.count() / active_target_ns_.count());
    completed_cycles_ += std::max<std::size_t>(1U, cycles);
    accumulated_ns_ = std::chrono::nanoseconds{current_elapsed.count() % active_target_ns_.count()};
    start_time_ = now;
    return true;
  }

  completed_cycles_ += 1U;
  accumulated_ns_ = current_elapsed;
  start_time_ = now;
  state_ = TimerState::Finished;
  overtime_ticking_ = true;
  return true;
}

std::chrono::nanoseconds CountdownTimer::raw_elapsed_ns(TimePoint now) const noexcept {
  if ((state_ == TimerState::Running || (state_ == TimerState::Finished && overtime_ticking_)) &&
      now > start_time_) {
    return accumulated_ns_ + (now - start_time_);
  }
  return accumulated_ns_;
}

std::chrono::milliseconds CountdownTimer::elapsed(TimePoint now) const noexcept {
  return std::chrono::duration_cast<std::chrono::milliseconds>(raw_elapsed_ns(now));
}

std::chrono::milliseconds CountdownTimer::remaining(TimePoint now) const noexcept {
  const auto current_elapsed = raw_elapsed_ns(now);
  if (current_elapsed >= active_target_ns_) {
    return std::chrono::milliseconds{0};
  }
  return std::chrono::duration_cast<std::chrono::milliseconds>(active_target_ns_ - current_elapsed);
}

std::chrono::milliseconds CountdownTimer::overtime(TimePoint now) const noexcept {
  const auto current_elapsed = raw_elapsed_ns(now);
  if (current_elapsed <= active_target_ns_) {
    return std::chrono::milliseconds{0};
  }
  return std::chrono::duration_cast<std::chrono::milliseconds>(current_elapsed - active_target_ns_);
}

double CountdownTimer::progress(TimePoint now) const noexcept {
  if (active_target_ns_.count() <= 0) {
    return state_ == TimerState::Finished ? 1.0 : 0.0;
  }
  const auto current_elapsed = raw_elapsed_ns(now);
  const double ratio =
    static_cast<double>(current_elapsed.count()) / static_cast<double>(active_target_ns_.count());
  return std::clamp(ratio, 0.0, 1.0);
}

std::span<const TimerPreset> CountdownTimer::default_presets() noexcept { return kDefaultPresets; }

} // namespace sw::core
