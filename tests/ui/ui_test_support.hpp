// SPDX-License-Identifier: GPL-3.0-or-later
// Helpers shared by the UI tests (headless, QT_QPA_PLATFORM=offscreen).
#pragma once

#include "document_session.hpp"
#include "editor_widget.hpp"
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"
#include "key_input.hpp"
#include "main_window.hpp"
#include "theme_applier.hpp"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
#include <QTextDocumentFragment>
#include <QTextLayout>

#include <memory>
#include <string>
#include <vector>

namespace hitokoto::test {

/// Factor for the time limits inside the UI tests. A build with sanitizers (HITOKOTO_SANITIZE, the
/// CI's job ui-sanitizers) runs several times slower; what a limit guards is the same there.
#ifdef HITOKOTO_TESTS_UNDER_SANITIZERS
inline constexpr int kTimeScale = 4;
#else
inline constexpr int kTimeScale = 1;
#endif

inline const core::ElementRegistry& registry() {
    static const core::ElementRegistry r = core::ElementRegistry::withBuiltins();
    return r;
}

/// A shown main window with the built-in light theme and a recording message sink.
struct Fixture {
    QTemporaryDir dir;
    std::unique_ptr<ui::MainWindow> window;
    QStringList messages;

    Fixture() {
        ui::applyThemeToApplication(*qApp, core::builtinTheme(core::ColorScheme::Light));
        window = std::make_unique<ui::MainWindow>(registry());
        window->applyTheme(core::builtinTheme(core::ColorScheme::Light));
        window->setMessageSink(
            [this](const QString& title, const QString& text) { messages << title + QStringLiteral(": ") + text; });
        window->setAskSave([](const QString&) { return ui::SaveChoice::Cancel; });
        window->resize(900, 700);
        window->show();
        (void)QTest::qWaitForWindowExposed(window.get());
    }

    [[nodiscard]] ui::EditorWidget* editor() const { return window->editor(); }
    [[nodiscard]] ui::DocumentSession* session() const { return window->session(); }

    QString writeFile(const QString& name, const QByteArray& bytes) const {
        const QString path = dir.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("cannot write fixture");
        f.write(bytes);
        return path;
    }

    static QByteArray readFile(const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return f.readAll();
    }

    /// Opens `path` and waits until it is fully loaded (or failed).
    bool open(const QString& path, int timeoutMs = 30000) {
        QSignalSpy done(session(), &ui::DocumentSession::loadFinished);
        QSignalSpy failed(session(), &ui::DocumentSession::loadFailed);
        window->openFile(path);
        return QTest::qWaitFor([&] { return !done.isEmpty() || !failed.isEmpty(); }, timeoutMs) && !done.isEmpty();
    }

    bool saveAndWait(const QString& path) {
        QSignalSpy done(session(), &ui::DocumentSession::saveFinished);
        session()->save(path);
        return QTest::qWaitFor([&] { return !done.isEmpty(); }, 30000);
    }

    /// Lets the lazy styler finish.
    void settle() const {
        (void)QTest::qWaitFor([&] { return editor()->styler().isComplete(); }, 30000);
        QCoreApplication::processEvents();
    }

    void placeCaret(int blockNumber, int column = 0) const {
        QTextCursor c(editor()->document()->findBlockByNumber(blockNumber));
        c.setPosition(c.block().position() + column);
        editor()->setTextCursor(c);
    }
};

/// The format applied at `column` of a block by the styler (merged over the block default).
inline QTextCharFormat formatAt(const QTextBlock& block, int column) {
    QTextCharFormat result;
    for (const auto& r : block.layout()->formats()) {
        if (column >= r.start && column < r.start + r.length) {
            result.merge(r.format);
        }
    }
    return result;
}

inline bool isCollapsed(const QTextBlock& block, int column) {
    return formatAt(block, column).intProperty(QTextFormat::FontPixelSize) == 1;
}

/// Numbers of the blocks that carry per-block metadata (QTextBlockUserData).
inline QList<int> blocksWithUserData(const QTextDocument* document) {
    QList<int> result;
    for (QTextBlock b = document->begin(); b.isValid(); b = b.next()) {
        if (b.userData() != nullptr) {
            result << b.blockNumber();
        }
    }
    return result;
}

/// The text a tooltip with `text` shows: its characters, or what its rich text shows when Qt takes
/// it for rich text (a no-break space shows as a space).
inline QString shownText(const QString& text) {
    return Qt::mightBeRichText(text) ? QTextDocumentFragment::fromHtml(text).toPlainText().replace(QChar::Nbsp, u' ')
                                     : text;
}

/// The window of the tooltip on screen, if one is.
inline QWidget* toolTipWindow() {
    for (QWidget* window : QApplication::topLevelWidgets()) {
        if (window->isVisible() && window->inherits("QTipLabel")) {
            return window;
        }
    }
    return nullptr;
}

/// A red image of 120 x 80 pixels at `path`.
inline bool writeRedImage(const QString& path) {
    QImage image(120, 80, QImage::Format_RGB32);
    image.fill(QColor(255, 0, 0));
    return image.save(path, "PNG");
}

/// How many pixels of `image` are pure red.
inline int redPixels(const QImage& image) {
    const QImage rgb = image.convertToFormat(QImage::Format_RGB32);
    int count = 0;
    for (int y = 0; y < rgb.height(); ++y) {
        for (int x = 0; x < rgb.width(); ++x) {
            count += rgb.pixel(x, y) == qRgb(255, 0, 0) ? 1 : 0;
        }
    }
    return count;
}

/// A theme colour as Qt's colour.
inline QColor qcolor(const core::Color& c) {
    return {c.r, c.g, c.b, c.a};
}

/// The colour of the editor's viewport at (x, y), as it is painted.
inline QColor pixel(ui::EditorWidget* editor, qreal x, qreal y) {
    return editor->viewport()->grab().toImage().pixelColor(static_cast<int>(x), static_cast<int>(y));
}

/// A Markdown document of at least `target` bytes: headings, paragraphs and lists in turn.
inline QByteArray largeDocument(qsizetype target) {
    QByteArray out;
    out.reserve(target + 512);
    int i = 0;
    while (out.size() < target) {
        ++i;
        out += QByteArray(1 + i % 3, '#') + " Section " + QByteArray::number(i) + "\n\n";
        out +=
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore.\n\n";
        out += "- first item of the list\n- second item of the list\n\n";
    }
    return out;
}

/// The lines of `document`, one per block.
inline std::vector<std::u16string> linesOf(const QTextDocument* document) {
    std::vector<std::u16string> lines;
    for (QTextBlock b = document->begin(); b.isValid(); b = b.next()) {
        lines.push_back(b.text().toStdU16String());
    }
    return lines;
}

/// Every line as the styler renders it and as it is formatted equals the whole-document reference
/// (ADR 0006); empty if so, else the first difference.
inline QString differenceFromTheReference(ui::EditorWidget* editor) {
    const auto* doc = editor->document();
    const QTextCursor cursor = editor->textCursor();
    const int anchor = doc->findBlock(cursor.anchor()).blockNumber();
    const auto reference = registry().renderDocument(linesOf(doc), core::revealedLines(anchor, cursor.blockNumber()));
    auto& styler = editor->styler();
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        auto got = styler.renderFor(b);
        auto want = reference.at(static_cast<std::size_t>(b.blockNumber()));
        if (!got.table && b.layout()->formats() != styler.formats().ranges(got)) {
            return QStringLiteral("line %1 \"%2\" is formatted for another render").arg(b.blockNumber()).arg(b.text());
        }
        got.blockState = {};
        want.blockState = {};
        if (!(got == want)) {
            return QStringLiteral(
                       "line %1 \"%2\" differs (heading %3/%4, break %5/%6, underline %7/%8, flows %9/%10, "
                       "hard break %11/%12, wrap indent %13/%14)")
                .arg(b.blockNumber())
                .arg(b.text())
                .arg(got.headingLevel)
                .arg(want.headingLevel)
                .arg(got.thematicBreak)
                .arg(want.thematicBreak)
                .arg(got.setextUnderline)
                .arg(want.setextUnderline)
                .arg(got.flowsToNext)
                .arg(want.flowsToNext)
                .arg(got.hardBreak)
                .arg(want.hardBreak)
                .arg(got.wrapIndent)
                .arg(want.wrapIndent);
        }
    }
    return {};
}

}  // namespace hitokoto::test
