#include "mcp/DestructiveImpact.hpp"

#include "mcp/ToolSupport.hpp"

#include "engine/components/Animator.hpp"
#include "engine/components/AudioListener.hpp"
#include "engine/components/Camera.hpp"
#include "engine/components/Collider.hpp"
#include "engine/components/Joint.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ShadowCaster.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/SceneManager.hpp"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace mcp {
namespace {

constexpr int kMaximumReturnedAffectedItems = 100;
constexpr int kMaximumContextItems = 10;

QJsonObject ComponentItem(Component* component, const QString& effect,
                          const QString& reason) {
    QJsonObject item{
        {"kind", "Component"},
        {"id", QString::fromStdString(component->GetID())},
        {"type", QString::fromStdString(component->GetTypeName())},
        {"effect", effect},
        {"reason", reason}
    };
    if (GameObject* owner = component->GetGameObject()) {
        item["objectId"] = QString::fromStdString(owner->GetID());
        item["objectName"] = QString::fromStdString(owner->GetName());
    }
    return item;
}

QJsonObject ObjectItem(GameObject* object, const QString& effect,
                       const QString& reason) {
    return QJsonObject{
        {"kind", "GameObject"},
        {"id", QString::fromStdString(object->GetID())},
        {"name", QString::fromStdString(object->GetName())},
        {"effect", effect},
        {"reason", reason},
        {"componentCount", static_cast<int>(object->GetAllComponents().size())}
    };
}

std::vector<GameObject*> AllObjects() {
    std::vector<GameObject*> result;
    Container* container = support::ActiveContainer();
    SceneManager* scenes = container ? container->FindSystem<SceneManager>() : nullptr;
    if (!scenes) return result;
    for (Scene* scene : scenes->GetScenes()) {
        if (!scene) continue;
        const auto& objects = scene->GetAllGameObjects();
        result.insert(result.end(), objects.begin(), objects.end());
    }
    return result;
}

void AppendAffected(ImpactClarification& impact, const QJsonObject& item) {
    ++impact.affectedTotal;
    if (impact.affected.size() < kMaximumReturnedAffectedItems)
        impact.affected.append(item);
    else
        impact.truncated = true;
}

QString AffectedSummary(const ImpactClarification& impact) {
    QStringList lines;
    const int shown = std::min(kMaximumContextItems,
                               static_cast<int>(impact.affected.size()));
    for (int i = 0; i < shown; ++i) {
        const QJsonObject item = impact.affected[i].toObject();
        const QString kind = item.value("kind").toString();
        const QString name = kind == "GameObject"
            ? item.value("name").toString()
            : item.value("type").toString() + QStringLiteral(" on ") +
                  item.value("objectName").toString();
        lines.push_back(QStringLiteral("• %1 — %2").arg(name, item.value("reason").toString()));
    }
    if (impact.affectedTotal > shown)
        lines.push_back(QStringLiteral("• …and %1 more affected item(s)")
                            .arg(impact.affectedTotal - shown));
    return lines.join('\n');
}

QString ScopedFingerprint(const QString& operation, const QString& targetId,
                          const ImpactClarification& impact) {
    QJsonObject snapshot{
        {"operation", operation},
        {"target", targetId},
        {"affected", impact.affected},
        {"affectedTotal", impact.affectedTotal}
    };
    const QByteArray hash = QCryptographicHash::hash(
        QJsonDocument(snapshot).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex().left(24);
    return operation + ':' + targetId + ':' + QString::fromLatin1(hash);
}

void AddExternalJointImpacts(ImpactClarification& impact, const QSet<QString>& bodyObjectIds,
                             const QSet<QString>& excludedComponentIds = {}) {
    QSet<QString> seen = excludedComponentIds;
    for (GameObject* object : AllObjects()) {
        if (!object) continue;
        for (Component* component : object->GetAllComponents()) {
            auto* joint = dynamic_cast<Joint*>(component);
            if (!joint || seen.contains(QString::fromStdString(joint->GetID()))) continue;
            if (!bodyObjectIds.contains(QString::fromStdString(joint->GetConnectedBody()))) continue;
            seen.insert(QString::fromStdString(joint->GetID()));
            AppendAffected(impact, ComponentItem(
                joint, QStringLiteral("removed"),
                QStringLiteral("its connected Rigidbody is being destroyed")));
        }
    }
}

} // namespace

ImpactClarification AnalyzeComponentRemoval(GameObject* owner, Component* component) {
    ImpactClarification impact;
    const QString type = QString::fromStdString(component->GetTypeName());
    const QString objectName = QString::fromStdString(owner->GetName());
    const QString componentId = QString::fromStdString(component->GetID());

    impact.request.title = QStringLiteral("Confirm component change");
    impact.request.question = QStringLiteral("What should RockEngine do with %1 on %2?")
                                  .arg(type, objectName);

    // The target is always included. Additional rows describe cascaded destruction
    // or behavior changes that are easy to miss from the original request.
    AppendAffected(impact, ComponentItem(
        component, QStringLiteral("removed"), QStringLiteral("the requested component")));

    QSet<QString> alreadyAffected{componentId};
    if (dynamic_cast<RigidBody*>(component)) {
        for (Component* sibling : owner->GetAllComponents()) {
            if (!sibling || sibling == component) continue;
            if (!dynamic_cast<Collider*>(sibling) && !dynamic_cast<Joint*>(sibling)) continue;
            alreadyAffected.insert(QString::fromStdString(sibling->GetID()));
            AppendAffected(impact, ComponentItem(
                sibling, QStringLiteral("removed"),
                dynamic_cast<Collider*>(sibling)
                    ? QStringLiteral("its Box2D shape belongs to this Rigidbody")
                    : QStringLiteral("the joint is tied to this Rigidbody's lifetime")));
        }
        AddExternalJointImpacts(
            impact, QSet<QString>{QString::fromStdString(owner->GetID())}, alreadyAffected);
    } else if (auto* sprite = dynamic_cast<SpriteRenderer*>(component)) {
        Q_UNUSED(sprite);
        if (auto* caster = owner->GetComponent<ShadowCaster>()) {
            if (caster->GetShape() == ShadowCaster::Shape::SpriteBounds ||
                caster->GetShape() == ShadowCaster::Shape::SpriteAlpha) {
                AppendAffected(impact, ComponentItem(
                    caster, QStringLiteral("behavior_change"),
                    QStringLiteral("its shadow silhouette currently comes from this sprite")));
            }
        }
        if (auto* animator = owner->GetComponent<Animator>()) {
            AppendAffected(impact, ComponentItem(
                animator, QStringLiteral("behavior_change"),
                QStringLiteral("animation can no longer display sprite frames")));
        }
    } else if (dynamic_cast<Collider*>(component)) {
        int otherColliders = 0;
        for (Component* sibling : owner->GetAllComponents())
            if (sibling != component && dynamic_cast<Collider*>(sibling)) ++otherColliders;
        if (auto* caster = owner->GetComponent<ShadowCaster>();
            caster && caster->GetShape() == ShadowCaster::Shape::FromCollider &&
            otherColliders == 0) {
            AppendAffected(impact, ComponentItem(
                caster, QStringLiteral("behavior_change"),
                QStringLiteral("it will have no Collider silhouette to cast shadows from")));
        }
    } else if (auto* camera = dynamic_cast<Camera*>(component)) {
        if (Camera::GetMain() == camera) {
            AppendAffected(impact, ComponentItem(
                camera, QStringLiteral("behavior_change"),
                QStringLiteral("this is currently the main rendering camera")));
        }
    } else if (auto* listener = dynamic_cast<AudioListener*>(component)) {
        if (AudioListener::GetMain() == listener) {
            AppendAffected(impact, ComponentItem(
                listener, QStringLiteral("behavior_change"),
                QStringLiteral("spatial audio will fall back to the main Camera or another listener")));
        }
    }

    const int consequential = std::max(0, impact.affectedTotal - 1);
    impact.request.context = consequential > 0
        ? QStringLiteral("Removing this component affects %1 additional item(s):\n%2")
              .arg(consequential).arg(AffectedSummary(impact))
        : QStringLiteral("This removes the component from the object and is not currently undoable.\n%1")
              .arg(AffectedSummary(impact));
    impact.request.options = {
        {QStringLiteral("remove"), QStringLiteral("Remove %1").arg(type),
         consequential > 0
             ? QStringLiteral("Remove it and accept the listed cascading or behavioral effects.")
             : QStringLiteral("Remove only this component.")},
        {QStringLiteral("disable"), QStringLiteral("Disable it instead"),
         QStringLiteral("Keep the component and its authored data, but turn it off.")},
        {QStringLiteral("inspect"), QStringLiteral("Inspect affected items first"),
         QStringLiteral("Make no changes and return the affected IDs to the assistant.")},
        {QStringLiteral("cancel"), QStringLiteral("Cancel"),
         QStringLiteral("Keep the component and make no changes.")}
    };
    impact.request.allowMultiple = false;
    impact.request.scope = ScopedFingerprint(QStringLiteral("remove_component"), componentId, impact);
    return impact;
}

ImpactClarification AnalyzeObjectDestruction(GameObject* object) {
    ImpactClarification impact;
    std::vector<GameObject*> subtree;
    object->recurseTopDown([&subtree](GameObject* current) { subtree.push_back(current); });
    QSet<QString> subtreeIds;
    QSet<QString> componentIds;
    int componentCount = 0;
    for (GameObject* current : subtree) {
        subtreeIds.insert(QString::fromStdString(current->GetID()));
        componentCount += static_cast<int>(current->GetAllComponents().size());
        for (Component* component : current->GetAllComponents())
            componentIds.insert(QString::fromStdString(component->GetID()));
        AppendAffected(impact, ObjectItem(
            current, QStringLiteral("removed"),
            current == object ? QStringLiteral("the requested object")
                              : QStringLiteral("a descendant of the requested object")));
    }
    AddExternalJointImpacts(impact, subtreeIds, componentIds);

    const QString objectName = QString::fromStdString(object->GetName());
    const int descendants = std::max(0, static_cast<int>(subtree.size()) - 1);
    const int externalEffects = std::max(0, impact.affectedTotal - static_cast<int>(subtree.size()));
    impact.request.title = QStringLiteral("Confirm object destruction");
    impact.request.question = QStringLiteral("What should RockEngine do with %1?").arg(objectName);
    impact.request.context = QStringLiteral(
        "Destroying this object removes %1 descendant object(s) and %2 component(s). "
        "%3 external dependent joint(s) will also be removed.\n%4")
        .arg(descendants).arg(componentCount).arg(externalEffects).arg(AffectedSummary(impact));
    impact.request.options = {
        {QStringLiteral("destroy"),
         descendants > 0 ? QStringLiteral("Destroy object and descendants")
                         : QStringLiteral("Destroy object"),
         QStringLiteral("Permanently remove the object hierarchy and accept all listed effects.")},
        {QStringLiteral("deactivate"), QStringLiteral("Deactivate it instead"),
         QStringLiteral("Keep the hierarchy and authored data, but make it inactive.")},
        {QStringLiteral("inspect"), QStringLiteral("Inspect affected items first"),
         QStringLiteral("Make no changes and return the affected IDs to the assistant.")},
        {QStringLiteral("cancel"), QStringLiteral("Cancel"),
         QStringLiteral("Keep the object hierarchy and make no changes.")}
    };
    impact.request.allowMultiple = false;
    impact.request.scope = ScopedFingerprint(
        QStringLiteral("destroy_object"), QString::fromStdString(object->GetID()), impact);
    return impact;
}

} // namespace mcp
