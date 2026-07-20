#pragma once
#include <string>
#include <vector>

class Container;

// Hierarchy queries over a set of object ids.
//
// Free functions rather than SelectionManager members: "given a set of GameObject
// ids, drop the ones already covered by another" is a graph question, equally
// needed by duplicate, export and a future multi-reparent, none of which involve
// selection.
namespace HierarchyUtils {

// `ids` minus every id whose Transform ancestor chain reaches another id in `ids`.
// Order is preserved.
//
// Ids that don't resolve to a GameObject with a Transform (assets, dead ids) pass
// through unchanged — dropping them would silently mangle a mixed selection, and
// callers that want only GameObjects already filter by type themselves.
std::vector<std::string> FilterToRoots(Container* container,
                                       const std::vector<std::string>& ids);

}
