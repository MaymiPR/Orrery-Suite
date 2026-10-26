"""Measurement / null-test harness for Scorpius: renders numpy signals through the reference (Cradle Orion) or our
build with tools/RefRender (an offline JUCE VST3 host). Renders stay local (nulltest/renders is git-ignored).

    import orion
    y = orion.render(x)                                   # x: (2, n) or (n,) float, 48 kHz; reference, preset "start me up"
    y = orion.render(x, params={'Comp On/Off': 0}, hidden={'drive.driveMode': 'b'})
    ys = orion.render_many([dict(x=x1, params=orion.ONLY('drive')), dict(x=x2)], cache=True)

params: host parameters by the names hosts show, by displayed value (on/off switches 0 / 1, 'Harm Freq' 50 / 80,
amounts 0 ... 1, gains in dB). hidden: the settings the host cannot see, by their path in the saved preset
(orionstate.py): 'drive.driveMode' a / b / c, 'ids.compression.mode' 'Mode A' / 'Mode B'; they are written into a
copy of the default state that is loaded before the parameters.
"""
import hashlib, os, subprocess, uuid, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.dirname(HERE)                                          # plugins/Scorpius
ROOT = os.path.dirname(os.path.dirname(PLUGIN))                         # the repository
EXE = os.path.join(ROOT, 'build', 'RefRender_artefacts', 'Release', 'RefRender.exe')
PLUGINS = {
    'ref': (r'C:\Program Files\Common Files\VST3\Cradle Orion.vst3', 'Orion'),
    'ours': (os.path.join(ROOT, 'build', 'Scorpius_artefacts', 'Release', 'VST3', 'Scorpius.vst3'), 'Scorpius'),
}
DEFAULT_STATE = os.path.join(ROOT, 'reference', 'orion', 'state_default.bin')
WORK = os.path.join(HERE, 'renders', 'tmp')
CACHE = os.path.join(HERE, 'renders', 'cache')
STATES = os.path.join(HERE, 'renders', 'states')
FS = 48000
SKIP = 4800        # samples left out of a null: the reference's settings ramp in over the first ~5 ms
SETTLE = 400       # ms of message loop after the parameters: the reference applies its settings asynchronously

MODULES = {'drive': 'Drive On/Off', 'harm': 'Harm On/Off', 'comp': 'Comp On/Off', 'lift': 'Lift On/Off', 'eq': 'EQ On/Off'}
PARALLELS = ['Sub', 'Low', 'Loud', 'Grit']


def ONLY(*on, hipass=None):
    """parameters with only the named modules on ('drive', 'harm', 'comp', 'lift', 'eq'; parallels by name, e.g.
    'sub'), every other module and parallel off"""
    p = {name: (1 if key in on else 0) for key, name in MODULES.items()}
    for k in PARALLELS: p[f'Par {k} On/Off'] = 1 if k.lower() in on else 0
    if hipass is not None: p['Drive Hi-Pass'] = hipass
    return p


NONE = ONLY()


def _stereo(x):
    x = np.asarray(x, np.float32)
    if x.ndim == 1: x = np.stack([x, x])
    return x


def state_for(hidden):
    """a state file with the hidden settings written in (cached by content)"""
    if not hidden: return None
    sys.path.insert(0, HERE)
    import orionstate, tgpstate
    key = hashlib.sha1(repr(sorted(hidden.items())).encode()).hexdigest()[:16]
    path = os.path.join(STATES, key + '.bin')
    if not os.path.exists(path):
        os.makedirs(STATES, exist_ok=True)
        state = open(DEFAULT_STATE, 'rb').read()
        m, comp = tgpstate.split(state)
        head = comp.index(b'--** Cradle')
        text = comp[head:].decode('latin-1')
        for k, v in hidden.items(): text = orionstate.set_value(text, 'preset.' + k, str(v))
        open(path, 'wb').write(tgpstate.join(state, m, comp[:head] + text.encode('latin-1')))
    return path


def _key(j, plugin, fs, block):
    h = hashlib.sha1(_stereo(j['x']).tobytes())
    for k in ('params', 'norm', 'auto', 'raw', 'hidden'): h.update(repr((k, j.get(k) if not isinstance(j.get(k), dict) else sorted(j[k].items()))).encode())
    h.update(repr((plugin, fs, block)).encode())
    return h.hexdigest()


def render_many(jobs, plugin='ref', fs=FS, block=512, cache=False):
    """jobs: dicts with x, and optional params / norm / auto / raw / hidden. Returns a list of (2, n) arrays.
    cache=True keeps each output in renders/cache, keyed by input and settings."""
    if cache:
        os.makedirs(CACHE, exist_ok=True)
        keys = [_key(j, plugin, fs, block) for j in jobs]
        have = {i: np.load(os.path.join(CACHE, k + '.npy')) for i, k in enumerate(keys) if os.path.exists(os.path.join(CACHE, k + '.npy'))}
        todo = [i for i in range(len(jobs)) if i not in have]
        new = render_many([jobs[i] for i in todo], plugin, fs, block) if todo else []
        for i, y in zip(todo, new):
            have[i] = y
            np.save(os.path.join(CACHE, keys[i] + '.npy'), y)
        return [have[i] for i in range(len(jobs))]
    os.makedirs(WORK, exist_ok=True)
    tag = uuid.uuid4().hex[:8]
    lines, outs = [], []
    for i, j in enumerate(jobs):
        x = _stereo(j['x'])
        fin = os.path.join(WORK, f'{tag}_{i}_in.f32'); fout = os.path.join(WORK, f'{tag}_{i}_out.f32')
        x.T.astype(np.float32).tofile(fin)
        parts = [f'"{fin}"', f'"{fout}"']
        st = state_for(j.get('hidden'))
        if st: parts.append(f'"state={st}"')
        for k, v in (j.get('params') or {}).items(): parts.append(f'"param:{k}={v}"')
        for k, v in (j.get('norm') or {}).items(): parts.append(f'"norm:{k}={v}"')
        for k, s, v in (j.get('auto') or []): parts.append(f'"auto:{k}@{int(s)}={v}"')
        if j.get('raw'): parts.append('raw=1')
        parts.append(f'settle={SETTLE}')
        lines.append(' '.join(parts)); outs.append((fin, fout))
    jobfile = os.path.join(WORK, f'{tag}_jobs.txt')
    open(jobfile, 'w').write('\n'.join(lines) + '\n')
    path, name = PLUGINS[plugin]
    r = subprocess.run([EXE, path, name, jobfile, f'fs={fs}', f'block={block}'], capture_output=True, text=True)
    if 'no parameter' in r.stdout: raise RuntimeError(r.stdout)
    res = []
    for fin, fout in outs:
        if not os.path.exists(fout): raise RuntimeError('render failed:\n' + r.stdout + r.stderr)
        res.append(np.fromfile(fout, np.float32).reshape(-1, 2).T.copy())
        os.remove(fin); os.remove(fout)
    os.remove(jobfile)
    return res


def render(x, plugin='ref', fs=FS, block=512, cache=False, **kw):
    return render_many([dict(x=x, **kw)], plugin, fs, block, cache)[0]


def latency(plugin='ref', fs=FS):
    r = subprocess.run([EXE, PLUGINS[plugin][0], PLUGINS[plugin][1], os.devnull, f'fs={fs}'], capture_output=True, text=True)
    return int(r.stdout.split('latency ')[1].split()[0])


# --- signals and measures (as The Antiparticle's harness) -------------------------------------------------------
def tone(f, secs, db, fs=FS, phase=0.0):
    t = np.arange(int(secs * fs)) / fs
    return (10 ** (db / 20) * np.sin(2 * np.pi * f * t + phase)).astype(np.float32)


def noise(secs, db_rms, fs=FS, seed=1, pink=False):
    rng = np.random.default_rng(seed)
    x = rng.standard_normal(int(secs * fs))
    if pink:
        X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1 / fs); X[1:] /= np.sqrt(f[1:]); X[0] = 0
        x = np.fft.irfft(X, len(x))
    x /= np.sqrt(np.mean(x ** 2))
    return (x * 10 ** (db_rms / 20)).astype(np.float32)


def db(x):
    return 20 * np.log10(np.maximum(np.abs(x), 1e-12))


def rms_db(x):
    x = np.asarray(x, np.float64)
    return 10 * np.log10(max(float(np.mean(x * x)), 1e-30))


def null_db(a, b, skip=SKIP):
    """residual of b against a, relative to a (dB), after the first `skip` samples (the reference ramps its settings
    in over the first few ms of a render)"""
    n = min(a.shape[-1], b.shape[-1])
    return rms_db(a[..., skip:n] - b[..., skip:n]) - rms_db(a[..., skip:n])


def harmonics(y, f, fs=FS, count=10):
    """levels (dBFS of a sine) of the fundamental and its harmonics in a steady stretch, Hann window"""
    y = np.asarray(y, np.float64)
    w = np.hanning(len(y)); Y = np.abs(np.fft.rfft(y * w)) * 2 / w.sum()
    fr = np.fft.rfftfreq(len(y), 1 / fs)
    out = []
    for k in range(1, count + 1):
        i = np.argmin(np.abs(fr - k * f)); out.append(20 * np.log10(max(Y[max(0, i - 2):i + 3].max(), 1e-12)))
    return np.array(out)
