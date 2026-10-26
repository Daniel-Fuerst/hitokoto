// SPDX-License-Identifier: GPL-3.0-or-later
// Loading and saving the one open document. Reading, decoding, encoding and writing run on a
// worker thread; the text is inserted into the editor in short slices so the UI stays
// responsive with large files (S001, S004, S007).
#pragma once

#include "hitokoto/core/text_codec.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QTimer>

#include <vector>

namespace hitokoto::ui {

class EditorWidget;

/// How the file looked on disk; used to write it back the same way (ADR 0002).
struct FileFormat {
    bool hasBom = false;
    core::LineEnding ending = core::LineEnding::LF;  ///< all breaks, or the majority for mixed files
    bool mixedEndings = false;                       ///< endings other than the majority live in BlockData
};

class DocumentSession : public QObject {
    Q_OBJECT
public:
    explicit DocumentSession(EditorWidget* editor, QObject* parent = nullptr);
    ~DocumentSession() override;
    DocumentSession(const DocumentSession&) = delete;
    DocumentSession& operator=(const DocumentSession&) = delete;

    [[nodiscard]] QString path() const { return path_; }
    [[nodiscard]] const FileFormat& format() const noexcept { return format_; }
    [[nodiscard]] bool isLoading() const noexcept { return loading_; }
    [[nodiscard]] bool isSaving() const noexcept { return savesInFlight_ > 0; }

    /// Empty, untitled document.
    void newDocument();
    /// Starts loading `path`; emits loadFinished() or loadFailed(). The current document stays
    /// untouched if the file cannot be read or decoded.
    void load(const QString& path);
    /// Saves to `path` in the background; emits saveFinished() or saveFailed().
    void save(const QString& path);
    /// Saves on the calling thread (used when the app is about to quit). Returns success.
    bool saveNow(const QString& path, QString* error = nullptr);
    /// Blocks until background work has finished.
    void waitForIdle();

signals:
    void loadStarted(const QString& path);
    void firstChunkShown();
    void loadFinished(const QString& path);
    void loadFailed(const QString& path, const QString& message);
    /// The copy of the text a save will write is complete; the file will be this document state.
    void snapshotTaken();
    void saveFinished(const QString& path);
    void saveFailed(const QString& path, const QString& message);
    void pathChanged(const QString& path);

private:
    struct Snapshot {
        QString text;
        bool rawSeparators = false;  ///< text uses U+2029 between blocks (QTextDocument raw text)
        std::vector<core::LineEnding> perBreak;
        FileFormat format;
        quint64 editCount = 0;
    };
    struct WriteOutcome {
        bool ok = false;
        QString message;
    };

    [[nodiscard]] Snapshot snapshot() const;
    static WriteOutcome write(Snapshot snapshot, const QString& path);
    void finishSave(const QString& path, const WriteOutcome& outcome, quint64 editCount, quint64 documentId);
    void abortLoad(const QString& path, const QString& message);
    void beginInsert(QString text, core::DecodedText meta, const QString& path);
    void insertSlice();
    void finishInsert();

    EditorWidget* editor_;
    QThreadPool pool_;
    QString path_;
    FileFormat format_;
    std::vector<core::LineEnding> pendingPerBreak_;
    QString pendingText_;
    QString pendingPath_;
    qsizetype pendingOffset_ = 0;
    QTimer insertTimer_;
    bool loading_ = false;
    bool firstChunk_ = false;
    quint64 loadGeneration_ = 0;
    quint64 documentId_ = 0;  ///< changes whenever another document replaces the current one
    int savesInFlight_ = 0;
};

}  // namespace hitokoto::ui
