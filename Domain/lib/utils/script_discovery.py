import os
import sys
import importlib
import inspect


# Source mtime of each module the last time discovery imported or reloaded it.
# Discovery runs on demand (opening the inspector's script picker, resolving a
# dropped .py), so a module already in sys.modules would otherwise be reported
# from whatever was on disk the first time it was imported -- an added or renamed
# class would never show up in the picker.
_SEEN_MTIMES = {}


def discover_scripts(scripts_dir):
    """Enumerate every user script class available under *scripts_dir*.

    Scans the folder for ``.py`` files, imports each by its file stem (the same
    module name the engine uses to instantiate a ScriptComponent), and collects
    the ``ScriptableComponent`` subclasses **defined in** that file.

    Returns a list of dicts::

        [{"module": "PlayerPaddle", "class": "PlayerPaddle"}, ...]

    A class is included only when it is authored in that module
    (``cls.__module__ == <stem>``) — re-imported bases (e.g. an imported
    ``ScriptableComponent`` or a shared base pulled in with ``from ... import``)
    are skipped. A broken script is logged and skipped, never fatal.

    Files created after the interpreter started are picked up too: the import
    system caches each ``sys.path`` directory's listing, and
    ``invalidate_caches()`` is the documented way to make a brand-new module
    importable without restarting.
    """
    # discover_scripts may run before any ScriptComponent has been instantiated,
    # so the scripts folder is not guaranteed to be on sys.path yet — add it.
    if scripts_dir and scripts_dir not in sys.path:
        sys.path.append(scripts_dir)

    importlib.invalidate_caches()

    from Domain.lib.api.components.scriptable_component_handler import ScriptableComponent

    results = []
    try:
        entries = sorted(os.listdir(scripts_dir))
    except OSError as e:
        print(f"[script_discovery] Cannot list '{scripts_dir}': {e}", file=sys.stderr)
        return results

    for filename in entries:
        if not filename.endswith(".py") or filename.startswith("__"):
            continue
        stem = filename[:-3]

        try:
            mtime = os.path.getmtime(os.path.join(scripts_dir, filename))
        except OSError:
            mtime = None

        try:
            module = sys.modules.get(stem)
            if module is None:
                module = importlib.import_module(stem)
            elif mtime is not None and _SEEN_MTIMES.get(stem) != mtime:
                # Edited since we last looked. Reloading is safe here: live
                # ScriptComponents re-resolve their class from the module on every
                # hot-reload rather than holding the class object, and the engine
                # never type-checks instances against it.
                module = importlib.reload(module)
        except Exception as e:
            print(f"[script_discovery] Skipping '{filename}': {e}", file=sys.stderr)
            continue

        _SEEN_MTIMES[stem] = mtime

        for name, obj in inspect.getmembers(module, inspect.isclass):
            if (issubclass(obj, ScriptableComponent)
                    and obj is not ScriptableComponent
                    and obj.__module__ == stem):
                results.append({"module": stem, "class": name})

    return results
