#pragma once
#include "engine/serialization/Serializable.hpp"

class Resource : public Serializable{

public:
    static inline const Event NAME_CHANGED_EVENT      = Resource::CreateEvent();
    static inline const Event FILE_PATH_CHANGED_EVENT = Resource::CreateEvent();

    virtual void Init(){}
    virtual void Awake(){}
    virtual void Update(){}
    virtual void Shutdown(){}
    virtual YAML::Node Serialize() override { return {}; }
    virtual void Deserialize(const YAML::Node& node) override;
    virtual void Validate();

    // File-backed resources take their display name from the descriptor filename.
    // Sprites override this because several sprites share one texture descriptor.
    virtual std::string GetName() const;

    void SetFilePath(const std::string& path);
    const std::string& GetFilePath() const { return m_filePath; }
    virtual void RelocateSourcePath(const std::string& oldPath, const std::string& newPath,
                                    bool directory) {}

protected:
    static std::string RemapPath(const std::string& path, const std::string& oldPath,
                                 const std::string& newPath, bool directory);
    std::string m_filePath;
};
