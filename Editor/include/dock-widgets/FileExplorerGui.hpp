#pragma once

#include <QFileSystemModel>
#include <QTreeView>
#include <QWidget>
#include <QMouseEvent>

class FileExplorerGui : public QWidget
{
    Q_OBJECT

public:
    static FileExplorerGui* Get() {
        static FileExplorerGui* instance = nullptr;
        if (!instance) {
            instance = new FileExplorerGui();
            instance->SetProjectDirectory("C:/Users/rockl/Coding Projects/RockEngine");
        }
        return instance;
    }
    explicit FileExplorerGui(QWidget *parent = nullptr);
    void SetProjectDirectory(const QString& dir);
    void Init();

Q_SIGNALS:
    void FileSelected(const QString& path);
    void FileDoubleClicked(const QString& path);
    void RaiseFolderView();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    QFileSystemModel* model;
    QTreeView* tree;
};
