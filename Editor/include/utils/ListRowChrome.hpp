#pragma once
#include <algorithm>
#include <functional>
#include <vector>

#include <QCursor>
#include <QEvent>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "engine/utils/EngineUtils.hpp"

// Shared chrome for editable list rows: the drag handle, the remove button, and
// the drag-to-reorder behaviour behind them.
//
// Both the inspector's generic ListPropertyWidget and the Animator's frame strip
// draw rows of "one element + controls". They had drifted into two different
// answers -- the Animator grew a pair of ↑/↓ buttons and an "×", the inspector a
// bare "−" -- so reordering worked one way in one place and not at all in the
// other. This is the single implementation both now use.
namespace ListRow {

// Two columns of grip dots, the conventional "drag me" affordance. A widget
// rather than a glyph label so the cursor and hit area are exactly the grip and
// not a whole text cell.
class DragHandle : public QWidget {
public:
    static constexpr int kWidth = 14;

    explicit DragHandle(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedWidth(kWidth);
        setCursor(Qt::OpenHandCursor);
        setToolTip("Drag to reorder");
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(130, 130, 130));
        const int cx = width() / 2;
        const int cy = height() / 2;
        for (int row = -1; row <= 1; ++row) {
            p.drawEllipse(QPoint(cx - 3, cy + row * 5), 1, 1);
            p.drawEllipse(QPoint(cx + 3, cy + row * 5), 1, 1);
        }
    }
};

// The destructive action, styled as such: the project's trash icon on a red
// ground. Replaces the "×" that read as "close" rather than "delete".
inline QPushButton* MakeRemoveButton(QWidget* parent) {
    auto* btn = new QPushButton(parent);
    btn->setObjectName("ListRemoveButton");   // styled in default.qss
    btn->setIcon(QIcon(QString::fromStdString(
        EngineUtils::GetAssetPath("Domain/lib/assets/icons/trashcan_icon.png"))));
    btn->setIconSize(QSize(14, 14));
    btn->setFixedSize(24, 22);
    btn->setToolTip("Remove");
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}

// Drag-to-reorder over a vertical stack of rows.
//
// Reordering is committed once, on release, rather than live as the pointer
// crosses each boundary: these rows own child widgets (an asset picker, a hover
// preview) and rebuilding the list mid-drag would delete the handle currently
// holding the mouse grab.
//
// Feedback during the drag is a thin insertion line -- a real child widget moved
// around rather than custom painting, so it composites over the rows without the
// container needing to know it exists.
class ReorderController : public QObject {
public:
    // `rowsHost` owns the rows in a top-to-bottom layout. `onReorder(from, to)`
    // is called on release with list indices, and is responsible for the actual
    // data move and the rebuild.
    ReorderController(QWidget* rowsHost, std::function<void(int, int)> onReorder,
                      QObject* parent)
        : QObject(parent), m_host(rowsHost), m_onReorder(std::move(onReorder)) {}

    // Call once per row, in display order, as rows are built.
    void RegisterRow(QWidget* row, QWidget* handle) {
        m_rows.push_back(row);
        handle->installEventFilter(this);
        m_handles.push_back(handle);
    }

    // Rows are rebuilt wholesale, so the controller is reset alongside them.
    void Clear() {
        m_rows.clear();
        m_handles.clear();
        m_dragFrom = -1;
        hideIndicator();
    }

protected:
    bool eventFilter(QObject* obj, QEvent* e) override {
        auto* handle = qobject_cast<QWidget*>(obj);
        if (!handle) return QObject::eventFilter(obj, e);

        switch (e->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(e);
            if (me->button() != Qt::LeftButton) break;
            m_dragFrom = indexOfHandle(handle);
            if (m_dragFrom < 0) break;
            handle->setCursor(Qt::ClosedHandCursor);
            // Explicit, not Qt's implicit press-grab: consuming the press in a
            // filter means the handle's own mousePressEvent never runs, so
            // relying on the implicit grab to deliver the moves that follow is
            // relying on an ordering detail. This is unambiguous.
            handle->grabMouse();
            showIndicatorAt(m_dragFrom);
            return true;
        }
        case QEvent::MouseMove: {
            if (m_dragFrom < 0) break;
            showIndicatorAt(targetIndexAt(QCursor::pos()));
            return true;
        }
        case QEvent::MouseButtonRelease: {
            if (m_dragFrom < 0) break;
            handle->releaseMouse();
            handle->setCursor(Qt::OpenHandCursor);
            const int to = targetIndexAt(QCursor::pos());
            const int from = m_dragFrom;
            m_dragFrom = -1;
            hideIndicator();
            // An insertion point after the dragged row is the same position it
            // already occupies -- committing it would fire a spurious change and
            // a rebuild for a no-op.
            if (m_onReorder && to != from && to != from + 1)
                m_onReorder(from, to > from ? to - 1 : to);
            return true;
        }
        default: break;
        }
        return QObject::eventFilter(obj, e);
    }

private:
    int indexOfHandle(QWidget* handle) const {
        for (std::size_t i = 0; i < m_handles.size(); ++i)
            if (m_handles[i] == handle) return static_cast<int>(i);
        return -1;
    }

    // The insertion slot the pointer is over: 0..count, where N means "after the
    // last row". Boundaries are row centres, so the target flips as soon as the
    // pointer passes the middle of a neighbour.
    int targetIndexAt(const QPoint& globalPos) const {
        if (!m_host || m_rows.empty()) return 0;
        const int y = m_host->mapFromGlobal(globalPos).y();
        for (std::size_t i = 0; i < m_rows.size(); ++i) {
            const QWidget* row = m_rows[i];
            if (!row) continue;
            if (y < row->y() + row->height() / 2) return static_cast<int>(i);
        }
        return static_cast<int>(m_rows.size());
    }

    void showIndicatorAt(int slot) {
        if (!m_host) return;
        if (!m_indicator) {
            m_indicator = new QWidget(m_host);
            m_indicator->setFixedHeight(2);
            m_indicator->setStyleSheet("background: rgb(87, 126, 100);");
            m_indicator->setAttribute(Qt::WA_TransparentForMouseEvents);
        }
        int y = 0;
        if (m_rows.empty())                                  y = 0;
        else if (slot >= static_cast<int>(m_rows.size()))    y = m_rows.back()->geometry().bottom();
        else if (m_rows[slot])                               y = m_rows[slot]->y();

        m_indicator->setGeometry(0, y - 1, m_host->width(), 2);
        m_indicator->show();
        m_indicator->raise();
    }

    void hideIndicator() { if (m_indicator) m_indicator->hide(); }

    QWidget* m_host = nullptr;
    std::function<void(int, int)> m_onReorder;
    std::vector<QWidget*> m_rows;
    std::vector<QWidget*> m_handles;
    QWidget* m_indicator = nullptr;
    int m_dragFrom = -1;
};

} // namespace ListRow
