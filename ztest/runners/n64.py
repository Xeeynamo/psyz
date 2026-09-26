import contextlib
import os
import shutil
import signal
import socket
import subprocess
import tempfile
import time

from .base import Driver, Stream, fail, find_gdb, read_fd, terminate

MARKER = b"@ZTEST_ARGS@"


class N64Emu(Driver):
    """ares with its GDB server; ares waits for gdb at boot, then gdb prints
    every ztest_n64_log call. Arguments are patched into a copy of the ROM."""

    target = "n64"

    def launch(self, args):
        ares = self.executable("ares")
        gdb = find_gdb()
        port = self.opts.gdb_port or 9123
        rom = self.opts.exe
        base = os.path.splitext(rom)[0]
        elf = next((p for p in (base + ".elf", os.path.join(
            os.path.dirname(rom), "obj", os.path.basename(base) + ".elf"))
            if os.path.isfile(p)), None)
        if not os.path.isfile(rom) or not elf:
            fail("ROM or ELF not found next to %s" % rom)
        self.gdb_pid = None
        self.rom = self.patched_rom(rom, args)
        # ares always opens a window; without a display run it in a
        # headless compositor.
        wrap = []
        headless = self.opts.headless or not (
            os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY"))
        if headless:
            if not shutil.which("gamescope"):
                fail("no display and no gamescope for a headless ares")
            wrap = ["gamescope", "--backend", "headless", "--"]
        self.proc = subprocess.Popen(
            wrap + [ares, "--system", "Nintendo 64", "--no-file-prompt",
             "--setting", "General/HomebrewMode=true",
             "--setting", "Boot/AwaitGDBClient=true",
             "--setting", "DebugServer/Enabled=true",
             "--setting", "DebugServer/UseIPv4=true",
             "--setting", "DebugServer/Port=%d" % port,
             "--setting", "Video/Driver=None",
             "--setting", "Audio/Driver=None", self.rom],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.wait_for_port(port)
        self.script = self.gdb_script(port)
        import pty
        self.gdb_pid, self.fd = pty.fork()
        if self.gdb_pid == 0:
            os.execvp(gdb, [gdb, "-q", "-batch", "-iex", "set auto-load off",
                            "-x", self.script, elf])
        return Stream(self.console())

    @staticmethod
    def patched_rom(rom, args):
        with open(rom, "rb") as f:
            data = bytearray(f.read())
        at = data.find(MARKER)
        if at < 0:
            fail("%s has no %s marker" % (rom, MARKER.decode()))
        text = " ".join(args).encode()[:255] + b"\0"
        data[at:at + len(text)] = text
        fd, path = tempfile.mkstemp(suffix=".z64")
        with os.fdopen(fd, "wb") as f:
            f.write(data)
        return path

    def wait_for_port(self, port):
        deadline = time.time() + 20
        while time.time() < deadline:
            if self.proc.poll() is not None:
                fail("ares exited early")
            with contextlib.suppress(OSError):
                socket.create_connection(("127.0.0.1", port), 0.5).close()
                return
            time.sleep(0.2)
        terminate(self.proc)
        fail("ares GDB server did not open port %d" % port)

    def gdb_script(self, port):
        lines = ["set pagination off", "set confirm off", "set width 0",
                 "target remote 127.0.0.1:%d" % port,
                 "break ztest_n64_log", "commands", "silent",
                 'printf "%s", (char*)$a0', "continue", "end", "continue"]
        fd, path = tempfile.mkstemp(suffix=".gdb")
        with os.fdopen(fd, "w") as f:
            f.write("\n".join(lines) + "\n")
        return path

    def console(self):
        for data in read_fd(self.fd):
            yield data.replace(b"\r", b"")

    def stop(self):
        if self.gdb_pid:
            with contextlib.suppress(ProcessLookupError):
                os.kill(self.gdb_pid, signal.SIGKILL)
            with contextlib.suppress(OSError):
                os.close(self.fd)
            with contextlib.suppress(ChildProcessError):
                os.waitpid(self.gdb_pid, 0)
            for path in (self.script, self.rom):
                with contextlib.suppress(OSError):
                    os.remove(path)
        super().stop()
