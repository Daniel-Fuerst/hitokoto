// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_session.hpp"

#include "block_data.hpp"
#include "editor_widget.hpp"
#include "hitokoto/core/file_io.hpp"
#include "qt_bridge.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QTextBlock>
#include <QTextBlockUserData>
#include <QTextCursor>
#include <QTextDocument>

#include <filesystem>

namespace hitokoto::ui {
namespace {

// First slice is shown at once; the rest follows in slices of about this many characters.
constexpr qsizetype kFirstChunk = qsizetype{64} * 1024;
constexpr qsizetype kChunk = qsizetype{256} * 1024;
constexpr qint64 kSliceNanoseconds = 8'000'000;

std::filesystem::path toPath(const QString& p) {
    return std::filesystem::path(p.toStdU16String());
}

/// Cut position at or after `from + size`, just after a line break if possible.
qsizetype cutAfter(const QString& text, qsizetype from, qsizetype size) {
    const qsizetype target = from + size;
    if (target >= text.size()) {
        return text.size();
    }
    const qsizetype nl = text.indexOf(QLatin1Char('\n'), target);
    return nl < 0 ? text.size() : nl + 1;
}

QString describe(const QString& path, const std::string& message) {
    return QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), QString::fromStdString(message));
}

}  // namespace

DocumentSession::DocumentSession(EditorWidget* editor, QObject* parent) : QObject(parent), editor_(editor) {
    pool_.setMaxThreadCount(1);  // loads and saves run in order
    insertTimer_.setInterval(0);
    connect(&insertTimer_, &QTimer::timeout, this, &DocumentSession::insertSlice);
}

DocumentSession::~DocumentSession() {
    pool_.waitForDone();
}

void DocumentSession::waitForIdle() {
    pool_.waitForDone();
    QCoreApplication::sendPostedEvents(this);
}

void DocumentSession::newDocument() {
    ++loadGeneration_;
    ++documentId_;
    insertTimer_.stop();
    pendingText_.clear();
    pendingPerBreak_.clear();
    loading_ = false;
    editor_->document()->setUndoRedoEnabled(true);
    editor_->setReadOnly(false);
    editor_->setPlainText(QString());
    editor_->document()->setModified(false);
    format_ = FileFormat{};
    path_.clear();
    emit pathChanged(path_);
}

void DocumentSession::load(const QString& path) {
    const quint64 generation = ++loadGeneration_;
    // The current document is about to be replaced: no edits from here on (they would be lost).
    insertTimer_.stop();
    loading_ = true;
    editor_->setReadOnly(true);
    emit loadStarted(path);
    pool_.start([this, path, generation] {
        auto read = core::readFile(toPath(path));
        if (!read.bytes) {
            const QString message = describe(path, read.error ? read.error->message : "cannot read file");
            QMetaObject::invokeMethod(
                this,
                [this, path, message, generation] {
                    if (generation == loadGeneration_)
                        abortLoad(path, message);
                },
                Qt::QueuedConnection);
            return;
        }
        auto decoded = core::decodeUtf8(*read.bytes);
        read.bytes.reset();
        if (!decoded.value) {
            const QString message =
                describe(path, decoded.error->message + " at byte " + std::to_string(decoded.error->byteOffset) +
                                   ". The file was not opened; nothing was changed.");
            QMetaObject::invokeMethod(
                this,
                [this, path, message, generation] {
                    if (generation == loadGeneration_)
                        abortLoad(path, message);
                },
                Qt::QueuedConnection);
            return;
        }
        QString text = toQString(std::u16string_view(decoded.value->text));
        decoded.value->text.clear();
        decoded.value->text.shrink_to_fit();
        QMetaObject::invokeMethod(
            this,
            [this, text = std::move(text), meta = std::move(*decoded.value), path, generation]() mutable {
                if (generation == loadGeneration_)
                    beginInsert(std::move(text), std::move(meta), path);
            },
            Qt::QueuedConnection);
    });
}

void DocumentSession::abortLoad(const QString& path, const QString& message) {
    loading_ = false;
    editor_->setReadOnly(false);
    emit loadFailed(path, message);
}

void DocumentSession::beginInsert(QString text, core::DecodedText meta, const QString& path) {
    loading_ = true;
    ++documentId_;
    pendingText_ = std::move(text);
    pendingPath_ = path;
    pendingPerBreak_ = std::move(meta.perBreak);
    format_ = FileFormat{meta.hasBom, meta.uniformEnding, !pendingPerBreak_.empty()};

    QTextDocument* doc = editor_->document();
    doc->setUndoRedoEnabled(false);
    editor_->setReadOnly(true);
    const qsizetype cut = cutAfter(pendingText_, 0, kFirstChunk);
    editor_->setPlainText(pendingText_.left(cut));
    pendingOffset_ = cut;
    path_ = path;
    emit pathChanged(path_);
    firstChunk_ = true;
    if (pendingOffset_ >= pendingText_.size()) {
        finishInsert();
    } else {
        insertTimer_.start();
    }
}

void DocumentSession::insertSlice() {
    if (firstChunk_) {
        firstChunk_ = false;
        emit firstChunkShown();
    }
    QElapsedTimer timer;
    timer.start();
    QTextCursor end(editor_->document());
    end.movePosition(QTextCursor::End);
    while (pendingOffset_ < pendingText_.size() && timer.nsecsElapsed() < kSliceNanoseconds) {
        const qsizetype cut = cutAfter(pendingText_, pendingOffset_, kChunk);
        end.insertText(pendingText_.mid(pendingOffset_, cut - pendingOffset_));
        pendingOffset_ = cut;
    }
    if (pendingOffset_ >= pendingText_.size()) {
        insertTimer_.stop();
        finishInsert();
    }
}

void DocumentSession::finishInsert() {
    QTextDocument* doc = editor_->document();
    if (!pendingPerBreak_.empty()) {
        std::size_t i = 0;
        for (QTextBlock b = doc->begin(); b.isValid() && i < pendingPerBreak_.size(); b = b.next(), ++i) {
            // Lines without an entry save with the majority ending; store only the others.
            if (pendingPerBreak_[i] == format_.ending) {
                continue;
            }
            auto* data = dynamic_cast<BlockData*>(b.userData());
            if (data == nullptr) {
                data = new BlockData;
                b.setUserData(data);
            }
            data->ending = pendingPerBreak_[i];
        }
    }
    pendingPerBreak_.clear();
    pendingText_.clear();
    pendingText_.squeeze();
    doc->setUndoRedoEnabled(true);
    doc->setModified(false);
    editor_->setReadOnly(false);
    editor_->moveCursor(QTextCursor::Start);
    loading_ = false;
    if (firstChunk_) {
        firstChunk_ = false;
        emit firstChunkShown();
    }
    emit loadFinished(pendingPath_);
}

DocumentSession::Snapshot DocumentSession::snapshot() const {
    Snapshot s;
    s.format = format_;
    s.editCount = editor_->editCount();
    const QTextDocument* doc = editor_->document();
    if (!format_.mixedEndings) {
        s.text = doc->toRawText();
        s.rawSeparators = true;
        return s;
    }
    QString text;
    text.reserve(doc->characterCount());
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        text += b.text();
        if (b.next().isValid()) {
            text += QLatin1Char('\n');
            const auto* data = dynamic_cast<const BlockData*>(b.userData());
            s.perBreak.push_back(data && data->ending ? *data->ending : format_.ending);
        }
    }
    s.text = std::move(text);
    return s;
}

DocumentSession::WriteOutcome DocumentSession::write(Snapshot s, const QString& path) {
    if (s.rawSeparators) {
        // QTextDocument separates blocks with U+2029; files never contain it (rejected on load).
        s.text.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    }
    const auto bytes = core::encodeUtf8(view(s.text), s.format.hasBom, s.format.ending, s.perBreak);
    if (!bytes) {
        return {false, QStringLiteral("internal error: text could not be encoded")};
    }
    if (const auto err = core::writeFileAtomically(toPath(path), *bytes)) {
        return {false, describe(path, err->message)};
    }
    return {true, {}};
}

void DocumentSession::save(const QString& path) {
    if (loading_) {
        emit saveFailed(path, QStringLiteral("The file is still loading."));
        return;
    }
    Snapshot s = snapshot();
    emit snapshotTaken();
    const quint64 editCount = s.editCount;
    const quint64 documentId = documentId_;
    ++savesInFlight_;
    pool_.start([this, s = std::move(s), path, editCount, documentId]() mutable {
        const WriteOutcome outcome = write(std::move(s), path);
        QMetaObject::invokeMethod(
            this, [this, path, outcome, editCount, documentId] { finishSave(path, outcome, editCount, documentId); },
            Qt::QueuedConnection);
    });
}

bool DocumentSession::saveNow(const QString& path, QString* error) {
    waitForIdle();
    if (loading_) {
        if (error)
            *error = QStringLiteral("The file is still loading.");
        return false;
    }
    Snapshot s = snapshot();
    const quint64 editCount = s.editCount;
    ++savesInFlight_;
    const WriteOutcome outcome = write(std::move(s), path);
    finishSave(path, outcome, editCount, documentId_);
    if (!outcome.ok && error) {
        *error = outcome.message;
    }
    return outcome.ok;
}

void DocumentSession::finishSave(const QString& path, const WriteOutcome& outcome, quint64 editCount,
                                 quint64 documentId) {
    --savesInFlight_;
    if (!outcome.ok) {
        emit saveFailed(path, outcome.message);
        return;
    }
    if (documentId != documentId_) {
        // Another document was opened meanwhile; the saved file belongs to the old one.
        emit saveFinished(path);
        return;
    }
    if (editor_->editCount() == editCount) {
        editor_->document()->setModified(false);
    }
    if (path != path_) {
        path_ = path;
        emit pathChanged(path_);
    }
    emit saveFinished(path);
}

}  // namespace hitokoto::ui
