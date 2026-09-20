"""Create original quiet interface sounds; no downloaded samples or speech."""
import math
import random
import struct
import wave
from pathlib import Path


def create(name, duration, volume, tonal=False):
    rate = 44100
    rng = random.Random(711)
    previous = 0.0
    samples = []
    for i in range(int(rate * duration)):
        t = i / rate
        previous = 0.72 * previous + 0.28 * rng.uniform(-1, 1)
        envelope = math.sin(math.pi * min(1, t / duration)) ** 2
        value = previous * 0.6
        if tonal:
            value += math.sin(2 * math.pi * 440 * t) * 0.15 * math.exp(-t * 25)
        samples.append(struct.pack('<h', int(max(-1, min(1, value * envelope * volume)) * 32767)))
    destination = Path(__file__).resolve().parents[1] / 'ArtSource/UI/Glass' / name
    destination.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(destination), 'wb') as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(rate)
        output.writeframes(b''.join(samples))
    print(destination)


if __name__ == '__main__':
    create('S_UIHover.wav', 0.09, 0.16)
    create('S_UIPress.wav', 0.13, 0.22, tonal=True)
