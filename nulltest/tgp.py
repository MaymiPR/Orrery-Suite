"""Measurement / null-test harness: renders numpy signals through the reference (Cradle The God Particle) or our
build with tools/RefRender (an offline JUCE VST3 host). Renders stay local (nulltest/renders is git-ignored).

    import tgp
    y = tgp.render(x)                                    # x: (2, n) or (n,) float, 48 kHz; reference, default settings
    ys = tgp.render_many([dict(x=x1, params={'Character': 0.25}), dict(x=x2, norm={'Limiter Bypass': 1})])
    y = tgp.render(x, plugin='ours')

Parameters by the names hosts show: params={'Input Gain': -6} uses the displayed value, norm={...} the normalised
value; auto=[('Input Gain', sample, norm), ...] changes a parameter while rendering (on block boundaries).
plugin='model' renders through our processor with tools/ApRender: tune={'mid.mix.4': 0.7} overrides DSP constants
(Source/dsp/Tuning.h), tuning=file loads a tuning file for every job, ext=True selects the side-chain EXT.
"""
import hashlib, os, subprocess, tempfile, uuid
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, 'build', 'RefRender_artefacts', 'Release', 'RefRender.exe')
APEXE = os.path.join(ROOT, 'build', 'ApRender_artefacts', 'Release', 'ApRender.exe')   # our processor, tunable ('model')
if os.environ.get('AP_RENDER') == 'dev':      # the second copy, for DSP changes while a fit runs on the first
    APEXE = os.path.join(ROOT, 'build', 'ApRenderDev_artefacts', 'Release', 'ApRenderDev.exe')
PLUGINS = {
    'ref': (r'C:\Program Files\Common Files\VST3\Cradle The God Particle.vst3', 'The God Particle'),
    'ours': (os.path.join(ROOT, 'build', 'TheAntiparticle_artefacts', 'Release', 'VST3', 'The Antiparticle.vst3'), 'The Antiparticle'),
}
WORK = os.path.join(ROOT, 'nulltest', 'renders', 'tmp')
CACHE = os.path.join(ROOT, 'nulltest', 'renders', 'cache')
FS = 48000

# isolate one stage of the chain (the others bypassed)
EQ_ONLY = {'Character Bypass': 1, 'Limiter Bypass': 1}
CHAR_ONLY = {'EQ Bypass': 1, 'Limiter Bypass': 1}
LIM_ONLY = {'EQ Bypass': 1, 'Character Bypass': 1}
NONE = {'EQ Bypass': 1, 'Character Bypass': 1, 'Limiter Bypass': 1}


def _stereo(x):
    x = np.asarray(x, np.float32)
    if x.ndim == 1: x = np.stack([x, x])
    return x


def _key(j, plugin, fs, block, vary=False):
    h = hashlib.sha1(_stereo(j['x']).tobytes())
    for k in ('params', 'norm', 'auto', 'raw', 'reset', 'state'): h.update(repr((k, j.get(k))).encode())
    if j.get('salt'): h.update(repr(('salt', j['salt'])).encode())     # global state a job can't carry (oversampling)
    if j.get('sc') is not None: h.update(_stereo(j['sc']).tobytes())
    h.update(repr((plugin, fs, block) + (('vary',) if vary else ())).encode())
    return h.hexdigest()


def render_many(jobs, plugin='ref', fs=FS, block=512, cache=False, tuning=None, vary=False):
    """jobs: dicts with x, and optional params / norm / auto / raw / sc / state / reset. Returns a list of (2, n).
    cache=True keeps each output in renders/cache (keyed by the input and settings) so analyses can be re-run without
    rendering again; jobs with reset=False are never cached (they depend on the previous job). vary=True (reference and
    VST3 only): irregular blocks of 1 ... block samples, as FL Studio delivers them."""
    if cache and plugin != 'model':          # our processor changes with every build / tuning: never cached
        os.makedirs(CACHE, exist_ok=True)
        keys = [None if any(j.get('reset') is False for j in jobs) else _key(j, plugin, fs, block, vary) for j in jobs]
        have = {i: np.load(os.path.join(CACHE, k + '.npy')) for i, k in enumerate(keys) if k and os.path.exists(os.path.join(CACHE, k + '.npy'))}
        todo = [i for i in range(len(jobs)) if i not in have]
        new = render_many([jobs[i] for i in todo], plugin, fs, block, vary=vary) if todo else []
        for i, y in zip(todo, new):
            have[i] = y
            if keys[i]: np.save(os.path.join(CACHE, keys[i] + '.npy'), y)
        return [have[i] for i in range(len(jobs))]
    os.makedirs(WORK, exist_ok=True)
    tag = uuid.uuid4().hex[:8]
    lines, outs = [], []
    for i, j in enumerate(jobs):
        x = _stereo(j['x'])
        fin = os.path.join(WORK, f'{tag}_{i}_in.f32'); fout = os.path.join(WORK, f'{tag}_{i}_out.f32')
        x.T.astype(np.float32).tofile(fin)
        parts = [f'"{fin}"', f'"{fout}"']
        for k, v in (j.get('params') or {}).items(): parts.append(f'"param:{k}={v}"')
        for k, v in (j.get('norm') or {}).items(): parts.append(f'"norm:{k}={v}"')
        for k, s, v in (j.get('auto') or []): parts.append(f'"auto:{k}@{int(s)}={v}"')
        if j.get('raw'): parts.append('raw=1')
        if j.get('reset') is False: parts.append('reset=0')
        if j.get('state'): parts.append(f'"state={j["state"]}"')
        for k, v in (j.get('tune') or {}).items(): parts.append(f'"tune:{k}={float(v)!r}"')
        if j.get('ext'): parts.append('ext=1')
        if j.get('sc') is not None:
            fsc = os.path.join(WORK, f'{tag}_{i}_sc.f32'); _stereo(j['sc']).T.astype(np.float32).tofile(fsc); parts.append(f'"sc={fsc}"')
        lines.append(' '.join(parts)); outs.append((fin, fout, x.shape[1]))
    jobfile = os.path.join(WORK, f'{tag}_jobs.txt')
    open(jobfile, 'w').write('\n'.join(lines) + '\n')
    if plugin == 'model':
        cmd = [APEXE, jobfile, f'fs={fs}', f'block={block}'] + ([f'tuning={tuning}'] if tuning else [])
    else:
        path, name = PLUGINS[plugin]
        cmd = [EXE, path, name, jobfile, f'fs={fs}', f'block={block}'] + (['vary=1'] if vary else [])
    r = subprocess.run(cmd, capture_output=True, text=True)
    if 'no parameter' in r.stdout or 'unknown tuning' in r.stdout: raise RuntimeError(r.stdout)
    res = []
    for fin, fout, n in outs:
        if not os.path.exists(fout): raise RuntimeError('render failed:\n' + r.stdout + r.stderr)
        y = np.fromfile(fout, np.float32).reshape(-1, 2).T.copy()
        res.append(y)
        os.remove(fin); os.remove(fout)
    os.remove(jobfile)
    return res


def render(x, plugin='ref', **kw):
    fs = kw.pop('fs', FS); block = kw.pop('block', 512)
    return render_many([dict(x=x, **kw)], plugin, fs, block)[0]


def latency(plugin='ref', fs=FS):
    r = subprocess.run([EXE, PLUGINS[plugin][0], PLUGINS[plugin][1], os.devnull, f'fs={fs}'], capture_output=True, text=True)
    return int(r.stdout.split('latency ')[1].split()[0])


# --- signals --------------------------------------------------------------------------------------------------
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
