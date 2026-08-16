"""Offline contract checks for the discoverable property bridge.

This deliberately does not launch the editor. It catches the common regression where a
new component is registered with the engine but omitted from MCP property discovery.
Run from anywhere with `python tools/mcp-server/tests/property_coverage_test.py`.
"""

from __future__ import annotations

import ast
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
REGISTRARS = ROOT / "Engine/src/components/ComponentRegistrars.cpp"
PROPERTY_TOOLS = ROOT / "Editor/src/mcp/tools/PropertyTools.cpp"
SERVER = ROOT / "tools/mcp-server/server.py"


def main() -> int:
    registrars = REGISTRARS.read_text(encoding="utf-8")
    implementation = PROPERTY_TOOLS.read_text(encoding="utf-8")

    registered = set(re.findall(r'RegisterType\("([A-Za-z0-9_]+)"', registrars))
    component_bag = implementation.split("PropertyBag BuildComponentBag", 1)[1].split(
        "struct ResolvedAsset", 1
    )[0]
    covered = set(re.findall(r"dynamic_cast<([A-Za-z0-9_]+)\s*\*>", component_bag))
    missing = sorted(registered - covered)
    if missing:
        raise AssertionError(
            "registered components missing from MCP property discovery: "
            + ", ".join(missing)
        )

    # Parse, rather than import, so this test does not require the optional MCP SDK.
    tree = ast.parse(SERVER.read_text(encoding="utf-8"), filename=str(SERVER))
    functions = {node.name for node in tree.body if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))}
    required = {
        "get_capabilities", "list_components", "describe_component",
        "get_component_property", "set_component_property", "set_component_properties",
        "invoke_component_action", "describe_asset", "get_asset_property",
        "set_asset_property", "set_asset_properties", "save_asset", "resolve_reference",
        "get_console_messages", "capture_scene_view", "ask_user_clarification",
    }
    absent = sorted(required - functions)
    if absent:
        raise AssertionError("public MCP wrappers missing: " + ", ".join(absent))

    print(f"MCP property coverage OK: {len(registered)} component types, {len(required)} public tools")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
