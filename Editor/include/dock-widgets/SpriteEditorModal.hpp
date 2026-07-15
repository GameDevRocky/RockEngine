#pragma once
#include <QWidget>
#include <QPoint>
#include <string>

class Texture2D;
class SpriteCanvasWidget;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QLineEdit;
class QLabel;
class QPushButton;

// Centered, frameless, movable + resizable editor window for one texture's sprites.
// Launched from the "Edit Sprites" button in the texture inspector. Behaves like a
// light modal: it closes when the user clicks outside it (window deactivation), on
// Escape, or via the close button. Deletes itself on close (WA_DeleteOnClose) and
// auto-closes if the texture is destroyed in memory (Texture2D::DESTROYED_EVENT).
class SpriteEditorModal : public QWidget {
    Q_OBJECT
public:
    explicit SpriteEditorModal(Texture2D* tex, QWidget* parent = nullptr);
    ~SpriteEditorModal() override;

    void Close();   // public close entry point

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;  // drag the header to move
    void changeEvent(QEvent* e) override;                // close on click-outside (deactivate)
    void keyPressEvent(QKeyEvent* e) override;           // Escape closes

private:
    void BuildUi();
    void DeleteSelected();
    void ApplySlice();
    void UpdateStatus();
    void UpdateSliceLabels();
    void CenterOnScreen();

    Texture2D* m_texture = nullptr;
    int  m_texSubId = -1;
    bool m_texAlive = true;      // false once the texture fired DESTROYED_EVENT
    bool m_activatedOnce = false;

    SpriteCanvasWidget* m_canvas = nullptr;
    QWidget* m_header = nullptr;
    QLabel* m_titleLabel = nullptr;
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

    bool m_dragging = false;
    QPoint m_dragOffset;
};
