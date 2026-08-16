#include "mcp/Tools.hpp"

#include "mcp/McpDispatcher.hpp"
#include "mcp/ToolSupport.hpp"
#include "mcp/YamlJson.hpp"

#include "engine/components/Component.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/debug/Console.hpp"
#include "engine/debug/Message.hpp"
#include "dock-widgets/SceneViewGui.hpp"

#include <QBuffer>
#include <QJsonArray>
#include <QJsonObject>
#include <QPixmap>

#include <algorithm>
#include <vector>

namespace mcp {

// The fallback for "show me everything about this object" -- including fields no
// hand-written tool exposes yet.
void RegisterDebugTools(McpDispatcher& dispatcher) {
    dispatcher.RegisterTool("object.dump_state", [](const QJsonObject& params) {
        GameObject* object = nullptr;
        if (McpResult resolved = support::ResolveGameObject(params, &object); !resolved.ok)
            return resolved;

        QJsonArray components;
        for (Component* component : object->GetAllComponents()) {
            QJsonObject entry;
            entry["type"] = QString::fromStdString(component->GetTypeName());
            entry["state"] = YamlToJson(component->Serialize());
            components.append(entry);
        }

        QJsonObject data;
        data["gameobject"] = YamlToJson(object->Serialize());
        data["components"] = components;
        return McpResult::Ok(data);
    });

    dispatcher.RegisterTool("console.get_messages", [](const QJsonObject& params) {
        const int limit = std::clamp(params.value("limit").toInt(100), 1, 1000);
        const QString type = params.value("type").toString().toLower();
        std::vector<Message*> messages;
        for (const auto& [_, message] : Console::GetMessages()) {
            if (!message || message->isDestroyed) continue;
            if (!type.isEmpty() && QString::fromStdString(message->type).toLower() != type) continue;
            messages.push_back(message);
        }
        std::sort(messages.begin(), messages.end(), [](Message* a, Message* b) {
            return a->time_stamp > b->time_stamp;
        });
        QJsonArray items;
        for (int i = 0; i < std::min<int>(limit, messages.size()); ++i) {
            Message* message = messages[i];
            items.append(QJsonObject{
                {"text", QString::fromStdString(message->text)},
                {"type", QString::fromStdString(message->type)},
                {"file", QString::fromStdString(message->file_name)},
                {"line", QString::fromStdString(message->line_num)},
                {"function", QString::fromStdString(message->created_at)},
                {"count", message->count},
                {"time", message->time_stamp}
            });
        }
        return McpResult::Ok(QJsonObject{{"total", static_cast<int>(messages.size())},
                                         {"returned", items.size()}, {"items", items}});
    });

    // Return a PNG data URL so any MCP client can inspect the actual rendered
    // editor viewport without requiring a shared filesystem path.
    dispatcher.RegisterTool("scene.capture_view", [](const QJsonObject&) {
        SceneViewGui* view = SceneViewGui::Get();
        if (!view || !view->isVisible())
            return McpResult::Error(WrongMode, "the Scene view is not visible");
        const QPixmap pixmap = view->grab();
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!pixmap.save(&buffer, "PNG"))
            return McpResult::Error(WrongMode, "failed to capture the Scene view");
        return McpResult::Ok(QJsonObject{
            {"mimeType", "image/png"},
            {"width", pixmap.width()},
            {"height", pixmap.height()},
            {"dataUrl", "data:image/png;base64," + QString::fromLatin1(bytes.toBase64())}
        });
    });
}

} // namespace mcp
