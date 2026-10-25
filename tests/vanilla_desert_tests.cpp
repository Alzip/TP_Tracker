#include "model.hpp"
#include "map_positions.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tracker;
static void require(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
static const Json& check(const Model& m,const char* name) {
    for(const auto& c:m.catalogue.at("checks")) if(c.at("name")==name) return c;
    throw std::runtime_error(std::string("Missing check: ")+name);
}
static Model desert(const Json& data) {
    Model m; m.load(data); m.loadVanilla();
    for(const auto& i:data.at("items")) m.inventory[i.at("Name").get<std::string>()]=0;
    for(const char* p:{"Faron","Eldin","Lanayru"}) m.inventory[std::string("Vanilla ")+p+" Twilight Cleared"]=1;
    for(const char* d:{"Forest Temple","Goron Mines","Lakebed Temple","Arbiters Grounds","Snowpeak Ruins","Temple of Time","City in the Sky","Palace of Twilight"})
        m.inventory[std::string("Vanilla ")+d+" Complete"]=0;
    for(const char* d:{"Forest Temple","Goron Mines","Lakebed Temple"}) m.inventory[std::string("Vanilla ")+d+" Complete"]=1;
    for(const char* i:{"Vanilla Prologue Complete","Vanilla Midna Available","Vanilla Warp Unlocked","Vanilla Barnes Shop Open","Shadow Crystal","Progressive Clawshot","Progressive Bow","Lantern","Iron Boots","Bomb Bag","Vanilla Desert Visited"}) m.inventory[i]=1;
    m.inventory["Progressive Sword"]=3;
    m.inventory["Vanilla Current Wolf Form"]=0;
    m.inventory["Vanilla Bulblin Camp Cleared"]=0;
    m.inventory["Aurus Memo"]=0; // The durable visit must work after a consumed/untracked quest item.
    m.inventory["Desert Map Unlocked"]=1;
    m.stage="F_SP124"; m.room=0; m.layer=0;
    return m;
}
int main(int argc,char** argv) {
 try {
    require(argc==2,"Pass catalogue path"); Json data; std::ifstream f(argv[1]); f>>data;
    auto m=desert(data); m.solve();
    for(const char* name:{"Gerudo Desert East Canyon Chest","Gerudo Desert Lone Small Chest","Gerudo Desert Peahat Ledge Chest","Gerudo Desert West Canyon Chest","Gerudo Desert Campfire East Chest","Gerudo Desert Campfire North Chest","Gerudo Desert Campfire West Chest","Gerudo Desert Northeast Chest Behind Gates","Gerudo Desert Northwest Chest Behind Gates","Gerudo Desert North Small Chest Before Bulblin Camp","Bulblin Camp First Chest Under Tower At Entrance","Bulblin Camp Small Chest in Back of Camp"}) {
        require(m.accessible.at(name)==Truth::yes,"Visited desert chest is still locked");
        require(mapCheckVisible(m,check(m,name),true),"Accessible-only map hides a reachable chest");
    }
    require(m.accessible.at("Gerudo Desert Owl Statue Chest")==Truth::no,"Visit bypassed Dominion Rod");
    require(m.accessible.at("Outside Arbiters Grounds Lantern Chest")==Truth::no,"Camp exit opened before escape");
    m.inventory["Vanilla Bulblin Camp Cleared"]=1; m.solve();
    require(m.accessible.at("Outside Arbiters Grounds Lantern Chest")==Truth::yes,"Native cleared camp still requires a randomizer key");
    m.inventory["Lantern"]=0; m.solve();
    require(m.accessible.at("Outside Arbiters Grounds Lantern Chest")==Truth::no,"Camp clear bypassed Lantern");
    m=desert(data); m.inventory["Progressive Clawshot"]=0; m.solve();
    require(m.accessible.at("Gerudo Desert Peahat Ledge Chest")==Truth::no,"Visit bypassed Clawshot");
    require(m.accessible.at("Gerudo Desert East Canyon Chest")==Truth::yes,"Missing Clawshot locked ordinary chest");
    m=desert(data); m.inventory["Vanilla Current Wolf Form"]=1; m.solve();
    require(m.accessible.at("Gerudo Desert Campfire East Chest")==Truth::yes,"Transformable wolf lost boar route");
    m.inventory["Shadow Crystal"]=0; m.solve();
    require(m.accessible.at("Gerudo Desert Campfire East Chest")==Truth::no,"Wolf without transformation gained boar route");
    m=desert(data); m.inventory["Vanilla Desert Visited"]=0; m.solve();
    require(m.accessible.at("Gerudo Desert East Canyon Chest")==Truth::no,"Stage name/map bit alone unlocked desert");
    m=desert(data); m.inventory.erase("Vanilla Current Wolf Form"); m.inventory["Bomb Bag"]=0; m.solve();
    require(m.accessible.at("Gerudo Desert East Canyon Chest")!=Truth::yes,"Unknown form fabricated confirmed access");
    m=desert(data); m.normalGame=false; m.seedLoaded=true; m.solve();
    require(m.accessible.at("Gerudo Desert East Canyon Chest")!=Truth::yes,"Native visit anchor leaked into randomizer");
    std::cout<<"Native desert access, local gates and map-filter regressions passed\n";
 } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
