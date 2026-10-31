// SPDX-License-Identifier: GPL-3.0-or-later
// S024: the properties block shown in place of a note's front matter: a title, then one row per
// property with a type icon, the key and the value (Obsidian). It writes nothing: a click asks the
// editor to put the caret at the value in the YAML (PropertiesEmbedder). One widget paints the rows
// on screen; each row is an accessible element of its own (an accessible interface per row, not a
// widget, so that 5,000 properties cost no 5,000 widgets). Rows are laid out in slices (review O8).
#pragma once

#include "hitokoto/core/properties.hpp"
#include "hitokoto/core/theme.hpp"

#include <QAccessible>
#include <QElapsedTimer>
#include <QFont>
#include <QStringList>
#include <QWidget>

#include <vector>

namespace hitokoto::ui {

class PropertiesView : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesView(QWidget* parent = nullptr);
    ~PropertiesView() override;
    PropertiesView(const PropertiesView&) = delete;
    PropertiesView& operator=(const PropertiesView&) = delete;

    /// What the rows look like: resolved tokens, the block's font and its width.
    struct Look {
        core::PropertiesStyle style;
        QFont font;
        int width = 400;
    };
    /// Takes a new look; rows laid out already are laid out again at once.
    void setLook(const Look& look);
    [[nodiscard]] const Look& look() const noexcept { return look_; }

    /// Starts over with these properties; no row is laid out yet.
    void reset(std::vector<core::Property> properties);
    /// Lays out rows in document order until `nanoseconds` have passed since `timer` started; true
    /// when every row is laid out and the block has its height.
    bool build(const QElapsedTimer& timer, qint64 nanoseconds);
    [[nodiscard]] bool built() const noexcept { return laidOut_; }
    [[nodiscard]] std::size_t rowCount() const noexcept { return properties_.size(); }
    [[nodiscard]] const core::Property& property(std::size_t row) const { return properties_.at(row); }

    /// The text a row shows for its value, as its accessible name has it after "key: " (dates in
    /// the system's short format, a checkbox "checked" or "not checked", items joined by ", ",
    /// the full JSON of an unsupported value).
    [[nodiscard]] static QString valueText(const core::Property& property);
    /// The type as the accessible description names it ("text", "date and time", ...).
    [[nodiscard]] static QString typeName(core::PropertyType type);

    // Accessible elements: 0 is the title, 1 + i row i.
    [[nodiscard]] int accessibleCount() const noexcept { return 1 + static_cast<int>(rows_.size()); }
    [[nodiscard]] QString accessibleName(int element) const;
    [[nodiscard]] QString accessibleDescription(int element) const;
    /// In the view's coordinates.
    [[nodiscard]] QRect accessibleRect(int element) const;
    [[nodiscard]] QAccessibleInterface* accessibleElement(int element);

    /// Lines of keys and values painted by the last paint event (review O2: only those on screen).
    [[nodiscard]] int paintedTextLines() const noexcept { return paintedTextLines_; }

signals:
    /// A click released on row `row` (-1: the title or elsewhere in the block); `extend`: with
    /// Shift, the selection is extended to it.
    void activated(int row, bool extend);
    /// A mouse button went down on the block (true) or up again (false).
    void pressed(bool down);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    bool event(QEvent* event) override;

private:
    struct Chip {
        QRectF rect;  ///< relative to the value's top left
        QString text;
    };
    /// One property as laid out.
    struct Row {
        QRect rect;  ///< in the view's coordinates
        QString key;
        QString name;       ///< accessible name, "key: value"
        QString text;       ///< text, number, date: the value shown
        QStringList lines;  ///< unsupported: at most three lines of JSON
        std::vector<Chip> chips;
        qreal keyWidth = 0;  ///< of the key's text
        qreal valueX = 0;    ///< where the value starts in the row
        qreal valueWidth = 0;
    };
    [[nodiscard]] Row layoutRow(const core::Property& property, int top, int width) const;
    void paintRow(QPainter& painter, const Row& row, const core::Property& property) const;
    void paintIcon(QPainter& painter, const QRectF& rect, core::PropertyType type) const;
    void finishLayout();
    void forgetAccessibleElements();
    [[nodiscard]] int rowAt(QPoint position) const;
    [[nodiscard]] int innerWidth() const;
    void updateMetrics();

    Look look_;
    std::vector<core::Property> properties_;
    std::vector<Row> rows_;  ///< laid out so far, in document order
    QRect title_;
    qreal line_ = 0;  ///< line spacing of the block's font
    qreal gap_ = 0;   ///< width of a space
    qreal icon_ = 0;  ///< size of a type icon
    bool laidOut_ = false;
    bool pressed_ = false;
    int paintedTextLines_ = 0;
    std::vector<QAccessible::Id> accessibleIds_;  ///< registered on demand, 0 = none yet
};

}  // namespace hitokoto::ui
