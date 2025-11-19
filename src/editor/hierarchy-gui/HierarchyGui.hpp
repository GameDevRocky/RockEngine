#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QIcon>
#include <QPushButton>
#include <QLabel>
#include <QPixmap>

class HierarchyGui : public QWidget {
    Q_OBJECT

public:
    static HierarchyGui* Get() {
        static HierarchyGui* instance = new HierarchyGui(nullptr);
        return instance;
    }
    void Init();
    explicit HierarchyGui(QWidget* parent = nullptr);


private:
    ~HierarchyGui() override = default;
    void CreateHeader();

    QTextEdit* filter = nullptr;
    QVBoxLayout* layout = nullptr;



    QWidget* header = nullptr;

};
