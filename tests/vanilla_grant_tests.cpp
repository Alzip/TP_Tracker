#include "vanilla_desert.hpp"
#include "localization.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tracker;
static void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
static const Json& check(const Model& m, const std::string& name) {
    for (const auto& c : m.catalogue.at("checks")) if (c.at("name") == name) return c;
    throw std::runtime_error("Missing check: " + name);
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Pass catalogue path");
        Json data; std::ifstream input(argv[1]); input >> data;
        Model m; m.load(data); m.loadVanilla();
        const std::string front="Bulblin Camp First Chest Under Tower At Entrance";
        const std::string back="Bulblin Camp Small Chest in Back of Camp";
        std::set<int> events;
        std::map<std::string,int> inventory;
        auto observe=[&](int keys, bool recorded) {
            vanillaDesertProgress(inventory,keys,recorded,[&](int event){return events.contains(event);});
        };
        observe(0,false);
        require(inventory.at("Vanilla Desert Visited")==0,"Fabricated desert visit");
        require(inventory.at("Gerudo Desert Bulblin Camp Key")==0,"Fabricated native key");
        events.insert(0x4008); observe(0,false);
        require(inventory.at("Vanilla Desert Visited")==1,"First visit event ignored");
        require(inventory.at("Vanilla Bulblin Camp Cleared")==0,"First visit opened camp exit");
        observe(1,false);
        require(vanillaDesertCompletion(check(m,"Bulblin Guard Key"),inventory)==true,"Native key counter ignored");
        observe(0,true);
        require(inventory.at("Gerudo Desert Bulblin Camp Key")==1,"Consumed observed key forgotten");
        events={0x0b40}; observe(0,false);
        require(inventory.at("Vanilla Bulblin Camp Cleared")==1,"Native escape flag ignored");
        require(inventory.at("Gerudo Desert Bulblin Camp Key")==1,"Cleared camp forgot key prerequisite");
        require(inventory.at("Vanilla Desert Visited")==1,"Camp escape did not prove desert visit");
        require(vanillaDesertCompletion(check(m,front),inventory)==false,"Shared chest accepted randomizer flag 31");
        require(vanillaDesertCompletion(check(m,back),inventory)==false,"Shared chest accepted randomizer flag 30");
        require(!vanillaDesertCompletion(check(m,"Gerudo Desert East Canyon Chest"),inventory).has_value(),"Ordinary native chest flag suppressed");

        require(vanillaGrantName("chest:F_SP118:63",1,0x0f)==front,"Native arrows chest mapping wrong");
        require(vanillaGrantName("chest:F_SP118:63",1,0x05)==back,"Native rupee chest mapping wrong");
        require(vanillaGrantName("chest:F_SP118:63",0,0x05).empty(),"Wrong room accepted");
        require(vanillaGrantName("chest:F_SP118:63",1,0x04).empty(),"Unknown reward matched");
        require(vanillaGrantName("",1,0x05).empty(),"Unnamed rupee pickup matched chest");
        require(vanillaGrantName("chest:F_SP124:0",0,0x05)=="chest:F_SP124:0","Ordinary chest tag changed");
        require(vanillaGrantName("chest:F_SP118:31",1,0x0f)=="chest:F_SP118:31","Randomizer-style ID rewritten");
        std::set<std::string> journal;
        require(recordTrackedGrant(m,journal,vanillaGrantName("chest:F_SP118:63",1,0x0f)),"Actual native grant not journaled");
        require(m.obtained.contains(front)&&journal.contains(front),"First native chest not completed");
        require(!m.obtained.contains(back)&&!journal.contains(back),"Shared flag falsely completed second chest");
        require(!recordTrackedGrant(m,journal,vanillaGrantName("chest:F_SP118:63",1,0x0f)),"Duplicate grant not idempotent");
        // Reproduce a scan that wins the race with the actual grant observer.
        m.obtained.insert(back);
        require(recordTrackedGrant(m,journal,vanillaGrantName("chest:F_SP118:63",1,0x05)),"Already-scanned grant missing from journal");
        require(journal.contains(back),"Second native reward not retained");
        require(!recordTrackedGrant(m,journal,"unknown:check"),"Unknown grant recorded");
        const Json serialized=journal;
        auto restored=Json::parse(serialized.dump()).get<std::set<std::string>>();
        m.obtained.clear(); m.obtained=restored;
        require(m.obtained.contains(front)&&m.obtained.contains(back),"Separate rewards lost on journal reload");
        require(toggleSharedCampCompletion(m,journal,front),"Recovery undo failed");
        require(!m.obtained.contains(front)&&m.obtained.contains(back),"Recovery undo affected wrong chest");
        m.skipped.insert(front);
        require(toggleSharedCampCompletion(m,journal,front),"Recovery mark failed");
        require(m.obtained.contains(front)&&!m.skipped.contains(front),"Recovery mark did not clear skip");
        require(!toggleSharedCampCompletion(m,journal,"Wooden Sword Chest"),"Recovery enabled for unrelated chest");
        m.normalGame=false;
        require(!toggleSharedCampCompletion(m,journal,front),"Native recovery leaked into randomizer");
        m.normalGame=true;
        for(const auto& c:data.at("checks")) if(!m.enabled(c)) {
            require(!recordTrackedGrant(m,journal,c.at("name").get<std::string>()),"Disabled native check journaled");
            break;
        }
        auto base=std::string(argv[1]); base=base.substr(0,base.find_last_of("/\\")+1);
        Json translations; std::ifstream ko(base+"ko.json"); ko>>translations;
        Localization loc; loc.load(translations); loc.setEnabled(true);
        require(loc.stringLookup("Mark collected (tracker only)")!="Mark collected (tracker only)","Recovery label not localized");
        require(loc.rml(m.checkDetails(front)).find("These two native chests")==std::string::npos,"Recovery note not localized");
        std::cout<<"Native grant mapping, race, key, journal and manual recovery regressions passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
