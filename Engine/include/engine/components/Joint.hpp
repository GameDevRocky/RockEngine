#pragma once
#include "engine/components/Component.hpp"
#include "glm/glm.hpp"
#include "box2d/box2d.h"
#include "yaml-cpp/yaml.h"

class RigidBody;
class PhysicsSystem;

// Base for every Box2D joint component. A joint constrains two bodies: the
// RigidBody on this GameObject (body A) and the RigidBody on the GameObject named
// by connectedBodyGameObjectId (body B).
//
// Lifetime rule -- the whole reason this class carries its own subscriptions:
// Box2D's b2DestroyBody destroys every joint attached to that body, so if EITHER
// body dies the b2JointId dies with it. This component must follow. Init/PostInit
// each subscribe to one body's SHUTDOWN_EVENT and remove this component when it
// fires. This is the same tie-your-lifetime-to-the-body pattern Collider uses,
// just doubled.
//
// Phase split (mirrors Collider/BoxCollider): resolving components is safe in
// Init/PostInit, but touching a b2BodyId is not -- another GameObject's RigidBody
// may not have run Init() yet, since the scene sweeps every object through one
// phase before starting the next. So the actual b2Create*Joint is deferred to
// Awake(), where every body in the scene is guaranteed built.
class Joint : public Component {

public:
    static inline const Event CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event CONNECTED_BODY_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event COLLIDE_CONNECTED_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event LOCAL_ANCHOR_A_CHANGED_EVENT = Joint::CreateEvent();
    static inline const Event LOCAL_ANCHOR_B_CHANGED_EVENT = Joint::CreateEvent();

    YAML::Node Serialize() override;
    void Deserialize(const YAML::Node& node) override;

    void Init() override;
    void PostInit() override;
    void Awake() override;
    void Shutdown() override;

    // Swapping the connected body is the ONE change Box2D cannot apply in place
    // (there is no b2Joint_SetBodyA/B), so this destroys and rebuilds the joint.
    void SetConnectedBody(const std::string& gameObjectId);
    const std::string& GetConnectedBody() const { return connectedBodyGameObjectId; }

    void SetCollideConnected(bool value);
    bool GetCollideConnected() const { return collideConnected; }

    void SetLocalAnchorA(glm::vec2 anchor);
    glm::vec2 GetLocalAnchorA() const { return localAnchorA; }
    void SetLocalAnchorB(glm::vec2 anchor);
    glm::vec2 GetLocalAnchorB() const { return localAnchorB; }

    b2JointId GetJointId() const { return jointId; }
    RigidBody* GetOwnRigidBody() const { return ownRigidBody; }
    RigidBody* GetConnectedRigidBody() const { return connectedRigidBody; }
    // True once both bodies resolved and the Box2D joint exists.
    bool IsBuilt() const { return b2Joint_IsValid(jointId); }

    void Accept(IVisitor* v) override;
    std::string GetTypeName() const override { return "Joint"; }

    Joint() = default;

protected:
    // Each subtype fills its own b2<Type>JointDef and calls b2Create<Type>Joint.
    // Only called once both bodies are resolved and valid.
    virtual void CreateJoint() {}

    // Fill the shared b2JointDef fields (bodies, local frames, collideConnected).
    // Subtypes call this on their def's `base` member before adding their own fields.
    void FillBaseDef(b2JointDef& def) const;

    // Tear down and rebuild the b2Joint from current field values. Used by setters
    // that Box2D cannot apply in place. No-op before Awake().
    void RebuildJoint();

    // Copy the shared Joint fields into a freshly allocated subtype copy. Subtypes
    // call this from their Copy() then add their own fields.
    void CopyBaseTo(Joint* copy) const;

    // Push localAnchorA/B (plus any subtype axis rotation) to a live joint.
    virtual void RefreshLocalFrames();
    // Rotation of each local frame, in radians. Only prismatic/wheel override this
    // -- for every other joint type both frames stay unrotated.
    virtual float GetLocalFrameAngleA() const { return 0.0f; }
    virtual float GetLocalFrameAngleB() const { return 0.0f; }

    // Resolve the connected GameObject's RigidBody and subscribe to its shutdown.
    void ResolveConnectedBody();
    // Subscribe to one body's SHUTDOWN_EVENT so this joint is removed with it.
    int SubscribeCascade(RigidBody* body);

    PhysicsSystem* physicsSystem = nullptr;
    b2JointId jointId = b2_nullJointId;

    // Resolved at runtime, never serialized and never copied -- the copy re-resolves
    // against its own container, exactly like Collider's rigidBody pointer.
    RigidBody* ownRigidBody = nullptr;
    RigidBody* connectedRigidBody = nullptr;

    // The serialized cross-reference. A string id (not a pointer) because ids are
    // preserved verbatim by GameObject::Copy/Registry::Copy, so this resolves
    // correctly in the runtime container after the play-mode deep copy.
    std::string connectedBodyGameObjectId = "";

    bool collideConnected = false;
    glm::vec2 localAnchorA = {0, 0};   // pixels, local to body A's origin
    glm::vec2 localAnchorB = {0, 0};   // pixels, local to body B's origin

    // Kept so SetConnectedBody can drop the old body's subscription when retargeting.
    int connectedShutdownSubId = -1;
};
