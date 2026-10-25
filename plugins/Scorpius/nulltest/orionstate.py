"""Read / edit Orion's saved state (the same JUCE VST3 wrapper and Lua-table snapshot as The God Particle's, see
../../TheAntiparticle/nulltest/tgpstate.py). Values are set by their path in the preset table:
  python orionstate.py in.bin out.bin prefs.uiScaling=2 preset.drive.driveMode=b preset.eq.onOff=1 preset.eq.gain3=6
  python orionstate.py in.bin -          (prints the snapshot)"""
import os, re, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'TheAntiparticle', 'nulltest'))
import tgpstate


def block_span(text, path):
    """(start, end) of the table body for a dotted path like 'preset.parallels.sub'"""
    start, end = 0, len(text)
    for name in path:
        m = re.compile(r'\b' + re.escape(name) + r'\s*=\s*\{').search(text, start, end)
        if m is None: raise KeyError('.'.join(path))
        depth, i = 1, m.end()
        while depth:
            depth += {'{': 1, '}': -1}.get(text[i], 0); i += 1
        start, end = m.end(), i - 1
    return start, end


def set_value(text, dotted, value):
    *path, key = dotted.split('.')
    s, e = block_span(text, path) if path else (0, len(text))
    # only the block's own keys, not those of nested tables
    m = re.compile(r'(\n\s*' + re.escape(key) + r'\s*=\s*)([^,\n{]+)').search(text, s, e)
    if m is None: raise KeyError(dotted)
    v = value if re.fullmatch(r'-?[0-9.]+', value) else '"' + value + '"'
    return text[:m.start(2)] + v + text[m.end(2):]


if __name__ == '__main__':
    state = open(sys.argv[1], 'rb').read()
    m, comp = tgpstate.split(state)
    head = comp.index(b'--** Cradle')
    text = comp[head:].decode('latin-1')
    if sys.argv[2] == '-':
        print(text); sys.exit()
    for a in sys.argv[3:]:
        k, v = a.split('=', 1)
        text = set_value(text, k, v)
    # the snapshot's length is not stored separately inside the component data, so a changed length is fine
    open(sys.argv[2], 'wb').write(tgpstate.join(state, m, comp[:head] + text.encode('latin-1')))
