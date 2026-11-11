// Copyright (C) 2026 Ihor Dolinskyi
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>
#include <core/timer.hpp>

using namespace std::chrono_literals;

TEST_CASE("countdown timer", "[core][timer]") {
  sw::core::CountdownTimer timer(10'000ms);
  const auto t0 = sw::core::CountdownTimer::Clock::now();

  REQUIRE(timer.state() == sw::core::TimerState::Idle);
  REQUIRE(timer.target() == 10'000ms);
  REQUIRE(timer.remaining(t0) == 10'000ms);
  REQUIRE(timer.progress(t0) == 0.0);

  timer.start(t0);
  REQUIRE(timer.state() == sw::core::TimerState::Running);
  REQUIRE_FALSE(timer.update(t0 + 4'000ms));
  REQUIRE(timer.remaining(t0 + 4'000ms) == 6'000ms);
  REQUIRE(timer.elapsed(t0 + 4'000ms) == 4'000ms);
  REQUIRE(timer.progress(t0 + 4'000ms) == 0.4);

  // Add +5s live while running -> active target becomes 15s
  timer.add_time(5'000ms, t0 + 4'000ms);
  REQUIRE(timer.active_target() == 15'000ms);
  REQUIRE(timer.remaining(t0 + 4'000ms) == 11'000ms);

  // Finish at t0 + 15s, check overtime at t0 + 17s
  REQUIRE(timer.update(t0 + 15'000ms));
  REQUIRE(timer.state() == sw::core::TimerState::Finished);
  REQUIRE(timer.completed_cycles() == 1U);
  REQUIRE(timer.remaining(t0 + 17'000ms) == 0ms);
  REQUIRE(timer.overtime(t0 + 17'000ms) == 2'000ms);
  REQUIRE(timer.is_overtime_running());

  timer.pause(t0 + 17'000ms);
  REQUIRE_FALSE(timer.is_overtime_running());
  REQUIRE(timer.overtime(t0 + 20'000ms) == 2'000ms);

  // Test auto-repeat mode
  timer.reset();
  timer.set_target(5'000ms);
  timer.set_auto_repeat(true);
  timer.reset_cycles();
  timer.start(t0 + 20'000ms);

  REQUIRE(timer.update(t0 + 25'500ms));
  REQUIRE(timer.state() == sw::core::TimerState::Running);
  REQUIRE(timer.completed_cycles() == 1U);
  REQUIRE(timer.remaining(t0 + 25'500ms) == 4'500ms);

  REQUIRE_FALSE(sw::core::CountdownTimer::default_presets().empty());
}
