// Copyright (C) 2026 Ihor Dolinskyi
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

namespace sw::gui {

enum class BoardTheme {
  Emerald = 0,
  Amber = 1,
  Cyan = 2,
  Crimson = 3,
};

enum class BoardFontStyle {
  Vector7Segment = 0,
  ElectroFontTTF = 1,
  MonospaceTabular = 2,
};

class DigitalBoardWidget final : public QWidget {
public:
  explicit DigitalBoardWidget(QWidget *parent = nullptr);

  void set_mode_title(const QString &title);
  void set_status_text(const QString &status, bool active, bool alert = false);
  void set_main_time(const QString &hh_mm_ss, const QString &millis_part,
                     const QString &prefix_sign = QString());
  void set_sub_left(const QString &text);
  void set_sub_right(const QString &text);
  void set_progress(double ratio, bool show_bar);

  void set_theme(BoardTheme theme);
  [[nodiscard]] BoardTheme theme() const noexcept { return theme_; }

  void set_font_style(BoardFontStyle style);
  [[nodiscard]] BoardFontStyle font_style() const noexcept { return font_style_; }

  void set_electro_font_family(const QString &family);
  [[nodiscard]] bool has_electro_font() const noexcept { return !electro_family_.isEmpty(); }
  [[nodiscard]] QString electro_font_family() const { return electro_family_; }

  [[nodiscard]] QSize minimumSizeHint() const override;
  [[nodiscard]] QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  struct PaletteColors {
    QColor bg_top;
    QColor bg_bottom;
    QColor bezel_border;
    QColor primary;
    QColor glow;
    QColor ghost;
    QColor sub_text;
  };

  [[nodiscard]] PaletteColors current_palette() const noexcept;

  QString mode_title_{QStringLiteral("СЕКУНДОМІР")};
  QString status_text_{QStringLiteral("ГОТОВИЙ")};
  QString main_text_{QStringLiteral("00:00:00")};
  QString millis_text_{QStringLiteral(".000")};
  QString prefix_sign_{};
  QString sub_left_{QStringLiteral("ПОТОЧНЕ КОЛО: 00:00:00.000")};
  QString sub_right_{QStringLiteral("КІЛ: 0")};
  QString electro_family_{};

  double progress_{0.0};
  bool show_progress_{false};
  bool is_active_{false};
  bool is_alert_{false};
  BoardTheme theme_{BoardTheme::Emerald};
  BoardFontStyle font_style_{BoardFontStyle::Vector7Segment};
};

} // namespace sw::gui
