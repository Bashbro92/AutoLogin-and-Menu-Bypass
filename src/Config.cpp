#include "Config.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <iostream>

Config* Config::GetInstance() {
    static Config instance;
    return &instance;
}

std::string Config::GetConfigPath(const std::string& profileName) {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::string fullPath = std::string(path) + "\\ProjectSandboxAutoLogin_" + profileName + ".ini";
        return fullPath;
    }
    return "ProjectSandboxAutoLogin_" + profileName + ".ini";
}

void Config::Load(const std::string& profileName) {
    std::lock_guard<std::recursive_mutex> lock(ConfigMutex);
    std::string path = GetConfigPath(profileName);
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    std::string currentSection = "";

    AutoLoginEnabled = false;
    SelectedRegion = "";
    SelectedCharacterSlot = -1;
    ServerCheckboxes.clear();
    CachedCharacters.clear();

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (line[0] == '[') {
            size_t endPos = line.find(']');
            if (endPos != std::string::npos) {
                currentSection = line.substr(1, endPos - 1);
            }
            continue;
        }

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);

        if (currentSection == "Main") {
            if (key == "AutoLoginEnabled") AutoLoginEnabled = (value == "1");
            else if (key == "SelectedRegion") SelectedRegion = value;
            else if (key == "SelectedCharacterSlot") SelectedCharacterSlot = std::stoi(value);
        } else if (currentSection == "Servers") {
            ServerCheckboxes[key] = (value == "1");
        } else if (currentSection == "Characters") {
            // Expected format: Name|CombatLvl|OverallLvl|Label|SlotIndex
            std::stringstream ss(value);
            std::string item;
            std::vector<std::string> parts;
            while (std::getline(ss, item, '|')) {
                parts.push_back(item);
            }
            if (parts.size() == 5) {
                CharacterCacheData data;
                data.Name = parts[0];
                data.CombatLevel = std::stoi(parts[1]);
                data.OverallLevel = std::stoi(parts[2]);
                data.Label = parts[3];
                data.OriginalSlotIndex = std::stoi(parts[4]);
                CachedCharacters.push_back(data);
            }
        }
    }
}

void Config::Save(const std::string& profileName) {
    std::lock_guard<std::recursive_mutex> lock(ConfigMutex);
    std::string path = GetConfigPath(profileName);
    std::ofstream file(path);
    if (!file.is_open()) return;

    file << "[Main]\n";
    file << "AutoLoginEnabled=" << (AutoLoginEnabled ? "1" : "0") << "\n";
    file << "SelectedRegion=" << SelectedRegion << "\n";
    file << "SelectedCharacterSlot=" << SelectedCharacterSlot << "\n";
    
    file << "\n[Servers]\n";
    for (const auto& kv : ServerCheckboxes) {
        if (kv.second) {
            file << kv.first << "=1\n";
        }
    }

    file << "\n[Characters]\n";
    for (size_t i = 0; i < CachedCharacters.size(); ++i) {
        const auto& c = CachedCharacters[i];
        file << "Char" << i << "=" 
             << c.Name << "|" 
             << c.CombatLevel << "|" 
             << c.OverallLevel << "|" 
             << c.Label << "|" 
             << c.OriginalSlotIndex << "\n";
    }
}
