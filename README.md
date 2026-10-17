# BudsPlus

A native Windows app for controlling Nothing and CMF earbuds and headphones. It is a desktop port of
[ear (web)](https://earweb.bttl.xyz/), with the same protocol and features, built as a single portable `.exe`.
It needs no browser, no Web Serial, and no install.

Unofficial: not affiliated with or endorsed by Nothing Technology Limited.

## Features

- **Battery**: left, right and case levels, with charging state, shown in the window, the title strip and the tray tooltip.
  A balloon warns when an earbud drops to 15% or below.
- **Noise control**: noise cancelling, transparency or off, plus strength (high / mid / low / adaptive) where the model supports it.
  The tray menu can switch it too.
- **Equalizer**: listening-mode presets (Dirac OPTEO, Pop, Rock, Electronic, Enhance vocals, Classical) and a
  custom 3-band EQ (bass / mid / treble, −6 to +6 dB).
- **Ultra Bass**: on/off and level.
- **Spatial audio**: the modes the model supports.
- **Controls**: per-earbud gestures for double tap, triple tap, tap and hold (including the noise-control cycle), and double tap and hold.
- **Settings**:
  - in-ear detection;
  - low-latency mode;
  - personal sound;
  - dual connection, with the list of connected devices;
  - audio codec (AAC / LDAC);
  - ear-tip fit test.
- **Find my earbuds**: ring the left or right earbud.
- **App behaviour**:
  - detects the model through Fast Pair; you can also pick it by hand under More > Model;
  - reconnects automatically, including after a reboot that a codec change triggers;
  - light, dark and AMOLED themes;
  - per-monitor DPI;
  - optional close / start minimised to the notification area.

Controls appear only when the connected model supports them. These are built from ear (web)'s per-model configuration.
The full gesture editor is for the "espeon" family: CMF Buds Pro 2, CMF Buds 2 Plus, and CMF Buds 2.

## Using it

1. Pair the earbuds in **Windows Settings > Bluetooth & devices**: open the case and hold the button until the light flashes.
2. Run `dist\BudsPlus.exe`. Paired Nothing / CMF devices are listed; click **Connect**.
   Later runs connect to the last device by themselves.
3. Take the earbuds out of the case. The first connection can take up to 20 seconds.

Settings are stored in `%APPDATA%\BudsPlus\settings.ini`. Use More > Open settings folder to find it.
Set `BUDSPLUS_SETTINGS` to a path to keep the settings next to the exe or elsewhere.
Set `BUDSPLUS_LOG` to a file path to log every packet sent and received, in hex, for troubleshooting.
Start the app with `--minimized` to open straight to the tray, for example from a startup shortcut.

## Building

Requires TDM-GCC (or another MinGW-w64 g++ with C++17) and PowerShell.

```bash
powershell -ExecutionPolicy Bypass -File build.ps1
```

`Build.cmd` does the same with a double-click. The build makes these files in `dist\`:

- `BudsPlus.exe`: static, so no DLLs are needed;
- `engine-tests.exe`: protocol tests;
- `ui-tests.exe`: integration tests that drive the real exe against a simulated pair of earbuds.

Both test suites run on every build; pass `-SkipTests` to skip them.

### Trying the UI without earbuds

```bash
g++ -std=c++17 -O2 -municode -static tools/fake-buds.cpp -o dist/fake-buds.exe -lws2_32
```

Run `dist\fake-buds.exe`, then start BudsPlus with `BUDSPLUS_FAKE=127.0.0.1:47100:5C587F` set.
The last part is a Fast Pair model ID; `5C587F` is the CMF Buds 2 Plus.
To capture screenshots, use `tools\Capture-Window.ps1 -Out shot.png [-Key 0x23] [-Send <command>]`.

## Files

| File | What it is |
| --- | --- |
| `main.cpp` | The window, custom-painted cards, tray, menus, connection state machine |
| `protocol.hpp` | Packet building, CRC, stream framing, and decoding replies into device state |
| `models.hpp` | Fast Pair model IDs and per-model capabilities, from ear (web)'s config |
| `bt.hpp` | Paired-device list and the RFCOMM link (Winsock `AF_BTH`) on a worker thread |
| `settings.hpp` | The INI settings file |
| `fakebuds.hpp` | Simulated earbuds over TCP, for the tests |
| `tests.cpp`, `ui-tests.cpp` | Test suites |
| `packaging/Make-Assets.ps1` | Draws the app icon |

## License

BudsPlus is licensed under the [GNU Affero General Public License v3.0](LICENSE).
It ports the protocol and model data of [ear (web)](https://github.com/radiance-project/ear-web), which is AGPL-3.0.
