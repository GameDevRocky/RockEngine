#include "engine/bindings/PythonBindings.hpp"
#include "engine/utils/AnimationCurve.hpp"

#include <pybind11/operators.h>
#include <pybind11/stl.h>

#include <string>

// The first genuine py::class_ in the codebase, and deliberately so.
//
// Every other binding here is a free function keyed by a GameObject id string,
// because components live in a Registry and a Python object that outlived its
// C++ owner would be a dangling pointer waiting to happen. AnimationCurve has no
// registry identity: it is a value that copies, so binding it as a real class is
// the correct shape rather than a break with the convention. pybind11 owns the
// instance outright and there is nothing on the C++ side that can delete it out
// from under Python.
//
// The consequence worth knowing: a curve read back off a component is a COPY.
//
//     c = trail.width_curve   # copy
//     c.add_key(0.5, 0.8)     # edits the copy only
//     trail.width_curve = c   # <- this is what actually applies it
//
// Mutating the value returned by a getter never writes through, exactly as in
// Unity.

namespace py = pybind11;

void BindAnimationCurve(py::module_& m) {
    py::class_<Keyframe>(m, "Keyframe")
        .def(py::init<>())
        .def(py::init([](float time, float value, float inTangent, float outTangent) {
                 return Keyframe{ time, value, inTangent, outTangent };
             }),
             py::arg("time"), py::arg("value"),
             py::arg("in_tangent") = 0.0f, py::arg("out_tangent") = 0.0f)
        .def_readwrite("time", &Keyframe::time)
        .def_readwrite("value", &Keyframe::value)
        .def_readwrite("in_tangent", &Keyframe::inTangent)
        .def_readwrite("out_tangent", &Keyframe::outTangent)
        .def("__repr__", [](const Keyframe& k) {
            return "Keyframe(time=" + std::to_string(k.time)
                 + ", value=" + std::to_string(k.value) + ")";
        });

    py::class_<AnimationCurve> curve(m, "AnimationCurve");

    py::enum_<AnimationCurve::Wrap>(curve, "Wrap")
        .value("CLAMP", AnimationCurve::Wrap::Clamp)
        .value("LOOP", AnimationCurve::Wrap::Loop)
        .value("PING_PONG", AnimationCurve::Wrap::PingPong);

    curve
        .def(py::init<>())
        .def(py::init<std::vector<Keyframe>>(), py::arg("keys"))

        .def_static("constant", &AnimationCurve::Constant, py::arg("value"),
                    "A curve that returns the same value at every time.")
        .def_static("linear", &AnimationCurve::Linear,
                    py::arg("t0"), py::arg("v0"), py::arg("t1"), py::arg("v1"),
                    "A straight ramp between two points.")
        .def_static("ease_in_out", &AnimationCurve::EaseInOut,
                    py::arg("t0"), py::arg("v0"), py::arg("t1"), py::arg("v1"),
                    "An S-curve between two points, flat at both ends.")

        .def("evaluate", &AnimationCurve::Evaluate, py::arg("t"),
             "Sample the curve. Never raises; an empty curve evaluates to 0.")
        .def("add_key",
             [](AnimationCurve& c, float time, float value) { return c.AddKey(time, value); },
             py::arg("time"), py::arg("value"),
             "Insert a key, keeping the curve sorted by time. Returns its index.")
        .def("remove_key", &AnimationCurve::RemoveKey, py::arg("index"))
        .def("move_key",
             [](AnimationCurve& c, int index, const Keyframe& k) { return c.MoveKey(index, k); },
             py::arg("index"), py::arg("key"),
             "Overwrite one key. Returns the index it ended up at.")
        .def("clear", &AnimationCurve::Clear)

        .def_property_readonly("keys", &AnimationCurve::Keys)
        .def_property_readonly("start_time", &AnimationCurve::StartTime)
        .def_property_readonly("end_time", &AnimationCurve::EndTime)
        .def_property("wrap", &AnimationCurve::GetWrap, &AnimationCurve::SetWrap)

        .def("__len__", &AnimationCurve::KeyCount)
        .def("__call__", &AnimationCurve::Evaluate, py::arg("t"),
             "Alias for evaluate(), so a curve can be used like a function.")
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__repr__", [](const AnimationCurve& c) {
            return "AnimationCurve(" + std::to_string(c.KeyCount()) + " keys)";
        });
}
