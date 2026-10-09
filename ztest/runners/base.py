import contextlib
import os
import queue
import select
import shutil
import subprocess
import sys
import threading


class Stream:
    """Bytes produced by a background reader; read() never blocks longer than
    the timeout. Returns None on timeout and b"" once the source is closed."""

    def __init__(self, source):
        self.q = queue.Queue()
        self.closed = False
        threading.Thread(target=self.pump, args=(source,), daemon=True).start()

    def pump(self, source):
        try:
            for chunk in source:
                if chunk:
                    self.q.put(chunk)
        finally:
            self.q.put(b"")

    def read(self, timeout):
        if self.closed:
            return b""
        try:
            data = self.q.get(timeout=timeout)
        except queue.Empty:
            return None
        if not data:
            self.closed = True
        return data


def read_fd(fd):
    """Yields raw chunks from a file descriptor until EOF or error."""
    while True:
        if sys.platform != "win32" and not select.select([fd], [], [], 0.5)[0]:
            continue
        try:
            data = os.read(fd, 4096)
        except OSError:
            return
        if not data:
            return
        yield data


def terminate(proc, timeout=10):
    if proc and proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()


def fail(msg):
    sys.exit("zrunner: " + msg)


def host_target():
    if sys.platform.startswith("win"):
        return "windows"
    if sys.platform == "darwin":
        return "macos"
    return "linux"


def find_gdb():
    """The first gdb in PATH that starts and knows ARM and MIPS."""
    for gdb in ("gdb-multiarch", "gdb"):
        if not shutil.which(gdb):
            continue
        r = subprocess.run([gdb, "-batch", "-ex", "set architecture arm"],
                           capture_output=True)
        if r.returncode == 0:
            return gdb
    fail("no working gdb with ARM and MIPS support found in PATH")


class Driver:
    target = host_target()

    def __init__(self, opts):
        self.opts = opts
        self.proc = None
        self.args_file = None
        self.clean = False

    def launch(self, args):
        raise NotImplementedError

    def executable(self, *names):
        """--emulator-path when given, otherwise the first of names in PATH."""
        path = self.opts.emulator_path
        if path:
            if not os.path.isfile(path):
                fail("--emulator-path %s not found" % path)
            return os.path.abspath(path)
        for name in names:
            # .exe files run through mono or wine and need no executable bit.
            mode = os.F_OK if name.endswith(".exe") else os.F_OK | os.X_OK
            found = shutil.which(name, mode=mode)
            if found:
                return found
        fail("%s not found in PATH (or pass --emulator-path)" % names[0])

    def stop(self):
        terminate(self.proc)
        if self.args_file:
            with contextlib.suppress(OSError):
                os.remove(self.args_file)

    def collect(self, session):
        pass

    def write_args_file(self, args, directory=None):
        """For targets without argv: ztest reads ztest.args at startup."""
        self.args_file = os.path.join(directory or self.opts.workdir,
                                      "ztest.args")
        with open(self.args_file, "w") as f:
            f.write(" ".join(args) + "\n")

    def popen(self, cmd, **kw):
        self.proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, **kw)
        return Stream(read_fd(self.proc.stdout.fileno()))
