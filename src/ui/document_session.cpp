// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_session.hpp"

#include "block_data.hpp"
#include "block_styler.hpp"
#include "editor_widget.hpp"
#include "hitokoto/core/file_io.hpp"
#include "qt_bridge.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QTextBlock>
#include <QTextBlockUserData>
#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>
#include <climits>
#include <filesystem>
#include <iterator>
#include <utility>

namespace hitokoto::ui {
namespace {

// First slice is shown at once; the rest follows in slices of about this many characters.
constexpr qsizetype kFirstChunk = qsizetype{64} * 1024;
constexpr qsizetype kChunk = qsizetype{256} * 1024;
constexpr qint64 kSliceNanoseconds = 8'000'000;

// Saving copies the text out of the editor in slices of about this many characters, each in
// its own event-loop turn (S017): a few milliseconds per slice. QTextCursor::selectedText() is
// the only public way to copy a range; it costs about six times toRawText() per character, so
// the whole copy takes more CPU than one toRawText(), spread so that nothing stalls
// (docs/performance.md). After this many set-backs by edits the rest is copied in one go, so
// that a save always finishes.
constexpr qsizetype kCopySliceChars = qsizetype{128} * 1024;
constexpr int kCopyMaxSetBacks = 8;

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

DocumentSession::DocumentSession(EditorWidget* editor, const core::ElementRegistry& registry, QObject* parent)
    : QObject(parent), editor_(editor), registry_(registry) {
    pool_.setMaxThreadCount(1);  // loads and saves run in order
    insertTimer_.setInterval(0);
    connect(&insertTimer_, &QTimer::timeout, this, &DocumentSession::insertSlice);
    copyTimer_.setInterval(0);
    connect(&copyTimer_, &QTimer::timeout, this, &DocumentSession::copySlice);
    connect(editor_->document(), &QTextDocument::contentsChange, this, &DocumentSession::onContentsChange);
}

DocumentSession::~DocumentSession() {
    pool_.waitForDone();
}

void DocumentSession::waitForIdle() {
    finishCopy();
    pool_.waitForDone();
    QCoreApplication::sendPostedEvents(this);
}

void DocumentSession::finishCopy() {
    if (!copy_.active) {
        return;
    }
    // A save whose copy is still running is written, not dropped: the rest is copied in one go
    // (one short stall) and handed to the worker thread like a completed copy.
    const QString path = copy_.path;
    copyBlocks(true);
    startWrite(takeSnapshot(), path);
}

void DocumentSession::newDocument() {
    finishCopy();  // the document is about to be replaced; a pending save still belongs to it
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
    finishCopy();  // the document is about to be replaced; a pending save still belongs to it
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
        // ADR 0002, decision 6: text directly below a table gets a blank line, as Obsidian reads it.
        const std::size_t added = core::separateTextAfterTables(*decoded.value, registry_);
        QString text = toQString(std::u16string_view(decoded.value->text));
        decoded.value->text.clear();
        decoded.value->text.shrink_to_fit();
        QMetaObject::invokeMethod(
            this,
            [this, text = std::move(text), meta = std::move(*decoded.value), path, generation, added]() mutable {
                if (generation == loadGeneration_)
                    beginInsert(std::move(text), std::move(meta), path, added);
            },
            Qt::QueuedConnection);
    });
}

void DocumentSession::abortLoad(const QString& path, const QString& message) {
    loading_ = false;
    editor_->setReadOnly(false);
    emit loadFailed(path, message);
}

void DocumentSession::beginInsert(QString text, core::DecodedText meta, const QString& path,
                                  std::size_t blankLinesAdded) {
    loading_ = true;
    pendingBlankLines_ = blankLinesAdded;
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
    // Lines added on opening are a change the file does not have yet (ADR 0002, decision 6).
    const std::size_t added = std::exchange(pendingBlankLines_, 0);
    doc->setModified(added > 0);
    editor_->setReadOnly(false);
    editor_->moveCursor(QTextCursor::Start);
    loading_ = false;
    if (firstChunk_) {
        firstChunk_ = false;
        emit firstChunkShown();
    }
    emit loadFinished(pendingPath_);
    if (added > 0) {
        emit blankLinesAdded(static_cast<int>(std::min<std::size_t>(added, INT_MAX)));
    }
}

void DocumentSession::beginCopy(const QString& path) {
    cancelCopy();
    copySetBacks_ = 0;
    copy_.active = true;
    copy_.path = path;
    copy_.text.reserve(editor_->document()->characterCount());
}

void DocumentSession::cancelCopy() {
    copyTimer_.stop();
    copy_ = Copy{};
}

bool DocumentSession::copyBlocks(bool allAtOnce) {
    QTextDocument* doc = editor_->document();
    const int blockCount = doc->blockCount();
    if (copy_.setBacks > kCopyMaxSetBacks) {
        allAtOnce = true;  // starved by edits: finish in one go
    }
    QTextCursor cursor(doc);
    do {
        const QTextBlock first = doc->findBlockByNumber(copy_.copiedBlocks);
        QTextBlock last =
            allAtOnce ? doc->lastBlock() : doc->findBlock(first.position() + static_cast<int>(kCopySliceChars));
        if (!last.isValid()) {
            last = doc->lastBlock();  // the slice reaches past the end
        }
        copy_.slices.push_back({copy_.copiedBlocks, copy_.text.size(), copy_.perBreak.size()});
        // selectedText() takes the same fast path as toRawText(): raw text, blocks separated by
        // U+2029 (which a file never contains, ADR 0002).
        cursor.setPosition(first.position());
        cursor.setPosition(last.position() + last.length() - 1, QTextCursor::KeepAnchor);
        QString part = cursor.selectedText();
        part.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
        copy_.text += part;
        if (last.next().isValid()) {
            copy_.text += QLatin1Char('\n');
        }
        if (format_.mixedEndings) {
            for (QTextBlock b = first; b.isValid() && b.blockNumber() <= last.blockNumber(); b = b.next()) {
                if (b.next().isValid()) {
                    const auto* data = dynamic_cast<const BlockData*>(b.userData());
                    copy_.perBreak.push_back(data && data->ending ? *data->ending : format_.ending);
                }
            }
        }
        copy_.copiedBlocks = last.blockNumber() + 1;
        if (!allAtOnce && copy_.copiedBlocks < blockCount) {
            return false;
        }
    } while (copy_.copiedBlocks < blockCount);
    return true;
}

void DocumentSession::onContentsChange(int position, int /*charsRemoved*/, int /*charsAdded*/) {
    // The styler's own document changes (format ranges, markContentsDirty) are not edits; as of
    // Qt 6.8 and 6.12 they emit no contentsChange, the guard mirrors EditorWidget's for editCount.
    if (!copy_.active || editor_->styler().isBusy()) {
        return;
    }
    // Blocks before the changed one are untouched: set the copy back to the start of the slice
    // that holds the changed block and let the slices copy from there again. Changes behind the
    // copied part need nothing. An invalid block (position outside the document) sets back to the
    // start rather than trusting the position.
    const int block = std::max(editor_->document()->findBlock(position).blockNumber(), 0);
    if (block >= copy_.copiedBlocks || copy_.slices.empty()) {
        return;
    }
    auto slice = copy_.slices.end();
    while (slice != copy_.slices.begin() && std::prev(slice)->firstBlock > block) {
        --slice;
    }
    --slice;  // the slice whose firstBlock <= block
    copy_.text.truncate(slice->textOffset);
    copy_.perBreak.resize(slice->breaks);
    copy_.copiedBlocks = slice->firstBlock;
    copy_.slices.erase(slice, copy_.slices.end());
    ++copy_.setBacks;
    ++copySetBacks_;
}

DocumentSession::Snapshot DocumentSession::takeSnapshot() {
    Snapshot s;
    s.format = format_;
    s.editCount = editor_->editCount();
    s.text = std::move(copy_.text);
    s.perBreak = std::move(copy_.perBreak);
    cancelCopy();
    emit snapshotTaken();
    return s;
}

void DocumentSession::copySlice() {
    if (!copy_.active) {
        copyTimer_.stop();
        return;
    }
    if (!copyBlocks(false)) {
        return;
    }
    const QString path = copy_.path;
    startWrite(takeSnapshot(), path);
}

DocumentSession::WriteOutcome DocumentSession::write(Snapshot s, const QString& path) {
    const auto bytes = core::encodeUtf8(view(s.text), s.format.hasBom, s.format.ending, s.perBreak);
    if (!bytes) {
        return {false, QStringLiteral("internal error: text could not be encoded")};
    }
    if (const auto err = core::writeFileAtomically(toPath(path), *bytes)) {
        return {false, describe(path, err->message)};
    }
    return {true, {}};
}

void DocumentSession::startWrite(Snapshot s, const QString& path) {
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

void DocumentSession::save(const QString& path) {
    if (loading_) {
        emit saveFailed(path, QStringLiteral("The file is still loading."));
        return;
    }
    // The copy runs in slices from the event loop; a newer request replaces a running copy.
    beginCopy(path);
    copyTimer_.start();
}

bool DocumentSession::saveNow(const QString& path, QString* error) {
    waitForIdle();
    if (loading_) {
        if (error)
            *error = QStringLiteral("The file is still loading.");
        return false;
    }
    beginCopy(path);
    copyBlocks(true);
    Snapshot s = takeSnapshot();
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
