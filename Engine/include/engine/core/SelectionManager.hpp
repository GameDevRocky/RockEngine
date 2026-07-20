#pragma once
#include "engine/core/System.hpp"
#include "engine/serialization/Serializable.hpp"
#include <string>
#include <unordered_map>
#include <vector>

class GameObject;

// The editor's current selection, as an ordered list of ids.
//
// The LAST entry is the "primary" (Unity's active object): it drives the inspector,
// and adding to the selection retargets it. Order is otherwise preserved.
class SelectionManager : public System {
public:
    // Payload: std::string, the PRIMARY id — empty when the selection is empty.
    // Listeners that need the whole set pull GetSelectedIds() from the manager.
    //
    // The payload stays a single string rather than a vector because Observable
    // also delivers to every ANY_EVENT subscriber on this object, so a type change
    // would surface as a runtime std::bad_any_cast in some listener we didn't find
    // by grepping, not as a compile error.
    //
    // Handlers MUST NOT mutate the selection: Notify copies the callback list before
    // dispatch, so a handler that re-entered would leave later handlers reading state
    // that no longer matches the payload they were given.
    static inline const Event SELECTION_CHANGED_EVENT = Observable::CreateEvent();

    SelectionManager() = default;
    ~SelectionManager() override = default;

    void Init() override;
    void Shutdown() override;

    SelectionManager* Copy() override;
    SelectionManager* Copy(Container* container) override;

    void Select(const std::string& objectId);                 // replace with one
    void SelectMany(const std::vector<std::string>& ids);     // replace with N
    void AddToSelection(const std::string& objectId);
    void RemoveFromSelection(const std::string& objectId);
    void ToggleSelection(const std::string& objectId);
    void ClearSelection();
    void Deselect() { ClearSelection(); }                     // pre-multi-select name

    const std::vector<std::string>& GetSelectedIds() const { return m_selectedIds; }
    const std::string& GetPrimaryId() const;
    bool IsSelected(const std::string& objectId) const;
    std::size_t GetSelectionCount() const { return m_selectedIds.size(); }

    // The selection minus any object whose ancestor is also selected. What callers
    // that act on whole subtrees want — deleting a parent already destroys its
    // children, and applying a gizmo delta to both moves the child twice.
    std::vector<std::string> GetSelectedRoots() const;

    // Primary-based accessors, kept so every pre-multi-select call site still works.
    const std::string& GetSelectedId() const { return GetPrimaryId(); }
    bool HasSelection() const { return !m_selectedIds.empty(); }
    Serializable* GetSerializable() const;                        // the primary
    Serializable* GetSerializable(const std::string& id) const;   // any id

private:
    // The single mutator. Everything above funnels through it, so validation,
    // shutdown-subscription bookkeeping and change detection live in one place.
    void SetSelection(std::vector<std::string> ids);
    bool Resolves(const std::string& id) const;
    void ClearShutdownSubs();

    Registry* registry = nullptr;
    std::vector<std::string> m_selectedIds;               // ordered; back() is primary
    std::unordered_map<std::string, int> m_shutdownSubs;  // id -> subscription id
};
