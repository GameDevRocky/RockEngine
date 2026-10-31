from rock_engine.components import chain_collider_module
from ...utils.re_math import Vector2
from .collider_handler import Collider


class ChainCollider(Collider):
    _type_name = "ChainCollider"
    _addressed_by_component_id = True
    __slots__ = ()

    def __init__(self, obj_id=None, component_id=None):
        super().__init__(obj_id)
        self._component_id = component_id

    @property
    def points(self) -> list[Vector2]:
        component_id = self._resolve_component_id()
        if not component_id:
            return []
        return [Vector2(x, y) for x, y in chain_collider_module.get_points(component_id)]

    @points.setter
    def points(self, values):
        component_id = self._resolve_component_id()
        if component_id:
            chain_collider_module.set_points(component_id, [(p.x, p.y) for p in values])

    @property
    def is_loop(self) -> bool:
        return chain_collider_module.get_is_loop(self._resolve_component_id())

    @is_loop.setter
    def is_loop(self, value: bool):
        chain_collider_module.set_is_loop(self._resolve_component_id(), bool(value))

    @property
    def reverse_side(self) -> bool:
        return chain_collider_module.get_reverse_side(self._resolve_component_id())

    @reverse_side.setter
    def reverse_side(self, value: bool):
        chain_collider_module.set_reverse_side(self._resolve_component_id(), bool(value))

    @property
    def has_valid_geometry(self) -> bool:
        return chain_collider_module.has_valid_geometry(self._resolve_component_id())

    @property
    def alpha_threshold(self) -> float:
        return chain_collider_module.get_alpha_threshold(self._resolve_component_id())

    @alpha_threshold.setter
    def alpha_threshold(self, value: float):
        chain_collider_module.set_alpha_threshold(self._resolve_component_id(), float(value))

    def generate_from_sprite(self, alpha_threshold: float | None = None) -> bool:
        """Replace points with a simplified outline of the sprite's visible alpha."""
        component_id = self._resolve_component_id()
        if alpha_threshold is None:
            alpha_threshold = self.alpha_threshold
        return bool(component_id and chain_collider_module.generate_from_sprite(
            component_id, float(alpha_threshold)))
