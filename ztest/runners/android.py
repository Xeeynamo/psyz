import os
import shlex
import shutil
import subprocess
import time

from .base import Driver, fail

REMOTE = "/data/local/tmp/ztest"


def adb_devices():
    out = subprocess.run(["adb", "devices"], capture_output=True,
                         text=True).stdout
    return [l.split()[0] for l in out.splitlines()[1:]
            if l.strip().endswith("device")]


class Android(Driver):
    """A native executable pushed to /data/local/tmp and run over adb shell,
    on a phone or an emulator alike. The device is --serial, $ANDROID_SERIAL
    or the first one adb lists; with none online, the AVD from --avd (default:
    the first one listed) is booted headless and shut down afterwards. --exe
    may contain {abi}, replaced with the device's primary ABI."""

    target = "android"

    def adb(self, *args, **kw):
        return subprocess.run(["adb", "-s", self.serial] + list(args), **kw)

    def pick_device(self):
        serial = self.opts.serial or os.environ.get("ANDROID_SERIAL")
        if serial:
            return serial
        devices = adb_devices()
        return devices[0] if devices else None

    def launch(self, args):
        if not shutil.which("adb"):
            fail("adb not found in PATH")
        self.booted = None
        self.serial = self.pick_device() or self.boot()
        abi = self.adb("shell", "getprop", "ro.product.cpu.abi",
                       capture_output=True, text=True).stdout.strip()
        exe = self.opts.exe.replace("{abi}", abi)
        if not os.path.isfile(exe):
            fail("binary not found for %s: %s" % (abi, exe))
        quiet = dict(stdout=subprocess.DEVNULL, check=True)
        self.adb("shell", "rm -rf %s && mkdir -p %s" % (REMOTE, REMOTE),
                 **quiet)
        self.adb("push", exe, REMOTE + "/", **quiet)
        expected = os.path.join(self.opts.workdir, "expected")
        if os.path.isdir(expected):
            self.adb("push", expected, REMOTE + "/", **quiet)
        name = os.path.basename(exe)
        cmd = "cd %s && chmod 755 %s && ./%s %s" % (
            REMOTE, name, name, " ".join(shlex.quote(a) for a in args))
        return self.popen(["adb", "-s", self.serial, "shell", cmd])

    def collect(self, session):
        out = self.adb("shell", "ls %s/expected/ 2>/dev/null" % REMOTE,
                       capture_output=True, text=True).stdout.split()
        for name in out:
            if name.endswith(".actual.png"):
                self.adb("pull", "%s/expected/%s" % (REMOTE, name),
                         os.path.join(self.opts.workdir, "expected", name),
                         stdout=subprocess.DEVNULL)

    def stop(self):
        super().stop()
        if self.booted:
            subprocess.run(["adb", "-s", self.serial, "emu", "kill"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


    def boot(self):
        emulator = self.executable("emulator")
        avd = self.opts.avd or next(iter(subprocess.run(
            [emulator, "-list-avds"], capture_output=True,
            text=True).stdout.split()), None)
        if not avd:
            fail("no Android virtual device found (pass --avd)")
        before = set(adb_devices())
        self.booted = subprocess.Popen(
            [emulator, "-avd", avd, "-no-window", "-no-audio",
             "-no-snapshot-save", "-no-boot-anim"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        deadline = time.time() + 300
        while time.time() < deadline:
            new = [d for d in adb_devices() if d not in before
                   and d.startswith("emulator-")]
            if new:
                self.serial = new[0]
                done = self.adb("shell", "getprop", "sys.boot_completed",
                                capture_output=True, text=True).stdout.strip()
                if done == "1":
                    return self.serial
            time.sleep(2)
        fail("emulator %s did not boot within 300s" % avd)

