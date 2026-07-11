#!/usr/bin/env python3
"""Dump Loco.exe's RT_STRING table -- the TileKind id -> resource-path map.

UIResources::TileKind_GetOrLoadDescriptor (0x446ea0) turns a kind id straight into a
LoadStringA call against the exe's own string table, so string id N *is* kind id N and its
text is the resource path inside resource.RFD (minus the .dat/.bmp extension).

TileKind::TileKind_GetCategory (0x446030) is just `(kindId >> 10) & 0xff`, valid only when
that lands below 0x10 -- so each 0x400-id block is one category:

    0x0  0x0000-0x03ff  credits / misc UI text
    0x1  0x0400-0x07ff  startup, backdrop, cursors
    0x2  0x0800-0x0bff  building\\*          <- WalkerActor's destination pool
    0x3  0x0c00-0x0fff  track\\*             <- rail (walkable only while riding)
    0x4  0x1000-0x13ff  scenery\\*
    0x5  0x1400-0x17ff  cursors\\*
    0x6  0x1800-0x1bff  trains\\*
    0x7  0x1c00-0x1fff  people\\* + animals\\*  <- WalkerActor
    0x8  0x2000-0x23ff  vehicles\\*          <- RoadVehicleActor
    0x9  0x2400-0x27ff  toybox\\*
    0xa  0x2800-0x2bff  toybox\\*
    0xb  0x2c00-0x2fff  toybox\\*
    0xc  0x3000-0x33ff  paths\\*             <- WalkerActor's walkable network
    0xd  0x3400-0x37ff  roads\\*             <- RoadVehicleActor's drivable network
    0xe  0x3800-0x3bff  misc\\*
    0xf  0x3c00-0x3fff  post\\*, tutorial\\*, map\\*

Ids >= 0x4000 have no category -- they are ordinary localized UI strings and sound paths.

Usage:
    tools/resstrings.py                 # category histogram
    tools/resstrings.py 0x1c00 0x1fff   # dump an id range
    tools/resstrings.py --doc           # regenerate docs/TILEKIND_IDS.md
"""
import collections
import os
import struct
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(REPO, "loco", "Loco.exe")
DOC = os.path.join(REPO, "docs", "TILEKIND_IDS.md")

CATEGORY_LABELS = {
    0x0: "credits / misc UI text",
    0x1: "startup, backdrop, cursors",
    0x2: "building\\*  -- WalkerActor destination pool",
    0x3: "track\\*  -- rail (WalkerActor may walk it only while riding a train)",
    0x4: "scenery\\*",
    0x5: "cursors\\*",
    0x6: "trains\\*",
    0x7: "people\\* + animals\\*  -- WalkerActor",
    0x8: "vehicles\\*  -- RoadVehicleActor",
    0x9: "toybox\\*",
    0xA: "toybox\\*",
    0xB: "toybox\\*",
    0xC: "paths\\*  -- WalkerActor's walkable network",
    0xD: "roads\\*  -- RoadVehicleActor's drivable network",
    0xE: "misc\\*",
    0xF: "post\\*, tutorial\\*, map\\*",
}


def read_strings(path=EXE):
    """Walk the PE resource directory and return {string_id: text}."""
    data = open(path, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise SystemExit("not a PE: %s" % path)
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    secoff = pe + 24 + optsz
    rsrc = None
    for i in range(nsec):
        o = secoff + i * 40
        if data[o:o + 8].rstrip(b"\0") == b".rsrc":
            rsrc = struct.unpack_from("<IIII", data, o + 8)  # vsize, vaddr, rawsize, rawptr
    if rsrc is None:
        raise SystemExit("no .rsrc section")
    _, rva, _, raw = rsrc

    def entries(off):
        nnamed, nid = struct.unpack_from("<HH", data, off + 12)
        return [struct.unpack_from("<II", data, off + 16 + i * 8)
                for i in range(nnamed + nid)]

    strings = {}
    for type_id, type_off in entries(raw + (rva - rva)):
        if type_id & 0x80000000 or type_id != 6:  # RT_STRING only
            continue
        for name_id, name_off in entries(raw + (type_off & 0x7FFFFFFF)):
            for _lang, lang_off in entries(raw + (name_off & 0x7FFFFFFF)):
                data_rva, size = struct.unpack_from("<II", data, raw + lang_off)
                p = raw + (data_rva - rva)
                # One RT_STRING block holds 16 consecutive ids, each a length-prefixed
                # UTF-16LE run (a zero length means "no string at this id").
                q, base = p, (name_id - 1) * 16
                for k in range(16):
                    if q + 2 > p + size:
                        break
                    ln = struct.unpack_from("<H", data, q)[0]
                    q += 2
                    text = data[q:q + ln * 2].decode("utf-16-le", "replace")
                    q += ln * 2
                    if ln:
                        strings[base + k] = text
    return strings


def category_of(kind_id):
    """Mirror of TileKind::TileKind_GetCategory (0x446030)."""
    cat = (kind_id >> 10) & 0xFF
    return cat if cat < 0x10 else None


def histogram(strings):
    buckets = collections.defaultdict(list)
    for kind_id, text in strings.items():
        buckets[category_of(kind_id)].append((kind_id, text))
    for cat in sorted(buckets, key=lambda c: (c is None, c)):
        items = sorted(buckets[cat])
        if cat is None:
            print("  (no category, id >= 0x4000)      n=%4d   localized UI text + sound paths"
                  % len(items))
            continue
        print("  cat 0x%x  0x%04x-0x%04x  n=%4d   %s"
              % (cat, cat << 10, (cat << 10) | 0x3FF, len(items), CATEGORY_LABELS.get(cat, "")))


def write_doc(strings):
    buckets = collections.defaultdict(list)
    for kind_id, text in strings.items():
        buckets[category_of(kind_id)].append((kind_id, text))
    out = [
        "# TileKind id -> resource path",
        "",
        "Generated by `tools/resstrings.py --doc` -- do not hand-edit.",
        "",
        "A TileKind id *is* an `RT_STRING` id in `Loco.exe`: "
        "`UIResources::TileKind_GetOrLoadDescriptor` (0x446ea0) feeds the id straight to",
        "`LoadStringA`, and the resulting text is the resource path inside `resource.RFD` "
        "(without the `.dat`/`.bmp` extension).",
        "`TileKind::TileKind_GetCategory` (0x446030) is `(kindId >> 10) & 0xff`, valid only "
        "below `0x10` -- so every 0x400-id block is exactly one category.",
        "",
        "Ids >= 0x4000 carry no category; they are ordinary localized UI strings and "
        "`sounds\\*` paths, and are omitted below.",
        "",
    ]
    for cat in sorted(c for c in buckets if c is not None):
        items = sorted(buckets[cat])
        out.append("## Category 0x%x -- ids 0x%04x-0x%04x (%d strings)"
                   % (cat, cat << 10, (cat << 10) | 0x3FF, len(items)))
        out.append("")
        out.append(CATEGORY_LABELS.get(cat, ""))
        out.append("")
        out.append("| id | resource |")
        out.append("|---|---|")
        for kind_id, text in items:
            out.append("| `0x%04x` | `%s` |" % (kind_id, text))
        out.append("")
    open(DOC, "w").write("\n".join(out))
    print("wrote %s (%d categorized ids)"
          % (DOC, sum(len(v) for k, v in buckets.items() if k is not None)))


def main():
    strings = read_strings()
    args = sys.argv[1:]
    if not args:
        print("%d RT_STRING entries in %s" % (len(strings), EXE))
        histogram(strings)
    elif args[0] == "--doc":
        write_doc(strings)
    else:
        lo = int(args[0], 0)
        hi = int(args[1], 0) if len(args) > 1 else lo
        for kind_id in range(lo, hi + 1):
            if kind_id in strings:
                print("0x%04x  %s" % (kind_id, strings[kind_id]))


if __name__ == "__main__":
    main()
