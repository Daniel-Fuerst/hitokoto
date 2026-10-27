// SPDX-License-Identifier: GPL-3.0-or-later
// Loading and saving the one open document. Reading, decoding, encoding and writing run on a
// worker thread; the text is inserted into the editor in short slices so the UI stays
// responsive with large files (S001, S004, S007). Saving copies the text out of the editor in
// short slices as well, with the editor editable; the copy is set back where edits touch it, so
// the file is always a state the document had (S017).
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
    [[nodiscard]] bool isSaving() const noexcept { return savesInFlight_ > 0 || copy_.active; }
    /// How often edits set the running or last copy back (instrumentation for tests).
    [[nodiscard]] quint64 copySetBacks() const noexcept { return copySetBacks_; }

    /// Empty, untitled document.
    void newDocument();
    /// Starts loading `path`; emits loadFinished() or loadFailed(). The current document stays
    /// untouched if the file cannot be read or decoded.
    void load(const QString& path);
    /// Saves to `path` in the background; emits snapshotTaken() once the copy of the text is
    /// complete, then saveFinished() or saveFailed(). A save requested while an earlier copy is
    /// still running replaces that request.
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
        QString text;  ///< blocks joined with '\n'
        std::vector<core::LineEnding> perBreak;
        FileFormat format;
        quint64 editCount = 0;
    };
    struct WriteOutcome {
        bool ok = false;
        QString message;
    };

    /// The text being copied out of the editor for a save (S017).
    struct Copy {
        struct Slice {
            int firstBlock = 0;
            qsizetype textOffset = 0;  ///< where the slice starts in `text`
            std::size_t breaks = 0;    ///< size of `perBreak` before the slice
        };
        bool active = false;
        QString path;
        QString text;                            ///< copied blocks joined with '\n'
        std::vector<Slice> slices;               ///< what each slice copied, for setting back
        std::vector<core::LineEnding> perBreak;  ///< one per copied break (mixed endings only)
        int copiedBlocks = 0;
        int setBacks = 0;
    };

    void beginCopy(const QString& path);
    /// Copies blocks until the slice limits are reached (or everything, if `allAtOnce`).
    /// Returns true when the copy is complete.
    bool copyBlocks(bool allAtOnce);
    void copySlice();
    [[nodiscard]] Snapshot takeSnapshot();
    void cancelCopy();
    void onContentsChange(int position, int charsRemoved, int charsAdded);
    void startWrite(Snapshot s, const QString& path);
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
    Copy copy_;
    QTimer copyTimer_;
    quint64 copySetBacks_ = 0;
    bool loading_ = false;
    bool firstChunk_ = false;
    quint64 loadGeneration_ = 0;
    quint64 documentId_ = 0;  ///< changes whenever another document replaces the current one
    int savesInFlight_ = 0;
};

}  // namespace hitokoto::ui
