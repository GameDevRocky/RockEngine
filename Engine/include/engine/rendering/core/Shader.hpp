#pragma once
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "engine/serialization/Serializable.hpp"
#include "engine/rendering/core/Resource.hpp"

struct UniformInfo {
    GLenum type;
    GLint location;
};

class Shader : public Resource{
public:
    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    void Bind() const;
    void Unbind() const;

    std::string GetTypeName() {return "Shader";};

    void Accept(IVisitor* v) override;

    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetVec2(const std::string& name, const glm::vec2& value) const;
    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetVec4(const std::string& name, const glm::vec4& value) const;
    void SetMat4(const std::string& name, const glm::mat4& value) const;
    void SetTexture(const std::string& name, const GLint tex) const;
    void ReflectUniforms();

    const std::unordered_map<std::string, UniformInfo>& GetActiveUniforms() const { return active_uniforms; }

    const std::unordered_map<std::string, float>&     GetFloatDefaults() const { return m_floatDefaults; }
    const std::unordered_map<std::string, glm::vec2>& GetVec2Defaults()  const { return m_vec2Defaults; }
    const std::unordered_map<std::string, glm::vec3>& GetVec3Defaults()  const { return m_vec3Defaults; }
    const std::unordered_map<std::string, glm::vec4>& GetVec4Defaults()  const { return m_vec4Defaults; }

    GLuint GetProgramID() const { return program_id; }

    Shader() = default;
    ~Shader();

private:
    std::string name;
    std::string source_path;

    std::string vert_src; 
    std::string frag_src; 

    GLuint program_id = 0;
    GLuint CompileShader(GLenum type, const std::string& source);
    GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader);
    std::unordered_map<std::string, UniformInfo> active_uniforms;
    std::unordered_map<std::string, float>       m_floatDefaults;
    std::unordered_map<std::string, glm::vec2>   m_vec2Defaults;
    std::unordered_map<std::string, glm::vec3>   m_vec3Defaults;
    std::unordered_map<std::string, glm::vec4>   m_vec4Defaults;

};


