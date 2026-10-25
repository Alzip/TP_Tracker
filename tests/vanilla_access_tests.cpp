#include "model.hpp"
#include "vanilla.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tracker;
static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static Truth area(const Model& m, const char* name) {
    auto found=m.reached.find(name);
    if(found==m.reached.end()) return Truth::unknown;
    Truth result=Truth::no;
    for(auto form:found->second) result=either(result,form);
    return result;
}
static Model snapshot(const Json& data, bool lanayruCleared) {
    Model m; m.load(data); m.loadVanilla();
    for(const auto& item:data.at("items")) m.inventory[item.at("Name").get<std::string>()]=0;
    vanillaInventory(m.inventory,
        [](int id){return id==0xfe || id==0xee;},
        [](int){return 0xff;},
        [](int flag){return flag==0x0502 || flag==0x0604 || flag==0x0701;},
        [](int,int){return false;});
    for(const char* province:{"Faron","Eldin","Lanayru"})
        m.inventory[std::string("Vanilla ")+province+" Twilight Cleared"]=1;
    m.inventory["Vanilla Lanayru Twilight Cleared"]=lanayruCleared;
    for(const char* dungeon:{"Forest Temple","Goron Mines","Lakebed Temple","Arbiters Grounds",
                            "Snowpeak Ruins","Temple of Time","City in the Sky","Palace of Twilight"})
        m.inventory[std::string("Vanilla ")+dungeon+" Complete"]=0;
    m.inventory["Vanilla Forest Temple Complete"]=m.inventory["Vanilla Goron Mines Complete"]=1;
    for(const char* item:{"Lantern","Slingshot","Gale Boomerang","Iron Boots","Bomb Bag",
                         "Progressive Bow","Progressive Hidden Skill"}) m.inventory[item]=1;
    m.inventory["Progressive Sword"]=2;
    m.inventory["Vanilla Current Wolf Form"]=0;
    m.stage="F_SP115"; m.room=0; m.layer=0;
    return m;
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"catalogue path missing");
        Json data; std::ifstream input(argv[1]); input>>data;
        auto restored=snapshot(data,true); restored.solve();
        require(area(restored,"Lake Hylia")==Truth::yes,
                "post-Lanayru human Lake Hylia is still unreachable");
        require(area(restored,"Castle Town Center")==Truth::yes,
                "post-Lanayru human Castle Town is still unreachable");
        require(restored.inventory.at("Gate Keys")==0 && restored.inventory.at("Shadow Crystal")==0,
                "regression fixture accidentally includes later progression");
        require(area(restored,"Hyrule Castle Entrance")==Truth::no,
                "opening Castle Town incorrectly opened the final castle dungeon");
        require(area(restored,"Lake Hylia Lakebed Temple Entrance")==Truth::no &&
                restored.accessible.at("Lanayru Spring Back Room Lantern Chest")==Truth::no,
                "region recovery bypassed Zora Armor or Clawshot requirements");
        require(restored.checkDetails("Lake Hylia Underwater Chest",false).find("Area access: OPEN")!=std::string::npos,
                "detail view disagrees with solved Lake Hylia access");
        std::cout<<"PASS: restored-light human Lake Hylia / Castle Town, no Gate Keys or Shadow Crystal\n";

        // Refilling ordinary bombs at the native shop must not require reaching
        // Lake Hylia first. The upstream randomizer's large-rupee farming event
        // otherwise creates a circular dependency at the northern rock barriers.
        auto before=snapshot(data,false); before.solve();
        require(before.events.at("Can Refill Regular Bombs")==Truth::yes && before.evaluate("Regular_Bombs")==Truth::yes,
                "native Barnes bomb refill still depends on the Lake Hylia rupee event");
        require(before.accessible.at("Barnes Bomb Bag")==Truth::yes,
                "native Barnes purchase still depends on large-rupee farming");
        require(area(before,"North Eldin Field")==Truth::yes,
                "post-Goron native bomb bag does not open the northern field route");
        require(before.reached.at("Lake Hylia")[0]==Truth::no,
                "uncleared Lanayru gained human access");
        std::cout<<"PASS: post-Goron bomb refill and northern route, uncleared Lanayru stays twilight\n";

        // Story completion proves the restoration landing point even with an
        // empty/changed loadout. Do not mark its caves or equipment checks open.
        auto noBag=snapshot(data,true); noBag.inventory["Bomb Bag"]=0; noBag.solve();
        require(area(noBag,"Lake Hylia")==Truth::yes && area(noBag,"Castle Town Center")==Truth::yes,
                "restoration landing was lost when no bomb bag is tracked");
        require(noBag.evaluate("Bombs")==Truth::no &&
                area(noBag,"Lake Hylia Cave Entrance")==Truth::no &&
                noBag.accessible.at("Lake Hylia Underwater Chest")==Truth::yes,
                "landing anchor bypassed local tools or lost an available boots check");
        noBag.inventory["Iron Boots"]=0; noBag.solve();
        require(noBag.accessible.at("Lake Hylia Underwater Chest")==Truth::no,
                "landing anchor bypassed Iron Boots");
        std::cout<<"PASS: empty loadout keeps reachable shore, not gated caves or underwater checks\n";

        auto closed=snapshot(data,false);
        closed.inventory["Vanilla Goron Mines Complete"]=0;
        closed.inventory["Vanilla Barnes Shop Open"]=0;
        closed.inventory["Bomb Bag"]=0; closed.solve();
        require(closed.events.at("Can Refill Regular Bombs")==Truth::no &&
                closed.accessible.at("Barnes Bomb Bag")==Truth::no,
                "Barnes shop opened before its native story event");
        require(area(closed,"Lake Hylia")==Truth::no && area(closed,"Castle Town Center")==Truth::no,
                "current stage alone opened unreached regions");
        std::cout<<"PASS: closed shop and future regions remain closed; stage name is not an unlock\n";

        auto forced=snapshot(data,true);
        forced.inventory["Vanilla Current Wolf Form"]=1;
        forced.inventory["Vanilla Midna Available"]=0; forced.inventory["Bomb Bag"]=0;
        forced.solve();
        require(forced.reached.at("Lake Hylia")[0]==Truth::no &&
                forced.reached.at("Lake Hylia")[1]==Truth::no,
                "past restoration invented human form during a forced-wolf phase");
        auto uncertain=snapshot(data,true); uncertain.inventory.erase("Vanilla Current Wolf Form");
        uncertain.inventory["Bomb Bag"]=0; uncertain.solve();
        require(area(uncertain,"Lake Hylia")!=Truth::yes,
                "missing native form state fabricated confirmed access");
        std::cout<<"PASS: forced-wolf and missing-form snapshots do not invent human access\n";

        restored.inventory["Vanilla Palace of Twilight Complete"]=1; restored.solve();
        require(area(restored,"Hyrule Castle Entrance")==Truth::yes,
                "actual Palace completion did not open the final castle barrier");
        auto randomizer=snapshot(data,true); randomizer.normalGame=false;
        randomizer.seedLoaded=true; randomizer.inventory["Bomb Bag"]=0;
        randomizer.settings["Faron Twilight Cleared"]="On";
        randomizer.settings["Eldin Twilight Cleared"]="On";
        randomizer.settings["Lanayru Twilight Cleared"]="On";
        randomizer.solve();
        require(area(randomizer,"Lake Hylia")!=Truth::yes,
                "native restoration anchor leaked into randomizer mode");
        std::cout<<"PASS: final castle story gate and randomizer isolation\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
