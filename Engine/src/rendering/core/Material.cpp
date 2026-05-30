#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/SharedResources.hpp"
#include "engine/serialization/Serializable.hpp"
#include "engine/debug/Console.hpp"


void Material::Deserialize(const YAML::Node& node) {
    Serializable::Deserialize(node);
    if (node["name"]) name = node["name"].as<std::string>();
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

    auto prune = [&](auto& map, GLenum expectedType1, GLenum expectedType2 = 0) {
        for (auto it = map.begin(); it != map.end(); ) {
            auto search = shaderUniforms.find(it->first);
            if (search == shaderUniforms.end()) {
                Console::Alert("Material: Pruning unused uniform [" + it->first + "]");
                it = map.erase(it);
            } else {
 
                ++it;
            }
        }
    };

    prune(floatUniforms, GL_FLOAT);
    prune(vec2Uniforms, GL_FLOAT_VEC2);
    prune(vec3Uniforms, GL_FLOAT_VEC3);
    prune(vec4Uniforms, GL_FLOAT_VEC4);
    prune(texUniforms,   GL_SAMPLER_2D);
}


void Material::SetName(std::string& name){
    this->name = name; 
    Notify(NAME_CHANGED_EVENT);
}
void Material::SetShader(std::string& id){
    Shader* shader = SharedResources::Get().GetShader(id);
    if (shader){
        shader_id = shader->GetID();
        Validate();
        Notify(SHADER_CHANGED_EVENT);
        return;
    }
    
}

Shader* Material::GetShader(){
    Shader* shader = SharedResources::Get().GetShader(shader_id);
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
            Texture2D* tex = SharedResources::Get().GetTexture(kv.second);
            if (tex) {
                tex->Bind(textureSlot); 
                shader->SetTexture(kv.first, textureSlot);
                textureSlot++;
            }
        }
    
}
