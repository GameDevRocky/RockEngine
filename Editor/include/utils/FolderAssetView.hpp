#pragma once

#include <QHash>
#include <QListView>

class FolderAssetModel;
class QPushButton;
class QResizeEvent;

// Places real disclosure buttons over texture cells without changing cell layout.
class FolderAssetView final : public QListView {
public:
    explicit FolderAssetView(QWidget* parent = nullptr) : QListView(parent) {}
    void SetAssetModel(FolderAssetModel* assets);
    void RefreshToggleButtons();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    void Toggle(const QModelIndex& index);
    void QueueToggleSync();
    void SyncToggleButtons();
    void ClearToggleButtons();
    static QRect ToggleRect(const QRect& cell);

    QHash<QString, QPushButton*> m_toggleButtons;
    bool m_toggleSyncQueued = false;
};
