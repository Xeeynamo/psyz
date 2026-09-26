import contextlib
import os
import re
import signal
import subprocess
import tempfile
import threading
import time

from .base import Driver, Stream, fail, find_gdb, read_fd, terminate

GDB_NOISE = re.compile(rb"\x1b\[[0-9;?]*[A-Za-z]")


class NdsEmu(Driver):
    """melonDS with its ARM9 GDB stub (enabled, JIT off, --gdb-port or 3333).
    gdb breaks on ztest_nds_log to print the output and on ztest_nds_dump to
    write files to --workdir, then releases the ROM's startup wait."""

    target = "nds"

    def launch(self, args):
        melonds = self.executable("melonDS")
        gdb = find_gdb()
        port = self.opts.gdb_port or 3333
        elf = os.path.splitext(self.opts.exe)[0] + ".elf"
        for path, what in ((self.opts.exe, "ROM"), (elf, "ELF")):
            if not os.path.isfile(path):
                fail("%s not found: %s" % (what, path))
        self.gdb_pid = None
        env = dict(os.environ)
        env.setdefault("QT_QPA_PLATFORM", "offscreen")
        self.proc = subprocess.Popen([melonds, self.opts.exe],
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, env=env)
        self.wait_for_stub()
        self.script = self.gdb_script(port, args)
        import pty
        self.gdb_pid, self.fd = pty.fork()
        if self.gdb_pid == 0:
            os.execvp(gdb, [gdb, "-q", "-batch", "-iex", "set auto-load off",
                            "-x", self.script, elf])
        return Stream(self.console())

    def wait_for_stub(self):
        ready = threading.Event()
        log = []

        def pump():
            for chunk in read_fd(self.proc.stdout.fileno()):
                log.append(chunk)
                if b"GDB stub for core 9" in b"".join(log):
                    ready.set()

        threading.Thread(target=pump, daemon=True).start()
        if not ready.wait(20):
            terminate(self.proc)
            fail("melonDS GDB stub did not start; enable it for ARM9 and "
                 "disable the JIT in the melonDS settings\n"
                 + b"".join(log).decode("utf-8", "replace"))
        time.sleep(0.5)

    def gdb_script(self, port, args):
        text = " ".join(args).encode()[:252]
        text += b"\0" * (4 - len(text) % 4)
        lines = ["set pagination off", "set confirm off",
                 "set width 0", "target remote :%d" % port]
        # melonDS's stub drops the connection on byte writes; use words.
        lines += ["set var ((unsigned int*)ztest_nds_args)[%d] = %d"
                  % (i // 4, int.from_bytes(text[i:i + 4], "little"))
                  for i in range(0, len(text), 4)]
        lines += [
            "set var ztest_nds_wait = 0",
            "break ztest_nds_log", "commands", "silent",
            'printf "%s", (char*)$r0', "continue", "end",
            "break ztest_nds_dump", "commands", "silent",
            'eval "dump binary memory %s/%%s 0x%%x 0x%%x", (char*)$r0, $r1, '
            "$r1 + $r2" % self.opts.workdir,
            "continue", "end", "continue"]
        fd, path = tempfile.mkstemp(suffix=".gdb")
        with os.fdopen(fd, "w") as f:
            f.write("\n".join(lines) + "\n")
        return path

    def console(self):
        for data in read_fd(self.fd):
            yield GDB_NOISE.sub(b"", data).replace(b"\r", b"")

    def stop(self):
        if self.gdb_pid:
            with contextlib.suppress(ProcessLookupError):
                os.kill(self.gdb_pid, signal.SIGKILL)
            with contextlib.suppress(OSError):
                os.close(self.fd)
            with contextlib.suppress(ChildProcessError):
                os.waitpid(self.gdb_pid, 0)
            with contextlib.suppress(OSError):
                os.remove(self.script)
        super().stop()
