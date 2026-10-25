"""Read / write The God Particle's saved state (JUCE VST3 wrapper: 'VC2!' + size + XML, component data in JUCE's
base64; inside: a text snapshot of Lua tables). Used to set the reference's UI scale and other script state."""
import re, struct

TBL = ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"


def b64dec(enc, n):
    out = bytearray(n); pos = 0
    for ch in enc:
        v = TBL.index(ch)
        for b in range(6):
            if pos >= n * 8: break
            if v >> b & 1: out[pos >> 3] |= 1 << (pos & 7)
            pos += 1
    return bytes(out)


def b64enc(data):
    bits = len(data) * 8; out = []
    for i in range(0, bits, 6):
        v = 0
        for b in range(6):
            p = i + b
            if p < bits and data[p >> 3] >> (p & 7) & 1: v |= 1 << b
        out.append(TBL[v])
    return f"{len(data)}." + "".join(out)


def split(state):
    m = re.search(rb'<IComponent>(\d+)\.([^<]*)</IComponent>', state)
    return m, b64dec(m.group(2).decode(), int(m.group(1)))


def join(state, m, comp):
    xml = state[8:]
    xml = xml.replace(m.group(0), b"<IComponent>" + b64enc(comp).encode() + b"</IComponent>")
    return b"VC2!" + struct.pack("<I", len(xml) - 1) + xml   # size excludes the trailing NUL


def edit(state, old, new):
    m, comp = split(state)
    assert old in comp, old
    return join(state, m, comp.replace(old, new))


if __name__ == "__main__":
    import sys
    src, dst = sys.argv[1], sys.argv[2]
    s = open(src, "rb").read()
    for pair in sys.argv[3:]:
        o, n = pair.split("=>")
        s = edit(s, o.encode(), n.encode())
    open(dst, "wb").write(s)
