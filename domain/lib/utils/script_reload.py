import typing


def transfer_fields(old_instance, new_instance) -> None:
    """Copy state from old_instance to new_instance after a hot-reload.

    Two passes:
    1. Annotated fields — new class is the authority.  Values are transferred
       if the old instance had them (covers both class-level defaults and
       inspector-set overrides).
    2. Runtime instance attributes — attributes set dynamically in init /
       awake / start (e.g. sprite_renderer, rb, grounded).  These are copied
       verbatim so the component continues running without re-initialising.
       Attributes that were annotated in either class are skipped here to
       avoid double-handling.
    """
    new_hints = typing.get_type_hints(type(new_instance), include_extras=True)
    old_hints = typing.get_type_hints(type(old_instance), include_extras=True)

    # Pass 1 — annotated fields
    for name in new_hints:
        if name.startswith('_'):
            continue
        if hasattr(old_instance, name):
            try:
                setattr(new_instance, name, getattr(old_instance, name))
            except Exception:
                pass

    # Pass 2 — runtime instance attributes (init / awake / start state)
    for name, value in vars(old_instance).items():
        if name.startswith('_'):
            continue
        # Skip fields already covered by the annotation pass
        if name in new_hints or name in old_hints:
            continue
        try:
            setattr(new_instance, name, value)
        except Exception:
            pass
