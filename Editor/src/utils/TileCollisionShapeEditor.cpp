#include "utils/TileCollisionShapeEditor.hpp"

#include <algorithm>
#include <cmath>

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "utils/AssetThumbnails.hpp"

namespace {
constexpr qreal kHandleRadius = 6.0;
constexpr qreal kHitRadius = 10.0;
constexpr qreal kEdgeHitDistance = 8.0;
}

TileCollisionShapeEditor::TileCollisionShapeEditor(QWidget* parent) : QWidget(parent) {
    setFixedSize(240, 240);
    setMouseTracking(true);
    setToolTip("Drag points within the sprite; double-click an edge to add; right-click a point to remove (3–8 points)");
}

void TileCollisionShapeEditor::SetSprite(const std::string& spriteId) {
    m_sprite = spriteId.empty() ? QPixmap() : AssetThumbnails::forSpritePreview(spriteId);
    update();
}

void TileCollisionShapeEditor::SetPolygon(const TileCollisionPolygon& polygon) {
    m_polygon = IsValidTileCollisionPolygon(polygon) ? polygon : DefaultTileCollisionPolygon();
    m_dragIndex = -1;
    m_dragPolygon.clear();
    update();
}

QRectF TileCollisionShapeEditor::SpriteRect() const {
    constexpr qreal maxSize = 120.0;
    const qreal aspect = m_sprite.isNull() ? 1.0
        : static_cast<qreal>(m_sprite.width()) / m_sprite.height();
    const qreal width = aspect >= 1.0 ? maxSize : maxSize * aspect;
    const qreal height = aspect >= 1.0 ? maxSize / aspect : maxSize;
    return QRectF((this->width() - width) * 0.5, (this->height() - height) * 0.5,
                  width, height);
}

QPointF TileCollisionShapeEditor::ToWidget(glm::vec2 point) const {
    const QRectF rect = SpriteRect();
    return {rect.center().x() + point.x * rect.width(),
            rect.center().y() - point.y * rect.height()};
}

glm::vec2 TileCollisionShapeEditor::FromWidget(QPointF point) const {
    const QRectF rect = SpriteRect();
    return {static_cast<float>(std::clamp((point.x() - rect.center().x()) / rect.width(),
                                         -0.5, 0.5)),
            static_cast<float>(std::clamp((rect.center().y() - point.y()) / rect.height(),
                                         -0.5, 0.5))};
}

QPointF TileCollisionShapeEditor::VertexPosition(std::size_t index) const {
    return index < m_polygon.size() ? ToWidget(m_polygon[index]) : QPointF();
}

int TileCollisionShapeEditor::HitVertex(QPointF point) const {
    const auto& polygon = m_dragIndex >= 0 ? m_dragPolygon : m_polygon;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const QPointF delta = ToWidget(polygon[i]) - point;
        if (delta.x() * delta.x() + delta.y() * delta.y() <= kHitRadius * kHitRadius)
            return static_cast<int>(i);
    }
    return -1;
}

int TileCollisionShapeEditor::HitEdge(QPointF point) const {
    for (std::size_t i = 0; i < m_polygon.size(); ++i) {
        const QPointF a = ToWidget(m_polygon[i]);
        const QPointF b = ToWidget(m_polygon[(i + 1) % m_polygon.size()]);
        const QPointF edge = b - a;
        const qreal lengthSquared = QPointF::dotProduct(edge, edge);
        if (lengthSquared <= 0) continue;
        const qreal t = std::clamp(QPointF::dotProduct(point - a, edge) / lengthSquared,
                                   0.0, 1.0);
        if (t <= 0.05 || t >= 0.95) continue;
        const QPointF delta = point - (a + edge * t);
        if (QPointF::dotProduct(delta, delta) <= kEdgeHitDistance * kEdgeHitDistance)
            return static_cast<int>(i);
    }
    return -1;
}

void TileCollisionShapeEditor::Commit(TileCollisionPolygon polygon) {
    if (!IsValidTileCollisionPolygon(polygon) || polygon == m_polygon) return;
    m_polygon = std::move(polygon);
    update();
    if (onChanged) onChanged(m_polygon);
}

void TileCollisionShapeEditor::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(35, 35, 35));
    painter.setPen(QColor(75, 75, 75));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    const QRectF spriteRect = SpriteRect();
    painter.fillRect(spriteRect, QColor(25, 25, 25));
    if (!m_sprite.isNull()) painter.drawPixmap(spriteRect, m_sprite, m_sprite.rect());
    painter.setPen(QPen(QColor(110, 110, 110), 1, Qt::DashLine));
    painter.drawRect(spriteRect);

    const auto& polygon = m_dragIndex >= 0 ? m_dragPolygon : m_polygon;
    const bool valid = IsValidTileCollisionPolygon(polygon);
    const QColor color = valid ? QColor(95, 230, 100) : QColor(240, 95, 95);
    QPainterPath path;
    path.moveTo(ToWidget(polygon.front()));
    for (std::size_t i = 1; i < polygon.size(); ++i) path.lineTo(ToWidget(polygon[i]));
    path.closeSubpath();
    painter.fillPath(path, QColor(color.red(), color.green(), color.blue(), 35));
    painter.setPen(QPen(color, 2));
    painter.drawPath(path);
    painter.setBrush(color);
    for (glm::vec2 vertex : polygon) painter.drawEllipse(ToWidget(vertex), kHandleRadius, kHandleRadius);
}

void TileCollisionShapeEditor::mousePressEvent(QMouseEvent* event) {
    const int vertex = HitVertex(event->position());
    if (event->button() == Qt::RightButton) {
        if (vertex >= 0 && m_polygon.size() > 3) {
            TileCollisionPolygon next = m_polygon;
            next.erase(next.begin() + vertex);
            Commit(std::move(next));
        }
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && vertex >= 0) {
        m_dragIndex = vertex;
        m_dragPolygon = m_polygon;
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TileCollisionShapeEditor::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragIndex >= 0) {
        m_dragPolygon[m_dragIndex] = FromWidget(event->position());
        update();
        event->accept();
        return;
    }
    setCursor(HitVertex(event->position()) >= 0 ? Qt::SizeAllCursor
              : HitEdge(event->position()) >= 0 ? Qt::CrossCursor : Qt::ArrowCursor);
    QWidget::mouseMoveEvent(event);
}

void TileCollisionShapeEditor::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_dragIndex >= 0) {
        TileCollisionPolygon next = std::move(m_dragPolygon);
        m_dragIndex = -1;
        Commit(std::move(next));
        update();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void TileCollisionShapeEditor::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_polygon.size() < 8 &&
        HitVertex(event->position()) < 0) {
        const int edge = HitEdge(event->position());
        if (edge >= 0) {
            TileCollisionPolygon next = m_polygon;
            // Project onto the edge so adding a point preserves convexity.
            const glm::vec2 a = next[edge];
            const glm::vec2 b = next[(edge + 1) % next.size()];
            const glm::vec2 p = FromWidget(event->position());
            const float t = std::clamp(glm::dot(p - a, b - a) / glm::dot(b - a, b - a),
                                       0.0f, 1.0f);
            next.insert(next.begin() + edge + 1, a + (b - a) * t);
            Commit(std::move(next));
            event->accept();
            return;
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TileCollisionShapeEditor::contextMenuEvent(QContextMenuEvent* event) {
    event->accept();
}
