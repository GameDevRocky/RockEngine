#pragma once

#include "engine/utils/IVisitor.hpp"
#include <QGridLayout>
#include <QLabel>
#include "utils/ProperyFactory.hpp"
#include "engine/core/Observable.hpp"

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


        QWidget* GetContent(){ return content;}
        bool HasContent(){return containsContent;}

    protected:

        template<typename T>
        void BindProperty(Observable* instance, const std::string& label,
                          std::function<T()> getter, std::function<void(T)> setter,
                          Observable::Event event_id, const Properties::PropDesc& desc);

    private:
        void AddRow(const std::string& text, QWidget* widget);

        QGridLayout* layout = nullptr;
        int gridRow = 0;
        QWidget* content = nullptr;
        bool containsContent = false;
};

template<typename T>
void InspectorVisitor::BindProperty(Observable* instance, const std::string& label,
                                    std::function<T()> getter, std::function<void(T)> setter,
                                    Observable::Event event_id, const Properties::PropDesc& desc)
{
    auto* pw = PropertyFactory::Create<T>(desc);

    pw->onChanged = setter;
    pw->SetValue(getter());

    instance->Subscribe([pw, getter]() {
        if (!pw->IsValid()) return false;
        pw->SetValue(getter());
        return true;
    }, event_id);

    AddRow(label, pw->GetWidget());
}