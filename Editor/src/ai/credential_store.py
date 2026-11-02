"""Native credential-vault access for the RockEngine AI assistant.

The Qt layer calls these functions through pybind11. Secrets are stored in the
current user's Windows Credential Manager, macOS Keychain, or Linux Secret
Service. There is deliberately no plaintext fallback.
"""

from __future__ import annotations

import ctypes
import ctypes.util
import argparse
import shutil
import subprocess
import sys
from ctypes import wintypes

SERVICE = "RockEngineEditor.AiAssistant"


class CredentialStoreError(RuntimeError):
    pass


def is_available() -> tuple[bool, str]:
    if sys.platform in {"win32", "darwin"}:
        return True, ""
    if shutil.which("secret-tool"):
        return True, ""
    return (
        False,
        "Secret Service is unavailable (install secret-tool/libsecret to store API keys)",
    )


def store(account: str, secret: bytes) -> None:
    if not secret:
        raise CredentialStoreError("API key is empty")
    if sys.platform == "win32":
        _windows_store(account, secret)
    elif sys.platform == "darwin":
        _mac_store(account, secret)
    else:
        _secret_tool_store(account, secret)


def load(account: str) -> bytes | None:
    if sys.platform == "win32":
        return _windows_load(account)
    if sys.platform == "darwin":
        return _mac_load(account)
    return _secret_tool_load(account)


def remove(account: str) -> None:
    if sys.platform == "win32":
        _windows_remove(account)
    elif sys.platform == "darwin":
        _mac_remove(account)
    else:
        _secret_tool_remove(account)


# Windows Credential Manager -------------------------------------------------------

if sys.platform == "win32":
    CRED_TYPE_GENERIC = 1
    CRED_PERSIST_LOCAL_MACHINE = 2
    ERROR_NOT_FOUND = 1168

    class _CREDENTIALW(ctypes.Structure):
        _fields_ = [
            ("Flags", wintypes.DWORD),
            ("Type", wintypes.DWORD),
            ("TargetName", wintypes.LPWSTR),
            ("Comment", wintypes.LPWSTR),
            ("LastWritten", wintypes.FILETIME),
            ("CredentialBlobSize", wintypes.DWORD),
            ("CredentialBlob", ctypes.POINTER(ctypes.c_ubyte)),
            ("Persist", wintypes.DWORD),
            ("AttributeCount", wintypes.DWORD),
            ("Attributes", ctypes.c_void_p),
            ("TargetAlias", wintypes.LPWSTR),
            ("UserName", wintypes.LPWSTR),
        ]

    _PCREDENTIALW = ctypes.POINTER(_CREDENTIALW)
    _advapi32 = ctypes.WinDLL("Advapi32.dll", use_last_error=True)
    _advapi32.CredWriteW.argtypes = [ctypes.POINTER(_CREDENTIALW), wintypes.DWORD]
    _advapi32.CredWriteW.restype = wintypes.BOOL
    _advapi32.CredReadW.argtypes = [
        wintypes.LPCWSTR,
        wintypes.DWORD,
        wintypes.DWORD,
        ctypes.POINTER(_PCREDENTIALW),
    ]
    _advapi32.CredReadW.restype = wintypes.BOOL
    _advapi32.CredDeleteW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD]
    _advapi32.CredDeleteW.restype = wintypes.BOOL
    _advapi32.CredFree.argtypes = [ctypes.c_void_p]


def _target(account: str) -> str:
    return f"{SERVICE}/{account}"


def _windows_error(action: str) -> CredentialStoreError:
    code = ctypes.get_last_error()
    return CredentialStoreError(f"{action}: {ctypes.FormatError(code).strip()} ({code})")


def _windows_store(account: str, secret: bytes) -> None:
    blob = (ctypes.c_ubyte * len(secret)).from_buffer_copy(secret)
    credential = _CREDENTIALW()
    credential.Type = CRED_TYPE_GENERIC
    credential.TargetName = _target(account)
    credential.CredentialBlobSize = len(secret)
    credential.CredentialBlob = ctypes.cast(blob, ctypes.POINTER(ctypes.c_ubyte))
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE
    credential.UserName = account
    if not _advapi32.CredWriteW(ctypes.byref(credential), 0):
        raise _windows_error("Credential Manager write failed")


def _windows_load(account: str) -> bytes | None:
    credential = _PCREDENTIALW()
    if not _advapi32.CredReadW(_target(account), CRED_TYPE_GENERIC, 0, ctypes.byref(credential)):
        if ctypes.get_last_error() == ERROR_NOT_FOUND:
            return None
        raise _windows_error("Credential Manager read failed")
    try:
        return ctypes.string_at(
            credential.contents.CredentialBlob,
            credential.contents.CredentialBlobSize,
        )
    finally:
        _advapi32.CredFree(credential)


def _windows_remove(account: str) -> None:
    if _advapi32.CredDeleteW(_target(account), CRED_TYPE_GENERIC, 0):
        return
    if ctypes.get_last_error() != ERROR_NOT_FOUND:
        raise _windows_error("Credential Manager delete failed")


# macOS Keychain ------------------------------------------------------------------

if sys.platform == "darwin":
    _security_path = ctypes.util.find_library("Security")
    _core_foundation_path = ctypes.util.find_library("CoreFoundation")
    if not _security_path or not _core_foundation_path:
        raise ImportError("macOS Security framework is unavailable")
    _security = ctypes.CDLL(_security_path)
    _core_foundation = ctypes.CDLL(_core_foundation_path)

    _security.SecKeychainFindGenericPassword.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint32,
        ctypes.c_char_p,
        ctypes.c_uint32,
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_uint32),
        ctypes.POINTER(ctypes.c_void_p),
        ctypes.POINTER(ctypes.c_void_p),
    ]
    _security.SecKeychainFindGenericPassword.restype = ctypes.c_int32
    _security.SecKeychainAddGenericPassword.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint32,
        ctypes.c_char_p,
        ctypes.c_uint32,
        ctypes.c_char_p,
        ctypes.c_uint32,
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_void_p),
    ]
    _security.SecKeychainAddGenericPassword.restype = ctypes.c_int32
    _security.SecKeychainItemModifyAttributesAndData.argtypes = [
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_uint32,
        ctypes.c_void_p,
    ]
    _security.SecKeychainItemModifyAttributesAndData.restype = ctypes.c_int32
    _security.SecKeychainItemDelete.argtypes = [ctypes.c_void_p]
    _security.SecKeychainItemDelete.restype = ctypes.c_int32
    _security.SecKeychainItemFreeContent.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    _core_foundation.CFRelease.argtypes = [ctypes.c_void_p]

    _ERR_SEC_SUCCESS = 0
    _ERR_SEC_ITEM_NOT_FOUND = -25300


def _mac_find(account: str) -> tuple[int, ctypes.c_void_p, ctypes.c_void_p, int]:
    service = SERVICE.encode()
    account_bytes = account.encode()
    length = ctypes.c_uint32()
    data = ctypes.c_void_p()
    item = ctypes.c_void_p()
    status = _security.SecKeychainFindGenericPassword(
        None,
        len(service),
        service,
        len(account_bytes),
        account_bytes,
        ctypes.byref(length),
        ctypes.byref(data),
        ctypes.byref(item),
    )
    return status, data, item, length.value


def _mac_check(status: int, action: str) -> None:
    if status != _ERR_SEC_SUCCESS:
        raise CredentialStoreError(f"{action} failed with macOS status {status}")


def _mac_store(account: str, secret: bytes) -> None:
    status, old_data, item, _ = _mac_find(account)
    try:
        if status == _ERR_SEC_SUCCESS:
            buffer = ctypes.create_string_buffer(secret)
            _mac_check(
                _security.SecKeychainItemModifyAttributesAndData(
                    item, None, len(secret), ctypes.cast(buffer, ctypes.c_void_p)
                ),
                "Keychain update",
            )
            return
        if status != _ERR_SEC_ITEM_NOT_FOUND:
            _mac_check(status, "Keychain lookup")
        service = SERVICE.encode()
        account_bytes = account.encode()
        buffer = ctypes.create_string_buffer(secret)
        _mac_check(
            _security.SecKeychainAddGenericPassword(
                None,
                len(service),
                service,
                len(account_bytes),
                account_bytes,
                len(secret),
                ctypes.cast(buffer, ctypes.c_void_p),
                None,
            ),
            "Keychain write",
        )
    finally:
        if old_data:
            _security.SecKeychainItemFreeContent(None, old_data)
        if item:
            _core_foundation.CFRelease(item)


def _mac_load(account: str) -> bytes | None:
    status, data, item, length = _mac_find(account)
    if status == _ERR_SEC_ITEM_NOT_FOUND:
        return None
    _mac_check(status, "Keychain read")
    try:
        return ctypes.string_at(data, length)
    finally:
        if data:
            _security.SecKeychainItemFreeContent(None, data)
        if item:
            _core_foundation.CFRelease(item)


def _mac_remove(account: str) -> None:
    status, data, item, _ = _mac_find(account)
    if status == _ERR_SEC_ITEM_NOT_FOUND:
        return
    _mac_check(status, "Keychain lookup")
    try:
        _mac_check(_security.SecKeychainItemDelete(item), "Keychain delete")
    finally:
        if data:
            _security.SecKeychainItemFreeContent(None, data)
        if item:
            _core_foundation.CFRelease(item)


# Linux Secret Service -------------------------------------------------------------

def _secret_tool() -> str:
    executable = shutil.which("secret-tool")
    if not executable:
        raise CredentialStoreError(
            "Secret Service is unavailable (install secret-tool/libsecret)"
        )
    return executable


def _secret_args(account: str) -> list[str]:
    return ["application", "RockEngineEditor", "provider", account]


def _secret_tool_store(account: str, secret: bytes) -> None:
    result = subprocess.run(
        [_secret_tool(), "store", "--label=RockEngine AI credential", *_secret_args(account)],
        input=secret + b"\n",
        capture_output=True,
        check=False,
        timeout=10,
    )
    if result.returncode:
        raise CredentialStoreError(
            result.stderr.decode(errors="replace").strip() or "secret-tool store failed"
        )


def _secret_tool_load(account: str) -> bytes | None:
    result = subprocess.run(
        [_secret_tool(), "lookup", *_secret_args(account)],
        capture_output=True,
        check=False,
        timeout=10,
    )
    if result.returncode:
        return None
    return result.stdout.rstrip(b"\r\n") or None


def _secret_tool_remove(account: str) -> None:
    # A non-zero result means there was no matching item, which already satisfies
    # the requested postcondition.
    subprocess.run(
        [_secret_tool(), "clear", *_secret_args(account)],
        capture_output=True,
        check=False,
        timeout=10,
    )


def _main() -> int:
    parser = argparse.ArgumentParser(description="Read a RockEngine AI credential")
    parser.add_argument("command", choices=["get"])
    parser.add_argument("account")
    args = parser.parse_args()
    value = load(args.account)
    if not value:
        print("credential not found", file=sys.stderr)
        return 1
    sys.stdout.buffer.write(value + b"\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(_main())
