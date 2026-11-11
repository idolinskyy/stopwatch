// Copyright (C) 2026 Ihor Dolinskyi
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>
#include <core/stopwatch.hpp>
#include <core/time_utils.hpp>

using namespace std::chrono_literals;

TEST_CASE("time formatting and parsing", "[core][time]") {
  const auto duration = sw::core::compose(1, 23, 45, 678);
  REQUIRE(duration == 5'025'678ms);

  const auto parts = sw::core::decompose(duration);
  REQUIRE_FALSE(parts.negative);
  REQUIRE(parts.hours == 1);
  REQUIRE(parts.minutes == 23);
  REQUIRE(parts.seconds == 45);
  REQUIRE(parts.milliseconds == 678);
  REQUIRE(parts.centiseconds == 67);

  REQUIRE(sw::core::format_digital(duration, true, true) == "01:23:45.678");
  REQUIRE(sw::core::format_digital(duration, true, false) == "01:23:45");
  REQUIRE(sw::core::format_centiseconds(duration, true) == "01:23:45.67");
  REQUIRE(sw::core::format_digital(83'450ms, false, true) == "01:23.450");

  REQUIRE(sw::core::format_delta(1'250ms) == "+00:01.250");
  REQUIRE(sw::core::format_delta(-500ms) == "-00:00.500");
  REQUIRE(sw::core::format_delta(0ms) == " 00:00.000");

  REQUIRE(sw::core::parse_duration("05:30") == 330'000ms);
  REQUIRE(sw::core::parse_duration("01:02:03.450") == 3'723'450ms);
  REQUIRE(sw::core::parse_duration("25m") == 1'500'000ms);
  REQUIRE(sw::core::parse_duration("1h 15m 30s") == 4'530'000ms);
  REQUIRE(sw::core::parse_duration("2хв 10с") == 130'000ms);
  REQUIRE(sw::core::parse_duration("90") == 90'000ms);
  REQUIRE_FALSE(sw::core::parse_duration("").has_value());
  REQUIRE_FALSE(sw::core::parse_duration("abc").has_value());
}

TEST_CASE("stopwatch laps", "[core][stopwatch]") {
  sw::core::Stopwatch sw;
  const auto t0 = sw::core::Stopwatch::Clock::now();

  REQUIRE(sw.state() == sw::core::StopwatchState::Idle);
  REQUIRE(sw.elapsed(t0) == 0ms);
  REQUIRE_FALSE(sw.lap("Ignored", t0).has_value());

  sw.start(t0);
  REQUIRE(sw.state() == sw::core::StopwatchState::Running);
  REQUIRE(sw.elapsed(t0 + 1'500ms) == 1'500ms);

  // Record Lap 1 at 1500ms
  const auto lap1 = sw.lap("Розминка", t0 + 1'500ms);
  REQUIRE(lap1.has_value());
  REQUIRE(lap1->number == 1U);
  REQUIRE(lap1->split == 1'500ms);
  REQUIRE(lap1->total == 1'500ms);
  REQUIRE(lap1->delta == 0ms);
  REQUIRE(lap1->note == "Розминка");

  // Pause at 2500ms, wait 5000ms while paused, then resume at 7500ms
  sw.pause(t0 + 2'500ms);
  REQUIRE(sw.state() == sw::core::StopwatchState::Paused);
  REQUIRE(sw.elapsed(t0 + 7'500ms) == 2'500ms);
  REQUIRE(sw.current_lap_elapsed(t0 + 7'500ms) == 1'000ms);

  sw.start(t0 + 7'500ms);
  // Advance 200ms more -> total elapsed 2700ms, Lap 2 split = 1200ms
  const auto lap2 = sw.lap("Спринт", t0 + 7'700ms);
  REQUIRE(lap2.has_value());
  REQUIRE(lap2->number == 2U);
  REQUIRE(lap2->split == 1'200ms);
  REQUIRE(lap2->total == 2'700ms);
  REQUIRE(lap2->delta == -300ms);

  // Lap 3 at +2100ms -> total 4800ms, split 2100ms
  const auto lap3 = sw.lap("", t0 + 9'800ms);
  REQUIRE(lap3.has_value());
  REQUIRE(lap3->number == 3U);
  REQUIRE(lap3->split == 2'100ms);
  REQUIRE(lap3->delta == 900ms);

  const auto stats = sw.statistics();
  REQUIRE(stats.has_value());
  REQUIRE(stats->count == 3U);
  REQUIRE(stats->fastest_lap_number == 2U);
  REQUIRE(stats->fastest_split == 1'200ms);
  REQUIRE(stats->slowest_lap_number == 3U);
  REQUIRE(stats->slowest_split == 2'100ms);
  REQUIRE(stats->average_split == 1'600ms);

  REQUIRE(sw.set_lap_note(3U, "Фініш"));
  REQUIRE(sw.laps()[2].note == "Фініш");
  REQUIRE_FALSE(sw.export_csv().empty());
  REQUIRE_FALSE(sw.export_markdown().empty());

  sw.reset();
  REQUIRE(sw.state() == sw::core::StopwatchState::Idle);
  REQUIRE(sw.laps().empty());
  REQUIRE(sw.elapsed(t0 + 10'000ms) == 0ms);
}
