#include "utils/SpriteHoverColumn.hpp"
#include "utils/DragDropMime.hpp"

#include <QPainter>
#include <QLinearGradient>
#include <QWheelEvent>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QHideEvent>
#include <QPropertyAnimation>
#include <QDrag>
#include <QMimeData>
#include <QApplication>
#include <algorithm>

namespace {
    // Peak opacity of the vertical alpha mask (the "semi-transparent middle").
    constexpr float kPeakAlpha = 0.9f;
    // Fraction of the height over which the mask ramps in/out at each edge. Larger
    // == the top/bottom fade covers more of the column, leaving a smaller solid band.
    constexpr float kEdgeFade  = 0.45f;
}

SpriteHoverColumn::SpriteHoverColumn(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMouseTracking(true);

    m_fade = new QPropertyAnimation(this, "windowOpacity", this);
    m_fade->setDuration(160);
}

int SpriteHoverColumn::maxScroll() const {
    // Scroll range runs from the first cell centred (0) to the last cell centred.
    return std::max(0, (static_cast<int>(m_thumbs.size()) - 1) * kCellH);
}

void SpriteHoverColumn::FadeTo(double target, bool hideAtEnd) {
    m_fade->stop();
    QObject::disconnect(m_fadeConn);
    if (hideAtEnd)
        m_fadeConn = connect(m_fade, &QPropertyAnimation::finished, this, [this]() { hide(); });
    m_fade->setStartValue(windowOpacity());
    m_fade->setEndValue(target);
    m_fade->start();
}

void SpriteHoverColumn::FadeOut() {
    if (!isVisible()) return;
    FadeTo(0.0, true);
}

void SpriteHoverColumn::SetSprites(std::vector<QPixmap> thumbs, std::vector<QString> names,
                                   std::vector<std::string> ids) {
    m_thumbs = std::move(thumbs);
    m_names  = std::move(names);
    m_ids    = std::move(ids);
    m_scroll = 0;
    m_pressIndex = -1;
    update();
}

void SpriteHoverColumn::ShowOverCell(const QRect& globalCellRect) {
    if (m_thumbs.empty()) { FadeOut(); return; }

    const int rows = std::min(static_cast<int>(m_thumbs.size()), kMaxRows);
    const int w = kCellW;
    const int h = rows * kCellH;
    setFixedSize(w, h);

    // Centre the column on the hovered cell so it appears "over" it.
    const QPoint c = globalCellRect.center();
    move(c.x() - w / 2, c.y() - h / 2);
    m_scroll = 0;

    const bool wasVisible = isVisible();
    if (!wasVisible) setWindowOpacity(0.0);   // start transparent for the fade-in
    show();
    raise();
    update();
    FadeTo(1.0, false);
}

void SpriteHoverColumn::paintEvent(QPaintEvent*) {
    if (m_thumbs.empty()) return;

    const int W = width();
    const int H = height();

    // Render the panel + sprites into an offscreen buffer, then multiply its alpha
    // by a vertical gradient (CompositionMode_DestinationIn) so the whole column
    // fades to transparent at the top and bottom.
    QPixmap buf(size());
    buf.fill(Qt::transparent);
    {
        QPainter bp(&buf);
        bp.setRenderHint(QPainter::SmoothPixmapTransform, true);

        // Dark backing panel (its alpha is faded by the mask below).
        bp.fillRect(QRect(0, 0, W, H), QColor(24, 24, 26, 255));

        const int pad = topPad();   // headroom so the ends can reach the centre
        for (int i = 0; i < static_cast<int>(m_thumbs.size()); ++i) {
            const int cellTop = pad + i * kCellH - m_scroll;
            if (cellTop + kCellH < 0 || cellTop > H) continue;   // off-screen row

            const QPixmap& thumb = m_thumbs[i];
            if (!thumb.isNull()) {
                QPixmap sc = thumb.scaled(kThumb, kThumb, Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation);
                const int tx = (W - sc.width()) / 2;
                const int ty = cellTop + 8 + (kThumb - sc.height()) / 2;
                bp.drawPixmap(tx, ty, sc);
            } else {
                bp.fillRect(QRect((W - kThumb) / 2, cellTop + 8, kThumb, kThumb),
                            QColor(70, 70, 70));
            }

            if (i < static_cast<int>(m_names.size())) {
                const QRect textArea(2, cellTop + kThumb + 12, W - 4,
                                     kCellH - kThumb - 12);
                bp.setPen(QColor(220, 220, 220));
                const QString el = bp.fontMetrics().elidedText(
                    m_names[i], Qt::ElideRight, textArea.width());
                bp.drawText(textArea, Qt::AlignHCenter | Qt::AlignTop, el);
            }
        }

        // Vertical alpha mask: 0 at edges, kPeakAlpha through the middle.
        const int peak = static_cast<int>(255 * kPeakAlpha);
        QLinearGradient g(0, 0, 0, H);
        g.setColorAt(0.0,             QColor(0, 0, 0, 0));
        g.setColorAt(kEdgeFade,       QColor(0, 0, 0, peak));
        g.setColorAt(1.0 - kEdgeFade, QColor(0, 0, 0, peak));
        g.setColorAt(1.0,             QColor(0, 0, 0, 0));
        bp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        bp.fillRect(QRect(0, 0, W, H), g);
    }

    QPainter p(this);
    p.drawPixmap(0, 0, buf);
}

void SpriteHoverColumn::wheelEvent(QWheelEvent* event) {
    m_scroll = std::clamp(m_scroll - event->angleDelta().y(), 0, maxScroll());
    update();
    event->accept();
}

void SpriteHoverColumn::enterEvent(QEnterEvent*) { emit hovered(); }
void SpriteHoverColumn::leaveEvent(QEvent*)      { emit unhovered(); }

void SpriteHoverColumn::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    emit columnHidden();
}

int SpriteHoverColumn::indexAt(const QPoint& localPos) const {
    const int rel = localPos.y() + m_scroll - topPad();
    if (rel < 0) return -1;
    const int i = rel / kCellH;
    return (i >= 0 && i < static_cast<int>(m_ids.size())) ? i : -1;
}

void SpriteHoverColumn::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_pressPos = event->pos();
        m_pressIndex = indexAt(event->pos());
    }
}

void SpriteHoverColumn::mouseMoveEvent(QMouseEvent* event) {
    if (!(event->buttons() & Qt::LeftButton) || m_pressIndex < 0) return;
    if ((event->pos() - m_pressPos).manhattanLength() < QApplication::startDragDistance()) return;
    startDrag(m_pressIndex);
}

void SpriteHoverColumn::startDrag(int index) {
    if (index < 0 || index >= static_cast<int>(m_ids.size())) return;

    QDrag* drag = new QDrag(this);
    auto* mime = new QMimeData();
    mime->setData(kSpriteMimeType, QString::fromStdString(m_ids[index]).toUtf8());
    drag->setMimeData(mime);

    if (index < static_cast<int>(m_thumbs.size()) && !m_thumbs[index].isNull()) {
        QPixmap pm = m_thumbs[index].scaled(kThumb, kThumb, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation);
        drag->setPixmap(pm);
        drag->setHotSpot(QPoint(pm.width() / 2, pm.height() / 2));
    }

    m_pressIndex = -1;
    hide();   // dismiss the column while the sprite is being dragged
    drag->exec(Qt::CopyAction);
}
