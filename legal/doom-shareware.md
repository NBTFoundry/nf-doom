<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# DOOM shareware game data

`assets/doom1.wad` is id Software's proprietary DOOM Shareware v1.9 game data. It is not covered by the engine's GPL or by NBTFoundry's artwork license.

The existing file was compared byte for byte with DOOM1.WAD in the original `doom19s.zip` distribution. Its two DOOMS_19 volumes concatenate into the original self-extracting ZIP; comparison did not execute any DOS programs.

- Original distribution mirror: https://youfailit.net/pub/idgames/idstuff/doom/doom19s.zip
- Archive SHA-256: cacf0142b31ca1af00796b4a0339e07992ac5f21bc3f81e7532fe1b5e1b486e6
- WAD SHA-256: 1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771

This establishes provenance, not permission to redistribute an extracted WAD embedded in a new binary. The original archive inspected does not supply an explicit grant establishing that permission. Permission to distribute the extracted WAD must be established before publishing a package containing it. The asset is retained unchanged and is not silently removed or relabeled as open source.

## Decision

On 2026-11-22 the maintainer decided that packages and releases embed this file, as
local builds always did; the CI release build fetches it from Git LFS (doom 1.0.20
and 1.0.21 embedded only the LFS pointer by mistake, so they carry no game data).
No explicit grant was found: the maintainer distributes it on that basis. Using
separately supplied game data instead (the user's own `doom1.wad`, or Freedoom)
remains a possible later change to the build and distribution policy.
