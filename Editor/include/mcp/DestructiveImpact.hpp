#pragma once

#include "mcp/UserClarification.hpp"

#include <QJsonArray>
#include <QString>

class Component;
class GameObject;

namespace mcp {

struct ImpactClarification {
    ClarificationRequest request;
    QJsonArray affected;
    int affectedTotal = 0;
    bool truncated = false;
};

ImpactClarification AnalyzeComponentRemoval(GameObject* owner, Component* component);
ImpactClarification AnalyzeObjectDestruction(GameObject* object);

} // namespace mcp
