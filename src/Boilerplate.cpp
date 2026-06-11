#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#include "Boilerplate.h"
#include "imgui.h"
#include "PatternScanner.h"
#include "Diagnostics.h"
#include "Config.h"

// Unreal Engine SDK
#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/W_MainMenu_classes.hpp"
#include "SDK/W_PlayOnline_classes.hpp"
#include "SDK/W_LogIn_classes.hpp"
#include "SDK/W_CharacterSelection_classes.hpp"
#include "SDK/W_ServerSlotsList_classes.hpp"
#include "SDK/W_ServerSlotEntry_classes.hpp"
#include "SDK/W_ServerSlotEntryDisplay_classes.hpp"
#include "SDK/W_ThemeText_classes.hpp"
#include "SDK/CommonUI_classes.hpp"
#include "SDK/ProjectSandbox_classes.hpp"

static int GetPlayerCount() {
    int count = -1;
    uintptr_t baseAddr = (uintptr_t)GetModuleHandle(NULL);
    if (baseAddr) {
        SDK::UWorld** gWorldPtr = (SDK::UWorld**)(baseAddr + 0x0A7B8DB8);
        if (!IsBadReadPtr(gWorldPtr, sizeof(void*)) && *gWorldPtr) {
            SDK::UWorld* world = *gWorldPtr;
            if (!IsBadReadPtr(world, sizeof(SDK::UWorld))) {
                SDK::AGameStateBase* gameState = world->GameState;
                if (gameState && !IsBadReadPtr(gameState, sizeof(SDK::AGameStateBase))) {
                    count = gameState->PlayerArray.Num();
                }
            }
        }
    }
    return count;
}

static bool IsInGameWorld() {
    uintptr_t baseAddr = (uintptr_t)GetModuleHandle(NULL);
    if (baseAddr) {
        SDK::UWorld** gWorldPtr = (SDK::UWorld**)(baseAddr + 0x0A7B8DB8);
        if (!IsBadReadPtr(gWorldPtr, sizeof(void*)) && *gWorldPtr) {
            SDK::UWorld* world = *gWorldPtr;
            if (!IsBadReadPtr(world, sizeof(SDK::UWorld))) {
                // If NetDriver is valid, we are actively connected to a server!
                if (world->NetDriver && !IsBadReadPtr(world->NetDriver, sizeof(void*))) {
                    SDK::AGameStateBase* gameState = world->GameState;
                    if (gameState && !IsBadReadPtr(gameState, sizeof(SDK::AGameStateBase))) {
                        return gameState->PlayerArray.Num() > 0;
                    }
                }
            }
        }
    }
    return false;
}

static bool DoNameCheck(SDK::UObject* Obj) {
    std::string name = Obj->GetFullName();
    return name.find("Default__") == std::string::npos && name.find("Transient") != std::string::npos;
}

static bool IsValidUObject(SDK::UObject* Obj, SDK::UClass* TargetClass) {
    __try {
        if (!Obj) return false;
        
        // Safely check if it matches the class and is not a default object
        if (Obj->IsA(TargetClass) && !Obj->IsDefaultObject()) {
            return DoNameCheck(Obj);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return false;
}

static SDK::UObject* SafeGetUObjectByIndex(int i) {
    __try {
        if (!SDK::UObject::GObjects) return nullptr;
        return SDK::UObject::GObjects->GetByIndex(i);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

static bool IsWidgetVisible(SDK::UWidget* widget) {
    __try {
        if (!widget) return false;
        // Reading property directly is 1000x safer and faster than calling IsInViewport() (which triggers ProcessEvent)
        return widget->Visibility != SDK::ESlateVisibility::Collapsed &&
               widget->Visibility != SDK::ESlateVisibility::Hidden;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template<typename T>
T* FindWidget() {
    SDK::UClass* targetClass = T::StaticClass();
    if (!targetClass) return nullptr;
    
    if (!SDK::UObject::GObjects) return nullptr;
    int numObjects = SDK::UObject::GObjects->Num();
    
    __try {
        for (int i = 0; i < numObjects; ++i) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!Obj || Obj->IsDefaultObject()) continue;
            
            if (Obj->IsA(targetClass)) {
                if (DoNameCheck(Obj)) {
                    return static_cast<T*>(Obj);
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return nullptr;
}

template<typename T>
void FindWidgetsImpl(std::vector<T*>* widgets, SDK::UClass* targetClass, int numObjects) {
    __try {
        for (int i = 0; i < numObjects; ++i) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!Obj || Obj->IsDefaultObject()) continue;
            
            if (Obj->IsA(targetClass)) {
                if (DoNameCheck(Obj)) {
                    widgets->push_back(static_cast<T*>(Obj));
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        // Return whatever we safely found before the exception
    }
}

template<typename T>
std::vector<T*> FindWidgets() {
    std::vector<T*> widgets;
    SDK::UClass* targetClass = T::StaticClass();
    if (!targetClass) return widgets;
    
    if (!SDK::UObject::GObjects) return widgets;
    int numObjects = SDK::UObject::GObjects->Num();
    
    FindWidgetsImpl<T>(&widgets, targetClass, numObjects);
    return widgets;
}

void Boilerplate::QueueTask(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(m_TaskMutex);
    m_TaskQueue.push_back(task);
    m_HasTasks = true;
}

void Boilerplate::ExecuteTasks() {
    if (m_HasTasks) {
        std::vector<std::function<void()>> tasks;
        {
            std::lock_guard<std::mutex> lock(m_TaskMutex);
            tasks = std::move(m_TaskQueue);
            m_HasTasks = false;
        }
        for (auto& task : tasks) {
            task();
        }
    }
}

// Variables for AutoLogin UI state
static int selectedRegion = 0;
static const char* regions[] = { "Any", "US West", "US East", "EU", "AUS" };

#include <vector>
#include <string>
#include <map>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>



struct ServerDef {
    std::string name;
    std::string region;
    std::string ip;
};

static std::vector<ServerDef> g_DiscoveredServers;
static std::map<std::string, bool> g_ServerCheckboxes;
static std::string g_SelectedRegion = "";
static std::string g_TargetServer = "";
static bool g_ServersScanned = false;
static std::mutex g_ServersMutex;

// OSD & Ping Globals
static std::string g_JoinedServerName = "";
static std::string g_JoinedServerIP = "";
static std::string g_JoinedServerRegion = "";
static std::atomic<int> g_CurrentPing = -1;
static std::atomic<bool> g_PingThreadRunning = false;
static std::thread g_PingThread;

#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_structs.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/ProjectSandbox_classes.hpp"
#include "SDK/W_HUD_classes.hpp"
#include "SDK/UMG_classes.hpp"

#include <iostream>

static void PingThreadWorker() {
    HANDLE hIcmpFile = IcmpCreateFile();
    if (hIcmpFile == INVALID_HANDLE_VALUE) return;
    
    char ReplyBuffer[sizeof(ICMP_ECHO_REPLY) + 32];
    
    while (g_PingThreadRunning) {
        if (!g_JoinedServerIP.empty() && g_JoinedServerIP != "127.0.0.1" && g_JoinedServerIP != "None") {
            // Extract IP before port
            std::string ipOnly = g_JoinedServerIP;
            size_t colonPos = ipOnly.find(':');
            if (colonPos != std::string::npos) {
                ipOnly = ipOnly.substr(0, colonPos);
            }
            
            unsigned long ipaddr = inet_addr(ipOnly.c_str());
            if (ipaddr != INADDR_NONE) {
                DWORD dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, (LPVOID)"Ping", 4, NULL, ReplyBuffer, sizeof(ReplyBuffer), 1000);
                if (dwRetVal != 0) {
                    PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
                    g_CurrentPing = pEchoReply->RoundTripTime;
                } else {
                    g_CurrentPing = -1;
                }
            } else {
                g_CurrentPing = -1;
            }
        } else {
            g_CurrentPing = -1;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    IcmpCloseHandle(hIcmpFile);
}

Boilerplate::Boilerplate() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    g_PingThreadRunning = true;
    g_PingThread = std::thread(PingThreadWorker);
}
Boilerplate::~Boilerplate() {
    g_PingThreadRunning = false;
    if (g_PingThread.joinable()) {
        g_PingThread.join();
    }
}

Boilerplate* Boilerplate::GetInstance() {
    static Boilerplate instance;
    return &instance;
}

static std::string g_ServerStateText = "Not Connected";
static char g_CurrentProfileInput[64] = "Default";
static std::string g_CurrentProfile = "Default";

void Boilerplate::InitializeSDK() {
    if (m_SDKInitialized) return;
    
    // Load config on startup
    Config::GetInstance()->Load(g_CurrentProfile);
    
    // GObjects
    uintptr_t gObjectsInst = PatternScanner::FindPattern("", "48 8B 05 ? ? ? ? 48 8B 0C C8 48 8D 04 D1 EB");
    if (gObjectsInst) {
        uintptr_t gObjectsAddress = PatternScanner::ResolveRIP(gObjectsInst, 3, 7);
        if (gObjectsAddress) {
            SDK::UObject::GObjects.InitManually((void*)gObjectsAddress);
        }
    }
    
    // GNames
    uintptr_t gNamesInst = PatternScanner::FindPattern("", "48 8D 0D ? ? ? ? E8 ? ? ? ? 4C 8B C0 48 8B D6");
    if (gNamesInst) {
        // g_GNames logic here if you want manual string resolution
    }
    
    m_SDKInitialized = true;
}

void Boilerplate::Update() {
    if (!m_SDKInitialized) {
        InitializeSDK();
    }
    
    // AutoLogin State Machine
    static auto lastTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastTime).count() >= 1) {
        lastTime = now;
        
        if (!HasTasks()) {
            QueueTask([]() {
                auto config = Config::GetInstance();
                
                if (IsInGameWorld()) {
                    if (g_ServerStateText != "Connected") {
                        g_ServerStateText = "Connected";
                        std::cout << "[AutoLogin] State: Connected! Successfully loaded into game world." << std::endl;
                        config->AutoLoginEnabled = false;
                        config->Save(g_CurrentProfile);
                    }
                } else if (g_ServerStateText == "Connected") {
                    g_ServerStateText = config->AutoLoginEnabled ? "Idle" : "Not Connected";
                    g_JoinedServerName.clear();
                    g_JoinedServerIP.clear();
                    g_JoinedServerRegion.clear();
                    std::cout << "[AutoLogin] Returned to Menus. Resetting UI state." << std::endl;
                }

                if (!config->AutoLoginEnabled) return;

                static auto lastStateChangeTime = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                
                // Only attempt an AutoLogin action once every 2 seconds to allow UI transitions
                if (std::chrono::duration_cast<std::chrono::seconds>(now - lastStateChangeTime).count() < 2) {
                    return;
                }

                std::string state = g_ServerStateText;

                // State 1: Main Menu
                if (state == "Idle" || state == "Not Connected") {
                    if (auto menu = FindWidget<SDK::UW_MainMenu_C>()) {
                        g_ServerStateText = "Going Online";
                        lastStateChangeTime = now;
                        std::cout << "[AutoLogin] State: Going Online..." << std::endl;
                        menu->BndEvt__W_MainMenu_PlayOnlineButton_K2Node_ComponentBoundEvent_0_CommonButtonBaseClicked__DelegateSignature(nullptr);
                    }
                }
                // State 2: Login
                else if (state == "Going Online") {
                    if (auto login = FindWidget<SDK::UW_LogIn_C>()) {
                        g_ServerStateText = "Heading to Character Selection";
                        lastStateChangeTime = now;
                        std::cout << "[AutoLogin] State: Heading to Character Selection..." << std::endl;
                        login->BndEvt__W_LogIn_LoginWithSteamButton_K2Node_ComponentBoundEvent_8_CommonButtonBaseClicked__DelegateSignature(nullptr);
                    }
                }
                // State 3: Character Selection
                else if (state == "Heading to Character Selection") {
                    if (auto charSelect = FindWidget<SDK::UW_CharacterSelection_C>()) {
                        g_ServerStateText = "Picking Servers";
                        lastStateChangeTime = now;
                        std::cout << "[AutoLogin] State: Picking Servers..." << std::endl;
                        
                        int count = charSelect->Characters.Num();
                        
                        // Rescan and cache
                        {
                            std::lock_guard<std::recursive_mutex> lock(config->ConfigMutex);
                            config->CachedCharacters.clear();
                            for (int i = 0; i < count; ++i) {
                                auto* charData = charSelect->Characters[i];
                                if (charData) {
                                    CharacterCacheData data;
                                    data.Name = charData->Data.CharacterName.ToString();
                                    data.CombatLevel = charData->Summary.CombatLevel;
                                    data.OverallLevel = charData->Summary.OverallLevel;
                                    data.OriginalSlotIndex = i;
                                    data.Label = data.Name + " (Cb:" + std::to_string(data.CombatLevel) + " Total:" + std::to_string(data.OverallLevel) + ")";
                                    config->CachedCharacters.push_back(data);
                                }
                            }
                        }
                        config->Save(g_CurrentProfile);
                        
                        // Select Slot and Enter World
                        if (config->SelectedCharacterSlot >= 0 && config->SelectedCharacterSlot < count && charSelect->CharactersListView) {
                            auto* charData = charSelect->Characters[config->SelectedCharacterSlot];
                            if (charData) {
                                charSelect->CharactersListView->BP_SetSelectedItem(charData);
                            }
                        }
                        charSelect->BndEvt__W_CharacterSelection_PlayButton_K2Node_ComponentBoundEvent_5_CommonButtonBaseClicked__DelegateSignature(nullptr);
                    }
                }
                // State 4: Server Selection
                else if (state == "Picking Servers") {
                    if (auto serverList = FindWidget<SDK::UW_ServerSlotsList_C>()) {
                        std::vector<ServerDef> candidates;
                        {
                            std::lock_guard<std::mutex> lock(g_ServersMutex);
                            for (const auto& srv : g_DiscoveredServers) {
                                if (config->SelectedRegion.empty() || srv.region == config->SelectedRegion) {
                                    if (config->ServerCheckboxes[srv.name]) {
                                        candidates.push_back(srv);
                                    }
                                }
                            }
                            
                            // Fallback to any valid if none checked
                            if (candidates.empty()) {
                                for (const auto& srv : g_DiscoveredServers) {
                                    if (config->SelectedRegion.empty() || srv.region == config->SelectedRegion) {
                                        candidates.push_back(srv);
                                    }
                                }
                            }
                        }
                        
                        if (!candidates.empty()) {
                            ServerDef targetServer = candidates[rand() % candidates.size()];
                            g_ServerStateText = "Connecting to " + targetServer.name;
                            lastStateChangeTime = now;
                            std::cout << "[AutoLogin] State: " << g_ServerStateText << std::endl;
                            
                            g_JoinedServerName = targetServer.name;
                            g_JoinedServerIP = targetServer.ip;
                            g_JoinedServerRegion = targetServer.region;
                            
                            auto entryWidgets = FindWidgets<SDK::UW_ServerSlotEntry_C>();
                            for (auto* entryWidget : entryWidgets) {
                                if (entryWidget->ServerSlotName.ToString() == targetServer.name) {
                                    if (serverList->ServerSlotListView && entryWidget->As_Server_Slot_Entry) {
                                        serverList->ServerSlotListView->BP_SetSelectedItem(entryWidget->As_Server_Slot_Entry);
                                    }
                                    SDK::UFunction* clickFunc = entryWidget->Class->GetFunction("W_ServerSlotEntry_C", "Click");
                                    if (clickFunc) entryWidget->ProcessEvent(clickFunc, nullptr);
                                    break;
                                }
                            }
                        }
                    }
                }
        });
        }
    }
}

void Boilerplate::DrawUI() {
    // This runs in the ImGui DX12 hook loop
    Update();
    
    // --- OSD Optics Overlay ---
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
    ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    
    if (ImGui::Begin("OpticsOverlay", nullptr, overlayFlags)) {
        // Compile Time
        std::stringstream ss;
        ss << __DATE__ << " " << __TIME__;
        std::tm tm = {};
        int year, day;
        char month[4];
        if (sscanf(ss.str().c_str(), "%3s %d %d %d:%d:%d", month, &day, &year, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
            std::string ampm = tm.tm_hour >= 12 ? "PM" : "AM";
            int hour12 = tm.tm_hour % 12;
            if (hour12 == 0) hour12 = 12;
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Build: %s %d %d %02d:%02d:%02d %s", month, day, year, hour12, tm.tm_min, tm.tm_sec, ampm.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Build: %s %s", __DATE__, __TIME__);
        }
        
        // Server Status
        ImGui::Text("Server State: %s", g_ServerStateText.c_str());
        if (!g_JoinedServerName.empty()) {
            ImGui::Text("Joined: %s [%s] (%s)", g_JoinedServerName.c_str(), g_JoinedServerRegion.c_str(), g_JoinedServerIP.c_str());
        } else {
            ImGui::Text("Joined: None");
        }
        
        // Ping
        int ping = g_CurrentPing.load();
        if (ping == -1) {
            ImGui::Text("Ping: N/A");
        } else {
            ImVec4 pingColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
            if (ping > 150) pingColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
            else if (ping > 80) pingColor = ImVec4(1.0f, 1.0f, 0.2f, 1.0f);
            ImGui::TextColored(pingColor, "Ping: %d ms", ping);
        }
        
        // Player Count
        int playerCount = GetPlayerCount();
        
        if (playerCount != -1) {
            ImGui::Text("Players: %d", playerCount);
        } else {
            ImGui::Text("Players: N/A");
        }
    }
    ImGui::End();
    // --------------------------
    
    ImGui::Begin("Boilerplate Puzzle Window", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Universal Boilerplate Injected!");
    
    // Config Section
    ImGui::Separator();
    auto config = Config::GetInstance();
    
    ImGui::Text("Config Profile:");
    ImGui::InputText("##ProfileInput", g_CurrentProfileInput, IM_ARRAYSIZE(g_CurrentProfileInput));
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        g_CurrentProfile = g_CurrentProfileInput;
        config->Load(g_CurrentProfile);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        g_CurrentProfile = g_CurrentProfileInput;
        config->Save(g_CurrentProfile);
    }
    
    ImGui::Separator();
    ImGui::Text("Auto Login Tools:");
    
    if (ImGui::Checkbox("Enable Background AutoLogin", &config->AutoLoginEnabled)) {
        if (config->AutoLoginEnabled) {
            g_ServerStateText = "Not Connected";
        }
        config->Save(g_CurrentProfile);
    }
    
    ImGui::Separator();
    
    if (ImGui::Button("Scan For Servers")) {
        Boilerplate::GetInstance()->QueueTask([]() {
            auto entries = FindWidgets<SDK::UServerSlotEntry>();
            
            std::lock_guard<std::mutex> lock(g_ServersMutex);
            g_DiscoveredServers.clear();
            for (auto* entry : entries) {
                std::string name = entry->SlotName.ToString();
                std::string rawRegion = entry->Region.ToString();
                std::string region = rawRegion;
                
                if (rawRegion.find("us-west") != std::string::npos) region = "US West";
                else if (rawRegion.find("us-east") != std::string::npos) region = "US East";
                else if (rawRegion.find("eu-") != std::string::npos) region = "EU";
                else if (rawRegion.find("ap-") != std::string::npos) region = "AUS";
                else if (rawRegion.find("sa-") != std::string::npos) region = "SA";
                
                if (!name.empty() && !rawRegion.empty()) {
                    // Prevent duplicates
                    bool exists = false;
                    for (const auto& srv : g_DiscoveredServers) {
                        if (srv.name == name) { exists = true; break; }
                    }
                    if (!exists) {
                        g_DiscoveredServers.push_back({name, region, entry->ServerIP.ToString()});
                        std::cout << "[AutoLogin] Discovered Server: " << name << " in Region: " << region << " (IP: " << entry->ServerIP.ToString() << ")" << std::endl;
                    }
                }
            }
            std::cout << "[AutoLogin] Scanned " << g_DiscoveredServers.size() << " servers!" << std::endl;
            
            if (g_DiscoveredServers.empty()) {
                std::cout << "[AutoLogin] Please open the Server List in-game first to scan!" << std::endl;
            }
        });
    }
    
    std::lock_guard<std::mutex> drawLock(g_ServersMutex);
    if (!g_DiscoveredServers.empty()) {
        std::vector<std::string> regions;
        for (const auto& srv : g_DiscoveredServers) {
            if (std::find(regions.begin(), regions.end(), srv.region) == regions.end()) {
                regions.push_back(srv.region);
            }
        }
        
        ImGui::Text("Region Filter:");
        if (ImGui::BeginCombo("##RegionFilter", config->SelectedRegion.empty() ? "Any Region" : config->SelectedRegion.c_str())) {
            if (ImGui::Selectable("Any Region", config->SelectedRegion.empty())) {
                config->SelectedRegion = "";
                config->Save(g_CurrentProfile);
            }
            for (const auto& reg : regions) {
                if (ImGui::Selectable(reg.c_str(), config->SelectedRegion == reg)) {
                    config->SelectedRegion = reg;
                    config->Save(g_CurrentProfile);
                }
            }
            ImGui::EndCombo();
        }
        
        ImGui::Text("Specific Server Filter (Optional):");
        
        if (ImGui::Button("Clear All Filters")) {
            config->ServerCheckboxes.clear();
            config->Save(g_CurrentProfile);
        }
        
        for (const auto& srv : g_DiscoveredServers) {
            if (config->SelectedRegion.empty() || srv.region == config->SelectedRegion) {
                std::string label;
                if (config->SelectedRegion.empty()) {
                    label = srv.name + " (" + srv.region + ")##" + srv.name;
                } else {
                    label = srv.name + "##" + srv.name;
                }
                
                if (ImGui::Checkbox(label.c_str(), &config->ServerCheckboxes[srv.name])) {
                    config->Save(g_CurrentProfile);
                }
            }
        }
    }
    
    ImGui::Separator();
    
    if (ImGui::Button("1. Play Online")) {
        Boilerplate::GetInstance()->QueueTask([]() {
            auto widget = FindWidget<SDK::UW_MainMenu_C>();
            if (widget) {
                g_ServerStateText = "Going Online";
                std::cout << "[AutoLogin] Simulating Play Online click..." << std::endl;
                widget->BndEvt__W_MainMenu_PlayOnlineButton_K2Node_ComponentBoundEvent_0_CommonButtonBaseClicked__DelegateSignature(nullptr);
            } else {
                std::cout << "[AutoLogin] W_MainMenu_C not found!" << std::endl;
            }
        });
    }
    ImGui::SameLine();
    if (ImGui::Button("2. Login (Steam)")) {
        Boilerplate::GetInstance()->QueueTask([]() {
            auto widget = FindWidget<SDK::UW_LogIn_C>();
            if (widget) {
                g_ServerStateText = "Heading to Character Selection";
                std::cout << "[AutoLogin] Simulating Login with Steam click..." << std::endl;
                widget->BndEvt__W_LogIn_LoginWithSteamButton_K2Node_ComponentBoundEvent_8_CommonButtonBaseClicked__DelegateSignature(nullptr);
            } else {
                std::cout << "[AutoLogin] W_LogIn_C not found!" << std::endl;
            }
        });
    }
    
    // Character Selection
    if (ImGui::Button("3. Enter World")) {
        Boilerplate::GetInstance()->QueueTask([]() {
            auto widget = FindWidget<SDK::UW_CharacterSelection_C>();
            if (widget) {
                auto config = Config::GetInstance();
                int count = widget->Characters.Num();
                if (config->SelectedCharacterSlot >= 0 && config->SelectedCharacterSlot < count && widget->CharactersListView) {
                    std::cout << "[AutoLogin] Selecting Character Index: " << config->SelectedCharacterSlot << std::endl;
                    auto* charData = widget->Characters[config->SelectedCharacterSlot];
                    if (charData) {
                        widget->CharactersListView->BP_SetSelectedItem(charData);
                    }
                }
                
                g_ServerStateText = "Picking Servers";
                std::cout << "[AutoLogin] Simulating Enter World click..." << std::endl;
                widget->BndEvt__W_CharacterSelection_PlayButton_K2Node_ComponentBoundEvent_5_CommonButtonBaseClicked__DelegateSignature(nullptr);
            } else {
                std::cout << "[AutoLogin] W_CharacterSelection_C not found!" << std::endl;
            }
        });
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Scan Characters")) {
        Boilerplate::GetInstance()->QueueTask([]() {
            auto widget = FindWidget<SDK::UW_CharacterSelection_C>();
            if (widget) {
                int count = widget->Characters.Num();
                std::cout << "[AutoLogin] Found " << count << " character slots:" << std::endl;
                
                auto config = Config::GetInstance();
                {
                    std::lock_guard<std::recursive_mutex> lock(config->ConfigMutex);
                    config->CachedCharacters.clear();
                    
                    for (int i = 0; i < count; ++i) {
                        auto* charData = widget->Characters[i];
                        if (charData) {
                            CharacterCacheData data;
                            data.Name = charData->Data.CharacterName.ToString();
                            data.CombatLevel = charData->Summary.CombatLevel;
                            data.OverallLevel = charData->Summary.OverallLevel;
                            data.OriginalSlotIndex = i;
                            data.Label = data.Name + " (Cb:" + std::to_string(data.CombatLevel) + " Total:" + std::to_string(data.OverallLevel) + ")";
                            
                            config->CachedCharacters.push_back(data);
                            
                            std::cout << "  Slot " << i + 1 << ": " << data.Name 
                                      << " (Combat Lvl " << data.CombatLevel << ", Overall Lvl " << data.OverallLevel << ")" << std::endl;
                        }
                    }
                }
                config->Save(g_CurrentProfile);
            } else {
                std::cout << "[AutoLogin] W_CharacterSelection_C not found! Open the character screen first." << std::endl;
            }
        });
    }
    
    {
        std::lock_guard<std::recursive_mutex> drawLock(config->ConfigMutex);
        if (!config->CachedCharacters.empty()) {
            ImGui::Text("Select Character to Auto-Login:");
            std::string previewValue = "Select Character";
            for (const auto& c : config->CachedCharacters) {
                if (c.OriginalSlotIndex == config->SelectedCharacterSlot) {
                    previewValue = c.Label;
                    break;
                }
            }
            
            if (ImGui::BeginCombo("##CharacterSelect", previewValue.c_str())) {
                for (const auto& c : config->CachedCharacters) {
                    bool isSelected = (config->SelectedCharacterSlot == c.OriginalSlotIndex);
                    if (ImGui::Selectable(c.Label.c_str(), isSelected)) {
                        config->SelectedCharacterSlot = c.OriginalSlotIndex;
                        config->Save(g_CurrentProfile);
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
    }
    
    if (ImGui::Button("4. Select Server")) {
        Boilerplate::GetInstance()->QueueTask([]() {
            std::vector<ServerDef> candidates;
            {
                std::lock_guard<std::mutex> lock(g_ServersMutex);
                auto config = Config::GetInstance();
                for (const auto& srv : g_DiscoveredServers) {
                    if (config->SelectedRegion.empty() || srv.region == config->SelectedRegion) {
                        if (config->ServerCheckboxes[srv.name]) {
                            candidates.push_back(srv);
                        }
                    }
                }
                
                // If no checkboxes selected, any server in the region is valid
                if (candidates.empty()) {
                    for (const auto& srv : g_DiscoveredServers) {
                        if (config->SelectedRegion.empty() || srv.region == config->SelectedRegion) {
                            candidates.push_back(srv);
                        }
                    }
                }
            }
            
            if (!candidates.empty()) {
                ServerDef targetServer = candidates[rand() % candidates.size()];
                std::cout << "[AutoLogin] Attempting to join randomly selected server: " << targetServer.name << std::endl;
                
                g_JoinedServerName = targetServer.name;
                g_JoinedServerIP = targetServer.ip;
                g_JoinedServerRegion = targetServer.region;
                
                auto entryWidgets = FindWidgets<SDK::UW_ServerSlotEntry_C>();
                bool found = false;
                
                for (auto* entryWidget : entryWidgets) {
                    if (entryWidget->ServerSlotName.ToString() == targetServer.name) {
                        std::cout << "[AutoLogin] Found Server UI Widget! Clicking it..." << std::endl;
                        if (auto serverList = FindWidget<SDK::UW_ServerSlotsList_C>()) {
                            if (serverList->ServerSlotListView && entryWidget->As_Server_Slot_Entry) {
                                serverList->ServerSlotListView->BP_SetSelectedItem(entryWidget->As_Server_Slot_Entry);
                            }
                        }
                        SDK::UFunction* clickFunc = entryWidget->Class->GetFunction("W_ServerSlotEntry_C", "Click");
                        if (clickFunc) {
                            entryWidget->ProcessEvent(clickFunc, nullptr);
                            found = true;
                            break;
                        }
                    }
                }
                
                if (!found) {
                    std::cout << "[AutoLogin] Could not find the UI widget for " << targetServer.name << std::endl;
                    std::cout << "[AutoLogin] Please ensure the server is visible in the scroll list before clicking Select Server!" << std::endl;
                }
            } else {
                std::cout << "[AutoLogin] No valid servers found matching the criteria!" << std::endl;
            }
        });
    }
    
    ImGui::Separator();
    
    if (ImGui::Button("Test Logic Button")) {
        // Test logic
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Clear Console")) {
        Diagnostics::ClearConsole();
    }
    
    ImGui::End();
}
