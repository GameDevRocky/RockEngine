"""Type stub for the AnimationCurve re-export.

Why this file exists: ``animation_curve.py`` re-exports ``AnimationCurve`` and
``Keyframe`` from ``rock_engine.core``, which is a pybind11 module compiled into
the engine and *created at runtime by the embedded interpreter*. There is no
``rock_engine`` package on disk, so a static analyser (Pylance/Pyright/mypy)
cannot follow that import and the names come back unresolved in the editor.

Every other name a script imports is a real Python class under ``Domain/lib``, so
this is the first type that needed a stub at all. The runtime is unaffected --
``.pyi`` files are inert at execution time and only the analyser reads them, so
the real bound class is still what a script gets.

Keep this in sync with ``Engine/src/bindings/AnimationCurveBindings.cpp``: that
file is the source of truth for the surface below.
"""

from typing import Iterator, overload


class Keyframe:
    """One control point. Tangents are in value-units per time-unit, so a
    straight line between two keys has both tangents equal to the slope."""

    time: float
    value: float
    in_tangent: float
    out_tangent: float

    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self, time: float, value: float,
                 in_tangent: float = 0.0, out_tangent: float = 0.0) -> None: ...
    def __repr__(self) -> str: ...


class AnimationCurve:
    """A keyframed float-over-float function, evaluated with cubic Hermite
    interpolation.

    Usable as a script field, where it gets a curve editor in the Inspector::

        class Player(ScriptableComponent):
            recoil : AnimationCurve = AnimationCurve.ease_in_out(0, 1, 1, 0)

    A curve read back off a component or field is a **copy** -- mutating it does
    not write through. Assign it again to apply::

        c = trail.width_curve
        c.add_key(0.5, 0.8)
        trail.width_curve = c
    """

    class Wrap:
        """What Evaluate does outside the key range."""
        CLAMP: AnimationCurve.Wrap
        LOOP: AnimationCurve.Wrap
        PING_PONG: AnimationCurve.Wrap

    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self, keys: list[Keyframe]) -> None: ...

    # ── Factories ────────────────────────────────────────────────────────────
    @staticmethod
    def constant(value: float) -> AnimationCurve:
        """A curve that returns the same value at every time."""
        ...

    @staticmethod
    def linear(t0: float, v0: float, t1: float, v1: float) -> AnimationCurve:
        """A straight ramp between two points."""
        ...

    @staticmethod
    def ease_in_out(t0: float, v0: float, t1: float, v1: float) -> AnimationCurve:
        """An S-curve between two points, flat at both ends."""
        ...

    # ── Evaluation ───────────────────────────────────────────────────────────
    def evaluate(self, t: float) -> float:
        """Sample the curve. Never raises; an empty curve evaluates to 0."""
        ...

    def __call__(self, t: float) -> float:
        """Alias for evaluate(), so a curve can be used like a function."""
        ...

    # ── Editing ──────────────────────────────────────────────────────────────
    def add_key(self, time: float, value: float) -> int:
        """Insert a key, keeping the curve sorted by time. Returns its index."""
        ...

    def remove_key(self, index: int) -> None: ...

    def move_key(self, index: int, key: Keyframe) -> int:
        """Overwrite one key. Returns the index it ended up at."""
        ...

    def clear(self) -> None: ...

    # ── State ────────────────────────────────────────────────────────────────
    @property
    def keys(self) -> list[Keyframe]: ...
    @property
    def start_time(self) -> float: ...
    @property
    def end_time(self) -> float: ...

    wrap: AnimationCurve.Wrap

    def __len__(self) -> int: ...
    def __eq__(self, other: object) -> bool: ...
    def __ne__(self, other: object) -> bool: ...
    def __repr__(self) -> str: ...
