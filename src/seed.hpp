#pragma once
#include "model.hpp"
#include <filesystem>
namespace tracker {
bool backupSelectedCard(const std::filesystem::path& destination, std::string& error);
// Read-only lookup of the selected CARD file and slot, never a newest-file guess.
bool loadSiblingEntrances(Model& model, const std::filesystem::path& logPath, std::string& error);
bool loadSelectedSeed(Model& model, const std::filesystem::path& dataRoot, std::string& error);
}
