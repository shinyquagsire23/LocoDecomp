#!/usr/bin/env python3
"""Extract loco/Loco.exe's .rsrc directory into a Win32 .RES file for relinking.

The byte-match product (build/Loco-linked.exe, tools/link_check.sh) had NO .rsrc
section at all, because nothing in src/ can produce one: resources are data, not
code, and the repo has no .rc source (VC5 ships CVTRES.EXE but no RC.EXE).  The
original's resources are load-bearing at runtime, not decoration --
UIResources::TileKind_GetOrLoadDescriptor (0x446ea0) turns every TileKind id into
a LoadStringA against the exe's own RT_STRING table to get the resource path
inside resource.RFD, so with an empty string table the game loads no art at all
(see tools/resstrings.py / docs/TILEKIND_IDS.md).

So: walk the original's PE resource directory and re-emit every leaf as a .RES
entry, which the VC5 linker accepts on its command line directly (it shells out
to CVTRES internally).  This is a faithful copy of the original's resource data,
regenerated from the game the user owns -- nothing derived is checked in, and
loco/ stays gitignored.  Replacing it with a real hand-written .rc is future
work; the resource DATA would still be these same bytes.

Usage:
    tools/extract_res.py [--exe loco/Loco.exe] [-o link/Loco.res] [--list]
"""
import argparse
import os
import struct
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

RT_NAMES = {
    1: "CURSOR", 2: "BITMAP", 3: "ICON", 4: "MENU", 5: "DIALOG", 6: "STRING",
    7: "FONTDIR", 8: "FONT", 9: "ACCELERATOR", 10: "RCDATA", 11: "MESSAGETABLE",
    12: "GROUP_CURSOR", 14: "GROUP_ICON", 16: "VERSION", 24: "MANIFEST",
}

# Memory flags RC.EXE stamps by default per resource type.  The Win32 loader
# ignores these (they are 16-bit Windows heritage), but keep them plausible.
MOVEABLE, PURE, PRELOAD, DISCARDABLE = 0x10, 0x20, 0x40, 0x1000
DEFAULT_FLAGS = {
    1: MOVEABLE | PURE | DISCARDABLE,   # CURSOR
    2: MOVEABLE | PURE,                 # BITMAP
    3: MOVEABLE | PURE | DISCARDABLE,   # ICON
    4: MOVEABLE | PURE | DISCARDABLE,   # MENU
    5: MOVEABLE | PURE | DISCARDABLE,   # DIALOG
    6: MOVEABLE | PURE | DISCARDABLE,   # STRING
    12: MOVEABLE | PURE | DISCARDABLE,  # GROUP_CURSOR
    14: MOVEABLE | PURE | DISCARDABLE,  # GROUP_ICON
    16: MOVEABLE | PURE,                # VERSION
}


def read_sections(d):
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    optsz = struct.unpack_from("<H", d, pe + 20)[0]
    secs = []
    for i in range(nsec):
        off = pe + 24 + optsz + i * 40
        name = d[off:off + 8].rstrip(b"\0").decode("latin-1")
        vsz, va, rsz, ra = struct.unpack_from("<IIII", d, off + 8)
        secs.append((name, va, vsz, ra, rsz))
    return secs


class ResDir(object):
    """Walks a PE .rsrc tree, yielding (type, name, lang, data) leaves."""

    def __init__(self, d, rsrc_va, rsrc_raw):
        self.d, self.va, self.raw = d, rsrc_va, rsrc_raw

    def at(self, rva):
        return self.raw + (rva - self.va)

    def entry_name(self, nameoff):
        if not (nameoff & 0x80000000):
            return nameoff
        p = self.at(self.va + (nameoff & 0x7FFFFFFF))
        ln = struct.unpack_from("<H", self.d, p)[0]
        return self.d[p + 2:p + 2 + ln * 2].decode("utf-16le")

    def walk(self, off=None, path=()):
        if off is None:
            off = self.raw
        nnamed, nid = struct.unpack_from("<HH", self.d, off + 12)
        base = off + 16
        for i in range(nnamed + nid):
            nameoff, dataoff = struct.unpack_from("<II", self.d, base + i * 8)
            nm = self.entry_name(nameoff)
            if dataoff & 0x80000000:
                sub = self.at(self.va + (dataoff & 0x7FFFFFFF))
                for leaf in self.walk(sub, path + (nm,)):
                    yield leaf
            else:
                p = self.at(self.va + dataoff)
                drva, dsz, _cp, _rv = struct.unpack_from("<IIII", self.d, p)
                data = self.d[self.at(drva):self.at(drva) + dsz]
                t, n, lang = (list(path) + [nm])[:3]
                yield t, n, lang, data


def res_name_field(v):
    """.RES type/name field: 0xFFFF + WORD id, or a NUL-terminated UTF-16 string."""
    if isinstance(v, int):
        return struct.pack("<HH", 0xFFFF, v)
    return v.encode("utf-16le") + b"\0\0"


def res_entry(rtype, rname, lang, data):
    head = res_name_field(rtype) + res_name_field(rname)
    head += b"\0" * (-len(head) % 4)                       # DWORD-align the names
    flags = DEFAULT_FLAGS.get(rtype, MOVEABLE | PURE) if isinstance(rtype, int) \
        else MOVEABLE | PURE
    hdrsize = 8 + len(head) + 16                           # sizes + names + tail
    tail = struct.pack("<IHHII", 0, flags, lang, 0, 0)     # DataVersion, flags,
    out = struct.pack("<II", len(data), hdrsize) + head + tail  # lang, Version, Chars
    assert len(out) == hdrsize, (len(out), hdrsize)
    return out + data + b"\0" * (-len(data) % 4)


def build_res(leaves):
    # The .RES stream opens with a 32-byte null entry (type 0, name 0, no data).
    out = [struct.pack("<II", 0, 32) + struct.pack("<HHHH", 0xFFFF, 0, 0xFFFF, 0)
           + struct.pack("<IHHII", 0, 0, 0, 0, 0)]
    for t, n, lang, data in leaves:
        out.append(res_entry(t, n, lang, data))
    return b"".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=os.path.join(REPO, "loco", "Loco.exe"))
    ap.add_argument("-o", "--out", default=os.path.join(REPO, "link", "Loco.res"))
    ap.add_argument("--list", action="store_true", help="list leaves, write nothing")
    args = ap.parse_args()

    if not os.path.exists(args.exe):
        sys.stderr.write("no such exe: %s\n" % args.exe)
        return 1
    d = open(args.exe, "rb").read()
    rsrc = [s for s in read_sections(d) if s[0] == ".rsrc"]
    if not rsrc:
        sys.stderr.write("%s has no .rsrc section\n" % args.exe)
        return 1
    _, va, _, ra, _ = rsrc[0]
    leaves = list(ResDir(d, va, ra).walk())

    if args.list:
        for t, n, lang, data in leaves:
            tn = RT_NAMES.get(t, str(t)) if isinstance(t, int) else repr(t)
            print("%-14s %-8s lang=%d %7d bytes" % (tn, n, lang, len(data)))
    blob = build_res(leaves)
    if not args.list:
        open(args.out, "wb").write(blob)
    tot = sum(len(x[3]) for x in leaves)
    print("%d resources, %d bytes of data -> %s (%d bytes)"
          % (len(leaves), tot, args.out if not args.list else "(not written)", len(blob)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
