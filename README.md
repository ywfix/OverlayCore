A universal overlay base for Windows x64, built with DirectX 11 and Dear ImGui. 
Supports window targeting and an experimental ZBID mode for rendering above fullscreen application windows.

## Build

```sh
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

Run `build/bin/Release/overlay.exe` and select a window.
Insert/Home toggles the menu; End exits.

Configure modes in `src/overlay/config.h`, or override them with CMake, e.g. `-DOVERLAY_ZORDER_MODE=ON`.

## Credits

[Dear ImGui](https://github.com/ocornut/imgui)
