#include "mcp/Tools.hpp"

#include "mcp/McpDispatcher.hpp"
#include "mcp/ToolSupport.hpp"

#include "Engine.hpp"
#include "engine/core/Container.hpp"

#include <QJsonObject>

namespace mcp {

// Play-mode control has no Python binding (Application only exposes is_editor/is_player),
// and it is engine-mode control rather than a component property, so these call C++
// directly instead of adding a binding just to route through it.
void RegisterEngineModeTools(McpDispatcher& dispatcher) {
    dispatcher.RegisterTool("engine.get_mode", [](const QJsonObject&) {
        Engine* engine = Engine::Get();
        QJsonObject result;
        result["appMode"] = engine->IsPlayer() ? "Player" : "Editor";
        result["worldMode"] = support::WorldMode();
        result["isPaused"] = engine->IsPaused();
        result["isTransitioning"] = engine->IsTransitioningPlayMode();
        return McpResult::Ok(result);
    });

    // Both transitions are deferred: Request* submits a job that performs the world
    // copy (or teardown) from a later frame's main step. So a client that needs to act
    // on the running world must poll engine.get_mode until isTransitioning is false
    // rather than assuming the swap happened by the time this returns.
    dispatcher.RegisterTool("engine.enter_play_mode", [](const QJsonObject&) {
        Engine* engine = Engine::Get();
        if (engine->IsTransitioningPlayMode())
            return McpResult::Error(WrongMode, "a play-mode transition is already in flight");
        if (support::IsRuntimeWorld())
            return McpResult::Error(WrongMode, "already in play mode");

        engine->RequestEnterPlayMode();
        QJsonObject result;
        result["status"] = "transition_requested";
        result["poll"] = "engine.get_mode";
        return McpResult::Ok(result);
    });

    dispatcher.RegisterTool("engine.exit_play_mode", [](const QJsonObject&) {
        Engine* engine = Engine::Get();
        if (engine->IsTransitioningPlayMode())
            return McpResult::Error(WrongMode, "a play-mode transition is already in flight");
        if (!support::IsRuntimeWorld())
            return McpResult::Error(WrongMode, "not in play mode");

        engine->RequestExitPlayMode();
        QJsonObject result;
        result["status"] = "transition_requested";
        result["poll"] = "engine.get_mode";
        return McpResult::Ok(result);
    });

    dispatcher.RegisterTool("engine.pause", [](const QJsonObject&) {
        if (!support::IsRuntimeWorld())
            return McpResult::Error(WrongMode, "not in play mode");
        Engine::Get()->PauseMode();
        QJsonObject result;
        result["isPaused"] = Engine::Get()->IsPaused();
        return McpResult::Ok(result);
    });

    dispatcher.RegisterTool("engine.resume", [](const QJsonObject&) {
        if (!support::IsRuntimeWorld())
            return McpResult::Error(WrongMode, "not in play mode");
        Engine::Get()->ResumeMode();
        QJsonObject result;
        result["isPaused"] = Engine::Get()->IsPaused();
        return McpResult::Ok(result);
    });

    // One fixed step + one variable update, for inspecting a simulation frame by frame.
    dispatcher.RegisterTool("engine.step_frame", [](const QJsonObject&) {
        if (!Engine::Get()->IsPaused())
            return McpResult::Error(WrongMode, "step_frame requires a paused play-mode world");
        Engine::Get()->StepFrame();
        return McpResult::Ok();
    });
}

} // namespace mcp
