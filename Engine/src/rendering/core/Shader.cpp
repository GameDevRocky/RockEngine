#include "engine/rendering/core/Shader.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/debug/Console.hpp"
#include <iostream>


Shader::~Shader()
{
    if (program_id)
        glDeleteProgram(program_id);
}


void Shader::PostDeserialize(){
    vert_src = EngineUtils::ReadShader(vert_path);
    frag_src = EngineUtils::ReadShader(frag_path);
    GLuint vertex = CompileShader(GL_VERTEX_SHADER, vert_src);
    GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, frag_src);
    program_id = LinkProgram(vertex, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    Console::Warn(GetID());

}

Shader* Shader::LoadFromPath(const std::string& vert_path, const std::string& frag_path, const std::string& name){
    Shader* shader = new Shader();
    shader->vert_src = EngineUtils::ReadShader(vert_path);
    shader->frag_src = EngineUtils::ReadShader(frag_path);
    GLuint vertex = shader->CompileShader(GL_VERTEX_SHADER, shader->vert_src);
    GLuint fragment = shader->CompileShader(GL_FRAGMENT_SHADER, shader->frag_src);
    shader->program_id = shader->LinkProgram(vertex, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return shader;
}


void Shader::Deserialize(const YAML::Node& node) {
    Serializable::Deserialize(node);
    vert_path = node["vert_path"].as<std::string>();
    frag_path = node["frag_path"].as<std::string>();
    name = node["name"].as<std::string>();
    
}


GLuint Shader::CompileShader(GLenum type, const std::string& source)
{ 
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        std::cerr << "Shader compilation error: " << info << std::endl;
    }
    return shader;
}

GLuint Shader::LinkProgram(GLuint vertexShader, GLuint fragmentShader)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char info[512];
        glGetProgramInfoLog(program, 512, nullptr, info);
        std::cerr << "Shader linking error: " << info << std::endl;
    }
    return program;
}

void Shader::Bind() const { glUseProgram(program_id); }
void Shader::Unbind() const { glUseProgram(0); }

void Shader::SetInt(const std::string& name, int value) const
{
    glUniform1i(glGetUniformLocation(program_id, name.c_str()), value);
}
void Shader::SetFloat(const std::string& name, float value) const
{
    glUniform1f(glGetUniformLocation(program_id, name.c_str()), value);
}
void Shader::SetVec2(const std::string& name, const glm::vec2& value) const
{
    glUniform2fv(glGetUniformLocation(program_id, name.c_str()), 1, &value[0]);
}
void Shader::SetVec3(const std::string& name, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(program_id, name.c_str()), 1, &value[0]);
}
void Shader::SetVec4(const std::string& name, const glm::vec4& value) const
{
    glUniform4fv(glGetUniformLocation(program_id, name.c_str()), 1, &value[0]);
}
void Shader::SetMat4(const std::string& name, const glm::mat4& value) const
{
    glUniformMatrix4fv(glGetUniformLocation(program_id, name.c_str()), 1, GL_FALSE, &value[0][0]);
}
