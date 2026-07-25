from .component_handler import Component
from .collider_handler import Collider
from ..systems.time_system import Time
from rock_engine.core import gameobject_module

_TICK_METHODS = ('update', 'fixed_update', 'late_update')


def _coroutine_only_update(self):
    self._tick_coroutines()


class ScriptableComponent(Component):
    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        for method_name in _TICK_METHODS:
            if method_name in cls.__dict__:
                original = cls.__dict__[method_name]
                def _make_wrapper(orig, mn=method_name):
                    def _wrapper(self):
                        self._tick_coroutines()
                        orig(self)
                    _wrapper.__name__ = mn
                    return _wrapper
                setattr(cls, method_name, _make_wrapper(original))
        # A subclass with no tick method anywhere in its hierarchy still needs
        # its coroutines ticked. Inject the minimal update at class-definition
        # time (not lazily in start_coroutine): the engine caches each script's
        # lifecycle methods when it instantiates it, so a method that appears
        # later would never be called.
        if not any(
            m in klass.__dict__
            for klass in cls.__mro__
            if klass is not ScriptableComponent and klass is not object
            for m in _TICK_METHODS
        ):
            cls.update = _coroutine_only_update

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
        # Coroutine-only classes get their minimal update injected at class
        # definition time (see __init_subclass__) — nothing to patch here.

    def stop_all_coroutines(self):
        """Cancel all running coroutines on this component."""
        self._coroutines.clear()

    def _tick_coroutines(self):
        if not self._coroutines:
            return  # nothing to tick — skip the Time.delta_time engine call
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
        