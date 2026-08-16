#include "mcp/UserClarification.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace mcp {
namespace {

constexpr int kMaximumOptions = 12;
constexpr int kMaximumRetainedRequests = 64;

QString Limited(QString text, int length) {
    text = text.trimmed();
    return text.left(length);
}

QJsonObject RequestPresentation(const ClarificationRequest& request) {
    QJsonArray options;
    for (const ClarificationOption& option : request.options) {
        options.append(QJsonObject{
            {"id", option.id},
            {"label", option.label},
            {"description", option.description}
        });
    }
    return QJsonObject{
        {"title", request.title},
        {"question", request.question},
        {"context", request.context},
        {"options", options},
        {"allowMultiple", request.allowMultiple}
    };
}

} // namespace

struct UserClarificationService::Record {
    ClarificationRequest request;
    QString status = QStringLiteral("pending");
    QJsonObject answer;
    QDateTime createdAt = QDateTime::currentDateTimeUtc();
    QDateTime answeredAt;
    bool consumed = false;
};

UserClarificationService* UserClarificationService::Get() {
    static UserClarificationService* instance =
        new UserClarificationService(QCoreApplication::instance());
    return instance;
}

UserClarificationService::UserClarificationService(QObject* parent) : QObject(parent) {}

QString UserClarificationService::Create(ClarificationRequest request) {
    request.title = Limited(request.title, 160);
    if (request.title.isEmpty()) request.title = QStringLiteral("Clarification needed");
    request.question = Limited(request.question, 1000);
    request.context = Limited(request.context, 4000);

    QVector<ClarificationOption> sanitized;
    QSet<QString> ids;
    for (ClarificationOption option : request.options) {
        if (sanitized.size() >= kMaximumOptions) break;
        option.id = Limited(option.id, 80);
        option.label = Limited(option.label, 240);
        option.description = Limited(option.description, 700);
        if (option.id.isEmpty() || option.label.isEmpty() || option.id == QStringLiteral("other") ||
            ids.contains(option.id)) continue;
        ids.insert(option.id);
        sanitized.push_back(std::move(option));
    }
    sanitized.push_back({QStringLiteral("other"), QStringLiteral("Other"),
                         QStringLiteral("Provide a different answer or additional instructions.")});
    request.options = std::move(sanitized);

    PruneFinished();
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto* record = new Record();
    record->request = std::move(request);
    m_records.insert(id, record);
    Q_EMIT ClarificationRequested(id, RequestPresentation(record->request));
    return id;
}

bool UserClarificationService::Answer(const QString& requestId,
                                      const QStringList& selectedIds,
                                      const QString& otherText,
                                      QString* error) {
    Record* record = m_records.value(requestId, nullptr);
    if (!record) {
        if (error) *error = QStringLiteral("unknown clarification request");
        return false;
    }
    if (record->status != QStringLiteral("pending")) {
        if (error) *error = QStringLiteral("clarification has already been resolved");
        return false;
    }

    QHash<QString, QString> validOptions;
    for (const ClarificationOption& option : record->request.options)
        validOptions.insert(option.id, option.label);

    QStringList sanitizedIds;
    QSet<QString> seen;
    for (QString id : selectedIds) {
        id = id.trimmed();
        if (!validOptions.contains(id) || seen.contains(id)) continue;
        seen.insert(id);
        sanitizedIds.push_back(id);
    }
    if (sanitizedIds.isEmpty()) {
        if (error) *error = QStringLiteral("select at least one answer");
        return false;
    }
    if (!record->request.allowMultiple && sanitizedIds.size() != 1) {
        if (error) *error = QStringLiteral("this clarification accepts one answer");
        return false;
    }
    const QString trimmedOther = Limited(otherText, 4000);
    if (sanitizedIds.contains(QStringLiteral("other")) && trimmedOther.isEmpty()) {
        if (error) *error = QStringLiteral("describe the Other answer before continuing");
        return false;
    }

    QJsonArray selected;
    QJsonArray selectedIdValues;
    for (const QString& id : sanitizedIds) {
        selected.append(QJsonObject{{"id", id}, {"label", validOptions.value(id)}});
        selectedIdValues.append(id);
    }
    record->answer = QJsonObject{
        {"selected", selected},
        {"selectedIds", selectedIdValues},
        {"otherText", trimmedOther}
    };
    record->status = QStringLiteral("answered");
    record->answeredAt = QDateTime::currentDateTimeUtc();
    Q_EMIT ClarificationResolved(requestId, Status(requestId));
    return true;
}

bool UserClarificationService::Cancel(const QString& requestId) {
    Record* record = m_records.value(requestId, nullptr);
    if (!record || record->status != QStringLiteral("pending")) return false;
    record->status = QStringLiteral("cancelled");
    record->answeredAt = QDateTime::currentDateTimeUtc();
    Q_EMIT ClarificationResolved(requestId, Status(requestId));
    return true;
}

QJsonObject UserClarificationService::Status(const QString& requestId) const {
    Record* record = m_records.value(requestId, nullptr);
    if (!record) return QJsonObject{{"found", false}, {"requestId", requestId}};
    QJsonObject result{
        {"found", true},
        {"requestId", requestId},
        {"status", record->status},
        {"allowMultiple", record->request.allowMultiple},
        {"question", record->request.question}
    };
    if (!record->answer.isEmpty()) {
        for (auto it = record->answer.begin(); it != record->answer.end(); ++it)
            result.insert(it.key(), it.value());
    }
    if (record->answeredAt.isValid())
        result["answeredAt"] = record->answeredAt.toString(Qt::ISODateWithMs);
    result["consumed"] = record->consumed;
    return result;
}

bool UserClarificationService::Consume(const QString& requestId,
                                       const QString& expectedScope,
                                       const QString& requiredSelection,
                                       QJsonObject* answer, QString* error) {
    Record* record = m_records.value(requestId, nullptr);
    if (!record) {
        if (error) *error = QStringLiteral("unknown clarification request");
        return false;
    }
    if (record->request.scope != expectedScope) {
        if (error) *error = QStringLiteral("clarification does not authorize this action");
        return false;
    }
    if (record->status != QStringLiteral("answered")) {
        if (error) *error = record->status == QStringLiteral("cancelled")
            ? QStringLiteral("the user cancelled the clarification")
            : QStringLiteral("the clarification is still pending");
        return false;
    }
    if (record->consumed) {
        if (error) *error = QStringLiteral("clarification authorization was already used");
        return false;
    }
    const QJsonArray selected = record->answer.value(QStringLiteral("selectedIds")).toArray();
    bool containsRequired = false;
    for (const QJsonValue& value : selected)
        if (value.toString() == requiredSelection) containsRequired = true;
    if (!containsRequired) {
        if (error) *error = QStringLiteral("the user did not approve this action");
        return false;
    }
    record->consumed = true;
    if (answer) *answer = Status(requestId);
    return true;
}

void UserClarificationService::PruneFinished() {
    if (m_records.size() < kMaximumRetainedRequests) return;
    QList<QString> finished;
    for (auto it = m_records.cbegin(); it != m_records.cend(); ++it)
        if (it.value()->status != QStringLiteral("pending")) finished.push_back(it.key());
    std::sort(finished.begin(), finished.end(), [this](const QString& a, const QString& b) {
        return m_records[a]->answeredAt < m_records[b]->answeredAt;
    });
    while (m_records.size() >= kMaximumRetainedRequests && !finished.isEmpty()) {
        const QString id = finished.takeFirst();
        delete m_records.take(id);
    }
}

} // namespace mcp
