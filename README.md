# FovB0t

A lightweight FOV changer for the new 64-bit version of Call of Duty: Modern Warfare 3 (2011) (`iw5mp.exe`).

**WORKS AFTER THE MW3 UPDATE FROM [3 September 2026](https://steamdb.info/patchnotes/24615914/)**

## Features

- Change the in-game FOV from 40 to 120
- Reset the FOV to the default value of 65
- Simple in-game ImGui interface
- Toggle the menu with the `INSERT` key
- Game input is blocked while interacting with the menu
- Mouse-look is disabled while the menu is open
- Native 64-bit DLL

## Usage

1. Download `FovB0t.dll` from the latest GitHub Release.
2. Start the 64-bit version of the game.
3. Inject `FovB0t.dll` into `iw5mp.exe` using a DLL injector of your choice.
4. Press `INSERT` to open or close the FovB0t menu.
5. Use the FOV slider to select your preferred field of view.

## Controls

| Key | Action |
| --- | --- |
| `INSERT` | Open / close the FovB0t menu |

## Building From Source

FovB0t is written in C++ and built using Visual Studio.

### Requirements

- Visual Studio with C++ development tools
- Windows x64
- DirectX 9 SDK/runtime components required by the project

The repository includes the required ImGui and MinHook source files.

To build:

1. Open `FovB0t.slnx` in Visual Studio.
2. Select `Release`.
3. Select `x64`.
4. Build the solution.

The compiled DLL will be generated in the x64 Release output directory.

## Third-Party Libraries

FovB0t uses:

- Dear ImGui
- MinHook

Please refer to their respective projects and licenses for more information.

## Compatibility

FovB0t is designed for the new 64-bit version of Call of Duty: Modern Warfare 3 (2011).

The tool currently relies on offsets specific to the supported game build. Future game updates may require an updated version of FovB0t.

## Disclaimer

This project was created for educational purposes.

Use it at your own risk. The author is not responsible for crashes, incompatibilities, account actions, or other issues resulting from its use.

## Version

**FovB0t v1.0**

Initial release featuring an in-game FOV changer and ImGui interface.
