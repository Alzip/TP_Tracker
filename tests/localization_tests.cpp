#include "check_guides.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace tracker;
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::string join(const std::vector<std::string>& blocks) {
    std::string out;
    for (const auto& block : blocks) out += block;
    return out;
}
Json fixture() {
    return {
        {"version", 1}, {"language", "ko"},
        {"strings", {
            {"How to obtain", "획득 방법"}, {"OPEN", "진입 가능"}, {"LOCKED", "잠김"},
            {"Area access: ", "지역 접근: "}, {"Local requirement: ", "현장 조건: "},
            {"No additional items required.", "추가 아이템이 필요하지 않습니다."},
            {"Sources: ", "출처: "}, {"Obtained: ", "획득 수: "}, {"Starting inventory", "시작 아이템"},
            {"Collection directions are unavailable for this check.", "이 항목의 공략은 없습니다."},
            {"Guide text is shown in English; a Korean translation is not yet available.", "미번역 공략은 영어로 표시합니다."},
            {"Collection guide only. The requirements on the left use normal-game story progress and inventory.", "왼쪽 조건은 일반 게임의 진행 상황과 아이템을 사용합니다."},
            {"Unsafe display", "<button>한글 & \"시험\"</button>"}
        }},
        {"names", {
            {"Barnes Bomb Bag", "반즈의 폭탄주머니"}, {"Kakariko Village", "카카리코 마을"},
            {"Progressive Sword", "검 강화"}, {"Link's & Bag", "링크의 <가방> & 물건"}
        }},
        {"guides", {{"Barnes Bomb Bag", {{"steps", Json::array({"반즈에게 <폭탄> & \"주머니\"를 받으세요."})}}}}}
    };
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "catalogue path missing");
        Localization locale;
        locale.setEnabled(true);
        require(!locale.available() && !locale.enabled(), "missing locale was enabled");
        require(locale.stringLookup("OPEN") == "OPEN" && locale.nameLookup("Barnes Bomb Bag") == "Barnes Bomb Bag",
                "missing resource must fall back to English");
        const std::string markup = "<div class='tp-state OPEN' data-name='Barnes Bomb Bag'>OPEN | Kakariko Village</div>";
        require(locale.rml(markup) == markup, "missing resource modified RML");

        const auto source = fixture();
        locale.setEnabled(false);
        locale.load(source);
        require(locale.available() && !locale.enabled(), "loading enabled localization without a choice");
        require(locale.rml(markup) == markup && !locale.guideSteps("Barnes Bomb Bag"), "disabled localization changed display");
        locale.setEnabled(true);
        require(locale.stringLookup("OPEN") == "진입 가능" && locale.stringLookup("untranslated") == "untranslated",
                "string translation or English fallback failed");
        require(locale.nameLookup("Barnes Bomb Bag") == "반즈의 폭탄주머니" && locale.nameLookup("unknown") == "unknown",
                "name translation or canonical fallback failed");
        require(locale.match("Barnes Bomb Bag", "BARNES") && locale.match("Barnes Bomb Bag", "폭탄주머니") &&
                !locale.match("Barnes Bomb Bag", "카카리코"), "English/Korean search mismatch");
        require(locale.rml(markup) == "<div class='tp-state OPEN' data-name='Barnes Bomb Bag'>진입 가능 | 카카리코 마을</div>",
                "display translation changed class or canonical attribute");
        require(locale.rml("<div title='OPEN > LOCKED'>Barnes Bomb Bag</div><!-- OPEN -->") ==
                "<div title='OPEN > LOCKED'>반즈의 폭탄주머니</div><!-- OPEN -->", "quoted attribute or comment was rewritten");
        require(locale.rml("<div>Area access: OPEN</div><div>Local requirement: No additional items required.</div>") ==
                "<div>지역 접근: 진입 가능</div><div>현장 조건: 추가 아이템이 필요하지 않습니다.</div>", "structured requirement labels failed");
        require(locale.rml("<div>Kakariko Village — LOCKED</div><div>Progressive Sword: <span class='OPEN'>2</span></div>") ==
                "<div>카카리코 마을 — 잠김</div><div>검 강화: <span class='OPEN'>2</span></div>", "route or split inventory label failed");
        require(locale.rml("<div>Obtained: 2 / 3</div><div>Starting inventory (1)</div>") ==
                "<div>획득 수: 2 / 3</div><div>시작 아이템 (1)</div>", "item history counts were lost");
        require(locale.rml("<div>Link&#39;s &amp; Bag</div>") == "<div>링크의 &lt;가방&gt; &amp; 물건</div>",
                "escaped names failed to translate safely");
        require(locale.rml("<div>Unsafe display</div>") == "<div>&lt;button&gt;한글 &amp; &quot;시험&quot;&lt;/button&gt;</div>",
                "translation injected markup");
        const std::string unchanged = "<div>Unknown Barnes Bomb Bag text</div><div>Progressive Sword &gt;= 2 and Human Link</div><div>OPEN&nbsp;</div>";
        require(locale.rml(unchanged) == unchanged, "broad replacement changed unknown prose, logic or entity");
        require(locale.rml("<div title='OPEN") == "<div title='OPEN", "malformed tag was rewritten");
        require(source == fixture(), "localization mutated source keys");

        for (int scenario = 0; scenario < 10; ++scenario) {
            auto malformed = source;
            switch (scenario) {
            case 0: malformed["version"] = "1"; break;
            case 1: malformed["version"] = 2; break;
            case 2: malformed["language"] = "ja"; break;
            case 3: malformed.erase("names"); break;
            case 4: malformed["strings"]["OPEN"] = 42; break;
            case 5: malformed["names"]["Barnes Bomb Bag"] = "  "; break;
            case 6: malformed["guides"]["Barnes Bomb Bag"]["steps"] = Json::array(); break;
            case 7: malformed["guides"]["Barnes Bomb Bag"]["steps"][0] = Json::object(); break;
            case 8: malformed["strings"]["OPEN"] = std::string("bad\0text", 8); break;
            case 9: malformed["strings"]["OPEN"] = std::string(1, static_cast<char>(0xff)); break;
            }
            bool rejected = false;
            try { locale.load(malformed); } catch (const std::exception&) { rejected = true; }
            require(rejected && locale.stringLookup("OPEN") == "진입 가능" && locale.guideSteps("Barnes Bomb Bag").has_value(),
                    "malformed resource replaced valid translations");
        }
        Localization badInitial;
        bool rejected = false;
        try { badInitial.load(Json::array()); } catch (const std::exception&) { rejected = true; }
        badInitial.setEnabled(true);
        require(rejected && !badInitial.available() && badInitial.rml(markup) == markup,
                "bad initial resource did not retain English fallback");

        const Json guidesResource = {
            {"version", 1}, {"sources", {{"original", {{"label", "Original & Author"}, {"url", "https://example.org/guide"}}}}},
            {"checks", {
                {"Barnes Bomb Bag", {{"steps", Json::array({"Original English directions."})}, {"sources", Json::array({"original"})}}},
                {"Missing Translation", {{"steps", Json::array({"English fallback directions."})}, {"sources", Json::array({"original"})}}}
            }}
        };
        CheckGuides guides;
        guides.load(guidesResource);
        const auto english = join(guides.blocks("Barnes Bomb Bag"));
        require(english.find("Original English directions.") != std::string::npos && english.find("seed settings") != std::string::npos,
                "default English guide behavior changed");
        const auto korean = join(guides.blocks("Barnes Bomb Bag", &locale, true));
        require(korean.find("획득 방법") != std::string::npos && korean.find("반즈에게 &lt;폭탄&gt; &amp; &quot;주머니&quot;") != std::string::npos &&
                korean.find("Original English directions.") == std::string::npos, "localized guide missing or unsafe");
        require(korean.find("출처: Original &amp; Author") != std::string::npos && korean.find("일반 게임의 진행 상황") != std::string::npos &&
                korean.find("seed settings") == std::string::npos, "localized guide lost credit or used randomizer caveat");
        const auto fallback = join(guides.blocks("Missing Translation", &locale, true));
        require(fallback.find("미번역 공략은 영어로 표시합니다.") != std::string::npos && fallback.find("English fallback directions.") != std::string::npos,
                "partial guide translation hid or failed to label English fallback");
        require(join(guides.blocks("unknown", &locale)).find("이 항목의 공략은 없습니다.") != std::string::npos,
                "missing guide message was not localized");
        locale.setEnabled(false);
        require(join(guides.blocks("Barnes Bomb Bag", &locale)) == english, "English toggle failed to restore original guide");
        require(join(guides.blocks("Barnes Bomb Bag", nullptr, true)).find("normal-game story progress") != std::string::npos,
                "English normal-game guide still describes seed settings");

        // Exercise the shipped resource against the real catalogue and renderers.
        const auto resourceRoot = std::filesystem::path(argv[1]).parent_path();
        std::ifstream catalogueInput(argv[1]), localeInput(resourceRoot / "ko.json"), guidesInput(resourceRoot / "check_guides.json");
        Json catalogue, shippedLocale, shippedGuides;
        catalogueInput >> catalogue; localeInput >> shippedLocale; guidesInput >> shippedGuides;
        locale.load(shippedLocale); locale.setEnabled(true); guides.load(shippedGuides);
        Model model; model.load(catalogue);
        const auto aliases = model.aliases;
        const auto normalizedCatalogue = model.catalogue;
        for (const auto& [canonical, guide] : shippedLocale.at("guides").items()) {
            require(model.aliases.contains(canonical) && model.aliases.at(canonical) == canonical,
                    "translated guide key is not a canonical catalogue ID");
            require(!join(guides.blocks(canonical, &locale, true)).empty(), "shipped translated guide failed to render");
        }
        const std::string check = "Barnes Bomb Bag";
        const auto original = model.checkDetails(check, false);
        const auto rendered = locale.rml(original);
        require(rendered.find(locale.nameLookup(check)) != std::string::npos && model.aliases == aliases && model.catalogue == normalizedCatalogue,
                "localizing real details changed catalogue or alias routes");
        require(locale.nameLookup(check) != check && !model.give(locale.nameLookup(check)) && model.give(check) && model.obtained.contains(check),
                "display name replaced the canonical completion key");
        require(model.checkDetails(check, false).find("DONE") != std::string::npos && !model.obtained.contains(locale.nameLookup(check)),
                "completion did not retain the canonical check ID");
        std::cout << "Localization display/fallback/escaping/ID checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
