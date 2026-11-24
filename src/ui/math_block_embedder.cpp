// SPDX-License-Identifier: GPL-3.0-or-later
#include "math_block_embedder.hpp"

#include "block_styler.hpp"
#include "editor_formats.hpp"

#include <QTextDocument>

#include <algorithm>
#include <unordered_map>

namespace hitokoto::ui {

namespace {
constexpr int kMarginBlocks = 200;    // blocks above and below the screen kept up to date, as for tables
constexpr int kMaxBlockLines = 2000;  // a longer block stays source (it would not fit a screen anyway)
}  // namespace

MathBlockEmbedder::MathBlockEmbedder(QTextDocument* document, BlockStyler* styler, BlockLayout* layout,
                                     std::function<std::pair<int, int>()> visibleBlocks, QObject* parent)
    : QObject(parent), document_(document), styler_(styler), layout_(layout), visibleBlocks_(std::move(visibleBlocks)) {
    layout_->addClaimant(this);
}

MathBlockEmbedder::~MathBlockEmbedder() {
    layout_->removeClaimant(this);
}

bool MathBlockEmbedder::claims(const QTextBlock& block) const {
    return claimedNumbers_.contains(block.blockNumber());
}

void MathBlockEmbedder::setFormats(const EditorFormats* formats) {
    formats_ = formats;
    scheduleRefresh();
}

double MathBlockEmbedder::panelHeight(const std::u16string& source) const {
    if (formats_ == nullptr) {
        return 0;
    }
    const core::MathLayout& formula = formats_->mathLayout(source, 0, true);
    const core::MathStyle& style = formats_->math();
    return formula.ascent + formula.descent + 2 * style.displayPadding + 2 * style.displayMargin;
}

void MathBlockEmbedder::scheduleRefresh() {
    if (refreshPending_) {
        return;
    }
    refreshPending_ = true;
    QMetaObject::invokeMethod(
        this,
        [this] {
            refreshPending_ = false;
            refresh();
        },
        Qt::QueuedConnection);
}

void MathBlockEmbedder::forgetAll() {
    claimed_.clear();
    claimedNumbers_.clear();
    typeset_.clear();
    failed_.clear();
    scheduleRefresh();
}

MathBlockEmbedder::Found MathBlockEmbedder::find(int from, int to) const {
    Found found;
    // Start at the opening line of a block the range begins inside.
    QTextBlock block = document_->findBlockByNumber(from);
    while (block.isValid() && block.previous().isValid()) {
        const core::LineRender render = styler_->renderFor(block);
        if (!render.mathBlock || (render.mathDelimiter && render.blockState.active())) {
            break;
        }
        block = block.previous();
    }
    while (block.isValid() && block.blockNumber() <= to) {
        const core::LineRender render = styler_->renderFor(block);
        if (!(render.mathBlock && render.mathDelimiter && render.blockState.active())) {
            block = block.next();
            continue;
        }
        // An opening `$$` line: collect the block's source up to its closing line.
        const int open = block.blockNumber();
        std::u16string source;
        bool revealed = false;
        bool closed = false;
        QTextBlock line = block;
        int lines = 0;
        for (; line.isValid() && lines < kMaxBlockLines; line = line.next(), ++lines) {
            const core::LineRender r = line == block ? render : styler_->renderFor(line);
            revealed = revealed || styler_->isRevealed(line.blockNumber());
            const QString text = line.text();
            for (const core::MathSpan& m : r.math) {
                if (static_cast<qsizetype>(m.source.end()) <= text.size()) {
                    source += text.sliced(m.source.start, m.source.length).toStdU16String();
                }
            }
            source.push_back(u'\n');
            if (line != block && r.mathDelimiter && !r.blockState.active()) {
                closed = true;
                break;
            }
        }
        if (!line.isValid()) {
            break;  // unclosed: the block runs to the end of the document and stays source
        }
        const int last = line.blockNumber();
        if (closed && !revealed && formats_ != nullptr) {
            const core::MathLayout& formula = formats_->mathLayout(source, 0, true);
            if (formula.error) {
                found.failed.push_back({open, last, formula.error->message});
            } else {
                found.typeset.push_back({open, last, std::move(source)});
            }
        }
        block = line.next();
    }
    return found;
}

void MathBlockEmbedder::release(const QTextBlock& block) {
    if (!block.isValid() || layout_->claimedByOther(block, this)) {
        return;
    }
    layout_->setReservedHeight(block, 0);
    layout_->setHidden(block, false);
}

void MathBlockEmbedder::refresh() {
    if (refreshing_ || formats_ == nullptr) {
        return;
    }
    refreshing_ = true;
    const auto [firstVisible, lastVisible] = visibleBlocks_();
    const int from = std::max(0, firstVisible - kMarginBlocks);
    const int to = std::min(document_->blockCount() - 1, std::max(firstVisible, lastVisible) + kMarginBlocks);
    Found found = find(from, to);

    // What the blocks in range should be: the opening line's reserved height, hidden for the rest.
    std::unordered_map<int, double> wanted;  // block number -> reserved height, or -1 for hidden
    for (const Typeset& t : found.typeset) {
        wanted[t.open] = panelHeight(t.source);
        for (int n = t.open + 1; n <= t.last; ++n) {
            wanted[n] = -1;
        }
    }
    const BlockLayout::Batch batch(layout_);
    std::vector<QTextCursor> kept;
    std::unordered_set<int> keptNumbers;
    for (const QTextCursor& c : claimed_) {
        const QTextBlock block = document_->findBlock(c.position());
        const int n = block.blockNumber();
        if (n >= from && n <= to && !wanted.contains(n)) {
            release(block);
        } else if (block.isValid() && !keptNumbers.contains(n)) {
            kept.push_back(c);  // still wanted, or out of range: left as it is
            keptNumbers.insert(n);
        }
    }
    for (const auto& [n, height] : wanted) {
        const QTextBlock block = document_->findBlockByNumber(n);
        if (!block.isValid() || layout_->claimedByOther(block, this)) {
            continue;
        }
        if (height < 0) {
            layout_->setReservedHeight(block, 0);
            layout_->setHidden(block, true);
        } else {
            layout_->setHidden(block, false);
            layout_->setReservedHeight(block, height);
        }
        if (!keptNumbers.contains(n)) {
            QTextCursor c(document_);
            c.setPosition(block.position());
            kept.push_back(c);
            keptNumbers.insert(n);
        }
    }
    claimed_ = std::move(kept);
    claimedNumbers_ = std::move(keptNumbers);
    // Blocks out of range keep their entries until they come back into range.
    std::erase_if(typeset_, [from, to](const Typeset& t) { return t.last >= from && t.open <= to; });
    std::erase_if(failed_, [from, to](const Failed& f) { return f.last >= from && f.open <= to; });
    std::move(found.typeset.begin(), found.typeset.end(), std::back_inserter(typeset_));
    std::move(found.failed.begin(), found.failed.end(), std::back_inserter(failed_));
    refreshing_ = false;
}

}  // namespace hitokoto::ui
