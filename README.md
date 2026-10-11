# HexDec TC Converter

A modern, dark-themed C++ (SDL3 + OpenGL 3 + RmlUi + Skia) application for Hex -> Decimal and Decimal -> Hex conversion, featuring built-in TrinityCore opcode support.

## Features

- **Automatic Conversion**: Instant bi-directional conversion between Hexadecimal and Decimal values.
- **Opcode Support**: Full support for TrinityCore opcodes for WoW versions 1.60.1, 10.2.7, 12.0.1, 12.0.5 and 12.1.
- **Searchable List**: All available opcodes are listed and searchable. Selecting an opcode automatically fills the conversion fields.
- **Visual Feedback**: When an entered value matches an opcode, the list automatically scrolls to it and flashes for confirmation.
- **System Tray Integration**: The application can be minimized to the system tray, where it continues to run in the background.
- **Always on Top**: Optional toggle to keep the window above all other applications.
- **Modern UI**: Dark theme and a clean, responsive design.
- **Single-File**: All resources (icon, opcode data) are embedded directly into the .exe file.

## Usage

1. Build or download `HexDec TC.exe`.
2. Launch the app.
3. Select your desired WoW version from the dropdown menu.
4. Enter a value in either field or search for an opcode name in the list.

## Development and Build

The app is a single-file C++ build (SDL3 + RmlUi 6.3 + FreeType + Skia, static CRT — no DLL/vcredist dependency). Visual Studio 2022 and CMake are required.

The UI is declared in `cpp\ui\ui.rml` + `cpp\ui\ui.rcss` (embedded as RCDATA resources), data-bound to the `App` state in `cpp\src\app.cpp` via an RmlUi data model; `cpp\src\fx.cpp` paints the animated nebula/star backdrop and the window rim.

### Release build
Use the `build-cpp.bat` file in the root directory:
```batch
build-cpp.bat
```
CMake fetches SDL3/FreeType/RmlUi/json/Skia on first configure into `cpp\build\_deps`. The result is `cpp\build\Release\hexdectc.exe` (also copied to the Desktop).

### UI development
Set `HEXDEC_UI_DIR=E:\HexDecTC\cpp\ui` and the app loads the `.rml`/`.rcss` files from disk instead of the embedded copies — press **F5** in the app to hot-reload the document without rebuilding.

### Icon
`cpp\make-ico.ps1` redraws `assets\app-icon.png` (512 px master) and repacks `cpp\src\app.ico`.

## License
This project is free to use and modify.
