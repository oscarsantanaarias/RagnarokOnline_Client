# Ragnarok Online Client Reconstruction

A modern C++20 1:1 engine reconstruction of the Ragnarok Online client (`Ragexe`), reverse-engineered from native x86 binaries.

This project implements an offline-first client architecture capable of loading raw client assets and GRF archives, rendering 2D/3D composite scenes, handling actor sprites, animations, attachment hierarchies, collision maps (`.gat`), and world definitions (`.rsw`).

## Features

- **Standard C++20 Core**: Clean, modular systems architecture with zero third-party dependencies outside standard Windows platform APIs (`gdi32`, `winmm`, `imm32`, `ole32`, `dbghelp`).
- **1:1 Resource Pipeline**:
  - **GRF Archive Loader**: Custom deflate/inflate streaming reader with table decryption and CRC validation.
  - **SPR Loader**: Full palette mapping, 8-bit indexed frames, RLE compression decoding, and 32-bit RGBA sprite channels.
  - **ACT Loader**: Multi-action animation tracks, clip transformations, frame delays, and 16-byte multi-point attachment anchors.
  - **GAT World Collision**: Cell-based altitude sampling and walkability bitflag parsing.
  - **RSW World Structure**: Scene models, light sources, and ambient sound placement tables.
- **Hierarchical Composite Rendering**:
  - Proper neck/head anchor alignment matching native client logic (`body.attach_point[0] - head.attach_point[0]`).
  - Palette color tinting, transparency keying, and directional mirroring across all 8 facing angles.
  - Clean frame pacing locked at a rock-solid 60 FPS using microsecond-resolution steady clocks.
- **Actor & Navigation System**:
  - 8-direction character locomotion with trigonometric angle mapping.
  - Integrated click-to-move pathing and direct directional input.
- **Offline First**: Runs standalone without requiring network sockets, patch servers, or login daemons.

## Project Structure

```text
├── include/
│   └── ro/
│       ├── core/       # Application lifecycle, types, crash handler, command line
│       ├── resource/   # GRF, SPR, ACT, inflate parsers and resource management
│       ├── world/      # GAT collision and RSW scene data
│       ├── actor/      # Character entity, motion state, and direction logic
│       ├── camera/     # Viewport projection and camera tracking
│       ├── render/     # Software rasterizer and composite actor renderer
│       └── mode/       # Mode manager, LoginMode, and GameMode state machines
├── src/                # Implementation files
├── CMakeLists.txt      # Cross-platform CMake configuration
├── RagnarokOnline.sln  # Visual Studio 2022 Solution
├── RagnarokOnline.vcxproj # Visual Studio C++ project configuration
└── LICENSE             # GNU General Public License v3.0
```

## Building

### Visual Studio 2022

Open `RagnarokOnline.sln` in Visual Studio 2022 and build the solution (Release or Debug, x86 or x64).

### CMake & MinGW-w64

```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Direct GCC Command Line

```bash
g++ -std=c++20 -O2 -Iinclude \
    src/main.cpp \
    src/core/exception_handler.cpp \
    src/core/command_line.cpp \
    src/core/application.cpp \
    src/mode/mode_mgr.cpp \
    src/mode/login_mode.cpp \
    src/mode/game_mode.cpp \
    src/resource/grf.cpp \
    src/resource/resource_mgr.cpp \
    src/resource/spr.cpp \
    src/resource/act.cpp \
    src/resource/inflate.cpp \
    src/world/gat.cpp \
    src/world/rsw.cpp \
    src/actor/actor.cpp \
    src/camera/camera.cpp \
    src/render/renderer.cpp \
    -lgdi32 -lwinmm -limm32 -lole32 -ldbghelp \
    -o "Full Client/Ragexe.exe"
```

## Running

Ensure your client asset folder (`data/` or `data.grf`) is present inside the working directory or `Full Client/`:

```bash
cd "Full Client"
./Ragexe.exe
```

## License

This project is licensed under the **GNU General Public License v3.0** (GPL-3.0). See the [LICENSE](LICENSE) file for details.

