#pragma once

#include <QListView>
#include <QPersistentModelIndex>

// Handles texture disclosure independently from selection and sprite dragging.
class FolderAssetView final : public QListView {
public:
    explicit FolderAssetView(QWidget* parent = nullptr) : QListView(parent) {}

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    bool IsToggle(const QModelIndex& index, const QPoint& position) const;
    void Toggle(const QModelIndex& index);
    QPersistentModelIndex m_pressedToggle;
};
