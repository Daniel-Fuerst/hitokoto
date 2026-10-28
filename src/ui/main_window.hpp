// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hitokoto/core/markdown.hpp"
#include "hitokoto/core/theme.hpp"
#include "options.hpp"

#include <QMainWindow>

#include <functional>

namespace hitokoto::ui {

class EditorWidget;
class DocumentSession;

/// Answer to "save changes?" (S008).
enum class SaveChoice { Save, Discard, Cancel };

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const core::ElementRegistry& registry, QWidget* parent = nullptr);
    ~MainWindow() override;
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    [[nodiscard]] EditorWidget* editor() const noexcept { return editor_; }
    [[nodiscard]] DocumentSession* session() const noexcept { return session_; }

    // Test seams: replace the dialogs.
    using AskSaveFn = std::function<SaveChoice(const QString& documentName)>;
    using MessageFn = std::function<void(const QString& title, const QString& text)>;
    using PickPathFn = std::function<QString(bool forSaving, const QString& current)>;
    void setAskSave(AskSaveFn fn) { askSave_ = std::move(fn); }
    void setMessageSink(MessageFn fn) { message_ = std::move(fn); }
    void setPathPicker(PickPathFn fn) { pickPath_ = std::move(fn); }

    /// Options (no options dialog yet: the defaults in options.hpp apply).
    [[nodiscard]] const Options& options() const noexcept { return options_; }
    void setOptions(const Options& options) { options_ = options; }

    void applyTheme(const core::Theme& theme);
    void showMessage(const QString& title, const QString& text);

    /// File actions; they ask about unsaved changes first where needed.
    void newFile();
    void openFile(const QString& path);
    void open();  ///< asks for a path
    bool save();  ///< background save; false if cancelled
    bool saveAs();

    /// Ask about unsaved changes. Returns false if the user cancelled.
    bool maybeSave();

signals:
    void reloadThemeRequested();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createMenus();
    void updateTitle();
    [[nodiscard]] QString documentName() const;
    bool saveBlocking(const QString& path);

    EditorWidget* editor_;
    DocumentSession* session_;
    AskSaveFn askSave_;
    MessageFn message_;
    PickPathFn pickPath_;
    Options options_;
};

}  // namespace hitokoto::ui
