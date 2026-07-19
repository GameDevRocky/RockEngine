from .component_handler import Component
from .collider_handler import Collider
from rock_engine.core import gameobject_module


class ScriptableComponent(Component):
    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        for method_name in ('update', 'fixed_update', 'late_update'):
            if method_name in cls.__dict__:
                original = cls.__dict__[method_name]
                def _make_wrapper(orig, mn=method_name):
                    def _wrapper(self):
                        self._tick_coroutines()
                        orig(self)
                    _wrapper.__name__ = mn
                    return _wrapper
                setattr(cls, method_name, _make_wrapper(original))

    def __init__(self, obj_id=None):
        super().__init__(obj_id)
        self._coroutines = []  # list of [generator, instruction]

    def start_coroutine(self, gen):
        """Begin executing a generator-based coroutine."""
        try:
            instruction = next(gen)
            self._coroutines.append([gen, instruction])
        except StopIteration:
            pass
        # If the subclass has no update/fixed_update/late_update, _tick_coroutines
        # was never injected — patch in a minimal update so coroutines get ticked.
        cls = type(self)
        tick_methods = ('update', 'fixed_update', 'late_update')
        if not any(m in cls.__dict__ for m in tick_methods):
            def _coroutine_only_update(self):
                self._tick_coroutines()
            cls.update = _coroutine_only_update

    def stop_all_coroutines(self):
        """Cancel all running coroutines on this component."""
        self._coroutines.clear()

    def _tick_coroutines(self):
        from ..systems.time_system import Time
        dt = Time.delta_time
        still_running = []
        for gen, instruction in self._coroutines:
            if instruction is None or instruction.tick(dt):
                try:
                    next_instruction = next(gen)
                    still_running.append([gen, next_instruction])
                except StopIteration:
                    pass  # coroutine finished
            else:
                still_running.append([gen, instruction])
        self._coroutines = still_running

    def instantiate(self, name: str = "GameObject"):
        """Create a new GameObject in the same scene as this script's object."""
        from ..core.gameobject_handler import get_gameobject
        name = str(name)
        new_id = gameobject_module.instantiate(self._gameobject_id, name)
        if not new_id:
            return None
        return get_gameobject(new_id)

    def duplicate(self, target=None):
        """Clone a GameObject and its whole subtree, as a sibling of the original.

        Every cloned object and component gets a new id, and references between
        them are rewired to the clones; references to anything outside the
        subtree (sprites, materials, another object's Camera) still point at the
        original target. Defaults to this script's own GameObject. `target` may
        be a GameObject or any component handler on it.
        """
        from ..core.gameobject_handler import get_gameobject
        obj_id = self._gameobject_id if target is None else target.id
        new_id = gameobject_module.duplicate(obj_id)
        if not new_id:
            return None
        return get_gameobject(new_id)

    def on_collision_enter(self, other: Collider):
        pass
    def on_collision_exit(self, other: Collider):
        pass
    def on_trigger_enter(self, other: Collider):
        pass
    def on_trigger_exit(self, other: Collider):
        pass
    def on_shutdown(self):
        pass

    def handle_collision_enter(self, id):
        other = Collider(id)
        self.on_collision_enter(other)
       
    def handle_collision_exit(self, id):
        other = Collider(id)
        self.on_collision_exit(other)
        
    def handle_trigger_enter(self, id):
        other = Collider(id)
        self.on_trigger_enter(other)
        
    def handle_trigger_exit(self, id ):
        other = Collider(id)
        self.on_trigger_exit(other)
        