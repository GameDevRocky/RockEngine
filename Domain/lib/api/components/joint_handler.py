from __future__ import annotations

from rock_engine.components import joint_module
from ...utils.re_math import Vector2
from ..core.gameobject_handler import get_gameobject, GameObject
from .component_handler import Component


class Joint(Component):
    """Base for every Box2D joint handler.

    Unlike Rigidbody/Collider handlers, joints are addressed by their OWN
    component id rather than the GameObject id: one body may be constrained to
    several others at once, so an object id cannot say which joint you mean.
    That is what `_addressed_by_component_id` tells GameObject.add_component /
    get_components, so they hand each handler its specific instance.

    Use `gameobject.get_components(DistanceJoint)` to reach every joint of a type;
    `get_component` returns only the first.
    """

    _addressed_by_component_id = True

    def __init__(self, obj_id=None, component_id=None):
        super().__init__(obj_id, component_id)

    @property
    def connected_body(self) -> GameObject:
        obj_id = joint_module.get_connected_body(self._component_id)
        return get_gameobject(obj_id) if obj_id else None

    @connected_body.setter
    def connected_body(self, value):
        # Accepts a GameObject or a raw id, so scripts can pass either.
        obj_id = value.id if isinstance(value, GameObject) else (value or "")
        joint_module.set_connected_body(self._component_id, str(obj_id))

    @property
    def collide_connected(self) -> bool:
        return joint_module.get_collide_connected(self._component_id)

    @collide_connected.setter
    def collide_connected(self, value: bool):
        joint_module.set_collide_connected(self._component_id, bool(value))

    @property
    def local_anchor_a(self) -> Vector2:
        return Vector2(joint_module.get_local_anchor_a(self._component_id))

    @local_anchor_a.setter
    def local_anchor_a(self, value: Vector2):
        value = Vector2(value)
        joint_module.set_local_anchor_a(self._component_id, value.x, value.y)

    @property
    def local_anchor_b(self) -> Vector2:
        return Vector2(joint_module.get_local_anchor_b(self._component_id))

    @local_anchor_b.setter
    def local_anchor_b(self, value: Vector2):
        value = Vector2(value)
        joint_module.set_local_anchor_b(self._component_id, value.x, value.y)

    @property
    def is_built(self) -> bool:
        """True once both bodies resolved and the Box2D joint actually exists.

        False means either the joint is misconfigured (no connected body, or a
        body without a Rigidbody) or the scene is not in play mode yet.
        """
        return joint_module.is_built(self._component_id)
