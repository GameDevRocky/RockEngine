#pragma once

#include <functional>
#include <string>

#include <QPixmap>
#include <QWidget>

#include "engine/tilemap/Tile.hpp"

// Edits sprite-local polygon points. One unit is one rendered sprite width/height;
// vertex centers stay within the sprite rectangle.
class TileCollisionShapeEditor final : public QWidget {
    Q_OBJECT
public:
    explicit TileCollisionShapeEditor(QWidget* parent = nullptr);

    void SetSprite(const std::string& spriteId);
    void SetPolygon(const TileCollisionPolygon& polygon);
    const TileCollisionPolygon& Polygon() const { return m_polygon; }
    QPointF VertexPosition(std::size_t index) const;

    std::function<void(const TileCollisionPolygon&)> onChanged;

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    QRectF SpriteRect() const;
    QPointF ToWidget(glm::vec2 point) const;
    glm::vec2 FromWidget(QPointF point) const;
    int HitVertex(QPointF point) const;
    int HitEdge(QPointF point) const;
    void Commit(TileCollisionPolygon polygon);

    QPixmap m_sprite;
    TileCollisionPolygon m_polygon = DefaultTileCollisionPolygon();
    TileCollisionPolygon m_dragPolygon;
    int m_dragIndex = -1;
};
