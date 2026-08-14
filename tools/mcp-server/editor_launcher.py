"""Locate and start RockEngineLauncher, so a tool call can succeed against a cold machine."""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

EXE_NAME = "RockEngineLauncher.exe" if sys.platform == "win32" else "RockEngineLauncher"


def repo_root() -> Path:
    if override := os.environ.get("ROCKENGINE_ROOT"):
        return Path(override)
    # tools/mcp-server/editor_launcher.py -> repo root
    return Path(__file__).resolve().parents[2]


def editor_exe() -> Path:
    if override := os.environ.get("ROCKENGINE_EDITOR_EXE"):
        return Path(override)
    return repo_root() / "build" / "local" / "bin" / EXE_NAME


def is_running() -> bool:
    """Best-effort check by process name. Only used for reporting -- whether the bridge
    is actually reachable is decided by trying to connect."""
    try:
        if sys.platform == "win32":
            out = subprocess.run(
                ["tasklist", "/FI", f"IMAGENAME eq {EXE_NAME}", "/NH"],
                capture_output=True, text=True, timeout=10,
            ).stdout
            return EXE_NAME.lower() in out.lower()
        out = subprocess.run(["pgrep", "-f", EXE_NAME], capture_output=True, text=True, timeout=10)
        return out.returncode == 0
    except (OSError, subprocess.SubprocessError):
        return False


def ensure_editor_running() -> str:
    """Start the editor if it isn't already up. Returns a human-readable note about
    what happened, for inclusion in a timeout message."""
    if is_running():
        return "editor process is running but its bridge is not accepting connections yet"

    exe = editor_exe()
    if not exe.is_file():
        return (
            f"no editor executable at {exe} -- build it, or point ROCKENGINE_EDITOR_EXE at it"
        )

    kwargs: dict = {"cwd": str(exe.parent)}
    if sys.platform == "win32":
        # Detached, so the editor outlives this short-lived bridge process.
        kwargs["creationflags"] = subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
    else:
        kwargs["start_new_session"] = True

    try:
        subprocess.Popen([str(exe)], **kwargs)
    except OSError as e:
        return f"could not launch {exe}: {e}"
    return f"launched {exe}"
