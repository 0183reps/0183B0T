# 0183B0T

A lightweight and expandable in-game modification menu for the new 64-bit version of Call of Duty: Modern Warfare 3 (2011) (`iw5mp.exe`).

Originally released as **FovB0t v1.0**, the project started as a simple FOV changer.  
With **0183B0T v2.0**, it evolved into a more general-purpose in-game menu with support for multiple features.

**WORKS AFTER THE MW3 UPDATE FROM [3 September 2026](https://steamdb.info/patchnotes/24615914/)**

## Features

### Field of View

- Change the in-game FOV from 40 to 120
- Reset the FOV to the default value of 65
- Custom FOV is automatically reapplied when the game resets the FOV

### Performance

- Change the in-game FPS limit from 0 to 240
- Set the FPS limit to `0` for unlimited FPS
- Increase or decrease the FPS limit using `-` and `+`
- Hold the buttons to quickly adjust the FPS limit
- Enter an FPS limit manually
- Reset the FPS limit to the default value of 85

### Menu

- In-game Dear ImGui interface
- Toggle the menu with the `INSERT` key
- Game input is blocked while interacting with the menu
- Mouse-look is disabled while the menu is open
- Mouse cursor is released while the menu is open
- Supports windowed and fullscreen display modes
- Supports injection while the game is already running in fullscreen
- Native 64-bit DLL

## Usage

1. Download `0183B0T.dll` from the latest GitHub Release.
2. Start the 64-bit version of the game.
3. Inject `0183B0T.dll` into `iw5mp.exe` using a DLL injector of your choice.
4. Press `INSERT` to open or close the 0183B0T menu.
5. Configure the available features from the in-game menu.

## Controls

| Key | Action |
| --- | --- |
| `INSERT` | Open / close the 0183B0T menu |

## Building From Source

0183B0T is written in C++ and built using Visual Studio.

### Requirements

- Visual Studio with C++ development tools
- Windows x64
- DirectX 9 SDK/runtime components required by the project

The repository includes the required Dear ImGui and MinHook source files.

### Build

1. Open `0183B0T.slnx` in Visual Studio.
2. Select `Release`.
3. Select `x64`.
4. Build the solution.

The compiled DLL will be generated in the x64 Release output directory.

## Third-Party Libraries

0183B0T uses:

- Dear ImGui
- MinHook

Please refer to their respective projects and licenses for more information.

## Compatibility

0183B0T is designed for the new 64-bit version of Call of Duty: Modern Warfare 3 (2011).

The tool currently relies on offsets and internal game structures specific to the supported game build. Future game updates may require an updated version of 0183B0T.

## Disclaimer

This project was created for educational purposes.

Use it at your own risk. The author is not responsible for crashes, incompatibilities, account actions, or other issues resulting from its use.

## Version

**0183B0T v2.1**

Bugfix release improving FOV persistence across game state changes.

### What's New in v2.1

- Fixed custom FOV resetting after death and respawn
- Fixed custom FOV resetting when entering a new match
- Custom FOV is now automatically reapplied when the game resets `cg_fov`

### What's New in v2.0

- Renamed FovB0t to 0183B0T
- Expanded the project from a dedicated FOV changer into a multi-feature menu
- Added FPS limit control with a range of 0 to 240
- Added unlimited FPS support
- Added manual and button-based FPS limit controls
- Improved menu input handling
- Added proper mouse-look blocking while the menu is open
- Improved DirectX 9 renderer integration
- Added stable windowed and fullscreen mode switching
- Added support for direct injection while already running in fullscreen
- Improved project structure for adding future features