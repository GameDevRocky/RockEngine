import typing


def _base_type(hint):
    """Unwrap Annotated[T, ...] → T, or return hint unchanged."""
    if typing.get_origin(hint) is typing.Annotated:
        return typing.get_args(hint)[0]
    return hint


def _type_compatible(value, hint) -> bool:
    """Return True if *value* is type-compatible with the annotated type *hint*.

    Rules:
    - bool   : only an actual bool is accepted (bool subclasses int in Python).
    - int    : int accepted, but not bool.
    - float  : float or int accepted (widening is safe).
    - str    : only str.
    - Other  : if value is already an instance of the base type → OK.
               if value is a str → OK (ref fields are stored as string IDs by the engine).
               anything else → not compatible.
    """
    base = _base_type(hint)

    if base is bool:
        return type(value) is bool
    if base is int:
        return isinstance(value, int) and not isinstance(value, bool)
    if base is float:
        return isinstance(value, (float, int)) and not isinstance(value, bool)
    if base is str:
        return isinstance(value, str)

    # Vector types, Material, Sprite, custom script classes:
    # the engine stores ref types as plain str IDs, so a str value is always
    # compatible.  A native pybind11 object (e.g. a Vector2 instance) is also
    # compatible if it matches the base type.
    return isinstance(value, base) or isinstance(value, str)


def transfer_fields(old_instance, new_instance) -> None:
    """Copy state from old_instance to new_instance after a hot-reload.

    Priority rules (highest to lowest):
    1. New class's type  — if the annotation type changed, the new default wins.
    2. New class's value — if the stored value equals the old class-level default
                           the source was likely just edited; let the new default win.
    3. Explicitly-set values — inspector overrides and runtime assignments survive
                               the reload unchanged.

    Raises on critical failure (e.g. cannot resolve type hints for the new class)
    so the caller can perform a clean hard-stop instead of reloading with broken state.
    """
    # Resolving new hints is critical.  Any NameError / TypeError here means the
    # script has an unresolvable annotation — raise immediately so ApplyHotReload
    # can abort cleanly rather than proceeding with an incomplete transfer.
    new_hints = typing.get_type_hints(type(new_instance), include_extras=True)

    # Old hints are needed for Pass 2; failure is non-fatal.
    try:
        old_hints = typing.get_type_hints(type(old_instance), include_extras=True)
    except Exception:
        old_hints = {}

    old_instance_vars = vars(old_instance)
    old_class = type(old_instance)

    # ------------------------------------------------------------------
    # Pass 1 — annotated (inspector-facing) fields.
    # ------------------------------------------------------------------
    for name in new_hints:
        if name.startswith('_'):
            continue

        # Only consider values that were explicitly set as instance attributes.
        # Class-level defaults (e.g. `speed: str = "400"`) are NOT in vars(), so
        # changing a default in the source automatically takes effect.
        if name not in old_instance_vars:
            continue

        old_value = old_instance_vars[name]

        # Value equals the old class-level default → source probably changed the
        # default; let the new class win.
        old_class_default = old_class.__dict__.get(name)
        if old_class_default is not None and old_value == old_class_default:
            continue

        # Always transfer the existing value — even if its runtime type no longer
        # matches the new hint. The actual value wins; the inspector will
        # re-derive the displayed type from the runtime value.
        try:
            setattr(new_instance, name, old_value)
        except Exception as e:
            print(f"[transfer_fields] Could not set '{name}': {e}")

    # ------------------------------------------------------------------
    # Pass 2 — runtime instance attributes (set in init / awake / start).
    # ------------------------------------------------------------------
    for name, value in old_instance_vars.items():
        if name.startswith('_'):
            continue
        # Skip annotated fields; already handled above.
        if name in new_hints or name in old_hints:
            continue
        try:
            setattr(new_instance, name, value)
        except Exception as e:
            print(f"[transfer_fields] Could not set runtime attr '{name}': {e}")

