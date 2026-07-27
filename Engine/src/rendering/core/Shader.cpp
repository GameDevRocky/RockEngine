#include "engine/rendering/core/Shader.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/debug/Console.hpp"
#include "engine/utils/IVisitor.hpp"
#include <iostream>
#include "engine/utils/EngineUtils.hpp"

using namespace EngineUtils;

Shader::~Shader()
{
    if (program_id)
    glad_glDeleteProgram(program_id);
}

void Shader::Accept(IVisitor* v) { v->Visit(this); }


YAML::Node Shader::Serialize() {
    YAML::Node node;
    node["type"] = GetTypeName();
    node["id"] = GetID();
    node["name"] = GetName();
    node["source_path"] = ToAssetRelative(source_path);
    return node;
}

void Shader::Deserialize(const YAML::Node& node) {
    Resource::Deserialize(node);
    source_path = GetAssetPath(node["source_path"].as<std::string>());
    auto src = EngineUtils::ParseShaderSource(source_path);
    vert_src = src.vertex;
    frag_src = src.fragment;
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

GLint Shader::GetLocation(const std::string& name) const
{
    auto it = active_uniforms.find(name);
    if (it != active_uniforms.end()) return it->second.location;
    // Not reflected (e.g. optimizer-stripped or queried before ReflectUniforms):
    // resolve once and remember the answer, including -1 misses.
    const GLint loc = glad_glGetUniformLocation(program_id, name.c_str());
    active_uniforms.emplace(name, UniformInfo{ 0, loc });
    return loc;
}

void Shader::SetInt(const std::string& name, int value) const
{
    glad_glUniform1i(GetLocation(name), value);
}
void Shader::SetFloat(const std::string& name, float value) const
{
    glad_glUniform1f(GetLocation(name), value);
}
void Shader::SetVec2(const std::string& name, const glm::vec2& value) const
{
    glad_glUniform2fv(GetLocation(name), 1, &value[0]);
}
void Shader::SetVec3(const std::string& name, const glm::vec3& value) const
{
    glad_glUniform3fv(GetLocation(name), 1, &value[0]);
}
void Shader::SetVec4(const std::string& name, const glm::vec4& value) const
{
    glad_glUniform4fv(GetLocation(name), 1, &value[0]);
}
void Shader::SetMat4(const std::string& name, const glm::mat4& value) const
{
    glad_glUniformMatrix4fv(GetLocation(name), 1, GL_FALSE, &value[0][0]);
}
void Shader::SetTexture(const std::string& name, const GLint tex) const
{
    glad_glUniform1i(GetLocation(name), tex);
}
void Shader::ReflectUniforms() {
    active_uniforms.clear();
    m_floatDefaults.clear();
    m_vec2Defaults.clear();
    m_vec3Defaults.clear();
    m_vec4Defaults.clear();

    GLint count;
    glad_glGetProgramiv(program_id, GL_ACTIVE_UNIFORMS, &count);

    for (GLint i = 0; i < count; i++) {
        char name[256];
        GLenum type;
        GLint size;
        glad_glGetActiveUniform(program_id, i, sizeof(name), nullptr, &size, &type, name);
        GLint loc = glad_glGetUniformLocation(program_id, name);

        // GL_ACTIVE_UNIFORMS also enumerates UNIFORM BLOCK MEMBERS (every
        // uLights[i].posRange of sprite.glsl's LightBlock, and uAmbient with it).
        // Those live in a buffer, not in program state: they have no location,
        // cannot be set with glUniform*, and glGetUniformfv(-1) leaves the
        // destination untouched -- so capturing a "default" for one reads
        // uninitialized stack. Left in, Material::Validate() then seeds a
        // material entry for all ~160 of them, which lands in the .material file
        // and floods the material inspector with garbage rows.
        //
        // Location -1 is exactly the "not settable through this program" test,
        // and it costs nothing for ordinary uniforms.
        if (loc < 0) continue;

        active_uniforms[name] = { type, loc };

        // Read the initializer value from the freshly linked program — this is
        // exactly what the GLSL "= value" default evaluates to.
        switch (type) {
            case GL_FLOAT: {
                float v;
                glad_glGetUniformfv(program_id, loc, &v);
                m_floatDefaults[name] = v;
                break;
            }
            case GL_FLOAT_VEC2: {
                float v[2];
                glad_glGetUniformfv(program_id, loc, v);
                m_vec2Defaults[name] = glm::vec2(v[0], v[1]);
                break;
            }
            case GL_FLOAT_VEC3: {
                float v[3];
                glad_glGetUniformfv(program_id, loc, v);
                m_vec3Defaults[name] = glm::vec3(v[0], v[1], v[2]);
                break;
            }
            case GL_FLOAT_VEC4: {
                float v[4];
                glad_glGetUniformfv(program_id, loc, v);
                m_vec4Defaults[name] = glm::vec4(v[0], v[1], v[2], v[3]);
                break;
            }
            default:
                break;
        }
    }
}