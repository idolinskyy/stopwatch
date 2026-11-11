// Copyright (C) 2026 Ihor Dolinskyi
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "digital_board_widget.hpp"

#include <core/stopwatch.hpp>
#include <core/timer.hpp>

#include <QWidget>

#include <chrono>

class QButtonGroup;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QProcess;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTimer;

class MainWindow final : public QWidget {
public:
  explicit MainWindow(bool persist_settings = true, QWidget *parent = nullptr);

  [[nodiscard]] bool self_check(QString *error);

protected:
  void closeEvent(QCloseEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  enum class Mode { Stopwatch, Timer };

  void build_ui();
  void connect_signals();
  void load_settings();
  void save_settings() const;

  void on_tick();
  void refresh_board();
  void refresh_laps();
  void refresh_timer_labels();
  void sync_mode();
  void set_status(const QString &text);

  void toggle_run();
  void reset_active();
  void lap_or_repeat();
  void apply_duration();
  void nudge_timer(std::chrono::milliseconds delta);
  void select_preset(int index);
  void edit_last_note();
  void export_laps(bool markdown);
  void cycle_theme(int step);
  void cycle_font(int step);
  void play_alarm(bool repeating);
  void silence_alarm();
  void show_about();

  [[nodiscard]] bool typing_in_field() const;
  [[nodiscard]] QString overlap_problem() const;

  bool persist_settings_{true};
  bool play_sounds_{true};
  Mode mode_{Mode::Stopwatch};
  sw::core::Stopwatch stopwatch_{};
  sw::core::CountdownTimer timer_{std::chrono::minutes{5}};
  bool rebuilding_laps_{false};
  bool alarm_latched_{false};
  int alarm_beeps_left_{0};
  QProcess *alarm_process_{nullptr};

  sw::gui::DigitalBoardWidget *board_{nullptr};
  QButtonGroup *mode_group_{nullptr};
  QPushButton *stopwatch_mode_button_{nullptr};
  QPushButton *timer_mode_button_{nullptr};
  QComboBox *theme_combo_{nullptr};
  QComboBox *font_combo_{nullptr};
  QPushButton *run_button_{nullptr};
  QPushButton *reset_button_{nullptr};
  QPushButton *lap_button_{nullptr};
  QPushButton *repeat_button_{nullptr};
  QPushButton *plus_button_{nullptr};
  QPushButton *minus_button_{nullptr};
  QStackedWidget *pages_{nullptr};
  QLabel *stats_label_{nullptr};
  QTableWidget *laps_table_{nullptr};
  QLineEdit *duration_edit_{nullptr};
  QLabel *cycles_label_{nullptr};
  QButtonGroup *preset_group_{nullptr};
  QLabel *status_label_{nullptr};
  QTimer *ticker_{nullptr};
};
