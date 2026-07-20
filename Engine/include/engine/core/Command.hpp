#pragma once
#include <string>

class Container;

// One reversible edit. Base of everything the UndoSystem stores.
//
// Undo/Redo receive the container that owns the stack rather than looking one up.
// That is what makes the whole system safe across the play-mode swap: a command on
// the editor container's stack can only ever resolve editor objects, and a command
// on the runtime container's stack can only ever resolve runtime ones. There is no
// path by which a stack reaches into the other world.
//
// Commands must store ids and values, never pointers. An object can be destroyed
// and later restored by an undo, at which point only its id is still meaningful.
class Command {
public:
    virtual ~Command() = default;

    virtual void Undo(Container* container) = 0;
    virtual void Redo(Container* container) = 0;

    // Text for the editor's Undo/Redo menu entries ("Change Rotation").
    virtual std::string GetText() const = 0;

    // Commands sharing a merge id are candidates for coalescing; MergeWith decides.
    // Negative means "never merge" (the default), which is right for anything a
    // user triggers once per action rather than per input event.
    static constexpr int kNoMerge = -1;
    virtual int MergeId() const { return kNoMerge; }

    // Fold `other` (a newer edit) into this one. Only called by UndoSystem::Push,
    // and only after it has already checked the merge id, gesture and time window.
    virtual bool MergeWith(const Command* other) { return false; }

    // Stamped by UndoSystem::Push. Two commands from different gestures never merge,
    // even if they land within the time window — see UndoSystem::BreakMergeChain.
    unsigned GetGestureSerial() const { return m_gestureSerial; }
    void SetGestureSerial(unsigned serial) { m_gestureSerial = serial; }

private:
    unsigned m_gestureSerial = 0;
};
