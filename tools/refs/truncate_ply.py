"""Truncate a binary-little-endian PLY 'vertex' element to the first N rows.

Usage: truncate_ply.py <in.ply> <out.ply> <N>

Keeps the header verbatim except the 'element vertex COUNT' line, and copies
the first N*rowbytes of vertex data. Assumes a single 'vertex' element whose
properties are all scalar float32 (true for root.primitives_pyr0.ply).
"""
import sys

def main():
    inp, out, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
    with open(inp, "rb") as f:
        raw = f.read()

    # Split header / body at end_header\n
    marker = b"end_header\n"
    hdr_end = raw.find(marker) + len(marker)
    header = raw[:hdr_end].decode("ascii")
    body = raw[hdr_end:]

    lines = header.splitlines()
    props = [l for l in lines if l.startswith("property")]
    # all float32 => 4 bytes each
    for p in props:
        assert "float" in p, f"non-float property not supported: {p}"
    row_bytes = len(props) * 4

    # original vertex count
    vcount = None
    for i, l in enumerate(lines):
        if l.startswith("element vertex"):
            vcount = int(l.split()[-1])
            lines[i] = f"element vertex {n}"
    assert vcount is not None
    assert n <= vcount, f"N={n} exceeds vertex count {vcount}"

    new_header = ("\n".join(lines) + "\n").encode("ascii")
    new_body = body[: n * row_bytes]

    with open(out, "wb") as f:
        f.write(new_header)
        f.write(new_body)
    print(f"wrote {out}: {n}/{vcount} vertices, {len(props)} props, {row_bytes} B/row")

if __name__ == "__main__":
    main()
