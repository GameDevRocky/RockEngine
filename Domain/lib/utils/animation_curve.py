"""Keyframed float-over-float curves.

Unlike everything in ``lib/api/components``, this is a **re-export, not a proxy**.
Those handlers wrap an object that lives in the C++ Registry, so they hold an id
and forward every call. ``AnimationCurve`` is a plain value bound directly with
pybind11 -- the object Python holds *is* the curve, so there is nothing to
forward and nothing that can dangle.

The consequence: a curve read off a component is a **copy**. Mutating it does not
write back.

    curve = trail.width_curve      # a copy
    curve.add_key(0.5, 0.8)        # edits the copy
    trail.width_curve = curve      # this is what applies it

Usage::

    from Domain import *

    taper = AnimationCurve.linear(0, 1, 1, 0)   # full width at the head, 0 at the tail
    taper.add_key(0.5, 0.8)
    taper.evaluate(0.25)
    taper(0.25)                                  # same thing

    pulse = AnimationCurve.linear(0, 0, 1, 1)
    pulse.wrap = AnimationCurve.Wrap.PING_PONG   # bounce instead of clamping
"""

from rock_engine.core import AnimationCurve, Keyframe

__all__ = ["AnimationCurve", "Keyframe"]
