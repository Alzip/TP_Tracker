#include "map_text.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tracker;
using Json = nlohmann::json;

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static bool near(float a, float b) { return std::abs(a - b) < .01f; }
static Json readJson(const std::filesystem::path& path) {
    std::ifstream input(path); require(bool(input), "Missing map text fixture");
    Json result; input >> result; return result;
}
static std::vector<unsigned char> readBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary); require(bool(input), "Missing map atlas texture");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
int main(int argc, char** argv) {
    require(argc == 2, "Pass res/ko.json to map text tests");
    const auto decoded = mapCodepoints("A가…");
    require(decoded && decoded->size() == 3 && (*decoded)[1].value == 0xAC00 && (*decoded)[1].offset == 1 && (*decoded)[1].bytes == 3,
            "UTF-8 codepoint boundaries");
    require(mapCodepoints("\xF0\x9F\x8C\x99")->front().value == 0x1F319, "Four-byte Unicode codepoint");
    for (const std::string bad : {std::string("\xC0\xAF", 2), std::string("\xE0\x80\x80", 3), std::string("\xED\xA0\x80", 3),
                                 std::string("\xF4\x90\x80\x80", 4), std::string("\xF0\x80\x80\x80", 4),
                                 std::string("\x80", 1), std::string("\xEA\xB0", 2), std::string("\xC2x", 2)})
        require(!mapCodepoints(bad), "Malformed UTF-8 must be rejected");
    auto measure = [](uint32_t cp) -> std::optional<float> {
        if (cp == 0x1F319) return {};
        return cp == 'i' ? 2.f : cp == '.' ? 2.f : cp == ' ' || cp == 0x2026 ? 5.f : 10.f;
    };
    auto korean = layoutMapText("가나다라마바사", 20, 2, measure, "…");
    require(korean && korean->truncated && korean->lines.size() == 2 && korean->lines[0].text == "가나" && korean->lines[1].text == "다…",
            "Korean wrapping uses codepoint width and two-line ellipsis");
    auto words = layoutMapText("WWW iii WWW", 35, 2, measure);
    require(words && words->truncated && words->lines[0].text == "WWW" && words->lines[1].text == "iii...", "Prefer word boundaries");
    auto whitespace = layoutMapText("  W\ti\nWW  ", 40, 2, measure);
    require(whitespace && !whitespace->truncated && whitespace->lines[0].text == "W i" && near(whitespace->lines[0].width, 17) &&
            whitespace->lines[1].text == "WW", "Whitespace and explicit line breaks");
    auto tiny = layoutMapText("가", 3, 2, measure, "…");
    require(tiny && tiny->truncated && tiny->lines[0].width == 0, "Never draw an oversized glyph outside its box");
    require(!layoutMapText("short \xF0\x9F\x8C\x99", 10, 1, measure), "A missing glyph beyond the truncation point triggers fallback");
    require(!layoutMapText("text", 0, 2, measure) && !layoutMapText("text", 20, 0, measure), "Invalid layout bounds");

    const std::filesystem::path localePath = argv[1];
    const auto root = localePath.parent_path();
    const auto locale = readJson(localePath), manifest = readJson(root / "fonts/map-ko.json");
    const auto paths = MapFontAtlas::resourcePaths(manifest);
    require(bool(paths), "Valid atlas paths");
    std::vector<std::vector<unsigned char>> textures;
    for (const auto& path : *paths) textures.push_back(readBytes(root / path));
    MapFontAtlas atlas;
    require(atlas.load(manifest, textures) && atlas.ready(), "Committed atlas validates");
    require(atlas.glyph('W')->advance > atlas.glyph('i')->advance, "Widths use font metrics, not byte counts");
    size_t checked = 0;
    for (const char* section : {"names", "strings"}) for (const auto& [key, value] : locale.at(section).items()) {
        const auto text = value.get<std::string>();
        require(atlas.supports(text), "Final Korean names and UI strings require complete atlas coverage");
        const auto layout = atlas.layout(text, 16, 200, 2);
        require(layout && layout->lines.size() <= 2, "Every localized label can be laid out");
        for (const auto& line : layout->lines)
            require(mapCodepoints(line.text).has_value() && line.width <= 200.01f, "Rendered lines remain valid UTF-8 inside the tooltip");
        ++checked;
    }
    // The file format can be structurally valid but visually blank. Verify
    // actual tiled GX alpha data for every non-space glyph, including backtick.
    for (const auto& entry : manifest.at("glyphs")) {
        const auto* glyph = atlas.glyph(entry.at("codepoint").get<uint32_t>());
        if (glyph->codepoint == ' ') continue;
        const auto& page = atlas.pages()[glyph->page];
        bool visible = false;
        for (int y = glyph->y; y < glyph->y + glyph->height && !visible; ++y)
            for (int x = glyph->x; x < glyph->x + glyph->width; ++x) {
                const size_t tile = size_t(y / 4) * (page.width / 4) + x / 4;
                const size_t pixel = (y % 4) * 4 + x % 4;
                if (page.bytes[32 + tile * 64 + pixel * 2]) { visible = true; break; }
            }
        require(visible, "Every mapped glyph must have visible alpha pixels");
    }
    const auto* glyph = atlas.glyph(0xAC00);
    require(glyph, "Korean test glyph present");
    const auto& page = atlas.pages()[glyph->page];
    const auto quad = mapGlyphQuad(*glyph, page, 20, 100, .5f);
    require(near((quad.left - quad.textureLeft) / (quad.textureRight - quad.textureLeft), float(glyph->x) / page.width) &&
            near((quad.right - quad.textureRight) / (quad.textureRight - quad.textureLeft) + 1, float(glyph->x + glyph->width) / page.width) &&
            near((quad.top - quad.textureTop) / (quad.textureBottom - quad.textureTop), float(glyph->y) / page.height),
            "drawOut transform samples only the chosen glyph rectangle");
    require(chooseMapText(atlas, true, "English", "가").atlas, "Valid Korean label chooses atlas");
    require(!chooseMapText(atlas, false, "English", "가").atlas, "English setting retains native font");
    auto missing = chooseMapText(atlas, true, "Canonical English", "\xF0\x9F\x8C\x99");
    require(!missing.atlas && missing.text == "Canonical English", "Missing glyph falls back to the entire English label");
    require(!chooseMapText(atlas, true, "Unknown dynamic name", "").atlas, "Unknown names retain English");

    auto invalid = [&](Json broken, std::vector<std::vector<unsigned char>> data, const char* message) {
        MapFontAtlas font; require(!font.load(broken, std::move(data)) && !font.ready(), message);
        require(!chooseMapText(font, true, "English", "가").atlas, "Invalid atlas must use native English");
    };
    auto broken = manifest; broken["pages"][0]["resource"] = "../outside.bti";
    require(!MapFontAtlas::resourcePaths(broken), "Atlas resource paths cannot escape the font resources");
    invalid(broken, textures, "Reject invalid resource name");
    broken = manifest; broken["glyphs"][1]["codepoint"] = 0xD800; invalid(broken, textures, "Reject surrogate glyph");
    broken = manifest; broken["glyphs"][1]["x"] = 1023; invalid(broken, textures, "Reject off-atlas glyph");
    broken = manifest; broken["glyphs"][1]["codepoint"] = broken["glyphs"][0]["codepoint"]; invalid(broken, textures, "Reject duplicate codepoints");
    broken = manifest; broken["glyphs"][1]["page"] = 4; invalid(broken, textures, "Reject invalid texture page");
    broken = manifest; broken["glyphs"][1]["advance"] = -1; invalid(broken, textures, "Reject negative advance");
    auto damaged = textures; damaged[0].pop_back(); invalid(manifest, damaged, "Reject truncated BTI");
    damaged = textures; damaged[0][64] ^= 1; invalid(manifest, damaged, "Reject texture checksum mismatch");
    for (size_t field : {size_t(0), size_t(2), size_t(8), size_t(16), size_t(28)}) {
        damaged = textures; damaged[0][field] ^= 1; broken = manifest;
        broken["pages"][0]["fnv1a32"] = MapFontAtlas::checksum(damaged[0]);
        invalid(broken, damaged, "Reject wrong BTI format, dimensions, palette, mipmaps or offset even with a matching checksum");
    }
    require(!atlas.load(broken, damaged) && !atlas.ready(), "A failed reload clears earlier atlas state");
    std::cout << "Native map UTF-8 layout, fallback, GX atlas bounds and " << checked << " localized labels passed\n";
}
