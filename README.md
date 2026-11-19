# nbt-doom

Standalone PrBoom+ port for **NBT Manager** (QNX 6.5 ARMv7 + Linux host) with
GLES2 3D hardware acceleration, QNX `io-audio` / ALSA mixed DMX SFX + OPL2 FM music,
and shared BMW iDrive / wired USB gamepad input.

## Repository layout

- `third_party/prboom-plus/` — unmodified upstream PrBoom+ submodule
- `src/prboom_compat/` — freestanding SDL2/OpenGL compatibility headers, BMP loader, and `w_wad.c` VFS wrapper
- `src/gles2/` — GL1-to-GLES2 matrix stack, fixed-function GLSL ES 1.00 shader, quad/polygon batcher, and full-screen viewport scaler
- `src/wad_vfs.*`, `src/wad_embed.S`, `src/wad_embed_reg.cpp` — `.rodata` WAD embedding and virtual `/embedded/*` VFS
- `src/lifecycle.*` — NBT Manager bridge 1.4 handshake (`--ready-fd`, `--lease-fd`, `NBT_STATUS_FD`, `NBT_CONTROLLER_FD`, `NBT_GAMEPAD_FD`, `NBT_SETTINGS_FILE`, signals)
- `src/input_idrive.*` — iDrive controller input from the manager (`NBT_CONTROLLER_FD`, the SDK's `nbt::controller::Input`): knob, push, tilt, MENU, BACK, OPTION and the favourite keys
- `src/video_egl.*` — QNX Screen (`screen_create_window`) / Linux X11 + EGL + GLES2 context wrapper
- `src/sound_qnx.*` — 16-channel SFX + OPL2 mixer over runtime `dlopen("libasound.so.2")` (`io-audio` on QNX, ALSA on Linux)
- `src/i_*_nbt.cpp`, `src/main.cpp` — PrBoom+ platform backend glue and entrypoint

## Supplying `doom1.wad`

Place Shareware DOOM v1.9 at `assets/doom1.wad` (or pass `DOOM1_WAD=/path/to/doom1.wad`).
When omitted, host builds and tests use `-DNBT_NO_EMBEDDED_DOOM1` and look for `doom1.wad`
on disk in `NBT_DATA_DIR`, `.`, `assets/`, or `/app/data`.

## Build & test

```bash
# Build host binary, run unit tests + integration suite
nice make -j $(nproc) host test test-asan check-static-init

# Run Doom locally on X11 + Mesa EGL
./build/nbt-doom-host -iwad /path/to/doom1.wad

# Cross-compile and verify QNX 6.5 ARMv7 ELF
nice make -j $(nproc) qnx qnx-check

# Sign a .nbt package for NBT Manager (requires private RSA-3072 key)
nice make -j $(nproc) doom-package DOOM1_WAD=/path/to/doom1.wad APP_KEY=/path/to/apps.key
```

## Logs

Doom writes its messages to stderr and keeps no log file. NBT Manager pipes an
application's stdout and stderr into its log on the car
(`state/doom/output.log`, rotated at 64 KiB), notes how it ended (exit status
or signal), and lets the phone app show, share and clear it (**Show log** on
Doom's page). A crash adds a `[doom] FATAL sig=… code=… addr=… stage=…` line. The
**Debug log** setting (`debug_log`) adds verbose tracing. Older versions wrote
`doom.log` in the data folder; it is removed at start.

## iDrive controls (in-game / menu)

- **Rotate dial**: Strafe left / right (sensitivity sets hold duration; scrolls items in menu)
- **Tilt stick** (8-way): Move forward/backward & turn left/right (turn inversion configurable; navigates menus)
- **Push dial**: Fire weapon (Enter in menu)
- **BACK short tap**: Use / open doors (Backspace in menu)
- **BACK double-tap** (< 350 ms): Next weapon
- **BACK hold (>= 600 ms)** or **MENU button**: Toggle Doom Escape menu
- **BACK long hold (>= 1.4 s)**: Clean exit to NBT Manager (before manager's 2.0 s forced escape)


## Wired gamepads

Since 1.0.16 the package needs bridge 1.4 (NBT Manager kit dev111 or later):
required show/controller, optional status/live-settings/gamepad. The manager
decodes the iDrive controller and sends it on NBT_CONTROLLER_FD; Doom no longer
reads MOST. Bridge 1.3 supplies NBT_GAMEPAD_FD; Doom uses the manager's shared
wire decoder, neutral/reconnect guard and bounded nonblocking reader without
linking USB code.

Volume: the car's knob sets the master volume, as for any source; Doom's own
**Options → Sound Volume** (effects and music) scales its output below that
and is saved in `prboom-plus.cfg` in Doom's data folder. The car does not go
below 25 for the browser audio source Doom plays through (NBT-AAD's native
audio shows the same floor).

Test hook: a file `audio-conn` in Doom's data folder
(`/var/opt/nbt-manager/data/doom/audio-conn`) holding one connection number
(as `audioconn -l` lists them, e.g. `0x9a`) makes Doom build that connection
instead of `0xb8`; the log says so. Delete the file to go back.

Controller mapping in a level: knob turn strafes, push fires, tilt moves and
turns, BACK uses (twice quickly: next weapon; held: menu, then quit), MENU
opens the menu, OPTION the automap, favourite keys 1-7 pick weapons 1-7.
While Doom is shown, NBT Manager's capture shim keeps those keys from iDrive.

Xbox One and PS4 controllers share the same mapping. PS5 DualSense input is
experimental until tested on hardware. One wired controller is active at a time.

| Gamepad | In game |
| --- | --- |
| Left stick / D-pad | Move forward/back and strafe |
| Right stick horizontal | Proportional turning |
| RT/R2 or X/Square | Fire |
| A/Cross | Use/open |
| LT/L2 or left stick click | Sprint while held (respects Always Run) |
| LB/L1 / RB/R1 | Previous / next weapon |
| Y/Triangle or View/Share/Create | Automap |
| B/Circle or Menu/Options | Escape/menu |

In menus the D-pad/left stick navigates, A/Cross confirms and B/Circle backs
out. Stick deadzone is 20%; trigger press/release thresholds are 25%/20%.
Menu navigation repeats after 400 ms every 100 ms. All sources share held-key
ownership, so unplugging a pad cannot release an iDrive/keyboard-held action.
Disconnect, lost focus, malformed state, or 500 ms without valid snapshots
clears gamepad actions. Controls held during connection require a neutral state.

The build compiles the dependency-free gamepad SDK directly from NBT_MGR_DIR
for host, QNX and sanitizer tests. Xbox One and PS4 car checks are required
before NAS publication; see the manager's docs/gamepad-bridge.md.
