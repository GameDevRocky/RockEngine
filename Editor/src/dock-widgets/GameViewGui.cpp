#include "dock-widgets/GameViewGui.hpp"
#include "dock-widgets/SceneToolbars.hpp"
#include <QStringList>
#include <QDebug>
#include "engine/debug/Console.hpp"
#include "engine/core/InputManager.hpp"
#include "engine/core/TimeManager.hpp"
#include "engine/input/CursorManager.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include <glm/glm.hpp>
#include "engine/rendering/Renderer.hpp"
#include "Engine.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include <QCursor>
#include <QFocusEvent>
#include <QHideEvent>
#include <QImage>
#include <QPixmap>
#include <algorithm>
#include <cmath>

namespace {
    // The Game view's aspect/resolution presets, mirroring Unity's Game-view
    // dropdown. Aspect entries letterbox to a ratio; Resolution entries render
    // at an exact internal size, scaled to fit the panel.
    struct AspectPreset {
        const char* label;
        enum Kind { Free, Aspect, Resolution } kind;
        float aspect;   // Kind::Aspect
        int   w, h;     // Kind::Resolution
    };

    const AspectPreset kPresets[] = {
        { "Free Aspect",         AspectPreset::Free,       0.0f,        0,    0    },
        { "5:4",                 AspectPreset::Aspect,     5.0f / 4.0f,  0,    0    },
        { "4:3",                 AspectPreset::Aspect,     4.0f / 3.0f,  0,    0    },
        { "3:2",                 AspectPreset::Aspect,     3.0f / 2.0f,  0,    0    },
        { "16:10",               AspectPreset::Aspect,     16.0f / 10.0f, 0,   0    },
        { "16:9",                AspectPreset::Aspect,     16.0f / 9.0f, 0,    0    },
        { "21:9",                AspectPreset::Aspect,     21.0f / 9.0f, 0,    0    },
        { "32:9",                AspectPreset::Aspect,     32.0f / 9.0f, 0,    0    },
        // ---- fixed resolutions (a separator precedes this block) ----
        { "Full HD (1920x1080)", AspectPreset::Resolution, 0.0f,        1920, 1080 },
        { "WXGA (1366x768)",     AspectPreset::Resolution, 0.0f,        1366, 768  },
        { "QHD (2560x1440)",     AspectPreset::Resolution, 0.0f,        2560, 1440 },
        { "4K UHD (3840x2160)",  AspectPreset::Resolution, 0.0f,        3840, 2160 },
    };
    constexpr int kPresetCount = IM_ARRAYSIZE(kPresets);
    constexpr int kFirstResolutionIndex = 8;   // where the resolution block starts
}

GameViewGui::GameViewGui(QWidget* parent)
    : ViewportWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    m_cursorSubscription = CursorManager::Get().Subscribe([this]() {
        ApplyCursorState();
        return true;
    }, CursorManager::CHANGED_EVENT);
}

GameViewGui::~GameViewGui()
{
    CursorManager::Get().Unsubscribe(m_cursorSubscription);
    ReleaseCursorCapture();
    if (!imGuiInstance) return;
    makeCurrent();
    imGuiInstance->Shutdown();
    doneCurrent();
    delete imGuiInstance;
    imGuiInstance = nullptr;
}

void GameViewGui::Init(){
    resize(300, 500);
    std::cout << "GameViewGui Initialized" << std::endl;
}

RenderView* GameViewGui::CreateView(int pixelW, int pixelH)
{
    gameView = Renderer::Get().CreateGameView(pixelW, pixelH);
    return gameView;
}

void GameViewGui::OnViewInitialized()
{
    // Must precede imGuiInstance construction with no intervening event-loop
    // turn: eventFilter() calls ImGui::GetIO(), which asserts if a filtered
    // event arrives before ImGui::CreateContext() has run.
    installEventFilter(this);

    imGuiInstance = new ImGuiInstance();
    imGuiInstance->Init();
    imGuiInstance->AddDrawCall([this](){ DrawNoCameraOverlay(); });

    BuildToolBar();

    // Default to Free Aspect: fill the panel instead of letterboxing to the
    // camera's authored aspect. Matches Unity's default and fixes the image not
    // filling the Game view widget out of the box.
    ApplyAspectPreset(0);
    ApplyCursorState();

    std::cout << "GameViewGui OpenGL Initialized. " << std::endl;
}

void GameViewGui::OnResized(int w, int h)
{
    if (imGuiInstance) imGuiInstance->Resize(w, h, devicePixelRatioF());
    // Pinned to the corner, but the panel can still shrink under it.
    if (m_toolbar) m_toolbar->ClampToParent();
}

void GameViewGui::OnAfterPresent()
{
    if (!imGuiInstance) return;

    // ImGui::NewFrame() (inside Render()) asserts io.DeltaTime > 0, so feed it
    // the frame's unscaled delta before drawing.
    imGuiInstance->MakeCurrent();
    ImGuiIO& io = ImGui::GetIO();
    TimeManager* timeManager = Engine::Get()->GetActiveContainer()->FindSystem<TimeManager>();
    float dt = timeManager ? timeManager->UnscaledDeltaTime() : 0.0f;
    io.DeltaTime = (dt > 0.0f) ? dt : (1.0f / 60.0f);

    imGuiInstance->Render();
}

void GameViewGui::ReleaseCursorCapture()
{
    if (QWidget::mouseGrabber() == this)
        releaseMouse();
    m_appliedLockState = static_cast<int>(CursorLockState::None);
    m_haveLastMousePos = false;
}

void GameViewGui::ApplyCursorState()
{
    CursorManager& state = CursorManager::Get();
    const bool runtime = Engine::Get()->GetActiveContainer() &&
                         Engine::Get()->GetActiveContainer()->GetMode() == Container::Mode::Runtime;
    const bool canCapture = runtime && hasFocus() && window() && window()->isActiveWindow();
    const CursorLockState requested = runtime ? state.GetLockState() : CursorLockState::None;

    if (!canCapture || requested == CursorLockState::None) {
        ReleaseCursorCapture();
    }

    // Appearance belongs only to the Game view widget. A bad/missing sprite degrades to the
    // platform cursor; it must never leave the editor with an invisible pointer.
    if ((canCapture && requested == CursorLockState::Locked) || !state.IsVisible()) {
        setCursor(Qt::BlankCursor);
    } else if (!state.GetSpriteID().empty()) {
        Sprite* sprite = AssetManager::Get().GetSprite(state.GetSpriteID());
        Texture2D* texture = sprite ? sprite->GetTexture() : nullptr;
        QImage source(texture ? QString::fromStdString(texture->GetPath()) : QString());
        if (sprite && !source.isNull()) {
            const glm::vec2 uvMin = sprite->GetUVMin();
            const glm::vec2 uvMax = sprite->GetUVMax();
            const int x0 = std::clamp(static_cast<int>(std::lround(uvMin.x * source.width())),
                                      0, source.width());
            const int x1 = std::clamp(static_cast<int>(std::lround(uvMax.x * source.width())),
                                      0, source.width());
            // Sprite UVs are bottom-left; QImage rows are top-left.
            const int y0 = std::clamp(source.height() -
                                      static_cast<int>(std::lround(uvMax.y * source.height())),
                                      0, source.height());
            const int y1 = std::clamp(source.height() -
                                      static_cast<int>(std::lround(uvMin.y * source.height())),
                                      0, source.height());
            if (x1 > x0 && y1 > y0) {
                QPixmap pixmap = QPixmap::fromImage(source.copy(x0, y0, x1 - x0, y1 - y0));
                const glm::vec2 hotspot = state.GetHotspot();
                const int hotX = std::clamp(static_cast<int>(std::lround(hotspot.x)), 0,
                                            pixmap.width() - 1);
                const int hotY = std::clamp(static_cast<int>(std::lround(hotspot.y)), 0,
                                            pixmap.height() - 1);
                setCursor(QCursor(pixmap, hotX, hotY));
            } else {
                unsetCursor();
            }
        } else {
            unsetCursor();
        }
    } else {
        unsetCursor();
    }

    if (canCapture && requested != CursorLockState::None) {
        if (QWidget::mouseGrabber() != this)
            grabMouse();
        m_appliedLockState = static_cast<int>(requested);

        if (requested == CursorLockState::Locked) {
            const QPoint center = rect().center();
            if (inputManager && gameView) {
                const float dpr = devicePixelRatioF();
                inputManager->SetMouseScreenPosition(
                    gameView, {center.x() * dpr, center.y() * dpr});
            }
            QCursor::setPos(mapToGlobal(center));
        }
    }
}

void GameViewGui::ApplyAspectPreset(int index)
{
    if (index < 0 || index >= kPresetCount) return;
    aspectPresetIndex = index;
    if (!gameView) return;

    const AspectPreset& p = kPresets[index];
    switch (p.kind)
    {
        case AspectPreset::Free:       gameView->SetDisplayFree();               break;
        case AspectPreset::Aspect:     gameView->SetDisplayAspect(p.aspect);     break;
        case AspectPreset::Resolution: gameView->SetDisplayResolution(p.w, p.h); break;
    }
}

void GameViewGui::BuildToolBar()
{
    if (m_toolbar) return;

    m_toolbar = new GameViewToolbar(this);

    QStringList labels;
    labels.reserve(kPresetCount);
    for (int i = 0; i < kPresetCount; ++i)
        labels << QString::fromUtf8(kPresets[i].label);
    m_toolbar->SetItems(labels, kFirstResolutionIndex);

    m_toolbar->onIndexChanged = [this](int index) { ApplyAspectPreset(index); };
    m_toolbar->SetCurrentIndex(aspectPresetIndex);

    // Same corner the ImGui bar occupied.
    m_toolbar->move(8, 8);
    m_toolbar->show();
    m_toolbar->raise();
}

void GameViewGui::DrawNoCameraOverlay()
{
    // The Game view keeps blitting its last resolved frame when no Camera is
    // active (see GameRenderView::UpdateCamera), so overlay a notice to make
    // the "nothing is driving this view" state explicit instead of silent.
    if (!gameView || gameView->HasActiveCamera()) return;

    const char* text = "No cameras rendering";

    // The main viewport spans this widget's ImGui display, so Pos/Size is the
    // full panel rect in ImGui's coordinate space.
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 min = viewport->Pos;
    ImVec2 max(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
    ImVec2 center(viewport->Pos.x + viewport->Size.x * 0.5f,
                  viewport->Pos.y + viewport->Size.y * 0.5f);

    // Draw the text at 2x the base font size.
    ImFont* font = ImGui::GetFont();
    float fontSize = ImGui::GetFontSize() * 2.0f;
    ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
    ImVec2 pos(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    // Full-screen semi-transparent backdrop keeps the text legible over any
    // clear color and makes the "nothing is driving this view" state obvious.
    dl->AddRectFilled(min, max, IM_COL32(0, 0, 0, 140));
    dl->AddText(font, fontSize, pos, IM_COL32(230, 230, 230, 255), text);
}

void GameViewGui::keyPressEvent(QKeyEvent* event) {
    // Unity-style editor escape hatch: a game script cannot trap the developer's pointer.
    if (event->key() == Qt::Key_Escape &&
        CursorManager::Get().GetLockState() != CursorLockState::None) {
        CursorManager::Get().SetLockState(CursorLockState::None);
        CursorManager::Get().SetVisible(true);
    }
    inputManager->SetKeyState(event->key(), true);
}

void GameViewGui::keyReleaseEvent(QKeyEvent* event) {
    inputManager->SetKeyState(event->key(), false);
}


void GameViewGui::wheelEvent(QWheelEvent* event)
{
    event->accept();
}


void GameViewGui::mousePressEvent(QMouseEvent* event)
{
    setFocus(Qt::MouseFocusReason);
    inputManager->SetMouseButtonState(static_cast<int>(event->button()), true);
    event->accept();
}


void GameViewGui::mouseReleaseEvent(QMouseEvent* event)
{
    inputManager->SetMouseButtonState(static_cast<int>(event->button()), false);
    event->accept();
}


void GameViewGui::mouseMoveEvent(QMouseEvent* e)
{
    if (!inputManager || !gameView) {
        e->accept();
        return;
    }

    const float dpr = devicePixelRatioF();
    const auto applied = static_cast<CursorLockState>(m_appliedLockState);

    if (applied == CursorLockState::Locked) {
        const QPoint centerGlobal = mapToGlobal(rect().center());
        const QPoint global = e->globalPosition().toPoint();
        const QPoint logicalDelta = global - centerGlobal;
        if (!logicalDelta.isNull()) {
            inputManager->AddMouseDelta({logicalDelta.x() * dpr, logicalDelta.y() * dpr});
            QCursor::setPos(centerGlobal);
        }
        const QPoint center = rect().center();
        inputManager->SetMouseScreenPosition(gameView, {center.x() * dpr, center.y() * dpr});
        e->accept();
        return;
    }

    QPointF logicalPos = e->position();
    if (m_haveLastMousePos) {
        const QPointF delta = logicalPos - m_lastMousePos;
        inputManager->AddMouseDelta({delta.x() * dpr, delta.y() * dpr});
    }

    // Qt's grab delivers moves beyond the widget. Clamp and warp those back for true
    // confinement rather than merely continuing to receive out-of-bounds events.
    if (applied == CursorLockState::Confined) {
        const QPointF clamped(
            std::clamp(logicalPos.x(), 0.0, std::max(0, width() - 1) * 1.0),
            std::clamp(logicalPos.y(), 0.0, std::max(0, height() - 1) * 1.0));
        if (clamped != logicalPos) {
            logicalPos = clamped;
            QCursor::setPos(mapToGlobal(clamped.toPoint()));
        }
    }

    m_lastMousePos = logicalPos;
    m_haveLastMousePos = true;
    inputManager->SetMouseScreenPosition(gameView, {logicalPos.x() * dpr, logicalPos.y() * dpr});
    e->accept();
}

void GameViewGui::focusInEvent(QFocusEvent* event)
{
    ViewportWidget::focusInEvent(event);
    ApplyCursorState();
}

void GameViewGui::focusOutEvent(QFocusEvent* event)
{
    ReleaseCursorCapture();
    ViewportWidget::focusOutEvent(event);
}

void GameViewGui::hideEvent(QHideEvent* event)
{
    ReleaseCursorCapture();
    ViewportWidget::hideEvent(event);
}

bool GameViewGui::eventFilter(QObject* obj, QEvent* event)
{
    if (!imGuiInstance) return QOpenGLWidget::eventFilter(obj, event);

    // Two ImGui contexts are live (Scene + Game); make ours current before
    // touching its IO so events route to the right one.
    imGuiInstance->MakeCurrent();
    ImGuiIO& io = ImGui::GetIO();

    switch (event->type()) {
        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(event);
            io.MousePos = ImVec2(me->pos().x(), me->pos().y());
            break;   // let the game keep tracking the cursor too
        }
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            io.MousePos = ImVec2(me->pos().x(), me->pos().y());
            if (me->button() == Qt::LeftButton)  io.MouseDown[0] = true;
            if (me->button() == Qt::RightButton) io.MouseDown[1] = true;
            // Swallow the click (don't forward to the game) when the toolbar
            // wants it.
            return io.WantCaptureMouse;
        }
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            io.MousePos = ImVec2(me->pos().x(), me->pos().y());
            if (me->button() == Qt::LeftButton)  io.MouseDown[0] = false;
            if (me->button() == Qt::RightButton) io.MouseDown[1] = false;
            return io.WantCaptureMouse;
        }
        case QEvent::Wheel: {
            auto* we = static_cast<QWheelEvent*>(event);
            io.MouseWheel += we->angleDelta().y() / 120.0f;
            return io.WantCaptureMouse;
        }
        default:
            break;
    }
    return QOpenGLWidget::eventFilter(obj, event);
}
