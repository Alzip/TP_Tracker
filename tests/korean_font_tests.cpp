#include "korean_font.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
namespace fs = std::filesystem;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::string utf8(const fs::path& path) {
    const auto bytes = path.generic_u8string();
    return {bytes.begin(), bytes.end()};
}

fs::path utf8Path(const std::string& path) {
    return fs::path(std::u8string(path.begin(), path.end()));
}

std::vector<char> readBytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Font fixture/cache file could not be opened");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct TemporaryDirectory {
    fs::path path;
    ~TemporaryDirectory() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};

struct MockServices {
    std::vector<char> font;
    std::string modDirectory;
    std::string persistentDirectory;
    int liveBuffers = 0;
    int freedBuffers = 0;
    bool missingResource = false;
    bool persistentUnavailable = false;
};

MockServices& mock(ModContext* context) {
    return *reinterpret_cast<MockServices*>(context);
}

ModResult loadResource(ModContext* context, const char* path, ResourceBuffer* output) {
    auto& state = mock(context);
    if (state.missingResource) return MOD_UNAVAILABLE;
    if (std::strcmp(path, "fonts/NanumGothic-Regular.ttf") != 0) return MOD_UNAVAILABLE;
    output->data = std::malloc(state.font.size());
    if (!output->data) return MOD_ERROR;
    output->size = state.font.size();
    std::memcpy(output->data, state.font.data(), state.font.size());
    ++state.liveBuffers;
    return MOD_OK;
}

void freeResource(ModContext* context, ResourceBuffer* buffer) {
    if (buffer->data) {
        auto& state = mock(context);
        std::free(buffer->data);
        --state.liveBuffers;
        ++state.freedBuffers;
    }
    buffer->data = nullptr;
    buffer->size = 0;
}

const char* modDirectory(ModContext* context) {
    return mock(context).modDirectory.c_str();
}

ModResult persistentDirectory(ModContext* context, const char** output) {
    auto& state = mock(context);
    if (state.persistentUnavailable) {
        *output = nullptr;
        return MOD_UNAVAILABLE;
    }
    *output = state.persistentDirectory.c_str();
    return MOD_OK;
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Pass the bundled Korean TTF path");
        MockServices state;
        state.font = readBytes(utf8Path(argv[1]));
        require(state.font.size() > 1000, "Korean font fixture is empty or truncated");

        // Deliberately exercise the Windows UTF-8-to-native path conversion.
        // Unicode escapes keep this test independent of source codepage flags.
        std::u8string name = u8"tptracker-font-\uD55C\uAE00 \uACF5\uAC04, ";
        const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        name.append(unique.begin(), unique.end());
        const auto root = fs::temp_directory_path() / fs::path(name);
        require(fs::create_directory(root), "Could not create a unique font test directory");
        TemporaryDirectory cleanup{root};

        ResourceService resources{};
        resources.load = loadResource;
        resources.free = freeResource;
        HostService host{};
        host.mod_dir = modDirectory;
        host.data_dir = persistentDirectory;
        auto* context = reinterpret_cast<ModContext*>(&state);
        state.modDirectory = utf8(root / "cache");
        state.persistentDirectory = utf8(root / "persistent");

        auto setup = tracker::prepareKoreanFont(&resources, &host, context);
        require(setup.ready(), setup.error.c_str());
        require(state.liveBuffers == 0 && state.freedBuffers == 1, "Prepared font buffer leaked");
        const auto fontPath = utf8Path(setup.fontFile);
        require(readBytes(fontPath) == state.font, "Cached font differs from bundled font");
        require(setup.rcss.find("src: \"file://" + utf8(fontPath) + "\"") != std::string::npos,
                "RCSS lost the absolute UTF-8 font filename");
        require(setup.rcss.find("\\") == std::string::npos,
                "Windows path separators were not normalized for RCSS");
        require(setup.rcss.find("-rmlui-fallback-face: true") != std::string::npos,
                "Built-in UI font fallback is absent");

        // A reload must recover a stale/truncated copy in the mod's own cache.
        { std::ofstream stale(fontPath, std::ios::binary | std::ios::trunc); stale << "stale"; }
        auto repeated = tracker::prepareKoreanFont(&resources, &host, context);
        require(repeated.ready() && repeated.fontFile == setup.fontFile,
                "Repeated font preparation failed");
        require(readBytes(fontPath) == state.font, "Repeated setup retained a stale font cache");
        require(state.liveBuffers == 0 && state.freedBuffers == 2, "Repeated setup leaked font data");

        state.missingResource = true;
        state.modDirectory = utf8(root / "missing-resource");
        auto missing = tracker::prepareKoreanFont(&resources, &host, context);
        require(!missing.ready() && missing.rcss.empty() && !missing.error.empty(),
                "Missing font did not fail safely");
        require(!fs::exists(utf8Path(state.modDirectory)) && state.liveBuffers == 0,
                "Missing font left cache output or live data");
        state.missingResource = false;

        state.modDirectory = utf8(root / "bad\"directory");
        auto unsafe = tracker::prepareKoreanFont(&resources, &host, context);
        require(!unsafe.ready() && unsafe.rcss.empty() && !unsafe.error.empty(),
                "Unsafe quoted font path was accepted");
        require(state.liveBuffers == 0 && state.freedBuffers == 3,
                "Unsafe path leaked loaded font data");

        const auto blocked = root / "not-a-directory";
        { std::ofstream file(blocked); file << "block directory creation"; }
        state.modDirectory = utf8(blocked);
        auto failedWrite = tracker::prepareKoreanFont(&resources, &host, context);
        require(!failedWrite.ready() && failedWrite.rcss.empty() && !failedWrite.error.empty(),
                "Font cache write failure was not reported");
        require(state.liveBuffers == 0 && state.freedBuffers == 4,
                "Failed cache write leaked font data");

        state.modDirectory.clear();
        auto persistent = tracker::prepareKoreanFont(&resources, &host, context);
        require(persistent.ready() && readBytes(utf8Path(persistent.fontFile)) == state.font,
                "Persistent directory fallback failed");
        require(state.liveBuffers == 0 && state.freedBuffers == 5,
                "Persistent directory fallback leaked font data");
        state.persistentUnavailable = true;
        auto noDirectory = tracker::prepareKoreanFont(&resources, &host, context);
        require(!noDirectory.ready() && noDirectory.rcss.empty() && state.liveBuffers == 0,
                "Unavailable cache directory did not fail safely");

        auto noResourceService = tracker::prepareKoreanFont(nullptr, &host, context);
        auto noHostService = tracker::prepareKoreanFont(&resources, nullptr, context);
        require(!noResourceService.ready() && !noHostService.ready(), "Null services were accepted");
        std::cout << "Korean font cache: UTF-8 paths, exact bytes, reload, fallback and failure cleanup passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
