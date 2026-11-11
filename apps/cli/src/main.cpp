// Copyright (C) 2026 Ihor Dolinskyi
// SPDX-License-Identifier: GPL-3.0-or-later

#include <core/stopwatch.hpp>
#include <core/time_utils.hpp>
#include <core/timer.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <poll.h>
#include <termios.h>
#include <unistd.h>

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile std::sig_atomic_t g_stop_requested = 0;

void handle_signal(int /*signum*/) noexcept { g_stop_requested = 1; }

class RawTerminalGuard {
public:
  RawTerminalGuard() noexcept {
    if (::isatty(STDIN_FILENO) != 1) {
      return;
    }
    if (::tcgetattr(STDIN_FILENO, &orig_) == 0) {
      termios raw = orig_;
      raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
      raw.c_cc[VMIN] = 0;
      raw.c_cc[VTIME] = 0;
      if (::tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
        active_ = true;
        std::print("\033[?1049h\033[?25l\033[2J");
        std::fflush(stdout);
      }
    }
  }

  ~RawTerminalGuard() { restore(); }

  RawTerminalGuard(const RawTerminalGuard &) = delete;
  RawTerminalGuard &operator=(const RawTerminalGuard &) = delete;

  void restore() noexcept {
    if (active_) {
      (void)::tcsetattr(STDIN_FILENO, TCSANOW, &orig_);
      std::print("\033[?25h\033[?1049l");
      std::fflush(stdout);
      active_ = false;
    }
  }

  void read_line(const std::string &prompt, std::string &out_line) {
    if (!active_) {
      return;
    }
    (void)::tcsetattr(STDIN_FILENO, TCSANOW, &orig_);
    std::print("\033[?25h\n  \033[1;36m{}\033[0m", prompt);
    std::fflush(stdout);
    out_line.clear();
    (void)std::getline(std::cin, out_line);

    termios raw = orig_;
    raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    (void)::tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    std::print("\033[?25l\033[2J");
    std::fflush(stdout);
  }

  [[nodiscard]] bool is_active() const noexcept { return active_; }

private:
  termios orig_{};
  bool active_{false};
};

struct ColorTheme {
  std::string_view name;
  std::string_view primary_fg;
  std::string_view dim_fg;
  std::string_view accent_fg;
};

constexpr std::array<ColorTheme, 4> kThemes = {{
  {.name = "Смарагдовий",
   .primary_fg = "\033[1;38;5;48m",
   .dim_fg = "\033[38;5;236m",
   .accent_fg = "\033[1;38;5;84m"},
  {.name = "Бурштиновий",
   .primary_fg = "\033[1;38;5;214m",
   .dim_fg = "\033[38;5;236m",
   .accent_fg = "\033[1;38;5;220m"},
  {.name = "Блакитний",
   .primary_fg = "\033[1;38;5;51m",
   .dim_fg = "\033[38;5;236m",
   .accent_fg = "\033[1;38;5;45m"},
  {.name = "Червоний",
   .primary_fg = "\033[1;38;5;197m",
   .dim_fg = "\033[38;5;236m",
   .accent_fg = "\033[1;38;5;203m"},
}};

[[nodiscard]] std::array<std::string_view, 5> glyph_rows(char ch) noexcept {
  switch (ch) {
  case '0':
    return {"█████", "██ ██", "██ ██", "██ ██", "█████"};
  case '1':
    return {"   ██", "   ██", "   ██", "   ██", "   ██"};
  case '2':
    return {"█████", "   ██", "█████", "██   ", "█████"};
  case '3':
    return {"█████", "   ██", "█████", "   ██", "█████"};
  case '4':
    return {"██ ██", "██ ██", "█████", "   ██", "   ██"};
  case '5':
    return {"█████", "██   ", "█████", "   ██", "█████"};
  case '6':
    return {"█████", "██   ", "█████", "██ ██", "█████"};
  case '7':
    return {"█████", "   ██", "   ██", "   ██", "   ██"};
  case '8':
    return {"█████", "██ ██", "█████", "██ ██", "█████"};
  case '9':
    return {"█████", "██ ██", "█████", "   ██", "█████"};
  case ':':
    return {"   ", " ██", "   ", " ██", "   "};
  case '.':
    return {"   ", "   ", "   ", "   ", " ██"};
  case '+':
    return {"     ", "  █  ", "█████", "  █  ", "     "};
  case '-':
    return {"     ", "     ", "█████", "     ", "     "};
  default:
    return {"   ", "   ", "   ", "   ", "   "};
  }
}

[[nodiscard]] std::size_t utf8_columns(std::string_view text) noexcept {
  std::size_t columns = 0;
  for (const char raw : text) {
    const auto ch = static_cast<unsigned char>(raw);
    if ((ch & 0xC0U) != 0x80U) {
      ++columns;
    }
  }
  return columns;
}

void render_big_digital(std::string_view text, const ColorTheme &theme, bool alert_flash) {
  constexpr std::size_t kRows = 5U;
  std::array<std::string, kRows> rows;
  std::size_t width = 0;
  for (std::size_t row = 0; row < kRows; ++row) {
    std::string line;
    bool first = true;
    for (const char ch : text) {
      if (!first) {
        line.push_back(' ');
      }
      first = false;
      line.append(glyph_rows(ch)[row]);
    }
    width = std::max(width, utf8_columns(line));
    rows[row] = std::move(line);
  }
  for (std::string &row : rows) {
    const std::size_t columns = utf8_columns(row);
    if (columns < width) {
      row.append(width - columns, ' ');
    }
  }

  std::string rule;
  rule.reserve((width + 2U) * 3U);
  for (std::size_t i = 0; i < width + 2U; ++i) {
    rule += "─";
  }

  const std::string_view color = alert_flash ? "\033[1;5;38;5;196m" : theme.primary_fg;
  std::print("  {}╭{}╮\033[0m\033[K\n", theme.accent_fg, rule);
  for (const std::string &row : rows) {
    std::print("  {}│\033[0m {}{}\033[0m {}│\033[0m\033[K\n", theme.accent_fg, color, row,
               theme.accent_fg);
  }
  std::print("  {}╰{}╯\033[0m\033[K\n", theme.accent_fg, rule);
}

void print_help() {
  std::println("StopWatch — секундомір і таймер");
  std::println("");
  std::println("  StopWatch                        табло в терміналі");
  std::println("  StopWatch --stopwatch [--auto]   секундомір");
  std::println("  StopWatch --timer <час> [--auto] таймер: 5m30s, 00:10:00, 90s");
  std::println("  StopWatch --headless-timer <час> один рядок, без повного екрана");
  std::println("  StopWatch --help");
  std::println("");
  std::println("  Пробіл      старт / пауза");
  std::println("  L           коло або автоповтор");
  std::println("  R           скинути");
  std::println("  Tab, 1, 2   секундомір / таймер");
  std::println("  S, T        ввести інтервал");
  std::println("  P           наступний пресет");
  std::println("  +, -        ±30 с");
  std::println("  N           примітка до останнього кола");
  std::println("  E           stopwatch_laps.csv і stopwatch_laps.md");
  std::println("  C           наступна тема");
  std::println("  Q           вихід");
}

int run_headless_timer(std::chrono::milliseconds duration) {
  sw::core::CountdownTimer timer(duration);
  timer.start();
  std::println("Таймер: {} ({}). Ctrl+C — зупинити.", sw::core::format_compact(duration),
               sw::core::format_digital(duration));

  while (g_stop_requested == 0) {
    const bool finished_now = timer.update();
    const auto rem = timer.remaining();
    const double prog = timer.progress();
    const int filled = static_cast<int>(prog * 30.0);

    std::string bar;
    for (int i = 0; i < 30; ++i) {
      bar += (i < filled) ? "█" : "░";
    }

    std::print("\r  \033[1;32m{}\033[0m  [{}] {:5.1f}% ", sw::core::format_digital(rem), bar,
               prog * 100.0);
    std::fflush(stdout);

    if (finished_now || timer.is_finished()) {
      std::print("\a\nЧас вийшов ({}).\n", sw::core::format_digital(duration));
      return 0;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{30});
  }
  std::println("\nЗупинено.");
  return 0;
}

} // namespace

int main(int argc, char *argv[]) {
  (void)std::signal(SIGINT, handle_signal);
  (void)std::signal(SIGTERM, handle_signal);

  enum class Mode { Stopwatch, Timer };
  Mode active_mode = Mode::Stopwatch;
  bool auto_start = false;
  std::optional<std::chrono::milliseconds> initial_timer_duration;
  bool headless_timer = false;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};
    if (arg == "--help" || arg == "-h") {
      print_help();
      return 0;
    }
    if (arg == "--stopwatch" || arg == "-s") {
      active_mode = Mode::Stopwatch;
    } else if (arg == "--auto" || arg == "-a") {
      auto_start = true;
    } else if ((arg == "--timer" || arg == "-t") && i + 1 < argc) {
      active_mode = Mode::Timer;
      initial_timer_duration = sw::core::parse_duration(argv[++i]);
      if (!initial_timer_duration.has_value()) {
        std::println(stderr, "Невідомий час '{}'. Приклад: 5m30s, 05:00, 90s", argv[i]);
        return 1;
      }
    } else if (arg == "--headless-timer" && i + 1 < argc) {
      headless_timer = true;
      initial_timer_duration = sw::core::parse_duration(argv[++i]);
      if (!initial_timer_duration.has_value()) {
        std::println(stderr, "Невідомий час '{}'.", argv[i]);
        return 1;
      }
    } else {
      std::println(stderr, "Невідомий аргумент '{}'. Див. --help.", arg);
      return 1;
    }
  }

  if (headless_timer) {
    return run_headless_timer(initial_timer_duration.value_or(std::chrono::minutes{5}));
  }

  if (::isatty(STDIN_FILENO) != 1) {
    if (active_mode == Mode::Timer) {
      return run_headless_timer(initial_timer_duration.value_or(std::chrono::minutes{5}));
    }
    print_help();
    return 0;
  }

  sw::core::Stopwatch stopwatch;
  sw::core::CountdownTimer timer(initial_timer_duration.value_or(std::chrono::minutes{5}));
  std::size_t preset_index = 1U;
  std::size_t theme_index = 0U;
  std::string status_banner = "Готово.";

  if (auto_start) {
    if (active_mode == Mode::Stopwatch) {
      stopwatch.start();
    } else {
      timer.start();
    }
  }

  RawTerminalGuard term_guard;

  while (g_stop_requested == 0) {
    if (timer.update()) {
      std::print("\a");
      status_banner = "Час вийшов.";
    }

    const auto &theme = kThemes[theme_index];

    std::print("\033[H");
    std::print("  StopWatch    тема: {}{}\033[0m\033[K\n", theme.accent_fg, theme.name);

    if (active_mode == Mode::Stopwatch) {
      std::print("  \033[7;1m Секундомір \033[0m   \033[2m Таймер \033[0m\033[K\n\033[K\n");

      const auto elapsed = stopwatch.elapsed();
      const auto current_lap = stopwatch.current_lap_elapsed();
      const std::string main_digits = sw::core::format_digital(elapsed, true, true);

      render_big_digital(main_digits, theme, false);

      std::string_view state_badge = "\033[1;33mготовий\033[0m";
      if (stopwatch.state() == sw::core::StopwatchState::Running) {
        state_badge = "\033[1;32mіде\033[0m";
      } else if (stopwatch.state() == sw::core::StopwatchState::Paused) {
        state_badge = "\033[1;36mпауза\033[0m";
      }

      std::print("  Статус: {}   │   Поточне коло: {}{}\033[0m\033[K\n\n", state_badge,
                 theme.accent_fg, sw::core::format_digital(current_lap, true, true));

      if (const auto stats = stopwatch.statistics(); stats.has_value()) {
        std::print("  Кіл: {}  │  найкраще #{} ({})  │  найгірше #{} ({})  │  середнє {}\033[K\n",
                   stats->count, stats->fastest_lap_number,
                   sw::core::format_digital(stats->fastest_split, false, true),
                   stats->slowest_lap_number,
                   sw::core::format_digital(stats->slowest_split, false, true),
                   sw::core::format_digital(stats->average_split, false, true));
        std::print(
          "  "
          "──────────────────────────────────────────────────────────────────────────────\033[K\n");
        std::print("  \033[1m{:>4}   {:>14}   {:>14}   {:>12}   {:<20}\033[0m\033[K\n", "№",
                   "Час кола", "Загальний", "Δ", "Примітка");

        const auto &laps = stopwatch.laps();
        const std::size_t max_rows = 7U;
        const std::size_t start_idx = laps.size() > max_rows ? (laps.size() - max_rows) : 0U;
        for (std::size_t i = laps.size(); i > start_idx; --i) {
          const auto &lap = laps[i - 1U];
          std::string_view row_color = "";
          if (stats->count >= 2U) {
            if (lap.number == stats->fastest_lap_number) {
              row_color = "\033[32m";
            } else if (lap.number == stats->slowest_lap_number) {
              row_color = "\033[31m";
            }
          }
          std::print("  {}{:>4}   {:>14}   {:>14}   {:>12}   {:<20}\033[0m\033[K\n", row_color,
                     lap.number, sw::core::format_digital(lap.split, false, true),
                     sw::core::format_digital(lap.total, false, true),
                     sw::core::format_delta(lap.delta), lap.note);
        }
      } else {
        std::print("  \033[2mКіл ще немає.\033[0m\033[K\n");
      }
    } else {
      std::print("  \033[2m Секундомір \033[0m   \033[7;1m Таймер \033[0m\033[K\n\033[K\n");

      const bool finished = timer.is_finished();
      const auto display_time = finished ? timer.overtime() : timer.remaining();
      const std::string prefix = finished ? "+" : "";
      const std::string main_digits = prefix + sw::core::format_digital(display_time, true, true);

      render_big_digital(main_digits, theme, finished);

      std::string_view state_badge = "\033[1;33mготовий\033[0m";
      if (timer.state() == sw::core::TimerState::Running) {
        state_badge = "\033[1;32mвідлік\033[0m";
      } else if (timer.state() == sw::core::TimerState::Paused) {
        state_badge = "\033[1;36mпауза\033[0m";
      } else if (timer.is_overtime_running()) {
        state_badge = "\033[1;5;31mперебіг\033[0m";
      } else if (timer.state() == sw::core::TimerState::Finished) {
        state_badge = "\033[1;31mчас вийшов\033[0m";
      }

      const double prog = timer.progress();
      const int bar_width = 44;
      const int filled = static_cast<int>(prog * static_cast<double>(bar_width));
      std::string bar;
      for (int i = 0; i < bar_width; ++i) {
        bar += (i < filled) ? "█" : "░";
      }

      std::print(
        "  Статус: {}  │  ціль: \033[1m{}\033[0m  │  автоповтор: {}  │  циклів: {}\033[K\n",
        state_badge, sw::core::format_digital(timer.active_target(), true, false),
        timer.auto_repeat() ? "\033[1;32mувімкнено\033[0m" : "\033[2mвимкнено\033[0m",
        timer.completed_cycles());
      std::print("  Прогрес: {}{}\033[0m {:5.1f}%\033[K\n\n", theme.primary_fg, bar, prog * 100.0);

      std::print("  Пресети: ");
      const auto presets = sw::core::CountdownTimer::default_presets();
      for (std::size_t i = 0; i < presets.size(); ++i) {
        if (i == preset_index) {
          std::print("\033[7;1m {} \033[0m ", presets[i].name);
        } else {
          std::print("\033[2m{}\033[0m ", presets[i].name);
        }
      }
      std::print("\033[K\n");
    }

    std::print(
      "\n  ──────────────────────────────────────────────────────────────────────────────\033[K\n");
    std::print("  {}\033[K\n", status_banner);
    std::print("  \033[2mПробіл\033[0m старт/пауза  \033[2mL\033[0m коло/повтор  "
               "\033[2mR\033[0m скинути  \033[2m1/2\033[0m режим  \033[2mC\033[0m тема\033[K\n");
    std::print("  \033[2mS\033[0m інтервал  \033[2mP\033[0m пресет  \033[2m+/-\033[0m 30 с  "
               "\033[2mN\033[0m примітка  \033[2mE\033[0m експорт  \033[2mQ\033[0m вихід\033[J\n");
    std::fflush(stdout);

    pollfd pfd{.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
    const int ready = ::poll(&pfd, 1, 33);
    if (ready > 0 && (pfd.revents & POLLIN) != 0) {
      char ch = 0;
      if (::read(STDIN_FILENO, &ch, 1) > 0) {
        if (ch == 'q' || ch == 'Q') {
          break;
        }
        if (ch == '\t') {
          active_mode = (active_mode == Mode::Stopwatch) ? Mode::Timer : Mode::Stopwatch;
          status_banner = (active_mode == Mode::Stopwatch) ? "Секундомір." : "Таймер.";
        } else if (ch == '1') {
          active_mode = Mode::Stopwatch;
          status_banner = "Секундомір.";
        } else if (ch == '2') {
          active_mode = Mode::Timer;
          status_banner = "Таймер.";
        } else if (ch == ' ') {
          if (active_mode == Mode::Stopwatch) {
            stopwatch.toggle();
            status_banner = stopwatch.is_running() ? "Секундомір іде." : "Пауза.";
          } else {
            timer.toggle();
            status_banner = timer.is_running() ? "Відлік іде." : "Пауза.";
          }
        } else if (ch == 'r' || ch == 'R') {
          if (active_mode == Mode::Stopwatch) {
            stopwatch.reset();
            status_banner = "Секундомір скинуто.";
          } else {
            timer.reset();
            status_banner = "Таймер скинуто.";
          }
        } else if (ch == 'l' || ch == 'L') {
          if (active_mode == Mode::Stopwatch) {
            if (const auto rec = stopwatch.lap(); rec.has_value()) {
              status_banner = std::format("Коло #{}: {}", rec->number,
                                          sw::core::format_digital(rec->split, false, true));
            } else {
              status_banner = "Секундомір не запущено.";
            }
          } else {
            timer.set_auto_repeat(!timer.auto_repeat());
            status_banner = timer.auto_repeat() ? "Автоповтор увімкнено." : "Автоповтор вимкнено.";
          }
        } else if (ch == 'c' || ch == 'C') {
          theme_index = (theme_index + 1U) % kThemes.size();
          status_banner = std::format("Тема: {}", kThemes[theme_index].name);
        } else if (ch == 'p' || ch == 'P') {
          active_mode = Mode::Timer;
          const auto presets = sw::core::CountdownTimer::default_presets();
          preset_index = (preset_index + 1U) % presets.size();
          timer.set_target(presets[preset_index].duration);
          status_banner = std::format("Пресет: {}", presets[preset_index].name);
        } else if (ch == '+' || ch == '=') {
          active_mode = Mode::Timer;
          timer.add_time(std::chrono::seconds{30});
          status_banner = "+30 с.";
        } else if (ch == '-' || ch == '_') {
          active_mode = Mode::Timer;
          timer.add_time(std::chrono::seconds{-30});
          status_banner = "−30 с.";
        } else if (ch == 's' || ch == 'S' || ch == 't' || ch == 'T') {
          active_mode = Mode::Timer;
          std::string input;
          term_guard.read_line("Час (5m30s, 10:00, 90s): ", input);
          if (const auto parsed = sw::core::parse_duration(input); parsed.has_value()) {
            timer.set_target(*parsed);
            status_banner =
              std::format("Таймер: {}", sw::core::format_digital(*parsed, true, false));
          } else if (!input.empty()) {
            status_banner = "Не розпізнано час.";
          }
        } else if (ch == 'n' || ch == 'N') {
          if (!stopwatch.laps().empty()) {
            std::string note;
            const std::size_t last_num = stopwatch.laps().back().number;
            term_guard.read_line(std::format("Примітка до кола #{}: ", last_num), note);
            if (!note.empty()) {
              (void)stopwatch.set_lap_note(last_num, note);
              status_banner = std::format("Примітку до кола #{} записано.", last_num);
            }
          } else {
            status_banner = "Немає кіл.";
          }
        } else if (ch == 'e' || ch == 'E') {
          if (!stopwatch.laps().empty()) {
            std::ofstream csv_file("stopwatch_laps.csv");
            std::ofstream md_file("stopwatch_laps.md");
            if (!csv_file || !md_file) {
              status_banner = "Не вдалося записати файли.";
            } else {
              csv_file << stopwatch.export_csv();
              md_file << stopwatch.export_markdown();
              status_banner = "Записано stopwatch_laps.csv і stopwatch_laps.md.";
            }
          } else {
            status_banner = "Немає кіл для експорту.";
          }
        }
      }
    }
  }

  return 0;
}
