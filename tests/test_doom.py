#!/usr/bin/env python3
"""Integration tests for nbt-doom host binary and QNX .nbt packaging."""

import fcntl
import os
from pathlib import Path
import select
import shutil
import socket
import struct
import subprocess
import tempfile
import time
import unittest

BUILD_DIR = Path(os.environ.get("NBT_BUILD_DIR", "build"))
MGR_DIR = Path(os.environ.get("NBT_MGR_DIR", Path(__file__).resolve().parents[2] / "manager"))
HOST_BIN = BUILD_DIR / "nbt-doom-host"
QNX_BIN = BUILD_DIR / "nbt-doom-qnx"


class NbtDoomIntegrationTest(unittest.TestCase):
    def test_check_mode_ready_and_lease(self):
        self.assertTrue(HOST_BIN.exists(), f"missing {HOST_BIN}")
        with tempfile.TemporaryDirectory() as td:
            lease_path = Path(td) / "lease"
            lease_fd = os.open(lease_path, os.O_CREAT | os.O_RDWR, 0o600)
            rfd, wfd = os.pipe()
            proc = None
            try:
                os.set_inheritable(lease_fd, True)
                os.set_inheritable(wfd, True)
                env = dict(os.environ)
                env["NBT_LAUNCH"] = "check"
                env["NBT_DATA_DIR"] = td
                proc = subprocess.Popen(
                    [str(HOST_BIN), "--ready-fd", str(wfd), "--lease-fd", str(lease_fd)],
                    pass_fds=(wfd, lease_fd),
                    env=env,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                )
                os.close(wfd)
                wfd = -1
                readable, _, _ = select.select([rfd], [], [], 5.0)
                first = os.read(rfd, 16) if readable else b""
                self.assertTrue(first.startswith(b"R"), f"expected initial R, got {first!r}")
                # Check mode must keep lease and emit heartbeats until SIGTERM.
                time.sleep(1.3)
                self.assertIsNone(proc.poll(), "check mode exited early before manager stop")
                readable2, _, _ = select.select([rfd], [], [], 2.0)
                more = os.read(rfd, 16) if readable2 else b""
                self.assertTrue((first + more).count(b"R") >= 2, f"missing heartbeat: {(first + more)!r}")
                proc.terminate()
                _, err = proc.communicate(timeout=10)
                self.assertEqual(proc.returncode, 0, err.decode(errors="replace"))
            finally:
                if proc is not None and proc.poll() is None:
                    proc.kill()
                    proc.wait(timeout=5)
                if wfd >= 0:
                    os.close(wfd)
                os.close(rfd)
                os.close(lease_fd)

    def test_exclusive_lease_conflict_fails(self):
        with tempfile.TemporaryDirectory() as td:
            lease_path = Path(td) / "lease"
            lease_fd = os.open(lease_path, os.O_CREAT | os.O_RDWR, 0o600)
            fcntl.lockf(lease_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            rfd, wfd = os.pipe()
            try:
                os.set_inheritable(lease_fd, True)
                os.set_inheritable(wfd, True)
                env = dict(os.environ)
                env["NBT_LAUNCH"] = "check"
                proc = subprocess.run(
                    [str(HOST_BIN), "--ready-fd", str(wfd), "--lease-fd", str(lease_fd)],
                    pass_fds=(wfd, lease_fd),
                    env=env,
                    capture_output=True,
                    timeout=10,
                )
                os.close(wfd)
                wfd = -1
                ready = os.read(rfd, 16)
                self.assertEqual(proc.returncode, 1)
                self.assertEqual(ready, b"")
            finally:
                if wfd >= 0:
                    os.close(wfd)
                os.close(rfd)
                os.close(lease_fd)

    def test_invalid_ready_fd_rejected(self):
        proc = subprocess.run(
            [str(HOST_BIN), "--ready-fd", "2", "--lease-fd", "4"],
            capture_output=True,
            timeout=5,
        )
        self.assertEqual(proc.returncode, 2)

    def _run_xvfb_launch(self, sound_on):
        r_disp, w_disp = os.pipe()
        os.set_inheritable(w_disp, True)
        xvfb = subprocess.Popen(
            ["Xvfb", "-displayfd", str(w_disp), "-screen", "0", "800x480x24"],
            pass_fds=(w_disp,),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )
        os.close(w_disp)
        try:
            disp = os.read(r_disp, 32).decode().strip()
        finally:
            os.close(r_disp)
        self.assertTrue(disp.isdigit(), f"unexpected Xvfb display: {disp!r}")

        with tempfile.TemporaryDirectory() as td:
            lease_path = Path(td) / "lease"
            lease_fd = os.open(lease_path, os.O_CREAT | os.O_RDWR, 0o600)
            rfd, wfd = os.pipe()
            pad_host, pad_child = socket.socketpair(socket.AF_UNIX, socket.SOCK_DGRAM)
            pad_host.setblocking(False)
            pad_child.setblocking(False)
            os.set_inheritable(pad_child.fileno(), True)
            proc = None
            try:
                os.set_inheritable(lease_fd, True)
                os.set_inheritable(wfd, True)
                env = dict(os.environ)
                env["DISPLAY"] = f":{disp}"
                env["NBT_LAUNCH"] = "user"
                env["NBT_DATA_DIR"] = td
                env["NBT_NO_AUDIO"] = "1"
                env["NBT_SETTING_SOUND"] = "on" if sound_on else "off"
                env["NBT_SETTING_DEBUG_LOG"] = "on"
                env["NBT_BRIDGE_CAPS"] = "gamepad"
                env["NBT_GAMEPAD_FD"] = str(pad_child.fileno())
                env["LIBGL_ALWAYS_SOFTWARE"] = "1"
                # Neutral state + Menu tap over the live gamepad socket.
                pad_host.send(struct.pack("<IIBBhhhhHH", 1, 0, 1, 0, 0, 0, 0, 0, 0, 0))
                pad_host.send(struct.pack("<IIBBhhhhHH", 2, 0x0080, 1, 0, 0, 0, 0, 0, 0, 0))
                pad_host.send(struct.pack("<IIBBhhhhHH", 3, 0, 1, 0, 0, 0, 0, 0, 0, 0))
                proc = subprocess.Popen(
                    [
                        str(HOST_BIN),
                        "--ready-fd",
                        str(wfd),
                        "--lease-fd",
                        str(lease_fd),
                    ],
                    pass_fds=(wfd, lease_fd, pad_child.fileno()),
                    env=env,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                )
                os.close(wfd)
                wfd = -1
                pad_child.close()
                readable, _, _ = select.select([rfd], [], [], 10.0)
                ready = os.read(rfd, 16) if readable else b""
                self.assertEqual(ready, b"R", "failed to signal ready under Xvfb")

                # Allow Doom either to run frames (when doom1.wad is embedded)
                # or exit cleanly via I_Error on missing IWAD.
                time.sleep(0.8)
                has_wad = (Path(__file__).resolve().parent.parent / "assets" / "doom1.wad").exists()
                rc = proc.poll()
                if has_wad and rc is not None:
                    out, err = proc.communicate(timeout=5)
                    self.fail(
                        f"Doom exited early ({rc}) despite embedded doom1.wad:\n"
                        f"{err.decode(errors='replace')}"
                    )
                if rc is None:
                    proc.terminate()
                    out, err = proc.communicate(timeout=10)
                    self.assertEqual(
                        proc.returncode,
                        0,
                        f"clean SIGTERM exit failed: {err.decode(errors='replace')}",
                    )
                    # Doom logs to stderr only: the manager keeps it as the app's log.
                    self.assertFalse((Path(td) / "doom.log").exists())
                    log_text = err.decode(errors="replace")
                    if sound_on:
                        self.assertIn("I_InitSound rate=", log_text)
                    else:
                        self.assertIn("I_InitSound skipped (nosound)", log_text)
                    self.assertIn("[doom][dbg]", log_text)
                    self.assertRegex(log_text, r"gamepad_fd=\d+")
                    self.assertNotIn("gamepad_fd=-1", log_text)
                    self.assertIn("stage: running", log_text)
                    self.assertIn("I_SafeExit rc=0", log_text)
                else:
                    out, err = proc.communicate(timeout=5)
                    self.assertNotEqual(rc, 0)
            finally:
                if proc is not None and proc.poll() is None:
                    proc.kill()
                    proc.wait(timeout=5)
                if wfd >= 0:
                    os.close(wfd)
                os.close(rfd)
                os.close(lease_fd)
                pad_host.close()
                if pad_child.fileno() >= 0:
                    pad_child.close()
                xvfb.terminate()
                xvfb.wait(timeout=5)
                if xvfb.stderr:
                    xvfb.stderr.close()

    def test_xvfb_egl_initialization_and_launch(self):
        if shutil.which("Xvfb") is None:
            self.skipTest("Xvfb unavailable")
        self._run_xvfb_launch(sound_on=False)
        self._run_xvfb_launch(sound_on=True)

    def test_qnx_package_and_verify(self):
        package_py = MGR_DIR / "tools" / "package.py"
        verify_bin = MGR_DIR / "build" / "nbt-verify"
        if not QNX_BIN.exists() or not verify_bin.exists():
            self.skipTest("QNX binary or nbt-verify not built")

        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            priv = td_path / "app.key"
            pub = td_path / "app.pub"
            out_nbt = td_path / "doom-1.0.0.nbt"
            subprocess.run(
                ["openssl", "genrsa", "-out", str(priv), "3072"],
                check=True,
                capture_output=True,
            )
            subprocess.run(
                ["openssl", "rsa", "-in", str(priv), "-pubout", "-out", str(pub)],
                check=True,
                capture_output=True,
            )
            subprocess.run(
                [
                    "python3",
                    str(package_py),
                    "--app",
                    "doom",
                    "--version",
                    "1.0.0",
                    "--sequence",
                    "1",
                    "--compat",
                    "nbt_6.5_armv7",
                    "--key",
                    str(priv),
                    "--bridge",
                    "1.4",
                    "--bridge-uses",
                    "show,controller",
                    "--bridge-optional",
                    "status,live-settings,gamepad",
                    "--setting",
                    "turn_sens|Turn Sensitivity|medium|low,medium,high",
                    "--setting",
                    "invert_turn|Invert Turn|off|off,on",
                    "--setting",
                    "brightness|Brightness|2|0,1,2,3,4",
                    "--setting",
                    "always_run|Always Run|on|off,on",
                    "--setting",
                    "sound|Sound|on|off,on",
                    "--setting",
                    "show_fps|Show FPS|off|off,on",
                    "--setting",
                    "debug_log|Debug log|off|off,on",
                    "--output",
                    str(out_nbt),
                    str(QNX_BIN),
                ],
                check=True,
                capture_output=True,
            )
            v = subprocess.run(
                [str(verify_bin), str(out_nbt), str(pub), "doom", "nbt_6.5_armv7"],
                check=True,
                capture_output=True,
                text=True,
            )
            self.assertIn("app=doom\n", v.stdout)
            self.assertIn("bridge=1.4\n", v.stdout)
            self.assertIn("bridge-uses=show,controller\n", v.stdout)
            self.assertIn("bridge-optional=status,live-settings,gamepad\n", v.stdout)
            self.assertNotIn("bridge-most", v.stdout)
            raw = out_nbt.read_bytes()
            mlen = int.from_bytes(raw[8:12], "big")
            manifest = raw[24 : 24 + mlen].decode("ascii")
            self.assertNotIn("setting=aspect|", manifest)
            self.assertIn("setting=debug_log|Debug log|off|off,on\n", manifest)


if __name__ == "__main__":
    unittest.main()
