#pragma once
#include "engine/rendering/core/Shader.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/Resource.hpp"
#include <unordered_map>
#include <string>
#include <glm/glm.hpp>
#include "yaml-cpp/yaml.h"

class Material : public Resource
{
public:
    static inline const Event NAME_CHANGED_EVENT = Material::CreateEvent();
    static inline const Event SHADER_CHANGED_EVENT = Material::CreateEvent();

    Material() = default;

    YAML::Node Serialize() override { return YAML::Node(); }
    void Deserialize(const YAML::Node& node) override;
    void Awake() override;

    std::string GetTypeName() override {return "Material";};
    
    void SetName(std::string& n);
    std::string GetName() {return name;};


    void SetShader(std::string& id);
    Shader* GetShader();
    void Validate();
    void SetFloat(const std::string& name, float value){floatUniforms[name] = value;}
    void SetVec2(const std::string& name, const glm::vec2& value){vec2Uniforms[name] = value;}
    void SetVec3(const std::string& name, const glm::vec3& value){vec3Uniforms[name] = value;}
    void SetVec4(const std::string& name, const glm::vec4& value){vec4Uniforms[name] = value;}
    void SetTexture(const std::string& name, const std::string& tex_id){texUniforms[name] = tex_id;}
    void ApplyUniforms(); 

private:

    std::string name;
    std::string shader_id;  

    std::unordered_map<std::string, float> floatUniforms;
    std::unordered_map<std::string, glm::vec2> vec2Uniforms;
    std::unordered_map<std::string, glm::vec3> vec3Uniforms;
    std::unordered_map<std::string, glm::vec4> vec4Uniforms;
    std::unordered_map<std::string, glm::mat4> mat4Uniforms;
    std::unordered_map<std::string, std::string> texUniforms;
    std::vector<std::string> temp_ids;
};
