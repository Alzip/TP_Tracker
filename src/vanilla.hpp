#pragma once
#include <array>
#include <map>
#include <string>
#include <string_view>

namespace tracker {
// Native save adapters are separate from the randomizer's repurposed item IDs.
// Sources: pinned Dusklight src/d/d_item.cpp, d_save_bit_labels.inc,
// d_a_tag_statue_evt.cpp, and mods/randomizer/src/item.cpp portal switches.
// All callbacks read the current save. They never write game state.
template<class ReadFirst, class ReadSlot, class ReadEvent, class ReadSwitch>
void vanillaInventory(std::map<std::string, int>& inventory, ReadFirst first,
                      ReadSlot slot, ReadEvent event, ReadSwitch stageSwitch) {
    inventory["Shadow Crystal"] = event(0x0d04);
    inventory["Aurus Memo"] = slot(19) == 0x90 || first(0x90) || event(0x2680);
    // These quest objects share native slots and are consumed/replaced. Receipt
    // flags retain the completed prerequisite after the next quest step.
    for (const auto& item : std::array<std::pair<const char*, int>, 5>{{
             {"Renados Letter", 0x80}, {"Invoice", 0x81}, {"Wooden Statue", 0x82},
             {"Ilias Charm", 0x83}, {"Asheis Sketch", 0x91}}})
        inventory[item.first] = first(item.second);

    int skills = 0;
    for (int flag : {0x2904, 0x2908, 0x2902, 0x2901, 0x2a80, 0x2a40, 0x2a20}) {
        if (!event(flag)) break; // Native skills are learned in order.
        ++skills;
    }
    inventory["Progressive Hidden Skill"] = skills;

    const int book = slot(22);
    int sky = book == 0xe9 || book == 0xea || book == 0xeb || first(0xe9) ? 1 : 0;
    // 0x6004 is the completion flag, NOT the sixth statue. Amphitheater uses 0x6204.
    for (int flag : {0x6080, 0x6040, 0x6020, 0x6010, 0x6008, 0x6204})
        if (event(flag)) ++sky;
    if (book == 0xeb || event(0x6004)) sky = 7;
    inventory["Progressive Sky Book"] = sky;

    // getcheck(JEWEL_ROD) tests any rod, so it cannot identify the Coral Earring.
    const int rod = slot(20);
    const bool hasRod = first(0x4a) || rod == 0x4a || rod == 0x5b || rod == 0x5c ||
                        rod == 0x5d || rod == 0x5e || rod == 0x5f;
    const bool earring = first(0x3d) || rod == 0x5c || rod == 0x5e || rod == 0x5f;
    inventory["Progressive Fishing Rod"] = hasRod ? (earring ? 2 : 1) : 0;

    inventory["Faron Woods Coro Key"] = stageSwitch(2, 0x0c) || first(0xfe);
    inventory["North Faron Woods Gate Key"] = stageSwitch(2, 0x14) || first(0xee);
    inventory["Gate Keys"] = event(0x0810);
    // Return from the first Castle escape. Later durable milestones also prove
    // completion on imported saves whose early cutscene flags were normalized.
    inventory["Vanilla Prologue Complete"] = event(0x0502) || event(0x0610) || event(0x0d04);
    // Zant's post-Lakebed cutscene starts the temporary Midna restriction.
    // Healthy early-game wolf form must work before the Shadow Crystal exists.
    inventory["Vanilla Midna Available"] = !event(0x0c01) || event(0x0d04);
    inventory["Vanilla Warp Unlocked"] = event(0x0604);
    // Native Barnes TYPE_SHOP is absent until M_031, after Goron Mines clears.
    // The randomizer opens this shop early and removes that local requirement.
    inventory["Vanilla Barnes Shop Open"] = event(0x0701);

    struct Portal { const char* name; int stage, flag; };
    for (const auto& portal : std::array<Portal, 15>{{
             {"Ordon Spring Portal", 0, 0x34}, {"South Faron Portal", 2, 0x47},
             {"Upper Zoras River Portal", 4, 0x15}, {"Castle Town Portal", 6, 3},
             {"Gerudo Desert Portal", 10, 0x15}, {"North Faron Portal", 2, 2},
             {"Kakariko Gorge Portal", 6, 0x15}, {"Kakariko Village Portal", 3, 0x1f},
             {"Death Mountain Portal", 3, 0x15}, {"Zoras Domain Portal", 4, 2},
             {"Lake Hylia Portal", 4, 0x0a}, {"Mirror Chamber Portal", 10, 0x28},
             {"Snowpeak Portal", 8, 0x15}, {"Sacred Grove Portal", 7, 0x64},
             {"Bridge of Eldin Portal", 6, 0x63}}})
        inventory[portal.name] = stageSwitch(portal.stage, portal.flag);
}

// The normal game's story exits depend on completed bosses, not on the
// randomizer solver's hypothetical ability to defeat them in a future route.
inline std::string vanillaStoryKey(std::string_view event) {
    for (const char* dungeon : {"Forest Temple", "Goron Mines", "Lakebed Temple",
                               "Arbiters Grounds", "Snowpeak Ruins", "Temple of Time",
                               "City in the Sky", "Palace of Twilight"})
        if (event == std::string("Can Complete ") + dungeon)
            return std::string("Vanilla ") + dungeon + " Complete";
    return {};
}
}
