#pragma once
#include <QWidget>

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


};
