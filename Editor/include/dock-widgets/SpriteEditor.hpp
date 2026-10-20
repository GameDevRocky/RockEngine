#pragma once
#include <QWidget>
#include <string>

class Texture2D;
class SpriteCanvasWidget;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QLineEdit;
class QLabel;
class QPushButton;

// Dockable editor panel for defining sprites on a texture. MainWindow owns its
// QDockWidget and retargets this reusable panel whenever "Edit Sprites" is pressed.
class SpriteEditor : public QWidget {
    Q_OBJECT
public:
    explicit SpriteEditor(QWidget* parent = nullptr);
    ~SpriteEditor() override;

    void SetTexture(Texture2D* texture);

private:
    void BuildUi();
    void DeleteSelected();
    void ApplySlice();
    void UpdateStatus();
    void UpdateSliceLabels();

    Texture2D* m_texture = nullptr;
    int  m_texSubId = -1;

    SpriteCanvasWidget* m_canvas = nullptr;
    QLabel* m_status = nullptr;
    QCheckBox* m_snap = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QPushButton* m_sliceBtn = nullptr;

    QWidget* m_slicePanel = nullptr;
    QComboBox* m_sliceMode = nullptr;
    QLabel* m_sliceLabelA = nullptr;
    QLabel* m_sliceLabelB = nullptr;
    QSpinBox* m_sliceA = nullptr;
    QSpinBox* m_sliceB = nullptr;
    QLineEdit* m_slicePrefix = nullptr;
    QCheckBox* m_sliceReplace = nullptr;
};
