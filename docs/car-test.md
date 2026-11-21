<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Car test: Doom as a guest (second run under nbt-runner)

Parked, ignition on. About 30 minutes. Keep SSH/ENET recovery at hand.

The first run (guest1/guest2, kit dev120) installed and passed the health
check, then died at start with SIGILL in picolibc's `strcmp`, the only Thumb
code in the guest with `strlen` and `memchr`. guest3/guest4 are built with the
SDK's ARM-state versions of the three, and the SDK now refuses Thumb code in
any payload.

Doom built without the QNX SDP: `nbt-runner` (built with the QNX 6.5 SP1 SDP)
with Doom appended as a **guest**. The runner copies Doom out to Doom's data
folder and starts it as its own process. Doom draws through a GL command
stream, which the runner executes on the GPU, and plays sound through a
shared ring. The QNX 6.5 VM ran this build with stand-ins for the GPU and
sound (16,578 frames, clean exit). This is the first run with the real ones.
The main questions are whether it starts and draws, the frame rate compared
with Doom 1.0.18 (the QNX build), sound, and input feel.

## What you need

- NBT Manager from test kit **dev120** (installed in the first run) and its
  phone app, paired.
- From the kit folder: `doom-1.0.20-guest3.nbt` and `doom-1.0.20-guest4.nbt`
  (`doom-guest3-SHA256SUMS.txt`). Copy both to the phone.
- A USB gamepad (Xbox or DualShock 4 / DualSense), for B.4.
- SSH to the head unit (logs, part D).

## A. Install, health check, settings

1. On the phone: import `doom-1.0.20-guest3.nbt` and send it to the car. It is
   an update of **doom** (1.0.20-guest2 → 1.0.20-guest3). Approve on the car.
   The health check (about 10 s) must pass.
2. Hold the push on **doom**. It shows 7 settings: Turn Sensitivity, Invert
   Turn, Brightness, Always Run, Sound, Show FPS and Debug log. Set **Show
   FPS: on** and **Debug log: on**.

## B. Start and play

1. Select **doom**. Within a few seconds the title screen appears and the
   demo loop plays, **with music and sound effects**. Note how long it took.
2. MENU → New Game → a skill level: E1M1. Note the FPS counter while walking
   and in a fight. If you remember Doom 1.0.18's numbers, compare them.
3. iDrive controller:
   - tilt to move and turn;
   - knob to strafe;
   - push to fire;
   - BACK tap to use or open, double tap for the next weapon;
   - OPTION for the automap;
   - favourite keys 1–7 for weapons;
   - BACK hold or MENU for Doom's menu.
   Open a door, pick something up, kill something: note any sound gap or
   stutter.
4. Plug the gamepad in. A notice names it. Play: left stick to move, right
   stick to turn, RT to fire, A to use. Unplug it: the iDrive controller plays
   again.
5. While playing, change **Brightness** from the phone (the application's
   settings). It applies without restarting.
6. Doom's menu → Save Game into a slot. Quit with a long BACK hold (1.4 s): the
   manager returns. Start **doom** again → Load Game: the save is there.

## C. Stop, start again, update

1. Hold the push on **doom** → **Stop** (or the long BACK hold in game): the
   screen returns.
2. Start it again: it starts as in B.1.
3. While Doom runs, send `doom-1.0.20-guest4.nbt` from the phone and approve.
   The update stops the running copy, runs the health check and installs.
   Start Doom again: the settings page shows 1.0.20-guest4, and the save from
   B.6 loads.

## D. Over SSH (optional)

- `pidin arg | grep -e doom -e nbt-payload`: the application
  (`…/apps/doom/bin/app`, the runner) and its child, the guest
  (`…/.nbt-payload-<pid>` in Doom's data folder).
- `kill <app pid>` (SIGTERM): a clean stop as in C.1; the guest ends too.
- `kill -9 <guest pid>`: the application ends ("guest killed by signal 9"),
  and the manager shows it stopped. Start it again: it runs.

## Logs (before switching the car off: `/dev/shmem` is RAM)

- Doom's log: in the phone app, **doom** → **Show log**, then **Share**.
  It holds the runner's lines (`runner: …`) and Doom's own (more of them with
  Debug log on). `runner: 1 frames presented` means the first frame reached
  the screen; a count follows every minute and at the end, and
  `runner: GL error 0x…` names the first GL error. A crash shows as
  `runner: guest killed by signal N`, after
  `Process … (.nbt-payload-…) terminated SIG… ip=…`. A black screen with
  sound points at the GL stream; sound missing with a picture points at the
  audio ring.
- The manager's debug log (`PROCEDURE.md` step 9).

## Back

To return to the QNX build, uninstall **doom** and install Doom 1.0.20 from
the GitHub release (sequence 1000020). After that, test packages need a
higher sequence than 1000020.

## Report

| Step | Result | Notes |
| --- | --- | --- |
| A.1 update to guest3 + health check | | |
| A.2 7 settings shown and kept | | |
| B.1 title + demo, music and sound (time to start) | | |
| B.2 FPS walking / fighting (and 1.0.18's if known) | | |
| B.3 iDrive controls, sound during play | | |
| B.4 gamepad named, plays, unplug | | |
| B.5 brightness from the phone, live | | |
| B.6 save, quit, load | | |
| C.1 stop | | |
| C.2 start again | | |
| C.3 update while running; save kept | | |
| D clean stop, guest killed | | |

Add the car's software version (Navigation → Settings → Position and
software version) and the pad model.
