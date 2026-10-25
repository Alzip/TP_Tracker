#!/usr/bin/env python3
"""Apply and verify the reviewed, normal-game-only access patch."""
from pathlib import Path
import hashlib


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError("Source context changed; refusing to apply patch")
    return text.replace(old, new, 1)


p = Path('src/model.cpp')
s = p.read_text(encoding='utf-8')
s = replace_once(s, '''        const std::string native = "Human_Link and Vanilla_Barnes_Shop_Open";
        return requirement == "Nothing" ? native : "(" + requirement + ") and " + native;''', '''        // Native purchase is available when Barnes opens after Goron Mines.
        // Requiring the randomizer's large-rupee farm (Lake Hylia) creates a
        // cycle: bombs -> northern rocks -> Lake Hylia -> bomb purchase/refill.
        return "Human_Link and Vanilla_Barnes_Shop_Open and 'Can_Farm_Rupees'";''')
s = replace_once(s, 'std::string Model::checkDetails(', '''static std::string areaEventRequirement(const std::string& area, const std::string& event,
                                        const std::string& requirement, bool normalGame) {
    if (normalGame && area == "Kakariko Barnes Bomb Shop Lower" && event == "Can Refill Regular Bombs")
        return "Human_Link and Vanilla_Barnes_Shop_Open and 'Can_Farm_Rupees'";
    return requirement;
}
std::string Model::checkDetails(''')
s = replace_once(s, '                const auto definition=words(scalar(value)); expressions+=" "+definition;', '''                const auto nativeRequirement=areaEventRequirement(area.at("Name").get<std::string>(),event,scalar(value),normalGame);
                const auto definition=words(nativeRequirement); expressions+=" "+definition;''')
s = replace_once(s, 'escape(displayLogic(scalar(value)))+"</div>";', 'escape(displayLogic(nativeRequirement))+"</div>";')
s = replace_once(s, '    reached["Root"][4] = Truth::no;\n', '''    reached["Root"][4] = Truth::no;
    // Restoring Lanayru places native human Link at Lake Hylia. That completed
    // story transition is an additional proven starting point, independent of
    // retracing the randomizer's fixed Ordon spawn with the present loadout.
    // Do not infer this from a stage name, tear count, or an unknown/wolf form;
    // caves, equipment checks and the final castle barrier retain their gates.
    if (normalGame && reached.contains("Lake Hylia") &&
        evaluate("Starting_Form == Human") == Truth::yes &&
        evaluate("Can_Complete_Prologue and Can_Complete_Lanayru_Twilight") == Truth::yes)
        reached.at("Lake Hylia")[0] = Truth::yes;
''')
s = replace_once(s, '                            promote(events[event], both(reach, evaluate(scalar(requirement), f)));', '''                            promote(events[event], both(reach, evaluate(
                                areaEventRequirement(name,event,scalar(requirement),normalGame), f)));''')
p.write_text(s, encoding='utf-8', newline='\n')
p = Path('CMakeLists.txt')
s = replace_once(p.read_text(encoding='utf-8'), '    add_executable(tptracker_vanilla_completion_tests tests/vanilla_completion_tests.cpp)', '''    add_executable(tptracker_vanilla_access_tests tests/vanilla_access_tests.cpp src/model.cpp)
    target_include_directories(tptracker_vanilla_access_tests PRIVATE src)
    add_test(NAME tptracker_vanilla_access COMMAND tptracker_vanilla_access_tests "${CMAKE_CURRENT_SOURCE_DIR}/res/catalogue.json")
    add_executable(tptracker_vanilla_completion_tests tests/vanilla_completion_tests.cpp)''')
p.write_text(s, encoding='utf-8', newline='\n')
p = Path('mod.json')
s = replace_once(p.read_text(encoding='utf-8'), '0.4.44+vanilla.2.ko3', '0.4.44+vanilla.3.ko4')
p.write_text(s, encoding='utf-8', newline='\n')
expected = {
    'src/model.cpp': '321286d9add5832e3b91df178e4934728389adf40d2a7835fc4591004df2c44d',
    'CMakeLists.txt': '4d5250f93969188c1f90fb87ad79ec338e76f90249dae5b8e49e210ddccffea4',
    'mod.json': '198244dd6e6e0d31cf588fc19b0f2e1a78a8e77c4a1cde9898b84669b4d800ab',
    'tests/vanilla_access_tests.cpp': '18b48bff2fe6765fcbe12abac1d9253f19e66cbbe06c6b1a042d22799f298cb9',
}
for name, digest in expected.items():
    data = Path(name).read_text(encoding='utf-8').encode('utf-8')
    if hashlib.sha256(data).hexdigest() != digest:
        raise RuntimeError(f'Patched source differs from the locally tested version: {name}')
    print(f'Exact reviewed source verified: {name}')
