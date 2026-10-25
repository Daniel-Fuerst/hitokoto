// SPDX-License-Identifier: GPL-3.0-or-later
// Helpers shared by the UI tests (headless, QT_QPA_PLATFORM=offscreen).
#pragma once

#include "document_session.hpp"
#include "editor_widget.hpp"
#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"
#include "main_window.hpp"
#include "theme_applier.hpp"

#include <QApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
#include <QTextLayout>

#include <memory>

namespace hitokoto::test {

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

}  // namespace hitokoto::test
