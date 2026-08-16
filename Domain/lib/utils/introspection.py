import sys
import typing
from .properties import Range, Step, Tooltip, ReadOnly, Options, Slider, RangeSlider


def _get_ref_classes():
    """Lazily import the classes used for reference-field detection, to avoid
    circular imports at module load time. Returns
    (Material, Sprite, GameObject, Component, ScriptableComponent)."""
    try:
        from Domain.lib.api.rendering.material_handler import Material
        from Domain.lib.api.rendering.sprite_handler import Sprite
        from Domain.lib.api.core.gameobject_handler import GameObject
        from Domain.lib.api.components.component_handler import Component
        from Domain.lib.api.components.scriptable_component_handler import ScriptableComponent
        return Material, Sprite, GameObject, Component, ScriptableComponent
    except ImportError:
        return None, None, None, None, None


# Cache of engine component type name (e.g. "Camera") -> handler class, built
# once from the exported component handlers.
_COMPONENT_CLASS_BY_TYPE = None


def _component_classes_by_type():
    global _COMPONENT_CLASS_BY_TYPE
    if _COMPONENT_CLASS_BY_TYPE is None:
        _COMPONENT_CLASS_BY_TYPE = {}
        try:
            from Domain.lib.api import components as comps
            for attr in dir(comps):
                cls = getattr(comps, attr)
                type_name = isinstance(cls, type) and getattr(cls, '_type_name', None)
                if type_name:
                    _COMPONENT_CLASS_BY_TYPE[type_name] = cls
        except ImportError:
            pass
    return _COMPONENT_CLASS_BY_TYPE


def make_component_ref(gameobject_id, type_name):
    """Build a component handler of engine type `type_name` (e.g. "Camera")
    bound to `gameobject_id`, for a script's ``field : <ComponentType>``
    reference. Empty/falsy id → None (an unassigned reference). Called from the
    C++ ScriptComponent when applying a stored value to the live instance.
    """
    if not gameobject_id:
        return None
    cls = _component_classes_by_type().get(type_name)
    return cls(gameobject_id) if cls else None


def make_gameobject_ref(gameobject_id):
    """Build a GameObject handler bound to `gameobject_id`, for a script's
    ``field : GameObject`` reference. Empty/falsy id → None (an unassigned
    reference). Called from the C++ ScriptComponent when applying a stored value
    to the live instance — the gameobject analogue of ``make_component_ref``.
    """
    if not gameobject_id:
        return None
    from Domain.lib.api.core.gameobject_handler import get_gameobject
    return get_gameobject(gameobject_id)


def _unwrap_annotated(hint):
    """Split ``Reflect[T, meta...]`` into ``(T, [meta...])``.

    A bare type comes back unchanged with an empty metadata list, so callers can
    run this over anything. It is applied at two levels: the field's own
    annotation, and — for ``list[...]`` fields — the element type, because
    ``list[Reflect[Vector3, Range(0, 1)]]`` puts the metadata on the ELEMENT and
    that is the thing the inspector draws a widget for.
    """
    if typing.get_origin(hint) is typing.Annotated:
        args = typing.get_args(hint)
        return args[0], list(args[1:])
    return hint, []


def _map_type(base_type, MaterialCls, SpriteCls, GameObjectCls,
              ComponentCls=None, ScriptableComponentCls=None):
    """Map a single Python type to (type_name, ref_type_name).

    Handles scalars (float/int/bool/str), Vector2/3/4, and class refs:
    Material -> "material", Sprite -> "sprite", the base GameObject -> an
    unfiltered "gameobject:" reference, a native component handler (Camera,
    Rigidbody, a collider, ...) -> a "component:<EngineTypeName>" reference,
    any other class (a user script subclass) -> a class-filtered
    "gameobject:<ClassName>" reference. Returns (None, "") if unmappable. Used
    for both top-level fields and the element type of list[...] fields.
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
        elif (ScriptableComponentCls and issubclass(base_type, ScriptableComponentCls)):
            # A user script subclass → filter the picker to that script class.
            ref_type_name = f"gameobject:{base_type.__name__}"
        elif (ComponentCls and issubclass(base_type, ComponentCls)):
            # A built-in component handler (Camera, Rigidbody, colliders, ...) →
            # pick a GameObject that HAS this native component. Stored as the
            # object's id; resolved to a component handler at runtime.
            engine_type = getattr(base_type, '_type_name', base_type.__name__)
            ref_type_name = f"component:{engine_type}"
        else:
            # Any other class → treat the name as a script-class filter.
            ref_type_name = f"gameobject:{base_type.__name__}"

    return type_name, ref_type_name


def get_exposed_fields(cls):
    """Introspect a ScriptableComponent subclass for editor-exposed fields.

    Returns a list of dicts with keys:
        name, type_name, default, min, max, step, tooltip, read_only, widget,
        option_labels, option_values, ref_type_name

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

    ``list[T]`` of any of the above becomes a resizable list of rows, and the
    element may carry its own ``Reflect`` metadata — which is where metadata
    belongs for a list, since it is a ROW that gets a widget::

        volumes: list[Reflect[float, Range(0, 1), Slider()]]
        states:  list[Reflect[str, Options("Idle", "Run")]]

    Such element metadata is returned in the same min/max/step/widget/option
    keys a scalar field uses; ``type_name == "list"`` is what marks them as
    describing each element rather than the field.
    """
    try:
        hints = typing.get_type_hints(cls, include_extras=True)
    except Exception:
        return []

    MaterialCls, SpriteCls, GameObjectCls, ComponentCls, ScriptableComponentCls = _get_ref_classes()

    fields = []
    for name, hint in hints.items():
        if name.startswith('_'):
            continue

        # ---- Unpack Reflect[T, metadata...] or use the raw type ----
        base_type, metadata = _unwrap_annotated(hint)

        # A list[...] field's element type carries its own Reflect metadata, and
        # that metadata describes each ROW the inspector draws rather than the
        # list as a whole -- list[Reflect[float, Range(0, 1), Slider()]] is a
        # column of sliders. Element metadata is merged in last so it wins over
        # anything an outer Reflect[list[...], ...] said about the same thing.
        is_list = typing.get_origin(base_type) is list
        element_type = None
        if is_list:
            list_args = typing.get_args(base_type)
            if not list_args:
                continue  # bare `list` annotation is ambiguous — skip
            element_type, element_metadata = _unwrap_annotated(list_args[0])
            metadata = metadata + element_metadata
            if typing.get_origin(element_type) is list:
                print(f"[introspection] Ignoring '{name}': a list of lists has no "
                      f"inspector widget.", file=sys.stderr)
                continue

        # ---- Resolve default value ----
        default = None
        has_default = False
        for klass in cls.__mro__:
            if name in klass.__dict__:
                default = klass.__dict__[name]
                has_default = True
                break

        # list[...] fields with no class-level default → empty list
        if not has_default and is_list:
            default = []
            has_default = True

        if not has_default:
            from .re_math import Vector2, Vector3, Vector4
            fallbacks = {float: 0.0, int: 0, bool: False, str: ""}
            default = fallbacks.get(base_type)

            if default is None:
                qual = getattr(base_type, '__qualname__', '') or ''
                if 'Vector4' in qual:
                    default = Vector4()
                elif 'Vector3' in qual:
                    default = Vector3()
                elif 'Vector2' in qual:
                    default = Vector2()

            # Material, Sprite, and any other class type (custom scripts) default to empty ID
            if default is None and isinstance(base_type, type):
                default = ""

            if default is None:
                continue

        # ---- Map type to engine type name ----
        field_ref_type_name = ""
        element_type_name = ""
        element_ref_type_name = ""

        if is_list:
            # list[T] field — map the element type, which has already had any
            # Reflect[...] wrapper stripped off it above.
            element_type_name, element_ref_type_name = _map_type(
                element_type, MaterialCls, SpriteCls, GameObjectCls,
                ComponentCls, ScriptableComponentCls)
            if element_type_name is None:
                print(f"[introspection] Ignoring '{name}': {element_type!r} is not a "
                      f"type the inspector can list.", file=sys.stderr)
                continue
            type_name = "list"
            if not isinstance(default, list):
                default = []
        else:
            type_name, field_ref_type_name = _map_type(
                base_type, MaterialCls, SpriteCls, GameObjectCls,
                ComponentCls, ScriptableComponentCls)

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

        # ---- Extract optional metadata (Range / Step / Tooltip / ReadOnly / Options) ----
        field_min       = None
        field_max       = None
        field_step      = 0.1
        field_tooltip   = ""
        field_read_only = False
        field_widget    = ""
        option_labels   = []
        option_values   = []

        for m in metadata:
            if isinstance(m, Range):
                field_min = m.min
                field_max = m.max
            elif isinstance(m, Step):
                field_step = m.value
            elif isinstance(m, Tooltip):
                field_tooltip = m.text
            elif isinstance(m, ReadOnly):
                field_read_only = bool(m.value)
            elif m is ReadOnly:
                # Written without parentheses. Annotated takes any object as
                # metadata, so this is legal Python that would otherwise be
                # dropped on the floor — honour the obvious intent.
                field_read_only = True
            elif isinstance(m, Options):
                option_labels = list(m.labels)
                option_values = list(m.values)
            elif isinstance(m, Slider) or m is Slider:
                field_widget = "slider"
            elif isinstance(m, RangeSlider) or m is RangeSlider:
                field_widget = "range_slider"

        # Every check below is about the type the WIDGET carries, which for a list
        # is one row's element type -- `list` itself is never what a Slider or an
        # Options() was asking for.
        widget_type = element_type_name if type_name == "list" else type_name
        described = (f"a list of {widget_type}" if type_name == "list" else widget_type)

        # Both sliders are bounded controls: the Range IS the thing being dragged
        # along, and the field type has to be one the widget can carry. Anything
        # else drops back to the ordinary editor with a reason, because a slider
        # that silently renders as a number box looks like the metadata was
        # ignored rather than rejected.
        if field_widget:
            wanted = "vec2" if field_widget == "range_slider" else "float"
            if widget_type != wanted:
                print(f"[introspection] Ignoring {field_widget} on '{name}': needs a "
                      f"{'Vector2' if wanted == 'vec2' else 'float'} field "
                      f"(this one is {described}).", file=sys.stderr)
                field_widget = ""
            elif field_min is None or field_max is None or field_max <= field_min:
                print(f"[introspection] Ignoring {field_widget} on '{name}': it needs a "
                      f"Range(min, max) to slide along.", file=sys.stderr)
                field_widget = ""

        # A dropdown needs a value the combo box can carry: an int (the option's
        # value) or a str (the label itself). Anything else is a mistake worth
        # saying out loud, since silently rendering a normal field would just look
        # like Options() did nothing.
        if option_labels and widget_type not in ("int", "str"):
            print(f"[introspection] Ignoring Options() on '{name}': only int and "
                  f"str fields can be dropdowns (this one is {described}).",
                  file=sys.stderr)
            option_labels = []
            option_values = []

        fields.append({
            "name":                  name,
            "type_name":             type_name,
            "default":               default,
            "min":                   field_min,
            "max":                   field_max,
            "step":                  field_step,
            "tooltip":               field_tooltip,
            "read_only":             field_read_only,
            "widget":                field_widget,
            "option_labels":         option_labels,
            "option_values":         option_values,
            "ref_type_name":         field_ref_type_name,
            "element_type_name":     element_type_name,
            "element_ref_type_name": element_ref_type_name,
        })

    return fields
