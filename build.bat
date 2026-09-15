@echo off
echo Building HexDec TC (Tauri release build)...
call npx tauri build
echo.
echo Build finished! You can find the installer and executable in
echo   src-tauri\target\release\bundle
pause
