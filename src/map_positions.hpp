#pragma once
#include "model.hpp"
#include <algorithm>

namespace tracker {
inline bool passesAccessibilityFilter(const Model& state, const std::string& name, bool accessibleOnly) {
    if(!accessibleOnly) return true;
    const auto found=state.accessible.find(name);
    return found!=state.accessible.end() && found->second==Truth::yes &&
        !state.obtained.contains(name) && !state.skipped.contains(name);
}

inline bool markerMilestoneVisible(const Model& state, const std::string& name) {
    return !state.mapHidden.contains(name);
}

// Shared by every map/minimap rendering path, including fallback markers.
inline bool mapCheckVisible(const Model& state, const Json& check, bool accessibleOnly) {
    const auto& categories=check.at("categories");
    const auto name=check.at("name").get<std::string>();
    // Keep this sign in the tracker and read-hint journal, but off both maps.
    // The native game already draws Tears of Light / Twilit Bugs.
    return name!="Lake Hylia Hint Sign"
        && std::find(categories.begin(),categories.end(),"Twilit Insect")==categories.end()
        && passesAccessibilityFilter(state,name,accessibleOnly)
        && state.enabled(check) && !state.skipped.contains(name)
        && markerMilestoneVisible(state,name);
}

// An explicit entrance is authoritative even when its interior uses an F_
// stage name (Iza's boat course). Never draw non-local placeholder coordinates.
inline const Json* worldMapAnchor(const Json& point) {
    if (point.contains("overworld") && point.at("overworld").is_object())
        return &point.at("overworld");
    if (point.value("local", true) && point.at("stage").get<std::string>().starts_with("F_"))
        return &point;
    return nullptr;
}
}
