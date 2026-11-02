#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "utils/SceneEditController.hpp"

namespace {

struct ToolLog {
    std::vector<std::string> calls;
};

class RecordingTool final : public SceneEditTool {
public:
    RecordingTool(ToolLog& log, std::string name, bool claimPress, bool claimKey = false)
        : log(log), name(std::move(name)), claimPress(claimPress), claimKey(claimKey) {}

    bool PointerPress(const SceneEditContext&, const SceneEditPointerEvent&) override {
        log.calls.push_back(name + ".press");
        return claimPress;
    }
    bool PointerMove(const SceneEditContext&, const SceneEditPointerEvent&) override {
        log.calls.push_back(name + ".move");
        return true;
    }
    bool PointerRelease(const SceneEditContext&, const SceneEditPointerEvent&) override {
        log.calls.push_back(name + ".release");
        return true;
    }
    bool KeyPress(const SceneEditContext&, int, Qt::KeyboardModifiers) override {
        log.calls.push_back(name + ".key");
        return claimKey;
    }
    void Finish(const SceneEditContext&) override { log.calls.push_back(name + ".finish"); }

private:
    ToolLog& log;
    std::string name;
    bool claimPress;
    bool claimKey;
};

TEST(SceneEditControllerTest, LastRegisteredClaimingToolOwnsGestureUntilMatchingRelease) {
    ToolLog log;
    std::vector<std::unique_ptr<SceneEditTool>> tools;
    tools.push_back(std::make_unique<RecordingTool>(log, "first", true));
    tools.push_back(std::make_unique<RecordingTool>(log, "last", true));
    SceneEditController controller(std::move(tools));
    const SceneEditContext context;
    SceneEditPointerEvent event;
    event.button = Qt::LeftButton;

    EXPECT_TRUE(controller.PointerPress(context, event));
    EXPECT_TRUE(controller.PointerMove(context, event));
    event.button = Qt::RightButton;
    EXPECT_FALSE(controller.PointerRelease(context, event));
    EXPECT_TRUE(controller.PointerMove(context, event));
    event.button = Qt::LeftButton;
    EXPECT_TRUE(controller.PointerRelease(context, event));
    EXPECT_EQ(log.calls, (std::vector<std::string>{
        "last.press", "last.move", "last.move", "last.release"}));
}

TEST(SceneEditControllerTest, FinishCancelsCaptureAndLaterReleaseDoesNothing) {
    ToolLog log;
    std::vector<std::unique_ptr<SceneEditTool>> tools;
    tools.push_back(std::make_unique<RecordingTool>(log, "tool", true));
    SceneEditController controller(std::move(tools));
    const SceneEditContext context;
    SceneEditPointerEvent event;
    event.button = Qt::LeftButton;

    ASSERT_TRUE(controller.PointerPress(context, event));
    controller.Finish(context);
    EXPECT_FALSE(controller.PointerRelease(context, event));
    EXPECT_EQ(log.calls, (std::vector<std::string>{"tool.press", "tool.finish"}));
    EXPECT_TRUE(controller.PointerPress(context, event));
}

TEST(SceneEditControllerTest, KeyDispatchStopsAtFirstClaimingToolInReverseOrder) {
    ToolLog log;
    std::vector<std::unique_ptr<SceneEditTool>> tools;
    tools.push_back(std::make_unique<RecordingTool>(log, "first", false, true));
    tools.push_back(std::make_unique<RecordingTool>(log, "last", false, false));
    SceneEditController controller(std::move(tools));

    EXPECT_TRUE(controller.KeyPress({}, Qt::Key_Delete, Qt::NoModifier));
    EXPECT_EQ(log.calls, (std::vector<std::string>{"last.key", "first.key"}));
}

} // namespace
