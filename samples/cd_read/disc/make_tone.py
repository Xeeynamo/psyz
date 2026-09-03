#!/usr/bin/env python3
import math
import struct
import sys
import wave

RATE = 44100
SECONDS = 6
NOTES = (440.0, 660.0)


def main(path):
    frames = bytearray()
    for i in range(RATE * SECONDS):
        freq = NOTES[(i // (RATE // 2)) % len(NOTES)]
        v = int(12000 * math.sin(2.0 * math.pi * freq * i / RATE))
        frames += struct.pack("<hh", v, v)
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(bytes(frames))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "tone.wav")
