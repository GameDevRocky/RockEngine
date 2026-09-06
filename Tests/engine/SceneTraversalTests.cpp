#include <doctest.h>

#include <string>

#include "Engine.hpp"
#include "engine/components/Component.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/serialization/Registry.hpp"

namespace {

class AwakeProbe final : public Component {
public:
    AwakeProbe(int& awakeCount, int& shutdownCount, bool mutateScene)
        : awakeCount(awakeCount), shutdownCount(shutdownCount), mutateScene(mutateScene) {}

    std::string GetTypeName() const override { return "AwakeProbe"; }

    void Awake() override {
        ++awakeCount;
        RuntimeObject::Awake();

        if (!mutateScene) return;
        mutateScene = false;

        GameObject* owner = GetGameObject();
        Scene* scene = owner ? owner->GetScene() : nullptr;
        REQUIRE(scene != nullptr);

        // This is the relevant ObjectPool shape: mutate the registry/root list from
        // Awake, then query roots again. The old Scene::Awake range-for held iterators
        // into the cache this query clears and potentially reallocates.
        scene->AddGameObject(new GameObject());
        (void)scene->GetRootObjects();
    }

    void Shutdown() override {
        ++shutdownCount;
        RuntimeObject::Shutdown();
    }

private:
    int& awakeCount;
    int& shutdownCount;
    bool mutateScene;
};

GameObject* AddProbeObject(Scene& scene, int& awakeCount, int& shutdownCount,
                           bool mutateScene) {
    auto* object = new GameObject();
    scene.AddGameObject(object);
    object->AddComponent(new AwakeProbe(awakeCount, shutdownCount, mutateScene));
    return object;
}

Container* TestContainer() {
    static Container* container = [] {
        Engine* engine = Engine::Get();
        engine->SetAppMode(AppMode::Player);
        engine->Init();
        return engine->GetEditorContainer();
    }();
    return container;
}

} // namespace

TEST_CASE("scene lifecycle traversal survives hierarchy mutation from Awake") {
    Container* container = TestContainer();
    Registry* registry = container->FindSystem<Registry>();
    REQUIRE(registry != nullptr);

    auto* scene = new Scene();
    scene->Attach(container);
    registry->Register(scene);
    scene->Init();

    int rootAwakes = 0;
    int rootShutdowns = 0;
    int childAAwakes = 0;
    int childAShutdowns = 0;
    int childBAwakes = 0;
    int childBShutdowns = 0;

    GameObject* root = AddProbeObject(*scene, rootAwakes, rootShutdowns, true);
    GameObject* childA = AddProbeObject(*scene, childAAwakes, childAShutdowns, false);
    GameObject* childB = AddProbeObject(*scene, childBAwakes, childBShutdowns, false);
    childA->GetTransform()->SetParent(root->GetTransform());
    childB->GetTransform()->SetParent(root->GetTransform());

    scene->PostInit();
    container->SetMode(Container::Mode::Runtime);

    REQUIRE_NOTHROW(scene->Awake());
    CHECK(rootAwakes == 1);
    CHECK(childAAwakes == 1);
    CHECK(childBAwakes == 1);
    CHECK(scene->GetRootObjects().size() == 2); // original root + root spawned in Awake

    scene->Shutdown();
    registry->FlushPendingShutdowns();
    container->SetMode(Container::Mode::Editor);

    CHECK(rootShutdowns == 1);
    CHECK(childAShutdowns == 1);
    CHECK(childBShutdowns == 1);
}
