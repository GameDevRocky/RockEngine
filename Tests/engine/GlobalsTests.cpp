#include <doctest.h>

#include "engine/core/Globals.hpp"

#include <any>
#include <optional>
#include <sstream>

namespace {

YAML::Node TestConfig()
{
    return YAML::Load(R"(
Globals:
  Rendering:
    useGPUNormalMapGeneration:
      type: bool
      value: true
    normalStrength:
      type: float
      value: 1.5
      widget: slider
      range: [0.0, 4.0]
      step: 0.25
    sampleCount:
      type: int
      value: 16
      range: [1, 64]
    qualityLevel:
      type: int
      value: 2
      widget: dropdown
      options:
        - {label: Low, value: 1}
        - {label: High, value: 2}
    previewMode:
      type: string
      value: Quality
      widget: dropdown
      options: [Fast, Quality]
    releaseNotes:
      type: string
      value: Ready
      widget: multiline
    normalRange:
      type: vec2
      value: [0.2, 0.8]
      widget: range_slider
      range: [0.0, 1.0]
    lightDirection:
      type: vec3
      value: [0.0, 1.0, 0.0]
    channelMask:
      type: vec4
      value: [1.0, 0.0, 1.0, 0.0]
    tint:
      type: color
      value: [0.1, 0.2, 0.3, 1.0]
  Physics:
    gravity:
      type: vec2
      value: [0.0, -9.8]
)");
}

std::string Emit(const YAML::Node& node)
{
    std::ostringstream output;
    output << node;
    return output.str();
}

} // namespace

TEST_CASE("Globals loads grouped typed properties and editor metadata")
{
    Globals* globals = Globals::Get();
    globals->Deserialize(TestConfig());

    REQUIRE(globals->GetGroups().size() == 2);
    CHECK(globals->Get<bool>("Rendering", "useGPUNormalMapGeneration"));
    CHECK(globals->Get<float>("Rendering", "normalStrength") == doctest::Approx(1.5f));
    CHECK(globals->Get<int>("Rendering", "sampleCount") == 16);
    CHECK(globals->Get<int>("Rendering", "qualityLevel") == 2);
    CHECK(globals->Get<std::string>("Rendering", "previewMode") == "Quality");
    CHECK(globals->Get<std::string>("Rendering", "releaseNotes") == "Ready");

    const glm::vec2 gravity = globals->Get<glm::vec2>("Physics", "gravity");
    CHECK(gravity.x == doctest::Approx(0.0f));
    CHECK(gravity.y == doctest::Approx(-9.8f));

    const GlobalProperty* strength = globals->FindProperty("Rendering", "normalStrength");
    REQUIRE(strength != nullptr);
    CHECK(strength->descriptor.tag == Properties::Tags::SLIDER);
    CHECK(strength->descriptor.min == doctest::Approx(0.0f));
    CHECK(strength->descriptor.max == doctest::Approx(4.0f));
    CHECK(strength->descriptor.step == doctest::Approx(0.25f));

    const GlobalProperty* range = globals->FindProperty("Rendering", "normalRange");
    REQUIRE(range != nullptr);
    CHECK(range->descriptor.tag == Properties::Tags::RANGE_SLIDER);

    const GlobalProperty* notes = globals->FindProperty("Rendering", "releaseNotes");
    REQUIRE(notes != nullptr);
    CHECK(notes->descriptor.tag == Properties::Tags::MULTILINE);

    const GlobalProperty* quality = globals->FindProperty("Rendering", "qualityLevel");
    REQUIRE(quality != nullptr);
    CHECK(quality->descriptor.tag == Properties::Tags::DROPDOWN);
    CHECK(quality->descriptor.dropdownOptions.size() == 2);
}

TEST_CASE("Globals serialization round trips values and widget declarations")
{
    Globals* globals = Globals::Get();
    globals->Deserialize(TestConfig());
    const std::string serialized = Emit(globals->Serialize());

    globals->Deserialize(YAML::Load(serialized));

    CHECK(Emit(globals->Serialize()) == serialized);
}

TEST_CASE("Globals accepts legacy Gatekeeper boolean shorthand")
{
    Globals* globals = Globals::Get();
    globals->Deserialize(YAML::Load(R"(
Gatekeeper:
  Rendering:
    useGpuParticles: false
)"));

    CHECK_FALSE(globals->Get<bool>("Rendering", "useGpuParticles", true));
    CHECK(globals->FindProperty("Rendering", "useGpuParticles") != nullptr);
}

TEST_CASE("Globals is a process singleton and is not container bound")
{
    CHECK(Globals::Get() == Globals::Get());
    CHECK(Globals::Get()->GetContainer() == nullptr);
}

TEST_CASE("Globals publishes property changes with old and new values")
{
    Globals* globals = Globals::Get();
    globals->Deserialize(TestConfig());

    int calls = 0;
    std::optional<GlobalPropertyChange> received;
    const int subscription = globals->Subscribe([&](std::any payload) {
        ++calls;
        received = std::any_cast<GlobalPropertyChange>(payload);
        return true;
    }, Globals::PROPERTY_CHANGED_EVENT);

    CHECK(globals->SetValue("Rendering", "sampleCount", GlobalValue(32)));
    REQUIRE(received.has_value());
    CHECK(calls == 1);
    CHECK(received->group == "Rendering");
    CHECK(received->property == "sampleCount");
    CHECK(std::get<int>(received->previousValue) == 16);
    CHECK(std::get<int>(received->value) == 32);

    // Reassigning the current value is not a change and must not wake subscribers.
    CHECK(globals->SetValue("Rendering", "sampleCount", GlobalValue(32)));
    CHECK(calls == 1);
    CHECK_FALSE(globals->SetValue("Rendering", "sampleCount", GlobalValue(32.0f)));
    CHECK(calls == 1);

    globals->Unsubscribe(subscription);
}
