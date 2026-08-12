#include "utils/LoadingOverlay.hpp"

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "engine/debug/Console.hpp"

LoadingOverlay* LoadingOverlay::Get() {
    static LoadingOverlay* instance = new LoadingOverlay();
    return instance;
}

LoadingOverlay::LoadingOverlay(QWidget* parent) : QWidget(parent) {
    setObjectName("LoadingOverlay");
    // The app-wide stylesheet has an unqualified `QWidget { background:
    // palette(window); }` rule, so without an id-scoped override this would
    // paint solid grey. WA_TranslucentBackground is not the answer -- it is a
    // top-level attribute and does nothing for a child widget.
    setStyleSheet("#LoadingOverlay { background: rgba(0, 0, 0, 150); }");
    setFocusPolicy(Qt::StrongFocus);
    BuildUi();
    hide();
}

void LoadingOverlay::BuildUi() {
    m_card = new QWidget(this);
    m_card->setObjectName("LoadingCard");
    m_card->setStyleSheet(
        "#LoadingCard { background: #232326; border: 1px solid #4a4a50;"
        " border-radius: 6px; }");

    auto* layout = new QVBoxLayout(m_card);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(10);

    m_title = new QLabel(m_card);
    m_title->setStyleSheet("color: rgb(230,230,230); font-size: 14px; font-weight: bold;"
                           " background: transparent; border: none;");
    layout->addWidget(m_title);

    m_bar = new QProgressBar(m_card);
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(6);
    layout->addWidget(m_bar);

    m_detail = new QLabel(m_card);
    m_detail->setStyleSheet("color: rgb(150,150,150); font-size: 11px;"
                            " background: transparent; border: none;");
    layout->addWidget(m_detail);

    m_card->setFixedWidth(380);
    m_card->adjustSize();
}

void LoadingOverlay::Attach(QWidget* host) {
    if (!host || m_host == host) return;
    m_host = host;
    setParent(host);
    // MainWindow has no resizeEvent override, and adding one would mean editing
    // a file with nothing to do with this feature. An event filter tracks its
    // geometry without touching MainWindowGui at all.
    host->installEventFilter(this);
    setGeometry(host->rect());
    CenterCard();
    hide();
}

bool LoadingOverlay::eventFilter(QObject* obj, QEvent* e) {
    if (obj == m_host &&
        (e->type() == QEvent::Resize || e->type() == QEvent::Show ||
         e->type() == QEvent::LayoutRequest)) {
        setGeometry(m_host->rect());
        CenterCard();
    }
    return QWidget::eventFilter(obj, e);
}

void LoadingOverlay::CenterCard() {
    if (!m_card) return;
    m_card->adjustSize();
    m_card->move(rect().center() - m_card->rect().center());
}

void LoadingOverlay::paintEvent(QPaintEvent*) {
    // A plain QWidget subclass ignores a stylesheet background unless it runs
    // this explicitly -- same incantation CollapsableWidget uses.
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

// ─────────────────────────────────────────────────────────────────────────────
void LoadingOverlay::Sync(const std::vector<JobStatus>& jobs) {
    if (!m_host) return;

    const JobStatus* modal = nullptr;
    for (const JobStatus& j : jobs)
        if (j.modal) { modal = &j; break; }

    if (!modal) {
        m_pendingId = 0;
        if (m_visible) HideOverlay();
        return;
    }

    // Start (or restart) the grace clock when a new modal job appears, so a
    // scene that loads in 20ms never flashes a card.
    if (m_pendingId != modal->id) {
        m_pendingId = modal->id;
        m_pendingSince = std::chrono::steady_clock::now();
    }

    const auto now = std::chrono::steady_clock::now();

    if (!m_visible) {
        const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_pendingSince).count();
        // Per-job, not a global constant: an operation that is known to block
        // (a play-mode transition) asks for 0 so the card is up before the stall
        // starts, while fast work keeps the delay that stops it flashing.
        if (waited < modal->graceMs) return;
    }

    // Fail visible, not bricked. The overlay holds a keyboard grab and eats every
    // mouse event, so a job that never reaches a terminal state -- a main step
    // that always returns true, a worker wedged on a network path -- would leave
    // the editor permanently unusable with no way out. Let go after a while and
    // say so; the job itself keeps running.
    const auto shown = std::chrono::duration_cast<std::chrono::seconds>(
        now - m_pendingSince).count();
    if (m_visible && shown > kStuckTimeoutSec) {
        Console::Warn("Loading overlay released: '" + std::string(m_title->text().toStdString())
                      + "' has run for over " + std::to_string(kStuckTimeoutSec)
                      + "s. The operation is still running.");
        m_pendingId = 0;      // don't re-arm for this job
        HideOverlay();
        return;
    }

    ShowFor(*modal);
}

void LoadingOverlay::ShowFor(const JobStatus& job) {
    m_title->setText(QString::fromStdString(job.title));
    m_detail->setText(QString::fromStdString(job.label));

    if (job.fraction < 0.0f) {
        // Qt renders a busy sweep for a 0..0 range -- the honest display for
        // work whose size isn't known (or isn't divisible, like a play-mode
        // container copy).
        m_bar->setRange(0, 0);
    } else {
        m_bar->setRange(0, 1000);
        m_bar->setValue(static_cast<int>(job.fraction * 1000.0f));
    }

    if (!m_visible) {
        m_visible = true;
        setGeometry(m_host->rect());
        show();
        raise();
        setFocus(Qt::OtherFocusReason);
        grabKeyboard();
    }
    CenterCard();
}

void LoadingOverlay::HideOverlay() {
    m_visible = false;
    releaseKeyboard();
    hide();
}

// ─────────────────────────────────────────────────────────────────────────────
void LoadingOverlay::mousePressEvent(QMouseEvent* e)       { e->accept(); }
void LoadingOverlay::mouseReleaseEvent(QMouseEvent* e)     { e->accept(); }
void LoadingOverlay::mouseDoubleClickEvent(QMouseEvent* e) { e->accept(); }
void LoadingOverlay::mouseMoveEvent(QMouseEvent* e)        { e->accept(); }
void LoadingOverlay::wheelEvent(QWheelEvent* e)            { e->accept(); }

void LoadingOverlay::keyPressEvent(QKeyEvent* e) {
    // Everything, deliberately including Escape. There is no cancellation in
    // this version, so Escape must not look like it does something.
    e->accept();
}
