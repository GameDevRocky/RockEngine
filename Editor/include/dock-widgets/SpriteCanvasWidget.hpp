#pragma once
#include <QWidget>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <string>
#include <vector>
#include <glm/glm.hpp>

class Texture2D;
class Sprite;

// Interactive QPainter canvas that draws a texture (sprite sheet) and its sprite
// regions as draggable rectangles, plus a pivot handle. Editing mutates the engine
// Sprite objects directly (SetUVMin/SetUVMax/SetPivot), which auto-save the parent
// texture. Creating a new region calls AssetManager::CreateSprite.
//
// Coordinate spaces:
//   * image px  — [0..W]x[0..H], TOP-LEFT origin (QPixmap/QPainter space)
//   * UV        — [0..1], BOTTOM-LEFT origin (textures load vertically flipped), so
//                 conversions flip Y: uv.y = 1 - py/H  and  py = (1-uv.y)*H
//   * screen    — image px * zoom + panOffset
class SpriteCanvasWidget : public QWidget {
    Q_OBJECT
public:
    explicit SpriteCanvasWidget(QWidget* parent = nullptr);

    // Set the sheet being edited. Re-loads the pixmap and fits the view only when the
    // texture actually changes (so re-selecting a sprite doesn't reset the view).
    void SetTexture(Texture2D* tex);
    Texture2D* GetTexture() const { return m_texture; }

    void SetSelectedSprite(const std::string& id);
    const std::string& GetSelectedSprite() const { return m_selectedSpriteId; }

    void SetSnapToPixel(bool on) { m_snapToPixel = on; update(); }
    bool GetSnapToPixel() const { return m_snapToPixel; }

    // Editing is disabled during play mode (assets are shared across containers).
    void SetEditable(bool on) { m_editable = on; }

    void FitView();

signals:
    void spriteSelected(const QString& id);   // "" means deselect
    void spritesChanged();                     // a sprite was created/edited

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    enum class Drag { None, Move, Resize, DrawNew, MovePivot, Pan };

    // --- coordinate conversions ---
    QPointF ImageToScreen(const glm::vec2& imgPx) const;
    glm::vec2 ScreenToImage(const QPointF& screen) const;
    glm::vec2 UVToImage(const glm::vec2& uv) const;     // flips Y
    glm::vec2 ImageToUV(const glm::vec2& imgPx) const;  // flips Y
    QRectF ImageRectToScreen(const QRectF& imgRect) const;
    QRectF SpriteImageRect(Sprite* s) const;            // image-px rect of a sprite

    // --- helpers ---
    std::vector<Sprite*> SpritesForTexture() const;
    Sprite* SelectedSprite() const;
    Sprite* SpriteAtScreen(const QPointF& screen) const;
    int HandleAtScreen(Sprite* s, const QPointF& screen) const;   // 0..7 or -1
    bool PivotAtScreen(Sprite* s, const QPointF& screen) const;
    QPointF PivotScreenPos(Sprite* s) const;
    glm::vec2 MaybeSnap(const glm::vec2& imgPx) const;
    QRectF ClampToImage(const QRectF& imgRect) const;
    void CommitEdit();                                   // pending rect/pivot -> engine

    Texture2D* m_texture = nullptr;
    QPixmap m_pixmap;
    std::string m_selectedSpriteId;

    float m_zoom = 1.0f;
    QPointF m_panOffset{0, 0};

    bool m_snapToPixel = false;
    bool m_editable = true;
    bool m_needsFit = false;   // fit-to-view once the widget first gains a real size

    // interaction state
    Drag m_drag = Drag::None;
    int m_activeHandle = -1;
    QPointF m_lastMouse{0, 0};       // for pan
    glm::vec2 m_grabImg{0, 0};       // image point where a Move/DrawNew started
    QRectF m_dragOrigRect;           // sprite image rect at drag start
    QRectF m_pendingRect;            // live image rect while dragging (Move/Resize/DrawNew)
    glm::vec2 m_pendingPivot{0, 0};  // live pivot fraction while MovePivot
};
