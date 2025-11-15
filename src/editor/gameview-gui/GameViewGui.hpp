#pragma once
#include <QWidget>

class GameViewGui : public QWidget {
    Q_OBJECT

public:
    static GameViewGui* Get() {
        static GameViewGui* instance = new GameViewGui(nullptr);
        return instance;
    }
    void Init();
    explicit GameViewGui(QWidget* parent = nullptr);
private:
    ~GameViewGui() override = default;


};
