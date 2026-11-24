// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_menu.hpp"

#include "qt_bridge.hpp"
#include "toggle_switch.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QCoreApplication>
#include <QFont>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <tuple>

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

/// S048: a section heading followed by its line, which takes the rest of the width.
QHBoxLayout* makeHeadingRow(QLabel* label, SectionRule* rule) {
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(label, 0, Qt::AlignLeft | Qt::AlignVCenter);
    row->addWidget(rule, 1, Qt::AlignVCenter);
    return row;
}

}  // namespace

SectionRule::SectionRule(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void SectionRule::setRule(const QColor& color, double width) {
    color_ = color;
    width_ = width;
    update();
}

void SectionRule::paintEvent(QPaintEvent* /*event*/) {
    // Whole logical pixels, so that a line of width 1 stays crisp instead of blurring over two rows.
    const int thickness = static_cast<int>(std::lround(width_));
    if (thickness <= 0) {
        return;
    }
    QPainter painter(this);
    painter.fillRect(QRect(0, (height() - thickness) / 2, width(), thickness), color_);
}

SettingsMenu::SettingsMenu(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::NoFocus);
    setObjectName(QStringLiteral("settingsMenu"));
    setAccessibleName(tr("Settings menu"));
    setMinimumWidth(260);

    mainLayout_ = new QVBoxLayout(this);
    titleLabel_ = makeLabel(tr("Settings"), this, "settingsTitleLabel");

    appearanceLabel_ = makeLabel(tr("Appearance"), this, "appearanceLabel");
    appearanceRule_ = new SectionRule(this);
    appearanceRule_->setObjectName(QStringLiteral("appearanceRule"));
    appearanceHeading_ = makeHeadingRow(appearanceLabel_, appearanceRule_);
    themeKeyLabel_ = makeLabel(tr("Theme"), this);
    themeCombo_ = new QComboBox(this);
    themeCombo_->setObjectName(QStringLiteral("themeSelector"));
    themeCombo_->setAccessibleName(tr("Theme"));
    themeCombo_->setFocusPolicy(Qt::NoFocus);
    connect(themeCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0 && index < entries_.size()) {
            const ThemeEntry entry = entries_.at(index);
            emit themeEntrySelected(entry);
        }
    });

    appearanceLayout_ = new QGridLayout;
    appearanceLayout_->addWidget(themeKeyLabel_, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    appearanceLayout_->addWidget(themeCombo_, 0, 1);
    appearanceLayout_->setColumnStretch(1, 1);

    // S043: the section Feel, with the switch for smooth scrolling (S027).
    feelLabel_ = makeLabel(tr("Feel"), this, "feelLabel");
    feelRule_ = new SectionRule(this);
    feelRule_->setObjectName(QStringLiteral("feelRule"));
    feelHeading_ = makeHeadingRow(feelLabel_, feelRule_);
    smoothKeyLabel_ = makeLabel(tr("Smooth scrolling"), this, "smoothScrollingLabel");
    smoothSwitch_ = new ToggleSwitch(this);
    smoothSwitch_->setObjectName(QStringLiteral("smoothScrollingSwitch"));
    smoothSwitch_->setAccessibleName(tr("Smooth scrolling"));
    smoothSwitch_->setCheckedImmediately(true);
    connect(smoothSwitch_, &QAbstractButton::toggled, this, &SettingsMenu::smoothScrollingToggled);

    feelLayout_ = new QGridLayout;
    feelLayout_->addWidget(smoothKeyLabel_, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    feelLayout_->addWidget(smoothSwitch_, 0, 1, Qt::AlignRight | Qt::AlignVCenter);
    feelLayout_->setColumnStretch(0, 1);

    QString version = QCoreApplication::applicationVersion();
    if (version.isEmpty()) {
        version = QStringLiteral(HITOKOTO_UI_VERSION);
    }
    const QString versionText = version.startsWith(QLatin1Char('v')) ? version : QStringLiteral("v%1").arg(version);
    versionLabel_ = makeLabel(versionText, this, "settingsVersionLabel");

    mainLayout_->addWidget(titleLabel_);
    mainLayout_->addLayout(appearanceHeading_);
    mainLayout_->addLayout(appearanceLayout_);
    mainLayout_->addLayout(feelHeading_);
    mainLayout_->addLayout(feelLayout_);
    mainLayout_->addWidget(versionLabel_, 0, Qt::AlignRight);

    applyTheme(core::builtinTheme(core::ColorScheme::Light));
    hide();
}

void SettingsMenu::setAvailableThemes(const QList<ThemeEntry>& entries, const QString& activePath,
                                      const core::Theme& activeTheme) {
    const QSignalBlocker blocker(themeCombo_);
    if (entries_ != entries) {
        entries_ = entries;
        themeCombo_->clear();
        for (const ThemeEntry& entry : entries_) {
            themeCombo_->addItem(entry.name);
        }
    }

    int selected = -1;
    if (!activePath.isEmpty()) {
        for (int i = 0; i < entries_.size(); ++i) {
            if (!entries_.at(i).builtinScheme.has_value() && entries_.at(i).path == activePath) {
                selected = i;
                break;
            }
        }
    }
    if (selected < 0 && !activeTheme.name.empty()) {
        const QString activeName = QString::fromStdString(activeTheme.name);
        for (int i = 0; i < entries_.size(); ++i) {
            if (entries_.at(i).name == activeName) {
                selected = i;
                break;
            }
        }
    }
    if (selected < 0) {
        for (int i = 0; i < entries_.size(); ++i) {
            if (entries_.at(i).builtinScheme == activeTheme.scheme) {
                selected = i;
                break;
            }
        }
    }
    if (selected >= 0 && themeCombo_->currentIndex() != selected) {
        themeCombo_->setCurrentIndex(selected);
    }
    adjustSize();
}

void SettingsMenu::setSmoothScrolling(bool on) {
    // The state the switch already has stays as it is, so that a running animation of a flip goes on.
    if (smoothSwitch_->isChecked() != on) {
        smoothSwitch_->setCheckedImmediately(on);
    }
}

void SettingsMenu::applyTheme(const core::Theme& theme) {
    style_ = core::toolbarStyle(theme);

    QFont uiFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    if (!theme.uiFontFamily.empty()) {
        uiFont.setFamilies({QString::fromStdString(theme.uiFontFamily)});
    }
    if (theme.uiFontSize > 0) {
        uiFont.setPointSizeF(theme.uiFontSize);
    }
    setFont(uiFont);
    for (QLabel* label : {themeKeyLabel_, smoothKeyLabel_}) {
        label->setFont(uiFont);
    }
    // S048: section headings in their own colour and weight, each followed by a line.
    const core::SettingsSectionStyle section = core::settingsSectionStyle(theme);
    QFont headingFont = uiFont;
    headingFont.setWeight(static_cast<QFont::Weight>(section.weight));
    const int ruleGap = static_cast<int>(std::lround(section.ruleGap));
    for (const auto& [label, rule, row] : {std::tuple{appearanceLabel_, appearanceRule_, appearanceHeading_},
                                           std::tuple{feelLabel_, feelRule_, feelHeading_}}) {
        label->setFont(headingFont);
        setLabelColor(label, toQColor(section.text));
        rule->setRule(toQColor(section.rule), section.ruleWidth);
        rule->setFixedHeight(label->sizeHint().height());
        row->setSpacing(ruleGap);
    }
    themeCombo_->setFont(uiFont);
    if (QAbstractItemView* view = themeCombo_->view()) {
        view->setFont(uiFont);
    }
    QFont titleFont = uiFont;
    titleFont.setWeight(QFont::DemiBold);
    titleLabel_->setFont(titleFont);

    QFont versionFont = uiFont;
    const qreal hintSize = uiFont.pointSizeF() * 0.85;
    if (hintSize > 0) {
        versionFont.setPointSizeF(hintSize);
    }
    versionLabel_->setFont(versionFont);

    const QColor textColor = toQColor(theme.text);
    const QColor mutedColor = toQColor(theme.textMuted);
    setLabelColor(titleLabel_, textColor);
    setLabelColor(themeKeyLabel_, mutedColor);
    setLabelColor(smoothKeyLabel_, mutedColor);
    smoothSwitch_->setSwitchStyle(core::switchStyle(theme));
    setLabelColor(versionLabel_, mutedColor);

    const int pad = std::max(8, static_cast<int>(std::lround(style_.padding * 2.0)));
    const int gap = std::max(4, static_cast<int>(std::lround(style_.spacing)));
    mainLayout_->setContentsMargins(pad, pad, pad, pad);
    mainLayout_->setSpacing(gap);
    appearanceLayout_->setHorizontalSpacing(gap * 2);
    feelLayout_->setHorizontalSpacing(gap * 2);

    adjustSize();
    update();
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
