"""Source/dsp/CharacterTable.h from the per-band fits:
  measurements/character/bandlaw.json   per band and node: shaper drive k, mix, static law, detector (fit_bandlaw.py)
  measurements/character/width.json     per band and node: side gain re mid (fit_width.py)
  measurements/character/fit_char.json  the earlier broadband fits (fit_character.py), for nodes without a band law
The make-up gains are the measured small-signal band gains (BandFilters.h). The detector type (mean-square or peak) is
per band: the one most of the band's usable nodes were fitted with; nodes fitted with the other fall back to fit_char.json.

    python make_table.py [--min-db -40] [--broad high] [--law bandlaw.json] [--shaper high]
                                                         band-law fits worse than this residual are not used;
                                                         --broad: bands kept entirely on fit_char.json
                                                         --shaper: the band's k and dry share at every row as read
                                                         with anti-phase tones (shapeside.py, shaper_<band>.txt);
                                                         band:row applies it from that row up only
The plug-in's table: python make_table.py --min-db 0 --law bandlaw_final_v2.json --shaper high
(low and mid: bandlaw_final.json; high: fit_music.py --shaper --loud at every node, --relative at 100 %, merged with
mergelaws.py; its shaper k, dry share and mid gain as measured, shapeside.py / shapemid.py)."""
import json, os, sys
import tgp, ident as I

DIR = os.path.join(tgp.ROOT, 'measurements', 'character')
HEADER = os.path.join(tgp.PLUGIN, 'Source', 'dsp', 'CharacterTable.h')
NL_HEADER = os.path.join(tgp.PLUGIN, 'Source', 'dsp', 'NlBandFilters.h')
NAMES = ['makeup', 'mix', 'cubic', 'thresh', 'ratio', 'knee', 'attack', 'release', 'side', 'gainAttack', 'gainRelease',
         'attack2', 'release2', 'slowOffset', 'width', 'mixDb', 'shapeDry', 'shapeMid']
BANDS = ['low', 'mid', 'high']
BROAD_PEAK = {'low': 0, 'mid': 0, 'high': 1}     # the detectors fit_char.json was fitted with


def measured_shaper(band):
    """{row: (k, d)} from shapeside.py's lines 'SHAPER band amount k d ...' (amount 0 .. 1 = rows 0 .. 16)"""
    rows = [l.split() for l in open(os.path.join(DIR, f'shaper_{band}.txt')) if l.startswith('SHAPER')]
    return {int(round(float(r[2]) * 16)): (float(r[3]), float(r[4])) for r in rows}


def measured_shape_mid(band):
    """{row: gamma - 1} from shapemid.py's lines 'SHAPEMID row gamma' ({} without a file)"""
    p = os.path.join(DIR, f'shapemid_{band}.txt')
    if not os.path.exists(p): return {}
    return {int(l.split()[1]): float(l.split()[2]) - 1 for l in open(p) if l.startswith('SHAPEMID')}


def build(min_db=-40.0, broad_bands=(), law_file='bandlaw.json', shaper_bands=()):
    """the 17 rows (amount 0, 12.5, ..., 200 %) per band: a fitted law where there is one (law_file, keys
    'band|node' with node 0 .. 8 in steps of 0.5), else the broadband fit at the 25 % nodes, else the mean of the
    neighbours; make-up gain and width measured at every row"""
    law = json.load(open(os.path.join(DIR, law_file)))
    broad = json.load(open(os.path.join(DIR, 'fit_char.json')))
    width = json.load(open(os.path.join(DIR, 'width.json')))
    key = lambda b, k: f'{b}|{k // 2}' if k % 2 == 0 else f'{b}|{k / 2}'
    table, peak, source = {}, {}, {}
    for bi, b in enumerate(BANDS):
        usable = lambda k: key(b, k) in law and law[key(b, k)]['residual_dB'] < min_db and b not in broad_bands
        flags = [law[key(b, k)].get('peak', 0) for k in range(1, 17) if usable(k)]
        peak[b] = int(sum(flags) * 2 > len(flags)) if flags else BROAD_PEAK[b]
        rows, src = [None] * 17, ['-'] * 17
        for k in range(1, 17):
            e = law.get(key(b, k))
            if usable(k) and e.get('peak', 0) == peak[b]:
                row = {n: e[n] for n in ('mix', 'thresh', 'knee', 'ratio', 'attack', 'release')}
                row['cubic'] = e['k'] ** 2 / 6
                row['shapeDry'] = e.get('shapeDry', 0.0); row['mixDb'] = e.get('mixDb', 0.0)
                if 'attack2' in e: row.update(attack2=e['attack2'], release2=e['release2'], slowOffset=e['slowOffset'])
                rows[k], src[k] = row, 'law'
            elif k % 2 == 0:
                rows[k], src[k] = {n: v for n, v in broad[b][k // 2].items() if n in NAMES}, 'broad'
        # amount 0: the 25 % node's detectors and static law with no wet share (transparent, MEASUREMENTS.md 3.2; the
        # reference keeps its detectors running at 0 %: a jump from 0 % settles like one from 100 %). The bands with a
        # fitted shaper dry share (d = 1 - amount / 100 %) keep the drive and are all dry at 0 %; the high band's shaper
        # fades out with its drive
        if b not in broad_bands: rows[0] = dict(rows[2], mix=0.0, shapeDry=1.0)
        else: rows[0] = dict(rows[2], mix=0.0, cubic=0.0, shapeDry=0.0)
        for k in range(1, 17, 2):
            if rows[k] is None:
                lo, hi = rows[k - 1], rows[k + 1]
                rows[k] = {n: 0.5 * (lo.get(n, 0.0) + hi.get(n, 0.0)) for n in NAMES}; src[k] = 'mean'
        from_row = {sb.split(':')[0]: int(sb.split(':')[1]) if ':' in sb else 0 for sb in shaper_bands}
        if b in from_row:
            for k, (kk, dd) in measured_shaper(b).items():
                if k >= from_row[b]: rows[k] = dict(rows[k], cubic=kk * kk / 6, shapeDry=dd, shapeMid=measured_shape_mid(b).get(k, 0.0)); src[k] += '*'
            if from_row[b] == 0: rows[0] = dict(rows[0], shapeDry=1.0)
        for k in range(17):
            rows[k]['side'] = 0.0      # the detector hears the mid signal only (anti-phase tones are not compressed)
            rows[k]['makeup'] = I.GAINS16[bi][k]
            rows[k]['width'] = width[b][k]
        table[b], source[b] = rows, src
    return table, peak, source


def write(table, peak):
    n = len(table['low'])
    lines = ['#pragma once', f"// Generated by nulltest/make_table.py: the character section's per-band settings at amount 0 .. 200 % in {n} steps",
             '// (order of ap::dsp::BandParam). Do not edit by hand; re-run the fits and the script.', 'namespace ap::dsp', '{',
             f'inline constexpr int characterDetectorPeak[3] {{ {peak["low"]}, {peak["mid"]}, {peak["high"]} }};',
             f'inline constexpr double characterTable[3][{n}][{len(NAMES)}] {{']
    for b in BANDS:
        lines.append(f'    {{ // {b}')
        for row in table[b]:
            lines.append('        { ' + ', '.join(f'{row.get(n, 0.0):.6g}' for n in NAMES) + ' },')
        lines.append('    },')
    lines += ['};', '}', '']
    open(HEADER, 'w').write('\n'.join(lines))


def write_nl_filters():
    """NlBandFilters.h: what each band's nonlinear stage hears (ident.nl_firs), at 48 kHz"""
    irs = I.nl_firs()
    n = len(irs[0])
    lines = ['#pragma once', "// Generated by nulltest/make_table.py (ident.nl_firs): the signals the character bands' nonlinear stages",
             '// hear at 48 kHz: the FIR bands of BandFilters.h, the mid band with a Linkwitz-Riley edge above 2675.8 Hz',
             '// (nulltest/nlhear.py). Do not edit by hand.', 'namespace ap::dsp', '{', f'inline constexpr int nlBandTaps48k = {n};',
             f'inline constexpr double nlBandIr48k[3][{n}] {{']
    for h in irs:
        lines.append('    {')
        for i in range(0, n, 8):
            lines.append('        ' + ', '.join(f'{v:.10g}' for v in h[i:i + 8]) + ',')
        lines.append('    },')
    lines += ['};', '}', '']
    open(NL_HEADER, 'w').write('\n'.join(lines))


if __name__ == '__main__':
    min_db = float(sys.argv[sys.argv.index('--min-db') + 1]) if '--min-db' in sys.argv else -40.0
    broad = [sys.argv[i + 1] for i, a in enumerate(sys.argv) if a == '--broad']
    law_file = sys.argv[sys.argv.index('--law') + 1] if '--law' in sys.argv else 'bandlaw.json'
    shaper = [sys.argv[i + 1] for i, a in enumerate(sys.argv) if a == '--shaper']
    table, peak, source = build(min_db, broad, law_file, shaper)
    write(table, peak)
    write_nl_filters()
    print('detector peak', peak)
    for b in BANDS: print(b, ' '.join(source[b]))
