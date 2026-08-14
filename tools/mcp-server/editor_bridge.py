"""Client for the editor's MCP bridge (Editor/src/mcp/).

Protocol is newline-delimited JSON-RPC 2.0 over a QLocalServer endpoint: a named pipe
on Windows, a Unix domain socket elsewhere.

Synchronous by design. The MCP server layer is async, but the wire protocol is a
strictly serial request/response exchange over one connection, so an async
implementation would buy nothing and still need a lock to keep two coroutines from
interleaving writes. server.py runs these calls off the event loop instead.
"""

from __future__ import annotations

import json
import socket
import sys
import time
from pathlib import Path
from typing import Any

import editor_launcher

SERVER_NAME = "RockEngine.McpBridge.v1"
WINDOWS_PIPE_PATH = rf"\\.\pipe\{SERVER_NAME}"

# Generous: a cold editor start pays for Qt init, GL context creation and the startup
# asset scan before it ever reaches Editor::PostInit and opens the pipe.
CONNECT_TIMEOUT_SEC = 90.0
_BACKOFF_SEC = (0.25, 0.5, 1.0, 2.0, 3.0)


class EditorRpcError(Exception):
    """The editor accepted the call and answered with a JSON-RPC error."""

    def __init__(self, code: int, message: str):
        super().__init__(f"[{code}] {message}")
        self.code = code
        self.message = message


class _PipeTransport:
    """Windows named pipe. Opened as a file -- CreateFile works on pipes, so this
    needs no pywin32."""

    def __init__(self, path: str):
        self._f = open(path, "r+b", buffering=0)

    def send(self, data: bytes) -> None:
        self._f.write(data)
        self._f.flush()

    def read_some(self) -> bytes:
        # One CreateFile-backed read returns whatever is in the pipe, up to the size
        # asked for; it does not block for a full buffer.
        return self._f.read(65536)

    def close(self) -> None:
        self._f.close()


class _SocketTransport:
    """Unix domain socket."""

    def __init__(self, path: str):
        self._s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._s.connect(path)

    def send(self, data: bytes) -> None:
        self._s.sendall(data)

    def read_some(self) -> bytes:
        return self._s.recv(65536)

    def close(self) -> None:
        self._s.close()


def _unix_socket_path() -> str:
    # Where QLocalServer puts its socket when given a bare name.
    for base in (Path(f"/tmp/{SERVER_NAME}"), Path.home() / f".{SERVER_NAME}"):
        if base.exists():
            return str(base)
    return f"/tmp/{SERVER_NAME}"


class EditorBridge:
    def __init__(self, *, autolaunch: bool = True):
        self._transport: _PipeTransport | _SocketTransport | None = None
        self._buffer = b""
        self._next_id = 1
        self._autolaunch = autolaunch

    def __enter__(self) -> "EditorBridge":
        self.connect()
        return self

    def __exit__(self, *_exc) -> None:
        self.close()

    def _open_transport(self) -> _PipeTransport | _SocketTransport:
        if sys.platform == "win32":
            return _PipeTransport(WINDOWS_PIPE_PATH)
        return _SocketTransport(_unix_socket_path())

    def connect(self) -> None:
        """Connect, launching the editor and waiting for it if it isn't up yet."""
        if self._transport is not None:
            return

        try:
            self._transport = self._open_transport()
            return
        except OSError:
            pass

        launched_msg = ""
        if self._autolaunch:
            launched_msg = editor_launcher.ensure_editor_running()

        deadline = time.monotonic() + CONNECT_TIMEOUT_SEC
        attempt = 0
        while time.monotonic() < deadline:
            time.sleep(_BACKOFF_SEC[min(attempt, len(_BACKOFF_SEC) - 1)])
            attempt += 1
            try:
                self._transport = self._open_transport()
                return
            except OSError:
                continue

        raise ConnectionError(
            f"no MCP bridge on {SERVER_NAME} after {CONNECT_TIMEOUT_SEC:.0f}s. {launched_msg}".strip()
        )

    def close(self) -> None:
        if self._transport is not None:
            try:
                self._transport.close()
            except OSError:
                pass
            self._transport = None
        self._buffer = b""

    def call(self, method: str, params: dict[str, Any] | None = None) -> Any:
        """Issue one request and return its `result`. Reconnects once if the editor
        was restarted since the last call."""
        try:
            return self._call_once(method, params)
        except (OSError, EOFError):
            self.close()
            self.connect()
            return self._call_once(method, params)

    def _call_once(self, method: str, params: dict[str, Any] | None) -> Any:
        self.connect()
        assert self._transport is not None

        request_id = self._next_id
        self._next_id += 1
        request = {
            "jsonrpc": "2.0",
            "id": request_id,
            "method": method,
            "params": params or {},
        }
        self._transport.send(json.dumps(request).encode("utf-8") + b"\n")

        response = json.loads(self._read_line())
        if "error" in response:
            error = response["error"]
            raise EditorRpcError(error.get("code", 0), error.get("message", "unknown error"))
        return response.get("result")

    def _read_line(self) -> bytes:
        while b"\n" not in self._buffer:
            chunk = self._transport.read_some()  # type: ignore[union-attr]
            if not chunk:
                raise EOFError("editor closed the connection")
            self._buffer += chunk
        line, _, self._buffer = self._buffer.partition(b"\n")
        return line
