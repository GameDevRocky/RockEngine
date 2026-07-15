#include "dock-widgets/SpriteCanvasWidget.hpp"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QLineF>
#include <algorithm>
#include <cmath>

#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/Sprite.hpp"

namespace {
    constexpr double kHandlePx = 4.0;   // half-size of a resize handle, in screen px
    constexpr double kPivotPx  = 6.0;   // pivot marker radius, in screen px
    constexpr float  kMinPx    = 1.0f;  // minimum sprite size in image px
}

SpriteCanvasWidget::SpriteCanvasWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(200, 200);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

// ---------------------------------------------------------------- conversions

QPointF SpriteCanvasWidget::ImageToScreen(const glm::vec2& p) const {
    return QPointF(p.x * m_zoom + m_panOffset.x(), p.y * m_zoom + m_panOffset.y());
}

glm::vec2 SpriteCanvasWidget::ScreenToImage(const QPointF& s) const {
    if (m_zoom <= 0.0f) return {0, 0};
    return { static_cast<float>((s.x() - m_panOffset.x()) / m_zoom),
             static_cast<float>((s.y() - m_panOffset.y()) / m_zoom) };
}

glm::vec2 SpriteCanvasWidget::UVToImage(const glm::vec2& uv) const {
    const float W = static_cast<float>(m_pixmap.width());
    const float H = static_cast<float>(m_pixmap.height());
    return { uv.x * W, (1.0f - uv.y) * H };
}

glm::vec2 SpriteCanvasWidget::ImageToUV(const glm::vec2& px) const {
    const float W = static_cast<float>(m_pixmap.width());
    const float H = static_cast<float>(m_pixmap.height());
    if (W <= 0.0f || H <= 0.0f) return {0, 0};
    return { px.x / W, 1.0f - px.y / H };
}

QRectF SpriteCanvasWidget::ImageRectToScreen(const QRectF& r) const {
    QPointF tl = ImageToScreen({ static_cast<float>(r.left()),  static_cast<float>(r.top())    });
    QPointF br = ImageToScreen({ static_cast<float>(r.right()), static_cast<float>(r.bottom()) });
    return QRectF(tl, br);
}

// Image-space (top-left origin) rect of a sprite, derived from its UV rect.
QRectF SpriteCanvasWidget::SpriteImageRect(Sprite* s) const {
    if (!s) return {};
    glm::vec2 uvMin = s->GetUVMin();
    glm::vec2 uvMax = s->GetUVMax();
    const float W = static_cast<float>(m_pixmap.width());
    const float H = static_cast<float>(m_pixmap.height());
    double left   = uvMin.x * W;
    double right  = uvMax.x * W;
    double top    = (1.0 - uvMax.y) * H;   // uvMax.y (top of region) -> smaller image Y
    double bottom = (1.0 - uvMin.y) * H;
    return QRectF(QPointF(left, top), QPointF(right, bottom));
}

// ---------------------------------------------------------------- helpers

std::vector<Sprite*> SpriteCanvasWidget::SpritesForTexture() const {
    std::vector<Sprite*> out;
    if (!m_texture) return out;
    for (const auto& [id, sprite] : AssetManager::Get().GetAllSprites())
        if (sprite && sprite->GetTextureID() == m_texture->GetID())
            out.push_back(sprite);
    return out;
}

Sprite* SpriteCanvasWidget::SelectedSprite() const {
    if (m_selectedSpriteId.empty()) return nullptr;
    Sprite* s = AssetManager::Get().GetSprite(m_selectedSpriteId);
    if (s && m_texture && s->GetTextureID() == m_texture->GetID()) return s;
    return nullptr;
}

// Smallest sprite whose screen rect contains the point (so nested regions stay reachable).
Sprite* SpriteCanvasWidget::SpriteAtScreen(const QPointF& p) const {
    Sprite* best = nullptr;
    double bestArea = 0.0;
    for (Sprite* s : SpritesForTexture()) {
        QRectF r = ImageRectToScreen(SpriteImageRect(s));
        if (!r.contains(p)) continue;
        double area = r.width() * r.height();
        if (!best || area < bestArea) { best = s; bestArea = area; }
    }
    return best;
}

int SpriteCanvasWidget::HandleAtScreen(Sprite* s, const QPointF& p) const {
    if (!s) return -1;
    QRectF r = ImageRectToScreen(SpriteImageRect(s));
    const double cx = r.center().x();
    const double cy = r.center().y();
    QPointF handles[8] = {
        {r.left(),  r.top()},    {cx, r.top()},    {r.right(), r.top()},
        {r.right(), cy},         {r.right(), r.bottom()}, {cx, r.bottom()},
        {r.left(),  r.bottom()}, {r.left(),  cy}
    };
    for (int i = 0; i < 8; ++i) {
        QRectF hit(handles[i].x() - kHandlePx * 2, handles[i].y() - kHandlePx * 2,
                   kHandlePx * 4, kHandlePx * 4);
        if (hit.contains(p)) return i;
    }
    return -1;
}

QPointF SpriteCanvasWidget::PivotScreenPos(Sprite* s) const {
    if (!s) return {};
    glm::vec2 uvMin = s->GetUVMin();
    glm::vec2 uvMax = s->GetUVMax();
    glm::vec2 pivot = s->GetPivot();
    glm::vec2 pivotUV = uvMin + pivot * (uvMax - uvMin);
    return ImageToScreen(UVToImage(pivotUV));
}

bool SpriteCanvasWidget::PivotAtScreen(Sprite* s, const QPointF& p) const {
    if (!s) return false;
    QPointF pv = PivotScreenPos(s);
    return QLineF(pv, p).length() <= kPivotPx * 2.0;
}

glm::vec2 SpriteCanvasWidget::MaybeSnap(const glm::vec2& px) const {
    if (!m_snapToPixel) return px;
    return { std::round(px.x), std::round(px.y) };
}

QRectF SpriteCanvasWidget::ClampToImage(const QRectF& r) const {
    const double W = m_pixmap.width();
    const double H = m_pixmap.height();
    double left   = std::clamp(r.left(),   0.0, W);
    double top    = std::clamp(r.top(),    0.0, H);
    double right  = std::clamp(r.right(),  0.0, W);
    double bottom = std::clamp(r.bottom(), 0.0, H);
    return QRectF(QPointF(left, top), QPointF(right, bottom)).normalized();
}

// ---------------------------------------------------------------- view

void SpriteCanvasWidget::SetTexture(Texture2D* tex) {
    if (tex == m_texture) return;             // keep view when nothing changed
    m_texture = tex;
    m_selectedSpriteId.clear();
    m_drag = Drag::None;
    m_pixmap = QPixmap();
    if (m_texture) {
        const std::string& path = m_texture->GetPath();
        if (!path.empty()) m_pixmap = QPixmap(QString::fromStdString(path));
    }
    // Fit now if we already have a real size; otherwise defer to the first resizeEvent
    // (the canvas may be created before it is shown, e.g. inside the editor modal).
    m_needsFit = true;
    if (width() > 0 && height() > 0 && !m_pixmap.isNull()) {
        FitView();
        m_needsFit = false;
    }
    update();
}

void SpriteCanvasWidget::resizeEvent(QResizeEvent*) {
    if (m_needsFit && !m_pixmap.isNull() && width() > 0 && height() > 0) {
        FitView();
        m_needsFit = false;
    }
}

void SpriteCanvasWidget::SetSelectedSprite(const std::string& id) {
    if (m_selectedSpriteId == id) return;
    m_selectedSpriteId = id;
    update();
}

void SpriteCanvasWidget::FitView() {
    if (m_pixmap.isNull() || width() <= 0 || height() <= 0) {
        m_zoom = 1.0f;
        m_panOffset = QPointF(0, 0);
        return;
    }
    const float zx = static_cast<float>(width())  / m_pixmap.width();
    const float zy = static_cast<float>(height()) / m_pixmap.height();
    m_zoom = std::min(zx, zy) * 0.9f;
    m_panOffset = QPointF((width()  - m_pixmap.width()  * m_zoom) / 2.0,
                          (height() - m_pixmap.height() * m_zoom) / 2.0);
}

// ---------------------------------------------------------------- paint

void SpriteCanvasWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 32));

    if (m_pixmap.isNull()) {
        painter.setPen(QColor(150, 150, 150));
        painter.drawText(rect(), Qt::AlignCenter,
                         m_texture ? "Texture image could not be loaded"
                                   : "Select a texture to edit its sprites");
        return;
    }

    // The sheet.
    QRectF dst = ImageRectToScreen(QRectF(0, 0, m_pixmap.width(), m_pixmap.height()));
    painter.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0f);
    painter.drawPixmap(dst, m_pixmap, m_pixmap.rect());

    Sprite* selected = SelectedSprite();

    // Sprite regions.
    for (Sprite* s : SpritesForTexture()) {
        QRectF imgRect = (s == selected && (m_drag == Drag::Move || m_drag == Drag::Resize))
                             ? m_pendingRect
                             : SpriteImageRect(s);
        QRectF r = ImageRectToScreen(imgRect);
        const bool isSel = (s == selected);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(isSel ? QColor(80, 180, 255) : QColor(90, 200, 120),
                            isSel ? 2.0 : 1.0));
        painter.drawRect(r);

        painter.setPen(isSel ? QColor(200, 230, 255) : QColor(180, 220, 190));
        painter.drawText(r.adjusted(2, 1, -2, -2), Qt::AlignTop | Qt::AlignLeft,
                         QString::fromStdString(s->GetName()));
    }

    // Handles + pivot for the selected sprite.
    if (selected) {
        QRectF imgRect = (m_drag == Drag::Move || m_drag == Drag::Resize)
                             ? m_pendingRect : SpriteImageRect(selected);
        QRectF r = ImageRectToScreen(imgRect);
        const double cx = r.center().x();
        const double cy = r.center().y();
        QPointF handles[8] = {
            {r.left(), r.top()}, {cx, r.top()}, {r.right(), r.top()},
            {r.right(), cy}, {r.right(), r.bottom()}, {cx, r.bottom()},
            {r.left(), r.bottom()}, {r.left(), cy}
        };
        painter.setBrush(QColor(80, 180, 255));
        painter.setPen(QPen(QColor(20, 40, 60), 1.0));
        for (const QPointF& h : handles)
            painter.drawRect(QRectF(h.x() - kHandlePx, h.y() - kHandlePx,
                                    kHandlePx * 2, kHandlePx * 2));

        // Pivot crosshair.
        glm::vec2 pivot = (m_drag == Drag::MovePivot) ? m_pendingPivot : selected->GetPivot();
        glm::vec2 uvMin = selected->GetUVMin();
        glm::vec2 uvMax = selected->GetUVMax();
        QPointF pv = ImageToScreen(UVToImage(uvMin + pivot * (uvMax - uvMin)));
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 200, 60), 1.5));
        painter.drawEllipse(pv, kPivotPx, kPivotPx);
        painter.drawLine(pv + QPointF(-kPivotPx - 3, 0), pv + QPointF(kPivotPx + 3, 0));
        painter.drawLine(pv + QPointF(0, -kPivotPx - 3), pv + QPointF(0, kPivotPx + 3));
    }

    // Rubber band for a new region.
    if (m_drag == Drag::DrawNew) {
        QRectF r = ImageRectToScreen(m_pendingRect);
        painter.setBrush(QColor(80, 180, 255, 40));
        painter.setPen(QPen(QColor(80, 180, 255), 1.0, Qt::DashLine));
        painter.drawRect(r);
    }
}

// ---------------------------------------------------------------- mouse

void SpriteCanvasWidget::mousePressEvent(QMouseEvent* e) {
    m_lastMouse = e->position();

    if (e->button() == Qt::MiddleButton) {
        m_drag = Drag::Pan;
        return;
    }
    if (e->button() != Qt::LeftButton || m_pixmap.isNull()) return;

    const QPointF p = e->position();
    Sprite* selected = SelectedSprite();

    if (m_editable && selected) {
        int handle = HandleAtScreen(selected, p);
        if (handle >= 0) {
            m_drag = Drag::Resize;
            m_activeHandle = handle;
            m_dragOrigRect = SpriteImageRect(selected);
            m_pendingRect = m_dragOrigRect;
            return;
        }
        if (PivotAtScreen(selected, p)) {
            m_drag = Drag::MovePivot;
            m_pendingPivot = selected->GetPivot();
            return;
        }
    }

    if (Sprite* hit = SpriteAtScreen(p)) {
        m_selectedSpriteId = hit->GetID();
        emit spriteSelected(QString::fromStdString(m_selectedSpriteId));
        if (m_editable) {
            m_drag = Drag::Move;
            m_dragOrigRect = SpriteImageRect(hit);
            m_pendingRect = m_dragOrigRect;
            m_grabImg = ScreenToImage(p);
        }
        update();
        return;
    }

    // Empty area.
    if (m_editable) {
        m_drag = Drag::DrawNew;
        m_grabImg = MaybeSnap(ScreenToImage(p));
        m_pendingRect = QRectF(m_grabImg.x, m_grabImg.y, 0.0, 0.0);
    }
    update();
}

void SpriteCanvasWidget::mouseMoveEvent(QMouseEvent* e) {
    const QPointF p = e->position();

    switch (m_drag) {
        case Drag::Pan: {
            m_panOffset += (p - m_lastMouse);
            m_lastMouse = p;
            update();
            return;
        }
        case Drag::Move: {
            glm::vec2 cur = ScreenToImage(p);
            glm::vec2 delta = cur - m_grabImg;
            QRectF r = m_dragOrigRect.translated(delta.x, delta.y);
            if (m_snapToPixel)
                r.moveTo(std::round(r.left()), std::round(r.top()));
            // keep fully inside the image
            const double W = m_pixmap.width(), H = m_pixmap.height();
            if (r.left() < 0)  r.moveLeft(0);
            if (r.top() < 0)   r.moveTop(0);
            if (r.right() > W) r.moveRight(W);
            if (r.bottom() > H) r.moveBottom(H);
            m_pendingRect = r;
            update();
            return;
        }
        case Drag::Resize: {
            glm::vec2 cur = MaybeSnap(ScreenToImage(p));
            double left = m_dragOrigRect.left(), right = m_dragOrigRect.right();
            double top = m_dragOrigRect.top(), bottom = m_dragOrigRect.bottom();
            const int h = m_activeHandle;
            if (h == 0 || h == 6 || h == 7) left = cur.x;          // left edge
            if (h == 2 || h == 3 || h == 4) right = cur.x;         // right edge
            if (h == 0 || h == 1 || h == 2) top = cur.y;           // top edge
            if (h == 4 || h == 5 || h == 6) bottom = cur.y;        // bottom edge
            if (right - left < kMinPx) { if (h == 0 || h == 6 || h == 7) left = right - kMinPx; else right = left + kMinPx; }
            if (bottom - top < kMinPx) { if (h == 0 || h == 1 || h == 2) top = bottom - kMinPx; else bottom = top + kMinPx; }
            m_pendingRect = ClampToImage(QRectF(QPointF(left, top), QPointF(right, bottom)));
            update();
            return;
        }
        case Drag::DrawNew: {
            glm::vec2 cur = MaybeSnap(ScreenToImage(p));
            QRectF r(QPointF(m_grabImg.x, m_grabImg.y), QPointF(cur.x, cur.y));
            m_pendingRect = ClampToImage(r.normalized());
            update();
            return;
        }
        case Drag::MovePivot: {
            Sprite* s = SelectedSprite();
            if (s) {
                glm::vec2 uvMin = s->GetUVMin();
                glm::vec2 uvMax = s->GetUVMax();
                glm::vec2 uv = ImageToUV(ScreenToImage(p));
                glm::vec2 span = uvMax - uvMin;
                glm::vec2 pivot{
                    span.x != 0.0f ? (uv.x - uvMin.x) / span.x : 0.0f,
                    span.y != 0.0f ? (uv.y - uvMin.y) / span.y : 0.0f
                };
                m_pendingPivot = { std::clamp(pivot.x, 0.0f, 1.0f),
                                   std::clamp(pivot.y, 0.0f, 1.0f) };
            }
            update();
            return;
        }
        default:
            return;
    }
}

void SpriteCanvasWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (m_drag == Drag::None) return;
    if (m_drag == Drag::Pan) { m_drag = Drag::None; return; }

    if (m_drag == Drag::DrawNew) {
        QRectF r = m_pendingRect.normalized();
        if (r.width() >= 2.0 && r.height() >= 2.0 && m_texture) {
            glm::vec2 uvMin{ static_cast<float>(r.left()  / m_pixmap.width()),
                             1.0f - static_cast<float>(r.bottom() / m_pixmap.height()) };
            glm::vec2 uvMax{ static_cast<float>(r.right() / m_pixmap.width()),
                             1.0f - static_cast<float>(r.top()    / m_pixmap.height()) };
            Sprite* s = AssetManager::Get().CreateSprite(m_texture->GetID(), uvMin, uvMax);
            if (s) {
                m_selectedSpriteId = s->GetID();
                emit spriteSelected(QString::fromStdString(m_selectedSpriteId));
                emit spritesChanged();
            }
        } else {
            // Treat a tiny drag as a click on empty space -> deselect.
            m_selectedSpriteId.clear();
            emit spriteSelected(QString());
        }
        m_drag = Drag::None;
        update();
        return;
    }

    CommitEdit();
    m_drag = Drag::None;
    update();
}

void SpriteCanvasWidget::CommitEdit() {
    Sprite* s = SelectedSprite();
    if (!s) return;

    if (m_drag == Drag::Move || m_drag == Drag::Resize) {
        QRectF r = m_pendingRect.normalized();
        const double W = m_pixmap.width(), H = m_pixmap.height();
        if (W <= 0 || H <= 0) return;
        glm::vec2 uvMin{ std::clamp(static_cast<float>(r.left()  / W), 0.0f, 1.0f),
                         std::clamp(1.0f - static_cast<float>(r.bottom() / H), 0.0f, 1.0f) };
        glm::vec2 uvMax{ std::clamp(static_cast<float>(r.right() / W), 0.0f, 1.0f),
                         std::clamp(1.0f - static_cast<float>(r.top()    / H), 0.0f, 1.0f) };
        s->SetUVMin(uvMin);
        s->SetUVMax(uvMax);
        emit spritesChanged();
    } else if (m_drag == Drag::MovePivot) {
        s->SetPivot(m_pendingPivot);
        emit spritesChanged();
    }
}

void SpriteCanvasWidget::wheelEvent(QWheelEvent* e) {
    if (m_pixmap.isNull()) return;
    const double factor = e->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    const float oldZoom = m_zoom;
    m_zoom = std::clamp(m_zoom * static_cast<float>(factor), 0.05f, 64.0f);

    // Keep the image point under the cursor fixed.
    const QPointF cursor = e->position();
    m_panOffset = cursor - (cursor - m_panOffset) * (m_zoom / oldZoom);
    update();
}
