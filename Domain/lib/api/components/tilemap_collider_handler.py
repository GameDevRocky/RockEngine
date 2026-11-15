from .collider_handler import Collider


class TileMapCollider(Collider):
    _type_name = "TileMapCollider"
    __slots__ = ()

    def __init__(self, obj_id=None):
        super().__init__(obj_id)
