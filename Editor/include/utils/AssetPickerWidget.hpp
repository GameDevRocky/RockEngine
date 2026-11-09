#pragma once
#include <string>
#include <vector>
#include <functional>
#include <utility>
#include <algorithm>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QScreen>
#include <QGuiApplication>
#include <QTimer>
#include <QPushButton>
#include <QSlider>
#include <QMimeData>
#include <QAbstractItemView>
#include "utils/VerticalResizeGrip.hpp"

// A QListWidget that hands drag payload construction back to its owner, so the
// picker can emit whatever MIME type the assets it is showing call for (sprite
// ids, asset paths, ...) without this header knowing about any of them.
class PickerListWidget : public QListWidget {
    Q_OBJECT
public:
    using QListWidget::QListWidget;
    std::function<QMimeData*(const std::vector<std::string>& ids)> mimeFactory;

protected:
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override {
        if (!mimeFactory) return QListWidget::mimeData(items);
        std::vector<std::string> ids;
        ids.reserve(items.size());
        for (const QListWidgetItem* it : items)
            ids.push_back(it->data(Qt::UserRole).toString().toStdString());
        if (ids.empty()) return QListWidget::mimeData(items);
        return mimeFactory(ids);
    }
};

class AssetPickerWidget : public QWidget {
    Q_OBJECT
public:
    static constexpr int kThumbSize  = 72;
    static constexpr int kCellWidth  = kThumbSize + 18;
    static constexpr int kCellHeight = kThumbSize + 30;
    static constexpr int kInlineThumbSize  = kThumbSize / 2;
    static constexpr int kInlineCellWidth  = 64;
    static constexpr int kInlineCellHeight = 68;
    static constexpr int kListThreshold = 28;

    // thumbnailGen:    optional GL/image render → QPixmap.
    // fallbackIconGen: optional — used when thumbnailGen returns null (e.g. shaders via CustomIconProvider).
    // Grey placeholder is used only when both return nothing.
    explicit AssetPickerWidget(
        std::vector<std::pair<std::string, std::string>> items,
        std::function<QPixmap(const std::string& id)> thumbnailGen = nullptr,
        std::function<QIcon(const std::string& id)>   fallbackIconGen = nullptr,
        QWidget* parent = nullptr)
        : QWidget(parent, Qt::Popup),
          m_allItems(std::move(items)),
          m_thumbnailGen(std::move(thumbnailGen)),
          m_fallbackIconGen(std::move(fallbackIconGen))
    {
        setAttribute(Qt::WA_DeleteOnClose);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(4);

        // Drag handle + close button. Always built (it is also the title), but only
        // actually draggable once the popup is a real window — see setPersistent.
        m_header = new QWidget(this);
        m_header->setCursor(Qt::SizeAllCursor);
        auto* hl = new QHBoxLayout(m_header);
        hl->setContentsMargins(2, 0, 0, 0);
        hl->setSpacing(4);
        m_title = new QLabel("Select Asset", m_header);
        m_title->setStyleSheet("font-weight:bold;");
        hl->addWidget(m_title);
        hl->addStretch();
        m_closeBtn = new QPushButton("×", m_header);   // ×
        m_closeBtn->setFixedWidth(22);
        m_closeBtn->setFlat(true);
        connect(m_closeBtn, &QPushButton::clicked, this, [this]() { close(); });
        hl->addWidget(m_closeBtn);
        root->addWidget(m_header);

        m_search = new QLineEdit(this);
        m_search->setPlaceholderText("Search…");
        m_search->setClearButtonEnabled(true);
        root->addWidget(m_search);

        m_list = new PickerListWidget(this);
        m_list->setViewMode(QListView::IconMode);
        m_list->setIconSize(QSize(kThumbSize, kThumbSize));
        m_list->setGridSize(QSize(kCellWidth, kCellHeight));
        m_list->setResizeMode(QListView::Adjust);
        m_list->setWordWrap(true);
        // Cell spacing is carried by the item style. Keeping the layout spacing
        // at zero lets the computed grid widths consume the viewport exactly.
        m_list->setSpacing(0);
        m_list->setMovement(QListView::Static);
        m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_list->setUniformItemSizes(true);
        m_list->viewport()->installEventFilter(this);
        root->addWidget(m_list);

        // Confirm bar, only shown in multi-select mode.
        m_confirmBar = new QWidget(this);
        auto* cl = new QHBoxLayout(m_confirmBar);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->addStretch();
        m_confirmBtn = new QPushButton("Add Selected", m_confirmBar);
        connect(m_confirmBtn, &QPushButton::clicked, this, [this]() { commitMulti(); });
        cl->addWidget(m_confirmBtn);
        root->addWidget(m_confirmBar);
        m_confirmBar->hide();

        m_footer = new QWidget(this);
        auto* footerLayout = new QHBoxLayout(m_footer);
        footerLayout->setContentsMargins(0, 0, 0, 0);
        footerLayout->setSpacing(4);
        footerLayout->addStretch();
        footerLayout->addWidget(new QLabel("Size", m_footer));
        m_cellSizeSlider = new QSlider(Qt::Horizontal, m_footer);
        m_cellSizeSlider->setRange(16, 128);
        m_cellSizeSlider->setValue(kThumbSize);
        m_cellSizeSlider->setFixedWidth(100);
        m_cellSizeSlider->setToolTip("Cell size; drag left for list view");
        m_cellSizeSlider->setAccessibleName("Reference cell size");
        footerLayout->addWidget(m_cellSizeSlider);
        root->addWidget(m_footer);

        buildAllItems();

        // Show ~3 rows by default; let the popup resize vertically.
        setMinimumWidth(kCellWidth * 10);
        setMinimumHeight(kCellHeight * 5 + m_search->sizeHint().height() + 16);

        connect(m_search, &QLineEdit::textChanged, this, &AssetPickerWidget::filterItems);
        connect(m_list, &QListWidget::itemDoubleClicked,
                this, &AssetPickerWidget::commitSelection);
        connect(m_list, &QListWidget::itemSelectionChanged,
                this, [this]() { updateConfirmText(); });
        connect(m_cellSizeSlider, &QSlider::valueChanged,
                this, [this] { updateCellLayout(); });

        m_search->setFocus();
    }

    // Single-pick callback (double-click / Enter in single mode).
    std::function<void(const std::string&)> onSelected;
    // Multi-pick callback, fired by the confirm button / Enter in multi mode.
    std::function<void(const std::vector<std::string>&)> onSelectedMany;
    // Inline owners use Escape to collapse their detail row instead of leaving
    // an expanded but empty shell after QWidget::close().
    std::function<void()> onDismissed;

    void setTitle(const QString& t) { if (m_title) m_title->setText(t); }
    int cellSize() const { return m_cellSizeSlider->value(); }
    void setCellSize(int size) { m_cellSizeSlider->setValue(size); }

    // Replace the model after an engine Observable reports a structural or name
    // change. Preserves the search text and current selection, so an event-driven
    // refresh does not interrupt what the user is doing in an expanded picker.
    void refreshItems(std::vector<std::pair<std::string, std::string>> items) {
        const QString filter = m_search->text();
        m_allItems = std::move(items);
        buildAllItems();
        filterItems(filter);
        applySelection();
        QTimer::singleShot(0, this, [this]() { applySelection(); });
    }

    // Ctrl/Shift-extendable selection plus an "Add Selected" button. Implies
    // persistent, since picking several then confirming is a multi-step gesture
    // that a click-away-to-dismiss popup would keep interrupting.
    void setMultiSelect(bool on) {
        m_multiSelect = on;
        m_list->setSelectionMode(on ? QAbstractItemView::ExtendedSelection
                                    : QAbstractItemView::SingleSelection);
        m_confirmBar->setVisible(on);
        if (on) setPersistent(true);
        updateConfirmText();
    }

    // Lets items be dragged OUT of the picker and dropped on a compatible field.
    // `factory` builds the payload for the dragged ids. Implies persistent — the
    // picker has to survive the drag gesture to be a useful drag source.
    void setDragMimeFactory(std::function<QMimeData*(const std::vector<std::string>&)> factory) {
        m_list->mimeFactory = std::move(factory);
        const bool on = static_cast<bool>(m_list->mimeFactory);
        m_list->setDragEnabled(on);
        m_list->setDragDropMode(on ? QAbstractItemView::DragOnly : QAbstractItemView::NoDragDrop);
        m_list->setDefaultDropAction(Qt::CopyAction);
        if (on) setPersistent(true);
    }

    // Popup (default): grabs input and closes on any click outside — right for a
    // one-shot pick. Persistent: a frameless tool window that stays put, so it can
    // be moved, dragged out of, and used for several picks in a row. Must be set
    // before showAt().
    void setPersistent(bool on) {
        if (m_persistent == on) return;
        m_persistent = on;
        setWindowFlags(on ? (Qt::Tool | Qt::FramelessWindowHint)
                          : Qt::Popup);
    }

    // Preselect the item with this id when the popup opens: it becomes the current
    // (highlighted) item and is scrolled into view, so a field that already has a
    // value doesn't make the user hunt for it. Empty selects a supplied "None"
    // item; no match leaves the default first item. Call before showAt().
    void setSelectedId(std::string id) {
        m_selectedId = std::move(id);
        if (m_inline) {
            applySelection();
            QTimer::singleShot(0, this, [this]() { applySelection(); });
        }
    }

    // Embeds the existing searchable thumbnail grid into a property row. The
    // floating title/close chrome and window sizing are removed, while the same
    // filtering, selection, thumbnails, and callbacks remain in use.
    void setInline(bool on) {
        if (m_inline == on) return;
        m_inline = on;
        setAttribute(Qt::WA_DeleteOnClose, !on);
        setWindowFlags(on ? Qt::Widget
                          : (m_persistent ? (Qt::Tool | Qt::FramelessWindowHint)
                                          : Qt::Popup));
        m_header->setVisible(!on);
        if (on) {
            setObjectName("InlineObjectRefPicker");
            setMinimumSize(0, 0);
            setMaximumWidth(QWIDGETSIZE_MAX);
            m_list->setObjectName("InlineObjectRefPickerList");
            m_list->setIconSize(QSize(kInlineThumbSize, kInlineThumbSize));
            m_list->setGridSize(QSize(kInlineCellWidth, kInlineCellHeight));
            m_list->setWordWrap(false);
            m_list->setTextElideMode(Qt::ElideRight);
            m_cellWidth = 0;
            m_cellHeight = 0;
            m_footer->layout()->setContentsMargins(0, 0, VerticalResizeGrip::kSize + 3, 0);
            const int footerHeight = m_footer->sizeHint().height();
            const int minHeight = kInlineCellHeight + m_search->sizeHint().height()
                                + footerHeight + 18;
            const int maxHeight = 650;
            m_resizeGrip = new VerticalResizeGrip(this, minHeight, maxHeight);
            setFixedHeight(kInlineCellHeight * 2 + m_search->sizeHint().height()
                           + footerHeight + 18);
            setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            m_cellSizeSlider->setValue(kInlineThumbSize);
            show();
            updateCellLayout();
            applySelection();
        }
    }

    // Show the popup with its top-left at the given global position, nudged so the
    // whole widget stays within the available area of the screen it lands on — it is
    // never clipped by a physical screen edge (or the taskbar). Use this instead of
    // move()+show() so callers don't each have to redo the clamping.
    void showAt(QPoint globalTopLeft) {
        adjustSize();  // resolve the real popup size before clamping to the screen
        const QSize sz = size();
        QScreen* scr = screen();
        if (!scr) scr = QGuiApplication::primaryScreen();
        if (scr) {
            const QRect avail = scr->availableGeometry();
            const int maxX = std::max(avail.left(), avail.right()  - sz.width()  + 1);
            const int maxY = std::max(avail.top(),  avail.bottom() - sz.height() + 1);
            globalTopLeft.setX(std::clamp(globalTopLeft.x(), avail.left(), maxX));
            globalTopLeft.setY(std::clamp(globalTopLeft.y(), avail.top(),  maxY));
        }
        move(globalTopLeft);
        show();
        if (m_persistent) { raise(); activateWindow(); }
        // Apply now and again next event-loop turn: right after show() the list's
        // viewport may not be laid out yet, so scrollToItem could no-op. The
        // deferred pass guarantees the item ends up visible.
        applySelection();
        QTimer::singleShot(0, this, [this]() { applySelection(); });
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == m_list->viewport() && event->type() == QEvent::Resize)
            updateCellLayout();
        return QWidget::eventFilter(watched, event);
    }

    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        if (m_resizeGrip) {
            m_resizeGrip->move(width() - VerticalResizeGrip::kSize - 1,
                               height() - VerticalResizeGrip::kSize - 1);
            m_resizeGrip->raise();
        }
    }

    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            if (m_multiSelect) commitMulti();
            else commitSelection(m_list->currentItem());
        } else if (e->key() == Qt::Key_Escape) {
            if (m_inline && onDismissed) onDismissed();
            else close();
        } else {
            QWidget::keyPressEvent(e);
        }
    }

    // Dragging the header moves the whole window. Works for the plain popup too,
    // not just the persistent tool window — a picker that opened over the thing you
    // were trying to compare it against should be movable either way.
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && headerHit(e)) {
            m_dragging = true;
            m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
            e->accept();
            return;
        }
        QWidget::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (m_dragging) {
            move(e->globalPosition().toPoint() - m_dragOffset);
            e->accept();
            return;
        }
        QWidget::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (m_dragging) { m_dragging = false; e->accept(); return; }
        QWidget::mouseReleaseEvent(e);
    }

private slots:
    void filterItems(const QString& filter) {
        const QString lower = filter.toLower();
        for (int i = 0; i < m_list->count(); ++i) {
            auto* item = m_list->item(i);
            item->setHidden(!lower.isEmpty() &&
                            !item->text().toLower().contains(lower));
        }
        // Keep a valid current item.
        if (!m_list->currentItem() || m_list->currentItem()->isHidden()) {
            for (int i = 0; i < m_list->count(); ++i) {
                if (!m_list->item(i)->isHidden()) {
                    m_list->setCurrentRow(i);
                    break;
                }
            }
        }
    }

    void commitSelection(QListWidgetItem* item) {
        // A double-click in multi mode means "take everything I have selected",
        // not "take just this one" — otherwise the selection you built is lost.
        if (m_multiSelect) { commitMulti(); return; }
        if (!item) return;
        const std::string id = item->data(Qt::UserRole).toString().toStdString();
        if (onSelected) onSelected(id);
        if (!m_inline) close();
    }

private:
    void commitMulti() {
        std::vector<std::string> ids = selectedIds();
        if (ids.empty()) return;
        if (onSelectedMany) onSelectedMany(ids);
        else if (onSelected) for (const auto& id : ids) onSelected(id);
        if (!m_inline) close();
    }

    // Selection in the order the list shows it, so frames land in the order the
    // user sees rather than the order they happened to ctrl-click.
    std::vector<std::string> selectedIds() const {
        std::vector<std::string> ids;
        for (int i = 0; i < m_list->count(); ++i) {
            QListWidgetItem* item = m_list->item(i);
            if (item->isSelected() && !item->isHidden())
                ids.push_back(item->data(Qt::UserRole).toString().toStdString());
        }
        return ids;
    }

    void updateConfirmText() {
        if (!m_confirmBtn) return;
        const int n = static_cast<int>(selectedIds().size());
        m_confirmBtn->setText(n > 0 ? QString("Add Selected (%1)").arg(n)
                                    : QString("Add Selected"));
        m_confirmBtn->setEnabled(n > 0);
    }

    bool headerHit(QMouseEvent* e) const {
        if (!m_header) return false;
        const QPoint p = e->position().toPoint();
        // The close button lives in the header but must stay clickable.
        if (m_closeBtn && m_closeBtn->geometry().translated(m_header->pos()).contains(p))
            return false;
        return m_header->geometry().contains(p);
    }

    // Make the item whose id matches m_selectedId the current one and scroll it
    // into view. No-op if unset or not found.
    void applySelection() {
        if (!m_list) return;
        for (int i = 0; i < m_list->count(); ++i) {
            QListWidgetItem* item = m_list->item(i);
            if (item->data(Qt::UserRole).toString().toStdString() == m_selectedId) {
                m_list->setCurrentItem(item);
                m_list->scrollToItem(item, QAbstractItemView::PositionAtCenter);
                return;
            }
        }
    }

    void buildAllItems() {
        m_list->clear();
        m_cellWidth = 0;
        m_cellHeight = 0;
        for (const auto& [name, id] : m_allItems) {
            const QString display = QString::fromStdString(name.empty() ? id : name);
            auto* item = new QListWidgetItem(display);
            item->setData(Qt::UserRole, QString::fromStdString(id));
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
            item->setToolTip(display);

            QPixmap px;
            if (id.empty()) {
                // A deliberately quiet clear-reference tile: distinct from a
                // missing thumbnail placeholder, but still the same cell shape.
                px = QPixmap(kThumbSize, kThumbSize);
                px.fill(Qt::transparent);
                QPainter painter(&px);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(QPen(QColor(125, 125, 130), 2.0));
                painter.setBrush(QColor(42, 42, 44));
                painter.drawRoundedRect(QRectF(8, 8, kThumbSize - 16, kThumbSize - 16), 8, 8);
                painter.drawLine(QPointF(23, 23), QPointF(kThumbSize - 23, kThumbSize - 23));
                item->setToolTip("Clear the current reference");
            } else if (m_thumbnailGen) {
                px = m_thumbnailGen(id);
            }

            if (!px.isNull()) {
                item->setIcon(QIcon(px));
            } else if (m_fallbackIconGen) {
                QIcon icon = m_fallbackIconGen(id);
                if (!icon.isNull()) {
                    item->setIcon(icon);
                } else {
                    QPixmap placeholder(kThumbSize, kThumbSize);
                    placeholder.fill(QColor(70, 70, 70));
                    item->setIcon(QIcon(placeholder));
                }
            } else {
                QPixmap placeholder(kThumbSize, kThumbSize);
                placeholder.fill(QColor(70, 70, 70));
                item->setIcon(QIcon(placeholder));
            }
            m_list->addItem(item);
        }
        updateCellLayout();
        if (m_list->count() > 0)
            m_list->setCurrentRow(0);
    }

    // At the slider's small end, use full-width rows with names. Otherwise fit
    // as many thumbnail cells as the viewport allows and stretch them evenly.
    void updateCellLayout() {
        if (!m_list || !m_list->viewport()) return;
        const int available = m_list->viewport()->width();
        if (available <= 0) return;

        const int thumb = m_cellSizeSlider ? m_cellSizeSlider->value() : kThumbSize;
        const bool listMode = thumb <= kListThreshold;
        if (listMode != m_listMode) {
            m_listMode = listMode;
            m_cellWidth = 0;
            m_cellHeight = 0;
            m_list->setViewMode(listMode ? QListView::ListMode : QListView::IconMode);
            m_list->setFlow(listMode ? QListView::TopToBottom : QListView::LeftToRight);
            m_list->setWrapping(!listMode);
            m_list->setMovement(QListView::Static);
            m_list->setResizeMode(QListView::Adjust);
            m_list->setSpacing(0);
            m_list->setGridSize(QSize());
        }
        if (listMode) {
            constexpr int rowHeight = 36;
            m_list->setIconSize(QSize(20, 20));
            if (m_cellWidth == available && m_cellHeight == rowHeight) return;
            m_cellWidth = available;
            m_cellHeight = rowHeight;
            for (int i = 0; i < m_list->count(); ++i) {
                auto* item = m_list->item(i);
                item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                item->setSizeHint(QSize(available, rowHeight));
            }
            return;
        }

        m_list->setIconSize(QSize(thumb, thumb));
        const int minCellWidth = thumb + (m_inline ? 28 : 18);
        const int cellHeight = thumb + (m_inline ? 32 : 30);
        const int columns = std::max(1, available / minCellWidth);
        const int cellWidth = std::max(1, available / columns);
        if (cellWidth == m_cellWidth && cellHeight == m_cellHeight) return;
        m_cellWidth = cellWidth;
        m_cellHeight = cellHeight;

        m_list->setGridSize(QSize(cellWidth, cellHeight));
        for (int i = 0; i < m_list->count(); ++i) {
            auto* item = m_list->item(i);
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
            item->setSizeHint(QSize(cellWidth, cellHeight));
        }
    }

    QWidget*     m_header  = nullptr;
    QLabel*      m_title   = nullptr;
    QPushButton* m_closeBtn = nullptr;
    QLineEdit*   m_search  = nullptr;
    PickerListWidget* m_list = nullptr;
    QWidget*     m_confirmBar = nullptr;
    QPushButton* m_confirmBtn = nullptr;
    QWidget*     m_footer = nullptr;
    QSlider*     m_cellSizeSlider = nullptr;
    VerticalResizeGrip* m_resizeGrip = nullptr;
    std::vector<std::pair<std::string, std::string>> m_allItems;
    std::function<QPixmap(const std::string& id)>    m_thumbnailGen;
    std::function<QIcon(const std::string& id)>      m_fallbackIconGen;
    std::string m_selectedId;
    int    m_cellWidth   = 0;
    int    m_cellHeight  = 0;
    bool   m_multiSelect = false;
    bool   m_persistent  = false;
    bool   m_inline      = false;
    bool   m_listMode    = false;
    bool   m_dragging    = false;
    QPoint m_dragOffset;
};
