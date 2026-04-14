import typing
from .properties import Range, Step, Tooltip


def get_exposed_fields(cls):
    """Introspect a ScriptableComponent subclass for editor-exposed fields.
    
    Returns a list of dicts with keys:
        name, type_name, default, min, max, step, tooltip
    
    Only class-level annotated fields with a default value are returned.
    Fields starting with '_' are excluded.
    """
    try:
        hints = typing.get_type_hints(cls, include_extras=True)
    except Exception:
        return []

    fields = []
    for name, hint in hints.items():
        if name.startswith('_'):
            continue

        # Get default value from class hierarchy
        default = None
        has_default = False
        for klass in cls.__mro__:
            if name in klass.__dict__:
                default = klass.__dict__[name]
                has_default = True
                break

        if not has_default:
            # Unpack type early to determine fallback default
            base_type_check = hint
            if typing.get_origin(hint) is typing.Annotated:
                base_type_check = typing.get_args(hint)[0]

            from .re_math import Vector2, Vector3, Vector4
            fallbacks = {
                float: 0.0,
                int: 0,
                bool: False,
                str: "",
            }
            default = fallbacks.get(base_type_check)
            if default is None:
                qual = getattr(base_type_check, '__qualname__', '') or ''
                if 'Vector4' in qual:
                    default = Vector4()
                elif 'Vector3' in qual:
                    default = Vector3()
                elif 'Vector2' in qual:
                    default = Vector2()

            if default is None:
                continue

        # Unpack Annotated[T, ...] or use raw type
        base_type = hint
        metadata = []
        if typing.get_origin(hint) is typing.Annotated:
            args = typing.get_args(hint)
            base_type = args[0]
            metadata = list(args[1:])

        # Map Python type to engine type name
        type_map = {
            float: "float",
            int: "int",
            bool: "bool",
            str: "str",
        }
        type_name = type_map.get(base_type)

        # Check for Vector2
        if type_name is None:
            qual = getattr(base_type, '__qualname__', '') or ''
            mod = getattr(base_type, '__module__', '') or ''
            if 'Vector4' in qual or 'Vector4' in mod:
                type_name = "vec4"
            elif 'Vector3' in qual or 'Vector3' in mod:
                type_name = "vec3"
            elif 'Vector2' in qual or 'Vector2' in mod:
                type_name = "vec2"

        if type_name is None:
            continue

        # Extract metadata
        field_min = None
        field_max = None
        field_step = 0.1
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
            "name": name,
            "type_name": type_name,
            "default": default,
            "min": field_min,
            "max": field_max,
            "step": field_step,
            "tooltip": field_tooltip,
        })

    return fields
