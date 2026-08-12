#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

#include "engine/utils/Properties.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "utils/PropertyWidget.hpp"
#include "utils/AssetPickerWidget.hpp"
#include "utils/AssetThumbnails.hpp"
#include "utils/RefDropFilter.hpp"
#include "utils/DragDropMime.hpp"
#include "utils/ListRowChrome.hpp"

// Keeps a PropertyWidget wrapper alive exactly as long as the Qt widget it drives.
// The wrappers are plain C++ objects whose lambdas capture `this`, so freeing them
// on our own schedule can outlive-or-underlive the QWidget; parenting the holder to
// the row makes Qt's (deferred) row deletion free the wrapper at the right moment.
class PropertyWidgetHolder : public QObject {
public:
    PropertyWidgetHolder(std::unique_ptr<PropertyWidgetBase> w, QObject* parent)
        : QObject(parent), wrapper(std::move(w)) {}
private:
    std::unique_ptr<PropertyWidgetBase> wrapper;
};

// The Animator's frame strip: one sprite asset-reference row per frame, exactly
// like a SPRITE field in the object inspector (same "…" picker, same drag-drop,
// same hover preview), plus reorder/remove controls.
//
// Drops are multi-aware: dragging several sprites out of the asset picker (or
// several sprite files out of the Folder view) appends them all in one gesture,
// which is the whole point of authoring a flipbook.
class FrameListWidget : public QWidget {
    Q_OBJECT
public:
    // Callbacks let this widget stay free of Animator/state coupling.
    std::function<std::vector<std::string>()>          getFrames;
    std::function<void(int idx, const std::string&)>   onSetFrame;
    std::function<void(const std::vector<std::string>&)> onAddFrames;
    std::function<void(int idx)>                       onRemoveFrame;
    std::function<void(int from, int to)>              onMoveFrame;

    explicit FrameListWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setAcceptDrops(true);
        m_layout = new QVBoxLayout(this);
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(2);

        // Reorder commits through the Animator's existing MoveFrame backend, so
        // the drag has the same effect the old ↑/↓ buttons did -- just in one
        // gesture and across any distance.
        m_reorder = new ListRow::ReorderController(this,
            [this](int from, int to) { if (onMoveFrame) onMoveFrame(from, to); }, this);
    }

    void Rebuild() {
        clear();
        const std::vector<std::string> frames = getFrames ? getFrames() : std::vector<std::string>{};

        for (int i = 0; i < static_cast<int>(frames.size()); ++i)
            m_layout->addWidget(buildRow(i, frames[i], static_cast<int>(frames.size())));

        if (frames.empty()) {
            auto* empty = new QLabel("Drag sprites here", this);
            empty->setAlignment(Qt::AlignCenter);
            empty->setStyleSheet(
                "color:#777; border:1px dashed #555; border-radius:3px; padding:10px;");
            m_layout->addWidget(empty);
        }

        auto* addBtn = new QPushButton("+ Add Frames", this);
        addBtn->setToolTip("Pick one or more sprites (Ctrl/Shift to multi-select)");
        connect(addBtn, &QPushButton::clicked, this, [this, addBtn]() { openAddPicker(addBtn); });
        m_layout->addWidget(addBtn);
    }

protected:
    void dragEnterEvent(QDragEnterEvent* e) override { handleDrag(e); }
    void dragMoveEvent(QDragMoveEvent* e)  override { handleDrag(e); }

    void dropEvent(QDropEvent* e) override {
        const std::vector<std::string> ids = RefDropFilter::SpriteIdsFromMime(e->mimeData());
        std::vector<std::string> valid;
        for (const auto& id : ids)
            if (AssetManager::Get().GetSprite(id)) valid.push_back(id);
        if (valid.empty()) { e->ignore(); return; }
        e->setDropAction(Qt::CopyAction);
        e->accept();
        if (onAddFrames) onAddFrames(valid);
    }

private:
    void handleDrag(QDropEvent* e) {
        for (const auto& id : RefDropFilter::SpriteIdsFromMime(e->mimeData())) {
            if (AssetManager::Get().GetSprite(id)) {
                // Copy, never Move — a Move result makes an item view source delete
                // the row it believes moved out of it.
                e->setDropAction(Qt::CopyAction);
                e->accept();
                return;
            }
        }
        e->ignore();
    }

    QWidget* buildRow(int index, const std::string& spriteId, int /*frameCount*/) {
        auto* row = new QWidget(this);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(3);

        // Grip first, ahead of everything including the index, because it is the
        // handle for the row itself. Same class the inspector's list rows use --
        // the ↑/↓ pair this replaced only moved one step at a time and had to be
        // disabled at the ends, which drag-reorder makes unnecessary.
        auto* handle = new ListRow::DragHandle(row);
        rl->addWidget(handle);

        auto* idxLabel = new QLabel(QString::number(index), row);
        idxLabel->setFixedWidth(18);
        idxLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        idxLabel->setStyleSheet("color:#888;");
        rl->addWidget(idxLabel);

        // The same widget class the object inspector uses for a SPRITE field, so a
        // frame slot behaves identically to every other asset reference in the app.
        auto ref = std::make_unique<ObjectRefPropertyWidget>(
            Properties::PropDesc().Tag(Properties::Tags::SPRITE)
                                  .RefType(Properties::Tags::OBJECT_REF));
        ref->SetValue(spriteId);
        ref->onChanged = [this, index](const std::string& id) {
            if (onSetFrame) onSetFrame(index, id);
        };
        rl->addWidget(ref->GetWidget(), 1);
        new PropertyWidgetHolder(std::move(ref), row);   // freed with the row

        auto* del = ListRow::MakeRemoveButton(row);
        del->setToolTip("Remove frame");
        connect(del, &QPushButton::clicked, this, [this, index]() {
            if (onRemoveFrame) onRemoveFrame(index);
        });
        rl->addWidget(del);

        m_reorder->RegisterRow(row, handle);
        return row;
    }

    void openAddPicker(QWidget* anchor) {
        std::vector<std::pair<std::string, std::string>> items;
        for (const auto& [id, sp] : AssetManager::Get().GetAllSprites())
            items.push_back({ sp->GetName(), id });

        auto* picker = new AssetPickerWidget(std::move(items),
            [](const std::string& id) { return AssetThumbnails::forSprite(id); },
            nullptr, this);
        picker->setTitle("Add Frames");
        picker->setMultiSelect(true);
        // Also usable as a drag source, so sprites can be dropped straight onto the
        // strip (or onto one specific slot) instead of always appending.
        picker->setDragMimeFactory([](const std::vector<std::string>& ids) -> QMimeData* {
            auto* mime = new QMimeData();
            QStringList list;
            for (const auto& id : ids) list << QString::fromStdString(id);
            mime->setData(kSpriteListMimeType, list.join('\n').toUtf8());
            // Single-value sprite fields understand only this one.
            if (!ids.empty())
                mime->setData(kSpriteMimeType, QString::fromStdString(ids.front()).toUtf8());
            return mime;
        });
        picker->onSelectedMany = [this](const std::vector<std::string>& ids) {
            if (onAddFrames) onAddFrames(ids);
        };
        picker->showAt(anchor->mapToGlobal(QPoint(0, anchor->height())));
    }

    void clear() {
        if (m_reorder) m_reorder->Clear();
        while (QLayoutItem* item = m_layout->takeAt(0)) {
            if (QWidget* w = item->widget()) w->deleteLater();
            delete item;
        }
    }

    QVBoxLayout* m_layout = nullptr;
    ListRow::ReorderController* m_reorder = nullptr;
};
