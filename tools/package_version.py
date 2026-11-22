#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Keeps the PrBoom+ version in release/package.args equal to the pin's: the
phone app shows it before an install (NBT Manager's PROTOCOL.md, "Package
information"). It is the pinned prboom2/CMakeLists.txt's project VERSION.
`make package-args` writes it; with --check (`make test`), fails when the file
lags the pin.

    package_version.py [--check] release/package.args
"""
import re
import sys

LABEL = "PrBoom+"
UPSTREAM = "third_party/prboom-plus"


def pinned():
    path = UPSTREAM + "/prboom2/CMakeLists.txt"
    with open(path) as f:
        version = re.search(r'^project\("PrBoom-Plus"\s+VERSION\s+([0-9][0-9.]*)', f.read(), re.M)
    if not version:
        sys.exit("%s: no project VERSION" % path)
    return version.group(1)


def main():
    args = sys.argv[1:]
    check = args[:1] == ["--check"]
    args = args[1:] if check else args
    if len(args) != 1:
        sys.exit(__doc__)
    path = args[0]
    with open(path) as f:
        lines = f.read().split("\n")
    at = [i + 1 for i in range(len(lines) - 1)
          if lines[i] == "--property" and lines[i + 1].startswith(LABEL + "|")]
    if len(at) != 1:
        sys.exit("%s: needs one --property %s|<version>" % (path, LABEL))
    want = "%s|%s" % (LABEL, pinned())
    if lines[at[0]] == want:
        return
    if check:
        sys.exit("%s: %s, the pin builds %s: make package-args" % (path, lines[at[0]], want))
    lines[at[0]] = want
    with open(path, "w") as f:
        f.write("\n".join(lines))


if __name__ == "__main__":
    main()
