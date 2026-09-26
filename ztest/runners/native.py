import os

from .base import Driver, fail


class Native(Driver):
    """Runs the binary on this machine from --workdir."""

    def launch(self, args):
        if not os.path.isfile(self.opts.exe):
            fail("binary not found: %s" % self.opts.exe)
        return self.popen([self.opts.exe] + args, cwd=self.opts.workdir)


class Wine(Driver):
    """Runs a Windows build under wine."""

    target = "windows"

    def launch(self, args):
        wine = self.executable("wine")
        env = dict(os.environ, WINEDEBUG=os.environ.get("WINEDEBUG", "-all"))
        return self.popen([wine, self.opts.exe] + args, cwd=self.opts.workdir,
                          env=env)
