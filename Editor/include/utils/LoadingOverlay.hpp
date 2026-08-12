#pragma once
#include <QLabel>
#include <QProgressBar>
#include <QWidget>
#include <chrono>
#include <vector>

#include "engine/jobs/Job.hpp"

// The modal loading card: a dimmed scrim over the whole main window with a
// centered progress card, shown while a modal job runs.
//
// A plain child QWidget of MainWindow rather than a top-level window, which is
// what every other "modal" in this editor is (SpriteEditorModal, AssetPicker).
// The difference matters here: a scrim has to cover AND follow the window it is
// dimming, and a top-level would mean re-implementing follow-on-move,
// follow-on-resize, multi-monitor DPI, and z-order against every dock. A child
// widget that is neither the central widget nor in a layout floats freely, and
// raise() puts it above the central widget and every docked panel.
//
// Explicitly NOT a QDialog and never exec()'d. exec() spins a nested QEventLoop,
// inside which frameSwapped still fires -> Editor::FrameTick -> Engine::Update
// -> JobSystem::Pump, re-entering the pump from inside a job step. Same reason
// QApplication::processEvents() is off the table.
class LoadingOverlay : public QWidget {
    Q_OBJECT
public:
    static LoadingOverlay* Get();

    // Attach to the main window. Safe to call once MainWindow exists.
    void Attach(QWidget* host);

    // Called every frame from Editor::Update with the current job list. Progress
    // is a pull, not a notify -- see JobSystem's header for why.
    void Sync(const std::vector<JobStatus>& jobs);

protected:
    void paintEvent(QPaintEvent*) override;
    bool eventFilter(QObject* obj, QEvent* e) override;

    // Swallow every input that reaches the scrim. Without accepting them
    // explicitly, unhandled events propagate to the parent and the "modal" leaks.
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    explicit LoadingOverlay(QWidget* parent = nullptr);

    void BuildUi();
    void ShowFor(const JobStatus& job);
    void HideOverlay();
    void CenterCard();

    QWidget*      m_card  = nullptr;
    QLabel*       m_title = nullptr;
    QLabel*       m_detail = nullptr;
    QProgressBar* m_bar   = nullptr;

    QWidget* m_host = nullptr;

    // Don't flash a card at the user for work that finishes in a few frames.
    // Tracked as a timestamp rather than a QTimer so it costs nothing per frame
    // and can't fire after the job is already gone.
    static constexpr int kShowDelayMs = 150;
    // Release the scrim if a job runs this long without finishing, so a wedged
    // operation can never leave the editor permanently uninteractable.
    static constexpr int kStuckTimeoutSec = 30;
    std::chrono::steady_clock::time_point m_pendingSince{};
    JobId m_pendingId = 0;
    bool  m_visible = false;
};
