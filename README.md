# AutoLogin and Menu Bypass

An Unreal Engine 5 utility DLL that automates the login sequence, menu navigation, and server selection process. This trainer provides a fully featured ImGui overlay to filter servers by region, manage profiles, and monitor real-time server connections.

## Features
- **AutoLogin State Machine**: Automatically navigates from the Main Menu -> Login -> Character Selection -> Server Selection.
- **Server Filtering**: Filter and automatically connect to specific servers based on Region and Server Name.
- **Persistent Profiles**: Save your AutoLogin configurations (including multiple server checkbox filters) to local profiles.
- **Real-time Diagnostics**: On-Screen Display (OSD) showing the current active Build version, Server Connection State, Ping, and active Player Count.
- **Seamless Reset Logic**: Robust logic using the game's internal `NetDriver` and `GameState` to determine when the user connects or disconnects, effortlessly resetting the AutoLogin sequence without relying on unstable UI visibility bugs.

## Technical Details
- Built for Unreal Engine 5.
- Utilizes **MinHook** for hooking DirectX 12 (`ExecuteCommandLists` and `Present`).
- Uses **ImGui** for the overlay interface.
- Reads `UWorld` and `GameState` structures dynamically via memory offsets to extract real-time player counts and connection states.

## Usage
1. Inject `AutoLogin_and_Menu_Bypass.dll` into the target game process.
2. The ImGui overlay will appear automatically.
3. Check **Enable Background AutoLogin** to automatically start the sequence, or use the manual simulator buttons to navigate the menus manually.
4. Select your preferred servers in the "Specific Server Filter" section to limit which servers the AutoLogin system attempts to join.
