from pathlib import Path
p=Path.cwd()
def replace(s,a,b):
    if s.count(a)!=1: raise ValueError(f'Expected one context, found {s.count(a)}: {a[:100]}')
    return s.replace(a,b,1)
f=p/'src/model.cpp'; s=f.read_text(encoding="utf-8")
s=replace(s,'#include "vanilla.hpp"','#include "vanilla.hpp"\n#include "vanilla_desert.hpp"')
s=replace(s,'std::string Model::checkDetails(','''static std::string areaExitRequirement(const std::string& area, const std::string& exit,
                                       const std::string& requirement, bool normalGame) {
    if (normalGame && area == "Bulblin Camp" && exit == "Outside Arbiters Grounds")
        return "Vanilla_Bulblin_Camp_Cleared";
    return requirement;
}
std::string Model::checkDetails(''')
s=replace(s,'    std::string expressions;','''    if (normalGame && vanillaSharedCampChest(name))
        out += "<div class='tp-note'>These two native chests share a save flag. New pickups are recorded separately. For an earlier pickup, use Mark collected; this changes only the tracker.</div>";
    std::string expressions;''')
s=replace(s,'    bool changed = true;\n    size_t passes = 0;', '''    // The native first-visit event proves the desert landing independently of
    // the consumed memo and of unlocked portals. Preserve current form and all
    // local tool/gate requirements. A map bit or a stage name alone is not proof.
    if (normalGame && reached.contains("Gerudo Desert") &&
        evaluate("Vanilla_Desert_Visited") == Truth::yes) {
        const auto form = inventory.find("Vanilla Current Wolf Form");
        if (form != inventory.end() && (form->second == 0 || form->second == 1))
            reached.at("Gerudo Desert")[form->second == 1 ? 2 : 0] = Truth::yes;
    }
    bool changed = true;
    size_t passes = 0;''')
s=replace(s,'auto access = both(reach, evaluate(scalar(requirement), f));','auto access = both(reach, evaluate(areaExitRequirement(name,vanilla,scalar(requirement),normalGame), f));')
s=replace(s,'auto access = both(anyForm, evaluate(scalar(requirement), 4));','auto access = both(anyForm, evaluate(areaExitRequirement(name,vanilla,scalar(requirement),normalGame), 4));')
f.write_text(s, encoding="utf-8", newline="\n")
f=p/'src/mod.cpp';s=f.read_text(encoding="utf-8")
s=replace(s,'#include "vanilla_completion.hpp"','#include "vanilla_completion.hpp"\n#include "vanilla_desert.hpp"')
s=replace(s,'UiElementHandle detailText = 0, detailButton = 0;','UiElementHandle detailText = 0, detailButton = 0, detailCompletionButton = 0;')
s=replace(s,'''    const int last=static_cast<int>(detailSectionCount)-1+(skipSelectedDisabled(nullptr,nullptr) ? 0 : 1);
    if(direction) detailScrollRow=std::clamp(detailScrollRow+(direction==2 ? 1 : -1),0,last);
    detailScrollRow=std::clamp(detailScrollRow,0,last);
    svc_ui->elem_focus(mod_ctx,detailScrollRow==static_cast<int>(detailSectionCount) ? detailButton : detailAnchors[detailScrollRow]);''','''    std::vector<UiElementHandle> targets(detailAnchors.begin(),detailAnchors.begin()+detailSectionCount);
    if (detailButton && !skipSelectedDisabled(nullptr,nullptr)) targets.push_back(detailButton);
    if (detailCompletionButton) targets.push_back(detailCompletionButton);
    const int last=static_cast<int>(targets.size())-1;
    if(direction) detailScrollRow=std::clamp(detailScrollRow+(direction==2 ? 1 : -1),0,last);
    detailScrollRow=std::clamp(detailScrollRow,0,last);
    svc_ui->elem_focus(mod_ctx,targets[detailScrollRow]);''')
s=replace(s,'bool skipSelectedDisabled(ModContext*,void*) {', '''void toggleSelectedCampCompletion(ModContext*, void*) {
    const auto oldJournal=journal, oldObtained=model.obtained, oldSkipped=model.skipped;
    if (tracker::toggleSharedCampCompletion(model,journal,selectedCheck)) {
        if (!persist()) { journal=oldJournal; model.obtained=oldObtained; model.skipped=oldSkipped; }
        ++revision;
    }
}
bool skipSelectedDisabled(ModContext*,void*) {''')
s=replace(s,'    if(detailToggle) setControlLabel(','''    if(detailCompletionButton) setControlLabel(detailCompletionButton,
        journal.contains(selectedCheck) ? "Undo collected mark (tracker only)" : "Mark collected (tracker only)");
    if(detailToggle) setControlLabel(''')
s=replace(s,'''    // Direct-pane rows let native controller focus scroll each guide paragraph''','''    if (model.normalGame && tracker::vanillaSharedCampChest(selectedCheck)) {
        UiControlDesc recovered=UI_CONTROL_DESC_INIT;
        recovered.kind=UI_CONTROL_BUTTON; recovered.label="Mark collected (tracker only)";
        recovered.on_pressed=toggleSelectedCampCompletion;
        result=addControl(left,&recovered,&detailCompletionButton);
        if(result!=MOD_OK) return result;
    }
    // Direct-pane rows let native controller focus scroll each guide paragraph''')
s=replace(s,'detailText=detailButton=detailToggle=0;','detailText=detailButton=detailToggle=detailCompletionButton=0;')
s=replace(s,'    if (model.normalGame) {\n        tracker::vanillaInventory(','''    bool observedNativeKey=false;
    if (model.normalGame) {
        const int campKeys = dStage_stagInfo_GetSaveTbl(stageInfo)==0x0a ? dComIfGs_getKeyNum()
            : dComIfGs_getSaveData()->getSave(0x0a).getBit().getKeyNum();
        // Capture a key observed in the actual desert save table before it is
        // consumed at the gate; never substitute a dungeon's current key count.
        if (campKeys>0) observedNativeKey=journal.insert("Bulblin Guard Key").second;
        tracker::vanillaDesertProgress(inventory,campKeys,journal.contains("Bulblin Guard Key"),
            [](int event) { return savedEvent(static_cast<unsigned>(event)); });
        tracker::vanillaInventory(''')
s=replace(s,'        const auto nativeComplete = model.normalGame ? tracker::vanillaCompletionOverride(check,', '''        auto nativeComplete = model.normalGame ? tracker::vanillaDesertCompletion(check,inventory) : std::nullopt;
        if (!nativeComplete.has_value() && model.normalGame) nativeComplete = tracker::vanillaCompletionOverride(check,''')
s=replace(s,'[](int index) { return dComIfGs_isCollectMirror(index) != 0; }) : std::nullopt;', '[](int index) { return dComIfGs_isCollectMirror(index) != 0; });')
s=replace(s,'    if (unskipped) persist();','    if (unskipped || observedNativeKey) persist();')
s=replace(s,'inventory["Vanilla Current Wolf Form"] = dComIfGp_getLinkPlayer() && daPy_py_c::checkNowWolf();','inventory["Vanilla Current Wolf Form"] = dComIfGp_getLinkPlayer() ? (daPy_py_c::checkNowWolf() ? 1 : 0) : -1;')
s=replace(s,'''    if (info && info->check_name && model.give(info->check_name)) {
        journal.insert(model.aliases.at(info->check_name));
        ++revision;
        persist();
    }''','''    if (info && info->check_name) {
        const std::string name = model.normalGame
            ? tracker::vanillaGrantName(info->check_name,dComIfGp_roomControl_getStayNo(),info->item)
            : std::string(info->check_name);
        if (tracker::recordTrackedGrant(model,journal,name)) { ++revision; persist(); }
    }''')
f.write_text(s, encoding="utf-8", newline="\n")
f=p/'res/ko.json';s=f.read_text(encoding="utf-8")
s=replace(s,'    "Vanilla Barnes Shop Open": "일반 게임 반즈의 폭탄 가게 개점",','''    "Vanilla Barnes Shop Open": "일반 게임 반즈의 폭탄 가게 개점",
    "Vanilla Desert Visited": "일반 게임 겔드 사막 방문 완료",
    "Vanilla Bulblin Camp Cleared": "일반 게임 불블린 야영지 탈출 완료",''')
s=replace(s,'  "strings": {','''  "strings": {
    "Mark collected (tracker only)": "획득 완료로 표시 (트래커만)",
    "Undo collected mark (tracker only)": "획득 완료 표시 취소 (트래커만)",
    "These two native chests share a save flag. New pickups are recorded separately. For an earlier pickup, use Mark collected; this changes only the tracker.": "일반 게임에서 이 두 상자는 저장 플래그를 공유합니다. 이후 획득은 개별 기록하며, 이전에 획득한 상자는 완료로 표시할 수 있습니다. 게임 진행 상태는 변경하지 않습니다.",''')
f.write_text(s, encoding="utf-8", newline="\n")
f=p/'CMakeLists.txt';s=f.read_text(encoding="utf-8");s=replace(s,'    add_executable(tptracker_vanilla_completion_tests', '''    add_executable(tptracker_vanilla_desert_tests tests/vanilla_desert_tests.cpp src/model.cpp)
    target_include_directories(tptracker_vanilla_desert_tests PRIVATE src)
    add_test(NAME tptracker_vanilla_desert COMMAND tptracker_vanilla_desert_tests "${CMAKE_CURRENT_SOURCE_DIR}/res/catalogue.json")
    add_executable(tptracker_vanilla_grant_tests tests/vanilla_grant_tests.cpp src/model.cpp)
    target_include_directories(tptracker_vanilla_grant_tests PRIVATE src)
    add_test(NAME tptracker_vanilla_grant COMMAND tptracker_vanilla_grant_tests "${CMAKE_CURRENT_SOURCE_DIR}/res/catalogue.json")
    add_executable(tptracker_vanilla_completion_tests''');f.write_text(s, encoding="utf-8", newline="\n")
f=p/'mod.json';f.write_text(replace(f.read_text(encoding="utf-8"),'0.4.44+vanilla.3.ko4','0.4.44+vanilla.4.ko5'), encoding="utf-8", newline="\n")

import hashlib
expected = {'CMakeLists.txt': '2129bfb5eb43f3ec3267e2e30ef10341d7d5c9daa4db5bc404d108615fd141cd', 'DESERT_FIX_KO.md': '10e6ea2bf27ed4e3a2844e842f98fe658ceebf972ffcac03e0bea0b4dbaa9203', 'mod.json': '6fefcca478847337fccca3f1471a2270a042f35356b32b44fb027f856ccc7a89', 'res/ko.json': '7b861d080763768e04b97a086980c229a32d5bb99dbbe02708c6507003686a25', 'src/mod.cpp': '0c3a2fe83ebd8020f584b323a9beed3bfdf3bae074c0c9db2033c2ebd8881cd8', 'src/model.cpp': '583b5d048b336af92b8e59ef9dd796afdf3a7b06f954330dce4ff026bde8d8fb', 'src/vanilla_desert.hpp': '456ce292bbec2f2950df1a7b39ef03929d4a7f86dcfa732ba942edaa9e27a13a', 'tests/vanilla_desert_tests.cpp': 'd40b845cb89a0fb072687a6365078823cd9ada4a31780f351f4fc47225a03653', 'tests/vanilla_grant_tests.cpp': '314f6efbb498281fcaee3f1e7e5d65e9815a5dbef70541b054e0f6175abbe9c8'}
for name, digest in expected.items():
    actual = hashlib.sha256(Path(name).read_text(encoding="utf-8").encode("utf-8")).hexdigest()
    if actual != digest: raise RuntimeError("Reviewed source mismatch: " + name)
    print("Exact reviewed source verified:",name)
