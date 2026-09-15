# HexDec TC Converter

A modern, dark-themed Tauri + Svelte + Rust application for Hex -> Decimal and Decimal -> Hex conversion, featuring built-in TrinityCore opcode support.

## Features

- **Automatic Conversion**: Instant bi-directional conversion between Hexadecimal and Decimal values.
- **Opcode Support**: Full support for TrinityCore opcodes for WoW versions 10.2.7, 12.0.1, 12.0.5 and 12.1.
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

Building the project requires Node.js, the Rust toolchain, and the Tauri prerequisites (WebView2 + MSVC build tools on Windows).

### Dev mode
```batch
npm install
npx tauri dev
```

### Release build
Use the `build.bat` file in the root directory:
```batch
build.bat
```
The resulting installer and executable will be placed in `src-tauri\target\release\bundle`.

## License
This project is free to use and modify.
