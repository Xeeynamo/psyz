#!/usr/bin/env python3
"""Run the PsyZ test suite on the PSP, in PPSSPP or on real hardware.

    runner_psp.py emu [--timeout N]
    runner_psp.py hw  [--timeout N]

Both modes echo the suite's stdout as it arrives, stop at line PSYZ_TESTS_DONE.
"""

import argparse
import contextlib
import os
import pty
import re
import select
import shutil
import signal
import subprocess
import sys
import time

TESTS_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")

# --benchmark swaps both of these: the benchmark is a separate binary with its
# own completion line.
SENTINEL = "PSYZ_TESTS_DONE"
EXECUTABLE = "psyz_tests_psp"

out = []  # every chunk of suite output seen so far


def fail(msg):
    sys.exit("runner_psp.py: " + msg)


def emit(text, strip=False):
    if strip:  # hardware interleaves pspsh's own terminal control sequences
        text = ANSI.sub("", text).replace("\r", "")
    sys.stdout.write(text)
    sys.stdout.flush()
    out.append(text)


def done():
    return SENTINEL in "".join(out)


def clear_failure_artifacts():
    dst = os.path.join(TESTS_DIR, "expected")
    with contextlib.suppress(OSError):
        for name in os.listdir(dst):
            if name.endswith(".actual.png"):
                with contextlib.suppress(OSError):
                    os.remove(os.path.join(dst, name))


def run_emu(timeout):
    ppsspp = os.environ.get("PPSSPP", "PPSSPPHeadless")
    elf = os.path.join(TESTS_DIR, EXECUTABLE)
    if not os.path.isfile(elf):
        fail("ELF not found: %s" % elf)
    if not shutil.which(ppsspp):
        fail("%s not found in PATH (set PPSSPP)" % ppsspp)

    rel = os.path.join(os.path.basename(TESTS_DIR), EXECUTABLE)

    # -r: mount path to host0:/, same as PSPLINK
    # -l: enable full logs
    proc = subprocess.Popen(
        [ppsspp, "--timeout=%d" % timeout, "-l", "-r", ".", rel],
        cwd=os.path.dirname(TESTS_DIR), stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True, bufsize=1)
    deadline = time.time() + timeout + 30
    try:
        for line in proc.stdout:
            if line.startswith("I stdout: "):
                emit(line[len("I stdout: "):])
            if done() or time.time() > deadline:
                break
    finally:
        terminate(proc)


def run_hw(timeout):
    prx = os.path.join(TESTS_DIR, EXECUTABLE + ".prx")
    if not os.path.isfile(prx):
        fail("PRX not found: %s (configure with -DBUILD_PRX=ON)" % prx)
    elf = os.path.join(TESTS_DIR, EXECUTABLE)
    if os.path.isfile(elf) and os.path.getmtime(prx) < os.path.getmtime(elf):
        fail("Tests must be compiled with -DBUILD_PRX=ON")
    for tool in ("pspsh", "usbhostfs_pc"):
        if not shutil.which(tool):
            fail("%s not found in PATH" % tool)

    # host0:/ is whatever directory usbhostfs_pc runs from
    with usbhostfs():
        pspsh("host0:/" + EXECUTABLE + ".prx", timeout)


@contextlib.contextmanager
def usbhostfs():
    pids = subprocess.run(["pgrep", "-x", "usbhostfs_pc"],
                          capture_output=True, text=True).stdout.split()
    for pid in pids:
        try:
            cwd = os.path.realpath(os.readlink("/proc/%s/cwd" % pid))
        except OSError:
            cwd = "?"
        if cwd == TESTS_DIR:
            print("runner_psp.py: using running usbhostfs_pc (pid %s)" % pid)
            yield
            return
        fail("usbhostfs_pc is serving %s, but host0:/ must be %s.\n"
            % (cwd, TESTS_DIR))

    print("runner_psp.py: starting usbhostfs_pc in %s" % TESTS_DIR)
    proc = subprocess.Popen(["usbhostfs_pc"], cwd=TESTS_DIR,
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    time.sleep(2)  # let it claim the USB device before pspsh connects
    if proc.poll() is not None:
        fail("usbhostfs_pc exited immediately")
    try:
        yield
    finally:
        terminate(proc)


def terminate(proc):
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()


def pspsh(prx, timeout):
    """ldstart the PRX from a real pty, then reset PSPLINK.

    `pspsh -e` never connects the TTY output channel, and tty mode's readline
    callback misbehaves unless stdin is a terminal, so neither a pipe nor a
    heredoc can capture the console's output.
    """
    pid, fd = pty.fork()
    if pid == 0:
        os.execvp("pspsh", ["pspsh"])

    def pump(seconds):
        """Echo output for up to `seconds`; False once there is no more."""
        deadline = time.time() + seconds
        while time.time() < deadline:
            if not select.select([fd], [], [], 0.2)[0]:
                continue
            try:
                data = os.read(fd, 4096)
            except OSError:
                return False
            if not data:
                return False
            emit(data.decode("utf-8", "replace"), strip=True)
            if done():
                return False
        return True

    try:
        pump(3)
        os.write(fd, b"ldstart " + prx.encode() + b"\n")
        deadline = time.time() + timeout
        while time.time() < deadline and not done() and pump(2):
            pass
        pump(1)
    finally:
        with contextlib.suppress(OSError):
            os.write(fd, b"reset\n")
            time.sleep(2)
        os.kill(pid, signal.SIGINT)  # pspsh exits cleanly on either signal
        time.sleep(1)
        with contextlib.suppress(ProcessLookupError):
            os.kill(pid, signal.SIGTERM)
        os.close(fd)
        with contextlib.suppress(ChildProcessError):
            os.waitpid(pid, 0)


def report():
    text = "".join(out)
    if SENTINEL not in text:
        fail("no %s sentinel (timeout or crash)" % SENTINEL)

    if "failed=0" in [l for l in text.splitlines() if SENTINEL in l][-1]:
        return 0

    if SENTINEL == "PSYZ_BENCH_DONE":
        # The benchmark prints its own [FAIL] lines with the measured figures;
        # repeating them here would only obscure the numbers.
        return 1

    failed = {l.split()[-1] for l in text.splitlines() if "[  FAILED  ]" in l}
    for name in failed:
        print("runner_psp.py: failure: %s" % name, file=sys.stderr)
    if failed:
        return 1
    return 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", choices=["emu", "hw"])
    ap.add_argument("--timeout", type=int,
                    help="seconds to wait for the suite (default 300, "
                         "1200 with --benchmark)")
    ap.add_argument("--benchmark", action="store_true",
                    help="run the GPU benchmark instead of the test suite")
    ap.add_argument("--executable",
                    help="override the binary executable to load")
    args = ap.parse_args()

    global SENTINEL, EXECUTABLE
    if args.benchmark:
        SENTINEL, EXECUTABLE = "PSYZ_BENCH_DONE", "psyz_benchmark"
    if args.executable:
        EXECUTABLE = args.executable

    # The benchmark replays tens of thousands of primitives per measurement
    # and takes several minutes on hardware.
    default_timeout = 1200 if args.benchmark else 300

    clear_failure_artifacts()
    if args.mode == "emu":
        run_emu(args.timeout or default_timeout)
    else:
        run_hw(args.timeout or default_timeout)
    return report()


if __name__ == "__main__":
    sys.exit(main())
