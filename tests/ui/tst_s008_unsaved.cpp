// SPDX-License-Identifier: GPL-3.0-or-later
// S008: unsaved changes are never lost silently.
#include "ui_test_support.hpp"

#include <QBuffer>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

#include <optional>

using namespace hitokoto;

namespace {

/// What the question about unsaved changes showed.
struct Shown {
    bool found = false;
    Qt::TextFormat format = Qt::AutoText;
    QString title;
    QString text;
    QMessageBox::Icon icon = QMessageBox::NoIcon;
    QMessageBox::StandardButtons buttons;
    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton;
    int redPixels = -1;
};

/// Answers the next modal message box with `answer` (or Escape) and records what it showed.
class Answerer {
public:
    explicit Answerer(std::optional<QMessageBox::StandardButton> answer) : answer_(answer) {
        poll_.setInterval(5);
        QObject::connect(&poll_, &QTimer::timeout, &poll_, [this] { look(); });
        poll_.start();
    }
    [[nodiscard]] const Shown& shown() const { return shown_; }

private:
    void look() {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (box == nullptr || !box->isVisible()) {
            return;
        }
        poll_.stop();
        shown_.found = true;
        shown_.format = box->textFormat();
        shown_.title = box->windowTitle();
        shown_.text = box->text();
        shown_.icon = box->icon();
        shown_.buttons = box->standardButtons();
        shown_.defaultButton = box->standardButton(box->defaultButton());
        shown_.redPixels = test::redPixels(box->grab().toImage());
        if (answer_) {
            box->button(*answer_)->click();
        } else {
            test::keyClick(box, Qt::Key_Escape);
        }
    }

    std::optional<QMessageBox::StandardButton> answer_;
    QTimer poll_;
    Shown shown_;
};

/// A file name that holds a red picture of 60 x 40 pixels as a data URL in an image tag: Qt draws
/// the picture where it takes the name for rich text. A name cannot hold a slash; the URL spells
/// it %2F, which Qt decodes.
QString nameWithPicture() {
    QImage image(60, 40, QImage::Format_RGB32);
    image.fill(QColor(255, 0, 0));
    QBuffer png;
    png.open(QIODevice::WriteOnly);
    image.save(&png, "PNG");
    const QString base64 = QString::fromLatin1(png.data().toBase64()).replace(u'/', QStringLiteral("%2F"));
    return QStringLiteral("<img src=\"data:image%2Fpng;base64,") + base64 + QStringLiteral("\">.md");
}

}  // namespace

class tst_s008_unsaved : public QObject {
    Q_OBJECT
private slots:
    void modifiedMarkerFollowsEditsAndSaves() {
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("a.md"), "x");
        QVERIFY(f.open(path));
        QVERIFY(!f.window->isWindowModified());
        test::keyClick(f.editor(), Qt::Key_End);
        test::keyClicks(f.editor(), QStringLiteral("y"));
        QVERIFY(f.window->isWindowModified());
        QVERIFY(f.saveAndWait(path));
        QVERIFY(!f.window->isWindowModified());
    }

    void undoBackToSavedStateClearsMarker() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("a.md"), "x")));
        test::keyClicks(f.editor(), QStringLiteral("abc"));
        QVERIFY(f.window->isWindowModified());
        f.editor()->undo();
        QVERIFY(!f.window->isWindowModified());
    }

    void styleChangesDoNotCountAsModifications() {
        test::Fixture f;
        QVERIFY(f.open(f.writeFile(QStringLiteral("a.md"), "# a\n- b\n## c\n")));
        for (int i = 0; i < 3; ++i)
            f.placeCaret(i);
        f.settle();
        QVERIFY(!f.editor()->document()->isModified());
    }

    void cancelKeepsWindowOpenAndText() {
        test::Fixture f;
        test::keyClicks(f.editor(), QStringLiteral("draft"));
        int asked = 0;
        f.window->setAskSave([&](const QString&) {
            ++asked;
            return ui::SaveChoice::Cancel;
        });
        QVERIFY(!f.window->close());
        QCOMPARE(asked, 1);
        QVERIFY(f.window->isVisible());
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("draft"));
    }

    void discardCloses() {
        test::Fixture f;
        test::keyClicks(f.editor(), QStringLiteral("draft"));
        f.window->setAskSave([](const QString&) { return ui::SaveChoice::Discard; });
        QVERIFY(f.window->close());
    }

    void saveChoiceWritesBeforeClosing() {
        test::Fixture f;
        const QString path = f.writeFile(QStringLiteral("a.md"), "");
        QVERIFY(f.open(path));
        test::keyClicks(f.editor(), QStringLiteral("keep"));
        f.window->setAskSave([](const QString&) { return ui::SaveChoice::Save; });
        QVERIFY(f.window->close());
        QCOMPARE(test::Fixture::readFile(path), QByteArray("keep"));
    }

    void newDuringLoadKeepsUndoWorking() {
        test::Fixture f;
        QByteArray big;
        for (int i = 0; i < 200000; ++i)
            big += "- line of a large list\n";
        f.session()->load(f.writeFile(QStringLiteral("big.md"), big));
        QTRY_VERIFY(f.editor()->document()->blockCount() > 1);  // first chunk is in
        f.window->newFile();
        QVERIFY(!f.session()->isLoading());
        QVERIFY(f.editor()->document()->isEmpty());
        test::keyClicks(f.editor(), QStringLiteral("abc"));
        QVERIFY(f.editor()->document()->isUndoAvailable());
        f.editor()->undo();
        QVERIFY(f.editor()->document()->isEmpty());
        QTest::qWait(100);
        QVERIFY2(f.editor()->document()->isEmpty(), "no late chunks of the abandoned load");
    }

    void openingAnotherFileAsksFirst() {
        test::Fixture f;
        test::keyClicks(f.editor(), QStringLiteral("draft"));
        int asked = 0;
        f.window->setAskSave([&](const QString&) {
            ++asked;
            return ui::SaveChoice::Cancel;
        });
        f.window->openFile(f.writeFile(QStringLiteral("other.md"), "other"));
        QCOMPARE(asked, 1);
        QTest::qWait(50);
        QCOMPARE(f.editor()->toPlainText(), QStringLiteral("draft"));
    }

    // Rework of 2026-11-21. Scenario: the question shows the name as its characters. A name that
    // looks like markup shows as written; Qt draws nothing from it.
    void theQuestionShowsTheNameAsItsCharacters_data() {
        QTest::addColumn<QString>("name");
        QTest::newRow("tag") << QStringLiteral("<b>notes</b>.md");
        QTest::newRow("picture") << nameWithPicture();
        QTest::newRow("plain") << QStringLiteral("notes.md");
    }

    void theQuestionShowsTheNameAsItsCharacters() {
        QFETCH(QString, name);
        QWidget parent;
        parent.show();
        const Answerer answerer(QMessageBox::Cancel);
        QCOMPARE(ui::MainWindow::askSaveInDialog(&parent, name), ui::SaveChoice::Cancel);
        const Shown& shown = answerer.shown();
        QVERIFY(shown.found);
        QCOMPARE(shown.text, QStringLiteral("Save changes to %1 before closing it?").arg(name));
        QCOMPARE(shown.redPixels, 0);
        QCOMPARE(shown.format, Qt::PlainText);
        QCOMPARE(shown.title, QStringLiteral("Unsaved changes"));
        QCOMPARE(shown.icon, QMessageBox::Question);
        QCOMPARE(shown.buttons, QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        QCOMPARE(shown.defaultButton, QMessageBox::Save);
    }

    // The answers as before: each button, and Escape cancels.
    void theQuestionAnswersAsBefore_data() {
        QTest::addColumn<int>("button");
        QTest::addColumn<int>("choice");
        QTest::newRow("Save") << int(QMessageBox::Save) << int(ui::SaveChoice::Save);
        QTest::newRow("Discard") << int(QMessageBox::Discard) << int(ui::SaveChoice::Discard);
        QTest::newRow("Cancel") << int(QMessageBox::Cancel) << int(ui::SaveChoice::Cancel);
        QTest::newRow("Escape") << 0 << int(ui::SaveChoice::Cancel);
    }

    void theQuestionAnswersAsBefore() {
        QFETCH(int, button);
        QFETCH(int, choice);
        QWidget parent;
        parent.show();
        const Answerer answerer(button == 0 ? std::nullopt
                                            : std::optional(static_cast<QMessageBox::StandardButton>(button)));
        QCOMPARE(int(ui::MainWindow::askSaveInDialog(&parent, QStringLiteral("notes.md"))), choice);
        QVERIFY(answerer.shown().found);
    }

    // The window asks with this question when a document whose name looks like markup is closed.
    // Windows allows no < and > in a name.
    void closingADocumentWhoseNameLooksLikeMarkupShowsItsName() {
#ifdef Q_OS_WIN
        QSKIP("Windows allows no < and > in a file name");
#else
        QTemporaryDir dir;
        const QString name = nameWithPicture();
        QFile file(dir.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("x");
        file.close();
        ui::MainWindow window(test::registry());
        window.setMessageSink([](const QString&, const QString&) {});
        window.show();
        QSignalSpy loaded(window.session(), &ui::DocumentSession::loadFinished);
        window.openFile(file.fileName());
        QTRY_COMPARE(loaded.size(), 1);
        test::keyClicks(window.editor(), QStringLiteral("draft"));
        const Answerer answerer(QMessageBox::Cancel);
        QVERIFY(!window.close());
        QVERIFY(answerer.shown().found);
        QCOMPARE(answerer.shown().text, QStringLiteral("Save changes to %1 before closing it?").arg(name));
        QCOMPARE(answerer.shown().redPixels, 0);
        QCOMPARE(answerer.shown().format, Qt::PlainText);
        window.setAskSave([](const QString&) { return ui::SaveChoice::Discard; });
#endif
    }
};

QTEST_MAIN(tst_s008_unsaved)
#include "tst_s008_unsaved.moc"
