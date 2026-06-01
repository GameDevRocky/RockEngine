import typing
from .properties import Range, Step, Tooltip


def _get_ref_classes():
    """Lazily import Material and Sprite to avoid circular imports at module load time."""
    try:
        from Domain.lib.api.rendering.material_handler import Material
        from Domain.lib.api.rendering.sprite_handler import Sprite
        return Material, Sprite
    except ImportError:
        return None, None


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

    MaterialCls, SpriteCls = _get_ref_classes()

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
        type_map = {float: "float", int: "int", bool: "bool", str: "str"}
        type_name = type_map.get(base_type)
        field_ref_type_name = ""

        if type_name is None:
            qual = getattr(base_type, '__qualname__', '') or ''
            mod  = getattr(base_type, '__module__',  '') or ''
            if 'Vector4' in qual or 'Vector4' in mod:
                type_name = "vec4"
            elif 'Vector3' in qual or 'Vector3' in mod:
                type_name = "vec3"
            elif 'Vector2' in qual or 'Vector2' in mod:
                type_name = "vec2"

        # ---- Ref types: Material, Sprite, or any other class = script/GO ref ----
        if type_name is None and isinstance(base_type, type):
            type_name = "str"
            if MaterialCls and base_type is MaterialCls:
                field_ref_type_name = "material"
            elif SpriteCls and base_type is SpriteCls:
                field_ref_type_name = "sprite"
            else:
                # Any user-defined class is treated as a GameObject reference
                # filtered to GameObjects that have that script attached.
                field_ref_type_name = f"gameobject:{base_type.__name__}"

            # Normalize default: handler instance → its ID string; None/anything else → ""
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
            "name":          name,
            "type_name":     type_name,
            "default":       default,
            "min":           field_min,
            "max":           field_max,
            "step":          field_step,
            "tooltip":       field_tooltip,
            "ref_type_name": field_ref_type_name,
        })

    return fields
