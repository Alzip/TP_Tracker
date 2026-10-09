#pragma once
#include "model.hpp"
#include <algorithm>
#include <cctype>
namespace tracker {
inline uint64_t entranceKey(std::string_view name) {
    uint64_t hash=14695981039346656037ull;
    for(unsigned char c:name) { hash^=c; hash*=1099511628211ull; }
    return hash;
}
struct EntranceSpawn {
    std::string stage;
    int room=-1, point=0, layer=-1;
    bool matches(const Json& edge) const {
        return edge.at("stage")==stage && edge.at("room")==room && edge.at("spawn")==point &&
               (edge.at("layer")==-1 || edge.at("layer")==layer);
    }
};
// No guessing from rooms alone: identify the requested native spawn, verify
// its rewritten target, then commit only when the engine starts that scene.
struct EntranceJourney {
    std::string seed, source, target;
    void reset() { seed.clear(); source.clear(); target.clear(); }
    const Json* endpoint(const Model& model,const std::string& id) const {
        if(!model.catalogue.contains("entrance_edges")) return nullptr;
        for(const auto& e:model.catalogue.at("entrance_edges")) if(e.at("id")==id) return &e;
        return nullptr;
    }
    void begin(const Model& model,const EntranceSpawn& request,const std::string& fromStage,int fromRoom) {
        reset(); if(!model.shuffled || !model.catalogue.contains("entrance_edges")) return;
        int specificity=-1;
        for(const auto& e:model.catalogue.at("entrance_edges")) {
            const auto id=e.at("id").get<std::string>();
            if(!model.entranceTargets.contains(id) || !request.matches(e)) continue;
            if(const auto* reverse=endpoint(model,e.at("reverse")); reverse &&
               (reverse->at("stage")!=fromStage || reverse->at("room")!=fromRoom)) continue;
            int score=e.at("layer")==request.layer ? 1 : 0;
            if(score<specificity) continue;
            if(score==specificity && source!=id) { source.clear(); continue; } // ambiguous: reveal nothing
            source=id; specificity=score;
        }
        if(source.empty()) return;
        seed=model.seed; target=model.entranceTargets.at(source);
    }
    void validateRedirect(const Model& model,const EntranceSpawn& redirected) {
        const auto* e=endpoint(model,target);
        if(seed!=model.seed || !e || !redirected.matches(*e)) reset();
    }
    bool arrive(Model& model,const EntranceSpawn& actual) {
        const auto* e=endpoint(model,target);
        bool changed=seed==model.seed && !source.empty() && e && actual.matches(*e) && model.visitedEntrances.insert(source).second;
        reset(); return changed;
    }
};
struct EntranceJournalEntry {
    std::string source, destination;
};
struct EntranceJournalView {
    size_t discovered=0;
    std::vector<EntranceJournalEntry> entries;
};
inline EntranceJournalView entranceJournalView(const Model& model,std::string query) {
    EntranceJournalView view;
    if(!model.catalogue.contains("entrance_edges")) return view;
    auto lower=[](std::string text) {
        std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    };
    const auto first=query.find_first_not_of(" \t\r\n");
    query=first==std::string::npos ? "" : query.substr(first,query.find_last_not_of(" \t\r\n")-first+1);
    query=lower(query);
    EntranceJourney journey;
    for(const auto& edge:model.catalogue.at("entrance_edges")) {
        const auto id=edge.at("id").get<std::string>();
        // Filter only confirmed discoveries; searching must never probe the full seed table.
        if(!model.visitedEntrances.contains(id) || !model.entranceTargets.contains(id)) continue;
        const auto* target=journey.endpoint(model,model.entranceTargets.at(id));
        if(!target) continue;
        ++view.discovered;
        const auto source=model.settings.value("Decouple Double Door Entrances",Json("Off"))=="On" &&
                          !edge.at("couple").get<std::string>().empty() ? id : edge.at("label").get<std::string>();
        const auto destination=target->at("target").get<std::string>();
        if(!query.empty() && lower(source).find(query)==std::string::npos && lower(destination).find(query)==std::string::npos) continue;
        view.entries.push_back({source,destination});
    }
    std::sort(view.entries.begin(),view.entries.end(),[](const auto& a,const auto& b) { return a.source<b.source; });
    return view;
}

}
