#pragma once

#include "engine/utils/IVisitor.hpp"
#include <QGridLayout>
#include <QLabel>
#include <optional>
#include <vector>
#include <utility>
#include "utils/ProperyFactory.hpp"
#include "engine/core/Observable.hpp"

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


        QWidget* GetContent(){ return content;}
        bool HasContent(){return content;}

        // Subscriptions this visitor registered on the inspected object(s) while
        // building property widgets. The owner (InspectorGui) must Unsubscribe these
        // when the inspector is rebuilt — otherwise they accumulate on long-lived
        // objects/assets every time they're inspected.
        const std::vector<std::pair<Observable*, int>>& GetSubscriptions() const {
            return m_subscriptions;
        }

    protected:

        template<typename T>
        void BindProperty(Observable* instance, const std::string& label,
                          std::function<T()> getter, std::function<void(T)> setter,
                          Observable::Event event_id, const Properties::PropDesc& desc,
                          std::optional<T> initialValue = std::nullopt);

    private:
        void AddRow(const std::string& text, QWidget* widget);
        void AddFullRow(QWidget* widget);

        QGridLayout* layout = nullptr;
        int gridRow = 0;
        QWidget* content = nullptr;
        std::vector<std::pair<Observable*, int>> m_subscriptions;
};

template<typename T>
void InspectorVisitor::BindProperty(Observable* instance, const std::string& label,
                                    std::function<T()> getter, std::function<void(T)> setter,
                                    Observable::Event event_id, const Properties::PropDesc& desc,
                                    std::optional<T> initialValue)
{
    auto* pw = PropertyFactory::Create<T>(desc);

    pw->onChanged = setter;
    if (initialValue.has_value()) {
        pw->SetValue(*initialValue);
    } else {
        pw->SetValue(getter());
    }

    int subId = instance->Subscribe([pw, getter]() {
        if (!pw->IsValid()) return false;
        pw->SetValue(getter());
        return true;
    }, event_id);
    m_subscriptions.emplace_back(instance, subId);

    AddRow(label, pw->GetWidget());
}
