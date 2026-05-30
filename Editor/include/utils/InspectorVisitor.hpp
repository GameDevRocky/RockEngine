#pragma once

#include "engine/utils/IVisitor.hpp"
#include <QGridLayout>
#include <QLabel>
#include <optional>
#include "utils/ProperyFactory.hpp"
#include "engine/core/Observable.hpp"

class ScriptComponent;

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


        QWidget* GetContent(){ return content;}
        bool HasContent(){return content;}

    protected:

        template<typename T>
        void BindProperty(Observable* instance, const std::string& label,
                          std::function<T()> getter, std::function<void(T)> setter,
                          Observable::Event event_id, const Properties::PropDesc& desc,
                          std::optional<T> initialValue = std::nullopt);

    private:
        void AddRow(const std::string& text, QWidget* widget);

        QGridLayout* layout = nullptr;
        int gridRow = 0;
        QWidget* content = nullptr;
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

    instance->Subscribe([pw, getter]() {
        if (!pw->IsValid()) return false;
        pw->SetValue(getter());
        return true;
    }, event_id);

    AddRow(label, pw->GetWidget());
}
