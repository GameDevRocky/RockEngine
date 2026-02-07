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
    void Deserialize(const YAML::Node& node) override;

    void Bind() const;
    void Unbind() const;

    std::string GetTypeName() {return "Shader";};
    std::string GetName() {return name;};

    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetVec2(const std::string& name, const glm::vec2& value) const;
    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetVec4(const std::string& name, const glm::vec4& value) const;
    void SetMat4(const std::string& name, const glm::mat4& value) const;
    void SetTexture(const std::string& name, const GLint tex) const;
    void ReflectUniforms();

    const std::unordered_map<std::string, UniformInfo>& GetActiveUniforms() const { return active_uniforms; }

    GLuint GetProgramID() const { return program_id; }

    Shader() = default;
    ~Shader();

private:
    std::string name;
    std::string vert_path; 
    std::string frag_path; 

    std::string vert_src; 
    std::string frag_src; 

    GLuint program_id = 0;
    GLuint CompileShader(GLenum type, const std::string& source);
    GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader);
    std::unordered_map<std::string, UniformInfo> active_uniforms;

};


