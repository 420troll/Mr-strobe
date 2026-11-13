# EtherDream ABI verification

Verified on 2026-11-13 against the installed Maxwell files. This inspection was read-only; neither file was replaced or modified.

## Installed binaries

| File | Architecture | Size | SHA256 |
|---|---:|---:|---|
| `C:\Program Files\Maxwell\resources\support\EtherDream.dll` | PE32+ x86-64 | 397,312 | `0f5dadade848905d92164dc091d8b5659f9ff118a465bda9f1604849cde03e3e` |
| `C:\Program Files\Maxwell\resources\externals\jit.maxwell.mxe64` | PE32+ x86-64 | 149,504 | `e39a38a4329a3b5e2d37444e4cf18613f380e9bc824c3adf096ba163353fa777` |

`objdump -p` inspected the PE export and import tables. The installed EtherDream DLL has 33 named exports at ordinals 1 through 33. Maxwell imports these seven entries **by ordinal**, not by symbol name:

| Ordinal | Installed export | Published signature |
|---:|---|---|
| 18 | `EtherDreamCloseDevice` | `bool __stdcall (const int *CardNum)` |
| 19 | `EtherDreamGetCardNum` | `int __stdcall (void)` |
| 20 | `EtherDreamGetDeviceName` | `void __stdcall (const int *CardNum, char *buf, int max)` |
| 21 | `EtherDreamGetStatus` | `int __stdcall (const int *CardNum)` |
| 23 | `EtherDreamOpenDevice` | `bool __stdcall (const int *CardNum)` |
| 25 | `EtherDreamStop` | `bool __stdcall (const int *CardNum)` |
| 27 | `EtherDreamWriteFrame` | `bool __stdcall (const int *CardNum, const EAD_Pnt_s *data, int Bytes, uint16_t PPS, uint16_t Reps)` |

The V2 `.def` file preserves these ordinals. Windows x64 uses one platform calling convention, but the declarations retain `__stdcall` to match the published source ABI and any x86 analysis tools.

## Point representation and scaling

The published `j4cDAC.h` at commit `ebd615f190ddd55a105702db0f2e537df4459a7b` defines `EAD_Pnt_s` as eight consecutive signed 16-bit fields (`X,Y,R,G,B,I,AL,AR`), exactly 16 bytes. Compile-time size and offset assertions enforce that layout.

The reference driver's conversion code passes signed X/Y directly to the EtherDream protocol and shifts each color/intensity field left by one. V2 therefore maps:

- X/Y: `-32768..32767` linearly to Helios `0..4095`, with exact endpoint mapping.
- R/G/B/I: clamp negative values to zero, then linearly map `0..32767` to `0..255`.
- AL/AR: retained in the input ABI but not emitted because the original Helios `HeliosPoint` has only XY/RGBI.
- A zero I value remains zero, preserving intentional intensity blanking. Global disarm or per-output blackout zeros all RGBI channels.

The official Helios SDK API and point layout were checked at commit `5881d9f8dd092f18be726cdfd687b9908108d62a`: `HeliosPoint` is 8 bytes with unsigned 12-bit XY stored in `uint16_t` and unsigned 8-bit RGBI.

## Status, lengths, rate, and repetitions

- `Bytes` is validated as a positive multiple of 16 and capped at the Helios USB maximum of 4095 points.
- PPS is passed as points per second and zero is rejected.
- EtherDream READY and BUSY are 1 and 2. V2 reports BUSY while accepted work is queued or being sent.
- Repetition 0 is a successful no-op, matching the reference driver. `65535` means continuous playback. Other positive values are sent as finite, single-frame repetitions on the worker thread.
- Calls are logged with timestamps, byte/sample counts, PPS, repetitions, and acceptance result.

## What remains unverified

The static import table proves which functions Maxwell resolves, not that a normal Maxwell session calls `EtherDreamWriteFrame`. No proxy has been installed into Maxwell, and no two-DAC hardware test has been run. A controlled diagnostic session must establish the runtime call sequence and confirm Maxwell's actual channel ranges before this route can be called validated.
