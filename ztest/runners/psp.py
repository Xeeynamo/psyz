import contextlib
import os
import re
import select
import shutil
import signal
import subprocess
import time

from .base import Driver, Stream, fail, read_fd, terminate

ANSI = re.compile(rb"\x1b\[[0-9;?]*[A-Za-z]")
PSPSH_PROMPT = re.compile(rb"(^|\n)[a-z]+[0-9]*:/[^>\n]*> ")
PPSSPP_LOG = re.compile(rb"^[DIWENV] ")
PPSSPP_WRITE = re.compile(rb"^D (\d+)=sceIoWrite\((\d), ")


class PspEmu(Driver):
    """PPSSPPHeadless with --workdir mounted as host0:/."""

    target = "psp"

    def launch(self, args):
        ppsspp = self.executable("PPSSPPHeadless")
        if not os.path.isfile(self.opts.exe):
            fail("ELF not found: %s" % self.opts.exe)
        # A loose ELF gets no argv, so ztest reads host0:/ztest.args instead.
        self.write_args_file(args)
        # PPSSPP only boots an ELF located inside the -r directory.
        elf = self.opts.exe
        self.copy = None
        if os.path.relpath(elf, self.opts.workdir).startswith(".."):
            self.copy = elf = os.path.join(self.opts.workdir, ".zrunner.elf")
            shutil.copyfile(self.opts.exe, elf)
        cmd = [ppsspp, "--timeout=%d" % self.opts.timeout, "-l", "-r",
               self.opts.workdir, elf]
        self.proc = subprocess.Popen(cmd, cwd=self.opts.workdir,
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT)
        return Stream(self.records(read_fd(self.proc.stdout.fileno())))

    @staticmethod
    def records(chunks):
        """Rebuilds the byte stream from PPSSPP's log.

        Every sceIoWrite on fd 1 or 2 is logged as `I stdout: ` (or stderr)
        followed by the text with its trailing newline removed, any further
        lines raw, then a `D N=sceIoWrite(fd, ...)` line carrying the length.
        """
        buf = b""
        record = None
        for chunk in chunks:
            buf += chunk
            *lines, buf = buf.split(b"\n")
            for line in lines:
                if line.startswith((b"I stdout: ", b"I stderr: ")):
                    record = line[len(b"I stdout: "):]
                    continue
                m = PPSSPP_WRITE.match(line)
                if m and record is not None:
                    if len(record) < int(m.group(1)):
                        record += b"\n"
                    yield record
                    record = None
                elif record is not None and not PPSSPP_LOG.match(line):
                    record += b"\n" + line

    def stop(self):
        super().stop()
        if self.copy:
            with contextlib.suppress(OSError):
                os.remove(self.copy)


class PspHw(Driver):
    """Real hardware through PSPLINK: usbhostfs_pc serves --workdir as host0:/
    (and the PRX directory as host1:/ when it lives elsewhere), and pspsh runs
    ldstart on the PRX from a real pty, passing the arguments along."""

    target = "psp"

    def launch(self, args):
        prx = self.opts.exe
        if not prx.endswith(".prx"):
            prx += ".prx"
        if not os.path.isfile(prx):
            fail("PRX not found: %s (build with -DBUILD_PRX=ON)" % prx)
        for tool in ("pspsh", "usbhostfs_pc"):
            if not shutil.which(tool):
                fail("%s not found in PATH" % tool)
        roots = [self.opts.workdir]
        rel = os.path.relpath(prx, self.opts.workdir)
        if rel.startswith(".."):
            roots.append(os.path.dirname(prx))
            path = "host1:/" + os.path.basename(prx)
        else:
            path = "host0:/" + rel
        self.usbhostfs = None
        self.start_usbhostfs(roots)
        import pty
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execvp("pspsh", ["pspsh"])
        cmd = " ".join(["ldstart", path] + args).encode() + b"\n"
        return Stream(self.console(cmd))

    @staticmethod
    def serving(pid):
        """Root directories of a running usbhostfs_pc."""
        try:
            with open("/proc/%s/cmdline" % pid, "rb") as f:
                argv = f.read().decode().split("\0")[1:]
            cwd = os.readlink("/proc/%s/cwd" % pid)
        except OSError:
            return []
        roots = [a for a in argv if a and not a.startswith("-")]
        return [os.path.realpath(os.path.join(cwd, r)) for r in roots or [cwd]]

    def start_usbhostfs(self, roots):
        want = [os.path.realpath(r) for r in roots]
        pids = subprocess.run(["pgrep", "-x", "usbhostfs_pc"],
                              capture_output=True, text=True).stdout.split()
        for pid in pids:
            have = self.serving(pid)
            if have[:len(want)] == want:
                return
            fail("usbhostfs_pc (pid %s) serves %s, but this run needs %s"
                 % (pid, have, want))
        self.usbhostfs = subprocess.Popen(["usbhostfs_pc"] + want,
                                          stdout=subprocess.DEVNULL,
                                          stderr=subprocess.DEVNULL)
        time.sleep(2)  # let it claim the USB device before pspsh connects
        if self.usbhostfs.poll() is not None:
            fail("usbhostfs_pc exited immediately")

    def console(self, cmd):
        """`pspsh -e` never connects the TTY channel, so drive a real pty."""
        deadline = time.time() + 3
        while time.time() < deadline:
            if select.select([self.fd], [], [], 0.2)[0]:
                with contextlib.suppress(OSError):
                    os.read(self.fd, 4096)
        os.write(self.fd, cmd)
        at_line_start = True
        for data in read_fd(self.fd):
            data = ANSI.sub(b"", data).replace(b"\r", b"")
            if at_line_start:
                data = b"\n" + data
            data = PSPSH_PROMPT.sub(rb"\1", data)
            if at_line_start:
                data = data[1:]
            at_line_start = data.endswith(b"\n")
            yield data

    def stop(self):
        with contextlib.suppress(OSError):
            os.write(self.fd, b"reset\n")
            time.sleep(2)
        with contextlib.suppress(ProcessLookupError):
            os.kill(self.pid, signal.SIGINT)
        time.sleep(1)
        with contextlib.suppress(ProcessLookupError):
            os.kill(self.pid, signal.SIGTERM)
        with contextlib.suppress(OSError):
            os.close(self.fd)
        with contextlib.suppress(ChildProcessError):
            os.waitpid(self.pid, 0)
        terminate(self.usbhostfs)
        super().stop()
