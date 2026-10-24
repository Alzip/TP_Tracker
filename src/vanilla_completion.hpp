#pragma once
#include "json.hpp"
#include <array>
#include <optional>
#include <string>

namespace tracker {

// A present value replaces the catalogue's completion flags, including false.
// Several flags in the randomizer catalogue are repurposed event/item bits;
// falling back to them after a native false would mark unrelated checks DONE.
// Sources: pinned Dusklight d_a_obj_life_container.cpp / STAGE_LIFE,
// d_a_obj_bosswarp.cpp / collectCrystal/collectMirror, d_a_obj_master_sword.cpp,
// d_save_bit_labels.inc, and randomizer hooks.cpp / STAGE_LIFE suppression.
template<class ReadEvent, class ReadFirst, class ReadLife, class ReadCrystal, class ReadMirror>
std::optional<bool> vanillaCompletionOverride(const nlohmann::json& check,
        ReadEvent event, ReadFirst first, ReadLife life, ReadCrystal crystal, ReadMirror mirror) {
    const auto name = check.at("name").get<std::string>();
    struct Heart { const char* name; int save; };
    static constexpr std::array hearts{
        Heart{"Forest Temple Diababa Heart Container", 0x10},
        Heart{"Goron Mines Fyrus Heart Container", 0x11},
        Heart{"Lakebed Temple Morpheel Heart Container", 0x12},
        Heart{"Arbiters Grounds Stallord Heart Container", 0x13},
        Heart{"Snowpeak Ruins Blizzeta Heart Container", 0x14},
        Heart{"Temple of Time Armogohma Heart Container", 0x15},
        Heart{"City in the Sky Argorok Heart Container", 0x16},
        Heart{"Palace of Twilight Zant Heart Container", 0x17},
    };
    for (const auto& heart : hearts) if (name == heart.name) return life(heart.save) != 0;

    static constexpr std::array<const char*, 3> fusedRewards{
        "Forest Temple Dungeon Reward", "Goron Mines Dungeon Reward", "Lakebed Temple Dungeon Reward"};
    for (size_t i = 0; i < fusedRewards.size(); ++i)
        if (name == fusedRewards[i]) return crystal(static_cast<int>(i)) != 0;
    static constexpr std::array<const char*, 4> mirrorRewards{
        "Arbiters Grounds Dungeon Reward", "Snowpeak Ruins Dungeon Reward",
        "Temple of Time Dungeon Reward", "City in the Sky Dungeon Reward"};
    for (size_t i = 0; i < mirrorRewards.size(); ++i)
        if (name == mirrorRewards[i]) return mirror(static_cast<int>(i)) != 0;

    // The catalogue's 0x2120 is vanilla "saw Auru's location on map".
    if (name == "Sacred Grove Pedestal Master Sword") return first(0x29) != 0;
    if (name == "Sacred Grove Pedestal Shadow Crystal") return event(0x0d04) != 0;
    // 0x6308 is the randomizer's Shad reward flag, absent from the normal quest.
    if (name == "Shad Dominion Rod") return event(0x2580) != 0;
    // 0x3c10 is vanilla dialogue with the Goron Mines miniboss, not this wolf.
    if (name == "Faron Woods Golden Wolf") return event(0x2904) != 0;
    // Talking once (0x2520) is not proof that Auru's memo was received.
    if (name == "Auru Gift To Fyer") return first(0x90) || event(0x2510) || event(0x2680);
    if (name == "Coro Gate Key") return first(0xfe) != 0;
    if (name == "Ordon Sword") return first(0x28) != 0;
    if (name == "Ordon Shield") return first(0x2a) != 0;
    return std::nullopt;
}
}
