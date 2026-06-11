# AutoLogin & Menu Bypass: Patch Survival Guide

This document is a comprehensive breadcrumb trail and survival guide designed for future developers (or future AI assistants) to repair the AutoLogin and Menu Bypass DLL in the event of a game patch.

## 1. What Breaks During a Patch?
Because this mod is built on an Unreal Engine 5 C++ SDK (generated via Dumper-7), it relies on static memory offsets, hardcoded UFunction names, and Engine-level pointers. When a game updates, the executable changes.
- **Memory Offsets Shift:** Adding a single new variable to a class shifts all subsequent variables in memory. Reading shifted memory will crash the game.
- **Blueprint Function Names Change:** UFunction names often contain auto-generated node IDs (e.g., `..._K2Node_ComponentBoundEvent_0_...`). If the developer modifies the Blueprint graph, this ID changes, breaking our automated button clicking.
- **GObjects Address Changes:** The global array of all engine objects (`GObjects`) moves. If the Dumper-7 signature scan fails to find the new address, the DLL will crash on injection.

---

## 2. Standard Patch Day Repair SOP
If a patch drops and the DLL starts crashing or stops clicking buttons, follow these exact steps to repair it:

1. **Wait for the Game Update:** Ensure the game is fully patched and runs normally.
2. **Re-Dump the SDK:**
   - Launch the game.
   - Inject **Dumper-7** (or your preferred UE5 SDK dumper).
   - Wait for the dump to complete.
3. **Replace the SDK Folder:**
   - Navigate to the newly dumped `CppSDK` output folder.
   - Copy the entire `SDK` folder.
   - Paste and overwrite the `SDK` folder inside `m:\MCP Server\Trainers\AutoLogin and Menu Bypass\`.
4. **Recompile the DLL:**
   - Open your compiler (or ask the AI to run `cmake --build . --config Release`).
   - If it compiles cleanly, you are 90% done.
5. **Fix Compilation Errors (If Any):**
   - If an error states `Class has no member named X`, the developers renamed or deleted a variable. You must inspect the new SDK header for that class to find the new variable name.
   - If the button clicking no longer works (but doesn't crash), the Blueprint Delegate names changed. See the section below on how to find the new names.

---

## 3. Breadcrumb Trail: How We Built This (For Future AIs)

If the standard repair SOP fails, here is exactly how the core functionalities were reverse-engineered so you can reproduce the logic.

### A. Finding Widget Class Names
**The Goal:** We needed to find the UI menus (Main Menu, Login Menu, Character Selection, Escape Menu) to interact with them.
**The Method:** 
- We used an active game session and observed the **Console Logs**.
- Unreal Engine's `LogUIActionRouter` (part of the CommonUI plugin) actively prints the names of the active "leaf-most nodes" (the currently focused UI element).
- **Example Log:** `Applying input config for leaf-most node [W_EscapeMenu_C_2147479665]`
- By stripping the instance ID (`_2147479665`), we identified the core classes:
  - `UW_MainMenu_C` (Main Menu)
  - `UW_LogIn_C` (Steam Login)
  - `UW_CharacterSelection_C` (Character Screen)
  - `W_EscapeMenu_C` (Pause Menu)
  - `W_HUD_C` (Heads Up Display)

### B. Finding Hardcoded Blueprint Button Functions
**The Goal:** We needed a way to artificially click "Play Online", "Login", and "Enter World" without using mouse macros.
**The Method:**
- We opened the dumped SDK headers (e.g., `ProjectSandbox_classes.hpp` or `UMG_classes.hpp`) and searched for the widget classes we found above (e.g., `class UW_CharacterSelection_C`).
- Inside these classes, Unreal Engine generates `UFunction` delegates for every button click event authored in Blueprint. We searched the class definition for strings like `Button`, `Click`, or `BndEvt`.
- **Example Find:** 
  ```cpp
  void UW_CharacterSelection_C::BndEvt__W_CharacterSelection_PlayButton_K2Node_ComponentBoundEvent_5_CommonButtonBaseClicked()
  ```
- To call this in C++, we use the exact string name and pass `nullptr` to the signature delegate:
  ```cpp
  widget->BndEvt__W_CharacterSelection_PlayButton_K2Node_ComponentBoundEvent_5_CommonButtonBaseClicked__DelegateSignature(nullptr);
  ```
- **If this breaks:** Open the new SDK file for `UW_CharacterSelection_C`, search for `PlayButton` or `Clicked`, and replace the old function name with the newly generated one.

### C. Extracting Data Structures (Characters & Servers)
**The Goal:** We needed to read the list of characters (Combat Level, Name) and the list of available Servers (Name, Region, IP) to populate our ImGui overlay.
**The Method:**
- Inside `UW_CharacterSelection_C`, we found an array: `TArray<class UCharacterSelectionData*> Characters;`
- We inspected `UCharacterSelectionData` and found it held nested structs:
  - `Data.CharacterName.ToString()`
  - `Summary.CombatLevel`
  - `Summary.OverallLevel`
- For servers, we couldn't easily read an array from the parent widget, so we iterated `GObjects` searching for instances of `UServerSlotEntry`.
- By casting objects to `UServerSlotEntry*`, we could directly read properties like `SlotName`, `Region`, and `ServerIP` which are populated dynamically by the game's server browser.

### D. The Escape Menu "Change Servers" Injection
**The Problem:** We wanted to trigger the "Enter World" logic (which acts as a server switcher) while in the middle of a game, specifically when the Pause Menu was open. However, detecting the Escape Menu was difficult because `RemoveFromParent()` doesn't change a widget's internal `Visibility` flag.
**The Solution:**
- Because the game uses **CommonUI**, we realized `W_EscapeMenu` inherits from `UCommonActivatableWidget`.
- We wrote `IsActivatableWidgetActive()`, which casts the widget to `UCommonActivatableWidget` and reads the native `bIsActive` boolean.
- This perfectly hooks into the game's internal layer-stack routing, causing our ImGui overlay to appear and vanish perfectly in sync with the actual game menu.

---
## Summary for Future AI
1. **Rely on the SDK:** Your primary source of truth is always the headers dumped by Dumper-7.
2. **Search `GObjects`:** If you can't find a pointer to a widget, iterate `SDK::UObject::GObjects` and match class names or string names.
3. **Use CommonUI Properties:** If tracking menu visibility is failing, look for `bIsActive` or `IsInViewport()` instead of standard Slate `Visibility`.
4. **Use QueueTask:** Always execute `ProcessEvent` or UI interactions inside `Boilerplate::QueueTask` to ensure thread safety on the GameThread.
