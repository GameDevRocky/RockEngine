                                                        #pragma once
#include "engine/rendering/core/Shader.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/Resource.hpp"
#include <unordered_map>
#include <string>
#include <variant>
#include <glm/glm.hpp>
#include "yaml-cpp/yaml.h"

using UniformValue = std::variant<float, glm::vec2, glm::vec3, glm::vec4, std::string>;

class Material : public Resource
{
public:
    static inline const Event SHADER_CHANGED_EVENT = Material::CreateEvent();
    static inline const Event UNIFORM_CHANGED_EVENT = Material::CreateEvent();

    Material() = default;

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void Awake() override;

    std::string GetTypeName() const override {return "Material";};


    void SetShader(std::string& id);
    Shader* GetShader();
    void Validate();
    void SetFloat(const std::string& name, float value);
    void SetVec2(const std::string& name, const glm::vec2& value);
    void SetVec3(const std::string& name, const glm::vec3& value);
    void SetVec4(const std::string& name, const glm::vec4& value);
    void SetTexture(const std::string& name, const std::string& tex_id);

    float       GetFloat(const std::string& name) const;
    glm::vec2   GetVec2(const std::string& name) const;
    glm::vec3   GetVec3(const std::string& name) const;
    glm::vec4   GetVec4(const std::string& name) const;
    std::string GetTexUniform(const std::string& name) const;

    const std::unordered_map<std::string, float>&       GetFloatUniforms() const { return floatUniforms; }
    const std::unordered_map<std::string, glm::vec2>&   GetVec2Uniforms()  const { return vec2Uniforms; }
    const std::unordered_map<std::string, glm::vec3>&   GetVec3Uniforms()  const { return vec3Uniforms; }
    const std::unordered_map<std::string, glm::vec4>&   GetVec4Uniforms()  const { return vec4Uniforms; }
    const std::unordered_map<std::string, std::string>& GetTexUniforms()   const { return texUniforms; }

    // Binds every uniform this material owns and returns the first texture unit
    // it did NOT use, so a component's per-instance texture overrides can carry
    // on from there without stomping the material's own samplers.
    int ApplyUniforms();

    void Accept(IVisitor* v) override;

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
