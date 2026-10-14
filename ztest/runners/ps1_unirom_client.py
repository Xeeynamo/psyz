"""Serial client for UniROM 8. Features supported are:
* Uploads a PS-EXE binary
* Relays the console's TTY stdin/stdout
* Serves a PCDrv file driver."""

import os
import struct
import time

SLOW = 115200 # default baud rate
FAST = 518400 # fastest baud rate supported by UniROM 8.0.K
CHUNK = 2048  # buffered I/O
PING = b"\xaa"


class Error(Exception):
    pass


class Unirom:
    def __init__(self, device, root):
        import serial
        self.port = serial.Serial(device, SLOW, timeout=0.05)
        self.root = root
        self.files = {}
        self.devices = {}
        self.next_fd = 1
        self.buf = bytearray()

    def close(self):
        for f in self.files.values():
            f.close()
        self.files = {}
        self.port.close()

    def read(self, count, timeout=3.0):
        end = time.monotonic() + timeout
        while len(self.buf) < count:
            self.buf += self.port.read(count - len(self.buf))
            if time.monotonic() > end:
                raise Error("UniROM went silent: got %d of %d bytes"
                            % (len(self.buf), count))
        out = bytes(self.buf[:count])
        del self.buf[:count]
        return out

    def read32(self):
        return struct.unpack("<I", self.read(4))[0]

    def write32(self, value):
        self.port.write(struct.pack("<I", value & 0xFFFFFFFF))

    def expect(self, *tokens, timeout=3.0):
        """Reads until one of the tokens shows up, skipping the echo."""
        tail = b""
        end = time.monotonic() + timeout
        while True:
            if not self.buf:
                self.buf += self.port.read(1)
            if self.buf:
                tail = (tail + self.read(1))[-16:]
            for token in tokens:
                if tail.endswith(token):
                    return token
            if time.monotonic() > end:
                raise Error("UniROM did not answer %s"
                            % " or ".join(t.decode() for t in tokens))

    def drain(self, quiet=0.03):
        self.buf.clear()
        end = time.monotonic() + quiet
        while time.monotonic() < end:
            if self.port.read(256):
                end = time.monotonic() + quiet

    def set_baud(self, baud):
        self.port.baudrate = baud
        time.sleep(0.02) # wait for USB adapter to get the baud rate change

    def alive(self):
        self.drain()
        self.port.write(PING)
        return self.port.read(1) == PING

    def connect(self, timeout=5):
        """Finds the baud rate UniROM listens on"""
        end = time.monotonic() + timeout
        while True:
            for baud in (SLOW, FAST):
                self.set_baud(baud)
                if self.alive():
                    return
            if time.monotonic() > end:
                raise Error("UniROM does not answer: reset the console")

    def restart(self, timeout=20):
        """Reboots into a fresh UniROM, which always starts at the slow rate."""
        self.port.write(b"REST")
        self.port.flush()
        time.sleep(0.3)
        self.set_baud(SLOW)
        end = time.monotonic() + timeout
        while not self.alive():
            if time.monotonic() > end:
                raise Error("UniROM did not come back after a reset")

    def go_fast(self):
        self.port.write(b"FAST")
        self.port.flush()
        time.sleep(0.02)
        self.set_baud(FAST)
        if not self.alive():
            raise Error("UniROM does not answer at %d baud" % FAST)

    def upload(self, path):
        if self.port.baudrate != FAST:
            self.go_fast()
        if not self.debug():
            self.restart()
            self.go_fast()
            self.debug()
        self.send_exe(path)

    def handshake(self, command):
        self.port.write(command)
        self.expect(b"OKV2")
        self.port.write(b"UPV2")
        self.expect(b"OKAY")

    def debug(self):
        """Installs debug core, to serve PCDrv. Returns False if already installed."""
        self.port.write(b"DEBG")
        if self.expect(b"OKV2", b"OKAY") == b"OKAY":
            return False
        self.port.write(b"UPV2")
        self.expect(b"OKAY")
        end = time.monotonic() + 2
        while True:
            self.port.write(b"PING")
            try:
                self.expect(b"PONG", timeout=0.1)
                return True
            except Error:
                if time.monotonic() > end:
                    raise Error("the UniROM debug core does not answer")

    def send_data(self, data):
        """UniROM asks checksum for every chunk sent to verify data integrity"""
        for at in range(0, len(data), CHUNK):
            chunk = data[at:at + CHUNK]
            for _ in range(5):
                self.port.write(chunk)
                self.expect(b"CHEK", timeout=5)
                self.write32(sum(chunk))
                if self.expect(b"MORE", b"ERR!") == b"MORE":
                    break
            else:
                raise Error("UniROM keeps rejecting a chunk: check the cable")

    def recv_data(self, size):
        out = bytearray()
        while len(out) < size:
            out += self.read(min(CHUNK - len(out) % CHUNK, size - len(out)))
            if len(out) % CHUNK == 0:
                self.port.write(b"MORE")
        if self.read32() != sum(out) & 0xFFFFFFFF:
            raise Error("checksum mismatch on data sent by the console")
        return bytes(out)

    def send_exe(self, path):
        with open(path, "rb") as f:
            exe = f.read()
        exe += b"\0" * (-len(exe) % CHUNK)
        entry, load = struct.unpack_from("<I", exe, 0x10)[0], exe[0x18:0x1C]
        # The debug core calls the program on a stack that is not 8-byte
        # aligned, which breaks 64-bit varargs. This stub, placed after the
        # program, aligns it and makes a return from main reboot the console.
        stub = struct.pack(
            "<8I",
            0x3C1FBFC0,                   # lui   ra, 0xbfc0
            0x27BDFFE0,                   # addiu sp, sp, -32
            0x2408FFF8,                   # li    t0, -8
            0x03A8E824,                   # and   sp, sp, t0
            0x3C080000 | entry >> 16,     # lui   t0, entry >> 16
            0x35080000 | entry & 0xFFFF,  # ori   t0, t0, entry & 0xffff
            0x01000008,                   # jr    t0
            0)
        body = exe[CHUNK:] + stub
        start = struct.unpack("<I", load)[0] + len(body) - len(stub)
        self.handshake(b"SEXE")
        self.port.write(exe[:CHUNK])
        # Entry point, load address, size and byte sum of what follows.
        self.write32(start)
        self.port.write(load)
        self.write32(len(body))
        self.write32(sum(body))
        self.send_data(body)

    def reply(self, value=None):
        if value is None:
            self.port.write(b"NOPE")
        else:
            self.port.write(b"OKAY" + struct.pack("<I", value & 0xFFFFFFFF))

    def pcdrv(self):
        """Serves one PCDrv call. Every answer is OKAY and a value, or NOPE."""
        code = self.read32()
        self.port.write(b"OKAY")
        if code in (0x102, 0x103):
            name = b""
            while not name.endswith(b"\0"):
                name += self.read(1)
            mode = self.read32()
            name = name[:-1].decode().replace("\\", "/")
            path = os.path.join(self.root, name)
            try:
                if name in self.devices:
                    f = self.devices[name]()
                elif code == 0x102:
                    f = open(path, "w+b")
                else:
                    f = open(path, "r+b" if mode & 3 else "rb")
            except OSError:
                return self.reply()
            self.files[self.next_fd] = f
            self.next_fd += 1
            return self.reply(self.next_fd - 1)
        fd, arg, extra = self.read32(), self.read32(), self.read32()
        f = self.files.get(fd)
        if f is None or not 0x104 <= code <= 0x107:
            return self.reply()
        if code == 0x104:
            del self.files[fd]
            f.close()
            self.reply(0)
        elif code == 0x105:
            data = f.read(arg).ljust(arg, b"\0")
            self.reply(arg)
            self.write32(sum(data))
            self.send_data(data)
        elif code == 0x106:
            self.port.write(b"OKAY")
            f.write(self.recv_data(arg))
            f.flush()
            self.reply(arg)
        else:
            offset = struct.unpack("<i", struct.pack("<I", arg))[0]
            try:
                self.reply(f.seek(offset, extra))
            except (OSError, ValueError):
                self.reply()

    def console(self):
        """Yields TTY output. A PCDrv call arrives as a zero byte and 'p'."""
        while True:
            if not self.buf:
                self.buf += self.port.read(self.port.in_waiting or 1)
                yield b""
                continue
            at = self.buf.find(b"\0")
            if at:
                text = bytes(self.buf[:at] if at > 0 else self.buf)
                del self.buf[:len(text)]
                yield text
                continue
            try:
                marker = self.read(2, timeout=0.5)
            except Error:
                marker = self.read(len(self.buf))
            if marker == b"\0p":
                self.pcdrv()
            else:
                yield marker
