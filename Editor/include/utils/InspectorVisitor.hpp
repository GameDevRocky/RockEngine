#pragma once

#include "engine/utils/IVisitor.hpp"
#include <QGridLayout>
#include <QLabel>
#include <QString>
#include <optional>
#include <vector>
#include <utility>
#include <type_traits>
#include <functional>
#include <memory>
#include "utils/ProperyFactory.hpp"
#include "engine/core/Observable.hpp"
#include "engine/core/UndoSystem.hpp"
#include "engine/commands/PropertyCommand.hpp"
#include "engine/serialization/Registry.hpp"
#include "Engine.hpp"

class ScriptComponent;
class Sprite;
class Material;
class Texture2D;
class Shader;

class InspectorVisitor : public IVisitor{

    public:
        InspectorVisitor();
        void Visit(GameObject* obj) override;
        void Visit(Transform* transform) override;
        void Visit(SpriteRenderer* spriteRenderer) override;
        void Visit(Collider* collider) override;
        void Visit(BoxCollider* boxCollider) override;
        void Visit(CircleCollider* circleCollider) override;
        void Visit(CapsuleCollider* capsuleCollider) override;
        void Visit(RigidBody* rigidBody) override;
        void Visit(ScriptComponent* scriptComponent) override;
        void Visit(Sprite* sprite) override;
        void Visit(Material* material) override;
        void Visit(Texture2D* texture) override;
        void Visit(Shader* shader) override;
        void Visit(Camera* camera) override;
        void Visit(Animator* animator) override;


        QWidget* GetContent(){ return content;}
        bool HasContent(){return content;}

        // Subscriptions this visitor registered on the inspected object(s) while
        // building property widgets. The owner (InspectorGui) must Unsubscribe these
        // when the inspector is rebuilt — otherwise they accumulate on long-lived
        // objects/assets every time they're inspected.
        const std::vector<std::pair<Observable*, int>>& GetSubscriptions() const {
            return m_subscriptions;
        }

        // Deferred value-refresh closures, one per bound property. A property's
        // change-event subscription only flags it dirty (near-zero cost); the
        // owner (InspectorGui) calls these on a timer to actually push the new
        // value into the widget, so the inspector refresh rate is decoupled from
        // the engine frame rate. Cleared/rebuilt with the inspector.
        const std::vector<std::function<void()>>& GetRefreshers() const {
            return m_refreshers;
        }

    protected:

        // Builds one labelled property row and wires it in both directions:
        // widget edit -> setter, and object event -> widget refresh.
        //
        // `setter` takes its target as an argument and MUST NOT capture a
        // Serializable pointer. The undo stack stores it verbatim and replays it
        // long after this inspector was torn down — possibly after the object was
        // deleted and restored, at which point only the id is still meaningful.
        // Getters may capture freely; they only run while the widget is alive.
        //
        // TargetT is deduced from `instance` alone, so call sites still read
        // BindProperty<glm::vec2>(transform, ...). The setter is wrapped in
        // type_identity_t to make it a non-deduced context — otherwise TargetT
        // would also have to deduce from the setter argument, which fails for a
        // lambda and takes the whole call with it.
        template<typename T, typename TargetT>
        void BindProperty(TargetT* instance, const std::string& label,
                          std::function<T()> getter,
                          std::type_identity_t<std::function<void(TargetT*, const T&)>> setter,
                          Observable::Event event_id, const Properties::PropDesc& desc,
                          std::optional<T> initialValue = std::nullopt);

    private:
        void AddRow(const std::string& text, QWidget* widget);
        void AddFullRow(QWidget* widget);

        QGridLayout* layout = nullptr;
        int gridRow = 0;
        QWidget* content = nullptr;
        std::vector<std::pair<Observable*, int>> m_subscriptions;
        std::vector<std::function<void()>> m_refreshers;
};

template<typename T, typename TargetT>
void InspectorVisitor::BindProperty(TargetT* instance, const std::string& label,
                                    std::function<T()> getter,
                                    std::type_identity_t<std::function<void(TargetT*, const T&)>> setter,
                                    Observable::Event event_id,
                                    const Properties::PropDesc& desc,
                                    std::optional<T> initialValue)
{
    auto* pw = PropertyFactory::Create<T>(desc);

    const std::string targetId = instance->GetID();
    // The label doubles as the merge key: it is already unique per property
    // within an object, which is exactly the granularity coalescing needs, and it
    // saves threading a redundant key argument through every call site.
    const std::string propertyKey = label;
    std::string text = "Change " + label;
    while (!text.empty() && (text.back() == ' ' || text.back() == ':')) text.pop_back();

    pw->onChanged = [instance, targetId, propertyKey, getter, setter, text](T newValue) {
        // Read before applying. Change events carry no old value, and setters
        // early-return when the value is unchanged, so this is the only point
        // where the "before" state is still observable.
        const T oldValue = getter();
        // Also discards the engine->widget echo: an echo carries the value that
        // was just written, so it is a no-op by definition.
        if (oldValue == newValue) return;

        // Always the inspected instance, never a re-resolved one — it is the object
        // this widget was built for, in whichever container is on screen.
        setter(instance, newValue);

        // Record into the ACTIVE container's history, so a play-mode edit is
        // recorded against the runtime world and dies with it, while an editor
        // edit lands on the editor world's stack. UndoSystem::Push itself ignores
        // anything pushed while it is replaying.
        Container* active = Engine::Get()->GetActiveContainer();
        if (!active) return;
        auto* undoSystem = active->FindSystem<UndoSystem>();
        auto* registry = active->FindSystem<Registry>();
        if (!undoSystem || !registry) return;

        // Assets (Sprite/Material/Texture2D/Shader) are not RuntimeObjects and are
        // not registered, so they resolve to null and stay non-undoable by
        // construction — they persist to disk via meta files and reverting memory
        // alone would desync the two.
        if (!registry->Find<TargetT>(targetId)) return;

        undoSystem->Push(std::make_unique<PropertyCommand<TargetT, T>>(
            targetId, propertyKey, oldValue, newValue, setter, text));
    };

    if (initialValue.has_value()) {
        pw->SetValue(*initialValue);
    } else {
        pw->SetValue(getter());
    }

    // The change event only flags the widget dirty (cheap); the actual
    // pw->SetValue(getter()) is deferred to a timer flush in InspectorGui (see
    // GetRefreshers). This keeps a per-frame engine change — e.g. a physics body's
    // position streaming through Transform notifies — from forcing a styled
    // QDoubleSpinBox repaint every frame, which otherwise halves the framerate
    // while a moving object is selected.
    auto dirty = std::make_shared<bool>(false);
    int subId = instance->Subscribe([dirty]() {
        *dirty = true;
        return true;
    }, event_id);
    m_subscriptions.emplace_back(instance, subId);

    m_refreshers.emplace_back([pw, getter, dirty]() {
        if (!*dirty) return;
        *dirty = false;
        if (pw->IsValid()) pw->SetValue(getter());
    });

    AddRow(label, pw->GetWidget());
}
