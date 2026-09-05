// The highest-leverage test in the suite, and the only data-driven one.
//
// Rather than naming components, it walks whatever RegisterComponentTypes() registered and
// asserts the contract every Serializable owes: it can be created by name, it reports the
// name it was registered under, its YAML is stable across a re-parse, and Copy() produces a
// genuinely separate object with identical content. That last one is the play-mode
// invariant -- entering play mode deep-copies the editor container, so a component whose
// Copy() forgets a field loses that field the moment you press Play, in the runtime world
// only, which is about the worst possible place to notice it.
//
// The payoff: a component added next month is covered on the day it is registered, with no
// test written for it.

#include <doctest.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "engine/components/Component.hpp"
#include "engine/components/ComponentRegistrars.hpp"
#include "engine/serialization/Serializable.hpp"
#include "engine/serialization/SerializableFactory.hpp"

namespace {

// RegisterComponentTypes() is idempotent (it overwrites map entries) but there is no
// reason to run it per case.
const std::vector<std::string>& RegisteredTypes() {
    static const std::vector<std::string> names = [] {
        RegisterComponentTypes();
        return SerializableFactory::GetRegisteredTypeNames();
    }();
    return names;
}

std::string Emit(YAML::Node node) {
    std::ostringstream out;
    out << node;
    return out.str();
}

} // namespace

TEST_CASE("the component registry is populated") {
    const auto& names = RegisteredTypes();

    // A guard against the whole suite passing vacuously: every loop below is a no-op if
    // registration silently stopped working.
    REQUIRE_FALSE(names.empty());
    CHECK(names.size() >= 20);

    // Documented as sorted alphabetically, because the editor's Add Component picker
    // renders it directly.
    CHECK(std::is_sorted(names.begin(), names.end()));
}

TEST_CASE("every registered type can be created and reports its registered name") {
    for (const auto& name : RegisteredTypes()) {
        CAPTURE(name);

        std::unique_ptr<Serializable> created(SerializableFactory::Create(name));
        REQUIRE(created != nullptr);

        // The name in the factory and the name in the YAML `type:` field have to match, or
        // a scene saves under one name and fails to resolve under the other.
        CHECK(created->GetTypeName() == name);

        // Serializable's constructor assigns a UUID; nothing should ship without one.
        CHECK_FALSE(created->GetID().empty());
    }
}

TEST_CASE("every registered type survives a serialize/deserialize/serialize round trip") {
    for (const auto& name : RegisteredTypes()) {
        CAPTURE(name);

        std::unique_ptr<Serializable> original(SerializableFactory::Create(name));
        REQUIRE(original != nullptr);

        const YAML::Node first = original->Serialize();
        CHECK(first["type"].as<std::string>() == name);

        // Re-parse through text, exactly as a .scene file does -- an in-memory node can
        // hide type coercion that a YAML round trip exposes.
        const YAML::Node reparsed = YAML::Load(Emit(first));

        std::unique_ptr<Serializable> restored(SerializableFactory::Create(name));
        REQUIRE(restored != nullptr);
        REQUIRE_NOTHROW(restored->Deserialize(reparsed));

        CHECK(restored->GetID() == original->GetID());
        CHECK(Emit(restored->Serialize()) == Emit(first));
    }
}

TEST_CASE("Component::Deserialize tolerates a node missing its base fields") {
    // Serializable::Deserialize has always guarded its one field ("id") with `if (node[...])`.
    // Component::Deserialize did not: it read node["gameobject"] and node["enabled"] straight
    // through, and `.as<T>()` on an absent key throws. A component node written before either
    // field existed -- or truncated, or hand-edited -- therefore threw out of Component and
    // straight out of Scene::Deserialize, losing the entire scene over one missing key.
    //
    // NOTE: this covers Component's OWN two fields only. Most concrete components still read
    // their own fields unguarded (BoxCollider's "isSensor", RigidBody's "bodyType",
    // Transform's "parent_id", SpriteRenderer's "material_id", ScriptComponent's "module"),
    // so adding a field to one of those still breaks every scene saved before it. That is a
    // real but separate fix -- deliberately not made here.
    for (const auto& name : RegisteredTypes()) {
        CAPTURE(name);

        std::unique_ptr<Serializable> obj(SerializableFactory::Create(name));
        REQUIRE(obj != nullptr);
        auto* component = dynamic_cast<Component*>(obj.get());
        REQUIRE(component != nullptr);

        // A node carrying everything the component itself asks for, minus Component's two.
        YAML::Node node = obj->Serialize();
        node.remove("gameobject");
        node.remove("enabled");

        CHECK_NOTHROW(component->Component::Deserialize(node));
    }
}

TEST_CASE("Copy() yields a distinct object with identical serialized content") {
    for (const auto& name : RegisteredTypes()) {
        CAPTURE(name);

        std::unique_ptr<Serializable> original(SerializableFactory::Create(name));
        REQUIRE(original != nullptr);

        std::unique_ptr<Serializable> copy(original->Copy());
        // A null Copy() is the default from Serializable. For a registered component it
        // means play mode drops the component entirely.
        REQUIRE_MESSAGE(copy != nullptr, "Copy() not implemented for " << name);

        CHECK(copy.get() != original.get());
        // Copy deliberately PRESERVES ids -- the play-mode container swap relies on the
        // editor and runtime worlds sharing identity. Duplication is the case that remaps,
        // and it goes through IdRemapper instead.
        CHECK(copy->GetID() == original->GetID());
        CHECK(copy->GetTypeName() == original->GetTypeName());
        CHECK(Emit(copy->Serialize()) == Emit(original->Serialize()));
    }
}
