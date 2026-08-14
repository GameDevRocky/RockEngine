#pragma once

// Include pybind11 from a translation unit that also sees Qt headers.
//
// Qt defines `slots` and `signals` as empty macros, and CPython's object.h declares a
// member literally named `slots` -- so a Qt-first include order rewrites it to
// `PyType_Slot *;` and the build dies with a syntax error inside Python's own headers.
// Engine/ never hits this because it has no Qt; anything under Editor/ does.
//
// Undef around the include rather than defining QT_NO_KEYWORDS project-wide, which
// would force every existing Q_OBJECT class in the editor over to Q_SLOTS/Q_SIGNALS.
#pragma push_macro("slots")
#pragma push_macro("signals")
#undef slots
#undef signals

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#pragma pop_macro("signals")
#pragma pop_macro("slots")
