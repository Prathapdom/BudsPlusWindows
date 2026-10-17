# BudsPlus: notes for working on this code

Native C++17 Win32 app with no frameworks, in the same style as TDM (F:\Prathap\Others\TDownloadManager).
It ports ear (web) (https://earweb.bttl.xyz/, source: github.com/radiance-project/ear-web) to the desktop.

## Build and test
- Run `powershell -File build.ps1`. It uses TDM-GCC 9.2 at `F:\Software\TDM-GCC-64\bin\g++.exe` and windres from the same folder.
  It builds `dist\BudsPlus.exe`, then runs `engine-tests.exe` and `ui-tests.exe`. Both must pass.
- ui-tests start the real exe with three environment variables:
  - `BUDSPLUS_FAKE=127.0.0.1:<port>:<FastPairId>`: connect over TCP to `FakeBuds` (fakebuds.hpp) instead of Bluetooth;
  - `BUDSPLUS_TEST=1`: confirmations answer yes and balloons are off;
  - `BUDSPLUS_SETTINGS=<temp ini>`.
  The tests drive the app with `WM_COMMAND` (the command ids in `enum Cmd` in main.cpp).
  They read app state through `WM_TESTQUERY` (WM_APP+100; see `test()` in main.cpp), then check what FakeBuds received.
  When you add a command or query, keep the copies in ui-tests.cpp in step.
- For screenshots: run `dist\fake-buds.exe` (from tools/fake-buds.cpp), start the app with BUDSPLUS_FAKE set, then run `tools\Capture-Window.ps1`.
  Its `-Key 0x23` option scrolls to the end.

## Protocol facts (from ear (web)'s bluetooth_socket.js)
- **Transport:** RFCOMM, control service `aeac4a03-dff5-498f-843a-34487cf133eb`.
  The Fast Pair service `df21fe2c-...` sends `03 01 00 03 <model id 3 bytes>` first.
  That model ID maps to a base such as B184 through `models::FastPairIds`.
- **Packet:** `55 60 01 cmd(LE16) len(LE16) op payload crc16-modbus(LE)`.
  Replies may or may not carry the CRC; the Framer detects it by value.
  Do not use byte1 & 0x20 as a CRC flag, because 0x60 always has that bit set.
- **Command ranges:** get 0xC0xx, set 0xF0xx, reply 0x40xx, push 0xE0xx / 0x60xx.
  Reply offsets in `proto::apply` index the whole packet, header included, matching the web code.
- **Custom EQ:** bands go on the wire in the order mid, treble, bass.
  When reading, gains sit at offset 14 + i*13.
- **Noise-control gesture codes:** 10 = all three modes, 20 = NC + off, 21 = transparency + off, 22 = transparency + NC.
- **Reboots:** changing the codec (and dual connection on `dualReboot` models) reboots the earbuds.
  The app sets `awaitingReboot` and reconnects.
- **Conflicts (from ear (web)):**
  - on Dirac tier-1 models (B168, B172, B179), Dirac OPTEO and LDAC exclude each other;
  - on B170, spatial audio excludes bass enhance and advanced EQ.

## What a real CMF Buds Pro 2 does (firmware 0.0.8.8, 2026-10-17, captured with `BUDSPLUS_LOG`)
- Replies carry **no CRC**.
- Some unsolicited reports arrive with header `55 20 01` and op 0, for example spatial, custom EQ, latency and bass echoes after a set.
- Dual list: the payload is `total, offset, count-in-page`, then the devices. The earbuds return one device per page,
  and the app stops paging at `total`.
- The gesture reply has 15 records; device 4 is the case.
- Changing settings while on LDAC can reboot the earbuds and send them back to AAC. This was seen when selecting Dirac
  and then spatial audio with LDAC on.
- Any command sent while the link is down is dropped. The app re-reads everything after reconnecting.
- The captured replies are regression tests in `testRealCaptures()` (tests.cpp).

## Code conventions (shared with TDM)
- TDM-GCC quirks:
  - `far` / `near` are macros;
  - call newer user32 APIs through GetProcAddress;
  - write `std::max<LONG>` when mixing LONG and int;
  - `_WIN32_WINNT` is set to 0x0A00 at the top of the headers.
- UI: everything is painted in `render()`, which lays out, draws, and records clickable rectangles in `hits`.
  `card()` measures first, then draws. Write icon glyphs as `\uE…` escapes (Segoe Fluent Icons, falling back to MDL2).
- Look: Windows 11 Fluent, matching TDM: the TDM palette and blue accent, Segoe UI Variable at 14/12/20 px, 4px control and 8px card radii.
  Draw rounded shapes and circles with `fillRounded()` / `roundRect()` / `circle()`, never GDI `RoundRect` / `Ellipse`.
  paint() renders into a 32-bit DIB (`gCanvas`), and those helpers anti-alias the corners into it. GDI curves look jagged.
- Settings go in the INI file, never the registry. Saves are atomic (temp file, then rename).
- Hardware: the developer's PC has a CMF Buds Pro 2 (B172) paired. Testing against real earbuds changes their settings,
  so ask before doing that.
