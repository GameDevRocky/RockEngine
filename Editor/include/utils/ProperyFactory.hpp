#pragma once
#include "utils/PropertyWidget.hpp"

class PropertyFactory {
public:
    template<typename T>
    static PropertyWidget<T>* Create(const Properties::PropDesc& desc);
};


template<>
inline PropertyWidget<float>* PropertyFactory::Create<float>(const Properties::PropDesc& desc) {
    return new FloatPropertyWidget(desc);
}

template<>
inline PropertyWidget<glm::vec2>* PropertyFactory::Create<glm::vec2>(const Properties::PropDesc& desc) {
    return new Vec2PropertyWidget(desc);
}

template<>
inline PropertyWidget<bool>* PropertyFactory::Create<bool>(const Properties::PropDesc& desc) {
    return new BoolPropertyWidget(desc);
}

template<>
inline PropertyWidget<glm::vec4>* PropertyFactory::Create<glm::vec4>(const Properties::PropDesc& desc) {
    return new Vec4ColorPropertyWidget(desc);
}
template<>
inline PropertyWidget<std::string>* PropertyFactory::Create<std::string>(const Properties::PropDesc& desc) {
    if (desc.refType == Properties::Tags::OBJECT_REF)
        return new ObjectRefPropertyWidget(desc);
    return new StringPropertyWidget(desc);
}