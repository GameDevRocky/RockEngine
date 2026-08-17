from rock_engine.components import script_module


class ScriptRef:
    """A live reference to a script running on another GameObject.

    Produced for a field annotated with a script class::

        class PlayerController(ScriptableComponent):
            manager: GameManager        # picks a GameObject in the inspector

            def update(self):
                self.manager.add_score(10)      # calls straight through
                if self.manager.game_over: ...

    Every attribute goes through to the live instance, so this behaves like the
    script itself. It is a proxy rather than the instance because the instance is
    **replaced** on every hot-reload: a captured reference would keep talking to a
    script object that is no longer attached to anything, and the reference would
    silently stop working the moment its target's file was saved. Resolving per
    access costs one lookup and can never go stale — the same thin-proxy-over-an-id
    shape every other handler in this API uses.

    An unassigned or destroyed target is falsy, so a reference is checked the way
    it reads::

        if self.manager:
            self.manager.add_score(10)
    """

    __slots__ = ("_gameobject_id", "_class_name")

    def __init__(self, gameobject_id, class_name):
        # Through object.__setattr__ because this class forwards __setattr__ to
        # the target, and these two are its own state rather than the script's.
        object.__setattr__(self, "_gameobject_id", gameobject_id)
        object.__setattr__(self, "_class_name", class_name)

    # ── Identity ─────────────────────────────────────────────────────────────
    @property
    def id(self):
        """The target GameObject's id.

        Defined as a real property so it resolves without touching the script,
        which is what lets a reference to a DESTROYED object still serialize —
        the engine reads `.id` off ref values when saving (see RefToIdString),
        and going through __getattr__ would raise there.
        """
        return self._gameobject_id

    @property
    def script_class(self):
        return self._class_name

    @property
    def gameobject(self):
        """The GameObject the script is attached to."""
        from ..core.gameobject_handler import get_gameobject
        return get_gameobject(self._gameobject_id)

    def resolve(self):
        """The live script instance, or None. Rarely needed — attribute access
        already goes through it — but useful to hold across a tight loop."""
        if not self._gameobject_id or not self._class_name:
            return None
        return script_module.get_script_instance(self._gameobject_id, self._class_name)

    # ── Forwarding ───────────────────────────────────────────────────────────
    def __getattr__(self, name):
        # Only called when normal lookup fails, so `id` and the methods above are
        # never routed here.
        instance = self.resolve()
        if instance is None:
            raise AttributeError(
                f"{self._class_name!r} reference is unassigned or its GameObject "
                f"was destroyed (looking up {name!r})")
        return getattr(instance, name)

    def __setattr__(self, name, value):
        instance = self.resolve()
        if instance is None:
            raise AttributeError(
                f"cannot set {name!r}: {self._class_name!r} reference is "
                f"unassigned or its GameObject was destroyed")
        setattr(instance, name, value)

    def __bool__(self):
        return self.resolve() is not None

    def __eq__(self, other):
        if isinstance(other, ScriptRef):
            return (self._gameobject_id == other._gameobject_id
                    and self._class_name == other._class_name)
        # Comparing against the instance itself should hold, so wiring a
        # reference and then testing it against a script you already have works.
        return self.resolve() is other

    def __hash__(self):
        return hash((self._gameobject_id, self._class_name))

    def __repr__(self):
        state = "live" if self else "unresolved"
        return f"<ScriptRef {self._class_name} on {self._gameobject_id or 'None'} ({state})>"
