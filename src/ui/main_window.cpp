// SPDX-License-Identifier: GPL-3.0-or-later
#include "main_window.hpp"

#include "hitokoto/core/settings.hpp"
#include "theme_applier.hpp"

#include <QApplication>
#include <QCoreApplication>

#include "application_icon.hpp"
#include "document_session.hpp"
#include "editor_widget.hpp"
#include "find_bar.hpp"
#include "find_controller.hpp"
#include "menu_bar.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>

namespace hitokoto::ui {
namespace {
const QString kFilter = QStringLiteral("Markdown (*.md *.markdown *.mdown *.mkd *.txt);;All files (*)");
}

MainWindow::MainWindow(const core::ElementRegistry& registry, QWidget* parent)
    : QMainWindow(parent),
      editor_(new EditorWidget(registry, this)),
      session_(new DocumentSession(editor_, registry, this)) {
    const QIcon icon = applicationIcon();
    QGuiApplication::setWindowIcon(icon);
    setWindowIcon(icon);
    // S028: the find bar below the editor.
    auto* central = new QWidget(this);
    auto* column = new QVBoxLayout(central);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(editor_, 1);
    findBar_ = new FindBar(central);
    column->addWidget(findBar_);
    setCentralWidget(central);
    find_ = new FindController(editor_, findBar_, registry, this);
    menuBar_ = new MenuBar(this);
    setMenuBar(menuBar_);
    createMenus();
    connect(editor_, &EditorWidget::themeEntrySelected, this, &MainWindow::applyThemeEntry);
    connect(editor_, &EditorWidget::savedThemeLoaded, this, &MainWindow::applyTheme);
    // S039: problems of config.toml, shown once when it is read, like a broken theme.
    connect(editor_, &EditorWidget::settingsProblemsFound, this,
            [this](const QString& path, const QStringList& problems) {
                showMessage(tr("Settings error"), tr("%1 has errors; the settings concerned keep their defaults.\n\n%2")
                                                      .arg(path, problems.join(QLatin1Char('\n'))));
            });
    connect(editor_, &EditorWidget::settingsLoaded, this, [this](const core::Settings& settings) {
        options_.reportBlankLinesAdded = settings.reportBlankLinesAdded;
        options_.frontMatterAsProperties = settings.frontMatterAsProperties;
        options_.smoothScrolling = settings.smoothScrolling;
    });

    askSave_ = [this](const QString& name) {
        const auto answer =
            QMessageBox::question(this, tr("Unsaved changes"), tr("Save changes to %1 before closing it?").arg(name),
                                  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
        if (answer == QMessageBox::Save)
            return SaveChoice::Save;
        if (answer == QMessageBox::Discard)
            return SaveChoice::Discard;
        return SaveChoice::Cancel;
    };
    message_ = [this](const QString& title, const QString& text) {
        auto* box = new QMessageBox(QMessageBox::Warning, title, text, QMessageBox::Ok, this);
        box->setTextFormat(Qt::PlainText);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->open();
    };
    pickPath_ = [this](bool forSaving, const QString& current) {
        return forSaving ? QFileDialog::getSaveFileName(this, tr("Save As"), current, kFilter)
                         : QFileDialog::getOpenFileName(this, tr("Open"), current, kFilter);
    };

    connect(editor_->document(), &QTextDocument::modificationChanged, this, &QWidget::setWindowModified);
    connect(session_, &DocumentSession::pathChanged, this, [this] { updateTitle(); });
    connect(session_, &DocumentSession::loadFailed, this,
            [this](const QString&, const QString& message) { showMessage(tr("Cannot open file"), message); });
    connect(session_, &DocumentSession::blankLinesAdded, this, [this](int count) {
        if (!options_.reportBlankLinesAdded) {
            return;  // the modified mark says it (S018); the message is a feature toggle, off
        }
        showMessage(tr("Blank lines added"),
                    tr("%n blank line(s) added below tables, so that text written directly below a table "
                       "stays a paragraph as in other editors. Save to keep the change.",
                       nullptr, count));
    });
    connect(session_, &DocumentSession::saveFailed, this,
            [this](const QString&, const QString& message) { showMessage(tr("Cannot save file"), message); });

    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        const QSize avail = screen->availableSize();
        resize(std::min(avail.width() * 2 / 3, 1100),
               std::min(avail.height() * 4 / 5, 900));  // theme-lint: allow (window geometry, not styling)
    }
    updateTitle();
    editor_->setFocus();
}

MainWindow::~MainWindow() = default;

void MainWindow::createMenus() {
    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&New"), QKeySequence::New, this, &MainWindow::newFile);
    file->addAction(tr("&Open…"), QKeySequence::Open, this, &MainWindow::open);
    file->addSeparator();
    file->addAction(tr("&Save"), QKeySequence::Save, this, &MainWindow::save);
    file->addAction(tr("Save &As…"), QKeySequence::SaveAs, this, &MainWindow::saveAs);
    file->addSeparator();
    QAction* quit = file->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);
    quit->setMenuRole(QAction::QuitRole);

    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(tr("&Undo"), QKeySequence::Undo, editor_, &EditorWidget::undoEdit);
    edit->addAction(tr("&Redo"), QKeySequence::Redo, editor_, &EditorWidget::redoEdit);
    edit->addSeparator();
    edit->addAction(tr("Cu&t"), QKeySequence::Cut, editor_, &QTextEdit::cut);
    edit->addAction(tr("&Copy"), QKeySequence::Copy, editor_, &QTextEdit::copy);
    edit->addAction(tr("&Paste"), QKeySequence::Paste, editor_, &QTextEdit::paste);
    // S031: paste the clipboard's plain text without converting tables (decision 5).
    // The key reaches the window from every widget: it acts where the focus is (review O2), in a
    // cell editor or a text field as Ctrl+V there (both take plain text only), in the editor
    // without converting tables, elsewhere not at all.
    QAction* pastePlain =
        edit->addAction(tr("Paste as &Plain Text"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), this, [this] {
            QWidget* focus = QApplication::focusWidget();
            if (focus == nullptr || focus == editor_ || focus == editor_->viewport()) {
                editor_->pasteAsPlainText();
            } else if (auto* line = qobject_cast<QLineEdit*>(focus)) {
                line->paste();
            } else if (auto* text = qobject_cast<QPlainTextEdit*>(focus)) {
                text->paste();
            }
        });
    pastePlain->setObjectName(QStringLiteral("pasteAsPlainText"));
    const auto updatePastePlain = [pastePlain] { pastePlain->setEnabled(EditorWidget::canPastePlainText()); };
    connect(QApplication::clipboard(), &QClipboard::dataChanged, pastePlain, updatePastePlain);
    updatePastePlain();
    edit->addAction(tr("Select &All"), QKeySequence::SelectAll, editor_, &QTextEdit::selectAll);
    // S028: find in the file (decision 7; review O10: the platform's find-next keys plus F3).
    edit->addSeparator();
    edit->addAction(tr("&Find…"), QKeySequence::Find, find_, &FindController::open);
    QAction* findNext = edit->addAction(tr("Find &Next"), find_, &FindController::next);
    QAction* findPrevious = edit->addAction(tr("Find Pre&vious"), find_, &FindController::previous);
    QList<QKeySequence> nextKeys = QKeySequence::keyBindings(QKeySequence::FindNext);
    QList<QKeySequence> previousKeys = QKeySequence::keyBindings(QKeySequence::FindPrevious);
    if (!nextKeys.contains(QKeySequence(Qt::Key_F3))) {
        nextKeys << QKeySequence(Qt::Key_F3);
    }
    if (!previousKeys.contains(QKeySequence(Qt::SHIFT | Qt::Key_F3))) {
        previousKeys << QKeySequence(Qt::SHIFT | Qt::Key_F3);
    }
    findNext->setShortcuts(nextKeys);
    findPrevious->setShortcuts(previousKeys);

    QMenu* view = menuBar()->addMenu(tr("&View"));
    view->addAction(tr("&Reload Theme"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R), this,
                    &MainWindow::reloadThemeRequested);
}

void MainWindow::applyTheme(const core::Theme& theme) {
    menuBar_->applyStyle(core::menuBarStyle(theme));
    editor_->applyTheme(theme);
    find_->applyTheme(theme);
}

void MainWindow::applyThemeEntry(const ThemeEntry& entry) {
    const auto scheme = ThemeManager::systemScheme();
    core::ThemeLoadResult result{core::builtinTheme(entry.builtinScheme.value_or(scheme)), false, {}};
    if (!entry.builtinScheme.has_value()) {
        result = ThemeManager::loadThemeFile(entry.path, scheme);
        if (result.usedFallback) {
            const QStringList lines = ThemeManager::formatDiagnostics(result);
            showMessage(tr("Theme error"),
                        tr("Could not load \"%1\":\n%2").arg(entry.path, lines.join(QLatin1Char('\n'))));
        }
    }
    if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        applyThemeToApplication(*app, result.theme);
    }
    applyTheme(result.theme);
    if (result.usedFallback) {
        editor_->setActiveThemePath(QString());
    } else {
        QString error;
        if (!ThemeManager::saveSelectedTheme(editor_->themeConfigDir(), entry, &error)) {
            reportThemeNotSaved(error);
        }
        emit themeEntryChosen(entry);
    }
}

void MainWindow::reportThemeNotSaved(const QString& reason) {
    const QString path = ThemeManager::configFilePath(editor_->themeConfigDir());
    QString text = tr("The theme is used for now, but it could not be saved in %1: %2").arg(path, reason);
    if (!path.isEmpty()) {
        const QStringList problems = ThemeManager::formatSettingsDiagnostics(
            core::loadSettingsFile(std::filesystem::path(path.toStdU16String())));
        if (!problems.isEmpty()) {
            text += QStringLiteral("\n\n") + problems.join(QLatin1Char('\n'));
        }
    }
    showMessage(tr("Theme not saved"), text);
}

void MainWindow::showMessage(const QString& title, const QString& text) {
    if (message_)
        message_(title, text);
}

QString MainWindow::documentName() const {
    return session_->path().isEmpty() ? tr("Untitled") : QFileInfo(session_->path()).fileName();
}

void MainWindow::updateTitle() {
    setWindowTitle(QStringLiteral("%1[*] — hitokoto").arg(documentName()));
    setWindowModified(editor_->document()->isModified());
}

bool MainWindow::maybeSave() {
    // While a file is loading the editor is read-only: there is nothing of the user's to lose.
    if (session_->isLoading() || !editor_->document()->isModified()) {
        return true;
    }
    switch (askSave_(documentName())) {
        case SaveChoice::Save: {
            QString path = session_->path();
            if (path.isEmpty()) {
                path = pickPath_(true, QString());
                if (path.isEmpty())
                    return false;
            }
            return saveBlocking(path);
        }
        case SaveChoice::Discard:
            return true;
        case SaveChoice::Cancel:
            return false;
    }
    return false;
}

bool MainWindow::saveBlocking(const QString& path) {
    QString error;
    if (!session_->saveNow(path, &error)) {
        showMessage(tr("Cannot save file"), error);
        return false;
    }
    return true;
}

void MainWindow::newFile() {
    if (maybeSave()) {
        session_->newDocument();
    }
}

void MainWindow::openFile(const QString& path) {
    if (maybeSave()) {
        session_->load(path);
    }
}

void MainWindow::open() {
    if (!maybeSave()) {
        return;
    }
    const QString path = pickPath_(false, session_->path());
    if (!path.isEmpty()) {
        session_->load(path);
    }
}

bool MainWindow::save() {
    if (session_->path().isEmpty()) {
        return saveAs();
    }
    session_->save(session_->path());
    return true;
}

bool MainWindow::saveAs() {
    const QString path = pickPath_(true, session_->path());
    if (path.isEmpty()) {
        return false;
    }
    session_->save(path);
    return true;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    session_->waitForIdle();
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::setOptions(const Options& options) {
    options_ = options;
    editor_->setFrontMatterAsProperties(options_.frontMatterAsProperties);
    editor_->setSmoothScrolling(options_.smoothScrolling);
}

}  // namespace hitokoto::ui
