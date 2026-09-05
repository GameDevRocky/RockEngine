#pragma once
#include <functional>
#include <string>
#include <utility>
#include "engine/core/Command.hpp"

// "Set one process-global editor setting" — the grid/snap values, gizmo overlay
// visibility, the Scene view's shaded/unshaded toggle.
//
// Why this is not a PropertyCommand: that one re-resolves its target by id
// through the container's Registry on every apply, because a Serializable can be
// destroyed and restored by an undo, at which point only its id is meaningful.
// The targets here are singletons and view state — they have no id, are not in
// any Registry, and cannot be destroyed by an undo. So the setter reaches them
// directly and the Container handed to Undo/Redo is deliberately ignored.
//
// That ignores the container-scoping guarantee in Command.hpp, so be precise
// about why it is safe: the guarantee exists to stop a stack reaching into the
// OTHER world's objects. These settings belong to no world. There is nothing
// per-container to reach into, and the editor and runtime containers observe the
// same single value either way.
//
// The setter must not capture a pointer to anything destroyable, for the same
// reason PropertyCommand's must not — go through the singleton's Get() (or an
// editor Get()) inside the lambda so the target is re-resolved on every apply.
template <typename ValueT>
class EditorSettingCommand : public Command {
public:
    using Setter = std::function<void(const ValueT&)>;

    // Shares one merge id; MergeWith filters on the setting key.
    static constexpr int kMergeId = 2;   // 1 belongs to PropertyCommand

    // `mergeable` should be true only for settings driven by a stream of input
    // events -- a spinbox being typed into or dragged, where every keystroke
    // would otherwise become its own undo entry. Leave it false for anything a
    // user triggers once per action: folding two clicks of a toggle inside the
    // merge window would collapse them into an entry whose before and after are
    // identical, i.e. an undo step that does nothing.
    EditorSettingCommand(std::string settingKey,
                         ValueT oldValue,
                         ValueT newValue,
                         Setter setter,
                         std::string text,
                         bool mergeable = false)
        : m_key(std::move(settingKey))
        , m_old(std::move(oldValue))
        , m_new(std::move(newValue))
        , m_setter(std::move(setter))
        , m_text(std::move(text))
        , m_mergeable(mergeable)
    {}

    void Undo(Container* /*container*/) override { if (m_setter) m_setter(m_old); }
    void Redo(Container* /*container*/) override { if (m_setter) m_setter(m_new); }

    std::string GetText() const override { return m_text; }
    int MergeId() const override { return m_mergeable ? kMergeId : kNoMerge; }

    bool MergeWith(const Command* other) override {
        if (!m_mergeable) return false;
        // The dynamic_cast pins the value type, so a float edit can never fold
        // into a bool one even if they somehow shared a key.
        auto* o = dynamic_cast<const EditorSettingCommand<ValueT>*>(other);
        if (!o || !o->m_mergeable) return false;
        if (o->m_key != m_key) return false;

        // Keep the original "before", take the latest "after" -- the point of
        // folding a run of per-keystroke edits into one entry.
        m_new = o->m_new;
        return true;
    }

private:
    std::string m_key;
    ValueT m_old;
    ValueT m_new;
    Setter m_setter;
    std::string m_text;
    bool m_mergeable;
};
