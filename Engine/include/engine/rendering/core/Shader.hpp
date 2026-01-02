#pragma once
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "engine/serialization/Serializable.hpp"

class Shader : public Serializable{
public:
    Shader() = default;
    static Shader* LoadFromPath(const std::string& vert_path, const std::string& frag_path, const std::string& name = "Unnamed Shader");
    ~Shader();

    void Bind() const;
    void Unbind() const;
    void Init(){};
    YAML::Node Serialize() override { return YAML::Node(); }

    void Deserialize(const YAML::Node& node) override;
    void PostDeserialize() override;
    std::string GetTypeName() {return "Shader";};
    std::string GetName() {return name;};

    // Uniform setters
    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetVec2(const std::string& name, const glm::vec2& value) const;
    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetVec4(const std::string& name, const glm::vec4& value) const;
    void SetMat4(const std::string& name, const glm::mat4& value) const;

    GLuint GetProgramID() const { return program_id; }

private:
    std::string name;
    std::string vert_path; 
    std::string frag_path; 

    std::string vert_src; 
    std::string frag_src; 

    GLuint program_id = 0;
    GLuint CompileShader(GLenum type, const std::string& source);
    GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader);
};
