// SPDX-License-Identifier: GPL-3.0-or-later
#include "theme_applier.hpp"

#include "qt_bridge.hpp"

#include <QApplication>
#include <QFontDatabase>
#include <QStyle>
#include <QStyleFactory>

#include <cmath>

namespace hitokoto::ui {
namespace {

QString hex(const core::Color& c) {
    return toQColor(c).name(c.a == 255 ? QColor::HexRgb : QColor::HexArgb);
}
QString px(double v) {
    return QString::number(std::lround(v)) + QStringLiteral("px");
}

}  // namespace

QPalette makePalette(const core::Theme& t) {
    QPalette p;
    const QColor background = toQColor(t.background);
    const QColor surface = toQColor(t.surface);
    const QColor text = toQColor(t.text);
    const QColor muted = toQColor(t.textMuted);
    const QColor border = toQColor(t.border);
    p.setColor(QPalette::Window, background);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, background);
    p.setColor(QPalette::AlternateBase, surface);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, surface);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, toQColor(t.selectionText));
    p.setColor(QPalette::Highlight, toQColor(t.selection));
    p.setColor(QPalette::HighlightedText, toQColor(t.selectionText));
    p.setColor(QPalette::ToolTipBase, surface);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, muted);
    p.setColor(QPalette::Link, toQColor(t.accent));
    p.setColor(QPalette::LinkVisited, toQColor(t.secondary));
    p.setColor(QPalette::Accent, toQColor(t.accent));
    p.setColor(QPalette::Light, surface);
    p.setColor(QPalette::Midlight, border);
    p.setColor(QPalette::Mid, border);
    p.setColor(QPalette::Dark, border);
    p.setColor(QPalette::Shadow, border);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        p.setColor(QPalette::Disabled, role, muted);
    }
    return p;
}

QString makeStyleSheet(const core::Theme& t) {
    const QString radius = px(t.radius);
    const QString half = px(t.spacing / 2);
    const QString full = px(t.spacing);
    const QString twice = px(t.spacing * 2);
    const QString handle = px(std::max(4.0, t.spacing));

    QString buttonBackground = hex(t.primary);
    QString buttonText = hex(t.background);
    QString buttonBorder = hex(t.primary);
    switch (t.buttonStyle) {
        case core::ButtonStyle::Flat:
            buttonBackground = QStringLiteral("transparent");
            buttonText = hex(t.primary);
            buttonBorder = QStringLiteral("transparent");
            break;
        case core::ButtonStyle::Outlined:
            buttonBackground = QStringLiteral("transparent");
            buttonText = hex(t.primary);
            break;
        case core::ButtonStyle::Filled:
            break;
    }

    const core::MenuBarStyle mb = core::menuBarStyle(t);

    // Placeholders: %1 background, %2 surface, %3 border, %4 text, %5 muted, %6 accent,
    // %7 selection, %8 selection text, %9 radius.
    QString css = QStringLiteral(R"(
QMainWindow, QDialog, QMessageBox { background: %1; color: %4; }
QPlainTextEdit, QTextEdit { background: %1; color: %4; border: none; selection-background-color: %7; selection-color: %8; }
QMenu { background: %2; color: %4; border: 1px solid %3; border-radius: %9; }
QMenu::item:selected { background: %6; color: %1; border-radius: %9; }
QMenu::item:disabled { color: %5; }
QMenu::separator { height: 1px; background: %3; }
QScrollBar:vertical, QScrollBar:horizontal { background: transparent; border: none; }
QScrollBar::handle { background: %3; border-radius: %9; }
QScrollBar::handle:hover { background: %6; }
QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page { background: none; border: none; height: 0; width: 0; }
QToolTip { background: %2; color: %4; border: 1px solid %3; }
)")
                      .arg(hex(t.background), hex(t.surface), hex(t.border), hex(t.text), hex(t.textMuted),
                           hex(t.accent), hex(t.selection), hex(t.selectionText), radius);

    css += QStringLiteral(R"(
QMenuBar { background: %1; color: %2; border: none; border-bottom: %3 solid %4; padding: %5; spacing: %6; }
QMenuBar::item { background: transparent; padding: %7 %8; margin: 0px; border-radius: %9; }
QMenuBar::item:selected { background: %10; color: %11; }
QMenuBar::item:pressed { background: %12; color: %13; }
)")
               .arg(hex(mb.background), hex(mb.text), px(std::max(0.0, mb.borderWidth)), hex(mb.border),
                    px(std::max(0.0, mb.padding)), px(std::max(0.0, mb.spacing)),
                    px(std::max(0.0, mb.itemPaddingVertical)), px(std::max(0.0, mb.itemPaddingHorizontal)),
                    px(std::max(0.0, mb.itemRadius)))
               .arg(hex(mb.hoverBackground), hex(mb.hoverText), hex(mb.activeBackground), hex(mb.activeText));

    css += QStringLiteral(R"(
QMenu { padding: %1; }
QMenu::item { padding: %1 %3; }
QMenu::separator { margin: %1 %2; }
QScrollBar:vertical { width: %4; margin: %1 0; }
QScrollBar:horizontal { height: %4; margin: 0 %1; }
QScrollBar::handle:vertical { min-height: %5; }
QScrollBar::handle:horizontal { min-width: %5; }
QPushButton { padding: %1 %3; min-width: %5; border-radius: %6; }
)")
               .arg(half, full, twice, handle, px(t.spacing * 4), radius);

    css += QStringLiteral(R"(
QPushButton { background: %1; color: %2; border: 1px solid %3; }
QPushButton:hover { border-color: %4; }
QPushButton:focus { border-color: %4; }
QPushButton:pressed { background: %5; color: %6; }
QPushButton:disabled { color: %7; border-color: %8; }
)")
               .arg(buttonBackground, buttonText, buttonBorder, hex(t.accent), hex(t.selection), hex(t.selectionText),
                    hex(t.textMuted), hex(t.border));

    css += QStringLiteral(R"(
QComboBox { background: %1; color: %2; border: 1px solid %3; border-radius: %4; padding: %5 %6; }
QComboBox:hover, QComboBox:focus, QComboBox:on { border-color: %7; }
QComboBox::drop-down { border: none; width: %8; }
QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3; outline: 0; }
)")
               .arg(hex(t.surface), hex(t.text), hex(t.border), radius, half, full, hex(t.accent), px(t.spacing * 3));

    css += QStringLiteral(R"(
QComboBox QAbstractItemView::item:selected { background: %1; color: %2; }
)")
               .arg(hex(t.selection), hex(t.selectionText));
    return css;
}

void applyThemeToApplication(QApplication& app, const core::Theme& t) {
    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        QApplication::setStyle(fusion);
    }
    QApplication::setPalette(makePalette(t));
    QFont uiFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    if (!t.uiFontFamily.empty()) {
        uiFont.setFamilies({QString::fromStdString(t.uiFontFamily)});
    }
    if (t.uiFontSize > 0) {
        uiFont.setPointSizeF(t.uiFontSize);
    }
    QApplication::setFont(uiFont);
    app.setStyleSheet(makeStyleSheet(t));
}

}  // namespace hitokoto::ui
