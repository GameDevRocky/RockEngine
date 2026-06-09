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
inline PropertyWidget<glm::vec3>* PropertyFactory::Create<glm::vec3>(const Properties::PropDesc& desc) {
    return new Vec3PropertyWidget(desc);
}

template<>
inline PropertyWidget<bool>* PropertyFactory::Create<bool>(const Properties::PropDesc& desc) {
    return new BoolPropertyWidget(desc);
}

template<>
inline PropertyWidget<glm::vec4>* PropertyFactory::Create<glm::vec4>(const Properties::PropDesc& desc) {
    if (desc.tag == Properties::Tags::VECTOR4)
        return new Vec4PropertyWidget(desc);
    return new Vec4ColorPropertyWidget(desc);
}
template<>
inline PropertyWidget<std::string>* PropertyFactory::Create<std::string>(const Properties::PropDesc& desc) {
    if (desc.refType == Properties::Tags::OBJECT_REF) {
        if (desc.tag == Properties::Tags::TEXTURE ||
            desc.tag == Properties::Tags::SPRITE   ||
            desc.tag == Properties::Tags::MATERIAL)
            return new AssetPreviewPropertyWidget(desc);
        return new ObjectRefPropertyWidget(desc);
    }
    return new StringPropertyWidget(desc);
}

template<>
inline PropertyWidget<int>* PropertyFactory::Create<int>(const Properties::PropDesc& desc) {
    return new DropdownPropertyWidget(desc);
}