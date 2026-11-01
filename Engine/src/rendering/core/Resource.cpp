#include "engine/rendering/core/Resource.hpp"
#include "engine/rendering/core/AssetMetaService.hpp"
#include <filesystem>
#include <algorithm>
#include <cctype>



void Resource::Validate(){}

void Resource::Deserialize(const YAML::Node& node){
    Serializable::Deserialize(node);
}

std::string Resource::GetName() const {
    if (m_filePath.empty()) return {};
    std::filesystem::path file(m_filePath);
    std::filesystem::path stem = file.stem();
    const std::string sourceExt = stem.extension().string();
    if (AssetMetaService::MetaExtensionFor(sourceExt) == file.extension().string())
        stem = stem.stem();
    return stem.string();
}

void Resource::SetFilePath(const std::string& path){
    if (m_filePath == path) return;
    const std::string oldName = GetName();
    m_filePath = path;
    Notify(FILE_PATH_CHANGED_EVENT);
    if (GetName() != oldName) Notify(NAME_CHANGED_EVENT);
}

std::string Resource::RemapPath(const std::string& path, const std::string& oldPath,
                                const std::string& newPath, bool directory) {
    auto normalized = [](std::string value) {
        std::replace(value.begin(), value.end(), '\\', '/');
#ifdef _WIN32
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
        return value;
    };
    const std::string candidate = normalized(path);
    const std::string prefix = normalized(oldPath);
    if (candidate == prefix) return newPath;
    if (directory && candidate.size() > prefix.size() &&
        candidate.compare(0, prefix.size(), prefix) == 0 && candidate[prefix.size()] == '/')
        return newPath + path.substr(oldPath.size());
    return path;
}
