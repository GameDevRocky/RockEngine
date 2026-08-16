"""Offline safety-contract checks for in-editor AI clarification prompts.

This test deliberately does not launch Qt or import the optional MCP SDK. It protects
the important architectural properties of the feature: inline transcript presentation,
an automatic Other answer, multi-select, and editor-owned destructive authorization.
"""

from __future__ import annotations

import ast
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SERVER = ROOT / "tools/mcp-server/server.py"
SERVICE = ROOT / "Editor/src/mcp/UserClarification.cpp"
INLINE_WIDGET = ROOT / "Editor/src/dock-widgets/AiClarificationWidget.cpp"
CHAT = ROOT / "Editor/src/dock-widgets/AiChatGui.cpp"
ASSISTANT_STYLE = ROOT / "Domain/lib/assets/styling/ai_assistant.qss"
DEFAULT_STYLE = ROOT / "Domain/lib/assets/styling/default.qss"
CLARIFICATION_TOOLS = ROOT / "Editor/src/mcp/tools/ClarificationTools.cpp"
OBJECT_TOOLS = ROOT / "Editor/src/mcp/tools/ObjectTools.cpp"
LIFECYCLE_TOOLS = ROOT / "Editor/src/mcp/tools/LifecycleTools.cpp"
ASSISTANT = ROOT / "Editor/src/ai/AiAgentService.cpp"


def main() -> int:
    server = SERVER.read_text(encoding="utf-8")
    tree = ast.parse(server, filename=str(SERVER))
    functions = {
        node.name: node
        for node in tree.body
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))
    }
    for required in ("ask_user_clarification", "_wait_for_clarification"):
        if required not in functions:
            raise AssertionError(f"missing MCP clarification function: {required}")

    ask = functions["ask_user_clarification"]
    parameters = {argument.arg for argument in ask.args.args}
    if "allow_multiple" not in parameters:
        raise AssertionError("generic clarification tool lost multi-select support")
    if 'call("user.clarification_status"' not in server:
        raise AssertionError("MCP wrapper no longer polls the inline editor question")

    service = SERVICE.read_text(encoding="utf-8")
    if "ClarificationRequested" not in service or "ClarificationResolved" not in service:
        raise AssertionError("clarification service no longer publishes transcript state")
    if 'QStringLiteral("Other")' not in service:
        raise AssertionError("clarification service must always append Other")

    inline = INLINE_WIDGET.read_text(encoding="utf-8")
    chat = CHAT.read_text(encoding="utf-8")
    combined_ui = service + inline + chat
    if "QDialog" in combined_ui or "QApplication::processEvents" in combined_ui:
        raise AssertionError("clarification UI must stay inside the transcript")
    for token in ("QCheckBox", "QRadioButton", "AiClarificationBubble", "ExpansionChanged"):
        if token not in inline:
            raise AssertionError(f"inline answer bank lost required behavior: {token}")
    for token in ("AddClarification", "CreateClarificationWidget", "entry.expanded = false"):
        if token not in chat:
            raise AssertionError(f"chat transcript lost clarification behavior: {token}")

    assistant_style = ASSISTANT_STYLE.read_text(encoding="utf-8")
    default_style = DEFAULT_STYLE.read_text(encoding="utf-8")
    if "AiClarificationBubble" not in assistant_style:
        raise AssertionError("clarification bubble styling must live with the AI Assistant")
    if "UserClarificationDialog" in default_style:
        raise AssertionError("floating clarification dialog styling leaked into default.qss")

    registration = CLARIFICATION_TOOLS.read_text(encoding="utf-8")
    for method in ("user.request_clarification", "user.clarification_status"):
        if method not in registration:
            raise AssertionError(f"editor bridge is missing {method}")

    for path, required_choice in ((OBJECT_TOOLS, "remove"), (LIFECYCLE_TOOLS, "destroy")):
        implementation = path.read_text(encoding="utf-8")
        if "UserClarificationService::Get()->Consume" not in implementation:
            raise AssertionError(f"{path.name} allows an unscoped destructive action")
        if f'QStringLiteral("{required_choice}")' not in implementation:
            raise AssertionError(f"{path.name} no longer requires the destructive user choice")

    prompt = ASSISTANT.read_text(encoding="utf-8")
    for rule in ("ask_user_clarification", "allow_multiple=true", "Do not duplicate Other"):
        if rule not in prompt:
            raise AssertionError(f"assistant clarification contract lost rule: {rule}")

    print("MCP clarification contract OK: inline, collapsible, multi-select, Other, scoped approval")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
