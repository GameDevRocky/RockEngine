#include "engine/components/Joint.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/ComponentImpl.hpp"
#include "engine/core/GameObjectImpl.hpp"
#include "engine/core/PhysicsSystem.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/utils/IVisitor.hpp"
#include "yaml-cpp/yaml.h"
#include <iostream>

using namespace EngineUtils::RenderUtils;

YAML::Node Joint::Serialize(){
    YAML::Node node = Component::Serialize();
    node["connectedBody"] = connectedBodyGameObjectId;
    node["collideConnected"] = collideConnected;
    node["localAnchorA"][0] = localAnchorA.x;
    node["localAnchorA"][1] = localAnchorA.y;
    node["localAnchorA"].SetStyle(YAML::EmitterStyle::Flow);
    node["localAnchorB"][0] = localAnchorB.x;
    node["localAnchorB"][1] = localAnchorB.y;
    node["localAnchorB"].SetStyle(YAML::EmitterStyle::Flow);
    return node;
}

void Joint::Deserialize(const YAML::Node& node){
    Component::Deserialize(node);
    if (node["connectedBody"]) connectedBodyGameObjectId = node["connectedBody"].as<std::string>();
    if (node["collideConnected"]) collideConnected = node["collideConnected"].as<bool>();
    if (node["localAnchorA"])
        localAnchorA = {node["localAnchorA"][0].as<float>(), node["localAnchorA"][1].as<float>()};
    if (node["localAnchorB"])
        localAnchorB = {node["localAnchorB"][0].as<float>(), node["localAnchorB"][1].as<float>()};
    jointId = b2_nullJointId;
    state = State::Loaded;
}

int Joint::SubscribeCascade(RigidBody* body){
    if (!body) return -1;
    // Capture the id, never `this` -- the lambda outlives any particular container
    // (it is re-registered on the runtime copy too), and re-resolving through the
    // active registry is what makes it safe after the play-mode swap. A joint that
    // is already gone resolves to null and the handler quietly retires.
    const std::string jointComponentId = GetID();
    return body->Subscribe([jointComponentId](){
        Joint* self = Registry::FindInRuntime<Joint>(jointComponentId);
        if (!self) return false;
        if (GameObject* go = self->GetGameObject())
            go->RemoveComponent(self);
        return false;   // one-shot
    }, RuntimeObject::SHUTDOWN_EVENT);
}

void Joint::Init(){
    if (state >= State::Initialized) return;
    physicsSystem = container->FindSystem<PhysicsSystem>();

    ownRigidBody = GetComponent<RigidBody>();
    if (!ownRigidBody){
        // Deliberately NOT RequireComponent<RigidBody>() the way Collider does. A
        // collider auto-getting a kinematic body is harmless; a joint silently
        // anchored to an auto-created kinematic body would ignore every motor force
        // and fail invisibly. Better to stay inert and say so.
        std::cerr << GetTypeName() << "::Init - requires a RigidBody on the same GameObject: "
                  << (GetGameObject() ? GetGameObject()->GetName() : "<none>") << std::endl;
        state = State::Initialized;
        return;
    }

    SubscribeCascade(ownRigidBody);
    state = State::Initialized;
}

void Joint::ResolveConnectedBody(){
    connectedRigidBody = nullptr;
    if (connectedBodyGameObjectId.empty()) return;

    Registry* reg = container ? container->FindSystem<Registry>() : nullptr;
    if (!reg) return;

    GameObject* connected = reg->Find<GameObject>(connectedBodyGameObjectId);
    if (!connected) return;

    connectedRigidBody = connected->GetComponent<RigidBody>();
    if (connectedRigidBody)
        connectedShutdownSubId = SubscribeCascade(connectedRigidBody);
}

void Joint::PostInit(){
    if (state >= State::PostInitialized) return;
    // Only resolving C++ components here -- the connected body's b2BodyId may not
    // exist yet, which is why joint creation waits for Awake().
    ResolveConnectedBody();
    state = State::PostInitialized;
}

void Joint::Awake(){
    if (state >= State::Awakened) return;
    state = State::Awakened;

    if (!ownRigidBody || !connectedRigidBody) return;
    if (!b2Body_IsValid(ownRigidBody->GetBodyId()) ||
        !b2Body_IsValid(connectedRigidBody->GetBodyId())) return;

    CreateJoint();
}

void Joint::FillBaseDef(b2JointDef& def) const {
    def.bodyIdA = ownRigidBody ? ownRigidBody->GetBodyId() : b2_nullBodyId;
    def.bodyIdB = connectedRigidBody ? connectedRigidBody->GetBodyId() : b2_nullBodyId;
    def.localFrameA.p = {PixelsToMeters(localAnchorA.x), PixelsToMeters(localAnchorA.y)};
    def.localFrameB.p = {PixelsToMeters(localAnchorB.x), PixelsToMeters(localAnchorB.y)};
    def.localFrameA.q = b2MakeRot(GetLocalFrameAngleA());
    def.localFrameB.q = b2MakeRot(GetLocalFrameAngleB());
    def.collideConnected = collideConnected;
}

void Joint::RebuildJoint(){
    if (physicsSystem) physicsSystem->DestroyJoint(jointId);
    jointId = b2_nullJointId;

    // Before Awake there is nothing to rebuild -- Awake() will build it once.
    if (state < State::Awakened) return;
    if (!ownRigidBody || !connectedRigidBody) return;
    if (!b2Body_IsValid(ownRigidBody->GetBodyId()) ||
        !b2Body_IsValid(connectedRigidBody->GetBodyId())) return;

    CreateJoint();
}

void Joint::RefreshLocalFrames(){
    if (!b2Joint_IsValid(jointId)) return;
    b2Transform frameA;
    frameA.p = {PixelsToMeters(localAnchorA.x), PixelsToMeters(localAnchorA.y)};
    frameA.q = b2MakeRot(GetLocalFrameAngleA());
    b2Transform frameB;
    frameB.p = {PixelsToMeters(localAnchorB.x), PixelsToMeters(localAnchorB.y)};
    frameB.q = b2MakeRot(GetLocalFrameAngleB());
    b2Joint_SetLocalFrameA(jointId, frameA);
    b2Joint_SetLocalFrameB(jointId, frameB);
    b2Joint_WakeBodies(jointId);
}

void Joint::SetConnectedBody(const std::string& gameObjectId){
    if (connectedBodyGameObjectId == gameObjectId) return;
    connectedBodyGameObjectId = gameObjectId;

    // Stop listening to the body we are no longer attached to, otherwise its later
    // destruction would take this joint down with it.
    if (connectedRigidBody && connectedShutdownSubId >= 0)
        connectedRigidBody->Unsubscribe(connectedShutdownSubId);
    connectedShutdownSubId = -1;

    ResolveConnectedBody();
    RebuildJoint();   // no b2Joint_SetBodyA/B exists; retargeting means recreating

    this->Notify(Joint::CONNECTED_BODY_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void Joint::SetCollideConnected(bool value){
    if (collideConnected == value) return;
    collideConnected = value;
    if (b2Joint_IsValid(jointId)) b2Joint_SetCollideConnected(jointId, value);
    this->Notify(Joint::COLLIDE_CONNECTED_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void Joint::SetLocalAnchorA(glm::vec2 anchor){
    if (localAnchorA == anchor) return;
    localAnchorA = anchor;
    RefreshLocalFrames();
    this->Notify(Joint::LOCAL_ANCHOR_A_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void Joint::SetLocalAnchorB(glm::vec2 anchor){
    if (localAnchorB == anchor) return;
    localAnchorB = anchor;
    RefreshLocalFrames();
    this->Notify(Joint::LOCAL_ANCHOR_B_CHANGED_EVENT);
    this->Notify(Joint::CHANGED_EVENT);
}

void Joint::CopyBaseTo(Joint* copy) const {
    copy->id = id;
    copy->gameobject_id = gameobject_id;
    copy->enabled = enabled;
    copy->connectedBodyGameObjectId = connectedBodyGameObjectId;
    copy->collideConnected = collideConnected;
    copy->localAnchorA = localAnchorA;
    copy->localAnchorB = localAnchorB;
    // jointId and the resolved body pointers are deliberately NOT copied -- the new
    // container has its own b2World, so the copy rebuilds them in its own Awake().
    copy->state = State::Loaded;
}

void Joint::Accept(IVisitor* v) {
    v->Visit(this);
}

void Joint::Shutdown(){
    if (physicsSystem) physicsSystem->DestroyJoint(jointId);
    jointId = b2_nullJointId;
    physicsSystem = nullptr;
    ownRigidBody = nullptr;
    connectedRigidBody = nullptr;
    Component::Shutdown();
}
