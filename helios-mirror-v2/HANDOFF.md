# TROLLfx Helios Mirror V2 handoff

## Git state

- Repository: `420troll/Mr-strobe`
- Branch: `feature/helios-etherdream-v2`
- Implementation/build-fix head: `d4cb07dd005d041bc9bd0d055abb04f48f942633`
- Issue contract: [#1 — V2 EtherDream virtual DAC bridge](https://github.com/420troll/Mr-strobe/issues/1)
- Successful Windows x64 run: [GitHub Actions run 43532495535](https://github.com/420troll/Mr-strobe/actions/runs/43532495535)
- Artifact: `TROLLfx-Helios-Mirror-V2-windows-x64` containing `TROLLfx-Helios-Mirror-V2-win64.zip`

The existing `helios-mirror/` V1 files and workflows were not modified. Nothing was installed into Maxwell.

## Delivered

- Exact seven-function EtherDream x64 ABI required by Maxwell, including matching ordinals 18, 19, 20, 21, 23, 25, and 27.
- Read-only ABI report containing installed binary hashes, Maxwell's ordinal imports, published signatures, point layout, and documented scaling.
- Virtual single EtherDream device with Open/Status/Write/Stop/Close behavior and timestamped call diagnostics.
- One official Helios SDK lifecycle using `OpenDevicesOnlyUsb`, with independent workers for DAC 0 and DAC 1.
- Deep-copy conversion, bounded latest-frame mailboxes, independent transforms/blackouts, failure isolation, finite/continuous repetition handling, and stop cancellation.
- Native Windows controller for global arm plus per-output enable, blackout, flip, scale, and offset.
- Safe-default `mirror.ini`, read-only ABI script, guarded diagnostic install script, hash-manifested backup, and verified rollback script.
- Reproducible Windows packaging with pinned/hash-checked Helios SDK and libusb dependencies, licenses, and a package SHA256 manifest.

## Verified automatically

The successful Windows workflow completed every step:

1. Visual Studio 2022 x64 configure and Release compilation.
2. Core tests covering simulated 0/1/2-device discovery, endpoint coordinate/color conversion, clipping, blacking, independent transforms, point order, input-buffer ownership, bounded ordering, stop cancellation, validation, status, failure counters, and recovery.
3. Standalone fake-Maxwell harness loading `EtherDream.dll` by ordinal beside a mock two-device Helios SDK.
4. PE export verification using MSVC `dumpbin`.
5. Portable ZIP staging and artifact upload.

The same core suite was also compiled with strict GCC warnings on the development host and passed 25 consecutive runs.

## Explicitly not yet verified

This branch is ready for controlled hardware validation, not production deployment. The following tests require the user's Windows/laser setup:

1. With Maxwell closed, exercise the documented backup/install procedure and confirm the original DLL SHA256 is retained in the rollback manifest.
2. Start disarmed with projector shutters/interlocks engaged; confirm the log records Maxwell's `GetCardNum`, `OpenDevice`, `GetStatus`, and especially `WriteFrame` calls. If no WriteFrame appears, stop and mark this interception route unproven.
3. Confirm Maxwell's actual X/Y and RGBI numeric ranges using known test frames before clearing either blackout. Static ABI/source evidence supports the implemented full signed-coordinate and 15-bit positive-channel scaling, but Maxwell's producer values have not been observed.
4. Connect exactly two USB Helios DACs and confirm SDK enumeration/order, names, independent blackout, flip, scale, offset, and clipping on controlled non-audience-facing projectors.
5. Measure timing, frame drops, and relative phase at representative point counts/PPS; software workers do not claim optical phase lock.
6. Unplug/fail each DAC separately and verify the other continues, counters/logging reflect the failure, and reconnection behavior is acceptable.
7. Verify Stop/Close, Maxwell mode changes, and emergency shutdown produce immediate hardware blackout with no stale frame replay.
8. Close Maxwell and execute rollback; verify the restored `EtherDream.dll` hash exactly matches the recorded original.

Do not call the build production-ready until these checks pass. The binaries are unsigned and the software does not replace physical laser safety systems.
