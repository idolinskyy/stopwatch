// Copyright (C) 2026 Ihor Dolinskyi
// SPDX-License-Identifier: GPL-3.0-or-later

#include "digital_board_widget.hpp"

#include "electro_font_data.hpp"

#include <QDateTime>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace sw::gui {

namespace {

constexpr int kSegA = 1 << 0;
constexpr int kSegB = 1 << 1;
constexpr int kSegC = 1 << 2;
constexpr int kSegD = 1 << 3;
constexpr int kSegE = 1 << 4;
constexpr int kSegF = 1 << 5;
constexpr int kSegG = 1 << 6;

[[nodiscard]] int segment_mask(char ch) noexcept {
  switch (ch) {
  case '0':
    return kSegA | kSegB | kSegC | kSegD | kSegE | kSegF;
  case '1':
    return kSegB | kSegC;
  case '2':
    return kSegA | kSegB | kSegD | kSegE | kSegG;
  case '3':
    return kSegA | kSegB | kSegC | kSegD | kSegG;
  case '4':
    return kSegB | kSegC | kSegF | kSegG;
  case '5':
    return kSegA | kSegC | kSegD | kSegF | kSegG;
  case '6':
    return kSegA | kSegC | kSegD | kSegE | kSegF | kSegG;
  case '7':
    return kSegA | kSegB | kSegC;
  case '8':
    return kSegA | kSegB | kSegC | kSegD | kSegE | kSegF | kSegG;
  case '9':
    return kSegA | kSegB | kSegC | kSegD | kSegF | kSegG;
  default:
    return 0;
  }
}

[[nodiscard]] qreal glyph_units(char ch) noexcept {
  switch (ch) {
  case ':':
    return 0.36;
  case '.':
    return 0.32;
  case '+':
  case '-':
    return 0.62;
  case ' ':
    return 0.34;
  default:
    return 1.0;
  }
}

[[nodiscard]] QPolygonF horizontal_segment(qreal x0, qreal x1, qreal cy, qreal thickness) {
  const qreal slant = thickness * 0.55;
  const qreal half = thickness * 0.5;
  QPolygonF poly;
  poly << QPointF(x0, cy) << QPointF(x0 + slant, cy - half) << QPointF(x1 - slant, cy - half)
       << QPointF(x1, cy) << QPointF(x1 - slant, cy + half) << QPointF(x0 + slant, cy + half);
  return poly;
}

[[nodiscard]] QPolygonF vertical_segment(qreal cx, qreal y0, qreal y1, qreal thickness) {
  const qreal slant = thickness * 0.55;
  const qreal half = thickness * 0.5;
  QPolygonF poly;
  poly << QPointF(cx, y0) << QPointF(cx + half, y0 + slant) << QPointF(cx + half, y1 - slant)
       << QPointF(cx, y1) << QPointF(cx - half, y1 - slant) << QPointF(cx - half, y0 + slant);
  return poly;
}

[[nodiscard]] QPolygonF shrink_toward_center(const QPolygonF &poly, qreal factor) {
  if (poly.isEmpty()) {
    return {};
  }
  QPointF center;
  for (const QPointF &point : poly) {
    center += point;
  }
  center /= static_cast<qreal>(poly.size());
  QPolygonF inner;
  inner.reserve(poly.size());
  for (const QPointF &point : poly) {
    inner << (center + (point - center) * factor);
  }
  return inner;
}

void paint_segment(QPainter &painter, const QPolygonF &poly, bool lit, const QColor &lit_color,
                   const QColor &ghost_color, const QColor &glow_color, qreal thickness) {
  painter.setPen(Qt::NoPen);
  if (!lit) {
    QColor ghost = ghost_color;
    ghost.setAlpha(78);
    painter.setBrush(ghost);
    painter.drawPolygon(poly);
    return;
  }

  QColor glow = glow_color;
  glow.setAlpha(110);
  painter.setPen(QPen(glow, thickness * 0.95, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);
  painter.drawPolygon(poly);

  painter.setPen(Qt::NoPen);
  painter.setBrush(lit_color);
  painter.drawPolygon(poly);

  QColor hot = lit_color.lighter(165);
  hot.setAlpha(210);
  painter.setBrush(hot);
  painter.drawPolygon(shrink_toward_center(poly, 0.42));
}

void paint_vector_glyph(QPainter &painter, const QRectF &rect, char ch, const QColor &lit_color,
                        const QColor &ghost_color, const QColor &glow_color, bool colon_lit) {
  const qreal thickness = std::clamp(rect.height() * 0.105, 2.0, 28.0);
  const qreal margin = thickness * 0.18;
  const qreal left = rect.left();
  const qreal right = rect.right();
  const qreal top = rect.top();
  const qreal bottom = rect.bottom();
  const qreal mid_y = rect.center().y();
  const qreal mid_x = rect.center().x();

  if (ch == ':') {
    const qreal radius = thickness * 0.62;
    painter.setPen(Qt::NoPen);
    const QColor color = colon_lit ? lit_color : ghost_color;
    painter.setBrush(color);
    painter.drawEllipse(QPointF(mid_x, top + rect.height() * 0.32), radius, radius);
    painter.drawEllipse(QPointF(mid_x, top + rect.height() * 0.72), radius, radius);
    if (colon_lit) {
      QColor hot = lit_color.lighter(160);
      painter.setBrush(hot);
      painter.drawEllipse(QPointF(mid_x, top + rect.height() * 0.32), radius * 0.45, radius * 0.45);
      painter.drawEllipse(QPointF(mid_x, top + rect.height() * 0.72), radius * 0.45, radius * 0.45);
    }
    return;
  }

  if (ch == '.') {
    const qreal radius = thickness * 0.7;
    painter.setPen(Qt::NoPen);
    painter.setBrush(lit_color);
    painter.drawEllipse(QPointF(mid_x, bottom - radius * 1.15), radius, radius);
    painter.setBrush(lit_color.lighter(160));
    painter.drawEllipse(QPointF(mid_x, bottom - radius * 1.15), radius * 0.45, radius * 0.45);
    return;
  }

  if (ch == ' ') {
    return;
  }

  const auto draw_horiz = [&](qreal cy, bool on) {
    const QPolygonF poly =
      horizontal_segment(left + thickness * 0.72, right - thickness * 0.72, cy, thickness);
    paint_segment(painter, poly, on, lit_color, ghost_color, glow_color, thickness);
  };
  const auto draw_vert = [&](qreal cx, qreal y0, qreal y1, bool on) {
    const QPolygonF poly = vertical_segment(cx, y0, y1, thickness);
    paint_segment(painter, poly, on, lit_color, ghost_color, glow_color, thickness);
  };

  if (ch == '-' || ch == '+') {
    draw_horiz(mid_y, true);
    if (ch == '+') {
      draw_vert(mid_x, top + rect.height() * 0.28, bottom - rect.height() * 0.28, true);
    }
    return;
  }

  const int mask = segment_mask(ch);
  const bool known = mask != 0 || (ch >= '0' && ch <= '9');
  if (!known) {
    return;
  }

  const qreal x_left = left + thickness * 0.5 + margin;
  const qreal x_right = right - thickness * 0.5 - margin;
  const qreal y_top = top + thickness * 0.5 + margin;
  const qreal y_bot = bottom - thickness * 0.5 - margin;
  const qreal gap = thickness * 0.42;

  draw_horiz(y_top, (mask & kSegA) != 0);
  draw_horiz(mid_y, (mask & kSegG) != 0);
  draw_horiz(y_bot, (mask & kSegD) != 0);
  draw_vert(x_right, y_top + gap, mid_y - gap, (mask & kSegB) != 0);
  draw_vert(x_right, mid_y + gap, y_bot - gap, (mask & kSegC) != 0);
  draw_vert(x_left, mid_y + gap, y_bot - gap, (mask & kSegE) != 0);
  draw_vert(x_left, y_top + gap, mid_y - gap, (mask & kSegF) != 0);
}

[[nodiscard]] QString ghost_digits(const QString &text) {
  QString ghost;
  ghost.reserve(text.size());
  for (const QChar ch : text) {
    ghost.append(ch.isDigit() ? QChar('8') : ch);
  }
  return ghost;
}

void paint_text_glow(QPainter &painter, const QRectF &rect, const QString &text, const QFont &font,
                     const QColor &color, const QColor &glow, const QColor &ghost, int flags,
                     bool draw_ghost) {
  painter.setFont(font);
  if (draw_ghost) {
    painter.setPen(ghost);
    painter.drawText(rect, flags, ghost_digits(text));
  }

  QColor halo = glow;
  halo.setAlpha(90);
  painter.setPen(halo);
  const qreal spread = std::max<qreal>(1.0, font.pixelSize() * 0.035);
  painter.drawText(rect.translated(-spread, 0), flags, text);
  painter.drawText(rect.translated(spread, 0), flags, text);
  painter.drawText(rect.translated(0, -spread), flags, text);
  painter.drawText(rect.translated(0, spread), flags, text);
  painter.setPen(color);
  painter.drawText(rect, flags, text);
}

[[nodiscard]] QFont pick_mono_font(int pixel_size) {
  const QStringList families = {
    QStringLiteral("CaskaydiaCove Nerd Font Mono"), QStringLiteral("DejaVu Sans Mono"),
    QStringLiteral("Liberation Mono"), QStringLiteral("Noto Sans Mono")};
  const QStringList available = QFontDatabase::families();
  QFont font;
  font.setStyleHint(QFont::TypeWriter);
  font.setFixedPitch(true);
  for (const QString &family : families) {
    if (available.contains(family)) {
      font.setFamily(family);
      break;
    }
  }
  font.setPixelSize(pixel_size);
  font.setWeight(QFont::DemiBold);
  font.setLetterSpacing(QFont::AbsoluteSpacing, pixel_size * 0.04);
  return font;
}

[[nodiscard]] QString load_electro_family() {
  const QByteArray bytes(reinterpret_cast<const char *>(kElectroBoard7FontData.data()),
                         static_cast<qsizetype>(kElectroBoard7FontData.size()));
  const int id = QFontDatabase::addApplicationFontFromData(bytes);
  if (id < 0) {
    return {};
  }
  const QStringList families = QFontDatabase::applicationFontFamilies(id);
  return families.isEmpty() ? QString() : families.front();
}

[[nodiscard]] char ascii_of(QChar ch) noexcept {
  const char16_t code = ch.unicode();
  return code < 128 ? static_cast<char>(code) : ' ';
}

} // namespace

DigitalBoardWidget::DigitalBoardWidget(QWidget *parent) : QWidget(parent) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMinimumHeight(180);
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  electro_family_ = load_electro_family();
}

void DigitalBoardWidget::set_mode_title(const QString &title) {
  if (mode_title_ == title) {
    return;
  }
  mode_title_ = title;
  update();
}

void DigitalBoardWidget::set_status_text(const QString &status, bool active, bool alert) {
  if (status_text_ == status && is_active_ == active && is_alert_ == alert) {
    return;
  }
  status_text_ = status;
  is_active_ = active;
  is_alert_ = alert;
  update();
}

void DigitalBoardWidget::set_main_time(const QString &hh_mm_ss, const QString &millis_part,
                                       const QString &prefix_sign) {
  main_text_ = hh_mm_ss;
  millis_text_ = millis_part;
  prefix_sign_ = prefix_sign;
  update();
}

void DigitalBoardWidget::set_sub_left(const QString &text) {
  if (sub_left_ == text) {
    return;
  }
  sub_left_ = text;
  update();
}

void DigitalBoardWidget::set_sub_right(const QString &text) {
  if (sub_right_ == text) {
    return;
  }
  sub_right_ = text;
  update();
}

void DigitalBoardWidget::set_progress(double ratio, bool show_bar) {
  progress_ = std::clamp(ratio, 0.0, 1.0);
  show_progress_ = show_bar;
  update();
}

void DigitalBoardWidget::set_theme(BoardTheme theme) {
  theme_ = theme;
  update();
}

void DigitalBoardWidget::set_font_style(BoardFontStyle style) {
  if (style == BoardFontStyle::ElectroFontTTF && electro_family_.isEmpty()) {
    style = BoardFontStyle::Vector7Segment;
  }
  font_style_ = style;
  update();
}

void DigitalBoardWidget::set_electro_font_family(const QString &family) {
  electro_family_ = family;
  update();
}

QSize DigitalBoardWidget::minimumSizeHint() const { return {640, 180}; }

QSize DigitalBoardWidget::sizeHint() const { return {880, 280}; }

DigitalBoardWidget::PaletteColors DigitalBoardWidget::current_palette() const noexcept {
  switch (theme_) {
  case BoardTheme::Amber:
    return {.bg_top = QColor(QStringLiteral("#2a1c0c")),
            .bg_bottom = QColor(QStringLiteral("#120c06")),
            .bezel_border = QColor(QStringLiteral("#6a4a1e")),
            .primary = QColor(QStringLiteral("#ffbf3c")),
            .glow = QColor(QStringLiteral("#ff8a00")),
            .ghost = QColor(QStringLiteral("#3a2a14")),
            .sub_text = QColor(QStringLiteral("#e7c48a"))};
  case BoardTheme::Cyan:
    return {.bg_top = QColor(QStringLiteral("#0c2430")),
            .bg_bottom = QColor(QStringLiteral("#06141c")),
            .bezel_border = QColor(QStringLiteral("#1d6278")),
            .primary = QColor(QStringLiteral("#5dffff")),
            .glow = QColor(QStringLiteral("#00c8ff")),
            .ghost = QColor(QStringLiteral("#12303c")),
            .sub_text = QColor(QStringLiteral("#b7f6ff"))};
  case BoardTheme::Crimson:
    return {.bg_top = QColor(QStringLiteral("#320812")),
            .bg_bottom = QColor(QStringLiteral("#14060a")),
            .bezel_border = QColor(QStringLiteral("#7a2436")),
            .primary = QColor(QStringLiteral("#ff4d6a")),
            .glow = QColor(QStringLiteral("#ff1744")),
            .ghost = QColor(QStringLiteral("#3c121c")),
            .sub_text = QColor(QStringLiteral("#ffc1cb"))};
  case BoardTheme::Emerald:
  default:
    return {.bg_top = QColor(QStringLiteral("#0d2b22")),
            .bg_bottom = QColor(QStringLiteral("#06140f")),
            .bezel_border = QColor(QStringLiteral("#1e6b4a")),
            .primary = QColor(QStringLiteral("#3dff9a")),
            .glow = QColor(QStringLiteral("#14f08a")),
            .ghost = QColor(QStringLiteral("#14382c")),
            .sub_text = QColor(QStringLiteral("#b7ffd8"))};
  }
}

void DigitalBoardWidget::paintEvent(QPaintEvent * /*event*/) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);

  const PaletteColors palette = current_palette();
  const bool flash_dim = is_alert_ && ((QDateTime::currentMSecsSinceEpoch() / 260) % 2) == 1;
  QColor lit = flash_dim ? palette.primary.darker(190) : palette.primary;
  QColor glow = flash_dim ? palette.glow.darker(190) : palette.glow;
  const bool colon_lit = !is_active_ || ((QDateTime::currentMSecsSinceEpoch() / 500) % 2) == 0;

  painter.fillRect(rect(), QColor(QStringLiteral("#121418")));

  const QRectF outer = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
  QLinearGradient shell(outer.topLeft(), outer.bottomLeft());
  shell.setColorAt(0.0, palette.bg_top);
  shell.setColorAt(1.0, palette.bg_bottom);
  painter.setPen(QPen(palette.bezel_border, 1.6));
  painter.setBrush(shell);
  painter.drawRoundedRect(outer, 22.0, 22.0);

  QFont header_font = font();
  header_font.setPixelSize(13);
  header_font.setWeight(QFont::DemiBold);
  header_font.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
  painter.setFont(header_font);
  painter.setPen(palette.sub_text);
  const QRectF title_rect(outer.left() + 22.0, outer.top() + 10.0, outer.width() * 0.62, 28.0);
  painter.drawText(title_rect, Qt::AlignVCenter | Qt::AlignLeft, mode_title_);

  QFont status_font = header_font;
  status_font.setPixelSize(12);
  status_font.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
  const QFontMetricsF status_metrics(status_font);
  const qreal pill_w = status_metrics.horizontalAdvance(status_text_) + 28.0;
  const QRectF pill(outer.right() - pill_w - 18.0, outer.top() + 12.0, pill_w, 24.0);
  QColor pill_fill = palette.ghost;
  QColor pill_text = palette.sub_text;
  if (is_alert_) {
    pill_fill = flash_dim ? QColor(QStringLiteral("#5a1020")) : QColor(QStringLiteral("#ff2d4a"));
    pill_text = QColor(QStringLiteral("#fff5f7"));
  } else if (is_active_) {
    pill_fill = palette.primary;
    pill_fill.setAlpha(46);
    pill_text = palette.primary;
  }
  painter.setPen(Qt::NoPen);
  painter.setBrush(pill_fill);
  painter.drawRoundedRect(pill, 12.0, 12.0);
  painter.setFont(status_font);
  painter.setPen(pill_text);
  painter.drawText(pill, Qt::AlignCenter, status_text_);

  const qreal progress_block = show_progress_ ? 22.0 : 0.0;
  QRectF well = outer.adjusted(14.0, 46.0, -14.0, -12.0);
  QLinearGradient well_fill(well.topLeft(), well.bottomLeft());
  well_fill.setColorAt(0.0, palette.bg_bottom.darker(125));
  well_fill.setColorAt(1.0, palette.bg_top.darker(115));
  painter.setPen(QPen(palette.ghost, 1.0));
  painter.setBrush(well_fill);
  painter.drawRoundedRect(well, 14.0, 14.0);

  QRectF glass = well.adjusted(1.0, 1.0, -1.0, -well.height() * 0.62);
  QLinearGradient shine(glass.topLeft(), glass.bottomLeft());
  shine.setColorAt(0.0, QColor(255, 255, 255, 26));
  shine.setColorAt(1.0, QColor(255, 255, 255, 0));
  painter.setPen(Qt::NoPen);
  painter.setBrush(shine);
  painter.drawRoundedRect(glass, 12.0, 12.0);

  const QRectF sub_rect(well.left() + 16.0, well.bottom() - 28.0 - progress_block,
                        well.width() - 32.0, 22.0);
  const QRectF digit_rect(well.left() + 18.0, well.top() + 8.0, well.width() - 36.0,
                          std::max(40.0, sub_rect.top() - well.top() - 14.0));

  const QString full = prefix_sign_ + main_text_ + millis_text_;
  if (font_style_ == BoardFontStyle::Vector7Segment) {
    qreal units = 0.0;
    const qsizetype count = full.size();
    for (const QChar ch : full) {
      units += glyph_units(ascii_of(ch));
    }
    if (count > 1) {
      units += 0.13 * static_cast<qreal>(count - 1);
    }
    units = std::max(units, 1.0);
    constexpr qreal kAspect = 0.58;
    qreal glyph_h = digit_rect.height();
    qreal natural_w = units * glyph_h * kAspect;
    if (natural_w > digit_rect.width()) {
      glyph_h *= digit_rect.width() / natural_w;
    }
    const qreal digit_w = glyph_h * kAspect;
    const qreal gap = digit_w * 0.13;
    qreal used = 0.0;
    for (const QChar ch : full) {
      used += digit_w * glyph_units(ascii_of(ch));
    }
    if (count > 1) {
      used += gap * static_cast<qreal>(count - 1);
    }
    qreal x = digit_rect.left() + (digit_rect.width() - used) * 0.5;
    const qreal y = digit_rect.top() + (digit_rect.height() - glyph_h) * 0.5;
    const qsizetype main_chars = prefix_sign_.size() + main_text_.size();
    qsizetype index = 0;
    for (const QChar ch : full) {
      const qreal w = digit_w * glyph_units(ascii_of(ch));
      const bool fraction = index >= main_chars;
      QColor glyph_lit = lit;
      QColor glyph_glow = glow;
      if (fraction) {
        glyph_lit.setAlpha(230);
      }
      paint_vector_glyph(painter, QRectF(x, y, w, glyph_h), ascii_of(ch), glyph_lit, palette.ghost,
                         glyph_glow, colon_lit);
      x += w + gap;
      ++index;
    }
  } else {
    const int pixel = static_cast<int>(std::clamp(digit_rect.height() * 0.78, 28.0, 180.0));
    QFont digit_font = font_style_ == BoardFontStyle::ElectroFontTTF ? QFont(electro_family_)
                                                                     : pick_mono_font(pixel);
    if (font_style_ == BoardFontStyle::ElectroFontTTF) {
      digit_font.setPixelSize(pixel);
      digit_font.setLetterSpacing(QFont::AbsoluteSpacing, pixel * 0.03);
    } else {
      digit_font.setPixelSize(pixel);
    }
    QFontMetricsF metrics(digit_font);
    const qreal text_w = metrics.horizontalAdvance(full);
    if (text_w > digit_rect.width() && text_w > 0.0) {
      const int fitted = static_cast<int>(pixel * (digit_rect.width() / text_w));
      digit_font.setPixelSize(std::max(fitted, 12));
    }
    QColor ghost = palette.ghost.lighter(130);
    ghost.setAlpha(170);
    paint_text_glow(painter, digit_rect, full, digit_font, lit, glow, ghost, Qt::AlignCenter,
                    font_style_ == BoardFontStyle::ElectroFontTTF);
  }

  QFont sub_font = font();
  sub_font.setPixelSize(13);
  sub_font.setWeight(QFont::Medium);
  painter.setFont(sub_font);
  painter.setPen(palette.sub_text);
  painter.drawText(sub_rect, Qt::AlignVCenter | Qt::AlignLeft, sub_left_);
  painter.drawText(sub_rect, Qt::AlignVCenter | Qt::AlignRight, sub_right_);

  if (show_progress_) {
    const QRectF track(well.left() + 16.0, well.bottom() - 16.0, well.width() - 32.0, 8.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.ghost);
    painter.drawRoundedRect(track, 4.0, 4.0);
    QRectF fill = track;
    fill.setWidth(std::max(0.0, track.width() * progress_));
    painter.setBrush(is_alert_ ? QColor(QStringLiteral("#ff3355")) : lit);
    if (fill.width() > 0.5) {
      painter.drawRoundedRect(fill, 4.0, 4.0);
    }
  }
}

} // namespace sw::gui
