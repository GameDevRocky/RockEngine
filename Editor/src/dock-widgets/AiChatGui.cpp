#include "dock-widgets/AiChatGui.hpp"
#include "engine/components/Component.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/Resource.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "utils/AssetThumbnails.hpp"
#include "utils/DragDropMime.hpp"
#include "utils/EditorUtils.hpp"

#include <QAction>
#include <QActionGroup>
#include <QAbstractTextDocumentLayout>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEasingCurve>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMimeDatabase>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QPixmap>
#include <QPen>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOption>
#include <QStylePainter>
#include <QTimer>
#include <QTextBlock>
#include <QTextDocument>
#include <QtMath>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidgetAction>
#include <QUrl>

#include <yaml-cpp/yaml.h>

namespace {

// QSS has no variable syntax. This multiplier provides one control for the
// strength of every QSS-defined gradient color without duplicating alpha edits.
constexpr qreal kGradientColorStrength = 0.72;
constexpr int kGradientPointCount = 5;
constexpr int kGradientSizePercent[kGradientPointCount] = {42, 34, 29, 25, 22};
constexpr int kGradientDurationMs[kGradientPointCount] = {
    27000, 33000, 38000, 42000, 36000
};
constexpr const char* kGradientRoles[kGradientPointCount] = {
    "primary", "secondary", "accent", "primary", "secondary"
};

class ElidingComboBox final : public QComboBox {
public:
    using QComboBox::QComboBox;

protected:
    void paintEvent(QPaintEvent*) override {
        QStylePainter painter(this);
        QStyleOptionComboBox option;
        initStyleOption(&option);
        const QRect textRect = style()->subControlRect(
            QStyle::CC_ComboBox, &option,
            QStyle::SC_ComboBoxEditField, this);
        option.currentText = fontMetrics().elidedText(
            option.currentText, Qt::ElideRight, qMax(0, textRect.width()));
        painter.drawComplexControl(QStyle::CC_ComboBox, option);
        painter.drawControl(QStyle::CE_ComboBoxLabel, option);
    }
};

class CenteredPlusButton final : public QPushButton {
public:
    using QPushButton::QPushButton;

protected:
    void paintEvent(QPaintEvent*) override {
        QStylePainter painter(this);
        QStyleOptionButton option;
        initStyleOption(&option);
        option.text.clear();
        option.icon = {};
        painter.drawControl(QStyle::CE_PushButton, option);

        painter.setRenderHint(QPainter::Antialiasing);
        QPen pen(option.palette.color(QPalette::ButtonText), 1.25);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        const QPointF center((width() - 1) / 2.0, (height() - 1) / 2.0);
        constexpr qreal halfLength = 5.0;
        painter.drawLine(QPointF(center.x() - halfLength, center.y()),
                         QPointF(center.x() + halfLength, center.y()));
        painter.drawLine(QPointF(center.x(), center.y() - halfLength),
                         QPointF(center.x(), center.y() + halfLength));
    }
};

constexpr qint64 kMaxEmbeddedFileBytes = 128 * 1024;

QString NormalizedPath(const QString& path) {
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

bool SamePath(const QString& left, const QString& right) {
    if (left.isEmpty() || right.isEmpty()) return false;
#ifdef Q_OS_WIN
    return NormalizedPath(left).compare(NormalizedPath(right), Qt::CaseInsensitive) == 0;
#else
    return NormalizedPath(left) == NormalizedPath(right);
#endif
}

Resource* ResourceForId(const std::string& id) {
    if (id.empty()) return nullptr;
    AssetManager& assets = AssetManager::Get();
    if (auto* material = assets.GetMaterial(id)) return material;
    if (auto* texture = assets.GetTexture(id)) return texture;
    if (auto* sprite = assets.GetSprite(id)) return sprite;
    return nullptr;
}

std::string AssetIdForFile(const QString& path) {
    QString metadataPath = path;
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("png") || suffix == QStringLiteral("jpg") ||
        suffix == QStringLiteral("jpeg") || suffix == QStringLiteral("bmp")) {
        const QString textureMetadata = path + QStringLiteral(".texture");
        if (QFileInfo::exists(textureMetadata)) metadataPath = textureMetadata;
    }

    try {
        const YAML::Node node = YAML::LoadFile(metadataPath.toStdString());
        if (node["id"]) return node["id"].as<std::string>();
    } catch (...) {
    }
    return {};
}

Resource* ResourceForFile(const QString& path) {
    if (Resource* resource = ResourceForId(AssetIdForFile(path))) return resource;

    AssetManager& assets = AssetManager::Get();
    for (const auto& [id, material] : assets.GetAllMaterials()) {
        if (material && SamePath(path, QString::fromStdString(material->GetFilePath())))
            return material;
    }
    for (const auto& [id, texture] : assets.GetAllTextures()) {
        if (!texture) continue;
        if (SamePath(path, QString::fromStdString(texture->GetFilePath())) ||
            SamePath(path, QString::fromStdString(texture->GetPath())))
            return texture;
    }
    return nullptr;
}

QString ProjectRelativePath(const QString& path) {
    const QDir root(QDir::cleanPath(QString::fromUtf8(PROJECT_ROOT)));
    const QString relative = QDir::cleanPath(root.relativeFilePath(NormalizedPath(path)));
    return relative == QStringLiteral("..") || relative.startsWith(QStringLiteral("../"))
        ? QString() : relative;
}

bool IsTextFile(const QFileInfo& info) {
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(
        info, QMimeDatabase::MatchExtension);
    const QString name = mime.name();
    return name.startsWith(QStringLiteral("text/")) ||
           name == QStringLiteral("application/json") ||
           name == QStringLiteral("application/xml") ||
           name == QStringLiteral("application/javascript") ||
           name == QStringLiteral("application/x-yaml");
}

QString DumpYaml(const YAML::Node& node) {
    try {
        return QString::fromStdString(YAML::Dump(node));
    } catch (const std::exception& error) {
        return QStringLiteral("serialization_error: %1")
            .arg(QString::fromUtf8(error.what()));
    }
}

QString InstallUrl(ai::AiAgentService::Provider provider) {
    switch (provider) {
    case ai::AiAgentService::Provider::OpenAI:
        return QStringLiteral("https://developers.openai.com/codex/cli");
    case ai::AiAgentService::Provider::Claude:
        return QStringLiteral("https://code.claude.com/docs/en/setup");
    case ai::AiAgentService::Provider::Gemini:
        return QStringLiteral("https://github.com/google-gemini/gemini-cli");
    }
    return {};
}

QString CliName(ai::AiAgentService::Provider provider) {
    switch (provider) {
    case ai::AiAgentService::Provider::OpenAI: return QStringLiteral("Codex CLI");
    case ai::AiAgentService::Provider::Claude: return QStringLiteral("Claude Code CLI");
    case ai::AiAgentService::Provider::Gemini: return QStringLiteral("Gemini CLI");
    }
    return {};
}

QString ModelSettingsKey(ai::AiAgentService::Provider provider) {
    switch (provider) {
    case ai::AiAgentService::Provider::OpenAI:
        return QStringLiteral("ai/model/openai");
    case ai::AiAgentService::Provider::Claude:
        return QStringLiteral("ai/model/claude");
    case ai::AiAgentService::Provider::Gemini:
        return QStringLiteral("ai/model/gemini");
    }
    return {};
}

QVector<QPair<QString, QString>> ModelOptions(ai::AiAgentService::Provider provider) {
    switch (provider) {
    case ai::AiAgentService::Provider::OpenAI:
        return {{QStringLiteral("5.6 Sol"), QStringLiteral("gpt-5.6-sol")},
                {QStringLiteral("5.6 Terra"), QStringLiteral("gpt-5.6-terra")},
                {QStringLiteral("5.6 Luna"), QStringLiteral("gpt-5.6-luna")},
                {QStringLiteral("Provider default"), QString()}};
    case ai::AiAgentService::Provider::Claude:
        return {{QStringLiteral("Sonnet"), QStringLiteral("sonnet")},
                {QStringLiteral("Opus"), QStringLiteral("opus")},
                {QStringLiteral("Haiku"), QStringLiteral("haiku")},
                {QStringLiteral("Provider default"), QString()}};
    case ai::AiAgentService::Provider::Gemini:
        return {{QStringLiteral("Auto"), QStringLiteral("auto")},
                {QStringLiteral("3.1 Pro"), QStringLiteral("gemini-3.1-pro-preview")},
                {QStringLiteral("3 Flash"), QStringLiteral("gemini-3-flash-preview")},
                {QStringLiteral("2.5 Pro"), QStringLiteral("gemini-2.5-pro")},
                {QStringLiteral("2.5 Flash"), QStringLiteral("gemini-2.5-flash")}};
    }
    return {};
}

QPixmap ProviderLogo(ai::AiAgentService::Provider provider) {
    const QString filename = provider == ai::AiAgentService::Provider::OpenAI
        ? QStringLiteral("chatgpt_logo.svg")
        : (provider == ai::AiAgentService::Provider::Claude
            ? QStringLiteral("anthropic_logo.png")
            : QStringLiteral("gemini_logo.png"));
    const QString relativePath = QStringLiteral("Domain/lib/assets/icons/%1").arg(filename);
    const QString path = QString::fromStdString(
        EngineUtils::GetAssetPath(relativePath.toStdString()));
    const QPixmap source(path);
    if (source.isNull()) return {};
    const int size = provider == ai::AiAgentService::Provider::Claude ? 58 : 52;
    return source.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void LoadAssistantStyleSheet(QWidget* widget) {
    const QString path = QString::fromStdString(EngineUtils::GetAssetPath(
        "Domain/lib/assets/styling/ai_assistant.qss"));
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        widget->setStyleSheet(QString::fromUtf8(file.readAll()));
}

void RefreshDynamicStyle(QWidget* widget) {
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace

AiChatGui* AiChatGui::Get() {
    static AiChatGui* instance = new AiChatGui(nullptr);
    return instance;
}

AiChatGui::AiChatGui(QWidget* parent) : QWidget(parent) {
    BuildUi();
}

int AiChatGui::ProviderIndex(ai::AiAgentService::Provider provider) const {
    return static_cast<int>(provider);
}

ai::AiAgentService::Provider AiChatGui::CurrentProvider() const {
    return static_cast<ai::AiAgentService::Provider>(m_provider->currentIndex());
}

void AiChatGui::BuildUi() {
    setObjectName(QStringLiteral("AiAssistant"));
    setAcceptDrops(true);
    LoadAssistantStyleSheet(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_pages = new QStackedWidget(this);
    m_pages->setObjectName(QStringLiteral("AiPages"));
    root->addWidget(m_pages, 1);

    m_loginPage = new QWidget(m_pages);
    m_loginPage->setObjectName(QStringLiteral("AiLoginPage"));
    m_loginPage->setAttribute(Qt::WA_StyledBackground, true);
    auto* loginStack = new QGridLayout(m_loginPage);
    loginStack->setContentsMargins(0, 0, 0, 0);
    loginStack->setSpacing(0);

    m_gradientLayer = new QWidget(m_loginPage);
    m_gradientLayer->setObjectName(QStringLiteral("AiGradientLayer"));
    m_gradientLayer->setAttribute(Qt::WA_StyledBackground, true);
    m_gradientLayer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_gradientLayer->installEventFilter(this);
    for (int index = 0; index < kGradientPointCount; ++index) {
        auto* blob = new QWidget(m_gradientLayer);
        blob->setObjectName(QStringLiteral("AiGradientBlob"));
        blob->setProperty("gradientRole", kGradientRoles[index]);
        blob->setProperty("gradientIndex", index);
        blob->setAttribute(Qt::WA_StyledBackground, true);
        auto* opacity = new QGraphicsOpacityEffect(blob);
        opacity->setOpacity(kGradientColorStrength);
        blob->setGraphicsEffect(opacity);
        m_gradientBlobs.append(blob);
    }

    auto* loginSurface = new QWidget(m_loginPage);
    loginSurface->setObjectName(QStringLiteral("AiLoginSurface"));
    auto* loginLayout = new QVBoxLayout(loginSurface);
    loginLayout->setContentsMargins(16, 12, 16, 14);
    loginLayout->setSpacing(9);
    loginStack->addWidget(m_gradientLayer, 0, 0);
    loginStack->addWidget(loginSurface, 0, 0);
    loginSurface->raise();

    for (int index = 0; index < m_gradientBlobs.size(); ++index) {
        auto* animation = new QPropertyAnimation(
            m_gradientBlobs[index], "geometry", this);
        animation->setEasingCurve(QEasingCurve::InOutSine);
        connect(animation, &QPropertyAnimation::finished, this, [this, index]() {
            StartGradientAnimation(m_gradientAnimations[index],
                                   m_gradientBlobs[index],
                                   kGradientSizePercent[index],
                                   kGradientDurationMs[index]);
        });
        m_gradientAnimations.append(animation);
    }
    QTimer::singleShot(0, this, &AiChatGui::ConfigureGradientAnimations);

    auto* providerRow = new QHBoxLayout();
    auto* providerLabel = new QLabel(QStringLiteral("Provider"), loginSurface);
    providerLabel->setObjectName(QStringLiteral("AiMutedText"));
    m_provider = new QComboBox(loginSurface);
    m_provider->addItem(QStringLiteral("OpenAI"));
    m_provider->addItem(QStringLiteral("Claude"));
    m_provider->addItem(QStringLiteral("Gemini"));
    m_provider->setMaximumWidth(180);
    providerRow->addStretch();
    providerRow->addWidget(providerLabel);
    providerRow->addWidget(m_provider);
    providerRow->addStretch();
    loginLayout->addLayout(providerRow);
    loginLayout->addStretch(1);

    auto* loginContent = new QWidget(loginSurface);
    loginContent->setObjectName(QStringLiteral("AiLoginContent"));
    loginContent->setMaximumWidth(560);
    auto* contentLayout = new QVBoxLayout(loginContent);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(9);

    m_loginLogo = new QLabel(loginContent);
    m_loginLogo->setObjectName(QStringLiteral("AiLoginLogo"));
    m_loginLogo->setAlignment(Qt::AlignCenter);
    m_loginLogo->setFixedSize(86, 86);
    contentLayout->addWidget(m_loginLogo, 0, Qt::AlignHCenter);
    contentLayout->addSpacing(8);

    m_loginHeading = new QLabel(QStringLiteral("Enter API Key"), loginContent);
    m_loginHeading->setObjectName(QStringLiteral("AiLoginHeading"));
    m_loginHeading->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(m_loginHeading);

    m_loginExplanation = new QLabel(loginContent);
    m_loginExplanation->setObjectName(QStringLiteral("AiMutedText"));
    m_loginExplanation->setAlignment(Qt::AlignCenter);
    m_loginExplanation->setWordWrap(true);
    contentLayout->addWidget(m_loginExplanation);

    m_accountLogin = new QPushButton(QStringLiteral("Sign in with ChatGPT"), loginContent);
    m_accountLogin->setObjectName(QStringLiteral("AiSecondaryButton"));
    contentLayout->addWidget(m_accountLogin);

    m_apiSeparator = new QLabel(QStringLiteral("or connect with an API key"), loginContent);
    m_apiSeparator->setObjectName(QStringLiteral("AiMutedText"));
    m_apiSeparator->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(m_apiSeparator);

    m_credentialFrame = new QWidget(loginContent);
    m_credentialFrame->setObjectName(QStringLiteral("AiCredentialFrame"));
    m_credentialFrame->setAttribute(Qt::WA_Hover, true);
    auto* credentialLayout = new QHBoxLayout(m_credentialFrame);
    credentialLayout->setContentsMargins(3, 3, 4, 3);
    credentialLayout->setSpacing(4);
    m_apiKey = new QLineEdit(m_credentialFrame);
    m_apiKey->setObjectName(QStringLiteral("AiCredentialInput"));
    m_apiKey->setEchoMode(QLineEdit::Password);
    m_apiKey->setClearButtonEnabled(true);
    m_apiKey->setPlaceholderText(QStringLiteral("sk-…"));
    m_apiKey->installEventFilter(this);
    m_apiKeyLogin = new QPushButton(QStringLiteral("→"), m_credentialFrame);
    m_apiKeyLogin->setObjectName(QStringLiteral("AiCredentialSubmit"));
    m_apiKeyLogin->setFixedSize(30, 30);
    m_apiKeyLogin->setToolTip(QStringLiteral("Connect with this API key"));
    credentialLayout->addWidget(m_apiKey, 1);
    credentialLayout->addWidget(m_apiKeyLogin);
    contentLayout->addWidget(m_credentialFrame);

    m_loginStatus = new QLabel(loginContent);
    m_loginStatus->setObjectName(QStringLiteral("AiLoginError"));
    m_loginStatus->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_loginStatus->setWordWrap(true);
    m_loginStatus->hide();
    contentLayout->addWidget(m_loginStatus);

    m_installLink = new QLabel(loginContent);
    m_installLink->setObjectName(QStringLiteral("AiLink"));
    m_installLink->setAlignment(Qt::AlignCenter);
    m_installLink->setOpenExternalLinks(true);
    contentLayout->addWidget(m_installLink);
    m_apiKeyLink = new QLabel(loginContent);
    m_apiKeyLink->setObjectName(QStringLiteral("AiLink"));
    m_apiKeyLink->setAlignment(Qt::AlignCenter);
    m_apiKeyLink->setOpenExternalLinks(true);
    contentLayout->addWidget(m_apiKeyLink);

    m_loginCancel = new QPushButton(QStringLiteral("Cancel sign-in"), loginContent);
    m_loginCancel->setObjectName(QStringLiteral("AiHeaderAction"));
    m_loginCancel->hide();
    contentLayout->addWidget(m_loginCancel);

    auto* contentRow = new QHBoxLayout();
    contentRow->addStretch();
    contentRow->addWidget(loginContent, 1);
    contentRow->addStretch();
    loginLayout->addLayout(contentRow);
    loginLayout->addStretch(2);
    m_pages->addWidget(m_loginPage);

    auto* chatPage = new QWidget(m_pages);
    chatPage->setObjectName(QStringLiteral("AiChatPage"));
    auto* chatLayout = new QGridLayout(chatPage);
    chatLayout->setContentsMargins(0, 0, 0, 0);
    chatLayout->setSpacing(0);

    auto* headerOverlay = new QWidget(chatPage);
    headerOverlay->setObjectName(QStringLiteral("AiHeaderOverlay"));
    headerOverlay->setAttribute(Qt::WA_StyledBackground, true);
    headerOverlay->setFixedHeight(36);
    auto* chatHeader = new QHBoxLayout(headerOverlay);
    chatHeader->setContentsMargins(10, 4, 10, 4);
    chatHeader->setSpacing(3);
    chatHeader->addStretch(1);

    m_settings = new QPushButton(QStringLiteral("\u2699"), headerOverlay);
    m_settings->setObjectName(QStringLiteral("AiTopAction"));
    m_settings->setFixedSize(28, 28);
    m_settings->setToolTip(QStringLiteral("AI assistant settings"));
    m_settings->setAccessibleName(QStringLiteral("AI assistant settings"));

    m_settingsMenu = new QMenu(m_settings);
    m_settingsMenu->setObjectName(QStringLiteral("AiSettingsMenu"));
    m_settingsMenu->setAttribute(Qt::WA_TranslucentBackground);

    auto* accountWidget = new QWidget(m_settingsMenu);
    accountWidget->setObjectName(QStringLiteral("AiSettingsAccount"));
    auto* accountLayout = new QHBoxLayout(accountWidget);
    accountLayout->setContentsMargins(10, 7, 10, 8);
    accountLayout->setSpacing(10);
    m_settingsProviderLogo = new QLabel(accountWidget);
    m_settingsProviderLogo->setObjectName(QStringLiteral("AiSettingsProviderLogo"));
    m_settingsProviderLogo->setAlignment(Qt::AlignCenter);
    m_settingsProviderLogo->setFixedSize(30, 30);
    accountLayout->addWidget(m_settingsProviderLogo, 0, Qt::AlignTop);
    auto* accountText = new QVBoxLayout();
    accountText->setContentsMargins(0, 0, 0, 0);
    accountText->setSpacing(1);
    m_settingsProviderName = new QLabel(accountWidget);
    m_settingsProviderName->setObjectName(QStringLiteral("AiSettingsProviderName"));
    m_settingsConnection = new QLabel(accountWidget);
    m_settingsConnection->setObjectName(QStringLiteral("AiSettingsConnection"));
    accountText->addWidget(m_settingsProviderName);
    accountText->addWidget(m_settingsConnection);
    accountLayout->addLayout(accountText, 1);

    auto* accountAction = new QWidgetAction(m_settingsMenu);
    accountAction->setDefaultWidget(accountWidget);
    m_settingsMenu->addAction(accountAction);
    m_settingsMenu->addSeparator();

    m_providerMenu = new QMenu(QStringLiteral("\u2699  Provider settings"), m_settingsMenu);
    m_providerMenu->setObjectName(QStringLiteral("AiProviderMenu"));
    auto* providerGroup = new QActionGroup(m_providerMenu);
    providerGroup->setExclusive(true);
    for (int index = 0; index < 3; ++index) {
        const auto provider = static_cast<ai::AiAgentService::Provider>(index);
        auto* providerAction = m_providerMenu->addAction(
            ai::AiAgentService::ProviderName(provider));
        providerAction->setCheckable(true);
        providerAction->setData(index);
        providerGroup->addAction(providerAction);
        connect(providerAction, &QAction::triggered, this, [this, index]() {
            m_provider->setCurrentIndex(index);
        });
    }
    m_settingsMenu->addMenu(m_providerMenu);

    auto* shortcuts = m_settingsMenu->addAction(
        QStringLiteral("\u2328  Keyboard shortcuts\tEnter / Shift+Enter"));
    connect(shortcuts, &QAction::triggered, this, [this]() {
        m_prompt->setFocus();
    });
    m_settingsMenu->addSeparator();
    m_signOut = m_settingsMenu->addAction(QStringLiteral("\u21aa  Log out"));
    m_signOut->setToolTip(
        QStringLiteral("Erase this provider's credentials and sign out"));
    m_settings->setMenu(m_settingsMenu);
    chatHeader->addWidget(m_settings);

    m_newChat = new QPushButton(QStringLiteral("\u270e"), headerOverlay);
    m_newChat->setObjectName(QStringLiteral("AiTopAction"));
    m_newChat->setFixedSize(28, 28);
    m_newChat->setToolTip(QStringLiteral("Start a new conversation"));
    m_newChat->setAccessibleName(QStringLiteral("New chat"));
    chatHeader->addWidget(m_newChat);

    m_transcript = new QScrollArea(chatPage);
    m_transcript->setObjectName(QStringLiteral("AiTranscript"));
    m_transcript->setFrameShape(QFrame::NoFrame);
    m_transcript->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_transcript->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_transcript->setWidgetResizable(true);
    m_messageList = new QWidget(m_transcript);
    m_messageList->setObjectName(QStringLiteral("AiMessageList"));
    m_messageLayout = new QVBoxLayout(m_messageList);
    m_messageLayout->setContentsMargins(17, 42, 17, 168);
    m_messageLayout->setSpacing(10);
    m_transcript->setWidget(m_messageList);
    chatLayout->addWidget(m_transcript, 0, 0);

    auto* composerOverlay = new QWidget(chatPage);
    composerOverlay->setObjectName(QStringLiteral("AiComposerOverlay"));
    composerOverlay->setAttribute(Qt::WA_StyledBackground, true);
    composerOverlay->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    auto* bottomLayout = new QVBoxLayout(composerOverlay);
    bottomLayout->setContentsMargins(10, 0, 10, 8);
    bottomLayout->setSpacing(4);

    m_activity = new QLabel(composerOverlay);
    m_activity->setObjectName(QStringLiteral("AiActivity"));
    m_activity->setWordWrap(true);
    m_activity->hide();
    bottomLayout->addWidget(m_activity);

    m_composerFrame = new QWidget(composerOverlay);
    m_composerFrame->setObjectName(QStringLiteral("AiComposerFrame"));
    m_composerFrame->setAttribute(Qt::WA_Hover, true);
    m_composerFrame->setMinimumHeight(92);
    auto* composerLayout = new QVBoxLayout(m_composerFrame);
    composerLayout->setContentsMargins(11, 9, 8, 8);
    composerLayout->setSpacing(5);

    m_attachmentScroll = new QScrollArea(m_composerFrame);
    m_attachmentScroll->setObjectName(QStringLiteral("AiAttachmentStrip"));
    m_attachmentScroll->setFrameShape(QFrame::NoFrame);
    m_attachmentScroll->setWidgetResizable(false);
    m_attachmentScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_attachmentScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_attachmentScroll->setFixedHeight(76);
    m_attachmentScroll->viewport()->installEventFilter(this);
    m_attachmentList = new QWidget(m_attachmentScroll);
    m_attachmentList->setObjectName(QStringLiteral("AiAttachmentList"));
    m_attachmentLayout = new QHBoxLayout(m_attachmentList);
    m_attachmentLayout->setContentsMargins(0, 0, 0, 0);
    m_attachmentLayout->setSpacing(7);
    m_attachmentLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_attachmentScroll->setWidget(m_attachmentList);
    m_attachmentScroll->hide();
    composerLayout->addWidget(m_attachmentScroll);

    m_prompt = new QPlainTextEdit(m_composerFrame);
    m_prompt->setObjectName(QStringLiteral("AiPrompt"));
    m_prompt->setPlaceholderText(QStringLiteral("Ask for follow-up changes"));
    m_prompt->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_prompt->setMinimumHeight(34);
    m_prompt->setMaximumHeight(96);
    m_prompt->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // QPlainTextEdit otherwise consumes URL drops itself. The surrounding chat
    // surface owns attachment drops and keeps prompt text unchanged.
    m_prompt->setAcceptDrops(false);
    m_prompt->installEventFilter(this);
    composerLayout->addWidget(m_prompt);
    connect(m_prompt->document()->documentLayout(),
            &QAbstractTextDocumentLayout::documentSizeChanged,
            this, [this](const QSizeF&) {
        QTimer::singleShot(0, this, &AiChatGui::UpdatePromptHeight);
    });
    connect(m_prompt, &QPlainTextEdit::textChanged, this, [this]() {
        // Let QPlainTextDocumentLayout finish recalculating soft-wrapped lines
        // before reading each QTextBlock's visual line count.
        QTimer::singleShot(0, this, &AiChatGui::UpdatePromptHeight);
    });
    QTimer::singleShot(0, this, &AiChatGui::UpdatePromptHeight);

    auto* composerActions = new QHBoxLayout();
    composerActions->setContentsMargins(0, 0, 0, 0);
    composerActions->setSpacing(5);

    m_addContext = new CenteredPlusButton(m_composerFrame);
    m_addContext->setObjectName(QStringLiteral("AiComposerAction"));
    m_addContext->setFixedSize(32, 32);
    m_addContext->setToolTip(
        QStringLiteral("Attach files or drag in-game context here"));
    m_addContext->setAccessibleName(QStringLiteral("Add context"));
    auto* contextMenu = new QMenu(m_addContext);
    contextMenu->setObjectName(QStringLiteral("AiContextMenu"));
    auto* filesAction = contextMenu->addAction(QStringLiteral("Attach files…"));
    connect(filesAction, &QAction::triggered, this, &AiChatGui::OpenFilePicker);
    contextMenu->addSeparator();
    auto* dragHint = contextMenu->addAction(QStringLiteral("Drag from the editor"));
    dragHint->setEnabled(false);
    for (const QString& label : {
             QStringLiteral("Objects — Hierarchy"),
             QStringLiteral("Components — Inspector"),
             QStringLiteral("Materials, textures, sprites — Assets")}) {
        QAction* action = contextMenu->addAction(label);
        action->setEnabled(false);
    }
    m_addContext->setMenu(contextMenu);
    composerActions->addWidget(m_addContext);
    composerActions->addStretch(1);

    m_model = new ElidingComboBox(m_composerFrame);
    m_model->setObjectName(QStringLiteral("AiModelSelector"));
    m_model->setFixedWidth(82);
    m_model->view()->setTextElideMode(Qt::ElideRight);
    m_model->setToolTip(QStringLiteral("Model used for new AI requests"));
    composerActions->addWidget(m_model, 0, Qt::AlignVCenter);

    m_stop = new QPushButton(QStringLiteral("■"), m_composerFrame);
    m_stop->setObjectName(QStringLiteral("AiSendButton"));
    m_stop->setFixedSize(36, 36);
    m_stop->setToolTip(QStringLiteral("Stop response"));
    m_send = new QPushButton(m_composerFrame);
    m_send->setObjectName(QStringLiteral("AiComposerAction"));
    m_send->setIcon(EditorUtils::CustomIconProvider::aiSendIcon());
    m_send->setIconSize(QSize(14, 18));
    m_send->setFixedSize(32, 32);
    m_send->setToolTip(QStringLiteral("Send message (Enter)"));
    m_stop->hide();
    composerActions->addWidget(m_stop);
    composerActions->addWidget(m_send);
    composerLayout->addLayout(composerActions);
    bottomLayout->addWidget(m_composerFrame);

    chatLayout->addWidget(headerOverlay, 0, 0, Qt::AlignTop);
    chatLayout->addWidget(composerOverlay, 0, 0, Qt::AlignBottom);
    headerOverlay->raise();
    composerOverlay->raise();
    m_pages->addWidget(chatPage);

    RenderTranscript();

    connect(m_provider, &QComboBox::currentIndexChanged, this, [this](int index) {
        QSettings().setValue(QStringLiteral("ai/provider"), index);
        ClearLoginError();
        m_messages.clear();
        m_messageErrors.clear();
        m_streamingMessageIndex = -1;
        m_streamingBubble.clear();
        ClearAttachments();
        RenderTranscript();
        RefreshProviderUi();
        ai::AiAgentService::Get()->CheckAuthentication(CurrentProvider());
    });
    connect(m_accountLogin, &QPushButton::clicked, this, [this]() {
        ClearLoginError();
        m_loginAttempt = LoginAttempt::Account;
        ai::AiAgentService::Get()->SignInWithAccount(CurrentProvider());
    });
    connect(m_apiKeyLogin, &QPushButton::clicked, this, [this]() {
        ClearLoginError();
        m_loginAttempt = LoginAttempt::ApiKey;
        const QByteArray key = m_apiKey->text().toUtf8();
        m_apiKey->clear();
        ai::AiAgentService::Get()->SignInWithApiKey(CurrentProvider(), key);
    });
    connect(m_apiKey, &QLineEdit::returnPressed, m_apiKeyLogin, &QPushButton::click);
    connect(m_loginCancel, &QPushButton::clicked, ai::AiAgentService::Get(),
            &ai::AiAgentService::Cancel);
    connect(m_send, &QPushButton::clicked, this, &AiChatGui::SubmitPrompt);
    connect(m_stop, &QPushButton::clicked, ai::AiAgentService::Get(),
            &ai::AiAgentService::Cancel);
    connect(m_newChat, &QPushButton::clicked, this, [this]() {
        ai::AiAgentService::Get()->NewConversation(CurrentProvider());
        m_messages.clear();
        m_messageErrors.clear();
        m_streamingMessageIndex = -1;
        m_streamingBubble.clear();
        ClearAttachments();
        RenderTranscript();
        m_activity->clear();
        m_activity->hide();
    });
    connect(m_model, &QComboBox::activated, this, [this](int) {
        const QString key = ModelSettingsKey(CurrentProvider());
        const QString selectedModel = SelectedModel();
        QSettings settings;
        if (settings.value(key).toString() == selectedModel) return;
        settings.setValue(key, selectedModel);

        ai::AiAgentService::Get()->NewConversation(CurrentProvider());
        m_messages.clear();
        m_messageErrors.clear();
        m_streamingMessageIndex = -1;
        m_streamingBubble.clear();
        ClearAttachments();
        RenderTranscript();
        m_activity->clear();
        m_activity->hide();
    });
    connect(m_signOut, &QAction::triggered, this, [this]() {
        ClearLoginError();
        m_messages.clear();
        m_messageErrors.clear();
        m_streamingMessageIndex = -1;
        m_streamingBubble.clear();
        ClearAttachments();
        RenderTranscript();
        ai::AiAgentService::Get()->SignOut(CurrentProvider());
    });

    auto* service = ai::AiAgentService::Get();
    connect(service, &ai::AiAgentService::AuthenticationChanged, this,
            [this](ai::AiAgentService::Provider provider, bool available,
                   bool authenticated, const QString& detail) {
        AuthState& state = m_auth[ProviderIndex(provider)];
        state = {true, available, authenticated, detail};
        if (provider == CurrentProvider()) {
            if (authenticated) {
                ClearLoginError();
            } else if (!m_busy && m_loginAttempt != LoginAttempt::None &&
                       m_loginStatus->isHidden()) {
                ShowLoginError(detail);
            }
            RefreshProviderUi();
        }
    });
    connect(service, &ai::AiAgentService::BusyChanged, this, [this](bool busy) {
        m_busy = busy;
        if (!busy && m_streamingMessageIndex >= 0) {
            m_streamingMessageIndex = -1;
            m_streamingBubble.clear();
        }
        m_provider->setEnabled(!busy);
        m_accountLogin->setEnabled(!busy && m_auth[ProviderIndex(CurrentProvider())].cliAvailable);
        m_apiKeyLogin->setEnabled(!busy && m_auth[ProviderIndex(CurrentProvider())].cliAvailable);
        m_apiKeyLogin->setText(busy && m_pages->currentIndex() == 0
            ? QStringLiteral("…") : QStringLiteral("→"));
        m_apiKey->setEnabled(!busy);
        m_loginCancel->setVisible(busy && m_pages->currentIndex() == 0);
        m_prompt->setEnabled(!busy);
        m_addContext->setEnabled(!busy);
        m_attachmentScroll->setEnabled(!busy);
        m_model->setEnabled(!busy);
        m_send->setVisible(!busy);
        m_stop->setVisible(busy && m_pages->currentIndex() == 1);
        m_newChat->setEnabled(!busy);
        m_providerMenu->menuAction()->setEnabled(!busy);
        m_signOut->setEnabled(!busy);
    });
    connect(service, &ai::AiAgentService::ActivityChanged, this,
            [this](const QString& text) {
        m_activity->setText(text);
        m_activity->setVisible(!text.trimmed().isEmpty());
    });
    connect(service, &ai::AiAgentService::ResponseUpdated, this,
            [this](ai::AiAgentService::Provider provider, const QString& text) {
        if (provider == CurrentProvider()) UpdateStreamingMessage(text);
    });
    connect(service, &ai::AiAgentService::ResponseReady, this,
            [this](ai::AiAgentService::Provider provider, const QString& text) {
        if (provider == CurrentProvider()) FinishStreamingMessage(text);
    });
    connect(service, &ai::AiAgentService::RequestFailed, this,
            [this](ai::AiAgentService::Provider provider, const QString& error) {
        if (provider != CurrentProvider()) return;
        if (m_pages->currentIndex() == 0) ShowLoginError(error);
        else {
            m_streamingMessageIndex = -1;
            m_streamingBubble.clear();
            AppendMessage(QStringLiteral("Error"), error, true);
        }
    });
}

void AiChatGui::ConfigureGradientAnimations() {
    if (!m_gradientLayer || m_gradientLayer->width() <= 1 ||
        m_gradientLayer->height() <= 1) return;

    for (int index = 0; index < m_gradientAnimations.size(); ++index) {
        m_gradientAnimations[index]->stop();
        StartGradientAnimation(m_gradientAnimations[index],
                               m_gradientBlobs[index],
                               kGradientSizePercent[index],
                               kGradientDurationMs[index]);
    }
}

void AiChatGui::StartGradientAnimation(QPropertyAnimation* animation, QWidget* blob,
                                       int sizePercent, int baseDurationMs) {
    if (!animation || !blob || !m_gradientLayer ||
        m_gradientLayer->width() <= 1 || m_gradientLayer->height() <= 1) return;

    QRandomGenerator* random = QRandomGenerator::global();
    const int width = m_gradientLayer->width();
    const int height = m_gradientLayer->height();
    const int baseDiameter = qMax(100, qMax(width, height) * sizePercent / 100);

    const auto randomGeometry = [=](int minimumScale, int maximumScale) {
        const int diameter = baseDiameter *
                             random->bounded(minimumScale, maximumScale + 1) / 100;
        const int minX = -diameter / 2;
        const int maxX = width - diameter / 2;
        const int minY = -diameter / 2;
        const int maxY = height - diameter / 2;
        return QRect(random->bounded(minX, maxX + 1),
                     random->bounded(minY, maxY + 1), diameter, diameter);
    };

    const QRect current = blob->geometry();
    const bool hasAnimatedGeometry = current.width() >= baseDiameter * 3 / 5 &&
                                     qAbs(current.width() - current.height()) <= 2;
    animation->setDuration(baseDurationMs * random->bounded(88, 113) / 100);
    animation->setStartValue(hasAnimatedGeometry ? current : randomGeometry(52, 72));
    animation->setKeyValueAt(0.20, randomGeometry(105, 138));
    animation->setKeyValueAt(0.40, randomGeometry(52, 72));
    animation->setKeyValueAt(0.60, randomGeometry(105, 138));
    animation->setKeyValueAt(0.80, randomGeometry(52, 72));
    animation->setEndValue(randomGeometry(105, 138));
    animation->start();
}

void AiChatGui::Init() {
    if (m_initialized) return;
    const int savedProvider = qBound(0, QSettings().value(
        QStringLiteral("ai/provider"), 0).toInt(), 2);
    m_provider->setCurrentIndex(savedProvider);
    RefreshProviderUi();
    ai::AiAgentService::Get()->CheckAuthentication(CurrentProvider());
    m_initialized = true;
}

void AiChatGui::Shutdown() {
    ai::AiAgentService::Get()->Shutdown();
}

void AiChatGui::RefreshProviderUi() {
    const auto provider = CurrentProvider();
    const AuthState& state = m_auth[ProviderIndex(provider)];
    const QString providerName = ai::AiAgentService::ProviderName(provider);
    RefreshModelUi();

    m_loginHeading->setText(QStringLiteral("Enter API Key"));
    const QString theme = provider == ai::AiAgentService::Provider::OpenAI
        ? QStringLiteral("openai")
        : (provider == ai::AiAgentService::Provider::Claude
            ? QStringLiteral("claude") : QStringLiteral("gemini"));
    setProperty("providerTheme", theme);
    RefreshDynamicStyle(this);
    for (QWidget* child : findChildren<QWidget*>()) RefreshDynamicStyle(child);
    for (QWidget* blob : m_gradientBlobs) {
        blob->setProperty("providerTheme", theme);
        RefreshDynamicStyle(blob);
    }
    const QPixmap logo = ProviderLogo(provider);
    if (logo.isNull()) {
        m_loginLogo->setPixmap({});
        m_loginLogo->setText(QStringLiteral("AI"));
    } else {
        m_loginLogo->clear();
        m_loginLogo->setPixmap(logo);
    }
    if (provider == ai::AiAgentService::Provider::OpenAI) {
        m_loginExplanation->setText(QStringLiteral(
            "Connect OpenAI securely with a Platform API key, or use the official "
            "ChatGPT browser sign-in flow."));
        m_apiKey->setPlaceholderText(QStringLiteral("sk-…"));
        m_accountLogin->show();
        m_apiSeparator->show();
    } else if (provider == ai::AiAgentService::Provider::Claude) {
        m_loginExplanation->setText(QStringLiteral(
            "Connect Claude with a Console API key. RockEngine stores it only in your "
            "operating system's credential vault."));
        m_apiKey->setPlaceholderText(QStringLiteral("sk-ant-…"));
        m_accountLogin->hide();
        m_apiSeparator->hide();
    } else {
        m_loginExplanation->setText(QStringLiteral(
            "Connect Gemini with a Google AI Studio API key. RockEngine validates it, "
            "then stores it only in your operating system's credential vault."));
        m_apiKey->setPlaceholderText(QStringLiteral("AIza…"));
        m_accountLogin->hide();
        m_apiSeparator->hide();
    }

    const QString apiUrl = ai::AiAgentService::ApiKeyPortal(provider);
    m_apiKeyLink->setText(QStringLiteral("<a href=\"%1\">Create/manage %2 API keys</a>")
                              .arg(apiUrl, providerName));
    const bool showCliInstall = provider != ai::AiAgentService::Provider::Gemini &&
                                !state.cliAvailable;
    m_installLink->setText(!showCliInstall ? QString() :
        QStringLiteral("<a href=\"%1\">Install %2</a>")
            .arg(InstallUrl(provider), CliName(provider)));

    RefreshSettingsMenu();
    m_pages->setCurrentIndex(state.authenticated ? 1 : 0);
    m_accountLogin->setEnabled(!m_busy && state.cliAvailable);
    m_apiKeyLogin->setEnabled(!m_busy && state.cliAvailable);
    m_loginCancel->setVisible(m_busy && !state.authenticated);
    m_stop->setVisible(m_busy && state.authenticated);
}

void AiChatGui::RefreshModelUi() {
    const QSignalBlocker blocker(m_model);
    const QVector<QPair<QString, QString>> options = ModelOptions(CurrentProvider());
    const QString key = ModelSettingsKey(CurrentProvider());
    QSettings settings;
    QString selectedModel = settings.value(key).toString();
    if (!settings.contains(key) && !options.isEmpty()) {
        selectedModel = options.first().second;
        settings.setValue(key, selectedModel);
    }

    m_model->clear();
    for (const auto& option : options)
        m_model->addItem(option.first, option.second);

    int selectedIndex = m_model->findData(selectedModel);
    if (selectedIndex < 0 && !selectedModel.isEmpty()) {
        m_model->addItem(selectedModel, selectedModel);
        selectedIndex = m_model->count() - 1;
    }
    m_model->setCurrentIndex(qMax(0, selectedIndex));
    m_model->setToolTip(QStringLiteral("Model: %1").arg(m_model->currentText()));
}

QString AiChatGui::SelectedModel() const {
    return m_model ? m_model->currentData().toString() : QString();
}

void AiChatGui::OpenFilePicker() {
    auto* picker = new QFileDialog(
        this, QStringLiteral("Attach files"),
        QDir::cleanPath(QString::fromUtf8(PROJECT_ROOT)));
    picker->setFileMode(QFileDialog::ExistingFiles);
    picker->setNameFilter(QStringLiteral("All files (*)"));
    picker->setAttribute(Qt::WA_DeleteOnClose);
    connect(picker, &QFileDialog::filesSelected, this,
            [this](const QStringList& paths) {
        for (const QString& path : paths) AttachFile(path);
    });
    // open() is asynchronous and does not create the nested Qt event loop that
    // a dialog exec() would create inside the editor frame driver.
    picker->open();
}

bool AiChatGui::CanAttachMimeData(const QMimeData* mime) const {
    if (!mime || m_busy || !m_pages || m_pages->currentIndex() != 1) return false;
    if (mime->hasFormat(kGameObjectMimeType) ||
        mime->hasFormat(kComponentMimeType) ||
        mime->hasFormat(kSpriteMimeType) ||
        mime->hasFormat(kSpriteListMimeType)) return true;

    if (!mime->hasUrls()) return false;
    for (const QUrl& url : mime->urls()) {
        const QFileInfo info(url.toLocalFile());
        if (info.exists() && info.isFile()) return true;
    }
    return false;
}

bool AiChatGui::AttachMimeData(const QMimeData* mime) {
    if (!mime) return false;
    const qsizetype countBefore = m_attachments.size();

    if (mime->hasFormat(kGameObjectMimeType)) {
        AttachGameObject(QString::fromUtf8(
            mime->data(kGameObjectMimeType)).trimmed());
    }
    if (mime->hasFormat(kComponentMimeType)) {
        AttachComponent(QString::fromUtf8(
            mime->data(kComponentMimeType)).trimmed());
    }
    if (mime->hasFormat(kSpriteListMimeType)) {
        const QString ids = QString::fromUtf8(mime->data(kSpriteListMimeType));
        for (const QString& id : ids.split('\n', Qt::SkipEmptyParts))
            AttachSprite(id.trimmed());
    } else if (mime->hasFormat(kSpriteMimeType)) {
        AttachSprite(QString::fromUtf8(mime->data(kSpriteMimeType)).trimmed());
    }
    if (mime->hasUrls()) {
        for (const QUrl& url : mime->urls()) {
            const QFileInfo info(url.toLocalFile());
            if (info.exists() && info.isFile()) AttachFile(info.absoluteFilePath());
        }
    }
    return m_attachments.size() != countBefore;
}

void AiChatGui::AttachFile(const QString& path) {
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) return;

    if (Resource* resource = ResourceForFile(info.absoluteFilePath())) {
        Attachment attachment;
        attachment.id = QString::fromStdString(resource->GetID());
        attachment.path = NormalizedPath(info.absoluteFilePath());
        attachment.title = QString::fromStdString(resource->GetName());
        if (attachment.title.isEmpty()) attachment.title = info.fileName();

        if (dynamic_cast<Material*>(resource)) {
            attachment.kind = AttachmentKind::Material;
            attachment.subtitle = QStringLiteral("MATERIAL");
            attachment.key = QStringLiteral("material:%1").arg(attachment.id);
        } else if (dynamic_cast<Texture2D*>(resource)) {
            attachment.kind = AttachmentKind::Texture;
            attachment.subtitle = QStringLiteral("TEXTURE");
            attachment.key = QStringLiteral("texture:%1").arg(attachment.id);
        } else if (dynamic_cast<Sprite*>(resource)) {
            attachment.kind = AttachmentKind::Sprite;
            attachment.subtitle = QStringLiteral("SPRITE");
            attachment.key = QStringLiteral("sprite:%1").arg(attachment.id);
        }
        if (!attachment.key.isEmpty()) {
            AddAttachment(std::move(attachment));
            return;
        }
    }

    Attachment attachment;
    attachment.kind = AttachmentKind::File;
    attachment.path = NormalizedPath(info.absoluteFilePath());
    attachment.key = QStringLiteral("file:%1").arg(attachment.path);
    attachment.title = info.fileName();
    attachment.subtitle = info.suffix().isEmpty()
        ? QStringLiteral("FILE") : info.suffix().toUpper();
    AddAttachment(std::move(attachment));
}

void AiChatGui::AttachGameObject(const QString& id) {
    GameObject* object = Registry::FindInRuntime<GameObject>(id.toStdString());
    if (!object) return;

    Attachment attachment;
    attachment.kind = AttachmentKind::GameObject;
    attachment.id = id;
    attachment.key = QStringLiteral("gameobject:%1").arg(id);
    attachment.title = QString::fromStdString(object->GetName());
    attachment.subtitle = QStringLiteral("GAMEOBJECT");
    AddAttachment(std::move(attachment));
}

void AiChatGui::AttachComponent(const QString& id) {
    Component* component = Registry::FindInRuntime<Component>(id.toStdString());
    if (!component) return;

    Attachment attachment;
    attachment.kind = AttachmentKind::Component;
    attachment.id = id;
    attachment.key = QStringLiteral("component:%1").arg(id);
    attachment.title = QString::fromStdString(component->GetTypeName());
    if (GameObject* owner = component->GetGameObject()) {
        attachment.subtitle = QStringLiteral("COMPONENT / %1")
            .arg(QString::fromStdString(owner->GetName()));
    } else {
        attachment.subtitle = QStringLiteral("COMPONENT");
    }
    AddAttachment(std::move(attachment));
}

void AiChatGui::AttachSprite(const QString& id) {
    Sprite* sprite = AssetManager::Get().GetSprite(id.toStdString());
    if (!sprite) return;

    Attachment attachment;
    attachment.kind = AttachmentKind::Sprite;
    attachment.id = id;
    attachment.key = QStringLiteral("sprite:%1").arg(id);
    attachment.title = QString::fromStdString(sprite->GetName());
    if (attachment.title.isEmpty()) attachment.title = QStringLiteral("Sprite");
    attachment.subtitle = QStringLiteral("SPRITE");
    attachment.path = QString::fromStdString(sprite->GetFilePath());
    AddAttachment(std::move(attachment));
}

void AiChatGui::AddAttachment(Attachment attachment) {
    if (attachment.key.isEmpty()) return;
    for (const Attachment& existing : m_attachments) {
        if (existing.key == attachment.key) return;
    }
    // Keep accidental directory/multi-select drops from producing an enormous
    // invisible request. The strip is deliberately a small draft-context tray.
    if (m_attachments.size() >= 20) return;
    m_attachments.append(std::move(attachment));
    RenderAttachments();
}

void AiChatGui::RemoveAttachment(const QString& key) {
    for (qsizetype index = 0; index < m_attachments.size(); ++index) {
        if (m_attachments[index].key != key) continue;
        m_attachments.removeAt(index);
        RenderAttachments();
        return;
    }
}

void AiChatGui::ClearAttachments() {
    if (m_attachments.isEmpty()) return;
    m_attachments.clear();
    RenderAttachments();
}

void AiChatGui::RenderAttachments() {
    if (!m_attachmentLayout || !m_attachmentList || !m_attachmentScroll) return;
    while (QLayoutItem* item = m_attachmentLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) widget->deleteLater();
        delete item;
    }

    const auto iconFor = [](AttachmentKind kind) {
        switch (kind) {
        case AttachmentKind::File:       return QStringLiteral("F");
        case AttachmentKind::GameObject: return QStringLiteral("GO");
        case AttachmentKind::Component:  return QStringLiteral("C");
        case AttachmentKind::Material:   return QStringLiteral("M");
        case AttachmentKind::Texture:    return QStringLiteral("T");
        case AttachmentKind::Sprite:     return QStringLiteral("S");
        }
        return QStringLiteral("F");
    };

    const auto previewFor = [](const Attachment& attachment) -> QPixmap {
        constexpr int previewSize = 40;
        const auto iconPixmap = [](const QIcon& icon) {
            return icon.isNull() ? QPixmap{} : icon.pixmap(previewSize, previewSize);
        };

        switch (attachment.kind) {
        case AttachmentKind::File: {
            EditorUtils::CustomIconProvider provider;
            return iconPixmap(provider.icon(QFileInfo(attachment.path)));
        }
        case AttachmentKind::GameObject:
            return iconPixmap(EditorUtils::CustomIconProvider::gameObjectIcon());
        case AttachmentKind::Component: {
            Component* component = Registry::FindInRuntime<Component>(
                attachment.id.toStdString());
            if (auto* renderer = dynamic_cast<SpriteRenderer*>(component)) {
                if (!renderer->GetSpriteID().empty()) {
                    QPixmap sprite = AssetThumbnails::forSprite(renderer->GetSpriteID());
                    if (!sprite.isNull()) return sprite;
                }
            }
            return iconPixmap(EditorUtils::CustomIconProvider::componentIcon(
                attachment.title.toStdString()));
        }
        case AttachmentKind::Material:
            return AssetThumbnails::forMaterial(attachment.id.toStdString());
        case AttachmentKind::Texture:
            return AssetThumbnails::forTexture(attachment.id.toStdString());
        case AttachmentKind::Sprite:
            return AssetThumbnails::forSprite(attachment.id.toStdString());
        }
        return {};
    };

    for (const Attachment& attachment : m_attachments) {
        auto* chip = new QWidget(m_attachmentList);
        chip->setObjectName(QStringLiteral("AiAttachmentChip"));
        chip->setAttribute(Qt::WA_StyledBackground, true);
        chip->setFixedSize(238, 66);
        chip->setToolTip(!attachment.path.isEmpty() ? attachment.path : attachment.id);
        auto* chipLayout = new QHBoxLayout(chip);
        chipLayout->setContentsMargins(8, 7, 6, 7);
        chipLayout->setSpacing(9);

        auto* icon = new QLabel(chip);
        icon->setObjectName(QStringLiteral("AiAttachmentIcon"));
        icon->setAlignment(Qt::AlignCenter);
        icon->setFixedSize(46, 46);
        const QPixmap preview = previewFor(attachment);
        if (preview.isNull()) {
            icon->setText(iconFor(attachment.kind));
        } else {
            icon->setPixmap(preview.scaled(
                40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        chipLayout->addWidget(icon);

        auto* textLayout = new QVBoxLayout();
        textLayout->setContentsMargins(0, 2, 0, 2);
        textLayout->setSpacing(1);
        auto* title = new QLabel(attachment.title, chip);
        title->setObjectName(QStringLiteral("AiAttachmentTitle"));
        title->setMaximumWidth(142);
        title->setToolTip(attachment.title);
        auto* subtitle = new QLabel(attachment.subtitle, chip);
        subtitle->setObjectName(QStringLiteral("AiAttachmentKind"));
        subtitle->setMaximumWidth(142);
        textLayout->addWidget(title);
        textLayout->addWidget(subtitle);
        chipLayout->addLayout(textLayout, 1);

        auto* remove = new QPushButton(QStringLiteral("\u00d7"), chip);
        remove->setObjectName(QStringLiteral("AiAttachmentRemove"));
        remove->setFixedSize(24, 24);
        remove->setToolTip(QStringLiteral("Remove attachment"));
        connect(remove, &QPushButton::clicked, this,
                [this, key = attachment.key]() { RemoveAttachment(key); });
        chipLayout->addWidget(remove, 0, Qt::AlignTop);
        m_attachmentLayout->addWidget(chip);
    }

    const int count = static_cast<int>(m_attachments.size());
    const int contentWidth = count > 0 ? count * 238 + (count - 1) * 7 : 0;
    m_attachmentList->setFixedSize(contentWidth, 72);
    m_attachmentScroll->setVisible(count > 0);

    int left = 0, top = 0, right = 0, bottom = 0;
    m_messageLayout->getContentsMargins(&left, &top, &right, &bottom);
    m_messageLayout->setContentsMargins(left, top, right, count > 0 ? 244 : 168);
    if (count > 0) {
        QTimer::singleShot(0, m_attachmentScroll, [this]() {
            m_attachmentScroll->horizontalScrollBar()->setValue(
                m_attachmentScroll->horizontalScrollBar()->maximum());
        });
    }
}

void AiChatGui::UpdatePromptHeight() {
    if (!m_prompt || !m_prompt->document() ||
        !m_prompt->document()->documentLayout()) return;

    constexpr int minimumHeight = 34;
    constexpr int maximumHeight = 96;
    int visualLines = 0;
    for (QTextBlock block = m_prompt->document()->begin();
         block.isValid(); block = block.next()) {
        visualLines += qMax(1, block.lineCount());
    }

    // QPlainTextDocumentLayout reports its document height in blocks rather
    // than dependable viewport pixels. Convert its laid-out visual lines to
    // pixels ourselves, including soft wraps and explicit newlines.
    const int documentHeight = qMax(1, visualLines) *
                               m_prompt->fontMetrics().lineSpacing();
    const int targetHeight = qBound(
        minimumHeight, documentHeight + 14, maximumHeight);
    if (m_prompt->height() != targetHeight)
        m_prompt->setFixedHeight(targetHeight);
}

QString AiChatGui::BuildAttachmentContext(const Attachment& attachment) const {
    if (attachment.kind == AttachmentKind::File) {
        const QFileInfo info(attachment.path);
        YAML::Node metadata;
        metadata["kind"] = "file";
        metadata["name"] = attachment.title.toStdString();
        metadata["absolute_path"] = attachment.path.toStdString();
        const QString relative = ProjectRelativePath(attachment.path);
        if (!relative.isEmpty()) metadata["project_relative_path"] = relative.toStdString();
        metadata["exists"] = info.exists();
        if (info.exists()) {
            metadata["size_bytes"] = info.size();
            metadata["last_modified"] = info.lastModified().toString(Qt::ISODate).toStdString();
            metadata["mime_type"] = QMimeDatabase().mimeTypeForFile(
                info, QMimeDatabase::MatchExtension).name().toStdString();
        }

        QString context = DumpYaml(metadata);
        if (!info.exists() || !IsTextFile(info)) return context;

        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
            return context + QStringLiteral("\ncontent_error: unable to read file");
        QByteArray bytes = file.read(kMaxEmbeddedFileBytes + 1);
        const bool truncated = bytes.size() > kMaxEmbeddedFileBytes;
        if (truncated) bytes.truncate(kMaxEmbeddedFileBytes);
        context += QStringLiteral("\ncontent%1:\n<<<FILE_CONTENT\n%2\nFILE_CONTENT")
            .arg(truncated ? QStringLiteral("_truncated") : QString(),
                 QString::fromUtf8(bytes));
        return context;
    }

    YAML::Node metadata;
    metadata["source"] = "live RockEngine memory";
    metadata["attachment_id"] = attachment.id.toStdString();

    if (attachment.kind == AttachmentKind::GameObject) {
        GameObject* object = Registry::FindInRuntime<GameObject>(attachment.id.toStdString());
        metadata["kind"] = "gameobject";
        if (!object) {
            metadata["available"] = false;
            return DumpYaml(metadata);
        }
        metadata["available"] = true;
        metadata["active"] = object->GetActive();
        metadata["object"] = object->Serialize();
        if (Scene* scene = object->GetScene()) {
            metadata["scene"]["id"] = scene->GetID();
            metadata["scene"]["name"] = scene->GetName();
            metadata["scene"]["path"] = scene->GetPath();
        }
        YAML::Node components(YAML::NodeType::Sequence);
        for (Component* component : object->GetAllComponents()) {
            if (component) components.push_back(component->Serialize());
        }
        metadata["components"] = components;
        return DumpYaml(metadata);
    }

    if (attachment.kind == AttachmentKind::Component) {
        Component* component = Registry::FindInRuntime<Component>(attachment.id.toStdString());
        metadata["kind"] = "component";
        if (!component) {
            metadata["available"] = false;
            return DumpYaml(metadata);
        }
        metadata["available"] = true;
        metadata["component"] = component->Serialize();
        if (GameObject* owner = component->GetGameObject()) {
            metadata["owner"]["id"] = owner->GetID();
            metadata["owner"]["name"] = owner->GetName();
            metadata["owner"]["type"] = owner->GetTypeName();
            metadata["owner"]["active"] = owner->GetActive();
        }
        return DumpYaml(metadata);
    }

    Resource* resource = ResourceForId(attachment.id.toStdString());
    metadata["kind"] = attachment.subtitle.toLower().toStdString();
    if (!resource) {
        metadata["available"] = false;
        return DumpYaml(metadata);
    }
    metadata["available"] = true;
    metadata["id"] = resource->GetID();
    metadata["name"] = resource->GetName();
    metadata["type"] = resource->GetTypeName();
    metadata["metadata_file"] = resource->GetFilePath();
    metadata["resource"] = resource->Serialize();
    if (auto* texture = dynamic_cast<Texture2D*>(resource)) {
        metadata["source_image"] = texture->GetPath();
        metadata["width"] = texture->GetWidth();
        metadata["height"] = texture->GetHeight();
    } else if (auto* sprite = dynamic_cast<Sprite*>(resource)) {
        metadata["texture_id"] = sprite->GetTextureID();
        if (Texture2D* texture = sprite->GetTexture())
            metadata["source_image"] = texture->GetPath();
    }
    return DumpYaml(metadata);
}

QString AiChatGui::BuildRequestMessage(const QString& prompt) const {
    QString request = prompt.trimmed();
    if (request.isEmpty()) request = QStringLiteral("Use the attached context.");
    if (m_attachments.isEmpty()) return request;

    QStringList contexts;
    contexts.reserve(m_attachments.size());
    for (qsizetype index = 0; index < m_attachments.size(); ++index) {
        const Attachment& attachment = m_attachments[index];
        QString context;
        try {
            context = BuildAttachmentContext(attachment);
        } catch (const std::exception& error) {
            context = QStringLiteral("serialization_error: %1")
                .arg(QString::fromUtf8(error.what()));
        } catch (...) {
            context = QStringLiteral("serialization_error: unknown error");
        }
        contexts.append(QStringLiteral("--- attachment %1: %2 ---\n%3")
            .arg(index + 1)
            .arg(attachment.title, context));
    }
    return request + QStringLiteral(
        "\n\n<rockengine_attachments>\n"
        "These attachments are user-selected context for the request. Engine "
        "objects and assets are snapshots of their current live in-memory state.\n%1\n"
        "</rockengine_attachments>").arg(contexts.join(QStringLiteral("\n\n")));
}

void AiChatGui::RefreshSettingsMenu() {
    const auto provider = CurrentProvider();
    const AuthState& state = m_auth[ProviderIndex(provider)];
    const QString providerName = ai::AiAgentService::ProviderName(provider);
    m_settingsProviderName->setText(providerName);
    m_settingsConnection->setText(state.authenticated
        ? QStringLiteral("Connected account")
        : QStringLiteral("Signed out"));
    m_settingsConnection->setToolTip(state.detail);

    const QPixmap logo = ProviderLogo(provider);
    if (logo.isNull()) {
        m_settingsProviderLogo->setPixmap({});
        m_settingsProviderLogo->setText(providerName.left(1));
    } else {
        m_settingsProviderLogo->clear();
        m_settingsProviderLogo->setPixmap(
            logo.scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    for (QAction* action : m_providerMenu->actions()) {
        action->setChecked(action->data().toInt() == ProviderIndex(provider));
    }
}

bool AiChatGui::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_gradientLayer && event->type() == QEvent::Resize) {
        QTimer::singleShot(0, this, &AiChatGui::ConfigureGradientAnimations);
    }
    if ((watched == m_apiKey || watched == m_prompt) &&
        (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
        QWidget* frame = watched == m_apiKey ? m_credentialFrame : m_composerFrame;
        frame->setProperty("inputFocused", event->type() == QEvent::FocusIn);
        RefreshDynamicStyle(frame);
    }
    if (watched == m_prompt && event->type() == QEvent::Resize)
        QTimer::singleShot(0, this, &AiChatGui::UpdatePromptHeight);
    if (watched == m_prompt && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) &&
            !key->modifiers().testFlag(Qt::ShiftModifier)) {
            SubmitPrompt();
            return true;
        }
    }
    if (m_attachmentScroll && watched == m_attachmentScroll->viewport() &&
        event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        QScrollBar* bar = m_attachmentScroll->horizontalScrollBar();
        if (bar->maximum() > 0) {
            const int delta = !wheel->pixelDelta().isNull()
                ? wheel->pixelDelta().y()
                : wheel->angleDelta().y() / 2;
            bar->setValue(bar->value() - delta);
            wheel->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void AiChatGui::dragEnterEvent(QDragEnterEvent* event) {
    if (!CanAttachMimeData(event->mimeData())) {
        event->ignore();
        return;
    }
    event->setDropAction(Qt::CopyAction);
    event->accept();
    SetAttachmentDropActive(true);
}

void AiChatGui::dragMoveEvent(QDragMoveEvent* event) {
    if (!CanAttachMimeData(event->mimeData())) {
        event->ignore();
        SetAttachmentDropActive(false);
        return;
    }
    event->setDropAction(Qt::CopyAction);
    event->accept();
    SetAttachmentDropActive(true);
}

void AiChatGui::dragLeaveEvent(QDragLeaveEvent* event) {
    SetAttachmentDropActive(false);
    event->accept();
}

void AiChatGui::dropEvent(QDropEvent* event) {
    const bool supported = CanAttachMimeData(event->mimeData());
    if (supported) AttachMimeData(event->mimeData());
    SetAttachmentDropActive(false);
    if (!supported) {
        event->ignore();
        return;
    }
    // Always copy. Returning Move to the Hierarchy's InternalMove model would
    // make it remove the source row after attaching it.
    event->setDropAction(Qt::CopyAction);
    event->accept();
    m_prompt->setFocus();
}

void AiChatGui::SetAttachmentDropActive(bool active) {
    if (!m_composerFrame || m_composerFrame->property("dropActive").toBool() == active)
        return;
    m_composerFrame->setProperty("dropActive", active);
    RefreshDynamicStyle(m_composerFrame);
}

void AiChatGui::SubmitPrompt() {
    if (m_busy) return;
    const QString message = m_prompt->toPlainText().trimmed();
    if (message.isEmpty() && m_attachments.isEmpty()) return;
    const QString request = BuildRequestMessage(message);
    const QString transcriptText = message.isEmpty()
        ? QStringLiteral("Use the attached context.") : message;
    m_prompt->clear();
    ClearAttachments();
    AppendMessage(QStringLiteral("You"), transcriptText);
    ai::AiAgentService::Get()->SendMessage(CurrentProvider(), SelectedModel(), request);
}

void AiChatGui::AppendMessage(const QString& role, const QString& text, bool error) {
    m_messages.append({role, text});
    m_messageErrors.append(error);
    RenderTranscript();
}

void AiChatGui::UpdateStreamingMessage(const QString& text) {
    if (m_streamingMessageIndex < 0 || m_streamingMessageIndex >= m_messages.size()) {
        m_messages.append({QStringLiteral("Assistant"), text});
        m_messageErrors.append(false);
        m_streamingMessageIndex = m_messages.size() - 1;
        RenderTranscript();
        return;
    }

    m_messages[m_streamingMessageIndex].second = text;
    if (m_streamingBubble) m_streamingBubble->setText(text);
    QTimer::singleShot(0, m_transcript, [this]() {
        m_transcript->verticalScrollBar()->setValue(
            m_transcript->verticalScrollBar()->maximum());
    });
}

void AiChatGui::FinishStreamingMessage(const QString& text) {
    if (m_streamingMessageIndex < 0 || m_streamingMessageIndex >= m_messages.size()) {
        AppendMessage(QStringLiteral("Assistant"), text);
    } else {
        m_messages[m_streamingMessageIndex].second = text;
        if (m_streamingBubble) m_streamingBubble->setText(text);
    }
    m_streamingMessageIndex = -1;
    m_streamingBubble.clear();
}

void AiChatGui::RenderTranscript() {
    m_streamingBubble.clear();
    while (QLayoutItem* item = m_messageLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) widget->deleteLater();
        delete item;
    }

    if (m_messages.isEmpty()) {
        m_messageLayout->addStretch(1);
    } else {
        for (qsizetype i = 0; i < m_messages.size(); ++i) {
            const QString& role = m_messages[i].first;
            const bool userMessage = role == QStringLiteral("You");
            const bool error = m_messageErrors.value(i);

            auto* row = new QWidget(m_messageList);
            row->setObjectName(QStringLiteral("AiMessageRow"));
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(0);

            auto* bubble = new QLabel(row);
            bubble->setObjectName(error ? QStringLiteral("AiErrorBubble")
                                        : (userMessage ? QStringLiteral("AiUserBubble")
                                                       : QStringLiteral("AiAssistantBubble")));
            bubble->setText(error
                ? QStringLiteral("Error\n\n%1").arg(m_messages[i].second)
                : m_messages[i].second);
            bubble->setTextFormat(Qt::PlainText);
            bubble->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
            bubble->setWordWrap(true);
            if (userMessage || error) {
                bubble->setMaximumWidth(userMessage ? 430 : 560);
                bubble->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
            } else {
                bubble->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
            }
            if (i == m_streamingMessageIndex) m_streamingBubble = bubble;

            if (userMessage) rowLayout->addStretch(1);
            rowLayout->addWidget(bubble, userMessage || error ? 0 : 1);
            if (error) rowLayout->addStretch(1);
            m_messageLayout->addWidget(row);
        }
        m_messageLayout->addStretch(1);
    }

    QTimer::singleShot(0, m_transcript, [this]() {
        m_transcript->verticalScrollBar()->setValue(
            m_transcript->verticalScrollBar()->maximum());
    });
}

void AiChatGui::ClearLoginError() {
    m_loginAttempt = LoginAttempt::None;
    m_loginStatus->clear();
    m_loginStatus->hide();
}

void AiChatGui::ShowLoginError(const QString& detail) {
    QString message = detail.trimmed();
    if (message.isEmpty()) message = QStringLiteral("The provider did not return an error reason.");

    const QString heading = m_loginAttempt == LoginAttempt::ApiKey
        ? QStringLiteral("Unable to use API key")
        : QStringLiteral("Unable to log in");
    m_loginStatus->setText(QStringLiteral("%1\n%2").arg(heading, message));
    m_loginStatus->show();
}
