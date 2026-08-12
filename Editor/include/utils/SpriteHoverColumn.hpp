#pragma once
#include <QWidget>
#include <QPixmap>
#include <QString>
#include <QPoint>
#include <string>
#include <vector>

#include "utils/AssetPreviewDelegate.hpp"   // cell metrics, see kCellW below

class QPropertyAnimation;

// A floating, scrollable column of sprite previews shown over a hovered texture
// cell in the Folder view. Each entry is drawn at the same cell dimensions as the
// AssetPreviewDelegate. The whole column is alpha-masked with a vertical gradient:
// fully transparent at the top and bottom, semi-transparent through the middle.
//
// It is a frameless, translucent, always-on-top tool window so it can extend past
// the Folder view's bounds without being clipped, while still receiving wheel
// (scroll) and enter/leave events for hover coordination.
class SpriteHoverColumn : public QWidget {
    Q_OBJECT
public:
    // Cell metrics come from AssetPreviewDelegate, not copies of its numbers:
    // this column has to line its cells up with the Folder view grid it floats
    // over, so a mismatch is visible as misalignment rather than as a build error.
    static constexpr int kCellW   = AssetPreviewDelegate::kCellWidth;
    static constexpr int kCellH   = AssetPreviewDelegate::kCellHeight;
    static constexpr int kThumb   = AssetPreviewDelegate::kThumbSize;
    static constexpr int kMaxRows = 4;     // visible rows before it scrolls

    explicit SpriteHoverColumn(QWidget* parent = nullptr);

    // Replace the column contents. `thumbs`, `names` and `ids` are parallel; `ids`
    // are the sprite asset ids used as the drag payload.
    void SetSprites(std::vector<QPixmap> thumbs, std::vector<QString> names,
                    std::vector<std::string> ids);

    // Size to its contents and show (fading in) centred over `globalCellRect`.
    void ShowOverCell(const QRect& globalCellRect);

    // Fade out, then hide once the animation finishes.
    void FadeOut();

    bool HasContent() const { return !m_thumbs.empty(); }

signals:
    void hovered();       // cursor entered the column
    void unhovered();     // cursor left the column
    void columnHidden();  // the column became hidden (fade-out done, or dismissed)

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    // Half-column padding above the first cell (and below the last) so either end
    // can be scrolled to the vertical centre, picker-style.
    int topPad() const { return (height() - kCellH) / 2; }
    int maxScroll() const;
    void FadeTo(double target, bool hideAtEnd);
    // Sprite index under a local point, or -1. Accounts for scroll + padding.
    int indexAt(const QPoint& localPos) const;
    // Begin a sprite-id drag for the given index.
    void startDrag(int index);

    std::vector<QPixmap>     m_thumbs;
    std::vector<QString>     m_names;
    std::vector<std::string> m_ids;
    int m_scroll = 0;

    QPoint m_pressPos;
    int    m_pressIndex = -1;

    QPropertyAnimation*    m_fade = nullptr;
    QMetaObject::Connection m_fadeConn;
};
