#pragma once
#include "model.hpp"
#include <optional>
#include <string_view>

namespace tracker {
inline bool vanillaSharedCampChest(std::string_view name) {
    return name == "Bulblin Camp First Chest Under Tower At Entrance" ||
           name == "Bulblin Camp Small Chest in Back of Camp";
}

// Native events: VISITED_DESERT_FOR_THE_FIRST_TIME (0x4008),
// ESCAPED_BURNING_TENT_IN_BULBLIN_CAMP (0x0b40). The key is a normal
// small-key counter in save table 0x0a, not the randomizer's custom item.
// All arguments are observations; no native save flags are written.
template<class ReadEvent>
void vanillaDesertProgress(std::map<std::string,int>& inventory, int nativeKeys,
                           bool keyObserved, ReadEvent event) {
    const bool cleared = event(0x0b40);
    inventory["Vanilla Desert Visited"] = event(0x4008) || cleared;
    inventory["Vanilla Bulblin Camp Cleared"] = cleared;
    inventory["Gerudo Desert Bulblin Camp Key"] = nativeKeys > 0 || keyObserved || cleared;
}

inline std::optional<bool> vanillaDesertCompletion(const Json& check,
                                                  const std::map<std::string,int>& inventory) {
    const auto name = check.at("name").get<std::string>();
    // Both original actors use 63. The randomizer changes them to 31 and 30.
    // Neither the patched bits nor the shared native bit proves which reward
    // was received. Actual grants are retained separately in the tracker journal.
    if (vanillaSharedCampChest(name)) return false;
    if (name == "Bulblin Guard Key") {
        const auto it = inventory.find("Gerudo Desert Bulblin Camp Key");
        return it != inventory.end() && it->second > 0;
    }
    return std::nullopt;
}

// This is called only by the successful inventory-grant observer, NEVER by a
// preview/resolver or a chest-opening animation. Native camp chest actors have
// the same tag, but distinct, verified original rewards (angle.z: 0x0fff / 0x05ff).
// Exact stage-tag, room and item checks prevent matching unrelated pickups.
inline std::string vanillaGrantName(std::string_view tag, int room, int item) {
    if (tag == "chest:F_SP118:63") {
        if (room != 1) return {};
        if (item == 0x0f) return "Bulblin Camp First Chest Under Tower At Entrance";
        if (item == 0x05) return "Bulblin Camp Small Chest in Back of Camp";
        return {};
    }
    return std::string(tag);
}

// A save scan may have noticed a persistent flag before observe_gives fires.
// Still journal the actual grant even when Model::give says it is already DONE.
inline bool recordTrackedGrant(Model& model, std::set<std::string>& journal,
                               const std::string& name) {
    const auto found = model.aliases.find(name);
    if (found == model.aliases.end()) return false;
    const bool changed = model.give(name);
    if (!changed && !model.obtained.contains(found->second)) return false;
    const bool recorded = journal.insert(found->second).second;
    return changed || recorded;
}

// Recovery for an old save cannot reconstruct two independent rewards from
// one shared bit. The explicit user action changes the tracker journal only.
inline bool toggleSharedCampCompletion(Model& model, std::set<std::string>& journal,
                                       const std::string& name) {
    if (!model.normalGame || !vanillaSharedCampChest(name) || !model.aliases.contains(name)) return false;
    if (journal.erase(name)) model.obtained.erase(name);
    else { journal.insert(name); model.obtained.insert(name); model.skipped.erase(name); }
    return true;
}
}
