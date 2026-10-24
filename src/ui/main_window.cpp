// SPDX-License-Identifier: GPL-3.0-or-later
#include "main_window.hpp"

#include "document_session.hpp"
#include "editor_widget.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMenuBar>
#include <QMessageBox>
#include <QScreen>

#include <algorithm>

namespace hitokoto::ui {
namespace {
const QString kFilter = QStringLiteral("Markdown (*.md *.markdown *.mdown *.mkd *.txt);;All files (*)");
}

MainWindow::MainWindow(const core::ElementRegistry& registry, QWidget* parent)
    : QMainWindow(parent), editor_(new EditorWidget(registry, this)), session_(new DocumentSession(editor_, this)) {
    setCentralWidget(editor_);
    createMenus();

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
    edit->addAction(tr("&Undo"), QKeySequence::Undo, editor_, &QPlainTextEdit::undo);
    edit->addAction(tr("&Redo"), QKeySequence::Redo, editor_, &QPlainTextEdit::redo);
    edit->addSeparator();
    edit->addAction(tr("Cu&t"), QKeySequence::Cut, editor_, &QPlainTextEdit::cut);
    edit->addAction(tr("&Copy"), QKeySequence::Copy, editor_, &QPlainTextEdit::copy);
    edit->addAction(tr("&Paste"), QKeySequence::Paste, editor_, &QPlainTextEdit::paste);
    edit->addAction(tr("Select &All"), QKeySequence::SelectAll, editor_, &QPlainTextEdit::selectAll);

    QMenu* view = menuBar()->addMenu(tr("&View"));
    view->addAction(tr("&Reload Theme"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R), this,
                    &MainWindow::reloadThemeRequested);
}

void MainWindow::applyTheme(const core::Theme& theme) {
    editor_->applyTheme(theme);
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

}  // namespace hitokoto::ui
