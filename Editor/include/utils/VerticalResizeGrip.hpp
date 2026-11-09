#pragma once

#include <algorithm>
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>

// Bottom-right handle for changing a child widget's height within a layout.
class VerticalResizeGrip : public QWidget {
public:
    static constexpr int kSize = 14;

    VerticalResizeGrip(QWidget* target, int minHeight, int maxHeight)
        : QWidget(target), m_target(target), m_minH(minHeight), m_maxH(maxHeight)
    {
        setFixedSize(kSize, kSize);
        setCursor(Qt::SizeVerCursor);
        setToolTip("Drag to resize");
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setPen(QPen(QColor(120, 120, 120)));
        for (int i = 0; i < 3; ++i) {
            const int o = 3 + i * 4;
            p.drawLine(width() - o, height() - 2, width() - 2, height() - o);
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() != Qt::LeftButton || !m_target) { QWidget::mousePressEvent(e); return; }
        m_dragging = true;
        m_pressGlobalY = e->globalPosition().toPoint().y();
        m_pressHeight = m_target->height();
        e->accept();
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (!m_dragging || !m_target) { QWidget::mouseMoveEvent(e); return; }
        const int dy = e->globalPosition().toPoint().y() - m_pressGlobalY;
        m_target->setFixedHeight(std::clamp(m_pressHeight + dy, m_minH, m_maxH));
        e->accept();
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        m_dragging = false;
        e->accept();
    }

private:
    QWidget* m_target = nullptr;
    int m_minH = 0;
    int m_maxH = 0;
    bool m_dragging = false;
    int m_pressGlobalY = 0;
    int m_pressHeight = 0;
};
