// Copyright (C) 2026 Ihor Dolinskyi
// SPDX-License-Identifier: GPL-3.0-or-later

#include "main_window.hpp"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QPixmap>

#include <print>
#include <string_view>

namespace {

[[nodiscard]] QIcon make_icon() {
  QPixmap pixmap(64, 64);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setBrush(QColor(QStringLiteral("#10241c")));
  painter.setPen(QPen(QColor(QStringLiteral("#1e6b4a")), 3));
  painter.drawRoundedRect(QRectF(3, 3, 58, 58), 14, 14);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(QStringLiteral("#3dff9a")));
  painter.drawRoundedRect(QRectF(16, 18, 7, 28), 2, 2);
  painter.drawRoundedRect(QRectF(29, 18, 7, 28), 2, 2);
  painter.drawRoundedRect(QRectF(42, 18, 7, 28), 2, 2);
  return QIcon(pixmap);
}

void apply_dark_theme(QApplication &application) {
  application.setStyle(QStringLiteral("Fusion"));
  QPalette palette;
  palette.setColor(QPalette::Window, QColor(QStringLiteral("#121418")));
  palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#e8eaed")));
  palette.setColor(QPalette::Base, QColor(QStringLiteral("#1b1f26")));
  palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#232833")));
  palette.setColor(QPalette::Text, QColor(QStringLiteral("#e8eaed")));
  palette.setColor(QPalette::Button, QColor(QStringLiteral("#2a303a")));
  palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#e8eaed")));
  palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#1f7a52")));
  palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#f4fff8")));
  palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#1b1f26")));
  palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#e8eaed")));
  palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#8b93a1")));
  application.setPalette(palette);

  QFont font(QStringLiteral("Noto Sans"));
  font.setPointSize(11);
  application.setFont(font);
}

} // namespace

int main(int argc, char *argv[]) {
  QApplication application(argc, argv);
  QApplication::setApplicationName(QStringLiteral("StopWatch"));
  QApplication::setOrganizationName(QStringLiteral("StopWatch"));
  QApplication::setApplicationDisplayName(QStringLiteral("StopWatch"));
  apply_dark_theme(application);
  application.setWindowIcon(make_icon());

  bool self_check = false;
  for (int i = 1; i < argc; ++i) {
    const std::string_view argument{argv[i]};
    if (argument == "--self-check") {
      self_check = true;
    } else if (argument == "--help" || argument == "-h") {
      std::println("StopWatch-gui — секундомір і таймер");
      std::println("");
      std::println("  StopWatch-gui              відкрити вікно");
      std::println("  StopWatch-gui --self-check перевірка без екрана");
      return 0;
    } else {
      std::println(stderr, "Невідомий аргумент '{}'. Див. --help.", argument);
      return 1;
    }
  }

  MainWindow window(!self_check);
  if (self_check) {
    QString error;
    if (!window.self_check(&error)) {
      std::println(stderr, "Перевірка GUI не пройшла: {}", error.toStdString());
      return 1;
    }
    std::println("Перевірка GUI пройшла.");
    return 0;
  }

  window.show();
  return application.exec();
}
