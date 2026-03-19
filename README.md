# HexDec TC Converter

A modern, Windows 11-style .NET 9 WPF application for Hex -> Decimal and Decimal -> Hex conversion, featuring built-in TrinityCore opcode support.

## Features

- **Automatic Conversion**: Instant bi-directional conversion between Hexadecimal and Decimal values.
- **Opcode Support**: Full support for TrinityCore opcodes for WoW versions 10.2.7 and 12.0.
- **Searchable List**: All available opcodes are listed and searchable. Selecting an opcode automatically fills the conversion fields.
- **Visual Feedback**: When an entered value matches an opcode, the list automatically scrolls to it and flashes for confirmation.
- **System Tray Integration**: The application can be minimized to the system tray, where it continues to run in the background.
- **Always on Top**: Optional toggle to keep the window above all other applications.
- **Modern UI**: Dark theme support (including the title bar), sharp text rendering, and a clean, responsive design.
- **Single-File**: All resources (icon, opcode data) are embedded directly into the .exe file.

## Usage

1. Download the latest release from the `publish` folder.
2. Launch `HexDecTC.exe`.
3. Select your desired WoW version from the dropdown menu.
4. Enter a value in either field or search for an opcode name in the list.

## Development and Build

Building the project requires the .NET 9 SDK.

### Build Script
Use the `build.bat` file in the root directory to create a single, portable `.exe` file:
```batch
build.bat
```
The resulting executable will be placed in the `publish` folder.

## License
This project is free to use and modify.
