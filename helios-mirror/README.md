# TROLLfx Helios Mirror — experimental Maxwell adapter

**Status: Windows x64 prototype, not yet validated against Maxwell's installed DLL interface.** The project builds a DLL proxy that forwards Maxwell's Helios SDK calls to the genuine Helios library while duplicating `HeliosDacWriteFrame` to two USB DACs. It is not an analog ILDA splitter.

## Build (GitHub Actions)
Open **Actions → Build Helios Mirror → Run workflow** on this branch. Download the `helios-mirror-windows` artifact. The artifact contains the proxy DLL, controller and example configuration. This is a portable test build, not a signed installer.

## Before trying with Maxwell
1. Fully close Maxwell and back up its application folder. Locate the **actual** Helios SDK DLL used by Maxwell, if any. Maxwell may bundle the library under a different name, use internal code, or not load a Helios DLL at all.
2. Never replace a system DLL. If Maxwell loads `HeliosLaserDAC-x64.dll` from a local application folder, rename that existing original to `HeliosLaserDAC-real.dll`. Keep its matching `libusb-1.0.dll` beside it.
3. Place this build's `HeliosLaserDAC-x64.dll` alongside the renamed real DLL. If Maxwell uses a different library name or exports, **stop**: this prototype will not work without adapting the interception layer.
4. Copy `mirror.ini` into that same folder. Leave `armed=0` initially. Run `python mirror_control.py` to update the settings and arm after validation. Python 3 is needed only for the optional controller GUI.
5. Launch Maxwell and select the first Helios output. Both DACs must be attached to the same PC. The controller edits the INI and the proxy reloads it automatically.
6. After testing, close Maxwell; restore the original DLL name and remove the proxy.

## Defaults and device behavior
- Single `HeliosDacOpenDevices` and `HeliosDacCloseDevices` lifecycle forwarded to the original library.
- DAC index 0 receives original frames; DAC index 1 receives transformed copies.
- Device 1 supports flip X, flip Y, size and offset. Same colors/blanking as original; intensity carried through unmodified.
- Blackout is the default; user must explicitly arm. Both paths receive zero RGB/I when disarmed. Hardware e-stop and projector interlocks remain necessary.
- Poll config every 500 ms, avoiding per-frame disk I/O.
- If fewer than 2 devices open, only the original selected DAC is used. The mirrored output is skipped.
- Device indices depend on the Helios SDK's enumeration order. Verify on actual hardware.
- `GetStatus` is checked before the second write; no buffering or retries are used. This can drop secondary frames. True phase sync is **not guaranteed**.
- Other SDK entry points may exist in Maxwell; only known exports are proxied. If an export is missing, Maxwell might not initialize. Hardware validation required.

## Scope
This is a test harness for the user's proven two-Helios SDK setup. It does **not** prove Maxwell can be intercepted this way until its installed DLL and imports are inspected. Do not connect an audience-facing projector during development.

## Local build
Use Visual Studio 2022 developer command prompt:
```
cmake -S . -B build -A x64
cmake --build build --config Release
```

Official Helios SDK: https://github.com/Grix/helios_dac
