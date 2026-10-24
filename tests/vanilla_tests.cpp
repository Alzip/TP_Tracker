#include "model.hpp"
#include "vanilla.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tracker;
static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct NativeSave {
    std::set<int> first, events;
    std::map<int, int> slots;
    std::set<std::pair<int, int>> switches;
    void scan(Model& model) const {
        vanillaInventory(model.inventory,
            [&](int id) { return first.contains(id); },
            [&](int id) { auto it=slots.find(id); return it==slots.end() ? 0xff : it->second; },
            [&](int flag) { return events.contains(flag); },
            [&](int stage, int flag) { return switches.contains({stage, flag}); });
    }
};
static const Json& check(const Model& model, const char* name) {
    for (const auto& entry : model.catalogue.at("checks")) if (entry.at("name") == name) return entry;
    throw std::runtime_error(std::string("Missing fixture check: ")+name);
}
static void kakarikoRegressions(Model model, NativeSave save) {
    // Synthetic native-save snapshots: no game process or player's save is used.
    // The first visit is in twilight, with no Shadow Crystal or later-game gear.
    model.stage="F_SP109"; model.room=0; model.layer=0;
    model.inventory["Progressive Sword"]=2;
    model.inventory["Shadow Crystal"]=0;
    model.inventory["Vanilla Forest Temple Complete"]=0;
    model.inventory["Vanilla Current Wolf Form"]=0;
    model.solve();
    require(model.reached.at("Lower Kakariko Village")[0]==Truth::no &&
            model.reached.at("Lower Kakariko Village")[4]==Truth::no,
            "setting current stage bypassed Forest Temple story gate");

    model.inventory["Vanilla Forest Temple Complete"]=1;
    model.inventory["Vanilla Current Wolf Form"]=1;
    save.events.insert(0x0604); // Native first warp/bridge tutorial is complete.
    save.switches.insert({6,0x15}); // Kakariko Gorge warp.
    save.scan(model);
    model.solve();
    require(model.reached.at("Lower Kakariko Village")[4]==Truth::yes,
            "first Kakariko twilight visit has no Area Access");
    for (const char* name : {"Sanctuary Basement Twilit Insect 1", "Sanctuary Basement Twilit Insect 2",
                             "Sanctuary Basement Twilit Insect 3", "Kakariko Graveyard Twilit Insect",
                             "Kakariko Malo Mart Twilit Insect", "Kakariko Inn Pipe Twilit Insect",
                             "Kakariko Inn Bedroom Twilit Insect", "Kakariko Bug House Twilit Insect",
                             "Barnes Bomb Shop Twilit Insect", "Kakariko Destroyed Building Twilit Insect 1",
                             "Kakariko Destroyed Building Twilit Insect 2", "Kakariko Destroyed Building Twilit Insect 3",
                             "Kakariko Watchtower Twilit Insect", "Death Mountain Trail Twilit Insect Near Howling Stone",
                             "Death Mountain Trail Twilit Insect on Wall", "Death Mountain Trail Twilit Insect in Hot Spring"})
        require(model.enabled(check(model,name)) && model.accessible.at(name)==Truth::yes,
                "first Kakariko twilight insect route not recognized");
    require(model.reached.at("Lower Kakariko Village")[0]==Truth::no,
            "uncleared Eldin twilight falsely permits human form");
    require(model.accessible.at("Barnes Bomb Bag")==Truth::no &&
            model.checkDetails("Barnes Bomb Bag",false).find("Kakariko Barnes Bomb Shop Lower — LOCKED")!=std::string::npos,
            "twilight Barnes purchase is available or details disagree with checklist");
    require(model.accessible.at("Eldin Spring Underwater Chest")==Truth::no &&
            model.accessible.at("Kakariko Gorge Double Clawshot Chest")==Truth::no &&
            model.accessible.at("Talo Sharpshooting")==Truth::no,
            "first Kakariko visit bypasses local equipment/story gates");

    model.inventory["Vanilla Eldin Twilight Cleared"]=1;
    model.inventory["Vanilla Current Wolf Form"]=0;
    model.solve();
    require(model.reached.at("Lower Kakariko Village")[0]==Truth::yes &&
            model.reached.at("Kakariko Malo Mart")[0]==Truth::yes,
            "post-Eldin-clear human Kakariko Area Access not recognized");
    require(model.accessible.at("Kakariko Graveyard Lantern Chest")==Truth::yes,
            "post-Eldin-clear lantern route remains locked");
    require(model.accessible.at("Barnes Bomb Bag")==Truth::no,
            "restoring Eldin light alone prematurely opened Barnes shop");
    require(!model.enabled(check(model,"Sanctuary Basement Twilit Insect 1")),
            "post-Eldin-clear Kakariko insects remain displayed");
    require(model.reached.at("Death Mountain Trail")[0]==Truth::no,
            "Death Mountain climb ignored missing Iron Boots before Goron Mines");
    model.inventory["Iron Boots"]=1;
    model.solve();
    require(model.reached.at("Death Mountain Trail")[0]==Truth::yes,
            "Iron Boots did not unlock native Death Mountain climb");
    require(model.accessible.at("Eldin Spring Underwater Chest")==Truth::no,
            "Iron Boots alone bypassed underwater chest's boulder");
    model.inventory["Ball and Chain"]=1;
    model.solve();
    require(model.accessible.at("Eldin Spring Underwater Chest")==Truth::yes,
            "Kakariko boulder + boots requirement fails with both tools");
    require(model.accessible.at("Kakariko Gorge Double Clawshot Chest")==Truth::no,
            "unrelated boulder tool unlocked double-clawshot check");
    save.events.insert(0x0701);
    save.scan(model);
    model.inventory["Vanilla Goron Mines Complete"]=1;
    model.solve();
    require(model.accessible.at("Barnes Bomb Bag")==Truth::yes &&
            model.checkDetails("Barnes Bomb Bag",false).find("Kakariko Barnes Bomb Shop Lower — OPEN")!=std::string::npos,
            "native Goron-clear event did not open Barnes purchase consistently");
    std::cout << "Kakariko synthetic twilight, restored-light and local-gate snapshots passed\n";
}
int main(int argc, char** argv) {
    try {
        require(argc==2, "catalogue path missing");
        Json data; std::ifstream input(argv[1]); input >> data;
        Model model; model.load(data);
        for (const auto& item : data.at("items")) model.inventory[item.at("Name").get<std::string>()] = 0;
        for (const char* province : {"Faron", "Eldin", "Lanayru"})
            model.inventory[std::string("Vanilla ")+province+" Twilight Cleared"] = 0;
        for (const char* dungeon : {"Forest Temple", "Goron Mines", "Lakebed Temple", "Arbiters Grounds",
                                   "Snowpeak Ruins", "Temple of Time", "City in the Sky", "Palace of Twilight"})
            model.inventory[std::string("Vanilla ")+dungeon+" Complete"] = 0;
        model.inventory["Vanilla Current Wolf Form"] = 0;
        NativeSave save; save.scan(model);
        model.solve();
        require(model.accessible.at("Wooden Sword Chest")==Truth::unknown,
                "missing randomizer metadata silently became normal mode");

        model.obtained.insert("Ordon Shield"); model.skipped.insert("Coro Bottle");
        model.loadVanilla();
        require(model.normalGame && !model.seedLoaded && model.seed.empty(), "normal mode forged a seed");
        require(model.obtained.contains("Ordon Shield") && model.skipped.contains("Coro Bottle"), "mode switch lost journal");
        require(!model.shuffled && model.entrances.empty(), "normal mode retained randomized routes");
        model.solve();
        require(model.accessible.at("Wooden Sword Chest")==Truth::yes, "normal start remains UNKNOWN");
        require(model.accessible.at("Faron Field Male Beetle")==Truth::no, "normal start unlocked distant fields");
        require(model.accessible.at("Coro Lantern")==Truth::no, "normal start skipped the Ordon prologue gate");
        model.inventory["Progressive Sword"]=1; model.inventory["Slingshot"]=1;
        model.solve();
        require(model.accessible.at("Coro Lantern")==Truth::yes, "human prologue Woods route not supported");
        require(model.accessible.at("South Faron Woods Twilit Insect in Tunnel 1")==Truth::no,
                "pre-twilight human save exposed twilight insects");

        for (const char* name : {"Coro Lantern", "Kakariko Village Malo Mart Hylian Shield", "Faron Woods Golden Wolf",
                                 "Faron Woods Owl Statue Sky Character", "Faron Field Male Beetle"})
            require(model.enabled(check(model,name)), "normal collectible hidden by randomizer category settings");
        require(!model.enabled(check(model,"Ordon Hint Sign")), "randomizer-only sign appears in normal guide");
        require(!model.enabled(check(model,"Ordon Rupee Under Tall Tree 1")), "patched rupee flag used for native pickup");
        require(!model.give("Ordon Rupee Under Tall Tree 1") && !model.give("Ordon Hint Sign"),
                "unsupported normal check polluted completion journal");

        // Actual story state, not a hypothetical ability to complete a dungeon.
        save.events.insert(0x0502); save.first.insert(0xfe); save.first.insert(0xee); save.scan(model);
        model.inventory["Vanilla Faron Twilight Cleared"]=1;
        require(!model.enabled(check(model,"South Faron Woods Twilit Insect in Tunnel 1")),
                "cleared twilight province retains unavailable insects");
        model.inventory["Lantern"]=1; model.inventory["Gale Boomerang"]=1;
        model.solve();
        require(model.accessible.at("Coro Lantern")==Truth::yes, "post-prologue native path inaccessible");
        require(model.accessible.at("Faron Field Male Beetle")==Truth::no, "forest exit ignores undefeated boss");
        model.inventory["Vanilla Forest Temple Complete"]=1;
        model.solve();
        require(model.accessible.at("Faron Field Male Beetle")==Truth::yes, "cleared Forest Temple does not open field");
        model.inventory["Male Beetle"]=1;
        model.solve();
        require(model.accessible.at("Agitha Male Beetle Reward")!=Truth::yes,
                "owning a bug bypasses normal Castle Town area access");
        require(model.evaluate("'Can_Complete_Forest_Temple'")==Truth::yes, "native completed boss lost to missing equipment");
        model.inventory["Gale Boomerang"]=0;
        model.solve();
        require(model.accessible.at("Faron Field Tree Heart Piece")==Truth::no, "normal mode bypassed local equipment");
        model.inventory["Gale Boomerang"]=1;
        model.solve();
        require(model.accessible.at("Faron Field Tree Heart Piece")==Truth::yes, "normal item acquisition failed to update access");
        kakarikoRegressions(model,save);

        model.inventory["Faron Twilight Tear"]=16;
        model.inventory["Vanilla Faron Twilight Cleared"]=0;
        require(model.evaluate("Can_Complete_Faron_Twilight")==Truth::no, "tear count overwrote native twilight completion");
        model.inventory["Vanilla Faron Twilight Cleared"]=1;
        require(model.evaluate("Can_Complete_Faron_Twilight")==Truth::yes, "native twilight completion ignored");
        model.inventory["Vanilla Current Wolf Form"]=1;
        require(model.evaluate("Starting_Form == Wolf")==Truth::yes, "forced-wolf phase modeled as human");
        require(model.evaluate("Human_Link",2)==Truth::no, "normal mode conflated human and wolf requirements");
        require(model.evaluate("Can_Defeat_Shadow_Beast",2)==Truth::yes &&
                model.evaluate("Can_Defeat_Shadow_Beast",4)==Truth::yes,
                "healthy early-game wolf cannot defeat shadow beasts before crystal");
        model.events["Can Warp"]=Truth::yes;
        require(model.evaluate("Can_Use_Warp_Portals",2)==Truth::no,
                "portal ownership bypassed native first-warp tutorial");
        save.events.insert(0x0604); save.scan(model);
        require(model.evaluate("Can_Use_Warp_Portals",2)==Truth::yes, "native warp unlock not recognized");
        save.events.insert(0x0c01); save.scan(model);
        require(model.evaluate("Can_Defeat_Shadow_Beast",2)==Truth::no &&
                model.evaluate("Can_Use_Warp_Portals",2)==Truth::no, "MDH restriction ignored");
        save.events.insert(0x0d04); save.scan(model);
        require(model.evaluate("Can_Defeat_Shadow_Beast",2)==Truth::yes &&
                model.evaluate("Can_Use_Warp_Portals",2)==Truth::yes, "post-MDH recovery not recognized");

        // Native quest slots differ from the randomizer and must survive hand-in.
        save.slots[19]=0x90; save.scan(model);
        require(model.inventory["Aurus Memo"]==1, "native memo slot not read");
        save.slots[19]=0xff; save.events.insert(0x2680); save.scan(model);
        require(model.inventory["Aurus Memo"]==1, "consumed memo revoked desert access");
        save.slots[7]=0x90; save.events.erase(0x2680); save.scan(model);
        require(model.inventory["Aurus Memo"]==0, "randomizer memo slot leaks into vanilla");
        save.first.insert(0x80); save.first.insert(0x81); save.first.insert(0x82); save.first.insert(0x83);
        save.slots[21]=0x84; save.scan(model);
        for (const char* name : {"Renados Letter", "Invoice", "Wooden Statue", "Ilias Charm"})
            require(model.inventory[name]==1, "consumed native quest prerequisite forgotten");

        save.first.insert(0x4a); save.scan(model);
        require(model.inventory["Progressive Fishing Rod"]==1, "plain native rod falsely grants Coral Earring");
        save.first.insert(0x3d); save.scan(model);
        require(model.inventory["Progressive Fishing Rod"]==2, "native Coral Earring not recognized");

        for (int flag : {0x2904,0x2908,0x2902}) save.events.insert(flag);
        save.scan(model);
        require(model.inventory["Progressive Hidden Skill"]==3, "native learned skill sequence not recognized");
        save.slots[22]=0xea; save.events.insert(0x6080); save.events.insert(0x6204); save.scan(model);
        require(model.inventory["Progressive Sky Book"]==3, "native partial Sky Book or amphitheater character misread");
        save.events.insert(0x6004); save.scan(model);
        require(model.inventory["Progressive Sky Book"]==7, "native completion flag treated as just one character");
        save.events.erase(0x6004); save.slots[22]=0xeb; save.scan(model);
        require(model.inventory["Progressive Sky Book"]==7, "native completed Sky Book not recognized");
        save.switches.insert({4,0x0a}); save.scan(model);
        require(model.inventory["Lake Hylia Portal"]==1 && model.inventory["Zoras Domain Portal"]==0,
                "native portal adapter enables unrelated warps");
        require(model.inventory["Mirror Chamber Portal"]==0, "native portals unlocked wholesale");

        bool rejected=false;
        try { model.loadSeed("invalid"); } catch(const std::exception&) { rejected=true; }
        require(rejected && model.normalGame, "bad seed import destroyed normal mode");
        model.loadSeed("Hash: Regression\n# Settings\nLogic Rules: All Locations Reachable\nStarting Form: Human\nStarting Time of Day: Noon\n");
        require(model.seedLoaded && !model.normalGame, "seed import did not restore randomizer mode");
        model.resetSave();
        require(!model.normalGame && !model.seedLoaded && model.inventory.empty(), "normal state leaked across saves");
        model.solve();
        require(model.accessible.at("Wooden Sword Chest")==Truth::unknown, "reset no-seed path no longer fails closed");
        std::cout << "Normal-game settings, story gates, items, portals and mode transitions passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
