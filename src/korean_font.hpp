#pragma once

#include <mods/svc/host.h>
#include <mods/svc/resource.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>

namespace tracker {

struct KoreanFontSetup {
    std::string rcss;
    std::string error;
    std::string fontFile;

    // Ready to register through UiService, not proof of live glyph rendering.
    bool ready() const noexcept { return !rcss.empty() && error.empty(); }
};

// Dusklight 2.0.3's RmlUi fork supports @font-face through the public RCSS API.
// Its font reader opens files, not mod:// bundle resources. Materialize our own
// bundled font in the host-provided mod directory before registering the RCSS.
inline KoreanFontSetup prepareKoreanFont(
    const ResourceService* resources, const HostService* host, ModContext* context) {
    KoreanFontSetup result;
    if (!resources || !resources->load || !resources->free || !host || !host->mod_dir) {
        result.error = "Korean font services are unavailable.";
        return result;
    }

    ResourceBuffer buffer = RESOURCE_BUFFER_INIT;
    struct ReleaseBuffer {
        const ResourceService* resources;
        ModContext* context;
        ResourceBuffer* buffer;
        ~ReleaseBuffer() { resources->free(context, buffer); }
    } release{resources, context, &buffer};

    if (resources->load(context, "fonts/NanumGothic-Regular.ttf", &buffer) != MOD_OK ||
        !buffer.data || buffer.size == 0 ||
        buffer.size > static_cast<size_t>(std::numeric_limits<std::streamsize>::max())) {
        result.error = "Bundled Korean font could not be read.";
        return result;
    }

    try {
        const char* directory = host->mod_dir(context);
        if (!directory || !*directory) {
            if (!host->data_dir || host->data_dir(context, &directory) != MOD_OK ||
                !directory || !*directory) {
                result.error = "Korean font directory is unavailable.";
                return result;
            }
        }

        const std::string_view directoryBytes(directory);
        const std::u8string directoryUtf8(directoryBytes.begin(), directoryBytes.end());
        const auto fontDirectory = std::filesystem::absolute(
            std::filesystem::path(directoryUtf8) / "fonts");
        const auto fontPath = fontDirectory / "TPTracker-NanumGothic-76f45ef4.ttf";
        const auto utf8Path = fontPath.generic_u8string();
        result.fontFile.assign(utf8Path.begin(), utf8Path.end());

        // Forward slashes preserve Windows drive paths and avoid RCSS escapes.
        // The fork's quoted src parser does not decode CSS escape sequences, so
        // reject characters that cannot be represented faithfully instead of
        // producing malformed RCSS. Windows filenames already forbid these.
        if (result.fontFile.find_first_of("\"\\\r\n") != std::string::npos) {
            result.error = "Korean font path cannot be represented safely in RCSS.";
            return result;
        }

        std::filesystem::create_directories(fontDirectory);
        std::ofstream output(fontPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            result.error = "Korean font cache file could not be opened.";
            return result;
        }
        output.write(static_cast<const char*>(buffer.data),
                     static_cast<std::streamsize>(buffer.size));
        output.close();
        if (!output) {
            result.error = "Korean font cache file could not be written.";
            return result;
        }

        // file:// is handled by Dusklight's FileInterface_SDL. It also avoids
        // RmlUi JoinPath stripping the first slash of a Unix absolute path.
        // src is a quoted filename in this fork, not browser CSS url(...).
        // The fallback face also supplies Hangul to built-in settings controls
        // that keep their original Fira font. Icon font declarations remain.
        result.rcss =
            "@font-face { font-family: TPTrackerKorean; font-style: normal; "
            "font-weight: normal; -rmlui-fallback-face: true; src: \"file://" +
            result.fontFile + "\"; }\n"
            ".tp-korean-font { font-family: TPTrackerKorean; }\n";
    } catch (const std::exception&) {
        result.error = "Korean font cache could not be prepared.";
    }
    return result;
}

} // namespace tracker
