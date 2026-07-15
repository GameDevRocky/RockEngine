#include "dock-widgets/SpriteEditorModal.hpp"
#include "dock-widgets/SpriteCanvasWidget.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QSizeGrip>
#include <QApplication>
#include <QScreen>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QEvent>

#include <cmath>
#include <string>
#include <vector>

#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/Sprite.hpp"

SpriteEditorModal::SpriteEditorModal(Texture2D* tex, QWidget* parent)
    : QWidget(parent), m_texture(tex) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("SpriteEditorModal");
    setStyleSheet("#SpriteEditorModal { background: #232326; border: 1px solid #4a4a50; }");

    BuildUi();

    // Auto-close if the texture is destroyed in memory (one-shot; auto-unsubscribes).
    if (m_texture) {
        m_texSubId = m_texture->Subscribe([this]() {
            m_texAlive = false;
            if (m_canvas) m_canvas->SetTexture(nullptr);
            m_texture = nullptr;
            Close();
            return false;
        }, Texture2D::DESTROYED_EVENT);
    }

    m_canvas->SetTexture(m_texture);
    UpdateStatus();

    resize(960, 680);
    CenterOnScreen();
}

SpriteEditorModal::~SpriteEditorModal() {
    // Only touch the texture if it is still alive (if it fired DESTROYED_EVENT the
    // one-shot callback already auto-unsubscribed and cleared m_texture).
    if (m_texAlive && m_texture && m_texSubId != -1)
        m_texture->Unsubscribe(m_texSubId);
}

void SpriteEditorModal::BuildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(1, 1, 1, 1);
    root->setSpacing(4);

    // --- draggable header / title bar ---
    m_header = new QWidget();
    m_header->setStyleSheet("background: #2c2c30;");
    m_header->setFixedHeight(28);
    m_header->installEventFilter(this);
    auto* headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(8, 0, 4, 0);
    m_titleLabel = new QLabel("Sprite Editor");
    m_titleLabel->setStyleSheet("color: #dddddd; font-weight: bold;");
    auto* closeBtn = new QPushButton("X");
    closeBtn->setFixedSize(22, 22);
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(closeBtn);
    root->addWidget(m_header);

    // --- toolbar ---
    auto* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(6, 0, 6, 0);
    auto* fitBtn = new QPushButton("Fit");
    m_snap = new QCheckBox("Snap to pixel");
    m_deleteBtn = new QPushButton("Delete");
    m_deleteBtn->setEnabled(false);
    m_sliceBtn = new QPushButton("Slice...");
    m_sliceBtn->setCheckable(true);
    toolbar->addWidget(fitBtn);
    toolbar->addWidget(m_snap);
    toolbar->addStretch();
    toolbar->addWidget(m_deleteBtn);
    toolbar->addWidget(m_sliceBtn);
    root->addLayout(toolbar);

    // --- slice panel ---
    m_slicePanel = new QWidget();
    auto* slice = new QHBoxLayout(m_slicePanel);
    slice->setContentsMargins(6, 0, 6, 0);
    m_sliceMode = new QComboBox();
    m_sliceMode->addItem("By Cell Count");
    m_sliceMode->addItem("By Cell Size");
    m_sliceLabelA = new QLabel("Columns");
    m_sliceLabelB = new QLabel("Rows");
    m_sliceA = new QSpinBox(); m_sliceA->setRange(1, 8192); m_sliceA->setValue(1);
    m_sliceB = new QSpinBox(); m_sliceB->setRange(1, 8192); m_sliceB->setValue(1);
    m_slicePrefix = new QLineEdit(); m_slicePrefix->setPlaceholderText("name prefix");
    m_slicePrefix->setMaximumWidth(120);
    m_sliceReplace = new QCheckBox("Replace existing");
    auto* applyBtn = new QPushButton("Apply");
    slice->addWidget(m_sliceMode);
    slice->addWidget(m_sliceLabelA); slice->addWidget(m_sliceA);
    slice->addWidget(m_sliceLabelB); slice->addWidget(m_sliceB);
    slice->addWidget(m_slicePrefix);
    slice->addWidget(m_sliceReplace);
    slice->addStretch();
    slice->addWidget(applyBtn);
    m_slicePanel->setVisible(false);
    root->addWidget(m_slicePanel);

    // --- canvas ---
    m_canvas = new SpriteCanvasWidget();
    root->addWidget(m_canvas, 1);

    // --- status bar (with a resize grip) ---
    auto* statusBar = new QHBoxLayout();
    statusBar->setContentsMargins(6, 0, 0, 0);
    m_status = new QLabel();
    m_status->setStyleSheet("color: #9a9a9a; padding: 2px;");
    statusBar->addWidget(m_status);
    statusBar->addStretch();
    statusBar->addWidget(new QSizeGrip(this), 0, Qt::AlignBottom | Qt::AlignRight);
    root->addLayout(statusBar);

    // --- wiring ---
    connect(closeBtn, &QPushButton::clicked, this, [this]() { Close(); });
    connect(fitBtn, &QPushButton::clicked, this, [this]() { m_canvas->FitView(); m_canvas->update(); });
    connect(m_snap, &QCheckBox::toggled, this, [this](bool on) { m_canvas->SetSnapToPixel(on); });
    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() { DeleteSelected(); });
    connect(m_sliceBtn, &QPushButton::toggled, this, [this](bool on) { m_slicePanel->setVisible(on); });
    connect(m_sliceMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { UpdateSliceLabels(); });
    connect(applyBtn, &QPushButton::clicked, this, [this]() { ApplySlice(); });

    connect(m_canvas, &SpriteCanvasWidget::spriteSelected, this, [this](const QString& id) {
        m_deleteBtn->setEnabled(!id.isEmpty());
    });
    connect(m_canvas, &SpriteCanvasWidget::spritesChanged, this, [this]() { UpdateStatus(); });

    UpdateSliceLabels();
}

void SpriteEditorModal::Close() {
    close();   // WA_DeleteOnClose schedules deletion
}

bool SpriteEditorModal::eventFilter(QObject* obj, QEvent* e) {
    if (obj == m_header) {
        if (e->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(e);
            if (me->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragOffset = me->globalPosition().toPoint() - frameGeometry().topLeft();
            }
        } else if (e->type() == QEvent::MouseMove && m_dragging) {
            auto* me = static_cast<QMouseEvent*>(e);
            move(me->globalPosition().toPoint() - m_dragOffset);
        } else if (e->type() == QEvent::MouseButtonRelease) {
            m_dragging = false;
        }
    }
    return QWidget::eventFilter(obj, e);
}

void SpriteEditorModal::changeEvent(QEvent* e) {
    if (e->type() == QEvent::ActivationChange) {
        if (isActiveWindow()) {
            m_activatedOnce = true;
        } else if (m_activatedOnce && !QApplication::activePopupWidget()) {
            // Deactivated by a click outside the window (and not because a child popup
            // like a combo-box dropdown opened) -> behave like a click-away modal.
            Close();
        }
    }
    QWidget::changeEvent(e);
}

void SpriteEditorModal::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) { Close(); return; }
    QWidget::keyPressEvent(e);
}

void SpriteEditorModal::CenterOnScreen() {
    QScreen* scr = screen() ? screen() : QGuiApplication::primaryScreen();
    if (!scr) return;
    const QRect g = scr->availableGeometry();
    move(g.center() - QPoint(width() / 2, height() / 2));
}

void SpriteEditorModal::DeleteSelected() {
    const std::string id = m_canvas->GetSelectedSprite();
    if (id.empty()) return;
    AssetManager::Get().RemoveSprite(id);
    m_canvas->SetSelectedSprite("");
    m_deleteBtn->setEnabled(false);
    m_canvas->update();
    UpdateStatus();
}

void SpriteEditorModal::ApplySlice() {
    if (!m_texture) return;
    const int W = m_texture->GetWidth();
    const int H = m_texture->GetHeight();
    if (W <= 0 || H <= 0) return;

    const bool byCount = (m_sliceMode->currentIndex() == 0);
    int cols, rows;
    double cellW, cellH;
    if (byCount) {
        cols = m_sliceA->value();
        rows = m_sliceB->value();
        if (cols <= 0 || rows <= 0) return;
        cellW = static_cast<double>(W) / cols;
        cellH = static_cast<double>(H) / rows;
    } else {
        cellW = m_sliceA->value();
        cellH = m_sliceB->value();
        if (cellW <= 0 || cellH <= 0) return;
        cols = static_cast<int>(std::floor(W / cellW));
        rows = static_cast<int>(std::floor(H / cellH));
        if (cols <= 0 || rows <= 0) return;
    }

    std::string prefix = m_slicePrefix->text().toStdString();
    if (prefix.empty()) prefix = m_texture->GetName();

    if (m_sliceReplace->isChecked()) {
        std::vector<std::string> ids;
        for (const auto& [id, sp] : AssetManager::Get().GetAllSprites())
            if (sp && sp->GetTextureID() == m_texture->GetID()) ids.push_back(id);
        for (const auto& id : ids) AssetManager::Get().RemoveSprite(id);
        m_canvas->SetSelectedSprite("");
        m_deleteBtn->setEnabled(false);
    }

    const std::string texId = m_texture->GetID();
    int index = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const double x  = c * cellW;
            const double y  = r * cellH;
            const double x2 = x + cellW;
            const double y2 = y + cellH;
            glm::vec2 uvMin{ static_cast<float>(x  / W), static_cast<float>(1.0 - y2 / H) };
            glm::vec2 uvMax{ static_cast<float>(x2 / W), static_cast<float>(1.0 - y  / H) };
            AssetManager::Get().CreateSprite(texId, uvMin, uvMax,
                                             prefix + "_" + std::to_string(index));
            ++index;
        }
    }
    m_canvas->update();
    UpdateStatus();
}

void SpriteEditorModal::UpdateStatus() {
    if (!m_status) return;
    if (!m_texture) { m_status->setText("Texture destroyed"); return; }

    int count = 0;
    for (const auto& [id, s] : AssetManager::Get().GetAllSprites())
        if (s && s->GetTextureID() == m_texture->GetID()) ++count;

    m_titleLabel->setText(QString("Sprite Editor  -  %1")
                              .arg(QString::fromStdString(m_texture->GetName())));
    m_status->setText(QString("%1x%2   -   %3 sprites   -   drag to move/resize, draw on empty space to add")
                          .arg(m_texture->GetWidth()).arg(m_texture->GetHeight()).arg(count));
}

void SpriteEditorModal::UpdateSliceLabels() {
    const bool byCount = (m_sliceMode->currentIndex() == 0);
    m_sliceLabelA->setText(byCount ? "Columns" : "Cell W");
    m_sliceLabelB->setText(byCount ? "Rows" : "Cell H");
}
