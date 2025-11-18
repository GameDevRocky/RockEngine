#pragma once
#include "engine/rendering/core/Shader.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/serialization/Serializable.hpp"
#include <unordered_map>
#include <string>
#include <glm/glm.hpp>
#include "yaml-cpp/yaml.h"

class Material : public Serializable
{
public:
    Material() = default;

    YAML::Node Serialize() override { return YAML::Node(); }

    void Deserialize(const YAML::Node& node) override;
    void PostDeserialize() override; 
    std::string GetTypeName() override {return "Material";};
    std::string GetName() {return name;};
    void Init();
    void SetShader(Shader* s);
    
    Shader* GetShader() const { return shader; }

    void SetTexture(Texture2D* tex);
    Texture2D* GetTexture() const { return texture; }

    void SetFloat(const std::string& name, float value)
    {
        floatUniforms[name] = value;
    }

    void SetVec2(const std::string& name, const glm::vec2& value)
    {
        vec2Uniforms[name] = value;
    }

    void SetVec3(const std::string& name, const glm::vec3& value)
    {
        vec3Uniforms[name] = value;
    }

    void SetVec4(const std::string& name, const glm::vec4& value)
    {
        vec4Uniforms[name] = value;
    }

    void SetMat4(const std::string& name, const glm::mat4& value)
    {
        mat4Uniforms[name] = value;
    }

    void ApplyUniforms()
    {
        if (!shader) return;

        shader->Bind();

        for (auto& kv : floatUniforms)
            shader->SetFloat(kv.first, kv.second);

        for (auto& kv : vec2Uniforms)
            shader->SetVec2(kv.first, kv.second);

        for (auto& kv : vec3Uniforms)
            shader->SetVec3(kv.first, kv.second);

        for (auto& kv : vec4Uniforms)
            shader->SetVec4(kv.first, kv.second);

        for (auto& kv : mat4Uniforms)
            shader->SetMat4(kv.first, kv.second);
    }

private:

    std::string name;
    glm::vec4 tint;
    bool flipX;
    bool flipY;

    std::string shader_id;
    std::string texture_id;

    Shader* shader = nullptr;
    Texture2D* texture = nullptr;

   
    std::unordered_map<std::string, float> floatUniforms;
    std::unordered_map<std::string, glm::vec2> vec2Uniforms;
    std::unordered_map<std::string, glm::vec3> vec3Uniforms;
    std::unordered_map<std::string, glm::vec4> vec4Uniforms;
    std::unordered_map<std::string, glm::mat4> mat4Uniforms;
};
