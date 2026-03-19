# HexDec TC Converter

Egy modern, Windows 11 stílusú .NET 9 WPF alkalmazás Hex -> Decimal és Decimal -> Hex konverzióhoz, beépített TrinityCore opcode támogatással.

## Funkciók

- **Automatikus Konverzió**: Hexadecimális és Decimális értékek azonnali átalakítása oda-vissza.
- **Opcode Támogatás**: Támogatja a WoW 10.2.7 és 12.0 verziók TrinityCore opcode-jait.
- **Kereshető Lista**: Az összes elérhető opcode listázható és kereshető. A kiválasztott opcode értékei automatikusan kitöltik a mezőket.
- **Vizuális Visszajelzés**: Ha a beírt érték egyezik egy opcode-dal, a lista automatikusan oda görget és felvillan.
- **System Tray (Tálca) Integráció**: Az alkalmazás a tálcára kicsinyíthető, ahol folyamatosan fut.
- **Always on Top**: Bekapcsolható opció, hogy az ablak minden más felett maradjon.
- **Modern UI**: Dark theme támogatás (a címsoron is), éles szövegmegjelenítés és letisztult dizájn.
- **Single-File**: Minden erőforrás (ikon, opcode-ok) be van ágyazva az .exe fájlba.

## Használat

1. Töltsd le a legfrissebb kiadást a `publish` mappából.
2. Indítsd el a `HexDecTC.exe`-t.
3. Válaszd ki a kívánt WoW verziót a legördülő menüből.
4. Írj be egy értéket vagy keress rá egy opcode nevére a listában.

## Fejlesztés és Build

A projekt lefordításához .NET 9 SDK szükséges.

### Build szkript
A gyökérkönyvtárban található `build.bat` segítségével egyetlen, hordozható `.exe` fájlt készíthetsz:
```batch
build.bat
```
A végeredmény a `publish` mappába kerül.

## Licenc
Ez a projekt ingyenesen használható és módosítható.
