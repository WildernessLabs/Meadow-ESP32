# CLion project configuration

The CLion project files in `.idea/` and `CMakePresets.json` are committed, so
they must not contain a path that only exists on one machine. They name tools
by indirection instead:

| What | Committed reference | Resolved on each machine by |
|---|---|---|
| CMake profile | `CMakePresets.json` preset `Debug-ESP-IDF 6.1` (builds into `build/`, shared with `build.sh`) | the CLion toolchain named below |
| Compiler, CMake environment | toolchain name `ESP-IDF 6.1` | CLion → Settings → Build → Toolchains |
| OpenOCD (debug server) | `$PROJECT_DIR$/tools/clion/openocd` | `tools/clion/idf-env.sh` (IDF's `openocd-esp32`) |
| GDB | `$PROJECT_DIR$/tools/clion/gdb` | `tools/clion/idf-env.sh` (IDF's `xtensa-esp32-elf-gdb`) |

## Run configurations (folder *ESP 6.1*)

| Configuration | What it does |
|---|---|
| **App Flash** | `idf.py app`, then flashes the application over JTAG with `flash-app.cfg`, as `./flash.sh` does |
| **Build and Flash All** | `idf.py build`, then flashes bootloader, partition table and application over JTAG with `flash-all.cfg`, as `./flash.sh --all` does |
| **Debug (OpenOCD)** | runs **App Flash**, then starts OpenOCD over JTAG with `debug.cfg`, resets and halts the ESP32, and attaches GDB |
| **ESP Flash Monitor** | `idf.py flash monitor` in a terminal: flashes over the ESP-Prog's UART with esptool, then opens the console |

The ESP-Prog shows up as two serial ports: channel A is JTAG (OpenOCD) and
channel B is the UART (esptool and the console). If esptool picks the wrong
port, set `ESPPORT` to channel B's `/dev/cu.usbserial-*` in the login shell.
All of these use `build/`, the same directory as `build.sh`.

## One-time setup per machine

1. Install ESP-IDF 6.1 with the Espressif installer (EIM), so that
   `~/.espressif/tools/activate_idf_v6.1.sh` exists.
2. In CLion, add a toolchain named exactly **ESP-IDF 6.1**: System toolset,
   with the environment file set to `~/.espressif/tools/activate_idf_v6.1.sh`.
3. Open the project. CLion loads the `Debug-ESP-IDF 6.1` profile from
   `CMakePresets.json`, and the run configurations from
   `.idea/runConfigurations`.

## Overrides

The wrappers read these variables, either from the login shell (CLion on
macOS reads it at startup, so restart CLion after a change) or from
`tools/clion/tools.local.env`, which is not committed (see
`tools.local.env.example`):

| Variable | Effect |
|---|---|
| `IDF_VERSION` | IDF to use, default `6.1` |
| `IDF_ACTIVATE` | full path to an IDF activation script |
| `OPENOCD` | specific OpenOCD binary |
| `IDF_GDB` | specific GDB binary |

## Troubleshooting

**Debug session ends at once with "Debugger exited with code 137 (SIGKILL)".**
This is rarely a GDB problem. CLion kills GDB when `-target-select remote
tcp:localhost:3333` times out, which happens when OpenOCD failed to start.
Check OpenOCD directly:

    tools/clion/openocd -f debug.cfg -c "init; exit"

**Do not rely on an `openocd` that is already on `PATH`.** Homebrew and distro
packages ship upstream OpenOCD, which lacks Espressif's ESP32 support; the
wrapper always takes OpenOCD from the IDF environment unless `OPENOCD` is set.
