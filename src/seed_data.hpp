#pragma once
#include "model.hpp"
#include <sstream>

namespace tracker {
struct SeedLog {
    Json settings=Json::object();
    std::string seed;
    std::map<std::string,std::string> placements;
    std::vector<std::pair<std::string,std::string>> connections;
    std::map<std::string,int> starting;
};
// Parse only the native, flat sections we need. Never execute YAML tags or use
// the playthrough as a complete placement/entrance table. World 1 is the local
// single-player world supported by this tracker.
inline SeedLog readSeedLog(const std::string& text) {
    SeedLog result;
    std::string section,line;
    bool world=false;
    std::istringstream input(text);
    while(std::getline(input,line)) {
        if(line.size()>8192) throw std::runtime_error("Seed log line is too long");
        if(line.starts_with("Hash: ")) result.seed=trim(line.substr(6));
        auto clean=trim(line);
        if(clean=="# Settings") { section="settings"; continue; }
        if(line.starts_with("# ") && section=="settings") { section.clear(); continue; }
        if(section=="settings") {
            if(line.empty() || line[0]==' ' || line[0]=='#' || line[0]=='-') continue;
            auto colon=line.find(": "); if(colon==std::string::npos) continue;
            auto value=trim(line.substr(colon+2));
            if(value.size()>1 && value.front()=='"' && value.back()=='"') value=Json::parse(value).get<std::string>();
            result.settings[trim(line.substr(0,colon))]=value;
            continue;
        }
        if(!line.empty() && line[0]!=' ' && line[0]!='\r') {
            section=clean; world=false;
        }
        if(line.starts_with("    World ")) { world=clean=="World 1:"; continue; }
        if(!world) continue;
        if(section=="All Starting Items:" && clean.starts_with("- ")) { ++result.starting[clean.substr(2)]; continue; }
        auto colon=clean.find(": "); if(colon==std::string::npos) continue;
        auto key=trim(clean.substr(0,colon)), value=trim(clean.substr(colon+2));
        if(section=="All Locations:") result.placements[key]=value;
        if(section=="All Entrances:") result.connections.emplace_back(key,value);
    }
    return result;
}
inline std::string generalizedEntrance(std::string name) {
    for(auto word : {" North"," South"," East"," West"," Right"," Left"})
        for(size_t pos;(pos=name.find(word))!=std::string::npos;) name.erase(pos,std::char_traits<char>::length(word));
    return name;
}
inline bool shuffledEdge(const Json& edge,const Json& settings) {
    std::string type=edge.at("type"), key="Randomize "+type+" Entrances";
    if(type=="Spawn") key="Randomize Starting Spawn";
    if(type=="Warp Portal") return false;
    const auto setting=settings.value(key,Json("Off"));
    if(setting=="Off") return false;
    if(type=="Dungeon" && setting!="On + Hyrule Castle" &&
       (edge.at("id")=="Castle Town North Inside Barrier -> Hyrule Castle Entrance" ||
        edge.at("reverse")=="Castle Town North Inside Barrier -> Hyrule Castle Entrance")) return false;
    return true;
}
// Resolve display names against the catalogue, keeping direction and native
// destination identity. Coupled return routes are derived only for coupled seeds.
inline void resolveSeedEntrances(Model& model,const std::vector<std::pair<std::string,std::string>>& connections) {
    model.entranceTargets.clear(); model.entranceCandidates.clear(); model.shuffledEdges.clear();
    if(!model.catalogue.contains("entrance_edges")) return;
    const auto& edges=model.catalogue.at("entrance_edges");
    std::map<std::string,const Json*> byId;
    std::map<std::string,std::vector<const Json*>> byLabel, byTarget;
    std::map<std::string,std::string> representative;
    bool doubleDecoupled=model.settings.value("Decouple Double Door Entrances",Json("Off"))=="On";
    for(const auto& e:edges) {
        std::string id=e.at("id"); byId[id]=&e; representative[id]=id;
    }
    if(!doubleDecoupled) {
        std::map<std::string,std::string> group;
        for(const auto& e:edges) if(!e.at("couple").get<std::string>().empty())
            group[e.at("couple").get<std::string>()+(e.at("primary").get<bool>()?"+":"-")]=e.at("id");
        for(const auto& e:edges) if(!e.at("couple").get<std::string>().empty())
            representative[e.at("id")]=group.at(e.at("couple").get<std::string>()+(e.at("primary").get<bool>()?"+":"-"));
    }
    for(const auto& e:edges) {
        std::string id=e.at("id"), label=e.at("label");
        if(shuffledEdge(e,model.settings)) model.shuffledEdges.insert(id);
        if(representative.at(id)!=id) continue;
        if(!doubleDecoupled && !e.at("couple").get<std::string>().empty()) label=generalizedEntrance(label);
        auto arrow=label.find(" -> ");
        if(arrow==std::string::npos) continue;
        if(!e.value("impossible",false)) byLabel[label].push_back(&e);
        byTarget[label.substr(arrow+4)+" from "+label.substr(0,arrow)].push_back(&e);
    }
    auto assign=[&](const std::string& source,const std::string& target) {
        auto [it,added]=model.entranceTargets.emplace(source,target);
        if(!added && it->second!=target) throw std::runtime_error("Conflicting entrance connections in seed log");
    };
    std::map<std::string,size_t> total, occurrence;
    for(const auto& [source,target]:connections) ++total[source];
    for(const auto& [source,target]:connections) {
        const auto index=occurrence[source]++;
        auto a=byLabel.find(source), b=byTarget.find(target);
        // The native log prints each pool in metadata-ID order. Repeated source
        // aliases must match the entire group; a truncated group stays unknown.
        if(a==byLabel.end() || b==byTarget.end() || total[source]!=a->second.size()) continue;
        const auto& from=*a->second[index];
        auto id=from.at("id").get<std::string>();
        for(const auto* candidate:b->second) model.entranceCandidates[id].insert(candidate->at("id"));
        if(b->second.size()!=1) continue; // seed.dat resolves repeated destination aliases
        const auto& to=*b->second.front();
        assign(id,to.at("id"));
        if(model.settings.value("Decouple Entrances",Json("Off"))!="On" &&
           !from.at("reverse").get<std::string>().empty() && !to.at("reverse").get<std::string>().empty())
            assign(representative.at(to.at("reverse")),representative.at(from.at("reverse")));
    }
    for(const auto& [id,rep]:representative) if(id!=rep && model.entranceTargets.contains(rep))
        model.entranceTargets[id]=model.entranceTargets.at(rep);
    for(const auto& [id,target]:model.entranceTargets) model.entrances[id]=byId.at(target)->at("target");
    // Boss exits can have additional world-graph followers (e.g. Mirror Chamber).
    for(const auto& e:edges) if(model.shuffledEdges.contains(e.at("id").get<std::string>()))
        for(const auto& follower:e.at("followers")) {
            const auto id=follower.get<std::string>(); model.shuffledEdges.insert(id);
            if(model.entrances.contains(e.at("id").get<std::string>())) model.entrances[id]=model.entrances.at(e.at("id").get<std::string>());
        }
    model.entrancesComplete=true;
    for(const auto& id:model.shuffledEdges) if(!model.entrances.contains(id) && (!byId.contains(id) || !byId.at(id)->value("impossible",false))) model.entrancesComplete=false;
    if(!model.shuffled) model.entrancesComplete=true;
}
inline uint64_t nativeEntranceKey(const Json& e) {
    return uint64_t(e.at("stage_id").get<unsigned>()) |
        (uint64_t(uint8_t(e.at("room").get<int>()))<<8) |
        (uint64_t(uint8_t(e.at("layer").get<int>()))<<16) |
        (uint64_t(uint16_t(e.at("spawn").get<int>()))<<32);
}
inline void resolveNativeEntrances(Model& model,const std::string& text) {
    if(!trim(text).starts_with("formatVersion: 3")) throw std::runtime_error("Unsupported native seed format");
    constexpr uint64_t mask=0x0000ffff00ffffffull; // ignore struct padding
    std::map<uint64_t,uint64_t> overrides;
    std::istringstream input(text); std::string line; bool section=false;
    while(std::getline(input,line)) {
        if(trim(line)=="mEntranceOverrides:") { section=true; continue; }
        if(!section) continue;
        if(!line.empty() && line[0]!=' ' && line[0]!='\r') break;
        auto colon=line.find(':'); if(colon==std::string::npos) continue;
        auto parse=[](std::string value) { value=trim(value); size_t used=0; auto n=std::stoull(value,&used); if(used!=value.size()) throw std::runtime_error("Invalid native entrance tuple"); return n; };
        overrides[parse(line.substr(0,colon))&mask]=parse(line.substr(colon+1))&mask;
    }
    const auto& edges=model.catalogue.at("entrance_edges");
    std::map<uint64_t,std::vector<const Json*>> endpoints;
    for(const auto& e:edges) endpoints[nativeEntranceKey(e)].push_back(&e);
    for(const auto& e:edges) {
        auto id=e.at("id").get<std::string>();
        if(!model.shuffledEdges.contains(id)) continue;
        auto native=overrides.find(nativeEntranceKey(e));
        if(native==overrides.end()) continue;
        auto candidates=endpoints.find(native->second);
        if(candidates==endpoints.end()) { model.entrances.erase(id); model.entranceTargets.erase(id); continue; }
        const Json* target=nullptr;
        const auto constraint=model.entranceCandidates.find(id);
        for(const auto* candidate:candidates->second) {
            // Native overrides win if their physical target is unambiguous. A
            // seed can contain two logical sources sharing one native spawn;
            // the game's final override then supersedes one printed log row.
            if(candidates->second.size()>1 && constraint!=model.entranceCandidates.end() && !constraint->second.contains(candidate->at("id").get<std::string>())) continue;
            if(target && target->at("target")!=candidate->at("target")) { target=nullptr; break; }
            target=candidate;
        }
        if(!target) { model.entrances.erase(id); model.entranceTargets.erase(id); continue; }
        model.entranceTargets[id]=target->at("id"); model.entrances[id]=target->at("target");
    }
    for(const auto& e:edges) for(const auto& follower:e.at("followers")) {
        const auto id=e.at("id").get<std::string>();
        if(model.entrances.contains(id)) model.entrances[follower.get<std::string>()]=model.entrances.at(id);
    }
    model.entrancesComplete=true;
    for(const auto& e:edges) if(model.shuffledEdges.contains(e.at("id").get<std::string>()) && !e.value("impossible",false) && !model.entrances.contains(e.at("id").get<std::string>())) model.entrancesComplete=false;
}

}
