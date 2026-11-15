// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_menu.hpp"

#include "qt_bridge.hpp"

#include <QCoreApplication>
#include <QFont>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QSysInfo>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace hitokoto::ui {
namespace {

void setLabelColor(QLabel* label, const QColor& color) {
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, color);
    label->setPalette(pal);
}

QLabel* makeLabel(const QString& text, QWidget* parent, const char* objectName = nullptr) {
    auto* label = new QLabel(text, parent);
    label->setFocusPolicy(Qt::NoFocus);
    if (objectName != nullptr) {
        label->setObjectName(QString::fromLatin1(objectName));
    }
    return label;
}

}  // namespace

SettingsMenu::SettingsMenu(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::NoFocus);
    setObjectName(QStringLiteral("settingsMenu"));
    setAccessibleName(tr("Settings menu"));

    mainLayout_ = new QVBoxLayout(this);
    titleLabel_ = makeLabel(tr("Settings"), this, "settingsTitleLabel");
    sectionLabel_ = makeLabel(tr("Debug info"), this);

    mainLayout_->addWidget(titleLabel_);
    mainLayout_->addWidget(sectionLabel_);

    gridLayout_ = new QGridLayout();
    gridLayout_->setContentsMargins(0, 0, 0, 0);

    QString appName = QCoreApplication::applicationName();
    if (appName.isEmpty()) {
        appName = QStringLiteral("hitokoto");
    }
    const QString appVersion = QCoreApplication::applicationVersion();
    const QString appText = appVersion.isEmpty() ? appName : QStringLiteral("%1 %2").arg(appName, appVersion);
    const QString qtText =
        QStringLiteral("%1 (%2)").arg(QString::fromLatin1(qVersion()), QSysInfo::currentCpuArchitecture());
    const QString platform = QGuiApplication::platformName();
    const QString platformText = QStringLiteral("%1 (%2)").arg(
        QSysInfo::prettyProductName(), platform.isEmpty() ? QStringLiteral("unknown") : platform);

    keyLabels_ = {
        makeLabel(tr("Application"), this), makeLabel(tr("Qt"), this),       makeLabel(tr("Platform"), this),
        makeLabel(tr("Theme"), this),       makeLabel(tr("Document"), this),
    };

    appValue_ = makeLabel(appText, this, "debugAppValue");
    qtValue_ = makeLabel(qtText, this, "debugQtValue");
    platformValue_ = makeLabel(platformText, this, "debugPlatformValue");
    themeValue_ = makeLabel(QString(), this, "debugThemeValue");
    documentValue_ = makeLabel(QString(), this, "debugDocumentValue");

    const std::array<QLabel*, 5> values = {appValue_, qtValue_, platformValue_, themeValue_, documentValue_};
    for (int row = 0; row < 5; ++row) {
        gridLayout_->addWidget(keyLabels_[static_cast<std::size_t>(row)], row, 0, Qt::AlignLeft | Qt::AlignVCenter);
        gridLayout_->addWidget(values[static_cast<std::size_t>(row)], row, 1, Qt::AlignLeft | Qt::AlignVCenter);
    }
    gridLayout_->setColumnStretch(1, 1);
    mainLayout_->addLayout(gridLayout_);

    applyTheme(core::builtinTheme(core::ColorScheme::Light));
    setDocumentStats(1, 0);
    hide();
}

void SettingsMenu::applyTheme(const core::Theme& theme) {
    style_ = core::toolbarStyle(theme);

    const QString themeName = theme.name.empty() ? tr("Unnamed") : QString::fromStdString(theme.name);
    const QString schemeName = theme.scheme == core::ColorScheme::Dark ? tr("dark") : tr("light");
    themeValue_->setText(QStringLiteral("%1 (%2)").arg(themeName, schemeName));

    QFont uiFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    if (!theme.uiFontFamily.empty()) {
        uiFont.setFamilies({QString::fromStdString(theme.uiFontFamily)});
    }
    if (theme.uiFontSize > 0) {
        uiFont.setPointSizeF(theme.uiFontSize);
    }
    setFont(uiFont);
    for (QLabel* label : {sectionLabel_, appValue_, qtValue_, platformValue_, themeValue_, documentValue_}) {
        label->setFont(uiFont);
    }
    for (QLabel* key : keyLabels_) {
        key->setFont(uiFont);
    }
    QFont titleFont = uiFont;
    titleFont.setWeight(QFont::DemiBold);
    titleLabel_->setFont(titleFont);

    const QColor textColor = toQColor(theme.text);
    const QColor mutedColor = toQColor(theme.textMuted);
    setLabelColor(titleLabel_, textColor);
    setLabelColor(sectionLabel_, mutedColor);
    for (QLabel* key : keyLabels_) {
        setLabelColor(key, mutedColor);
    }
    for (QLabel* value : {appValue_, qtValue_, platformValue_, themeValue_, documentValue_}) {
        setLabelColor(value, textColor);
    }

    const int pad = std::max(8, static_cast<int>(std::lround(style_.padding * 2.0)));
    const int gap = std::max(4, static_cast<int>(std::lround(style_.spacing)));
    mainLayout_->setContentsMargins(pad, pad, pad, pad);
    mainLayout_->setSpacing(gap);
    gridLayout_->setHorizontalSpacing(gap * 2);
    gridLayout_->setVerticalSpacing(std::max(2, gap / 2));

    adjustSize();
    update();
}

void SettingsMenu::setDocumentStats(int lineCount, int charCount) {
    documentValue_->setText(tr("%n line(s), %1 char(s)", nullptr, lineCount).arg(charCount));
    adjustSize();
}

void SettingsMenu::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal border = style_.borderWidth;
    painter.setPen(border > 0 ? QPen(toQColor(style_.border), border) : QPen(Qt::NoPen));
    painter.setBrush(toQColor(style_.background));
    const QRectF inner = QRectF(rect()).adjusted(border / 2.0, border / 2.0, -border / 2.0, -border / 2.0);
    const qreal radius = std::max<qreal>(0, style_.radius - border / 2.0);
    painter.drawRoundedRect(inner, radius, radius);
}

}  // namespace hitokoto::ui
