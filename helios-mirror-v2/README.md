# TROLLfx Helios Mirror V2

Windows x64 EtherDream-compatible virtual DAC bridge for routing Maxwell XY/RGBI frames to as many as two USB Helios DACs. V2 is independent of the existing `helios-mirror/` prototype and does not rely on Maxwell's native Helios path.

## Current validation status

The installed Maxwell PE files were inspected read-only. Maxwell imports seven EtherDream functions by ordinal, and this build preserves those exact ordinals. The standalone ABI harness and simulated 0/1/2-device tests exercise the bridge without laser hardware.

**Still unproven:** Maxwell has not been run with this proxy, so a live `EtherDreamWriteFrame` call has not been observed. No two-Helios timing or optical-output test has been performed. This is an unsigned diagnostic build, not a production installer.

See [docs/ABI-VERIFICATION.md](docs/ABI-VERIFICATION.md) for the evidence and precise mappings.

## Safety behavior

- Global output is disarmed by default.
- Both DACs have independent enabled/blackout, flip X/Y, scale X/Y, and offset X/Y settings.
- Arming alone is insufficient: each output's blackout must also be cleared.
- Stop cancels queued frames before stopping both devices; queued data is never replayed after a stop.
- This software does not replace projector interlocks, shutters, scan-fail protection, an emergency stop, or controlled test conditions.

## Runtime design

`EtherDream.dll` exposes only the seven ordinals Maxwell actually imports. It dynamically loads the official `HeliosLaserDAC.dll` beside it, calls `OpenDevicesOnlyUsb` once, and owns one open/close lifecycle. Maxwell must not open the same Helios devices through another output mode at the same time.

`EtherDreamWriteFrame` validates and deep-copies the 16-byte source points. Each DAC has a one-frame latest-wins mailbox and its own worker, so USB readiness does not block Maxwell's render thread. Failure on one DAC does not stop the other. Diagnostics are appended to `helios-mirror-v2.log` beside the DLL.

## Portable artifact

The V2 GitHub Actions workflow builds and tests Windows x64, verifies the export ordinals, and creates `TROLLfx-Helios-Mirror-V2-win64.zip`. The ZIP includes:

- `EtherDream.dll`
- `TROLLfx-Helios-Mirror-V2.exe` controller
- official `HeliosLaserDAC.dll` pinned to the documented SDK commit
- official x64 `libusb-1.0.dll`
- safe-default `mirror.ini`, scripts, documentation, licenses, and SHA256 manifest

## Build and test

From a Visual Studio 2022 x64 developer prompt:

```powershell
cmake -S helios-mirror-v2 -B build-v2 -A x64
cmake --build build-v2 --config Release
ctest --test-dir build-v2 -C Release --output-on-failure
```

The ABI smoke test loads the proxy by ordinal next to a mock Helios SDK. It never accesses USB hardware.

## Controlled installation and rollback

Do not copy anything to `System32`. Do not run these steps until a controlled Maxwell diagnostic is authorized.

1. Close Maxwell and disconnect or shutter projectors.
2. Extract the portable ZIP.
3. Run `scripts\install-test.ps1` as an account allowed to write Maxwell's support directory.
4. Keep `armed=0` and both `blackout=1`; launch Maxwell and inspect `helios-mirror-v2.log` for the call sequence.
5. If no `EtherDreamWriteFrame` appears, stop: the route remains unproven.
6. Close Maxwell and run `scripts\rollback.ps1 -ManifestPath <path printed by install-test.ps1>`.

The install script refuses to continue while Maxwell is running, saves the exact original DLL under `resources\support\TROLLfx-backups`, and records original/proxy SHA256 values. Rollback verifies the saved original before and after restoration.
