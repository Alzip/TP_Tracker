#pragma once
#include "model.hpp"
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace tracker {
// Display data only. Canonical check/item/area names remain the model and save keys.
class Localization {
    Json data_ = Json::object();
    bool enabled_ = false;

    static bool validText(std::string_view text, size_t maximum) {
        if (text.empty() || text.size() > maximum || text.find_first_not_of(" \t\r\n") == std::string_view::npos)
            return false;
        for (unsigned char c : text)
            if (c < 0x20 && c != '\t' && c != '\r' && c != '\n') return false;
        return true;
    }
    const std::string* lookup(std::string_view category, std::string_view key) const {
        if (!enabled()) return nullptr;
        const auto& entries = data_.at(std::string(category));
        auto found = entries.find(std::string(key));
        return found == entries.end() ? nullptr : &found->get_ref<const std::string&>();
    }
    const std::string* textLookup(std::string_view key) const {
        if (const auto* value = lookup("strings", key)) return value;
        return lookup("names", key);
    }
    static std::string lowerAscii(std::string_view text) {
        std::string out(text);
        for (char& c : out) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        return out;
    }
    // Only entities produced by escape(), plus the equivalent apostrophe entity.
    // Other entities make this node ineligible for translation, preserving its bytes.
    static std::optional<std::string> decodeText(std::string_view text) {
        std::string out;
        for (size_t i = 0; i < text.size();) {
            if (text[i] != '&') { out += text[i++]; continue; }
            bool found = false;
            for (const auto& [entity, value] : {
                     std::pair<std::string_view, char>{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'},
                     {"&quot;", '"'}, {"&#39;", '\''}, {"&apos;", '\''}}) {
                if (!text.substr(i).starts_with(entity)) continue;
                out += value; i += entity.size(); found = true; break;
            }
            if (!found) return std::nullopt;
        }
        return out;
    }
    std::string translateText(std::string_view text, unsigned depth = 0) const {
        if (const auto* value = textLookup(text)) return *value;
        if (depth >= 12) return std::string(text);
        const auto first = text.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) return std::string(text);
        const auto end = text.find_last_not_of(" \t\r\n") + 1;
        if (first != 0 || end != text.size())
            return std::string(text.substr(0, first)) + translateText(text.substr(first, end - first), depth + 1) + std::string(text.substr(end));
        // These are the structural separators emitted by checkDetails(). A whole
        // exact key takes precedence; logical AND/OR expressions stay untouched.
        for (std::string_view separator : {" | ", " — "}) {
            const auto split = text.find(separator);
            if (split != std::string_view::npos)
                return translateText(text.substr(0, split), depth + 1) + std::string(separator) + translateText(text.substr(split + separator.size()), depth + 1);
        }
        for (std::string_view prefix : {"Area access: ", "Local requirement: ", "Sources: ", "Obtained: "})
            if (text.starts_with(prefix))
                return stringLookup(prefix) + translateText(text.substr(prefix.size()), depth + 1);
        const auto colon = text.find(": ");
        if (colon != std::string_view::npos)
            if (const auto* value = textLookup(text.substr(0, colon)))
                return *value + ": " + translateText(text.substr(colon + 2), depth + 1);
        // Relevant-item labels often end one text node immediately before a span.
        if (text.ends_with(':')) {
            if (const auto* value = textLookup(text.substr(0, text.size() - 1))) return *value + ":";
            if (const auto* value = textLookup(std::string(text) + " ")) {
                auto end = value->find_last_not_of(" \t\r\n");
                return end == std::string::npos ? std::string(text) : value->substr(0, end + 1);
            }
        }
        constexpr std::string_view starting = "Starting inventory";
        if (text.starts_with(std::string(starting) + " (") && text.ends_with(')')) {
            const auto count = text.substr(starting.size() + 2, text.size() - starting.size() - 3);
            if (!count.empty() && count.find_first_not_of("0123456789") == std::string_view::npos)
                return stringLookup(starting) + std::string(text.substr(starting.size()));
        }
        return std::string(text);
    }

public:
    void load(const Json& data) {
        if (!data.is_object() || !data.contains("version") || !data.at("version").is_number_integer() ||
            data.at("version") != 1 || !data.contains("language") || data.at("language") != "ko")
            throw std::runtime_error("Unsupported Korean localization resource");
        for (const char* category : {"strings", "names", "guides"})
            if (!data.contains(category) || !data.at(category).is_object() || data.at(category).size() > 10000)
                throw std::runtime_error("Invalid localization section");
        for (const char* category : {"strings", "names"})
            for (const auto& [key, value] : data.at(category).items())
                if (!validText(key, 4096) || !value.is_string() || !validText(value.get_ref<const std::string&>(), 8192))
                    throw std::runtime_error("Invalid localization text");
        for (const auto& [name, guide] : data.at("guides").items()) {
            if (!validText(name, 1024) || !guide.is_object() || !guide.contains("steps") ||
                !guide.at("steps").is_array() || guide.at("steps").empty() || guide.at("steps").size() > 8)
                throw std::runtime_error("Invalid localized collection guide");
            for (const auto& step : guide.at("steps"))
                if (!step.is_string() || !validText(step.get_ref<const std::string&>(), 4800))
                    throw std::runtime_error("Invalid localized collection-guide step");
        }
        // Also reject invalid UTF-8 in programmatically supplied JSON. Parsed JSON
        // already has this check; a bad reload must not replace usable translations.
        (void)data.dump();
        data_ = data;
    }
    bool available() const { return data_.contains("strings"); }
    void setEnabled(bool value) { enabled_ = value; }
    bool enabled() const { return enabled_ && available(); }
    std::string stringLookup(std::string_view english) const {
        const auto* value = lookup("strings", english);
        return value ? *value : std::string(english);
    }
    std::string nameLookup(std::string_view canonicalName) const {
        const auto* value = lookup("names", canonicalName);
        return value ? *value : std::string(canonicalName);
    }
    // Lex the original logic before expanding underscores: Ball_and_Chain is
    // one item, whereas "A and B" contains a conjunction. This function is
    // deliberately separate from rml(), so notes and arbitrary prose are never
    // rewritten as expressions. Parentheses, operators, quotes and numbers keep
    // their original order and are escaped by the caller when embedded in RML.
    std::string logic(std::string_view expression) const {
        auto atom = [&](std::string_view value) {
            std::string words(value);
            std::replace(words.begin(), words.end(), '_', ' ');
            if (!enabled()) return words;
            const auto start = words.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) return words;
            const auto end = words.find_last_not_of(" \t\r\n") + 1;
            const auto* translated = textLookup(std::string_view(words).substr(start, end-start));
            return translated ? words.substr(0,start) + *translated + words.substr(end) : words;
        };
        if (!enabled()) return atom(expression);
        std::string result;
        size_t begin = 0;
        bool quoted = false;
        for (size_t pos = 0; pos < expression.size();) {
            // Midna's is part of a setting name, not a quoted event. Only an
            // apostrophe at the start of an atom opens an event quotation.
            if (expression[pos] == '\'' &&
                (quoted || expression.substr(begin,pos-begin).find_first_not_of(" \t\r\n")==std::string_view::npos)) {
                result += atom(expression.substr(begin,pos-begin));
                result += '\''; quoted = !quoted; begin = ++pos; continue;
            }
            size_t length = 0;
            std::string replacement;
            if (!quoted) {
                const auto tail = expression.substr(pos);
                if (tail.starts_with(" and ") || tail.starts_with(" or ")) {
                    length = tail.starts_with(" and ") ? 5 : 4;
                    replacement = " " + stringLookup(length == 5 ? "and" : "or") + " ";
                } else if (tail.starts_with("==") || tail.starts_with("!=") || tail.starts_with(">=") || tail.starts_with("<=")) {
                    length = 2; replacement = std::string(tail.substr(0,2));
                } else if (expression[pos] == '(' || expression[pos] == ')' || expression[pos] == ',') {
                    length = 1; replacement = expression[pos];
                }
            }
            if (length) {
                result += atom(expression.substr(begin,pos-begin));
                result += replacement; pos += length; begin = pos;
            } else ++pos;
        }
        result += atom(expression.substr(begin));
        return result;
    }
    bool match(std::string_view canonicalName, std::string_view needle) const {
        const auto query = lowerAscii(needle);
        return lowerAscii(canonicalName).find(query) != std::string::npos ||
               lowerAscii(nameLookup(canonicalName)).find(query) != std::string::npos;
    }
    std::optional<std::vector<std::string>> guideSteps(std::string_view canonicalCheck) const {
        if (!enabled()) return std::nullopt;
        const auto& guides = data_.at("guides");
        const auto found = guides.find(std::string(canonicalCheck));
        if (found == guides.end()) return std::nullopt;
        return found->at("steps").get<std::vector<std::string>>();
    }
    std::string rml(std::string_view markup) const {
        if (!enabled()) return std::string(markup);
        std::string out;
        for (size_t pos = 0; pos < markup.size();) {
            if (markup[pos] != '<') {
                const auto next = markup.find('<', pos);
                const auto text = markup.substr(pos, next == std::string_view::npos ? markup.size() - pos : next - pos);
                const auto decoded = decodeText(text);
                const auto translated = decoded ? translateText(*decoded) : std::string{};
                out += decoded && translated != *decoded ? escape(translated) : std::string(text);
                pos += text.size();
                continue;
            }
            // Preserve comments and quoted attributes, including embedded '>'.
            if (markup.substr(pos).starts_with("<!--")) {
                const auto end = markup.find("-->", pos + 4);
                if (end == std::string_view::npos) { out += markup.substr(pos); break; }
                out += markup.substr(pos, end + 3 - pos); pos = end + 3; continue;
            }
            char quote = 0;
            auto end = pos + 1;
            for (; end < markup.size(); ++end) {
                const char c = markup[end];
                if (quote) { if (c == quote) quote = 0; }
                else if (c == '\'' || c == '"') quote = c;
                else if (c == '>') break;
            }
            if (end == markup.size()) { out += markup.substr(pos); break; }
            out += markup.substr(pos, end + 1 - pos);
            pos = end + 1;
        }
        return out;
    }
};
}
