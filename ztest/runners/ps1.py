import contextlib
import os
import re
import sys

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
    """Real hardware running UniROM: uploads the PS-EXE over serial, then
    streams the TTY and serves PCDrv from --workdir."""

    target = "ps1"

    def launch(self, args):
        from . import ps1_unirom_client as unirom
        try:
            import serial  # noqa: F401
        except ImportError:
            fail("pyserial not found (pip install pyserial)")
        self.dest = self.opts.serial or find_serial()
        if not os.path.isfile(self.opts.exe):
            fail("PS-EXE not found: %s" % self.opts.exe)
        if not os.path.exists(self.dest):
            fail("serial device %s not found (pass --serial)" % self.dest)
        self.write_args_file(args)
        low_latency(self.dest)
        self.errors = unirom.Error
        self.link = unirom.Unirom(self.dest, self.opts.workdir)
        try:
            self.link.connect()
            self.link.upload(self.opts.exe)
        except unirom.Error as e:
            fail("%s (%s)" % (e, self.dest))
        return Stream(self.console())

    def console(self):
        try:
            yield from self.link.console()
        except self.errors as e:
            yield ("\nzrunner: %s\n" % e).encode()
        except Exception:
            # stop() closes the port while this thread is still reading.
            return

    def stop(self):
        # Leaves a fresh UniROM behind, also when the program is still running.
        with contextlib.suppress(Exception):
            self.link.port.write(b"REST")
            self.link.port.flush()
        self.link.close()
        super().stop()


def find_serial():
    # USB vendors of the common UART adapters: FTDI, Silicon Labs, WCH, Prolific.
    UART_VENDORS = (0x0403, 0x10C4, 0x1A86, 0x067B)
    from serial.tools import list_ports
    ports = list(list_ports.comports())
    # "PS1 UART" is for custom Pico/Arduino/ESP32 devices
    found = ([p.device for p in ports if p.interface == "PS1 UART"]
             or [p.device for p in ports if p.vid in UART_VENDORS])
    if not found:
        fail("no PS1 serial adapter found (pass --serial)")
    if len(found) > 1:
        fail("several serial adapters found, pass --serial: %s"
             % ", ".join(sorted(found)))
    return found[0]


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
