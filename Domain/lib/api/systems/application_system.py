from rock_engine.systems import application_module


class _Application:
    """Which kind of process the game is running inside.

    Mirrors Unity's `Application.isEditor`. Use it to gate anything that should exist
    while you are working but must not ship: debug cheats, dev overlays, test spawners.

    This is NOT "am I in play mode" -- scripts only run in play mode anyway. It is
    "am I inside the editor, or inside a build someone downloaded".

        if Application.is_editor:
            self.spawn_debug_grid()
    """

    @property
    def is_editor(self) -> bool:
        return bool(application_module.is_editor())

    @property
    def is_player(self) -> bool:
        return bool(application_module.is_player())


Application = _Application()
