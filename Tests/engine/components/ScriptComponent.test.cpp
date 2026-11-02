#include <memory>
#include <gtest/gtest.h>
#include "RuntimeFixture.hpp"
#include "engine/components/ScriptComponent.hpp"

using ScriptComponentTest = RuntimeTest;

TEST_F(ScriptComponentTest, ScriptIdentitySurvivesSerializationAndCopy) {
    ScriptComponent script;
    YAML::Node saved = script.Serialize();
    saved["module"] = "example_script";
    saved["class"] = "ExampleScript";
    script.Deserialize(saved);

    std::unique_ptr<ScriptComponent> copy(script.Copy());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->GetScriptModuleName(), "example_script");
    EXPECT_EQ(copy->GetScriptClassName(), "ExampleScript");
    EXPECT_EQ(copy->GetID(), script.GetID());
}

TEST_F(ScriptComponentTest, UnresolvedScriptPreservesPendingFieldValues) {
    ScriptComponent script;
    YAML::Node saved = script.Serialize();
    saved["module"] = "deleted_module";
    saved["class"] = "DeletedScript";
    saved["fields"]["speed"] = 4.5f;
    saved["fields"]["label"] = "checkpoint";
    script.Deserialize(saved);

    YAML::Node serialized = script.Serialize();
    EXPECT_FLOAT_EQ(serialized["fields"]["speed"].as<float>(), 4.5f);
    EXPECT_EQ(serialized["fields"]["label"].as<std::string>(), "checkpoint");
    std::unique_ptr<ScriptComponent> copy(script.Copy());
    ASSERT_NE(copy, nullptr);
    EXPECT_FLOAT_EQ(copy->Serialize()["fields"]["speed"].as<float>(), 4.5f);
    EXPECT_EQ(copy->Serialize()["fields"]["label"].as<std::string>(), "checkpoint");
}

TEST_F(ScriptComponentTest, UnassignedScriptHasNoLiveInstanceOrActions) {
    ScriptComponent script;
    EXPECT_FALSE(script.IsScriptMissing());
    EXPECT_TRUE(script.GetScriptFilePath().empty());
    EXPECT_EQ(script.GetScriptInstanceHandle(), nullptr);
    EXPECT_TRUE(script.GetFields().empty());
    EXPECT_TRUE(script.GetScriptActions().empty());
    EXPECT_EQ(script.Serialize()["module"].as<std::string>(), "");
}
