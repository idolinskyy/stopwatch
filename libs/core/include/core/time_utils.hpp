// Copyright (C) 2026 Ihor Dolinskyi
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace sw::core {

struct DecomposedTime {
  std::int64_t hours{0};
  int minutes{0};
  int seconds{0};
  int milliseconds{0};
  int centiseconds{0};
  bool negative{false};
};

[[nodiscard]] DecomposedTime decompose(std::chrono::milliseconds duration) noexcept;

[[nodiscard]] std::chrono::milliseconds compose(std::int64_t hours, int minutes, int seconds,
                                                int milliseconds = 0) noexcept;

[[nodiscard]] std::string format_digital(std::chrono::milliseconds duration,
                                         bool always_show_hours = true,
                                         bool show_milliseconds = true);

[[nodiscard]] std::string format_centiseconds(std::chrono::milliseconds duration,
                                              bool always_show_hours = true);

[[nodiscard]] std::string format_delta(std::chrono::milliseconds delta);

[[nodiscard]] std::string format_compact(std::chrono::milliseconds duration);

[[nodiscard]] std::optional<std::chrono::milliseconds> parse_duration(std::string_view text);

} // namespace sw::core
