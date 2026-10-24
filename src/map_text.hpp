#pragma once
// Portable display-only text layout and validated native-map atlas resources.
// This file deliberately has no game, heap, GX or font-rasterizer dependency.
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace tracker {
struct MapCodepoint {
    uint32_t value;
    size_t offset, bytes;
};
inline std::optional<std::vector<MapCodepoint>> mapCodepoints(std::string_view text) {
    std::vector<MapCodepoint> out;
    for (size_t i = 0; i < text.size();) {
        const size_t start = i;
        const auto first = static_cast<unsigned char>(text[i++]);
        uint32_t cp = first;
        unsigned count = 0;
        uint32_t minimum = 0;
        if (first < 0x80) {}
        else if (first >= 0xC2 && first <= 0xDF) { cp = first & 0x1F; count = 1; minimum = 0x80; }
        else if (first >= 0xE0 && first <= 0xEF) { cp = first & 0x0F; count = 2; minimum = 0x800; }
        else if (first >= 0xF0 && first <= 0xF4) { cp = first & 7; count = 3; minimum = 0x10000; }
        else return std::nullopt;
        if (i + count > text.size()) return std::nullopt;
        for (unsigned n = 0; n < count; ++n) {
            const auto c = static_cast<unsigned char>(text[i++]);
            if ((c & 0xC0) != 0x80) return std::nullopt;
            cp = (cp << 6) | (c & 0x3F);
        }
        if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return std::nullopt;
        out.push_back({cp, start, i - start});
    }
    return out;
}

struct MapTextLine { std::string text; float width = 0; };
struct MapTextLayout { std::vector<MapTextLine> lines; bool truncated = false; };

// Word breaks are preferred; a long word (including Korean without spaces) is
// split only at a Unicode codepoint. Validate the entire input before clipping,
// so an unsupported character cannot hide beyond a successful first line.
template<class Measure>
std::optional<MapTextLayout> layoutMapText(std::string_view text, float maximumWidth,
                                         size_t maximumLines, Measure measure,
                                         std::string_view ellipsis = "...") {
    if (!std::isfinite(maximumWidth) || maximumWidth <= 0 || maximumLines == 0 || maximumLines > 32)
        return std::nullopt;
    const auto decoded = mapCodepoints(text);
    const auto dots = mapCodepoints(ellipsis);
    if (!decoded || !dots) return std::nullopt;
    struct Unit { std::string text; float width; bool space, newline; };
    std::vector<Unit> units;
    auto measured = [&](uint32_t cp) -> std::optional<float> {
        auto value = measure(cp);
        if (!value || !std::isfinite(*value) || *value < 0 || *value > 4096) return std::nullopt;
        return value;
    };
    for (const auto& cp : *decoded) {
        if (cp.value == '\r') continue;
        if (cp.value == '\n') { units.push_back({"", 0, false, true}); continue; }
        if ((cp.value < 32 && cp.value != '\t') || (cp.value >= 0x7F && cp.value < 0xA0)) return std::nullopt;
        const auto width = measured(cp.value == '\t' ? ' ' : cp.value);
        if (!width) return std::nullopt;
        units.push_back({cp.value == '\t' ? " " : std::string(text.substr(cp.offset, cp.bytes)), *width,
                         cp.value == ' ' || cp.value == '\t', false});
    }
    float ellipsisWidth = 0;
    for (const auto& cp : *dots) {
        const auto width = measured(cp.value);
        if (!width) return std::nullopt;
        ellipsisWidth += *width;
    }
    MapTextLayout result;
    size_t begin = 0;
    while (begin < units.size() && result.lines.size() < maximumLines) {
        while (begin < units.size() && units[begin].space) ++begin;
        if (begin == units.size()) break;
        size_t end = begin, next = begin, lastSpace = std::string::npos;
        float width = 0;
        while (end < units.size() && !units[end].newline) {
            if (width + units[end].width > maximumWidth + .001f) break;
            if (units[end].space) lastSpace = end;
            width += units[end++].width;
        }
        if (end < units.size() && units[end].newline) next = end + 1;
        else if (end == units.size()) next = end;
        else if (lastSpace != std::string::npos && lastSpace > begin) { end = lastSpace; next = lastSpace + 1; }
        else next = end;
        // A glyph wider than the entire box cannot be drawn without overflow.
        if (end == begin && !units[begin].newline) { next = begin; result.truncated = true; }
        while (end > begin && units[end - 1].space) --end;
        auto sum = [&](size_t limit) { float value = 0; for (size_t n = begin; n < limit; ++n) value += units[n].width; return value; };
        width = sum(end);
        size_t remaining = next;
        while (remaining < units.size() && units[remaining].space) ++remaining;
        const bool clipped = result.truncated || (result.lines.size() + 1 == maximumLines && remaining < units.size());
        if (clipped) {
            result.truncated = true;
            while (end > begin && width + ellipsisWidth > maximumWidth + .001f) { --end; width -= units[end].width; }
            while (end > begin && units[end - 1].space) { --end; width -= units[end].width; }
        }
        MapTextLine line;
        for (size_t n = begin; n < end; ++n) line.text += units[n].text;
        line.width = std::max(0.f, width);
        if (clipped && ellipsisWidth <= maximumWidth) { line.text += ellipsis; line.width += ellipsisWidth; }
        result.lines.push_back(std::move(line));
        if (clipped) break;
        begin = next;
    }
    return result;
}

struct MapAtlasGlyph {
    uint32_t codepoint = 0;
    size_t page = 0;
    int x = 0, y = 0, width = 0, height = 0;
    float bearingX = 0, bearingY = 0, advance = 0;
};
struct MapAtlasPage {
    int width = 0, height = 0;
    std::vector<unsigned char> bytes;
};
struct MapGlyphQuad {
    float left, top, right, bottom;
    float textureLeft, textureTop, textureRight, textureBottom;
};
inline MapGlyphQuad mapGlyphQuad(const MapAtlasGlyph& glyph, const MapAtlasPage& page,
                                float x, float baseline, float scale) {
    const float left = x + glyph.bearingX * scale, top = baseline + glyph.bearingY * scale;
    const float tx = left - glyph.x * scale, ty = top - glyph.y * scale;
    return {left, top, left + glyph.width * scale, top + glyph.height * scale,
            tx, ty, tx + page.width * scale, ty + page.height * scale};
}

class MapFontAtlas {
    std::map<uint32_t, MapAtlasGlyph> glyphs_;
    std::vector<MapAtlasPage> pages_;
    float fontSize_ = 0;
    static uint16_t be16(const std::vector<unsigned char>& b, size_t i) { return uint16_t((b[i] << 8) | b[i + 1]); }
    static uint32_t be32(const std::vector<unsigned char>& b, size_t i) {
        return (uint32_t(b[i]) << 24) | (uint32_t(b[i + 1]) << 16) | (uint32_t(b[i + 2]) << 8) | b[i + 3];
    }
    static int integer(const nlohmann::json& object, const char* key, int minimum, int maximum) {
        const auto& value = object.at(key);
        if (!value.is_number_integer()) throw std::runtime_error("Map atlas integer expected");
        const auto result = value.get<int64_t>();
        if (result < minimum || result > maximum) throw std::runtime_error("Map atlas integer out of range");
        return static_cast<int>(result);
    }
    static float real(const nlohmann::json& object, const char* key, float minimum, float maximum) {
        const auto& value = object.at(key);
        if (!value.is_number()) throw std::runtime_error("Map atlas metric expected");
        const auto result = value.get<float>();
        if (!std::isfinite(result) || result < minimum || result > maximum) throw std::runtime_error("Map atlas metric out of range");
        return result;
    }
public:
    static uint32_t checksum(const std::vector<unsigned char>& bytes) {
        uint32_t value = 2166136261u;
        for (auto byte : bytes) { value ^= byte; value *= 16777619u; }
        return value;
    }
    // Validate resource names before asking the host to load another resource.
    static std::optional<std::vector<std::string>> resourcePaths(const nlohmann::json& manifest) {
        try {
            if (!manifest.is_object() || integer(manifest, "version", 1, 1) != 1 || manifest.at("format") != "GX_RGBA8" ||
                !manifest.at("pages").is_array() || manifest.at("pages").empty() || manifest.at("pages").size() > 4) return std::nullopt;
            std::vector<std::string> paths;
            for (size_t i = 0; i < manifest.at("pages").size(); ++i) {
                const std::string expected = i == 0 ? "fonts/map-ko.bti" : "fonts/map-ko-" + std::to_string(i) + ".bti";
                if (manifest.at("pages")[i].at("resource") != expected) return std::nullopt;
                paths.push_back(expected);
            }
            return paths;
        } catch (...) { return std::nullopt; }
    }
    void clear() { glyphs_.clear(); pages_.clear(); fontSize_ = 0; }
    bool ready() const { return !pages_.empty() && !glyphs_.empty(); }
    const std::vector<MapAtlasPage>& pages() const { return pages_; }
    float fontSize() const { return fontSize_; }
    const MapAtlasGlyph* glyph(uint32_t cp) const {
        auto found = glyphs_.find(cp); return found == glyphs_.end() ? nullptr : &found->second;
    }
    bool load(const nlohmann::json& manifest, std::vector<std::vector<unsigned char>> textures) {
        clear();
        try {
            const auto paths = resourcePaths(manifest);
            if (!paths || textures.size() != paths->size()) return false;
            if (manifest.at("font") != "NanumGothic-Regular.ttf") return false;
            auto hash = [&](const char* key) {
                const auto value = manifest.at(key).get<std::string>();
                return value.size() == 64 && value.find_first_not_of("0123456789abcdef") == std::string::npos;
            };
            if (!hash("font_sha256") || !hash("text_sha256")) return false;
            const int fontSize = integer(manifest, "font_size", 24, 24);
            const int cell = integer(manifest, "cell_size", 32, 32);
            (void)integer(manifest, "line_height", 32, 32);
            const int count = integer(manifest, "glyph_count", 96, 4096);
            if (!manifest.at("glyphs").is_array() || manifest.at("glyphs").size() != size_t(count)) return false;
            std::vector<MapAtlasPage> pages;
            for (size_t i = 0; i < textures.size(); ++i) {
                const auto& page = manifest.at("pages")[i];
                const int width = integer(page, "width", cell, 1024), height = integer(page, "height", cell, 1024);
                if (width % cell || height % cell || (width & (width - 1)) || (height & (height - 1))) return false;
                auto& bytes = textures[i];
                const size_t expected = 32 + size_t(width) * height * 4;
                if (bytes.size() != expected || integer(page, "byte_size", 32, 32 + 1024 * 1024 * 4) != int(expected)) return false;
                if (!page.at("fnv1a32").is_number_unsigned() && !page.at("fnv1a32").is_number_integer()) return false;
                const auto expectedHash = page.at("fnv1a32").get<int64_t>();
                if (expectedHash < 0 || uint64_t(expectedHash) > UINT32_MAX || uint32_t(expectedHash) != checksum(bytes)) return false;
                if (bytes[0] != 6 || bytes[1] != 2 || be16(bytes, 2) != width || be16(bytes, 4) != height ||
                    bytes[20] != 1 || bytes[21] != 1 || bytes[24] != 1 || be32(bytes, 28) != 32) return false;
                for (size_t n : {6,7,8,9,10,11,12,13,14,15,16,17,18,19,22,23,25,26,27}) if (bytes[n] != 0) return false;
                pages.push_back({width, height, std::move(bytes)});
            }
            std::map<uint32_t, MapAtlasGlyph> glyphs;
            std::set<std::pair<size_t, int>> cells;
            for (const auto& item : manifest.at("glyphs")) {
                const auto cp = uint32_t(integer(item, "codepoint", 32, 0x10FFFF));
                if ((cp >= 0x7F && cp < 0xA0) || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
                MapAtlasGlyph g;
                g.codepoint = cp; g.page = size_t(integer(item, "page", 0, int(pages.size()) - 1));
                const auto& page = pages[g.page];
                g.x = integer(item, "x", 2, page.width - 2); g.y = integer(item, "y", 2, page.height - 2);
                g.width = integer(item, "width", 0, cell - 4); g.height = integer(item, "height", 0, cell - 4);
                g.bearingX = real(item, "bearing_x", -32, 32); g.bearingY = real(item, "bearing_y", -32, 32);
                g.advance = real(item, "advance", .01f, 64);
                if (g.x % cell != 2 || g.y % cell != 2 || g.x + g.width > page.width - 2 || g.y + g.height > page.height - 2 ||
                    (cp == ' ' ? g.width != 0 || g.height != 0 : g.width == 0 || g.height == 0)) return false;
                const int cellIndex = (g.y / cell) * (page.width / cell) + (g.x / cell);
                if (!cells.emplace(g.page, cellIndex).second || !glyphs.emplace(cp, g).second) return false;
            }
            for (uint32_t cp = 32; cp < 127; ++cp) if (!glyphs.contains(cp)) return false;
            if (!glyphs.contains(0x2026)) return false;
            glyphs_ = std::move(glyphs); pages_ = std::move(pages); fontSize_ = float(fontSize);
            return true;
        } catch (...) { clear(); return false; }
    }
    bool supports(std::string_view text) const {
        if (!ready()) return false;
        const auto decoded = mapCodepoints(text);
        if (!decoded) return false;
        for (const auto& cp : *decoded)
            if (cp.value != '\r' && cp.value != '\n' && cp.value != '\t' && !glyph(cp.value)) return false;
        return true;
    }
    std::optional<float> advance(uint32_t cp, float size) const {
        const auto* entry = glyph(cp);
        if (!entry || !std::isfinite(size) || size <= 0) return std::nullopt;
        return entry->advance * size / fontSize_;
    }
    std::optional<MapTextLayout> layout(std::string_view text, float size, float width, size_t lines) const {
        return layoutMapText(text, width, lines, [&](uint32_t cp) { return advance(cp, size); }, "\xE2\x80\xA6");
    }
};

struct MapTextChoice { std::string text; bool atlas = false; };
inline MapTextChoice chooseMapText(const MapFontAtlas& font, bool enabled, std::string_view english,
                                   std::string_view translated) {
    if (enabled && !translated.empty() && font.supports(translated)) return {std::string(translated), true};
    return {std::string(english), false};
}
} // namespace tracker
