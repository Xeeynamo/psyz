import contextlib
import os
import re
import select
import shutil
import signal
import struct
import subprocess
import sys
import time

from .base import Driver, Stream, fail

ANSI = re.compile(rb"\x1b\[[0-9;?]*[A-Za-z]")

class Ps1Emu(Driver):
    """Headless pcsx-redux: PCDrv rooted at --workdir, TTY on stdout, and
    -testmode turns the program's pcsx_exit() into the emulator exit code."""

    target = "ps1"

    def launch(self, args):
        redux = self.executable("pcsx-redux")
        if not os.path.isfile(self.opts.exe):
            fail("PS-EXE not found: %s" % self.opts.exe)
        self.write_args_file(args)
        cmd = [redux, "-cli", "-testmode", "-stdout", "-pcdrv", "-pcdrvbase",
               self.opts.workdir, "-run", "-exe", self.opts.exe]
        if self.opts.bios:
            cmd[1:1] = ["-bios", os.path.abspath(self.opts.bios)]
        return self.popen(cmd, cwd=self.opts.workdir)


class Ps1Hw(Driver):
    """Real hardware running UniROM: nops uploads the PS-EXE and then stays in
    monitor mode (/m), streaming the TTY and serving PCDrv from --workdir."""

    target = "ps1"

    def launch(self, args):
        self.nops = [self.executable("nops.exe")]
        if self.nops[0].endswith(".exe"):
            if not shutil.which("mono"):
                fail("mono not found in PATH (needed to run nops.exe)")
            self.nops.insert(0, "mono")
        self.dest = self.opts.serial or "/dev/ttyUSB0"
        if not os.path.isfile(self.opts.exe):
            fail("PS-EXE not found: %s" % self.opts.exe)
        if not os.path.exists(self.dest):
            fail("serial device %s not found (pass --serial)" % self.dest)
        self.write_args_file(args)
        self.log = os.path.join(self.opts.workdir, "ztest.log")
        self.pid = None
        low_latency(self.dest)
        self.upload()
        return Stream(self.console())

    def reset(self, wait=True):
        proc = subprocess.Popen(
            self.nops + ["/fast", "/reset", "/dest", self.dest],
            cwd=self.opts.workdir, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL, start_new_session=True)
        if wait:
            proc.wait(timeout=30)

    def flush_icache(self):
        """UniROM starts an EXE without flushing the I-cache, so a different
        EXE runs stale lines of the previous one and crashes. This stub tail
        calls FlushCache (A0:44) from an uncached address."""
        stub = os.path.join(self.opts.workdir, "ztest.flush")
        with open(stub, "wb") as f:
            f.write(struct.pack("<4I", 0x240A00A0, 0x01400008, 0x24090044, 0))
        try:
            for cmd in (["/bin", "ztest.flush", "0xA0010000"],
                        ["/jal", "0xA0010000"]):
                subprocess.run(
                    self.nops + ["/fast"] + cmd + ["/dest", self.dest],
                    cwd=self.opts.workdir, stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL, timeout=2)
        except subprocess.TimeoutExpired:
            pass  # UniROM is not answering, the upload retry handles it
        finally:
            os.remove(stub)

    def upload(self):
        # An EXE entered uncached (kseg1) flushes the I-cache itself, like the
        # UPX unpacker does.
        with open(self.opts.exe, "rb") as f:
            f.seek(0x10)
            if struct.unpack("<I", f.read(4))[0] >> 29 != 5:
                self.flush_icache()
        # mono block-buffers a pipe, so nops has to run on a pty. TERM=dumb
        # stops mono waiting 1s for an answer to its cursor position query.
        with contextlib.suppress(OSError):
            os.remove(self.log)
        exe = os.path.relpath(self.opts.exe, self.opts.workdir)
        import pty
        self.started = time.time()
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.chdir(self.opts.workdir)
            os.environ["TERM"] = "dumb"
            os.execvp(self.nops[0], self.nops + ["/fast", "/exe", exe, "/m",
                                                 "/dest", self.dest])

    def kill(self):
        deadline = time.time() + 0.5
        with contextlib.suppress(ProcessLookupError, ChildProcessError):
            os.kill(self.pid, signal.SIGINT)
            while os.waitpid(self.pid, os.WNOHANG)[0] == 0:
                if time.time() > deadline:
                    os.kill(self.pid, signal.SIGTERM)
                    os.waitpid(self.pid, 0)
                    break
                time.sleep(0.01)
        self.pid = None
        with contextlib.suppress(OSError):
            os.close(self.fd)

    def console(self):
        """nops injects PCDrv traffic via TTY, so ztest writes its
        output to ztest.log through PCDrv instead; follow that file."""
        offset = 0
        uploading = False
        retried = False
        tty = b""
        while True:
            if select.select([self.fd], [], [], 0.05)[0]:
                try:
                    data = os.read(self.fd, 4096)
                except OSError:
                    return
                if not data:
                    return
                tty = (tty + data)[-64:]
                uploading = uploading or b"Sending chunk" in tty
            if not uploading and time.time() - self.started > 2:
                self.kill()
                if retried:
                    print("zrunner: PS1 not answering on %s, reset it by hand"
                          % self.dest, file=sys.stderr)
                    return
                retried = True
                self.reset()
                time.sleep(4.5)  # UniROM takes 3.5 to 4s to boot
                self.upload()
                continue
            try:
                with open(self.log, "rb") as f:
                    f.seek(offset)
                    data = f.read()
            except OSError:
                continue
            if data:
                offset += len(data)
                yield data

    def stop(self):
        if self.pid:
            self.kill()
        if not self.clean:
            self.reset(wait=False)
        super().stop()


def low_latency(dest):
    """FTDI adapters hold short replies for 16ms unless the port asks for low
    latency, which costs every PCDrv call and upload chunk a round trip."""
    if not sys.platform.startswith("linux"):
        return
    import fcntl
    import struct
    TIOCGSERIAL, TIOCSSERIAL, ASYNC_LOW_LATENCY = 0x541E, 0x541F, 1 << 13
    with contextlib.suppress(OSError):
        fd = os.open(dest, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        try:
            info = bytearray(fcntl.ioctl(fd, TIOCGSERIAL, bytes(128)))
            flags = struct.unpack_from("i", info, 16)[0]
            if not flags & ASYNC_LOW_LATENCY:
                struct.pack_into("i", info, 16, flags | ASYNC_LOW_LATENCY)
                fcntl.ioctl(fd, TIOCSSERIAL, bytes(info))
        finally:
            os.close(fd)
