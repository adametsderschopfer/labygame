"""Create original LABY menu audio from deterministic synthesis.

No downloaded samples, speech, generative service, or third-party material is used.
"""

import math
import random
import struct
import wave
from pathlib import Path


RATE = 48_000
DESTINATION = Path(__file__).resolve().parents[1] / "ArtSource/UI/Glass"


def write_wave(name, duration, sample, channels=1):
    frames = bytearray()
    frame_count = int(RATE * duration)

    for index in range(frame_count):
        time = index / RATE
        values = sample(time, index)

        if channels == 1:
            values = (values,)

        for value in values:
            frames.extend(struct.pack("<h", int(max(-1.0, min(1.0, value)) * 32767)))

    destination = DESTINATION / name
    destination.parent.mkdir(parents=True, exist_ok=True)

    with wave.open(str(destination), "wb") as output:
        output.setnchannels(channels)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(frames)

    print(destination)


def create_hover():
    duration = 0.14
    rng = random.Random(711)
    filtered = 0.0

    def sample(time, _):
        nonlocal filtered
        filtered = filtered * 0.93 + rng.uniform(-1.0, 1.0) * 0.07
        attack = min(1.0, time / 0.018)
        release = max(0.0, 1.0 - time / duration) ** 2.4
        air = filtered * 0.055
        tone = math.sin(2.0 * math.pi * (690.0 - 90.0 * time / duration) * time) * 0.012
        return (air + tone) * attack * release

    write_wave("S_UIHover.wav", duration, sample)


def create_press():
    duration = 0.20
    rng = random.Random(173)
    filtered = 0.0

    def sample(time, _):
        nonlocal filtered
        filtered = filtered * 0.88 + rng.uniform(-1.0, 1.0) * 0.12
        attack = min(1.0, time / 0.012)
        release = max(0.0, 1.0 - time / duration) ** 2.2
        progress = time / duration
        frequency = 460.0 - 105.0 * progress
        tone = math.sin(2.0 * math.pi * frequency * time) * 0.034
        overtone = math.sin(2.0 * math.pi * frequency * 1.51 * time + 0.6) * 0.009
        return (tone + overtone + filtered * 0.018) * attack * release

    write_wave("S_UIPress.wav", duration, sample)


def create_ambient():
    duration = 48.0
    tau = 2.0 * math.pi
    haze_cycles = (43, 61, 79, 103, 127, 149, 181, 223)
    haze_phases = (0.2, 1.4, 2.6, 3.7, 4.3, 5.1, 0.9, 2.1)

    def channel(time, side):
        cycle = time / duration
        drift = math.sin(tau * cycle * 2.0 + side * 0.7)
        low = math.sin(tau * 55.0 * time + drift * 0.18) * 0.020
        low += math.sin(tau * 82.5 * time + math.sin(tau * cycle) * 0.11 + side * 0.31) * 0.012
        low += math.sin(tau * 110.0 * time + side * 0.57) * 0.006
        haze = 0.0

        for index, cycles in enumerate(haze_cycles):
            phase = haze_phases[index] + side * (0.22 + index * 0.035)
            haze += math.sin(tau * cycles * cycle + phase)

        haze *= 0.0022 * (0.72 + 0.28 * math.sin(tau * cycle * 3.0 + side))
        pulse_envelope = max(0.0, math.sin(tau * cycle * 3.0 - 1.1)) ** 14
        distant = math.sin(tau * 165.0 * time + side * 0.8) * pulse_envelope * 0.008
        return low + haze + distant

    def sample(time, _):
        return channel(time, -1.0), channel(time, 1.0)

    write_wave("S_MenuAmbient.wav", duration, sample, channels=2)


if __name__ == "__main__":
    create_hover()
    create_press()
    create_ambient()
