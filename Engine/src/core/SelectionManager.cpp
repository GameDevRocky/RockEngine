#include "engine/core/SelectionManager.hpp"
#include <algorithm>
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/HierarchyUtils.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/rendering/core/AssetManager.hpp"

namespace {
const std::string kEmptyId;
}

void SelectionManager::Init()
{
    registry = container->FindSystem<Registry>();
    m_selectedIds.clear();
    m_shutdownSubs.clear();
}

void SelectionManager::Shutdown()
{
    // Deliberately not unsubscribing: during container teardown the objects these
    // ids refer to may already be gone.
    m_shutdownSubs.clear();
    m_selectedIds.clear();
}

SelectionManager* SelectionManager::Copy()
{
    // Deliberately empty, matching Copy(Container*) — which is the only overload
    // Container::Copy ever calls, so this one was unreachable. It used to copy
    // `subscribers`, which would have handed the runtime manager the editor's Qt
    // handlers and driven editor widgets against runtime ids.
    return new SelectionManager();
}

SelectionManager* SelectionManager::Copy(Container* container)
{
    auto* copy = new SelectionManager();
    copy->Attach(container);
    return copy;
}

bool SelectionManager::Resolves(const std::string& id) const
{
    if (id.empty()) return false;
    if (registry && registry->Find<Serializable>(id)) return true;

    auto& assets = AssetManager::Get();
    return assets.GetSprite(id) || assets.GetMaterial(id)
        || assets.GetTexture(id) || assets.GetShader(id);
}

void SelectionManager::ClearShutdownSubs()
{
    for (const auto& [id, subId] : m_shutdownSubs) {
        if (!registry) break;
        if (auto* obj = registry->Find<GameObject>(id)) obj->Unsubscribe(subId);
    }
    m_shutdownSubs.clear();
}

void SelectionManager::SetSelection(std::vector<std::string> ids)
{
    // Drop empty, unresolvable and duplicate ids, preserving order.
    std::vector<std::string> next;
    next.reserve(ids.size());
    for (auto& id : ids) {
        if (!Resolves(id)) continue;
        if (std::find(next.begin(), next.end(), id) != next.end()) continue;
        next.push_back(std::move(id));
    }

    // Compare the whole vector, never just the primary: going {A,B,C} -> {A,C}
    // removes a non-primary entry and leaves the primary at C, so a primary-only
    // check would suppress a real change. This also means an unresolvable id now
    // clears the selection *and notifies*, where the old code cleared silently and
    // left every listener showing a stale object.
    if (next == m_selectedIds) return;

    ClearShutdownSubs();
    m_selectedIds = std::move(next);

    for (const std::string& id : m_selectedIds) {
        if (!registry) break;
        auto* obj = registry->Find<GameObject>(id);
        if (!obj) continue;   // assets have no shutdown event

        m_shutdownSubs[id] = obj->Subscribe([this, id](std::any) {
            // `this`, not Engine::Get()->GetActiveContainer(): during play mode an
            // editor object's destruction would otherwise deselect on the RUNTIME
            // manager. And RemoveFromSelection, not a full clear — destroying one of
            // N selected objects should drop that one.
            //
            // Erase first: this handler is one-shot (returns false), so Observable
            // unsubscribes it itself and ClearShutdownSubs must not then call
            // Unsubscribe with a stale id on an object mid-teardown.
            m_shutdownSubs.erase(id);
            RemoveFromSelection(id);
            return false;
        }, RuntimeObject::SHUTDOWN_EVENT);
    }

    Notify(SELECTION_CHANGED_EVENT,
           m_selectedIds.empty() ? std::string{} : m_selectedIds.back());
}

void SelectionManager::Select(const std::string& objectId)
{
    if (m_selectedIds.size() == 1 && m_selectedIds[0] == objectId) return;
    SetSelection({objectId});
}

void SelectionManager::SelectMany(const std::vector<std::string>& ids)
{
    SetSelection(ids);
}

void SelectionManager::AddToSelection(const std::string& objectId)
{
    if (IsSelected(objectId)) return;
    std::vector<std::string> next = m_selectedIds;
    next.push_back(objectId);        // appended, so it becomes the primary
    SetSelection(std::move(next));
}

void SelectionManager::RemoveFromSelection(const std::string& objectId)
{
    if (!IsSelected(objectId)) return;
    std::vector<std::string> next;
    next.reserve(m_selectedIds.size());
    for (const std::string& id : m_selectedIds)
        if (id != objectId) next.push_back(id);
    SetSelection(std::move(next));
}

void SelectionManager::ToggleSelection(const std::string& objectId)
{
    if (IsSelected(objectId)) RemoveFromSelection(objectId);
    else                      AddToSelection(objectId);
}

void SelectionManager::ClearSelection()
{
    SetSelection({});
}

const std::string& SelectionManager::GetPrimaryId() const
{
    return m_selectedIds.empty() ? kEmptyId : m_selectedIds.back();
}

bool SelectionManager::IsSelected(const std::string& objectId) const
{
    return std::find(m_selectedIds.begin(), m_selectedIds.end(), objectId)
        != m_selectedIds.end();
}

std::vector<std::string> SelectionManager::GetSelectedRoots() const
{
    return HierarchyUtils::FilterToRoots(container, m_selectedIds);
}

Serializable* SelectionManager::GetSerializable() const
{
    return GetSerializable(GetPrimaryId());
}

Serializable* SelectionManager::GetSerializable(const std::string& id) const
{
    if (id.empty()) return nullptr;

    // Runtime objects (GameObjects, Components) first, then assets.
    if (auto* reg = container ? container->FindSystem<Registry>() : nullptr) {
        if (auto* obj = reg->Find<Serializable>(id)) return obj;
    }

    auto& assets = AssetManager::Get();
    if (auto* s  = assets.GetSprite(id))   return s;
    if (auto* m  = assets.GetMaterial(id)) return m;
    if (auto* t  = assets.GetTexture(id))  return t;
    if (auto* sh = assets.GetShader(id))   return sh;

    return nullptr;
}
