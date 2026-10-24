#include "vanilla_completion.hpp"
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Pass catalogue path");
        std::ifstream input(argv[1]);
        const auto catalogue = nlohmann::json::parse(input);
        std::set<int> events, firstItems, hearts, crystals, mirrors;
        auto completion = [&](const char* name) {
            for (const auto& check : catalogue.at("checks")) if (check.at("name") == name)
                return tracker::vanillaCompletionOverride(check,
                    [&](int flag) { return events.contains(flag); },
                    [&](int item) { return firstItems.contains(item); },
                    [&](int save) { return hearts.contains(save); },
                    [&](int index) { return crystals.contains(index); },
                    [&](int index) { return mirrors.contains(index); });
            throw std::runtime_error(std::string("Missing catalogue check: ") + name);
        };

        // A boss may be beaten and its reward collected while the heart is left
        // behind. Native life flags and reward flags must remain independent.
        crystals.insert(0);
        require(completion("Forest Temple Dungeon Reward") == true, "Forest reward missing");
        require(completion("Forest Temple Diababa Heart Container") == false, "Boss reward completed uncollected heart");
        hearts.insert(0x10);
        require(completion("Forest Temple Diababa Heart Container") == true, "Native life bit ignored");
        require(completion("Goron Mines Fyrus Heart Container") == false, "Heart bit leaked between dungeons");
        mirrors.insert(2);
        require(completion("Temple of Time Dungeon Reward") == true, "Native mirror reward missing");
        require(completion("Arbiters Grounds Dungeon Reward") == false, "Mirror indices conflated");
        require(completion("City in the Sky Dungeon Reward") == false, "Later mirror assumed collected");
        hearts.insert(0x17);
        require(completion("Palace of Twilight Zant Heart Container") == true, "Palace native life bit ignored");

        // These catalogue event bits have unrelated meanings in the normal
        // game. A false override must suppress the fallback, not mean "unset".
        events = {0x2120, 0x6308, 0x3c10, 0x2520};
        require(completion("Sacred Grove Pedestal Master Sword") == false, "Auru map dialogue grants sword");
        require(completion("Sacred Grove Pedestal Shadow Crystal") == false, "Auru map dialogue grants crystal");
        require(completion("Shad Dominion Rod") == false, "Randomizer Shad bit grants native rod restoration");
        require(completion("Faron Woods Golden Wolf") == false, "Goron dialogue grants Ending Blow");
        require(completion("Auru Gift To Fyer") == false, "Conversation alone grants memo");
        firstItems.insert(0x29);
        require(completion("Sacred Grove Pedestal Master Sword") == true, "Native sword receipt missing");
        require(completion("Sacred Grove Pedestal Shadow Crystal") == false, "Sword alone grants transformation");
        events.insert(0x0d04);
        require(completion("Sacred Grove Pedestal Shadow Crystal") == true, "Native transformation missing");
        events.insert(0x2580);
        events.insert(0x2904);
        events.insert(0x2680);
        require(completion("Shad Dominion Rod") == true, "Native restored rod missing");
        require(completion("Faron Woods Golden Wolf") == true, "Ending Blow missing");
        require(completion("Auru Gift To Fyer") == true, "Consumed memo no longer completed");
        firstItems.insert(0xfe);
        require(completion("Coro Gate Key") == true, "Native gate key receipt missing");

        // Ordinary native chest checks still use the catalogue's existing
        // chest flags; absence of an override is distinct from a known false.
        require(!completion("Wooden Sword Chest").has_value(), "Native chest unexpectedly overridden");
        require(!completion("Kakariko Graveyard Golden Wolf").has_value(), "Verified native wolf flag overridden");
        std::cout << "Vanilla completion regressions passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
