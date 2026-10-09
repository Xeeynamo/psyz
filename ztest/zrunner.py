#!/usr/bin/env python3
"""Run a ztest binary on a target and render its results on the host.

    zrunner.py <runner> --exe PATH [--workdir DIR] [options] [-- device args]

The device always prints the plain protocol; this script parses it, renders it
for the host terminal, collects files written by the device and returns 0 only
when the run finished with no failures.
"""

import argparse
import base64
import difflib
import glob
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from runners import DRIVERS  # noqa: E402

TEST_RE = re.compile(
    r"^([A-Za-z_][A-Za-z0-9_]*::[A-Za-z_][A-Za-z0-9_]*)(?: (PASS|FAIL|SKIP))?$")
RESULT_RE = re.compile(r"^(?:(.*) )?(PASS|FAIL|SKIP)$")
SUMMARY_RE = re.compile(
    r"^ztest: (\d+) passed, (\d+) failed, (\d+) skipped(?:, (\d+) not run)?$")
FILE_RE = re.compile(r"^@ztest-file (\S+) (\d+)$")

EMOJI = {"RUN": "⏳", "PASS": "\U0001f7e2", "FAIL": "❌",
         "SKIP": "\U0001f7e1"}
COLOR = {"RUN": "RUN ", "PASS": "\x1b[32mPASS\x1b[0m",
         "FAIL": "\x1b[31mFAIL\x1b[0m", "SKIP": "\x1b[33mSKIP\x1b[0m"}


def detect_mode():
    if not sys.stdout.isatty() or os.environ.get("NO_COLOR"):
        return "plain"
    term = os.environ.get("TERM", "")
    if os.name == "nt":
        wt = os.environ.get("WT_SESSION") or os.environ.get("TERM_PROGRAM")
        return "emoji" if wt else "color"
    if not term or term == "dumb":
        return "plain"
    if term == "linux":
        return "color"
    for var in ("LC_ALL", "LC_CTYPE", "LANG"):
        value = os.environ.get(var)
        if value:
            value = value.lower()
            return "emoji" if "utf-8" in value or "utf8" in value else "color"
    return "color"


class Renderer:
    def __init__(self, mode, verbose):
        self.mode = mode
        self.verbose = verbose
        self.running = None

    def write(self, text):
        sys.stdout.write(text)
        sys.stdout.flush()

    def label(self, state, name):
        if self.mode == "emoji":
            return "%s %s" % (EMOJI[state], name)
        return "%s %s" % (COLOR[state], name)

    def start(self, name):
        if self.mode != "plain":
            self.write(self.label("RUN", name))
            self.running = name

    def finish(self, name, result):
        if self.mode == "plain":
            self.write("%s %s\n" % (name, result))
        elif self.running == name:
            self.write("\r\x1b[K%s\n" % self.label(result, name))
        else:
            self.write("%s\n" % self.label(result, name))
        self.running = None

    def line(self, text):
        if self.running:
            self.write("\r\x1b[K%s\n%s" % (text, self.label("RUN",
                                                            self.running)))
        else:
            self.write(text + "\n")

    def noise(self, text):
        if self.verbose:
            self.line(text)


class Session:
    """Parses the plain protocol out of a byte stream full of other output."""

    def __init__(self, renderer, workdir):
        self.r = renderer
        self.workdir = workdir
        self.partial = ""
        self.pending = None
        self.last = None
        self.results = []
        self.transcript = []
        self.summary = None
        self.file = None
        self.written = []

    def feed(self, data):
        text = self.partial + data.decode("utf-8", "replace")
        lines = text.split("\n")
        self.partial = lines.pop()
        for line in lines:
            self.on_line(line.rstrip("\r"))
        if self.file is None and self.partial:
            m = TEST_RE.match(self.partial.rstrip("\r"))
            if m and not m.group(2) and self.pending != m.group(1):
                self.begin(m.group(1))

    def begin(self, name):
        self.pending = name
        self.r.start(name)

    def end(self, name, result):
        if self.pending != name:
            self.r.running = None
        self.pending = None
        self.last = name
        self.results.append((name, result))
        self.transcript.append("%s %s" % (name, result))
        self.r.finish(name, result)

    def on_line(self, line):
        if self.file is not None:
            if line == "@ztest-end":
                self.save_file()
            else:
                self.file[2].append(line.strip())
            return
        at = line.find("@ztest-file ")
        if at > 0:  # a file block written while the test line was still open
            self.on_line(line[:at])
            line = line[at:]
        m = FILE_RE.match(line)
        if m:
            self.file = (m.group(1), int(m.group(2)), [])
            return
        m = SUMMARY_RE.match(line)
        if m:
            self.summary = tuple(int(g or 0) for g in m.groups())
            self.transcript.append(line)
            return
        m = TEST_RE.match(line)
        if m:
            if m.group(2):
                self.end(m.group(1), m.group(2))
            else:
                self.begin(m.group(1))
            return
        if self.pending:
            if line.startswith(self.pending):
                line = line[len(self.pending):]
            m = RESULT_RE.match(line)
            if m:
                if (m.group(1) or "").strip():
                    self.r.noise(m.group(1))
                self.end(self.pending, m.group(2))
                return
        if line.startswith("   ") and self.last and not self.pending:
            self.transcript.append(line)
            self.r.line(line)
            return
        self.r.noise(line)

    def save_file(self):
        path, size, chunks = self.file
        self.file = None
        data = base64.b64decode("".join(chunks))
        if len(data) != size:
            self.r.line("zrunner: %s: got %d bytes, expected %d"
                        % (path, len(data), size))
            return
        dst = os.path.join(self.workdir, path)
        os.makedirs(os.path.dirname(dst) or ".", exist_ok=True)
        with open(dst, "wb") as f:
            f.write(data)
        self.written.append(path)

    def done(self):
        return self.summary is not None


def clear_actuals(workdir):
    for path in glob.glob(os.path.join(workdir, "expected", "**",
                                       "*.actual.png"), recursive=True):
        os.remove(path)


def check_golden(session, golden, target):
    with open(golden) as f:
        want = f.read().replace("{TARGET}", target).splitlines()
    got = session.transcript
    if got == want:
        print("zrunner: self-test transcript matches %s" % golden)
        return 0
    sys.stdout.writelines(
        l + "\n" for l in difflib.unified_diff(want, got, "expected", "actual",
                                               lineterm=""))
    print("zrunner: self-test transcript differs from %s" % golden)
    return 1


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("runner", choices=sorted(DRIVERS))
    ap.add_argument("--exe", required=True, help="binary, PRX or ROM")
    ap.add_argument("--workdir", default=".",
                    help="directory holding expected/ (default: cwd)")
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--verbose", action="store_true",
                    help="show passing test logs and all device output")
    ap.add_argument("--filter", help="comma separated globs, -glob excludes")
    ap.add_argument("--output", choices=["auto", "emoji", "color", "plain"],
                    default="auto")
    ap.add_argument("--selftest", metavar="GOLDEN",
                    help="compare the transcript against GOLDEN")
    ap.add_argument("--serial", help="adb serial, or the PS1 serial port")
    ap.add_argument("--emulator-path",
                    help="emulator or uploader to use instead of PATH's")
    ap.add_argument("--bios", help="PS1 BIOS image for pcsx-redux")
    ap.add_argument("--gdb-port", type=int,
                    help="emulator GDB stub port (melonDS 3333, ares 9123)")
    ap.add_argument("--headless", action="store_true",
                    help="run ares in a headless compositor")
    ap.add_argument("--avd", help="Android virtual device to boot")
    ap.add_argument("--target", help="override the target name")
    argv = sys.argv[1:]
    device_args = argv[argv.index("--") + 1:] if "--" in argv else []
    opts = ap.parse_args(argv[:argv.index("--")] if "--" in argv else argv)
    opts.workdir = os.path.abspath(opts.workdir)
    opts.exe = os.path.abspath(opts.exe)

    args = ["--output=plain"]
    if opts.verbose or opts.selftest:
        args.append("--verbose")
    if opts.filter:
        args.append("--filter=" + opts.filter)
    args += device_args

    driver = DRIVERS[opts.runner](opts)
    target = opts.target or driver.target
    mode = opts.output if opts.output != "auto" else detect_mode()
    session = Session(Renderer(mode, opts.verbose), opts.workdir)

    clear_actuals(opts.workdir)
    deadline = time.time() + opts.timeout
    timed_out = False
    stream = driver.launch(args)
    try:
        while not session.done():
            if time.time() > deadline:
                timed_out = True
                break
            data = stream.read(0.25)
            if data is None:
                continue
            if not data:
                break
            session.feed(data)
    finally:
        driver.clean = session.done()
        driver.stop()
    driver.collect(session)

    if session.partial:
        session.on_line(session.partial)
    if timed_out:
        where = " while running " + session.pending if session.pending else ""
        print("\nzrunner: timed out after %ds%s" % (opts.timeout, where))
        return 2
    if not session.done():
        where = " during " + session.pending if session.pending else ""
        print("\nzrunner: the device stopped without a summary%s" % where)
        return 2
    if opts.selftest:
        return check_golden(session, opts.selftest, target)
    passed, failed, skipped, not_run = session.summary
    if mode != "plain" and failed:
        print()
        for name, result in session.results:
            if result == "FAIL":
                print(session.r.label("FAIL", name))
    line = "ztest: %d passed, %d failed, %d skipped" % (passed, failed,
                                                         skipped)
    if not_run:
        line += ", %d not run" % not_run
    print(line)
    return 1 if failed or not_run else 0


if __name__ == "__main__":
    sys.exit(main())
