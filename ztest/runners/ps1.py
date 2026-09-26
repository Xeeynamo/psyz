import contextlib
import os
import re
import select
import shutil
import signal
import subprocess
import time

from .base import Driver, Stream, fail, read_fd

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
        nops = [self.executable("nops.exe")]
        if nops[0].endswith(".exe"):
            if not shutil.which("mono"):
                fail("mono not found in PATH (needed to run nops.exe)")
            nops.insert(0, "mono")
        dest = self.opts.serial or "/dev/ttyUSB0"
        if not os.path.isfile(self.opts.exe):
            fail("PS-EXE not found: %s" % self.opts.exe)
        if not os.path.exists(dest):
            fail("serial device %s not found (pass --serial)" % dest)
        self.write_args_file(args)
        self.log = os.path.join(self.opts.workdir, "ztest.log")
        with contextlib.suppress(OSError):
            os.remove(self.log)
        # A fresh UniROM avoids faults left over from the previous program.
        subprocess.run(nops + ["/fast", "/reset", "/dest", dest],
                       cwd=self.opts.workdir, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, timeout=30)
        time.sleep(3)
        # mono block-buffers a pipe, so nops has to run on a pty.
        exe = os.path.relpath(self.opts.exe, self.opts.workdir)
        import pty
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.chdir(self.opts.workdir)
            os.execvp(nops[0], nops + ["/fast", "/exe", exe, "/m", "/dest",
                                       dest])
        return Stream(self.console())

    def console(self):
        """nops injects PCDrv traffic via TTY, so ztest writes its
        output to ztest.log through PCDrv instead; follow that file."""
        with contextlib.suppress(OSError):
            os.remove(self.log)
        offset = 0
        while True:
            if select.select([self.fd], [], [], 0.05)[0]:
                try:
                    if not os.read(self.fd, 4096):
                        return
                except OSError:
                    return
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
        with contextlib.suppress(ProcessLookupError):
            os.kill(self.pid, signal.SIGINT)
        time.sleep(0.5)
        with contextlib.suppress(ProcessLookupError):
            os.kill(self.pid, signal.SIGTERM)
        with contextlib.suppress(OSError):
            os.close(self.fd)
        with contextlib.suppress(ChildProcessError):
            os.waitpid(self.pid, 0)
        super().stop()
