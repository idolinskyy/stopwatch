// Copyright (C) 2026 Ihor Dolinskyi
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"

#include <core/time_utils.hpp>

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryFile>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

[[nodiscard]] QString clock_text(std::chrono::milliseconds duration, bool millis) {
  return QString::fromStdString(sw::core::format_digital(duration, true, millis));
}

struct ClockParts {
  QString prefix;
  QString main;
  QString millis;
};

[[nodiscard]] ClockParts split_clock(std::chrono::milliseconds duration, QString prefix = {}) {
  QString body = clock_text(duration, true);
  if (body.startsWith(QLatin1Char('-'))) {
    prefix = QStringLiteral("-");
    body.remove(0, 1);
  }
  const qsizetype dot = body.lastIndexOf(QLatin1Char('.'));
  if (dot < 0) {
    return {prefix, body, {}};
  }
  return {prefix, body.left(dot), body.mid(dot)};
}

void clear_checks(QButtonGroup *group) {
  if (group == nullptr) {
    return;
  }
  group->setExclusive(false);
  const QList<QAbstractButton *> buttons = group->buttons();
  for (QAbstractButton *button : buttons) {
    button->setChecked(false);
  }
  group->setExclusive(true);
}

[[nodiscard]] QString window_stylesheet() {
  return QStringLiteral(R"(
QWidget#MainWindowRoot { background: #121418; color: #e8eaed; }
QLabel#titleLabel { font-size: 20px; font-weight: 700; color: #f4f6f8; }
QLabel#subtitleLabel { color: #9aa3b2; font-size: 12px; }
QLabel#hintLabel, QLabel#statusLabel { color: #9aa3b2; }
QLabel#sectionLabel { color: #d7dde6; font-weight: 600; }
QPushButton {
  background: #2a303a;
  color: #e8eaed;
  border: 1px solid #3d4450;
  border-radius: 8px;
  padding: 8px 14px;
  min-height: 22px;
}
QPushButton:hover { background: #343b48; }
QPushButton:pressed { background: #232830; }
QPushButton:disabled { color: #6d7582; background: #1c2026; }
QPushButton#primaryButton {
  background: #1f7a52;
  border-color: #2ea871;
  color: #f4fff8;
  font-weight: 700;
}
QPushButton#primaryButton:hover { background: #249466; }
QPushButton#primaryButton:pressed { background: #186644; }
QPushButton#modeStopwatch, QPushButton#modeTimer {
  min-width: 132px;
  font-weight: 600;
}
QPushButton#modeStopwatch {
  border-top-right-radius: 0;
  border-bottom-right-radius: 0;
}
QPushButton#modeTimer {
  border-top-left-radius: 0;
  border-bottom-left-radius: 0;
}
QPushButton#modeStopwatch:checked, QPushButton#modeTimer:checked {
  background: #17382c;
  color: #b6ffd8;
  border: 1px solid #2f8f68;
}
QPushButton:focus, QLineEdit:focus, QComboBox:focus {
  border-color: #3dff9a;
}
QLineEdit, QComboBox {
  background: #1b1f26;
  color: #e8eaed;
  border: 1px solid #3d4450;
  border-radius: 8px;
  padding: 7px 10px;
  min-height: 20px;
}
QComboBox {
  min-width: 168px;
  padding: 7px 30px 7px 12px;
}
QComboBox::drop-down {
  subcontrol-origin: padding;
  subcontrol-position: center right;
  width: 22px;
  border: none;
  background: transparent;
  border-top-right-radius: 8px;
  border-bottom-right-radius: 8px;
}
QComboBox::down-arrow {
  image: url(:/sw/chevron-down.png);
  width: 12px;
  height: 8px;
}
QComboBox QAbstractItemView {
  background: #1b1f26;
  color: #e8eaed;
  border: 1px solid #3d4450;
  outline: 0;
  selection-background-color: #1f7a52;
  selection-color: #f4fff8;
}
QTableWidget {
  background: #171a1f;
  alternate-background-color: #1d2128;
  color: #e8eaed;
  gridline-color: #2a303a;
  border: 1px solid #2c313a;
  border-radius: 10px;
  selection-background-color: #1d4d3a;
  selection-color: #f4fff8;
}
QHeaderView::section {
  background: #22262e;
  color: #b7c0cc;
  border: none;
  padding: 7px;
  font-weight: 600;
}
QMenuBar {
  background: #121418;
  color: #e8eaed;
  padding: 2px 10px;
}
QMenuBar::item { padding: 4px 10px; background: transparent; }
QMenuBar::item:selected { background: #1f7a52; border-radius: 4px; }
QMenu {
  background: #1b1f26;
  color: #e8eaed;
  border: 1px solid #3d4450;
}
QMenu::item { padding: 6px 18px; }
QMenu::item:selected { background: #1f7a52; }
)");
}

} // namespace

MainWindow::MainWindow(bool persist_settings, QWidget *parent)
    : QWidget(parent), persist_settings_(persist_settings) {
  setObjectName(QStringLiteral("MainWindowRoot"));
  setWindowTitle(QStringLiteral("StopWatch"));
  setMinimumSize(880, 680);
  resize(1040, 760);
  setStyleSheet(window_stylesheet());
  build_ui();
  load_settings();
  connect_signals();
  sync_mode();
  refresh_laps();
  refresh_timer_labels();
  refresh_board();

  qApp->installEventFilter(this);

  ticker_ = new QTimer(this);
  ticker_->setTimerType(Qt::PreciseTimer);
  ticker_->setInterval(30);
  connect(ticker_, &QTimer::timeout, this, [this] { on_tick(); });
  ticker_->start();
}

void MainWindow::build_ui() {
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  auto *menu_bar = new QMenuBar(this);
  auto *help_menu = menu_bar->addMenu(QStringLiteral("Довідка"));
  help_menu->addAction(QStringLiteral("Про StopWatch"), this, [this] { show_about(); });
  help_menu->addAction(QStringLiteral("Про Qt…"), this, [this] { QMessageBox::aboutQt(this); });
  outer->addWidget(menu_bar);

  auto *root = new QVBoxLayout;
  root->setContentsMargins(18, 12, 18, 12);
  root->setSpacing(12);
  outer->addLayout(root);

  auto *header = new QHBoxLayout;
  auto *titles = new QVBoxLayout;
  titles->setSpacing(0);
  auto *title = new QLabel(QStringLiteral("StopWatch"), this);
  title->setObjectName(QStringLiteral("titleLabel"));
  auto *subtitle = new QLabel(QStringLiteral("Секундомір і таймер"), this);
  subtitle->setObjectName(QStringLiteral("subtitleLabel"));
  titles->addWidget(title);
  titles->addWidget(subtitle);

  stopwatch_mode_button_ = new QPushButton(QStringLiteral("Секундомір"), this);
  timer_mode_button_ = new QPushButton(QStringLiteral("Таймер"), this);
  stopwatch_mode_button_->setObjectName(QStringLiteral("modeStopwatch"));
  timer_mode_button_->setObjectName(QStringLiteral("modeTimer"));
  stopwatch_mode_button_->setCheckable(true);
  timer_mode_button_->setCheckable(true);
  stopwatch_mode_button_->setChecked(true);
  stopwatch_mode_button_->setCursor(Qt::PointingHandCursor);
  timer_mode_button_->setCursor(Qt::PointingHandCursor);
  mode_group_ = new QButtonGroup(this);
  mode_group_->setExclusive(true);
  mode_group_->addButton(stopwatch_mode_button_, 0);
  mode_group_->addButton(timer_mode_button_, 1);
  auto *mode_row = new QHBoxLayout;
  mode_row->setSpacing(0);
  mode_row->addWidget(stopwatch_mode_button_);
  mode_row->addWidget(timer_mode_button_);

  theme_combo_ = new QComboBox(this);
  theme_combo_->addItem(QStringLiteral("Смарагдовий"));
  theme_combo_->addItem(QStringLiteral("Бурштиновий"));
  theme_combo_->addItem(QStringLiteral("Блакитний"));
  theme_combo_->addItem(QStringLiteral("Червоний"));
  theme_combo_->setToolTip(QStringLiteral("Колір табло, клавіша C"));

  font_combo_ = new QComboBox(this);
  font_combo_->addItem(QStringLiteral("Векторний"));
  font_combo_->addItem(QStringLiteral("ElectroBoard 7"));
  font_combo_->addItem(QStringLiteral("Моноширинний"));
  font_combo_->setToolTip(QStringLiteral("Шрифт цифр, клавіша F"));

  header->addLayout(titles);
  header->addStretch(1);
  header->addLayout(mode_row);
  header->addSpacing(8);
  header->addWidget(theme_combo_);
  header->addWidget(font_combo_);
  root->addLayout(header);

  board_ = new sw::gui::DigitalBoardWidget(this);
  board_->setMinimumHeight(200);
  root->addWidget(board_, 5);

  auto *control_bar = new QWidget(this);
  control_bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto *controls = new QHBoxLayout(control_bar);
  controls->setContentsMargins(0, 0, 0, 0);
  controls->setSpacing(8);
  run_button_ = new QPushButton(QStringLiteral("Старт"), this);
  run_button_->setObjectName(QStringLiteral("primaryButton"));
  reset_button_ = new QPushButton(QStringLiteral("Скинути"), this);
  lap_button_ = new QPushButton(QStringLiteral("Коло"), this);
  repeat_button_ = new QPushButton(QStringLiteral("Автоповтор"), this);
  repeat_button_->setCheckable(true);
  plus_button_ = new QPushButton(QStringLiteral("+30 с"), this);
  minus_button_ = new QPushButton(QStringLiteral("−30 с"), this);
  for (QPushButton *button :
       {run_button_, reset_button_, lap_button_, repeat_button_, plus_button_, minus_button_}) {
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(40);
    controls->addWidget(button);
  }
  controls->addStretch(1);
  root->addWidget(control_bar);

  pages_ = new QStackedWidget(this);

  auto *laps_page = new QWidget(pages_);
  auto *laps_layout = new QVBoxLayout(laps_page);
  laps_layout->setContentsMargins(0, 0, 0, 0);
  laps_layout->setSpacing(8);
  stats_label_ = new QLabel(laps_page);
  stats_label_->setObjectName(QStringLiteral("sectionLabel"));
  stats_label_->setWordWrap(true);
  laps_table_ = new QTableWidget(0, 5, laps_page);
  laps_table_->setHorizontalHeaderLabels({QStringLiteral("№"), QStringLiteral("Час кола"),
                                          QStringLiteral("Загальний"), QStringLiteral("Δ"),
                                          QStringLiteral("Примітка")});
  laps_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  laps_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  laps_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  laps_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  laps_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
  laps_table_->verticalHeader()->setVisible(false);
  laps_table_->setShowGrid(false);
  laps_table_->setAlternatingRowColors(true);
  laps_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  laps_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  laps_table_->setEditTriggers(QAbstractItemView::DoubleClicked |
                               QAbstractItemView::EditKeyPressed);
  laps_table_->setMinimumHeight(120);
  auto *lap_actions = new QHBoxLayout;
  auto *note_hint = new QLabel(QStringLiteral("Подвійне клацання змінює примітку."), laps_page);
  note_hint->setObjectName(QStringLiteral("hintLabel"));
  auto *csv_button = new QPushButton(QStringLiteral("Експорт CSV"), laps_page);
  auto *md_button = new QPushButton(QStringLiteral("Експорт Markdown"), laps_page);
  csv_button->setCursor(Qt::PointingHandCursor);
  md_button->setCursor(Qt::PointingHandCursor);
  lap_actions->addWidget(note_hint);
  lap_actions->addStretch(1);
  lap_actions->addWidget(csv_button);
  lap_actions->addWidget(md_button);
  laps_layout->addWidget(stats_label_);
  laps_layout->addWidget(laps_table_, 1);
  laps_layout->addLayout(lap_actions);
  pages_->addWidget(laps_page);

  auto *timer_page = new QWidget(pages_);
  auto *timer_layout = new QVBoxLayout(timer_page);
  timer_layout->setContentsMargins(0, 0, 0, 0);
  timer_layout->setSpacing(8);
  auto *duration_row = new QHBoxLayout;
  duration_edit_ = new QLineEdit(timer_page);
  duration_edit_->setPlaceholderText(QStringLiteral("5m30s, 10:00, 90s, 2хв 10с"));
  duration_edit_->setClearButtonEnabled(true);
  auto *apply_button = new QPushButton(QStringLiteral("Застосувати"), timer_page);
  apply_button->setObjectName(QStringLiteral("primaryButton"));
  apply_button->setCursor(Qt::PointingHandCursor);
  duration_row->addWidget(duration_edit_, 1);
  duration_row->addWidget(apply_button);
  auto *format_hint =
    new QLabel(QStringLiteral("25m, 1h 15m 30s, 90s, 10:00, 01:02:03.5, 2хв 10с"), timer_page);
  format_hint->setObjectName(QStringLiteral("hintLabel"));
  format_hint->setWordWrap(true);

  auto *preset_grid = new QGridLayout;
  preset_grid->setSpacing(8);
  preset_group_ = new QButtonGroup(this);
  preset_group_->setExclusive(true);
  const auto presets = sw::core::CountdownTimer::default_presets();
  for (std::size_t i = 0; i < presets.size(); ++i) {
    auto *button = new QPushButton(
      QString::fromUtf8(presets[i].name.data(), static_cast<qsizetype>(presets[i].name.size())),
      timer_page);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(clock_text(presets[i].duration, false));
    preset_group_->addButton(button, static_cast<int>(i));
    preset_grid->addWidget(button, static_cast<int>(i) / 3, static_cast<int>(i) % 3);
  }
  cycles_label_ = new QLabel(timer_page);
  cycles_label_->setObjectName(QStringLiteral("sectionLabel"));
  timer_layout->addLayout(duration_row);
  timer_layout->addWidget(format_hint);
  timer_layout->addLayout(preset_grid);
  timer_layout->addWidget(cycles_label_);
  timer_layout->addStretch(1);
  pages_->addWidget(timer_page);
  root->addWidget(pages_, 3);

  status_label_ = new QLabel(QStringLiteral("Готово."), this);
  status_label_->setObjectName(QStringLiteral("sectionLabel"));
  status_label_->setWordWrap(true);
  auto *hint =
    new QLabel(QStringLiteral("Пробіл — старт/пауза    R — скинути    L — коло або автоповтор    "
                              "1/2 — режим    C — тема    F — шрифт    +/− — 30 с    "
                              "N — примітка    E — CSV    S — інтервал    Q — вихід"),
               this);
  hint->setObjectName(QStringLiteral("hintLabel"));
  hint->setWordWrap(true);
  root->addWidget(status_label_);
  root->addWidget(hint);

  ensurePolished();
  setMinimumSize(layout()->totalMinimumSize().expandedTo(QSize(960, 0)));

  connect(csv_button, &QPushButton::clicked, this, [this] { export_laps(false); });
  connect(md_button, &QPushButton::clicked, this, [this] { export_laps(true); });
  connect(apply_button, &QPushButton::clicked, this, [this] { apply_duration(); });
}

void MainWindow::connect_signals() {
  connect(mode_group_, &QButtonGroup::idClicked, this, [this](int id) {
    mode_ = id == 1 ? Mode::Timer : Mode::Stopwatch;
    sync_mode();
    set_status(mode_ == Mode::Stopwatch ? QStringLiteral("Секундомір.")
                                        : QStringLiteral("Таймер."));
  });
  connect(theme_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (index >= 0) {
      board_->set_theme(static_cast<sw::gui::BoardTheme>(index));
    }
  });
  connect(font_combo_, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (index >= 0) {
      board_->set_font_style(static_cast<sw::gui::BoardFontStyle>(index));
      if (index == static_cast<int>(sw::gui::BoardFontStyle::ElectroFontTTF) &&
          !board_->has_electro_font()) {
        font_combo_->setCurrentIndex(0);
        set_status(QStringLiteral("Шрифт ElectroBoard 7 не вдалося завантажити."));
      }
    }
  });
  connect(run_button_, &QPushButton::clicked, this, [this] { toggle_run(); });
  connect(reset_button_, &QPushButton::clicked, this, [this] { reset_active(); });
  connect(lap_button_, &QPushButton::clicked, this, [this] { lap_or_repeat(); });
  connect(repeat_button_, &QPushButton::toggled, this, [this](bool enabled) {
    timer_.set_auto_repeat(enabled);
    refresh_timer_labels();
    set_status(enabled ? QStringLiteral("Автоповтор увімкнено.")
                       : QStringLiteral("Автоповтор вимкнено."));
  });
  connect(plus_button_, &QPushButton::clicked, this,
          [this] { nudge_timer(std::chrono::seconds{30}); });
  connect(minus_button_, &QPushButton::clicked, this,
          [this] { nudge_timer(std::chrono::seconds{-30}); });
  connect(duration_edit_, &QLineEdit::returnPressed, this, [this] { apply_duration(); });
  connect(preset_group_, &QButtonGroup::idClicked, this, [this](int id) { select_preset(id); });
  connect(laps_table_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
    if (rebuilding_laps_ || item == nullptr || item->column() != 4) {
      return;
    }
    const QTableWidgetItem *number_item = laps_table_->item(item->row(), 0);
    if (number_item == nullptr) {
      return;
    }
    bool ok = false;
    const qulonglong number = number_item->text().toULongLong(&ok);
    if (!ok || number == 0) {
      return;
    }
    (void)stopwatch_.set_lap_note(static_cast<std::size_t>(number),
                                  item->text().trimmed().toStdString());
  });

  const auto bind = [this](const QKeySequence &sequence, const auto &handler) {
    auto *shortcut = new QShortcut(sequence, this);
    shortcut->setContext(Qt::WindowShortcut);
    connect(shortcut, &QShortcut::activated, this, handler);
  };
  bind(Qt::Key_R, [this] {
    if (!typing_in_field()) {
      reset_active();
    }
  });
  bind(Qt::Key_L, [this] {
    if (!typing_in_field()) {
      lap_or_repeat();
    }
  });
  bind(Qt::Key_1, [this] {
    if (!typing_in_field()) {
      mode_ = Mode::Stopwatch;
      sync_mode();
      set_status(QStringLiteral("Секундомір."));
    }
  });
  bind(Qt::Key_2, [this] {
    if (!typing_in_field()) {
      mode_ = Mode::Timer;
      sync_mode();
      set_status(QStringLiteral("Таймер."));
    }
  });
  bind(Qt::Key_C, [this] {
    if (!typing_in_field()) {
      cycle_theme(1);
    }
  });
  bind(Qt::Key_F, [this] {
    if (!typing_in_field()) {
      cycle_font(1);
    }
  });
  bind(Qt::Key_Plus, [this] {
    if (!typing_in_field()) {
      nudge_timer(std::chrono::seconds{30});
    }
  });
  bind(Qt::Key_Equal, [this] {
    if (!typing_in_field()) {
      nudge_timer(std::chrono::seconds{30});
    }
  });
  bind(Qt::Key_Minus, [this] {
    if (!typing_in_field()) {
      nudge_timer(std::chrono::seconds{-30});
    }
  });
  bind(Qt::Key_N, [this] {
    if (!typing_in_field()) {
      edit_last_note();
    }
  });
  bind(Qt::Key_E, [this] {
    if (!typing_in_field()) {
      export_laps(false);
    }
  });
  bind(Qt::Key_S, [this] {
    if (!typing_in_field()) {
      mode_ = Mode::Timer;
      sync_mode();
      duration_edit_->setFocus();
      duration_edit_->selectAll();
    }
  });
  bind(Qt::Key_Q, [this] {
    if (!typing_in_field()) {
      close();
    }
  });
  bind(QKeySequence::Quit, [this] { close(); });
  bind(Qt::Key_Escape, [this] {
    if (QWidget *focused = QApplication::focusWidget(); focused != nullptr && focused != this) {
      focused->clearFocus();
      setFocus();
    }
  });
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    const auto *key = static_cast<const QKeyEvent *>(event);
    if (key->key() == Qt::Key_Space && !key->isAutoRepeat() && !typing_in_field() &&
        qobject_cast<QAbstractButton *>(QApplication::focusWidget()) == nullptr) {
      toggle_run();
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void MainWindow::load_settings() {
  theme_combo_->setCurrentIndex(0);
  font_combo_->setCurrentIndex(1);
  duration_edit_->setText(clock_text(timer_.target(), false));
  board_->set_theme(sw::gui::BoardTheme::Emerald);
  board_->set_font_style(sw::gui::BoardFontStyle::ElectroFontTTF);
  if (!board_->has_electro_font()) {
    font_combo_->setCurrentIndex(0);
    board_->set_font_style(sw::gui::BoardFontStyle::Vector7Segment);
  }
  if (!persist_settings_) {
    return;
  }

  QSettings settings(QStringLiteral("StopWatch"), QStringLiteral("StopWatch"));
  const int theme = std::clamp(settings.value(QStringLiteral("theme"), 0).toInt(), 0, 3);
  const int font = std::clamp(settings.value(QStringLiteral("font"), 1).toInt(), 0, 2);
  theme_combo_->setCurrentIndex(theme);
  font_combo_->setCurrentIndex(font);
  board_->set_theme(static_cast<sw::gui::BoardTheme>(theme));
  board_->set_font_style(static_cast<sw::gui::BoardFontStyle>(font));
  if (font == static_cast<int>(sw::gui::BoardFontStyle::ElectroFontTTF) &&
      !board_->has_electro_font()) {
    font_combo_->setCurrentIndex(0);
    board_->set_font_style(sw::gui::BoardFontStyle::Vector7Segment);
  }

  const qint64 stored_ms = settings.value(QStringLiteral("timer_ms"), 5LL * 60 * 1000).toLongLong();
  if (stored_ms > 0 && stored_ms <= 100LL * 60 * 60 * 1000) {
    timer_.set_target(std::chrono::milliseconds{stored_ms});
    duration_edit_->setText(clock_text(timer_.target(), false));
  }
  const QByteArray geometry = settings.value(QStringLiteral("geometry")).toByteArray();
  if (!geometry.isEmpty()) {
    restoreGeometry(geometry);
  }
  if (width() < minimumWidth() || height() < minimumHeight()) {
    resize(std::max(width(), minimumWidth()), std::max(height(), minimumHeight()));
  }
}

void MainWindow::save_settings() const {
  if (!persist_settings_) {
    return;
  }
  QSettings settings(QStringLiteral("StopWatch"), QStringLiteral("StopWatch"));
  settings.setValue(QStringLiteral("theme"), theme_combo_->currentIndex());
  settings.setValue(QStringLiteral("font"), font_combo_->currentIndex());
  settings.setValue(QStringLiteral("timer_ms"), static_cast<qint64>(timer_.target().count()));
  settings.setValue(QStringLiteral("geometry"), saveGeometry());
}

void MainWindow::closeEvent(QCloseEvent *event) {
  save_settings();
  QWidget::closeEvent(event);
}

void MainWindow::on_tick() {
  const bool finished_now = timer_.update();
  if (finished_now && timer_.auto_repeat()) {
    play_alarm(false);
    set_status(QStringLiteral("Цикл %1.").arg(timer_.completed_cycles()));
    refresh_timer_labels();
  } else if (finished_now) {
    alarm_latched_ = true;
    alarm_beeps_left_ = 4;
    play_alarm(true);
    set_status(QStringLiteral("Час вийшов."));
    refresh_timer_labels();
  } else if (!timer_.is_finished() && alarm_latched_) {
    silence_alarm();
  }

  refresh_board();
  if (mode_ == Mode::Timer) {
    refresh_timer_labels();
  }
}

void MainWindow::refresh_board() {
  const bool timer_mode = mode_ == Mode::Timer;
  QString status;
  bool active = false;
  bool alert = false;
  ClockParts parts;
  if (!timer_mode) {
    board_->set_mode_title(QStringLiteral("СЕКУНДОМІР"));
    parts = split_clock(stopwatch_.elapsed());
    if (stopwatch_.is_running()) {
      status = QStringLiteral("АКТИВНИЙ");
      active = true;
    } else if (stopwatch_.state() == sw::core::StopwatchState::Paused) {
      status = QStringLiteral("ПАУЗА");
    } else {
      status = QStringLiteral("ГОТОВИЙ");
    }
    board_->set_sub_left(
      QStringLiteral("ПОТОЧНЕ КОЛО   %1").arg(clock_text(stopwatch_.current_lap_elapsed(), true)));
    if (const auto stats = stopwatch_.statistics(); stats.has_value()) {
      board_->set_sub_right(
        QStringLiteral("КІЛ %1    КРАЩЕ #%2").arg(stats->count).arg(stats->fastest_lap_number));
    } else {
      board_->set_sub_right(QStringLiteral("КІЛ 0"));
    }
    board_->set_progress(0.0, false);
    run_button_->setText(stopwatch_.is_running() ? QStringLiteral("Пауза")
                                                 : QStringLiteral("Старт"));
  } else {
    board_->set_mode_title(QStringLiteral("ТАЙМЕР"));
    alert = timer_.is_finished();
    active = timer_.is_running() || timer_.is_overtime_running();
    if (timer_.is_overtime_running()) {
      status = QStringLiteral("ПЕРЕБІГ");
      parts = split_clock(timer_.overtime(), QStringLiteral("+"));
    } else if (timer_.is_finished()) {
      status = QStringLiteral("ЧАС ВИЙШОВ");
      parts = split_clock(timer_.overtime(), QStringLiteral("+"));
    } else if (timer_.is_running()) {
      status = QStringLiteral("ВІДЛІК");
      parts = split_clock(timer_.remaining());
    } else if (timer_.state() == sw::core::TimerState::Paused) {
      status = QStringLiteral("ПАУЗА");
      parts = split_clock(timer_.remaining());
    } else {
      status = QStringLiteral("ГОТОВИЙ");
      parts = split_clock(timer_.remaining());
    }
    board_->set_sub_left(
      QStringLiteral("ЦІЛЬ   %1").arg(clock_text(timer_.active_target(), false)));
    board_->set_sub_right(QStringLiteral("ЦИКЛІВ %1    %2%")
                            .arg(timer_.completed_cycles())
                            .arg(timer_.progress() * 100.0, 0, 'f', 0));
    board_->set_progress(timer_.progress(), true);
    run_button_->setText(active ? QStringLiteral("Пауза") : QStringLiteral("Старт"));
  }
  board_->set_status_text(status, active, alert);
  board_->set_main_time(parts.main, parts.millis, parts.prefix);

  const QString next_title = timer_mode
                               ? QStringLiteral("Таймер — %1%2").arg(parts.prefix, parts.main)
                               : QStringLiteral("Секундомір — %1").arg(parts.main);
  if (windowTitle() != next_title) {
    setWindowTitle(next_title);
  }
}

void MainWindow::refresh_laps() {
  rebuilding_laps_ = true;
  const QSignalBlocker blocker(laps_table_);
  const auto &laps = stopwatch_.laps();
  const auto stats = stopwatch_.statistics();
  const bool colorize = stats.has_value() && stats->count >= 2U;
  laps_table_->setRowCount(static_cast<int>(laps.size()));
  int row = 0;
  for (auto it = laps.rbegin(); it != laps.rend(); ++it, ++row) {
    const QStringList cells = {
      QString::number(it->number),
      QString::fromStdString(sw::core::format_digital(it->split, false, true)),
      QString::fromStdString(sw::core::format_digital(it->total, false, true)),
      QString::fromStdString(sw::core::format_delta(it->delta)),
      QString::fromStdString(it->note),
    };
    QColor tint;
    if (colorize && it->number == stats->fastest_lap_number) {
      tint = QColor(QStringLiteral("#9dffc8"));
    } else if (colorize && it->number == stats->slowest_lap_number) {
      tint = QColor(QStringLiteral("#ffb3c0"));
    }
    for (int column = 0; column < cells.size(); ++column) {
      auto *item = new QTableWidgetItem(cells.at(column));
      item->setTextAlignment(column == 4 ? (Qt::AlignLeft | Qt::AlignVCenter) : Qt::AlignCenter);
      Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
      if (column == 4) {
        flags |= Qt::ItemIsEditable;
      }
      item->setFlags(flags);
      if (tint.isValid()) {
        item->setForeground(tint);
      }
      laps_table_->setItem(row, column, item);
    }
  }

  if (stats.has_value()) {
    stats_label_->setText(
      QStringLiteral("Кіл: %1    найкраще #%2 (%3)    найгірше #%4 (%5)    середнє %6")
        .arg(stats->count)
        .arg(stats->fastest_lap_number)
        .arg(QString::fromStdString(sw::core::format_digital(stats->fastest_split, false, true)))
        .arg(stats->slowest_lap_number)
        .arg(QString::fromStdString(sw::core::format_digital(stats->slowest_split, false, true)))
        .arg(QString::fromStdString(sw::core::format_digital(stats->average_split, false, true))));
  } else {
    stats_label_->setText(QStringLiteral("Кіл ще немає."));
  }
  rebuilding_laps_ = false;
}

void MainWindow::refresh_timer_labels() {
  const QSignalBlocker blocker(repeat_button_);
  repeat_button_->setChecked(timer_.auto_repeat());
  cycles_label_->setText(
    QStringLiteral("Циклів: %1, автоповтор %2")
      .arg(timer_.completed_cycles())
      .arg(timer_.auto_repeat() ? QStringLiteral("увімкнено") : QStringLiteral("вимкнено")));
}

void MainWindow::sync_mode() {
  {
    const QSignalBlocker block_stopwatch(stopwatch_mode_button_);
    const QSignalBlocker block_timer(timer_mode_button_);
    stopwatch_mode_button_->setChecked(mode_ == Mode::Stopwatch);
    timer_mode_button_->setChecked(mode_ == Mode::Timer);
  }
  const bool timer_mode = mode_ == Mode::Timer;
  lap_button_->setVisible(!timer_mode);
  repeat_button_->setVisible(timer_mode);
  plus_button_->setVisible(timer_mode);
  minus_button_->setVisible(timer_mode);
  pages_->setCurrentIndex(timer_mode ? 1 : 0);
  refresh_board();
}

void MainWindow::set_status(const QString &text) { status_label_->setText(text); }

void MainWindow::show_about() {
  QMessageBox box(this);
  box.setWindowTitle(QStringLiteral("Про StopWatch"));
  box.setTextFormat(Qt::RichText);
  box.setTextInteractionFlags(Qt::TextBrowserInteraction);
  box.setText(
    QStringLiteral("<p><b>StopWatch %1</b></p>"
                   "<p>Секундомір і таймер.</p>"
                   "<p>Код і шрифт ElectroBoard 7 — "
                   "<a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GNU GPL версії 3</a> "
                   "або новішої.</p>"
                   "<p>Вікно зібране з Qt %2, "
                   "<a href=\"https://www.gnu.org/licenses/lgpl-3.0.html\">GNU LGPL 3</a>. "
                   "Бібліотеки Qt підключаються динамічно і в цей репозиторій не входять. "
                   "Код Qt: <a href=\"https://download.qt.io\">download.qt.io</a>. "
                   "<a href=\"https://doc.qt.io/qt-6/licenses-used-in-qt.html\">Ліцензії "
                   "компонентів Qt</a>.</p>")
      .arg(QStringLiteral(STOPWATCH_VERSION), QLatin1String(qVersion())));
  if (auto *label = box.findChild<QLabel *>(QStringLiteral("qt_msgbox_label"))) {
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    label->setOpenExternalLinks(true);
  }
  auto *about_qt = box.addButton(QStringLiteral("Про Qt…"), QMessageBox::ActionRole);
  box.addButton(QMessageBox::Close);
  box.exec();
  if (box.clickedButton() == about_qt) {
    QMessageBox::aboutQt(this);
  }
}

void MainWindow::toggle_run() {
  if (mode_ == Mode::Stopwatch) {
    const bool was_running = stopwatch_.is_running();
    stopwatch_.toggle();
    set_status(was_running ? QStringLiteral("Секундомір на паузі.")
                           : QStringLiteral("Секундомір запущено."));
  } else {
    if (timer_.is_finished()) {
      silence_alarm();
    }
    const bool was_live = timer_.is_running() || timer_.is_overtime_running();
    timer_.toggle();
    if (was_live) {
      set_status(timer_.is_finished() ? QStringLiteral("Перебіг зупинено.")
                                      : QStringLiteral("Таймер на паузі."));
    } else {
      set_status(QStringLiteral("Відлік запущено."));
    }
    refresh_timer_labels();
  }
  refresh_board();
}

void MainWindow::reset_active() {
  if (mode_ == Mode::Stopwatch) {
    stopwatch_.reset();
    refresh_laps();
    set_status(QStringLiteral("Секундомір скинуто."));
  } else {
    timer_.reset();
    silence_alarm();
    refresh_timer_labels();
    set_status(QStringLiteral("Таймер скинуто до %1.").arg(clock_text(timer_.target(), false)));
  }
  refresh_board();
}

void MainWindow::lap_or_repeat() {
  if (mode_ == Mode::Timer) {
    repeat_button_->toggle();
    return;
  }
  if (const auto record = stopwatch_.lap(); record.has_value()) {
    refresh_laps();
    set_status(
      QStringLiteral("Коло #%1: %2")
        .arg(record->number)
        .arg(QString::fromStdString(sw::core::format_digital(record->split, false, true))));
  } else {
    set_status(QStringLiteral("Секундомір не запущено."));
  }
  refresh_board();
}

void MainWindow::apply_duration() {
  const auto parsed = sw::core::parse_duration(duration_edit_->text().trimmed().toStdString());
  if (!parsed.has_value() || parsed->count() <= 0) {
    set_status(QStringLiteral("Не розпізнано час."));
    return;
  }
  timer_.set_target(*parsed);
  clear_checks(preset_group_);
  silence_alarm();
  duration_edit_->setText(clock_text(timer_.target(), false));
  mode_ = Mode::Timer;
  sync_mode();
  refresh_timer_labels();
  set_status(QStringLiteral("Таймер: %1.").arg(clock_text(timer_.target(), false)));
}

void MainWindow::nudge_timer(std::chrono::milliseconds delta) {
  mode_ = Mode::Timer;
  timer_.add_time(delta);
  clear_checks(preset_group_);
  if (!timer_.is_finished()) {
    silence_alarm();
  }
  if (timer_.state() == sw::core::TimerState::Idle) {
    duration_edit_->setText(clock_text(timer_.target(), false));
  }
  sync_mode();
  refresh_timer_labels();
  const QString sign = delta.count() >= 0 ? QStringLiteral("+") : QStringLiteral("−");
  const auto magnitude = delta.count() >= 0 ? delta : -delta;
  set_status(QStringLiteral("Таймер %1%2.")
               .arg(sign, QString::fromStdString(sw::core::format_compact(magnitude))));
}

void MainWindow::select_preset(int index) {
  const auto presets = sw::core::CountdownTimer::default_presets();
  if (index < 0 || static_cast<std::size_t>(index) >= presets.size()) {
    return;
  }
  const auto &preset = presets[static_cast<std::size_t>(index)];
  timer_.set_target(preset.duration);
  silence_alarm();
  duration_edit_->setText(clock_text(timer_.target(), false));
  mode_ = Mode::Timer;
  sync_mode();
  refresh_timer_labels();
  set_status(QStringLiteral("%1.").arg(
    QString::fromUtf8(preset.name.data(), static_cast<qsizetype>(preset.name.size()))));
}

void MainWindow::edit_last_note() {
  if (stopwatch_.laps().empty()) {
    set_status(QStringLiteral("Немає кіл."));
    return;
  }
  mode_ = Mode::Stopwatch;
  sync_mode();
  laps_table_->setFocus();
  laps_table_->setCurrentCell(0, 4);
  if (QTableWidgetItem *item = laps_table_->item(0, 4); item != nullptr) {
    laps_table_->editItem(item);
  }
}

void MainWindow::export_laps(bool markdown) {
  if (stopwatch_.laps().empty()) {
    set_status(QStringLiteral("Немає кіл для експорту."));
    return;
  }
  const QString suggestion =
    markdown ? QStringLiteral("stopwatch_laps.md") : QStringLiteral("stopwatch_laps.csv");
  const QString filter =
    markdown ? QStringLiteral("Markdown (*.md)") : QStringLiteral("CSV (*.csv)");
  const QString path =
    QFileDialog::getSaveFileName(this, QStringLiteral("Експорт кіл"), suggestion, filter);
  if (path.isEmpty()) {
    return;
  }
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    set_status(QStringLiteral("Не вдалося записати файл."));
    return;
  }
  const std::string payload = markdown ? stopwatch_.export_markdown() : stopwatch_.export_csv();
  if (file.write(payload.data(), static_cast<qsizetype>(payload.size())) < 0) {
    set_status(QStringLiteral("Не вдалося записати файл."));
    return;
  }
  set_status(QStringLiteral("Збережено: %1").arg(path));
}

void MainWindow::cycle_theme(int step) {
  const int count = theme_combo_->count();
  if (count <= 0) {
    return;
  }
  int next = theme_combo_->currentIndex() + step;
  next %= count;
  if (next < 0) {
    next += count;
  }
  theme_combo_->setCurrentIndex(next);
}

void MainWindow::cycle_font(int step) {
  const int count = font_combo_->count();
  if (count <= 0) {
    return;
  }
  int next = font_combo_->currentIndex() + step;
  next %= count;
  if (next < 0) {
    next += count;
  }
  font_combo_->setCurrentIndex(next);
}

bool MainWindow::typing_in_field() const {
  const QWidget *focused = QApplication::focusWidget();
  return qobject_cast<const QLineEdit *>(focused) != nullptr;
}

namespace {

void append_u16(QByteArray &out, unsigned value) {
  out.append(static_cast<char>(static_cast<unsigned char>(value & 0xFFU)));
  out.append(static_cast<char>(static_cast<unsigned char>((value >> 8U) & 0xFFU)));
}

void append_u32(QByteArray &out, unsigned value) {
  append_u16(out, value & 0xFFFFU);
  append_u16(out, (value >> 16U) & 0xFFFFU);
}

[[nodiscard]] QByteArray make_alarm_wav() {
  constexpr int kRate = 22050;
  constexpr int kFrames = kRate * 7 / 10;
  QByteArray pcm(kFrames * 2, '\0');
  auto *samples = reinterpret_cast<qint16 *>(pcm.data());
  for (int i = 0; i < kFrames; ++i) {
    const double time = static_cast<double>(i) / static_cast<double>(kRate);
    const double freq = i < kFrames / 2 ? 880.0 : 1174.0;
    const double attack = std::min(1.0, static_cast<double>(i) / 280.0);
    const double release = std::min(1.0, static_cast<double>(kFrames - i) / 2400.0);
    const double sample = std::sin(6.283185307179586 * freq * time) * attack * release * 0.8;
    const int value = static_cast<int>(std::lround(std::clamp(sample, -1.0, 1.0) * 32767.0));
    samples[i] = static_cast<qint16>(value);
  }

  QByteArray wav;
  wav.reserve(44 + pcm.size());
  wav.append("RIFF", 4);
  append_u32(wav, static_cast<unsigned>(36 + pcm.size()));
  wav.append("WAVEfmt ", 8);
  append_u32(wav, 16U);
  append_u16(wav, 1U);
  append_u16(wav, 1U);
  append_u32(wav, static_cast<unsigned>(kRate));
  append_u32(wav, static_cast<unsigned>(kRate * 2));
  append_u16(wav, 2U);
  append_u16(wav, 16U);
  wav.append("data", 4);
  append_u32(wav, static_cast<unsigned>(pcm.size()));
  wav.append(pcm);
  return wav;
}

} // namespace

void MainWindow::play_alarm(bool repeating) {
  if (!play_sounds_) {
    return;
  }
  if (alarm_process_ == nullptr) {
    alarm_process_ = new QProcess(this);
    connect(alarm_process_, &QProcess::finished, this, [this] {
      if (!alarm_latched_ || alarm_beeps_left_ <= 0 || !timer_.is_finished()) {
        return;
      }
      --alarm_beeps_left_;
      QTimer::singleShot(400, this, [this] {
        if (alarm_latched_ && timer_.is_finished()) {
          play_alarm(true);
        }
      });
    });
    connect(alarm_process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
      if (error == QProcess::FailedToStart) {
        alarm_beeps_left_ = 0;
      }
    });
  }
  if (alarm_process_->state() != QProcess::NotRunning) {
    return;
  }

  const QString alarm_file =
    QStringLiteral("/usr/share/sounds/freedesktop/stereo/alarm-clock-elapsed.oga");
  const QString bell_file = QStringLiteral("/usr/share/sounds/freedesktop/stereo/complete.oga");
  const QString path = repeating && QFile::exists(alarm_file) ? alarm_file : bell_file;
  QString program;
  if (QFile::exists(QStringLiteral("/usr/bin/paplay"))) {
    program = QStringLiteral("paplay");
  } else if (QFile::exists(QStringLiteral("/usr/bin/pw-play"))) {
    program = QStringLiteral("pw-play");
  }
  if (!program.isEmpty() && QFile::exists(path)) {
    alarm_process_->start(program, {path});
    return;
  }

  auto *tone = new QTemporaryFile(this);
  tone->setAutoRemove(true);
  if (!tone->open()) {
    return;
  }
  tone->write(make_alarm_wav());
  tone->flush();
  alarm_process_->start(QStringLiteral("aplay"), {QStringLiteral("-q"), tone->fileName()});
}

void MainWindow::silence_alarm() {
  const bool repeating = alarm_latched_;
  alarm_latched_ = false;
  alarm_beeps_left_ = 0;
  if (repeating && alarm_process_ != nullptr && alarm_process_->state() != QProcess::NotRunning) {
    alarm_process_->kill();
  }
}

QString MainWindow::overlap_problem() const {
  const auto box_of = [this](const QWidget *widget) {
    return QRect(widget->mapTo(this, QPoint(0, 0)), widget->size()).adjusted(1, 1, -1, -1);
  };
  const auto clashes = [&](const QWidget *left, const QWidget *right) {
    if (left == nullptr || right == nullptr || !left->isVisible() || !right->isVisible()) {
      return false;
    }
    if (left->width() < 8 || left->height() < 8 || right->width() < 8 || right->height() < 8) {
      return false;
    }
    return box_of(left).intersects(box_of(right));
  };

  const QList<QWidget *> buttons = {run_button_,    reset_button_, lap_button_,
                                    repeat_button_, plus_button_,  minus_button_};
  for (const QWidget *button : buttons) {
    if (clashes(button, board_)) {
      return QStringLiteral("кнопки наїжджають на табло");
    }
    if (clashes(button, pages_)) {
      return QStringLiteral("кнопки наїжджають на панель під табло");
    }
    if (clashes(button, status_label_)) {
      return QStringLiteral("кнопки наїжджають на рядок стану");
    }
  }
  for (int i = 0; i < buttons.size(); ++i) {
    for (int j = i + 1; j < buttons.size(); ++j) {
      if (clashes(buttons.at(i), buttons.at(j))) {
        return QStringLiteral("кнопки керування наїжджають одна на одну");
      }
    }
  }
  if (clashes(stopwatch_mode_button_, timer_mode_button_) &&
      stopwatch_mode_button_->geometry().right() > timer_mode_button_->geometry().left() + 2) {
    return QStringLiteral("перемикач режимів наїжджає сам на себе");
  }
  if (clashes(timer_mode_button_, theme_combo_) || clashes(theme_combo_, font_combo_)) {
    return QStringLiteral("перемикачі зверху наїжджають один на одного");
  }
  return {};
}

bool MainWindow::self_check(QString *error) {
  const auto fail = [error](const QString &message) {
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };

  if (!board_->has_electro_font()) {
    return fail(QStringLiteral("не вдалося завантажити шрифт ElectroBoard 7"));
  }

  play_sounds_ = false;
  show();
  QApplication::processEvents();
  resize(std::max(width(), 1000), minimumHeight());
  QApplication::processEvents();
  if (board_->height() < 40 || run_button_->height() < 20) {
    return fail(QStringLiteral("вікно не розклалося"));
  }
  if (const QString problem = overlap_problem(); !problem.isEmpty()) {
    return fail(problem);
  }
  mode_ = Mode::Timer;
  sync_mode();
  QApplication::processEvents();
  if (const QString problem = overlap_problem(); !problem.isEmpty()) {
    return fail(problem);
  }
  mode_ = Mode::Stopwatch;
  sync_mode();
  QApplication::processEvents();

  board_->setFixedSize(960, 280);
  const auto bright_pixels = [this](sw::gui::BoardTheme theme, sw::gui::BoardFontStyle style) {
    board_->set_theme(theme);
    board_->set_font_style(style);
    board_->set_mode_title(QStringLiteral("СЕКУНДОМІР"));
    board_->set_status_text(QStringLiteral("АКТИВНИЙ"), true, false);
    board_->set_main_time(QStringLiteral("12:34:56"), QStringLiteral(".789"), {});
    board_->set_sub_left(QStringLiteral("ПОТОЧНЕ КОЛО   00:01:02.003"));
    board_->set_sub_right(QStringLiteral("КІЛ 4"));
    board_->set_progress(0.45, true);
    if (board_->size() != QSize(960, 280)) {
      return -1;
    }
    QImage image(board_->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    QPainter painter(&image);
    board_->render(&painter);
    painter.end();
    const QRect area = image.rect().adjusted(image.width() / 8, image.height() / 5,
                                             -image.width() / 8, -image.height() / 5);
    int hits = 0;
    for (int y = area.top(); y < area.bottom(); y += 2) {
      for (int x = area.left(); x < area.right(); x += 2) {
        const QColor color = image.pixelColor(x, y);
        const int peak = std::max(color.red(), std::max(color.green(), color.blue()));
        if (peak > 170) {
          ++hits;
        }
      }
    }
    return hits;
  };

  if (bright_pixels(sw::gui::BoardTheme::Emerald, sw::gui::BoardFontStyle::Vector7Segment) < 40) {
    return fail(QStringLiteral("векторне табло не малює цифри"));
  }
  if (bright_pixels(sw::gui::BoardTheme::Amber, sw::gui::BoardFontStyle::Vector7Segment) < 40) {
    return fail(QStringLiteral("бурштинова тема не малює цифри"));
  }
  if (bright_pixels(sw::gui::BoardTheme::Cyan, sw::gui::BoardFontStyle::ElectroFontTTF) < 20) {
    return fail(QStringLiteral("шрифт ElectroBoard 7 не малює цифри"));
  }
  if (bright_pixels(sw::gui::BoardTheme::Crimson, sw::gui::BoardFontStyle::MonospaceTabular) < 20) {
    return fail(QStringLiteral("моноширинне табло не малює цифри"));
  }
  board_->setMinimumSize(640, 260);
  board_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  board_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  theme_combo_->setCurrentIndex(0);
  font_combo_->setCurrentIndex(0);
  if (!stopwatch_.is_running()) {
    toggle_run();
  }
  if (!stopwatch_.is_running()) {
    return fail(QStringLiteral("секундомір не стартує"));
  }
  lap_or_repeat();
  if (stopwatch_.laps().size() != 1U || laps_table_->rowCount() != 1) {
    return fail(QStringLiteral("коло не потрапляє в таблицю"));
  }

  duration_edit_->setText(QStringLiteral("90s"));
  apply_duration();
  if (timer_.target() != std::chrono::seconds{90} || mode_ != Mode::Timer) {
    return fail(QStringLiteral("не застосовується тривалість 90s"));
  }
  duration_edit_->setText(QStringLiteral("немає"));
  apply_duration();
  if (timer_.target() != std::chrono::seconds{90}) {
    return fail(QStringLiteral("хибний час змінив таймер"));
  }

  select_preset(3);
  if (timer_.target() != std::chrono::minutes{3}) {
    return fail(QStringLiteral("пресет не встановлює 3 хвилини"));
  }
  toggle_run();
  if (!timer_.is_running()) {
    return fail(QStringLiteral("таймер не стартує"));
  }

  timer_.set_target(std::chrono::milliseconds{1});
  timer_.set_auto_repeat(false);
  timer_.start(sw::core::CountdownTimer::Clock::now() - std::chrono::milliseconds{40});
  on_tick();
  if (!timer_.is_finished() || !alarm_latched_) {
    return fail(QStringLiteral("таймер не сигналізує про завершення"));
  }
  return true;
}
