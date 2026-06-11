cmake_minimum_required(VERSION 3.24)
project(AutoLogin_and_Menu_Bypass CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# FetchContent for Dependencies
include(FetchContent)

# 1. ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

# 2. MinHook
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.3
)

# Disable conflicting Kiero2 backends
set(KIERO_BUILD_D3D9 OFF CACHE BOOL "" FORCE)
set(KIERO_BUILD_D3D10 OFF CACHE BOOL "" FORCE)
set(KIERO_BUILD_D3D11 OFF CACHE BOOL "" FORCE)
set(KIERO_BUILD_OPENGL OFF CACHE BOOL "" FORCE)
set(KIERO_BUILD_VULKAN OFF CACHE BOOL "" FORCE)
set(KIERO_BUILD_D3D12 ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    kiero
    GIT_REPOSITORY https://github.com/kirchesz/kiero2.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(imgui minhook kiero)

# The base TrainerOverlay SDK Path (Update this if it moves)
set(SDK_PATH "C:/Dumper-7/5.7.4-0+UE5-ProjectSandbox/CppSDK/SDK")

# Build UniversalBoilerplate.dll
add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/Hooks.cpp
    src/Boilerplate.cpp
    src/Diagnostics.cpp
    src/PatternScanner.cpp
    src/KernelMouseInterface.cpp
    src/Config.cpp
    
    # ImGui sources required
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    
    # MinHook sources required
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde32.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c

    # Basic UE SDK Dependencies
    "${SDK_PATH}/Basic.cpp"
    "${SDK_PATH}/CoreUObject_functions.cpp"
    "${SDK_PATH}/Engine_functions.cpp"
    "${SDK_PATH}/ProjectSandbox_functions.cpp"
    "${SDK_PATH}/UMG_functions.cpp"
    "${SDK_PATH}/W_CharacterSelection_functions.cpp"
    "${SDK_PATH}/W_LogIn_functions.cpp"
    "${SDK_PATH}/W_MainMenu_functions.cpp"
)

# Includes
target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${minhook_SOURCE_DIR}/include
    "C:/Dumper-7/5.7.4-0+UE5-ProjectSandbox/CppSDK"
)

# Add kiero compile definitions to support D3D12
target_compile_definitions(${PROJECT_NAME} PRIVATE KIERO_BUILD_D3D12)

# Libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    kiero::kiero
    d3d12
    d3d11
    d3dcompiler
    dxgi
    iphlpapi
    ws2_32
)

target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)
