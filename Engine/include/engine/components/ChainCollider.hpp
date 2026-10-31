#pragma once

#include "engine/components/Collider.hpp"
#include <vector>

// An editable, ordered, one-sided outline. Points are local to the collider
// center. Closed paths use a Box2D loop; open paths get internal ghost points.
class ChainCollider : public Collider {
public:
    static inline const Event POINTS_CHANGED_EVENT = Collider::CreateEvent();
    static inline const Event LOOP_CHANGED_EVENT = Collider::CreateEvent();
    static inline const Event SIDE_CHANGED_EVENT = Collider::CreateEvent();
    static inline const Event ALPHA_THRESHOLD_CHANGED_EVENT = Collider::CreateEvent();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;
    void Init() override;
    void Awake() override;
    void CreateShape() override;
    void OnEnabled() override;
    void OnDisabled() override;
    void SetFriction(float value) override;
    void SetBounciness(float value) override;
    void SetIsSensor(bool value) override;
    void Shutdown() override;
    ChainCollider* Copy() override;
    void Accept(IVisitor* visitor) override;

    std::string GetTypeName() const override { return "ChainCollider"; }

    const std::vector<glm::vec2>& GetPoints() const { return points; }
    void SetPoints(std::vector<glm::vec2> value);
    bool SetPoint(std::size_t index, glm::vec2 value);
    bool InsertPoint(std::size_t index, glm::vec2 value);
    bool RemovePoint(std::size_t index);
    void AppendPoint(glm::vec2 value);
    void PrependPoint(glm::vec2 value);
    bool GetIsLoop() const { return isLoop; }
    void SetIsLoop(bool value);
    bool GetReverseSide() const { return reverseSide; }
    void SetReverseSide(bool value);
    float GetAlphaThreshold() const { return alphaThreshold; }
    void SetAlphaThreshold(float value);
    bool HasValidGeometry() const;
    bool GenerateFromSprite();
    bool GenerateFromSprite(float threshold);

private:
    std::vector<glm::vec2> points{{-16.0f, -16.0f}, {16.0f, -16.0f},
                                  {16.0f, 16.0f}, {-16.0f, 16.0f}};
    bool isLoop = true;
    bool reverseSide = false;
    float alphaThreshold = 0.1f;
    b2ChainId chainId = b2_nullChainId;
};
