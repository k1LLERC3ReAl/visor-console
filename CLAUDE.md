# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Full rebuild:
```powershell
cd "C:\Cerea_K\visor_console"
gcc -Wall -Wextra -std=c11 -Isrc -c src/main.c           -o src/main.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/serial.c         -o src/serial.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/sensor_bar.c     -o src/sensor_bar.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/diagnostic.c     -o src/diagnostic.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/ui.c             -o src/ui.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/config.c         -o src/config.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/utils.c          -o src/utils.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/boards.c         -o src/boards.o
gcc -Wall -Wextra -std=c11 -Isrc -c src/upload.c         -o src/upload.o
gcc src/main.o src/serial.o src/sensor_bar.o src/diagnostic.o src/ui.o src/config.o src/utils.o src/boards.o src/upload.o -o visor.exe
```

Run:
```powershell
.\visor.exe
```

## Architecture

Visor Console is a Windows diagnostic tool for sensor bars connected to Arduino. It can:
1. Backup Arduino firmware before diagnostics
2. Compile and upload a diagnostic sketch with user-selected baudrate
3. Display real-time sensor readings and diagnostics
4. Restore original firmware automatically when done

**Main data flow:**
```
Arduino (COM port, tab-delimited text)
  → serial.c        reads raw lines via Windows HANDLE API
  → sensor_bar.c    parses sensor values and calibration lines
  → diagnostic.c    classifies each sensor (NEGRO / GRIS / BLANCO / TOUT)
  → ui.c            renders full-screen console display

upload.c ←→ arduino-cli, avrdude   (compile, flash, backup/restore)
```

**Key modules:**

| File | Responsibility |
|------|---------------|
| `src/main.c` | Menu loop, wires config + bar + serial + upload together |
| `src/config.c/.h` | Load/save `config.ini`; `AppConfig` struct with tool paths and board FQBN |
| `src/boards.c/.h` | Predefined Arduino board FQBNs (Nano old/new, Uno, Mega) |
| `src/upload.c/.h` | Wrapper around `arduino-cli` (compile, flash) and `avrdude` (backup, restore hex) |
| `src/sensor_bar.c/.h` | Parse sensor values, calibration; `SensorBar` + `SensorBarModel` |
| `src/diagnostic.c/.h` | Per-sensor classification against thresholds |
| `src/serial.c/.h` | Windows serial I/O via `CreateFile`, `SetCommState`, `ReadFile` |
| `src/ui.c/.h` | Console menus (7 options), reading screen with freeze, backup/upload/restore UI |
| `src/utils.c/.h` | Timing utilities |

**Bar types** (`BarType` enum in `sensor_bar.h`):
- `BAR_QTR8RC` (0) — 8 sensors, position 0–7000
- `BAR_IM_HEM_005` (1) — 16 sensors via 4-bit MUX, position −7500 to +7500

**Configuration** (`config.ini`, loaded at startup):
```ini
[serial]
port=COM3
baudrate=115200

[bar]
bar_type=0
threshold_black=700
threshold_white=300
timeout_ms=2000

[tools]
board_fqbn=arduino:avr:nano:cpu=atmega328old
arduino_cli_path=arduino-cli
avrdude_path=avrdude
backup_hex_path=backup_fw.hex
```
Note: `config.ini` is NOT saved automatically on exit — user must select option 7 (Save).

**Serial protocol** (Arduino → PC):
```
S1\tS2\t...\tSN\tpos\n            (sensor readings every 250ms)
Min: min1\tmin2\t...\tminN\n      (calibration minimums, sent at startup)
Max: max1\tmax2\t...\tmaxN\n      (calibration maximums, sent at startup)
```
Values normalized 0–1000 (0 = white, 1000 = black).

## Arduino sketches

- `arduino/qtr8rc_visor/qtr8rc_visor.ino` — QTR-8RC (8 sensors, A0–A5 + D12 + D9, 115200 baud, auto-calibration 10s)
- `arduino/im_hem_005/im_hem_005.ino` — IM-HEM-005 (16 sensors via 4-bit MUX A0–A4, 115200 baud, manual button calibration)

Both sketches now use 115200 baud (fixed from 9600 in im_hem_005.ino).

## Menu flow (7 options)

1. Select COM port (auto-detects available ports)
2. Select baudrate (9600, 19200, 38400, 57600, 115200)
3. **Select board** — Nano old/new, Uno, Mega, or custom FQBN
4. Select sensor bar type — loads defaults (baudrate, thresholds)
5. Configure thresholds — BLACK, WHITE, timeout_ms
6. **Start diagnostics** — Backup → Compile → Flash → Monitor → Restore
7. Save configuration

## Dependencies (external tools)

Must be installed and in PATH or configured in `config.ini [tools]`:
- `arduino-cli` — compile and flash sketches
- `avrdude` — backup/restore firmware hex

If not found, the app shows an error and exits. Paths default to tool names (assumes PATH).

## Diagnostic flow (option 6)

1. **Backup:** `avrdude` reads current flash to `backup_fw.hex`
2. **Patch baudrate:** Modifies sketch's `Serial.begin(XXXX)` to user-selected rate
3. **Compile & flash:** `arduino-cli` compiles and uploads patched sketch
4. **Wait 2s:** Arduino reboots and calibrates
5. **Monitor:** Real-time sensor display (Q to quit, F to freeze)
6. **Restore:** `avrdude` writes back `backup_fw.hex` to flash

If restore fails, the user gets a manual command to run separately.

## Windows-specific notes

- `serial.c` uses Windows API (`CreateFile`, `SetCommState`, `ReadFile`), not portable
- Console requires Windows 10+ for ANSI escape codes (UTF-8 output enabled)
- Port names format: `\\.\COM1`, `\\.\COM2`, etc.
- Binary uses UTF-8 and box-drawing characters (█, ║, etc.)
