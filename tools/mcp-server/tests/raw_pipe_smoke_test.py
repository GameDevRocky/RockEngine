"""Talk to the editor's MCP bridge directly, with no MCP SDK in the way.

This is the primary verification loop while the engine-side tool surface is being
built: launch the editor by hand, run this, and every failure is unambiguously in
the bridge rather than in MCP framing.

Usage:
    python tests/raw_pipe_smoke_test.py                 # ping only
    python tests/raw_pipe_smoke_test.py <method> [json]  # any single call
"""

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from editor_bridge import EditorBridge, EditorRpcError  # noqa: E402


def main() -> int:
    method = sys.argv[1] if len(sys.argv) > 1 else "ping"
    params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}

    try:
        with EditorBridge() as bridge:
            result = bridge.call(method, params)
    except ConnectionError as e:
        print(f"could not reach the editor: {e}")
        print("is RockEngineLauncher running?")
        return 1
    except EditorRpcError as e:
        print(f"tool returned an error: {e}")
        return 1

    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
