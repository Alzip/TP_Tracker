#pragma once
#include "model.hpp"
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
}
