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

    void SetName(std::string name);
    std::string GetName() {return name;}

    void SetFilePath(const std::string& path);
    const std::string& GetFilePath() const { return m_filePath; }

protected:
    std::string name;
    std::string m_filePath;
};