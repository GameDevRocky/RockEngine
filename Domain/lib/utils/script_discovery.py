import os
import sys
import importlib
import inspect


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
    """
    # discover_scripts may run before any ScriptComponent has been instantiated,
    # so the scripts folder is not guaranteed to be on sys.path yet — add it.
    if scripts_dir and scripts_dir not in sys.path:
        sys.path.append(scripts_dir)

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
            module = importlib.import_module(stem)
        except Exception as e:
            print(f"[script_discovery] Skipping '{filename}': {e}", file=sys.stderr)
            continue

        for name, obj in inspect.getmembers(module, inspect.isclass):
            if (issubclass(obj, ScriptableComponent)
                    and obj is not ScriptableComponent
                    and obj.__module__ == stem):
                results.append({"module": stem, "class": name})

    return results
