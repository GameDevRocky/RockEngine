#include "utils/StartupSplash.hpp"

#include <QApplication>
#include <QFont>
#include <QPainter>
#include <QPixmap>
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <thread>

#include "engine/jobs/BootProgress.hpp"

namespace {

constexpr int kW = 460;
constexpr int kH = 260;

// Built in code rather than loaded from Domain/: the splash has to exist before
// the asset system it is reporting on, so it cannot depend on that system's
// files being found. Also keeps it working in a bundled build with a relocated
// asset root.
QPixmap MakeSplashPixmap() {
    QPixmap px(kW, kH);
    px.fill(QColor(27, 27, 30));

    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(QPen(QColor(74, 74, 80), 1));
    p.drawRect(0, 0, kW - 1, kH - 1);

    QFont title = p.font();
    title.setPointSize(26);
    title.setBold(true);
    p.setFont(title);
    p.setPen(QColor(235, 235, 235));
    p.drawText(QRect(32, 60, kW - 64, 44), Qt::AlignLeft | Qt::AlignVCenter, "RockEngine");

    QFont sub = p.font();
    sub.setPointSize(9);
    sub.setBold(false);
    p.setFont(sub);
    p.setPen(QColor(120, 120, 125));
    p.drawText(QRect(32, 104, kW - 64, 20), Qt::AlignLeft | Qt::AlignVCenter, "Editor");

    return px;
}

} // namespace

StartupSplash* StartupSplash::Get() {
    static StartupSplash* instance = new StartupSplash();
    return instance;
}

StartupSplash::StartupSplash()
    : QSplashScreen(MakeSplashPixmap(), Qt::WindowStaysOnTopHint)
{
}

void StartupSplash::drawContents(QPainter* painter) {
    QSplashScreen::drawContents(painter);

    const int barX = 32;
    const int barW = kW - 64;
    const int barY = kH - 62;
    const int barH = 6;

    painter->setRenderHint(QPainter::Antialiasing, true);

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(45, 45, 45));
    painter->drawRoundedRect(barX, barY, barW, barH, 3, 3);

    if (m_fraction >= 0.0f) {
        const int filled = static_cast<int>(barW * (m_fraction < 1.0f ? m_fraction : 1.0f));
        if (filled > 0) {
            painter->setBrush(QColor(87, 126, 100));
            painter->drawRoundedRect(barX, barY, filled, barH, 3, 3);
        }
    } else {
        // Indeterminate: a static partial fill. There is no event loop to
        // animate against here, so a "moving" sweep would just sit still and
        // look broken -- a quiet stub reads better than a frozen animation.
        painter->setBrush(QColor(70, 84, 76));
        painter->drawRoundedRect(barX, barY, barW / 4, barH, 3, 3);
    }

    QFont f = painter->font();
    f.setPointSize(8);
    painter->setFont(f);
    painter->setPen(QColor(150, 150, 150));
    painter->drawText(QRect(barX, barY + barH + 8, barW, 18),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      QString::fromStdString(m_label));
}

void StartupSplash::Begin() {
    if (m_active) return;
    m_active = true;
    m_label = "Starting up";
    m_shownAt = std::chrono::steady_clock::now();

    // Place it on the screen the user is actually looking at.
    //
    // QSplashScreen centers on QGuiApplication::primaryScreen() by default, and
    // on a multi-monitor setup that is whichever display Windows calls primary
    // -- not necessarily the one in front of the user. On this machine it landed
    // on the secondary panel, so the splash worked perfectly and was invisible.
    // The cursor is the best available proxy for attention at a point in startup
    // where no window of ours exists yet to inherit a screen from.
    if (QScreen* target = QGuiApplication::screenAt(QCursor::pos())
                            ? QGuiApplication::screenAt(QCursor::pos())
                            : QGuiApplication::primaryScreen()) {
        const QRect avail = target->availableGeometry();
        move(avail.center() - QPoint(width() / 2, height() / 2));
    }

    show();
    raise();

    // One processEvents, here and ONLY here. show() posts the events that get
    // the window mapped and first painted by the platform, and with no event
    // loop running yet nothing would ever deliver them -- the splash would exist
    // and never appear. This is the documented Qt splash pattern.
    //
    // Safe at this point precisely because it is before MainWindow::PostInit:
    // no GL widget has been realized, so there is no initializeGL for a
    // re-entrant paint to land in. The per-asset progress callbacks below are a
    // different story and deliberately use repaint() only.
    QApplication::processEvents();
    repaint();

    BootProgress::SetSink([this](float fraction, const std::string& label) {
        OnProgress(fraction, label);
    });
}

void StartupSplash::OnProgress(float fraction, const std::string& label) {
    if (!m_active) return;
    m_fraction = fraction;
    m_label    = label;

    // Deliberately repaint() and NOT QApplication::processEvents(): this is
    // called from inside the first viewport's initializeGL, and pumping
    // arbitrary events there would let a paint, a resize, or a stray input
    // re-enter Qt's GL initialization while it is halfway through.
    repaint();
}

void StartupSplash::End(QWidget* mainWindow) {
    if (!m_active) return;
    m_active = false;
    BootProgress::SetSink({});

    // Hold briefly if the load was faster than the eye. Parallelising the
    // texture decode made the boot quick enough that the splash could otherwise
    // appear and vanish inside a couple of frames, which reads as a flicker
    // rather than as a splash screen.
    //
    // This is a real cost -- it delays startup by up to kMinVisibleMs on a fast
    // machine, in exchange for the screen being seen at all. Set kMinVisibleMs
    // to 0 if you would rather have the milliseconds back.
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_shownAt).count();
    if (elapsed < kMinVisibleMs) {
        m_label = "Ready";
        m_fraction = 1.0f;
        repaint();
        // A sleep, not a nested event loop: nothing may run between here and the
        // editor appearing, and a spinning loop would let a stray paint or input
        // reach half-initialized widgets.
        std::this_thread::sleep_for(std::chrono::milliseconds(kMinVisibleMs - elapsed));
    }

    if (mainWindow) finish(mainWindow);   // waits for the editor to be exposed
    else            close();
}
