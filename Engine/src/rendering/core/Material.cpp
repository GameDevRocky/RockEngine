#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/serialization/Serializable.hpp"
#include "engine/debug/Console.hpp"
#include <unordered_set>

// Uniforms the ENGINE writes every draw, which a material must never own.
//
// Validate() otherwise auto-seeds a material entry for every uniform a shader
// declares. For these that produces a permanently-empty, permanently-ignored row
// in the material inspector (the engine's per-draw write always lands after
// ApplyUniforms and wins), which reads as a broken control.
//
// uLitAmount is deliberately absent: seeding it IS the feature -- it gives every
// material a free "how strongly does light affect this" slider.
static const std::unordered_set<std::string>& EngineReservedUniforms()
{
    static const std::unordered_set<std::string> kReserved = {
        "uNormalMap", "uHasNormalMap", "uShadowAtlas"
    };
    return kReserved;
}


YAML::Node Material::Serialize() {
    YAML::Node node;
    node["type"] = GetTypeName();
    node["name"] = GetName();
    node["id"] = GetID();
    node["shader_id"] = shader_id;

    YAML::Node uniforms;
    YAML::Node textures(YAML::NodeType::Map);
    for (auto& kv : texUniforms) textures[kv.first] = kv.second;
    uniforms["textures"] = textures;

    YAML::Node floats(YAML::NodeType::Map);
    for (auto& kv : floatUniforms) floats[kv.first] = kv.second;
    uniforms["floats"] = floats;

    YAML::Node vec2s(YAML::NodeType::Map);
    for (auto& kv : vec2Uniforms) {
        YAML::Node v; v.push_back(kv.second.x); v.push_back(kv.second.y);
        v.SetStyle(YAML::EmitterStyle::Flow);
        vec2s[kv.first] = v;
    }
    uniforms["vec2"] = vec2s;

    YAML::Node vec3s(YAML::NodeType::Map);
    for (auto& kv : vec3Uniforms) {
        YAML::Node v; v.push_back(kv.second.x); v.push_back(kv.second.y); v.push_back(kv.second.z);
        v.SetStyle(YAML::EmitterStyle::Flow);
        vec3s[kv.first] = v;
    }
    uniforms["vec3"] = vec3s;

    YAML::Node vec4s(YAML::NodeType::Map);
    for (auto& kv : vec4Uniforms) {
        YAML::Node v; v.push_back(kv.second.x); v.push_back(kv.second.y); v.push_back(kv.second.z); v.push_back(kv.second.w);
        v.SetStyle(YAML::EmitterStyle::Flow);
        vec4s[kv.first] = v;
    }
    uniforms["vec4"] = vec4s;

    node["uniforms"] = uniforms;
    return node;
}

void Material::Deserialize(const YAML::Node& node) {
    Resource::Deserialize(node);
    if (node["shader_id"]) shader_id = node["shader_id"].as<std::string>();

    const YAML::Node& uniforms = node["uniforms"];
    if (!uniforms || !uniforms.IsMap()) return;

    if (uniforms["textures"] && uniforms["textures"].IsMap()) {
        for (auto pair : uniforms["textures"]) {
            SetTexture(pair.first.as<std::string>(), pair.second.as<std::string>());
        }
    }

    if (uniforms["floats"] && uniforms["floats"].IsMap()) {
        for (auto pair : uniforms["floats"]) {
            SetFloat(pair.first.as<std::string>(), pair.second.as<float>());
        }
    }

    if (uniforms["vec2"] && uniforms["vec2"].IsMap()) {
        for (auto pair : uniforms["vec2"]) {
            YAML::Node v = pair.second;
            SetVec2(pair.first.as<std::string>(), glm::vec2(v[0].as<float>(), v[1].as<float>()));
        }
    }
    if (uniforms["vec3"] && uniforms["vec3"].IsMap()) {
        for (auto pair : uniforms["vec3"]) {
            YAML::Node v = pair.second;
            SetVec3(pair.first.as<std::string>(), glm::vec3(v[0].as<float>(), v[1].as<float>(), v[2].as<float>() ));
        }
    }
    if (uniforms["vec4"] && uniforms["vec4"].IsMap()) {
        for (auto pair : uniforms["vec4"]) {
            YAML::Node v = pair.second;
            SetVec4(pair.first.as<std::string>(), glm::vec4(v[0].as<float>(), v[1].as<float>(), v[2].as<float>(), v[2].as<float>() ));
        }
    }
}


void Material::Awake(){
    SetShader(shader_id);
    Shader* shader = GetShader();
    if (!shader) {
        std::cout << "Shader not Gotten" << std::endl;
        return;
    }
}

void Material::Validate() {
    Shader* shader = GetShader();
    if (!shader) return;

    auto& shaderUniforms = shader->GetActiveUniforms();

    const auto& reserved = EngineReservedUniforms();

    auto prune = [&](auto& map) {
        for (auto it = map.begin(); it != map.end(); ) {
            // Reserved names are dropped even when the shader declares them, so a
            // material saved before this list existed sheds them on next load.
            if (reserved.count(it->first) ||
                shaderUniforms.find(it->first) == shaderUniforms.end()) {
                Console::Alert("Material: Pruning unused uniform [" + it->first + "]");
                it = map.erase(it);
            } else {
                ++it;
            }
        }
    };

    prune(floatUniforms);
    prune(vec2Uniforms);
    prune(vec3Uniforms);
    prune(vec4Uniforms);
    prune(texUniforms);

    // Seed uniforms the shader declares that the material doesn't have yet,
    // using the GLSL initializer values captured at link time as defaults.
    // emplace is a no-op if the key already exists, so user values survive.
    const auto& floatDefs = shader->GetFloatDefaults();
    const auto& vec2Defs  = shader->GetVec2Defaults();
    const auto& vec3Defs  = shader->GetVec3Defaults();
    const auto& vec4Defs  = shader->GetVec4Defaults();

    for (const auto& [uname, info] : shaderUniforms) {
        if (reserved.count(uname)) continue;
        switch (info.type) {
            case GL_FLOAT: {
                auto it = floatDefs.find(uname);
                floatUniforms.emplace(uname, it != floatDefs.end() ? it->second : 0.f);
                break;
            }
            case GL_FLOAT_VEC2: {
                auto it = vec2Defs.find(uname);
                vec2Uniforms.emplace(uname, it != vec2Defs.end() ? it->second : glm::vec2(0.f));
                break;
            }
            case GL_FLOAT_VEC3: {
                auto it = vec3Defs.find(uname);
                vec3Uniforms.emplace(uname, it != vec3Defs.end() ? it->second : glm::vec3(0.f));
                break;
            }
            case GL_FLOAT_VEC4: {
                auto it = vec4Defs.find(uname);
                vec4Uniforms.emplace(uname, it != vec4Defs.end() ? it->second : glm::vec4(0.f));
                break;
            }
            case GL_SAMPLER_2D:
                texUniforms.emplace(uname, std::string{});
                break;
            default:
                break;
        }
    }
}

void Material::SetShader(std::string& id){
    Shader* shader = AssetManager::Get().GetShader(id);
    if (shader){
        shader_id = shader->GetID();
        Validate();
        Notify(SHADER_CHANGED_EVENT);
        return;
    }
    
}

Shader* Material::GetShader(){
    Shader* shader = AssetManager::Get().GetShader(shader_id);
    if (shader) return shader;
    return nullptr;
}

void Material::ApplyUniforms(){
    Shader* shader = GetShader();
    if (!shader) return;
        for (auto& kv : floatUniforms)
            shader->SetFloat(kv.first, kv.second);

        for (auto& kv : vec2Uniforms)
            shader->SetVec2(kv.first, kv.second);

        for (auto& kv : vec3Uniforms)
            shader->SetVec3(kv.first, kv.second);

        for (auto& kv : vec4Uniforms)
            shader->SetVec4(kv.first, kv.second);

        int textureSlot = 0;
        for (auto& kv : texUniforms)
        {
            Texture2D* tex = AssetManager::Get().GetTexture(kv.second);
            if (tex) {
                tex->Bind(textureSlot); 
                shader->SetTexture(kv.first, textureSlot);
                textureSlot++;
            }
        }
    
}

float Material::GetFloat(const std::string& name) const {
    auto it = floatUniforms.find(name);
    return it != floatUniforms.end() ? it->second : 0.f;
}
glm::vec2 Material::GetVec2(const std::string& name) const {
    auto it = vec2Uniforms.find(name);
    return it != vec2Uniforms.end() ? it->second : glm::vec2(0.f);
}
glm::vec3 Material::GetVec3(const std::string& name) const {
    auto it = vec3Uniforms.find(name);
    return it != vec3Uniforms.end() ? it->second : glm::vec3(0.f);
}
glm::vec4 Material::GetVec4(const std::string& name) const {
    auto it = vec4Uniforms.find(name);
    return it != vec4Uniforms.end() ? it->second : glm::vec4(0.f);
}
std::string Material::GetTexUniform(const std::string& name) const {
    auto it = texUniforms.find(name);
    return it != texUniforms.end() ? it->second : "";
}

void Material::SetFloat(const std::string& name, float value) {
    floatUniforms[name] = value;
    Notify(UNIFORM_CHANGED_EVENT);
}

void Material::SetVec2(const std::string& name, const glm::vec2& value) {
    vec2Uniforms[name] = value;
    Notify(UNIFORM_CHANGED_EVENT);
}

void Material::SetVec3(const std::string& name, const glm::vec3& value) {
    vec3Uniforms[name] = value;
    Notify(UNIFORM_CHANGED_EVENT);
}

void Material::SetVec4(const std::string& name, const glm::vec4& value) {
    vec4Uniforms[name] = value;
    Notify(UNIFORM_CHANGED_EVENT);
}

void Material::SetTexture(const std::string& name, const std::string& tex_id) {
    texUniforms[name] = tex_id;
    Notify(UNIFORM_CHANGED_EVENT);
}

void Material::Accept(IVisitor* v) { v->Visit(this); }
