@echo off
echo Building HexDec TC Single-File Release (Optimized Size)...
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:PublishReadyToRun=true -p:IncludeNativeLibrariesForSelfExtract=true -p:EnableCompressionInSingleFile=true --output ./publish
echo.
echo Build finished! You can find the executable in the 'publish' folder.
echo Size optimization: Compression is enabled.
pause
