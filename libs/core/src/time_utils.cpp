// Copyright (C) 2026 Ihor Dolinskyi
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/time_utils.hpp"

#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <format>
#include <vector>

namespace sw::core {

namespace {

constexpr std::int64_t kMsPerSecond = 1'000LL;
constexpr std::int64_t kMsPerMinute = 60LL * kMsPerSecond;
constexpr std::int64_t kMsPerHour = 60LL * kMsPerMinute;

[[nodiscard]] std::string_view trim(std::string_view sv) noexcept {
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front())) != 0) {
    sv.remove_prefix(1);
  }
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back())) != 0) {
    sv.remove_suffix(1);
  }
  return sv;
}

[[nodiscard]] std::optional<std::int64_t> parse_non_negative_int(std::string_view sv) noexcept {
  sv = trim(sv);
  if (sv.empty()) {
    return std::nullopt;
  }
  std::int64_t value = 0;
  const auto *first = sv.data();
  const auto *last = sv.data() + sv.size();
  const auto [ptr, ec] = std::from_chars(first, last, value);
  if (ec != std::errc{} || ptr != last || value < 0) {
    return std::nullopt;
  }
  return value;
}

[[nodiscard]] std::optional<std::int64_t>
parse_seconds_with_fraction(std::string_view sv) noexcept {
  sv = trim(sv);
  if (sv.empty()) {
    return std::nullopt;
  }
  const auto dot_pos = sv.find('.');
  if (dot_pos == std::string_view::npos) {
    const auto sec = parse_non_negative_int(sv);
    if (!sec.has_value()) {
      return std::nullopt;
    }
    return *sec * kMsPerSecond;
  }

  const auto whole_part = sv.substr(0, dot_pos);
  const auto frac_part = sv.substr(dot_pos + 1);
  if (whole_part.empty() || frac_part.empty()) {
    return std::nullopt;
  }

  const auto sec = parse_non_negative_int(whole_part);
  if (!sec.has_value()) {
    return std::nullopt;
  }

  for (const char ch : frac_part) {
    if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
      return std::nullopt;
    }
  }

  std::int64_t millis = 0;
  if (frac_part.size() == 1) {
    millis = (frac_part[0] - '0') * 100LL;
  } else if (frac_part.size() == 2) {
    millis = ((frac_part[0] - '0') * 10LL + (frac_part[1] - '0')) * 10LL;
  } else {
    millis = (frac_part[0] - '0') * 100LL + (frac_part[1] - '0') * 10LL + (frac_part[2] - '0');
  }

  return (*sec * kMsPerSecond) + millis;
}

[[nodiscard]] std::optional<std::chrono::milliseconds>
parse_colon_duration(std::string_view sv) noexcept {
  std::vector<std::string_view> parts;
  std::size_t start = 0;
  while (start <= sv.size()) {
    const auto pos = sv.find(':', start);
    if (pos == std::string_view::npos) {
      parts.push_back(sv.substr(start));
      break;
    }
    parts.push_back(sv.substr(start, pos - start));
    start = pos + 1;
  }

  if (parts.size() != 2U && parts.size() != 3U) {
    return std::nullopt;
  }

  std::int64_t hours = 0;
  std::int64_t minutes = 0;
  std::int64_t seconds_ms = 0;

  if (parts.size() == 2U) {
    const auto m = parse_non_negative_int(parts[0]);
    const auto s = parse_seconds_with_fraction(parts[1]);
    if (!m.has_value() || !s.has_value()) {
      return std::nullopt;
    }
    minutes = *m;
    seconds_ms = *s;
  } else {
    const auto h = parse_non_negative_int(parts[0]);
    const auto m = parse_non_negative_int(parts[1]);
    const auto s = parse_seconds_with_fraction(parts[2]);
    if (!h.has_value() || !m.has_value() || !s.has_value()) {
      return std::nullopt;
    }
    hours = *h;
    minutes = *m;
    seconds_ms = *s;
  }

  const std::int64_t total_ms = (hours * kMsPerHour) + (minutes * kMsPerMinute) + seconds_ms;
  return std::chrono::milliseconds{total_ms};
}

[[nodiscard]] std::optional<std::int64_t> unit_multiplier_ms(std::string_view unit) noexcept {
  if (unit == "ms" || unit == "мс") {
    return 1LL;
  }
  if (unit.empty() || unit == "s" || unit == "sec" || unit == "secs" || unit == "с" ||
      unit == "сек") {
    return kMsPerSecond;
  }
  if (unit == "m" || unit == "min" || unit == "mins" || unit == "хв") {
    return kMsPerMinute;
  }
  if (unit == "h" || unit == "hr" || unit == "hrs" || unit == "г" || unit == "год") {
    return kMsPerHour;
  }
  return std::nullopt;
}

} // namespace

DecomposedTime decompose(std::chrono::milliseconds duration) noexcept {
  DecomposedTime result{};
  std::int64_t total_ms = duration.count();
  if (total_ms < 0) {
    result.negative = true;
    total_ms = -total_ms;
  }

  result.hours = total_ms / kMsPerHour;
  const std::int64_t rem_hours = total_ms % kMsPerHour;
  result.minutes = static_cast<int>(rem_hours / kMsPerMinute);
  const std::int64_t rem_minutes = rem_hours % kMsPerMinute;
  result.seconds = static_cast<int>(rem_minutes / kMsPerSecond);
  result.milliseconds = static_cast<int>(rem_minutes % kMsPerSecond);
  result.centiseconds = result.milliseconds / 10;
  return result;
}

std::chrono::milliseconds compose(std::int64_t hours, int minutes, int seconds,
                                  int milliseconds) noexcept {
  const std::int64_t total_ms =
    (hours * kMsPerHour) + (static_cast<std::int64_t>(minutes) * kMsPerMinute) +
    (static_cast<std::int64_t>(seconds) * kMsPerSecond) + static_cast<std::int64_t>(milliseconds);
  return std::chrono::milliseconds{total_ms};
}

std::string format_digital(std::chrono::milliseconds duration, bool always_show_hours,
                           bool show_milliseconds) {
  const auto parts = decompose(duration);
  const std::string_view sign = parts.negative ? "-" : "";

  if (always_show_hours || parts.hours > 0) {
    if (show_milliseconds) {
      return std::format("{}{:02}:{:02}:{:02}.{:03}", sign, parts.hours, parts.minutes,
                         parts.seconds, parts.milliseconds);
    }
    return std::format("{}{:02}:{:02}:{:02}", sign, parts.hours, parts.minutes, parts.seconds);
  }

  if (show_milliseconds) {
    return std::format("{}{:02}:{:02}.{:03}", sign, parts.minutes, parts.seconds,
                       parts.milliseconds);
  }
  return std::format("{}{:02}:{:02}", sign, parts.minutes, parts.seconds);
}

std::string format_centiseconds(std::chrono::milliseconds duration, bool always_show_hours) {
  const auto parts = decompose(duration);
  const std::string_view sign = parts.negative ? "-" : "";

  if (always_show_hours || parts.hours > 0) {
    return std::format("{}{:02}:{:02}:{:02}.{:02}", sign, parts.hours, parts.minutes, parts.seconds,
                       parts.centiseconds);
  }
  return std::format("{}{:02}:{:02}.{:02}", sign, parts.minutes, parts.seconds, parts.centiseconds);
}

std::string format_delta(std::chrono::milliseconds delta) {
  if (delta.count() == 0) {
    return " 00:00.000";
  }
  const auto parts = decompose(delta);
  const char sign = parts.negative ? '-' : '+';
  if (parts.hours > 0) {
    return std::format("{}{:02}:{:02}:{:02}.{:03}", sign, parts.hours, parts.minutes, parts.seconds,
                       parts.milliseconds);
  }
  return std::format("{}{:02}:{:02}.{:03}", sign, parts.minutes, parts.seconds, parts.milliseconds);
}

std::string format_compact(std::chrono::milliseconds duration) {
  const auto parts = decompose(duration);
  if (parts.hours > 0) {
    return std::format("{}г {:02}хв {:02}с", parts.hours, parts.minutes, parts.seconds);
  }
  if (parts.minutes > 0) {
    if (parts.seconds > 0) {
      return std::format("{}хв {:02}с", parts.minutes, parts.seconds);
    }
    return std::format("{}хв", parts.minutes);
  }
  if (parts.milliseconds > 0 && parts.seconds < 10) {
    return std::format("{}.{:01}с", parts.seconds, parts.milliseconds / 100);
  }
  return std::format("{}с", parts.seconds);
}

std::optional<std::chrono::milliseconds> parse_duration(std::string_view text) {
  text = trim(text);
  if (text.empty()) {
    return std::nullopt;
  }

  if (text.find(':') != std::string_view::npos) {
    return parse_colon_duration(text);
  }

  std::int64_t total_ms = 0;
  std::size_t idx = 0;
  bool any_token = false;

  while (idx < text.size()) {
    while (idx < text.size() && std::isspace(static_cast<unsigned char>(text[idx])) != 0) {
      ++idx;
    }
    if (idx >= text.size()) {
      break;
    }

    const std::size_t num_start = idx;
    bool has_dot = false;
    while (idx < text.size()) {
      const auto ch = static_cast<unsigned char>(text[idx]);
      if (std::isdigit(ch) != 0) {
        ++idx;
      } else if (text[idx] == '.' && !has_dot) {
        has_dot = true;
        ++idx;
      } else {
        break;
      }
    }

    if (idx == num_start) {
      return std::nullopt;
    }

    const std::string_view num_str = text.substr(num_start, idx - num_start);
    if (num_str == ".") {
      return std::nullopt;
    }

    while (idx < text.size() && std::isspace(static_cast<unsigned char>(text[idx])) != 0) {
      ++idx;
    }

    const std::size_t unit_start = idx;
    while (idx < text.size()) {
      const auto ch = static_cast<unsigned char>(text[idx]);
      if (std::isdigit(ch) != 0 || std::isspace(ch) != 0 || text[idx] == '.') {
        break;
      }
      ++idx;
    }

    std::string lower_unit;
    for (const char ch : text.substr(unit_start, idx - unit_start)) {
      const auto uch = static_cast<unsigned char>(ch);
      if (uch < 128U) {
        lower_unit.push_back(static_cast<char>(std::tolower(uch)));
      } else {
        lower_unit.push_back(ch);
      }
    }

    const auto mult = unit_multiplier_ms(lower_unit);
    if (!mult.has_value()) {
      return std::nullopt;
    }

    if (!has_dot) {
      const auto val = parse_non_negative_int(num_str);
      if (!val.has_value()) {
        return std::nullopt;
      }
      total_ms += (*val) * (*mult);
    } else {
      char *end_ptr = nullptr;
      const std::string num_copy{num_str};
      const double dval = std::strtod(num_copy.c_str(), &end_ptr);
      if (end_ptr != num_copy.c_str() + num_copy.size() || dval < 0.0 || !std::isfinite(dval)) {
        return std::nullopt;
      }
      total_ms += static_cast<std::int64_t>(std::llround(dval * static_cast<double>(*mult)));
    }
    any_token = true;
  }

  if (!any_token) {
    return std::nullopt;
  }
  return std::chrono::milliseconds{total_ms};
}

} // namespace sw::core
