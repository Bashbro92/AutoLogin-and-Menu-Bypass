#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>

class Boilerplate {
public:
    static Boilerplate* GetInstance();
    
    // Core Functions
    void InitializeSDK();
    void DrawUI();
    void Update();

    // Task execution on Game Thread
    void QueueTask(std::function<void()> task);
    bool HasTasks() const { return m_HasTasks; }
    void ExecuteTasks();

private:
    Boilerplate();
    ~Boilerplate();

    std::vector<std::function<void()>> m_TaskQueue;
    std::mutex m_TaskMutex;
    std::atomic<bool> m_HasTasks{false};
    
    bool m_SDKInitialized = false;
};
