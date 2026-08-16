class Range:
    """Metadata descriptor for numeric property constraints."""
    def __init__(self, min=None, max=None):
        self.min = min
        self.max = max


class Step:
    """Metadata descriptor for property step size."""
    def __init__(self, value=0.1):
        self.value = value


class Tooltip:
    """Metadata descriptor for property tooltip text."""
    def __init__(self, text):
        self.text = text


class Slider:
    """Metadata descriptor rendering a bounded ``float`` as a drag slider.

    **Requires a Range.** A slider is a position along a span, so without bounds
    there is nothing to drag along; introspection says so and falls back to the
    ordinary number box rather than drawing a handle that can never move::

        volume: Reflect[float, Range(0.0, 1.0), Step(0.01), Slider()] = 0.8

    ``Step`` sets the increments the handle lands on, and how many decimals the
    readout shows. The number beside the slider stays typeable, so an exact value
    is always reachable.
    """
    def __init__(self):
        pass


class RangeSlider:
    """Metadata descriptor rendering a ``Vector2`` as a two-handle span slider,
    where ``x`` is the low end and ``y`` the high end.

    **Requires a Range**, same as Slider — that Range is the outer bound both
    handles move within, while the Vector2 is the span currently selected::

        spawn_delay: Reflect[Vector2, Range(0.0, 10.0), Step(0.1),
                             RangeSlider()] = Vector2(1.0, 3.0)

    The handles cannot cross, so ``value.x <= value.y`` always holds.
    """
    def __init__(self):
        pass


class Options:
    """Metadata descriptor turning a field into an inspector dropdown.

    Valid on ``int`` and ``str`` fields, and the two store different things —
    pick whichever makes the script read better::

        # str: the field holds the chosen label, so comparisons read naturally
        state: Reflect[str, Options("Idle", "Patrol", "Chase")] = "Idle"
        if self.state == "Chase": ...

        # int: the field holds the option's value, which defaults to its index
        quality: Reflect[int, Options("Low", "Medium", "High")] = 0

        # int with explicit values, when the number means something
        damage: Reflect[int, Options({"Light": 5, "Heavy": 25})] = 5

    Accepts varargs (``Options("a", "b")``), one list (``Options(["a", "b"])``),
    a dict of label -> int, or ``(label, value)`` pairs. Order is preserved and
    is the order shown in the dropdown, which is why a set is not accepted.
    """
    def __init__(self, *choices):
        # One list or dict argument is unwrapped, so Options(["a","b"]) and
        # Options("a","b") mean the same thing. Tuples are deliberately NOT
        # unwrapped: Options(("Low", 0)) is a single (label, value) pair, and
        # unwrapping it would turn it into two labels.
        if len(choices) == 1 and isinstance(choices[0], (list, dict)):
            choices = choices[0]

        items = list(choices.items()) if isinstance(choices, dict) else list(choices)

        self.labels = []
        self.values = []
        for index, item in enumerate(items):
            # A 2-sequence whose second element is a real int is a (label, value)
            # pair; bool is excluded because it subclasses int and nobody means
            # ("Fast", True) as a value.
            if (isinstance(item, (tuple, list)) and len(item) == 2
                    and isinstance(item[1], int) and not isinstance(item[1], bool)):
                label, value = item
            else:
                label, value = item, index
            self.labels.append(str(label))
            self.values.append(int(value))


class ReadOnly:
    """Metadata descriptor marking a field display-only in the inspector.

    The field is still shown and still refreshes as the script changes it — it
    just cannot be edited from the editor. Use it for computed state you want to
    watch while playing::

        speed: Reflect[float, ReadOnly()] = 0.0

    Unlike its siblings this one takes no required argument, so ``ReadOnly``
    without parentheses is the mistake people actually make. Annotated accepts
    any object as metadata, so the bare class would sail through and silently do
    nothing; introspection therefore accepts both forms.
    """
    def __init__(self, value=True):
        self.value = value
