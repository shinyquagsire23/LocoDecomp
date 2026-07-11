#!/usr/bin/env python3
"""Per-COMDAT masked byte diff of two .obj files.

The verification step CLAUDE.md requires after any bulk identifier rename (function, global,
or struct MEMBER): a pure rename can never change generated code, so every COMDAT whose bytes
move is a real regression -- most often the member/local shadowing bug documented in
CLAUDE.md's naming section, which compiles cleanly and no other lint catches.

Do NOT just `cmp` the two .obj files: byte 5 of a COFF header is the build timestamp, so they
always differ. This compares COMDAT by COMDAT, masks relocation targets, and trims trailing
0xCC/0x90 padding -- the same normalization tools/match.py uses.

Usage:
    cp build/Foo.obj /path/to/scratch/Foo-before.obj   # BEFORE the rename
    tools/cc.sh src/Foo.cpp                            # rebuild AFTER
    tools/objdiff.py /path/to/scratch/Foo-before.obj build/Foo.obj

Exit status is 1 if any COMDAT differs or the two sides disagree on which COMDATs exist.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match


def load(path):
    out = {}
    for name, code, relocs in match.coff_functions(path):
        n = match.trim_pad(code)
        out[name] = match.mask(code, relocs, n)
    return out


def main(argv):
    if len(argv) != 3:
        sys.stderr.write("usage: objdiff.py BEFORE.obj AFTER.obj\n")
        return 2

    a, b = load(argv[1]), load(argv[2])
    ka, kb = set(a), set(b)
    bad = False

    for name in sorted(ka - kb):
        print("only in %s: %s" % (argv[1], name))
        bad = True
    for name in sorted(kb - ka):
        print("only in %s: %s" % (argv[2], name))
        bad = True

    differing = [k for k in sorted(ka & kb) if a[k] != b[k]]
    print("compared %d COMDATs; DIFFERING: %d" % (len(ka & kb), len(differing)))
    for name in differing:
        print("    %s  (%d vs %d bytes)" % (name, len(a[name]), len(b[name])))
        bad = True

    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
