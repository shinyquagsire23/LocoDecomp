#!/usr/bin/env python3
"""Hand-parse an MS-format COFF archive (.LIB, e.g. toolchain/vc50/LIB/LIBCMT.LIB) to
extract member .obj files by name or by defined symbol — for identifying opaque
past-TOTAL_APP_END FUN_ addresses as real statically-linked CRT internals (see
CLAUDE.md's "extracting the real .obj module" lesson).

GNU ar's x/p silently no-ops on this lib's long-name table (0-byte output, no error);
this reimplements the minimal archive walk needed instead: magic, 60-byte headers,
the '//' longname-table member, and the '/' linker member (symbol -> member-offset
index, used by --symbol lookups).

Usage:
  extract_lib_member.py LIB.lib                    # list all member names/sizes
  extract_lib_member.py LIB.lib --name NEEDLE       # extract members whose name contains NEEDLE (case-insensitive substring)
  extract_lib_member.py LIB.lib --symbol NAME       # find + extract the member that DEFINES symbol NAME (e.g. __ld12tod)
  extract_lib_member.py LIB.lib ... --outdir DIR    # default: cwd

Then disassemble with:
  objdump -d -M intel -b coff-i386 <extracted.obj>
"""
import argparse
import os
import struct
import sys

MAGIC = b"!<arch>\n"


def parse_archive(path):
    """Returns (data, members, symtab) where members is [(name, data_offset, size), ...]
    and symtab is {symbol_name: member_data_offset} from the first ('/') linker member."""
    with open(path, "rb") as f:
        data = f.read()
    assert data[:8] == MAGIC, "not an ar-format archive"
    pos = 8
    longnames = b""
    members = []
    symtab = {}
    n = len(data)
    while pos < n:
        if pos + 60 > n:
            break
        hdr = data[pos:pos + 60]
        if len(hdr) < 60:
            break
        name = hdr[0:16].decode("ascii", "replace").rstrip()
        size_str = hdr[48:58].decode("ascii", "replace").strip()
        try:
            size = int(size_str)
        except ValueError:
            break
        data_off = pos + 60
        member_data = data[data_off:data_off + size]
        if name == "/":
            if not symtab:  # first linker member only
                count = struct.unpack(">I", member_data[0:4])[0]
                offsets = struct.unpack(">%dI" % count, member_data[4:4 + 4 * count])
                names = member_data[4 + 4 * count:].split(b"\x00")
                for i in range(count):
                    symtab[names[i].decode("ascii", "replace")] = offsets[i]
        elif name == "//":
            longnames = member_data
        elif name.startswith("/"):
            try:
                off = int(name[1:])
                end = longnames.index(b"\x00", off)
                real_name = longnames[off:end].decode("ascii", "replace")
            except Exception:
                real_name = name
            members.append((real_name, data_off, size))
        else:
            members.append((name.rstrip("/"), data_off, size))
        pos = data_off + size
        if pos % 2 == 1:
            pos += 1
    return data, members, symtab


def member_at_offset(members, target_off):
    for name, off, size in members:
        if off - 60 <= target_off <= off:
            return name, off, size
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("lib_path")
    ap.add_argument("--name", help="extract members whose name contains this substring (case-insensitive)")
    ap.add_argument("--symbol", help="find + extract the member that defines this exact symbol name (e.g. __ld12tod)")
    ap.add_argument("--outdir", default=".", help="directory to write extracted .obj files into (default: cwd)")
    args = ap.parse_args()

    data, members, symtab = parse_archive(args.lib_path)

    if args.symbol:
        if args.symbol not in symtab:
            print(f"symbol {args.symbol!r} not found in archive symbol index", file=sys.stderr)
            sys.exit(1)
        target_off = symtab[args.symbol]
        hit = member_at_offset(members, target_off)
        if not hit:
            print(f"symbol {args.symbol!r} points at file offset {target_off:#x} but no member header found there", file=sys.stderr)
            sys.exit(1)
        name, off, size = hit
        outname = os.path.join(args.outdir, os.path.basename(name.replace("\\", "/")))
        with open(outname, "wb") as out:
            out.write(data[off:off + size])
        print(f"symbol {args.symbol!r} -> member {name} ({size} bytes) -> {outname}")
        return

    if args.name:
        pattern = args.name.lower()
        hits = [(n, o, s) for n, o, s in members if pattern in n.lower()]
        if not hits:
            print(f"no member name contains {args.name!r}", file=sys.stderr)
            sys.exit(1)
        for name, off, size in hits:
            outname = os.path.join(args.outdir, os.path.basename(name.replace("\\", "/")))
            with open(outname, "wb") as out:
                out.write(data[off:off + size])
            print(f"extracted {name} ({size} bytes) -> {outname}")
        return

    # default: list everything
    for name, off, size in members:
        print(f"{name}\t{size}\t@{off:#x}")


if __name__ == "__main__":
    main()
