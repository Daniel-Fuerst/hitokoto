// SPDX-License-Identifier: GPL-3.0-or-later
// The editing surface: a QPlainTextEdit whose document is the only copy of the text
// (ADR 0002). Rendering comes from the core through BlockStyler; this class adds the
// cursor-aware reveal, bullet drawing and click handling.
#pragma once

#include "block_styler.hpp"
#include "hitokoto/core/theme.hpp"

#include <QPlainTextEdit>
#include <QPointF>

class QPainter;
class QTextBlock;

namespace hitokoto::ui {

class EditorWidget : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit EditorWidget(const core::ElementRegistry& registry, QWidget* parent = nullptr);

    void applyTheme(const core::Theme& theme);

    [[nodiscard]] BlockStyler& styler() noexcept { return *styler_; }
    /// Counts real text edits (not formatting); used to decide whether a save is current.
    [[nodiscard]] quint64 editCount() const noexcept { return editCount_; }
    /// First and last block number on screen.
    [[nodiscard]] std::pair<int, int> visibleBlockRange() const;

signals:
    void painted();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    void updateReveal();
    void updateMargins();
    void paintCodeBlockPanels();
    void paintInlineCodeBoxes(QPainter& painter, const QTextBlock& block, const core::LineRender& render,
                              QPointF origin, const core::CodeStyle& style);
    void paintDecorations();

    BlockStyler* styler_ = nullptr;
    double padding_ = 0;
    double maxWidth_ = 0;
    quint64 editCount_ = 0;
};

}  // namespace hitokoto::ui
