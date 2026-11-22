#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Writes legal/NOTICE.txt: the notices the application's guest carries.

Ours and PrBoom+'s licence (GPL-2.0-or-later), the DOOM shareware game data
embedded in it, PrBoom+'s authors and the files in it under other licences
(read from their own files), the NBT App SDK (MPL-2.0) and its third-party
notices (NBT_SDK_DIR, default ../sdk), and picolibc, which the NBT
toolchain's runtime links statically (NBT_TOOLCHAIN, default the SDK's
installed release). Run again when the pin, the SDK or the toolchain
changes.
"""
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SDK = Path(os.environ.get("NBT_SDK_DIR", ROOT.parent / "sdk"))
CORE = ROOT / "third_party/prboom-plus"
SRC = CORE / "prboom2/src"
RULE = "\n" + "=" * 72 + "\n\n"

HEADER = """NBTFoundry

NBTFoundry nf-doom
Copyright (c) 2026 Fabien Poussin
Original software: GPL-2.0-or-later. You may redistribute and modify it under that license.
This software comes WITHOUT ANY WARRANTY, to the extent permitted by law.
Original NBTFoundry artwork: MPL-2.0. Trademark rights are separate.
Source: https://github.com/NBTFoundry/nf-doom
When redistributing binaries, include applicable notices and provide the source required by the component licenses.

The application's binary contains PrBoom+ and is distributed under the GNU
General Public License, version 2 or later: its complete source is the
repository above with its submodule (coelckers/prboom-plus) and the NBT App
SDK it names (release/sdk.ref). It also embeds the DOOM shareware episode
(assets/doom1.wad), id Software's game data, which the GPL does not cover.

Development notices; release blockers:
- DOOM shareware: permission to redistribute the extracted/embedded doom1.wad has not been established; see legal/doom-shareware.md
"""


def toolchain() -> Path:
    if os.environ.get("NBT_TOOLCHAIN"):
        return Path(os.environ["NBT_TOOLCHAIN"])
    version = re.search(r"^NBT_TOOLCHAIN_VERSION := (\S+)", (SDK / "module/flags.mk").read_text(), re.M)
    return SDK / "toolchain" / f"nbt-toolchain-{version.group(1)}"


def line_comment(path: Path) -> str:
    """A source file's licence: its leading // comment, without the comment marks."""
    lines = []
    for line in path.read_text().split("\n"):
        if not line.startswith("//"):
            break
        lines.append(re.sub(r"^// ?", "", line).rstrip())
    return "\n".join(lines).strip("\n-") + "\n"


def main():
    runtime = toolchain() / "share/licenses/picolibc"
    if not runtime.is_dir():
        sys.exit(f"no picolibc notices in {runtime}: set NBT_TOOLCHAIN")
    revision = subprocess.run(["git", "-C", str(CORE), "describe", "--tags", "--always"],
                              capture_output=True, text=True).stdout.strip()
    sdk_notices = subprocess.run([sys.executable, str(SDK / "tools/third_party_notices.py")],
                                 check=True, capture_output=True, text=True).stdout
    parts = [
        HEADER,
        "Head-unit risk and disclaimer\n\n" + (ROOT / "DISCLAIMER.md").read_text().split("\n", 1)[1],
        "GPL-2.0-or-later: this application and PrBoom+\n\n" + (ROOT / "LICENSES/GPL-2.0-or-later.txt").read_text(),
        "DOOM shareware game data (assets/doom1.wad)\n\n"
        + (ROOT / "LICENSES/LicenseRef-DOOM-Shareware.txt").read_text(),
        f"PrBoom+ {revision}, the game engine (third_party/prboom-plus)\n"
        "https://github.com/coelckers/prboom-plus; GNU GPL version 2 or later (above). Its authors:\n\n"
        + (CORE / "prboom2/AUTHORS").read_text(),
        "UMAPINFO parser, in PrBoom+ (prboom2/src/umapinfo.cpp): GNU LGPL version 2 or later,\n"
        "used here under the GPL above\n\n" + line_comment(SRC / "umapinfo.cpp"),
        "Scanner, in PrBoom+ (prboom2/src/scanner.cpp), under its BSD licence\n\n" + line_comment(SRC / "scanner.cpp"),
        "MPL-2.0: the NBT App SDK (https://github.com/NBTFoundry/sdk)\n\n" + (ROOT / "LICENSES/MPL-2.0.txt").read_text(),
        sdk_notices,
        "picolibc, the C library of the NBT toolchain's runtime, linked statically "
        "(https://github.com/picolibc/picolibc)\n\n"
        + (runtime / "COPYING.picolibc").read_text() + RULE.lstrip("\n")
        + "picolibc's newlib-derived parts\n\n" + (runtime / "COPYING.NEWLIB").read_text(),
    ]
    text = "\n\n" + RULE.join(p.rstrip("\n") + "\n" for p in parts)
    (ROOT / "legal").mkdir(exist_ok=True)
    (ROOT / "legal/NOTICE.txt").write_text(text)
    print(f"legal/NOTICE.txt: {len(parts)} sections")


if __name__ == "__main__":
    main()
