#include "dock-widgets/AiChatGui.hpp"
#include "engine/utils/EngineUtils.hpp"

#include <QComboBox>
#include <QEasingCurve>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QPixmap>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

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
    auto* chatLayout = new QVBoxLayout(chatPage);
    chatLayout->setContentsMargins(10, 9, 10, 8);
    chatLayout->setSpacing(7);

    auto* chatHeader = new QHBoxLayout();
    auto* chatHeaderLeft = new QWidget(chatPage);
    chatHeaderLeft->setFixedWidth(66);
    auto* chatTitle = new QLabel(QStringLiteral("AI Assistant Chat"), chatPage);
    chatTitle->setObjectName(QStringLiteral("AiPageTitle"));
    chatTitle->setAlignment(Qt::AlignCenter);
    m_signOut = new QPushButton(QStringLiteral("Sign out"), chatPage);
    m_signOut->setObjectName(QStringLiteral("AiHeaderAction"));
    m_signOut->setFixedWidth(66);
    m_signOut->setToolTip(QStringLiteral("Erase this provider's credentials and sign out"));
    chatHeader->addWidget(chatHeaderLeft);
    chatHeader->addWidget(chatTitle, 1);
    chatHeader->addWidget(m_signOut);
    chatLayout->addLayout(chatHeader);

    m_transcript = new QScrollArea(chatPage);
    m_transcript->setObjectName(QStringLiteral("AiTranscript"));
    m_transcript->setFrameShape(QFrame::NoFrame);
    m_transcript->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_transcript->setWidgetResizable(true);
    m_messageList = new QWidget(m_transcript);
    m_messageList->setObjectName(QStringLiteral("AiMessageList"));
    m_messageLayout = new QVBoxLayout(m_messageList);
    m_messageLayout->setContentsMargins(7, 8, 7, 8);
    m_messageLayout->setSpacing(10);
    m_transcript->setWidget(m_messageList);
    chatLayout->addWidget(m_transcript, 1);

    m_activity = new QLabel(chatPage);
    m_activity->setObjectName(QStringLiteral("AiActivity"));
    m_activity->setWordWrap(true);
    m_activity->setMinimumHeight(m_activity->fontMetrics().height() + 4);
    chatLayout->addWidget(m_activity);

    auto* composerRow = new QHBoxLayout();
    composerRow->setSpacing(7);
    m_newChat = new QPushButton(QStringLiteral("+"), chatPage);
    m_newChat->setObjectName(QStringLiteral("AiRoundButton"));
    m_newChat->setFixedSize(34, 34);
    m_newChat->setToolTip(QStringLiteral("Start a new conversation"));
    composerRow->addWidget(m_newChat, 0, Qt::AlignBottom);

    m_composerFrame = new QWidget(chatPage);
    m_composerFrame->setObjectName(QStringLiteral("AiComposerFrame"));
    m_composerFrame->setAttribute(Qt::WA_Hover, true);
    auto* composerLayout = new QHBoxLayout(m_composerFrame);
    composerLayout->setContentsMargins(5, 3, 5, 3);
    composerLayout->setSpacing(4);
    m_prompt = new QPlainTextEdit(m_composerFrame);
    m_prompt->setObjectName(QStringLiteral("AiPrompt"));
    m_prompt->setPlaceholderText(QStringLiteral("Ask a follow-up…"));
    m_prompt->setMinimumHeight(38);
    m_prompt->setMaximumHeight(76);
    m_prompt->installEventFilter(this);
    composerLayout->addWidget(m_prompt, 1);
    m_stop = new QPushButton(QStringLiteral("■"), m_composerFrame);
    m_stop->setObjectName(QStringLiteral("AiSendButton"));
    m_stop->setFixedSize(28, 28);
    m_stop->setToolTip(QStringLiteral("Stop response"));
    m_send = new QPushButton(QStringLiteral("↑"), m_composerFrame);
    m_send->setObjectName(QStringLiteral("AiSendButton"));
    m_send->setFixedSize(28, 28);
    m_send->setToolTip(QStringLiteral("Send message (Ctrl+Enter)"));
    m_stop->hide();
    composerLayout->addWidget(m_stop, 0, Qt::AlignBottom);
    composerLayout->addWidget(m_send, 0, Qt::AlignBottom);
    composerRow->addWidget(m_composerFrame, 1);
    chatLayout->addLayout(composerRow);

    auto* footer = new QHBoxLayout();
    m_providerStatus = new QLabel(chatPage);
    m_providerStatus->setObjectName(QStringLiteral("AiFooterText"));
    auto* shortcut = new QLabel(QStringLiteral("Ctrl+Enter to send"), chatPage);
    shortcut->setObjectName(QStringLiteral("AiFooterText"));
    footer->addWidget(m_providerStatus);
    footer->addStretch();
    footer->addWidget(shortcut);
    chatLayout->addLayout(footer);
    m_pages->addWidget(chatPage);

    RenderTranscript();

    connect(m_provider, &QComboBox::currentIndexChanged, this, [this](int index) {
        QSettings().setValue(QStringLiteral("ai/provider"), index);
        ClearLoginError();
        m_messages.clear();
        m_messageErrors.clear();
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
        RenderTranscript();
        m_activity->setText(QStringLiteral("Started a new conversation"));
    });
    connect(m_signOut, &QPushButton::clicked, this, [this]() {
        ClearLoginError();
        m_messages.clear();
        m_messageErrors.clear();
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
        m_provider->setEnabled(!busy);
        m_accountLogin->setEnabled(!busy && m_auth[ProviderIndex(CurrentProvider())].cliAvailable);
        m_apiKeyLogin->setEnabled(!busy && m_auth[ProviderIndex(CurrentProvider())].cliAvailable);
        m_apiKeyLogin->setText(busy && m_pages->currentIndex() == 0
            ? QStringLiteral("…") : QStringLiteral("→"));
        m_apiKey->setEnabled(!busy);
        m_loginCancel->setVisible(busy && m_pages->currentIndex() == 0);
        m_prompt->setEnabled(!busy);
        m_send->setVisible(!busy);
        m_stop->setVisible(busy && m_pages->currentIndex() == 1);
        m_newChat->setEnabled(!busy);
        m_signOut->setEnabled(!busy);
    });
    connect(service, &ai::AiAgentService::ActivityChanged,
            m_activity, &QLabel::setText);
    connect(service, &ai::AiAgentService::ResponseReady, this,
            [this](ai::AiAgentService::Provider provider, const QString& text) {
        if (provider == CurrentProvider()) AppendMessage(QStringLiteral("Assistant"), text);
    });
    connect(service, &ai::AiAgentService::RequestFailed, this,
            [this](ai::AiAgentService::Provider provider, const QString& error) {
        if (provider != CurrentProvider()) return;
        if (m_pages->currentIndex() == 0) ShowLoginError(error);
        else AppendMessage(QStringLiteral("Error"), error, true);
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

    if (!state.checked) {
        m_providerStatus->setText(
            QStringLiteral("%1 · Checking…").arg(providerName));
    } else {
        m_providerStatus->setText(QStringLiteral("%1 · %2").arg(
            providerName,
            state.authenticated ? QStringLiteral("Connected") : QStringLiteral("Signed out")));
    }

    m_pages->setCurrentIndex(state.authenticated ? 1 : 0);
    m_accountLogin->setEnabled(!m_busy && state.cliAvailable);
    m_apiKeyLogin->setEnabled(!m_busy && state.cliAvailable);
    m_loginCancel->setVisible(m_busy && !state.authenticated);
    m_stop->setVisible(m_busy && state.authenticated);
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
    if (watched == m_prompt && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) &&
            key->modifiers().testFlag(Qt::ControlModifier)) {
            SubmitPrompt();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void AiChatGui::SubmitPrompt() {
    if (m_busy) return;
    const QString message = m_prompt->toPlainText().trimmed();
    if (message.isEmpty()) return;
    m_prompt->clear();
    AppendMessage(QStringLiteral("You"), message);
    ai::AiAgentService::Get()->SendMessage(CurrentProvider(), message);
}

void AiChatGui::AppendMessage(const QString& role, const QString& text, bool error) {
    m_messages.append({role, text});
    m_messageErrors.append(error);
    RenderTranscript();
}

void AiChatGui::RenderTranscript() {
    while (QLayoutItem* item = m_messageLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) widget->deleteLater();
        delete item;
    }

    if (m_messages.isEmpty()) {
        auto* empty = new QLabel(QStringLiteral(
            "Start a conversation\n\nAsk the assistant to inspect code, edit project files, "
            "or work with the running scene."), m_messageList);
        empty->setObjectName(QStringLiteral("AiEmptyChat"));
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        m_messageLayout->addStretch(1);
        m_messageLayout->addWidget(empty);
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
            bubble->setMaximumWidth(userMessage ? 430 : 560);
            bubble->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

            if (userMessage) rowLayout->addStretch(1);
            rowLayout->addWidget(bubble);
            if (!userMessage) rowLayout->addStretch(1);
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
