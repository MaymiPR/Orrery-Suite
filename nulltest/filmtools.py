"""Frames of a RefCapture film (out.raw: per frame a float64 timestamp, then width * height * 3 bytes RGB)."""
import numpy as np


class Film:
    def __init__(self, path):
        self.w, self.h = map(int, open(path + '.txt').read().split())
        self.fsz = 8 + self.w * self.h * 3
        self.mm = np.memmap(path, dtype=np.uint8, mode='r')
        self.n = len(self.mm) // self.fsz
        self.t = np.array([np.frombuffer(self.mm[k * self.fsz:k * self.fsz + 8].tobytes(), np.float64)[0] for k in range(self.n)])

    def frame(self, i):
        return np.asarray(self.mm[i * self.fsz + 8:(i + 1) * self.fsz]).reshape(self.h, self.w, 3)

    def at(self, secs):
        return self.frame(int(np.argmin(np.abs(self.t - secs))))
