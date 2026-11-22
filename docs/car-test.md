<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Car test: Doom before a release

Parked, ignition on. About 30 minutes, all from the phone: no adapter, no
computer.

Run it on a candidate before tagging a release, and whenever the car-side
behaviour changed. Doom is a **guest**: NBT Manager's `nbt-runner` copies it
out to Doom's data folder and starts it as its own process, executes its GL
command stream on the GPU and plays its sound from a shared ring.

## What you need

- NBT Manager 0.3.1 or later on the car, and its phone app, paired.
- From the kit folder: the candidate `doom-<version>.nbt`, a second one with a
  higher sequence for part C, and their `SHA256SUMS` file. Copy them to the
  phone.
- A USB gamepad (Xbox or DualShock 4 / DualSense), for B.4.

## A. Install, health check, settings

1. In the phone app's library, add the candidate and open it. Under the file
   name: **PrBoom+** (the version), **Project: PrBoom+
   (https://github.com/coelckers/prboom-plus)**, **Licence: GPL-2.0-or-later**,
   then the text, ending with the repository's address.
2. Send it to the car (an install, or an update of **doom**) and approve on the
   car. The health check (about 10 s) must pass.
3. Hold the push on **doom**. It shows 7 settings: Turn Sensitivity, Invert
   Turn, Brightness, Always Run, Sound, Show FPS and Debug log. Set **Show
   FPS: on**.

## B. Start and play

1. Select **doom**. Within a few seconds the title screen appears and the
   demo loop plays, **with music and sound effects**.
2. MENU → New Game → a skill level: E1M1. Note the FPS counter while walking
   and in a fight.
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
3. While Doom runs, send the second package from the phone and approve. The
   update stops the running copy, runs the health check and installs. Start
   Doom again: the settings page shows the new version, and the save from B.6
   loads.

## Logs (before switching the car off: `/dev/shmem` is RAM)

Doom's log: in the phone app, **doom** → **Show log**, then **Share**. It holds
the runner's lines (`runner: …`) and Doom's own (more with Debug log on).
`runner: N frames presented` counts frames every minute and at the end;
`runner: GL error 0x…` names the first GL error; a crash shows as
`runner: guest killed by signal N`. A black screen with sound points at the GL
stream; sound missing with a picture points at the audio ring. For more, the
phone's **Collect everything** (Install NBT Manager on a car › More tools,
over the Wi-Fi ENET adapter).

## Back

Uninstall **doom** from the car's page, or install the last release from
GitHub (a test package with a lower sequence is refused).

## Report

| Step | Result | Notes |
| --- | --- | --- |
| A.1 information shown | | |
| A.2 install or update + health check | | |
| A.3 7 settings shown and kept | | |
| B.1 title + demo, music and sound | | |
| B.2 FPS walking / fighting | | |
| B.3 iDrive controls, sound during play | | |
| B.4 gamepad named, plays, unplug | | |
| B.5 brightness from the phone, live | | |
| B.6 save, quit, load | | |
| C.1 stop | | |
| C.2 start again | | |
| C.3 update while running; save kept | | |

Add the car's software version (Navigation → Settings → Position and
software version) and the pad model.
