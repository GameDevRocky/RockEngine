#include "mcp/Tools.hpp"

#include "mcp/McpDispatcher.hpp"
#include "mcp/UserClarification.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

#include <utility>

namespace mcp {

void RegisterClarificationTools(McpDispatcher& dispatcher) {
    dispatcher.RegisterTool("user.request_clarification", [](const QJsonObject& params) {
        ClarificationRequest request;
        request.title = params.value("title").toString(QStringLiteral("Clarification needed"));
        request.question = params.value("question").toString().trimmed();
        request.context = params.value("context").toString();
        request.allowMultiple = params.value("allowMultiple").toBool(false);
        if (request.question.isEmpty())
            return McpResult::Error(InvalidParams, "missing \"question\"");

        const QJsonArray options = params.value("options").toArray();
        if (options.isEmpty())
            return McpResult::Error(InvalidParams, "provide at least one concrete answer option");

        QSet<QString> ids;
        for (int i = 0; i < options.size(); ++i) {
            const QJsonValue value = options[i];
            ClarificationOption option;
            if (value.isString()) {
                option.label = value.toString().trimmed();
                option.id = QStringLiteral("option_%1").arg(i + 1);
            } else if (value.isObject()) {
                const QJsonObject object = value.toObject();
                option.id = object.value("id").toString().trimmed();
                option.label = object.value("label").toString().trimmed();
                option.description = object.value("description").toString().trimmed();
            }
            if (option.id.isEmpty()) option.id = QStringLiteral("option_%1").arg(i + 1);
            if (option.label.isEmpty())
                return McpResult::Error(InvalidParams,
                    QString("clarification option %1 has no label").arg(i + 1));
            if (option.id == "other" || ids.contains(option.id))
                return McpResult::Error(InvalidParams,
                    "option ids must be unique and \"other\" is reserved");
            ids.insert(option.id);
            request.options.push_back(std::move(option));
        }

        const QString requestId = UserClarificationService::Get()->Create(std::move(request));
        return McpResult::Ok(QJsonObject{
            {"requestId", requestId},
            {"status", "pending"},
            {"pollTool", "user.clarification_status"}
        });
    });

    dispatcher.RegisterTool("user.clarification_status", [](const QJsonObject& params) {
        const QString requestId = params.value("requestId").toString();
        if (requestId.isEmpty())
            return McpResult::Error(InvalidParams, "missing \"requestId\"");
        const QJsonObject status = UserClarificationService::Get()->Status(requestId);
        if (!status.value("found").toBool())
            return McpResult::Error(ObjectNotFound,
                                    "no clarification request with id " + requestId);
        return McpResult::Ok(status);
    });
}

} // namespace mcp
