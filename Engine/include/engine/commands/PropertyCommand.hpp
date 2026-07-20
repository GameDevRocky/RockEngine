#pragma once
#include <functional>
#include <string>
#include <utility>
#include "engine/core/Command.hpp"
#include "engine/core/Container.hpp"
#include "engine/serialization/Registry.hpp"

// "Set one property on one object" — the workhorse of the undo system.
//
// The setter takes its target as an argument and MUST NOT capture a Serializable
// pointer. Commands outlive whatever created them, and can outlive the object
// itself across a delete/undo cycle, so the target is re-resolved by id on every
// apply — through the container handed to Undo/Redo, never a global lookup.
template <typename TargetT, typename ValueT>
class PropertyCommand : public Command {
public:
    using Setter = std::function<void(TargetT*, const ValueT&)>;

    // All PropertyCommands share one merge id; MergeWith does the real filtering
    // on target and property.
    static constexpr int kMergeId = 1;

    PropertyCommand(std::string targetId,
                    std::string propertyKey,
                    ValueT oldValue,
                    ValueT newValue,
                    Setter setter,
                    std::string text)
        : m_targetId(std::move(targetId))
        , m_key(std::move(propertyKey))
        , m_old(std::move(oldValue))
        , m_new(std::move(newValue))
        , m_setter(std::move(setter))
        , m_text(std::move(text))
    {}

    void Undo(Container* container) override { Apply(container, m_old); }
    void Redo(Container* container) override { Apply(container, m_new); }

    std::string GetText() const override { return m_text; }
    int MergeId() const override { return kMergeId; }

    bool MergeWith(const Command* other) override {
        // The dynamic_cast checks target type and value type in one step, so a
        // float edit can never fold into a vec2 edit.
        auto* o = dynamic_cast<const PropertyCommand<TargetT, ValueT>*>(other);
        if (!o) return false;
        if (o->m_targetId != m_targetId) return false;
        if (o->m_key != m_key) return false;

        // Keep the original "before" and take the latest "after" — the whole point
        // of folding a stream of per-keystroke edits into one entry.
        m_new = o->m_new;
        return true;
    }

private:
    void Apply(Container* container, const ValueT& value) {
        if (!container) return;
        Registry* registry = container->FindSystem<Registry>();
        if (!registry) return;
        if (TargetT* target = registry->Find<TargetT>(m_targetId))
            m_setter(target, value);
    }

    std::string m_targetId;
    std::string m_key;
    ValueT m_old;
    ValueT m_new;
    Setter m_setter;
    std::string m_text;
};
