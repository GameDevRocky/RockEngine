import typing
from .properties import Range, Step, Tooltip


def _get_ref_classes():
    """Lazily import Material, Sprite, and GameObject to avoid circular imports at module load time."""
    try:
        from Domain.lib.api.rendering.material_handler import Material
        from Domain.lib.api.rendering.sprite_handler import Sprite
        from Domain.lib.api.core.gameobject_handler import GameObject
        return Material, Sprite, GameObject
    except ImportError:
        return None, None, None


def _map_type(base_type, MaterialCls, SpriteCls, GameObjectCls):
    """Map a single Python type to (type_name, ref_type_name).

    Handles scalars (float/int/bool/str), Vector2/3/4, and class refs
    (Material -> "material", Sprite -> "sprite", the base GameObject -> an
    unfiltered "gameobject:" reference, any other class -> a class-filtered
    "gameobject:<ClassName>" reference). Returns (None, "") if unmappable.
    Used for both top-level fields and the element type of list[...] fields.
    """
    type_map = {float: "float", int: "int", bool: "bool", str: "str"}
    type_name = type_map.get(base_type)
    ref_type_name = ""

    if type_name is None:
        qual = getattr(base_type, '__qualname__', '') or ''
        mod  = getattr(base_type, '__module__',  '') or ''
        if 'Vector4' in qual or 'Vector4' in mod:
            type_name = "vec4"
        elif 'Vector3' in qual or 'Vector3' in mod:
            type_name = "vec3"
        elif 'Vector2' in qual or 'Vector2' in mod:
            type_name = "vec2"

    if type_name is None and isinstance(base_type, type):
        type_name = "str"
        if MaterialCls and base_type is MaterialCls:
            ref_type_name = "material"
        elif SpriteCls and base_type is SpriteCls:
            ref_type_name = "sprite"
        elif GameObjectCls and base_type is GameObjectCls:
            # Base GameObject → reference to ANY object (no script-class filter).
            ref_type_name = "gameobject:"
        else:
            # A specific user script subclass → filter the picker to that class.
            ref_type_name = f"gameobject:{base_type.__name__}"

    return type_name, ref_type_name


def get_exposed_fields(cls):
    """Introspect a ScriptableComponent subclass for editor-exposed fields.

    Returns a list of dicts with keys:
        name, type_name, default, min, max, step, tooltip, ref_type_name

    Only class-level annotated fields are returned. Fields starting with '_' are excluded.

    Supported type annotations::

        speed: float = 10.0
        lives: int = 3
        active: bool = True
        label: str = "hello"
        offset: Vector2
        skin: Material          # material asset picker in inspector
        icon: Sprite            # sprite asset picker in inspector
        target: Enemy           # GameObject picker filtered to Enemy script
    """
    try:
        hints = typing.get_type_hints(cls, include_extras=True)
    except Exception:
        return []

    MaterialCls, SpriteCls, GameObjectCls = _get_ref_classes()

    fields = []
    for name, hint in hints.items():
        if name.startswith('_'):
            continue

        # ---- Resolve default value ----
        default = None
        has_default = False
        for klass in cls.__mro__:
            if name in klass.__dict__:
                default = klass.__dict__[name]
                has_default = True
                break

        if not has_default:
            base_type_check = hint
            if typing.get_origin(hint) is typing.Annotated:
                base_type_check = typing.get_args(hint)[0]

            # list[...] fields with no class-level default → empty list
            if typing.get_origin(base_type_check) is list:
                default = []
                has_default = True

        if not has_default:
            from .re_math import Vector2, Vector3, Vector4
            fallbacks = {float: 0.0, int: 0, bool: False, str: ""}
            default = fallbacks.get(base_type_check)

            if default is None:
                qual = getattr(base_type_check, '__qualname__', '') or ''
                if 'Vector4' in qual:
                    default = Vector4()
                elif 'Vector3' in qual:
                    default = Vector3()
                elif 'Vector2' in qual:
                    default = Vector2()

            # Material, Sprite, and any other class type (custom scripts) default to empty ID
            if default is None and isinstance(base_type_check, type):
                default = ""

            if default is None:
                continue

        # ---- Unpack Annotated[T, metadata...] or use the raw type ----
        base_type = hint
        metadata = []
        if typing.get_origin(hint) is typing.Annotated:
            args = typing.get_args(hint)
            base_type = args[0]
            metadata = list(args[1:])

        # ---- Map type to engine type name ----
        field_ref_type_name = ""
        element_type_name = ""
        element_ref_type_name = ""

        if typing.get_origin(base_type) is list:
            # list[T] field — map the element type. Nested lists are unsupported.
            list_args = typing.get_args(base_type)
            if not list_args:
                continue  # bare `list` annotation is ambiguous — skip
            element_type_name, element_ref_type_name = _map_type(
                list_args[0], MaterialCls, SpriteCls, GameObjectCls)
            if element_type_name is None or element_type_name == "list":
                continue  # unmappable or nested list element
            type_name = "list"
            if not isinstance(default, list):
                default = []
        else:
            type_name, field_ref_type_name = _map_type(base_type, MaterialCls, SpriteCls, GameObjectCls)

            # Normalize ref-field default: handler instance → its ID string; else ""
            if field_ref_type_name:
                if MaterialCls and isinstance(default, MaterialCls):
                    default = default.id
                elif SpriteCls and isinstance(default, SpriteCls):
                    default = default.id
                elif not isinstance(default, str):
                    default = ""

        if type_name is None:
            continue

        # ---- Extract optional metadata (Range / Step / Tooltip) ----
        field_min     = None
        field_max     = None
        field_step    = 0.1
        field_tooltip = ""

        for m in metadata:
            if isinstance(m, Range):
                field_min = m.min
                field_max = m.max
            elif isinstance(m, Step):
                field_step = m.value
            elif isinstance(m, Tooltip):
                field_tooltip = m.text

        fields.append({
            "name":                  name,
            "type_name":             type_name,
            "default":               default,
            "min":                   field_min,
            "max":                   field_max,
            "step":                  field_step,
            "tooltip":               field_tooltip,
            "ref_type_name":         field_ref_type_name,
            "element_type_name":     element_type_name,
            "element_ref_type_name": element_ref_type_name,
        })

    return fields
