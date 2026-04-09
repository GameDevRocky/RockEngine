#pragma once

#include "engine/utils/IVisitor.hpp"
#include <QFormLayout>
#include "engine/serialization/Serializable.hpp"


class InspectorVisitor : public IVisitor{

    public:
        InspectorVisitor();
        void Visit(GameObject* obj) override;
        void Visit(Transform* transform) override;
        QWidget* GetContent(){ return content;}
        bool HasContent(){return containsContent;}
    protected:
        void BindProperty(Serializable* instance, const std::string& label, std::function<glm::vec2()> getter,
            std::function<void(glm::vec2)> setter, Observable::Event event_id, PropDesc desc
        );
        void BindProperty(Serializable* instance, const std::string& text, std::function<float()> getter,
            std::function<void(float)> setter, Observable::Event event_id, PropDesc desc);

    private:
        QVBoxLayout* layout = nullptr;
        QWidget* content = nullptr;
        bool containsContent = false;

};