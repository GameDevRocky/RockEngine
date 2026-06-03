#include "engine/rendering/core/Shader.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/debug/Console.hpp"
#include <iostream>
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;

Shader::~Shader()
{
    if (program_id)
    glad_glDeleteProgram(program_id);
}


void Shader::Deserialize(const YAML::Node& node) {
    Resource::Deserialize(node);
    vert_path = GetAssetPath(node["vert_path"].as<std::string>());
    frag_path = GetAssetPath(node["frag_path"].as<std::string>());
    vert_src = EngineUtils::ReadShader(vert_path);
    frag_src = EngineUtils::ReadShader(frag_path);
    GLuint vertex = CompileShader(GL_VERTEX_SHADER, vert_src);
    GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, frag_src);
    program_id = LinkProgram(vertex, fragment);
    glad_glDeleteShader(vertex);
    glad_glDeleteShader(fragment);
    ReflectUniforms();
    
}


GLuint Shader::CompileShader(GLenum type, const std::string& source)
{ 
    GLuint shader = glad_glCreateShader(type);
    const char* src = source.c_str();
    glad_glShaderSource(shader, 1, &src, nullptr);
    glad_glCompileShader(shader);
    
    GLint success;
    glad_glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char info[512];
        glad_glGetShaderInfoLog(shader, 512, nullptr, info);
        std::cerr << "Shader compilation error: " << info << std::endl;
    }
    return shader;
}

GLuint Shader::LinkProgram(GLuint vertexShader, GLuint fragmentShader)
{
    GLuint program = glad_glCreateProgram();
    glad_glAttachShader(program, vertexShader);
    glad_glAttachShader(program, fragmentShader);
    glad_glLinkProgram(program);

    GLint success;
    glad_glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char info[512];
        glad_glGetProgramInfoLog(program, 512, nullptr, info);
        std::cerr << "Shader linking error: " << info << std::endl;
    }
    return program;
}

void Shader::Bind() const { glad_glUseProgram(program_id); }
void Shader::Unbind() const { glad_glUseProgram(0); }

void Shader::SetInt(const std::string& name, int value) const
{
    glad_glUniform1i(glad_glGetUniformLocation(program_id, name.c_str()), value);
}
void Shader::SetFloat(const std::string& name, float value) const
{
    glad_glUniform1f(glad_glGetUniformLocation(program_id, name.c_str()), value);
}
void Shader::SetVec2(const std::string& name, const glm::vec2& value) const
{
    glad_glUniform2fv(glad_glGetUniformLocation(program_id, name.c_str()), 1, &value[0]);
}
void Shader::SetVec3(const std::string& name, const glm::vec3& value) const
{
    glad_glUniform3fv(glad_glGetUniformLocation(program_id, name.c_str()), 1, &value[0]);
}
void Shader::SetVec4(const std::string& name, const glm::vec4& value) const
{
    glad_glUniform4fv(glad_glGetUniformLocation(program_id, name.c_str()), 1, &value[0]);
}
void Shader::SetMat4(const std::string& name, const glm::mat4& value) const
{
    glad_glUniformMatrix4fv(glad_glGetUniformLocation(program_id, name.c_str()), 1, GL_FALSE, &value[0][0]);
}
void Shader::SetTexture(const std::string& name, const GLint tex) const
{
    glad_glUniform1i(glad_glGetUniformLocation(program_id, name.c_str()), tex);
}
void Shader::ReflectUniforms() {
    active_uniforms.clear(); 
    GLint count;
    glad_glGetProgramiv(program_id, GL_ACTIVE_UNIFORMS, &count);

    for (GLint i = 0; i < count; i++) {
        char name[256];
        GLenum type;
        GLint size;
        glad_glGetActiveUniform(program_id, i, sizeof(name), nullptr, &size, &type, name);
        active_uniforms[name] = { type, glad_glGetUniformLocation(program_id, name) };
    }
}