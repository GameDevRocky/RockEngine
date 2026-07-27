from rock_engine.core import gameobject_module
from rock_engine.systems import scene_module


class Scene:
    @staticmethod
    def save(source, allow_in_play_mode: bool = True) -> bool:
        """Persist the scene containing `source` to the .scene file it was loaded from.

        `source` is any handler with an `.id` (a GameObject, or any component/
        ScriptableComponent — its owning GameObject's scene is used), or a raw
        GameObject id string.

        Normally scenes only save from Editor mode: Play mode runs a throwaway
        deep-copy of the editor world (see Engine's Play-mode invariant), so
        saving it would overwrite the authored scene with simulated state. Set
        `allow_in_play_mode=True` (the default) to explicitly opt out of that
        guard from a script — e.g. an in-game level painter that draws tiles at
        runtime and wants that layout written to disk. Set it False to restore
        the Editor-only behavior.

        Returns False if `source` isn't attached to a loaded scene (or the scene
        has no file path yet); True once the save call has been issued.
        """
        obj_id = getattr(source, "id", source)
        if not obj_id:
            return False
        scene_id = gameobject_module.get_scene_id(obj_id)
        if not scene_id:
            return False
        return bool(scene_module.save_scene(scene_id, bool(allow_in_play_mode)))
