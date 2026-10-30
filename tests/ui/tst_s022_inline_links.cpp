// SPDX-License-Identifier: GPL-3.0-or-later
// S022: inline Markdown links render with concealed markers and styled text, byte-exact source preserved.
#include "ui_test_support.hpp"

#include <QDesktopServices>
#include <QUrl>

using namespace hitokoto;

namespace {
QColor qcolor(const core::Color& c) {
    return {c.r, c.g, c.b, c.a};
}

class UrlCollector : public QObject {
    Q_OBJECT
public:
    QUrl lastUrl;
public slots:
    void handleUrl(const QUrl& url) { lastUrl = url; }
};
}  // namespace

class tst_s022_inline_links : public QObject {
    Q_OBJECT
private slots:
    void userExampleRendersProperly() {
        constexpr auto content =
            "### 6.1 Inline Links & Titles\n"
            "* [Standard Link](https://example.com)\n"
            "* [Link with Title](https://example.com \"Hover over me to see the title!\")\n"
            "* [Link with Parentheses in URL](https://en.wikipedia.org/wiki/Markdown_(markup_language))\n"
            "* [Link with Spaces in URL (Angle Brackets)](<https://example.com/path with spaces/index.html>)\n"
            "* [Link with **Bold**, *Italics*, and `Code` Inside Brackets](https://example.com)\n";

        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("links.md"), content);
        QVERIFY(f.open(path));

        // Place caret on line 0 so lines 1..5 are concealed.
        f.placeCaret(0);
        f.settle();

        const auto doc = f.editor()->document();
        const auto theme = core::builtinTheme(core::ColorScheme::Light);

        // Line 1: * [Standard Link](https://example.com)
        {
            const auto block = doc->findBlockByNumber(1);
            // '[' is at 2
            QVERIFY(test::isCollapsed(block, 2));
            // "Standard Link" is at 3..15
            const auto fmt = test::formatAt(block, 3);
            QVERIFY(!test::isCollapsed(block, 3));
            QCOMPARE(fmt.foreground().color(), qcolor(theme.accent));
            QVERIFY(fmt.isAnchor());
            QCOMPARE(fmt.anchorHref(), QStringLiteral("https://example.com"));
            // '](https://example.com)' at 16..37 is collapsed
            QVERIFY(test::isCollapsed(block, 16));
        }

        // Line 2: * [Link with Title](https://example.com "Hover over me to see the title!")
        {
            const auto block = doc->findBlockByNumber(2);
            QVERIFY(test::isCollapsed(block, 2));
            const auto fmt = test::formatAt(block, 3);
            QVERIFY(!test::isCollapsed(block, 3));
            QCOMPARE(fmt.foreground().color(), qcolor(theme.accent));
            QVERIFY(fmt.isAnchor());
            QCOMPARE(fmt.anchorHref(), QStringLiteral("https://example.com"));
            QCOMPARE(fmt.toolTip(), QStringLiteral("Hover over me to see the title!"));
            // Tail is collapsed
            QVERIFY(test::isCollapsed(block, 18));
        }

        // Line 3: * [Link with Parentheses in URL](https://en.wikipedia.org/wiki/Markdown_(markup_language))
        {
            const auto block = doc->findBlockByNumber(3);
            QVERIFY(test::isCollapsed(block, 2));
            const auto fmt = test::formatAt(block, 3);
            QVERIFY(!test::isCollapsed(block, 3));
            QCOMPARE(fmt.anchorHref(), QStringLiteral("https://en.wikipedia.org/wiki/Markdown_(markup_language)"));
            // Tail is collapsed
            QVERIFY(test::isCollapsed(block, 31));
        }

        // Line 4: * [Link with Spaces in URL (Angle Brackets)](<https://example.com/path with spaces/index.html>)
        {
            const auto block = doc->findBlockByNumber(4);
            QVERIFY(test::isCollapsed(block, 2));
            const auto fmt = test::formatAt(block, 3);
            QVERIFY(!test::isCollapsed(block, 3));
            QCOMPARE(fmt.anchorHref(), QStringLiteral("https://example.com/path with spaces/index.html"));
            // Tail is collapsed
            QVERIFY(test::isCollapsed(block, 43));
        }

        // Line 5: * [Link with **Bold**, *Italics*, and `Code` Inside Brackets](https://example.com)
        {
            const auto block = doc->findBlockByNumber(5);
            // '[' is collapsed
            QVERIFY(test::isCollapsed(block, 2));
            // "Link with " has anchor
            const auto fmt0 = test::formatAt(block, 3);
            QVERIFY(fmt0.isAnchor());
            QCOMPARE(fmt0.anchorHref(), QStringLiteral("https://example.com"));

            // Find **Bold**
            const int boldPos = block.text().indexOf(QStringLiteral("Bold"));
            QVERIFY(boldPos > 0);
            const auto boldFmt = test::formatAt(block, boldPos);
            QVERIFY(boldFmt.fontWeight() >= QFont::Bold);
            QVERIFY(boldFmt.isAnchor());
            QCOMPARE(boldFmt.anchorHref(), QStringLiteral("https://example.com"));

            // Find *Italics*
            const int italicPos = block.text().indexOf(QStringLiteral("Italics"));
            QVERIFY(italicPos > 0);
            const auto italicFmt = test::formatAt(block, italicPos);
            QVERIFY(italicFmt.fontItalic());
            QVERIFY(italicFmt.isAnchor());
            QCOMPARE(italicFmt.anchorHref(), QStringLiteral("https://example.com"));

            // Find `Code`
            const int codePos = block.text().indexOf(QStringLiteral("Code"));
            QVERIFY(codePos > 0);
            const auto codeFmt = test::formatAt(block, codePos);
            QVERIFY(codeFmt.isAnchor());
            QCOMPARE(codeFmt.anchorHref(), QStringLiteral("https://example.com"));

            // Tail is collapsed
            const int tailPos = block.text().indexOf(QStringLiteral("](https://example.com)"));
            QVERIFY(tailPos > 0);
            QVERIFY(test::isCollapsed(block, tailPos));
        }

        // Saving preserves the byte-exact original Markdown.
        const QString savePath = f.dir.filePath(QStringLiteral("saved.md"));
        QVERIFY(f.saveAndWait(savePath));
        QCOMPARE(test::Fixture::readFile(savePath), QByteArray(content));
    }

    void revealedLineShowsDelimitersWithMarkerColor() {
        constexpr auto content = "* [Standard Link](https://example.com)\nsecond line\n";
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("links.md"), content)));
        // Place caret on line 0 to reveal markers.
        f.placeCaret(0);
        f.settle();

        const auto block0 = f.editor()->document()->firstBlock();
        const auto theme = core::builtinTheme(core::ColorScheme::Light);

        // '[' at index 2 is revealed with marker color
        QVERIFY(!test::isCollapsed(block0, 2));
        QCOMPARE(test::formatAt(block0, 2).foreground().color(), qcolor(theme.marker));

        // Link text at index 3 is styled with accent/link text color
        QCOMPARE(test::formatAt(block0, 3).foreground().color(), qcolor(theme.accent));

        // ']' at index 16 is revealed with marker color
        QVERIFY(!test::isCollapsed(block0, 16));
        QCOMPARE(test::formatAt(block0, 16).foreground().color(), qcolor(theme.marker));
    }

    void customThemeLinkTokensApply() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("link.md"), "[Link](https://example.com)\nsecond\n")));
        f.placeCaret(0);

        auto theme = core::builtinTheme(core::ColorScheme::Light);
        theme.link.text = core::Color{0x12, 0x34, 0x56, 0xff};
        theme.link.marker = core::Color{0x78, 0x9a, 0xbc, 0xff};
        f.window->applyTheme(theme);
        f.settle();

        const auto block0 = f.editor()->document()->firstBlock();
        // '[' at 0 has marker color
        QCOMPARE(test::formatAt(block0, 0).foreground().color(), qcolor(*theme.link.marker));
        // Link text at 1 has link text color
        QCOMPARE(test::formatAt(block0, 1).foreground().color(), qcolor(*theme.link.text));
        // ']' at 5 has marker color
        QCOMPARE(test::formatAt(block0, 5).foreground().color(), qcolor(*theme.link.marker));
    }

    void hoverCursorAndClickOpenLink() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("test.md"), "plain text before\n[Link](https://example.com)\n")));
        // Caret on line 0 so line 1 is concealed
        f.placeCaret(0);
        f.settle();

        // Initial cursor on viewport is I-Beam
        QCOMPARE(f.editor()->viewport()->cursor().shape(), Qt::IBeamCursor);

        // Point on plain text (line 0, col 5)
        QTextCursor c0(f.editor()->document());
        c0.setPosition(5);
        const QPoint plainPt = f.editor()->cursorRect(c0).center();
        QMouseEvent movePlain(QEvent::MouseMove, plainPt, f.editor()->viewport()->mapToGlobal(plainPt), Qt::NoButton,
                              Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(f.editor()->viewport(), &movePlain);
        QCOMPARE(f.editor()->viewport()->cursor().shape(), Qt::IBeamCursor);

        // Point inside concealed link on line 1:
        // "[Link](https://example.com)" starts at position 18.
        // Index 18 is '[', index 19 is 'L', index 20 is 'i'
        QTextCursor c1(f.editor()->document());
        c1.setPosition(20);
        const QPoint linkPt = f.editor()->cursorRect(c1).center();
        QCOMPARE(f.editor()->anchorAt(linkPt), QStringLiteral("https://example.com"));

        // Hovering over the link turns cursor into pointing hand
        QMouseEvent moveLink(QEvent::MouseMove, linkPt, f.editor()->viewport()->mapToGlobal(linkPt), Qt::NoButton,
                             Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(f.editor()->viewport(), &moveLink);
        QCOMPARE(f.editor()->viewport()->cursor().shape(), Qt::PointingHandCursor);

        // Moving back to plain text restores I-Beam
        QCoreApplication::sendEvent(f.editor()->viewport(), &movePlain);
        QCOMPARE(f.editor()->viewport()->cursor().shape(), Qt::IBeamCursor);

        // Clicking the link opens it in the browser
        UrlCollector collector;
        QDesktopServices::setUrlHandler(QStringLiteral("https"), &collector, "handleUrl");

        QTest::mouseClick(f.editor()->viewport(), Qt::LeftButton, Qt::NoModifier, linkPt);
        QCOMPARE(collector.lastUrl, QUrl(QStringLiteral("https://example.com")));

        // Line 1 remains concealed after clicking the link
        const auto block1 = f.editor()->document()->findBlockByNumber(1);
        QVERIFY(test::isCollapsed(block1, 0));  // '[' is collapsed

        QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
    }

    void hoverTooltipShowsTitle() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("tip.md"), "[Link](https://example.com \"My Title\")\nsecond\n")));
        f.placeCaret(1);
        f.settle();

        QTextCursor c(f.editor()->document());
        c.setPosition(2);
        const QPoint linkPt = f.editor()->cursorRect(c).center();
        const auto fmt = f.editor()->formatAt(linkPt);
        QCOMPARE(fmt.toolTip(), QStringLiteral("My Title"));

        QHelpEvent help(QEvent::ToolTip, linkPt, f.editor()->viewport()->mapToGlobal(linkPt));
        QVERIFY(QCoreApplication::sendEvent(f.editor()->viewport(), &help));
    }
};

QTEST_MAIN(tst_s022_inline_links)
#include "tst_s022_inline_links.moc"
