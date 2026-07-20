#pragma once

// Records completed gizmo drags into the undo history.
//
// GizmosManager reports a finished gesture via GizmosManager::EDIT_COMMITTED_EVENT;
// this subscribes on the editor side and turns each reported edit into a command.
// Keeping the decision here (rather than pushing from Engine) is what stops
// script- or engine-driven transform changes from entering the history.
namespace GizmoUndoBridge {
    // Idempotent-by-convention: call once, from Editor::PostInit.
    void Install();
}
