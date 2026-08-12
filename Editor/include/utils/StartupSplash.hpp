#pragma once
#include <QSplashScreen>
#include <chrono>
#include <string>

// The boot screen, shown while the startup asset load runs.
//
// A QSplashScreen rather than the job system's LoadingOverlay, because the two
// operate under completely different rules. LoadingOverlay rides the job pump,
// which rides the frame loop. The startup load happens inside the first
// viewport's initializeGL -- before app->exec(), before any frame is drawn,
// before there is an event loop at all. Nothing driven by event delivery can
// paint during it.
//
// QSplashScreen is built for exactly that window: it is a frameless always-on-
// top pixmap window whose repaint() is documented to work before the event loop
// is running, and it closes itself against the main window with finish().
// Subclassed only to draw a progress bar, which the stock class has no notion of.
class StartupSplash : public QSplashScreen {
    Q_OBJECT
public:
    static StartupSplash* Get();

    // Show the splash and route BootProgress into it. Call immediately before
    // MainWindow::PostInit(), which is what triggers the asset load.
    void Begin();

    // Clear the sink and close against the main window. finish(w) waits for w to
    // be exposed before hiding, so there is no flash of empty desktop between
    // the splash going away and the editor appearing. Safe if Begin() never ran.
    void End(QWidget* mainWindow);

protected:
    // QSplashScreen's own hook: called with the pixmap already blitted, so this
    // only has to add the parts the base class doesn't know about.
    void drawContents(QPainter* painter) override;

private:
    StartupSplash();
    void OnProgress(float fraction, const std::string& label);

    // Minimum time the splash stays up, so a fast boot doesn't reduce it to a
    // flicker. Costs up to this much startup time on a machine that loads
    // quicker than this; set to 0 to disable.
    static constexpr int kMinVisibleMs = 700;

    float       m_fraction = -1.0f;   // negative == indeterminate
    std::string m_label;
    bool        m_active = false;
    std::chrono::steady_clock::time_point m_shownAt{};
};
