#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>

struct CharacterCacheData {
    std::string Name;
    int CombatLevel;
    int OverallLevel;
    std::string Label;
    int OriginalSlotIndex;
};

struct ServerDef {
    std::string name;
    std::string region;
    std::string ip;
};

class Config {
public:
    static Config* GetInstance();

    void Load(const std::string& profileName);
    void Save(const std::string& profileName);

    // Settings
    bool AutoLoginEnabled = false;
    std::string SelectedRegion = "";
    int SelectedCharacterSlot = -1;
    
    // Checked specific servers map (ServerName -> IsChecked)
    std::map<std::string, bool> ServerCheckboxes;

    // Cache of characters and servers discovered during previous injection
    std::vector<CharacterCacheData> CachedCharacters;
    std::vector<ServerDef> CachedServers;

    std::recursive_mutex ConfigMutex;

private:
    Config() = default;
    ~Config() = default;
    
    std::string GetConfigPath(const std::string& profileName);
};
