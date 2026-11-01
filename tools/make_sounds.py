"""Generates the toggle on / toggle off sound candidates used to design Hollow Mic (7 characters x on/off).
The shipped sounds are variant 5 "hollow" (copied to assets/sounds/hollow_on.wav and hollow_off.wav).
Usage: pip install numpy scipy ; python tools/make_sounds.py   ->  writes ./out/*.wav and out/audition.wav"""
import numpy as np, wave
from scipy import signal
SR = 44100
rng = np.random.default_rng(5)

def T(d): return np.arange(int(d * SR)) / SR
def lp(x, fc, o=2): b, a = signal.butter(o, fc / (SR / 2)); return signal.lfilter(b, a, x)
def n(name):                                                     # note name -> Hz  (A4 = 440)
    names = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
    s = names[name[0]] + (1 if "#" in name else 0); o = int(name[-1])
    return 440.0 * 2 ** ((s + 12 * (o - 4) - 9) / 12)

def attack(t, a): return np.where(t < a, 0.5 - 0.5 * np.cos(np.pi * np.minimum(t, a) / a), 1.0)

def release(t, dur):                       # raised-cosine release over the last 45% of the note (max 60 ms)
    r = min(0.060, dur * 0.45)
    u = np.clip((dur - t) / r, 0, 1)
    return 0.5 - 0.5 * np.cos(np.pi * u)

def tone(f, dur, atk=0.006, tau=0.08, harm=((1, 1.0), (2, 0.12), (3, 0.04)), glide=0.0, gtau=0.02, det=(0.0,)):
    t = T(dur); y = np.zeros_like(t)
    ff = f * (1 + glide * np.exp(-t / gtau))
    for d in det:
        ph = 2 * np.pi * np.cumsum(ff * (1 + d)) / SR
        for k, a in harm: y += a * np.sin(k * ph)
    return y / len(det) * attack(t, atk) * np.exp(-t / tau) * release(t, dur)

def fm_bell(f, dur, ratio=3.5, index=2.2, itau=0.05, atk=0.003, tau=0.10):
    t = T(dur); I = index * np.exp(-t / itau)
    y = np.sin(2 * np.pi * f * t + I * np.sin(2 * np.pi * f * ratio * t))
    return y * attack(t, atk) * np.exp(-t / tau) * release(t, dur)

def partials(f, dur, parts, atk=0.002):
    t = T(dur); y = np.zeros_like(t)
    for r, a, tau in parts: y += a * np.sin(2 * np.pi * f * r * t) * np.exp(-t / tau)
    return y * attack(t, atk) * release(t, len(t) / SR)

def place(*notes):
    L = max(int(s * SR) + len(x) for s, x in notes); out = np.zeros(L)
    for s, x in notes: out[int(s * SR):int(s * SR) + len(x)] += x
    return out

def echo(x, taps, damp=1400):
    b, a = signal.butter(1, damp / (SR / 2)); L = len(x) + int(max(d for d, _ in taps) * SR) + 10
    y = np.zeros(L); y[:len(x)] += x
    for d, g in taps: y[int(d * SR):int(d * SR) + len(x)] += g * lp(x, damp, 1)
    return y

def room(x, rt=0.22, wet=0.20, pre=0.006, damp=1800):
    t = T(rt); ir = rng.standard_normal(len(t)) * np.exp(-t / (rt / 5)); ir = lp(ir, damp, 3)
    ir = np.r_[np.zeros(int(pre * SR)), ir]; ir /= np.sqrt(np.sum(ir ** 2))
    y = signal.fftconvolve(x, ir); return np.r_[x, np.zeros(len(y) - len(x))] * (1 - 0.3 * wet) + y * wet

def pair(notes, on):   # on: rising order, off: falling order
    return notes if on else notes[::-1]

# ----------------------------------------------------------------- the 7 characters
def v1_musicbox(on):       # music-box: soft FM bell, perfect fifth
    a, b = pair((n("E5"), n("B5")), on)
    x = place((0, fm_bell(a, .26, 2.76, 1.6, .035, tau=.09)), (.075, fm_bell(b, .30, 2.76, 1.6, .035, tau=.11) * 1.1))
    return room(x, .25, .18)

def v2_kalimba(on):        # kalimba: plucked tines, major third + fourth
    a, b = pair((n("C5"), n("G5")), on)
    P = ((1, 1.0, .10), (5.4, .22, .025), (9.2, .06, .012))
    x = place((0, partials(a, .24, P)), (.065, partials(b, .28, P) * 1.1))
    return echo(x, ((.11, .16),), 1300)

def v3_droplet(on):        # water-drop bloop: one note with an exponential pitch bend
    t = T(.22); f = (n("A4") * (0.62 + 0.62 * (1 - np.exp(-t / .045)))) if on else (n("A4") * (1.30 - 0.52 * (1 - np.exp(-t / .045))))
    ph = 2 * np.pi * np.cumsum(f) / SR
    x = (np.sin(ph) + 0.10 * np.sin(2 * ph)) * attack(t, .004) * np.exp(-t / .07) * release(t, .22)
    return room(x, .20, .18)

def v4_glass(on):          # glass: FM, higher, airy (fourth)
    a, b = pair((n("A5"), n("D6")), on)
    x = place((0, fm_bell(a, .22, 2.0, 1.1, .03, tau=.07)), (.06, fm_bell(b, .30, 2.0, 1.1, .03, tau=.10)))
    return echo(x, ((.10, .18), (.20, .07)), 2400)

def v5_hollow(on):         # 'Hollow': tuned wooden marimba, low and round, with an empty-room tail
    a, b = pair((n("A4"), n("E5")), on)
    P = ((1, 1.0, .12), (4.0, .34, .035), (10.0, .07, .012))
    x = place((0, partials(a, .30, P)), (.085, partials(b, .34, P) * 1.15))
    return room(x, .30, .26, damp=1500)

def v6_phantom(on):        # 'Phantom': warm, slightly detuned pad blip, ghostly echo
    a, b = pair((n("D4"), n("A4")), on)
    H = ((1, 1.0), (2, .42), (3, .20), (4, .08))
    x = place((0, tone(a, .24, atk=.014, tau=.10, harm=H, det=(-.004, 0, .004))),
              (.09, tone(b, .30, atk=.014, tau=.12, harm=H, det=(-.004, 0, .004)) * 1.1))
    return echo(x, ((.13, .24), (.26, .10)), 1000)

def v7_chatpop(on):        # chat-app style: two quick rounded blips with a little pitch settle
    a, b = pair((n("E5"), n("A5")), on)
    g = -0.05 if on else 0.05
    x = place((0, tone(a, .13, atk=.004, tau=.05, glide=g, gtau=.02)),
              (.07, tone(b, .20, atk=.004, tau=.07, glide=g, gtau=.02) * 1.1))
    return echo(x, ((.09, .14),), 1800)

VARIANTS = [("1-musicbox", v1_musicbox, "FM music-box bell, perfect fifth, soft room"),
            ("2-kalimba", v2_kalimba, "plucked kalimba tines, fifth/octave-ish, dark echo"),
            ("3-droplet", v3_droplet, "single water-drop bloop with pitch bend"),
            ("4-glass", v4_glass, "airy FM glass, fourth, light echo"),
            ("5-hollow", v5_hollow, "low tuned marimba, 'Hollow' empty-room tail (app theme)"),
            ("6-phantom", v6_phantom, "warm detuned pad blip, ghostly echo (app theme)"),
            ("7-chatpop", v7_chatpop, "two quick rounded blips, chat-app style")]

# ----------------------------------------------------------------- loudness + export
def aw_rms(x):
    f = np.fft.rfftfreq(len(x), 1 / SR); f[0] = 1
    ra = (12194**2 * f**4) / ((f**2 + 20.6**2) * np.sqrt((f**2 + 107.7**2) * (f**2 + 737.9**2)) * (f**2 + 12194**2))
    return np.sqrt(np.sum(np.abs(np.fft.rfft(x) * ra) ** 2) * 2) / len(x)

def finish(x):
    x = x - x.mean()
    sm = np.convolve(np.abs(x), np.ones(441) / 441, "same")
    x = x[:min(len(x), np.where(sm > sm.max() * 10 ** (-34 / 20))[0][-1] + int(.03 * SR))]
    x = x[:int(.42 * SR)]
    nf = int(.03 * SR); x[-nf:] *= np.linspace(1, 0, nf)
    x[:int(.003 * SR)] *= np.linspace(0, 1, int(.003 * SR))
    return x

def write(path, x):
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((np.clip(x, -1, 1) * 32767).astype(np.int16).tobytes())

if __name__ == "__main__":
    import os; os.makedirs("out", exist_ok=True)
    raw = {}
    for name, fn, _ in VARIANTS:
        for on in (True, False): raw[(name, on)] = finish(fn(on))
    target = np.mean([aw_rms(v) for v in raw.values()])                # equal perceived loudness for every sound
    ceil = 10 ** (-7 / 20); final = {}
    for k, v in raw.items():
        y = v * (target / aw_rms(v)); y *= min(1.0, ceil / np.abs(y).max()); final[k] = y
    s = 0.35 * SR
    aud = []
    for i, (name, fn, desc) in enumerate(VARIANTS, 1):
        for on in (True, False):
            write(f"out/{name}_{'on' if on else 'off'}.wav", final[(name, on)])
        tick = tone(1200, .04, atk=.004, tau=.012) * 0.22
        mark = np.concatenate([np.r_[tick, np.zeros(int(.12 * SR))] for _ in range(i)])      # i soft ticks = pair number i
        aud += [mark, np.zeros(int(.35 * SR)), final[(name, True)], np.zeros(int(.4 * SR)), final[(name, False)], np.zeros(int(1.2 * SR))]
    write("out/audition.wav", np.concatenate(aud))
    print("done")
