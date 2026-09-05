// IdRemapper is what makes GameObject duplication produce NEW objects rather than a second
// reference to the same ones. It is deliberately blunt -- it rewrites any scalar that
// matches a mapped id, rather than knowing which fields hold ids -- and that bluntness is
// only safe because of invariants that nothing else enforces. These tests are that
// enforcement: when it breaks, duplication is silently wrong (shared components, refs
// pointing back at the original) rather than loud.

#include <doctest.h>

#include <string>

#include "engine/serialization/IdRemapper.hpp"

namespace {

// A 32-hex id shaped like EngineUtils::GenerateUUID's output, with a distinguishing tail.
std::string FakeId(char tag) {
    return std::string(31, 'a') + tag;
}

} // namespace

TEST_CASE("BuildMap mints one fresh id per node in both sequences") {
    YAML::Node gameobjects;
    YAML::Node go;
    go["id"] = FakeId('1');
    gameobjects.push_back(go);

    YAML::Node components;
    YAML::Node comp;
    comp["id"] = FakeId('2');
    components.push_back(comp);

    const auto map = IdRemapper::BuildMap(gameobjects, components);

    REQUIRE(map.size() == 2);
    // Components are mapped in the same pass as GameObjects on purpose: component_ids on a
    // GameObject points at them, so remapping one without the other tears the graph.
    REQUIRE(map.count(FakeId('1')) == 1);
    REQUIRE(map.count(FakeId('2')) == 1);

    CHECK(map.at(FakeId('1')) != FakeId('1'));
    CHECK(map.at(FakeId('2')) != FakeId('2'));
    CHECK(map.at(FakeId('1')) != map.at(FakeId('2')));  // one-to-one, never collapsed
}

TEST_CASE("BuildMap ignores nodes with no id and tolerates non-sequences") {
    YAML::Node gameobjects;
    YAML::Node withoutId;
    withoutId["name"] = "no id here";
    gameobjects.push_back(withoutId);

    YAML::Node notASequence;
    notASequence["id"] = FakeId('3');   // a map, not a sequence

    const auto map = IdRemapper::BuildMap(gameobjects, notASequence);
    CHECK(map.empty());
}

TEST_CASE("Apply rewrites mapped ids and leaves everything else alone") {
    const std::string oldId    = FakeId('1');
    const std::string newId    = FakeId('9');
    const std::string assetId  = FakeId('7');   // an asset, never part of the duplicated set
    const std::string outsideId = FakeId('8');  // a reference pointing out of the set

    IdRemapper::IdMap map{{oldId, newId}};

    YAML::Node node;
    node["gameobject"] = oldId;
    node["material"]   = assetId;
    node["target"]     = outsideId;
    node["name"]       = "Player";

    IdRemapper::Apply(node, map);

    CHECK(node["gameobject"].as<std::string>() == newId);
    // The whole safety argument for rewriting "any matching scalar" is that the map only
    // ever holds ids from inside the duplicated set. Asset ids and outward references must
    // survive untouched or a duplicate silently loses its sprite/material.
    CHECK(node["material"].as<std::string>() == assetId);
    CHECK(node["target"].as<std::string>()   == outsideId);
    CHECK(node["name"].as<std::string>()     == "Player");
}

TEST_CASE("Apply does not rewrite map keys") {
    const std::string oldId = FakeId('1');
    IdRemapper::IdMap map{{oldId, FakeId('9')}};

    YAML::Node node;
    node[oldId] = "some value";   // an id used as a FIELD NAME

    IdRemapper::Apply(node, map);

    // Keys are field names, never ids. Rewriting one would rename a field out of existence.
    REQUIRE(node[oldId]);
    CHECK(node[oldId].as<std::string>() == "some value");
}

TEST_CASE("Apply walks nested maps and sequences") {
    const std::string oldId = FakeId('1');
    const std::string newId = FakeId('9');
    IdRemapper::IdMap map{{oldId, newId}};

    YAML::Node root;
    root["component_ids"].push_back(oldId);      // scalar inside a sequence
    root["fields"]["ref"] = oldId;               // scalar inside a nested map

    YAML::Node listed;
    listed["gameobject"] = oldId;                // map inside a sequence
    root["children"].push_back(listed);

    IdRemapper::Apply(root, map);

    CHECK(root["component_ids"][0].as<std::string>()   == newId);
    CHECK(root["fields"]["ref"].as<std::string>()      == newId);
    CHECK(root["children"][0]["gameobject"].as<std::string>() == newId);
}

TEST_CASE("Apply with an empty map is a no-op") {
    const std::string id = FakeId('1');

    YAML::Node node;
    node["gameobject"] = id;

    IdRemapper::Apply(node, IdRemapper::IdMap{});

    CHECK(node["gameobject"].as<std::string>() == id);
}
