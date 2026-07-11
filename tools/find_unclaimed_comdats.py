#!/usr/bin/env python3
"""Find COMDATs our objects ALREADY EMIT that no `// FUNCTION: LOCO` marker claims, and
locate them in the image by relocation-masked byte comparison.

The producers are compiler-generated COMDATs with no source line of their own -- `??_G`
scalar-deleting-dtor thunks, `??1`/`??0` implicit dtors/ctors, `??_F` vector-ctor-iterator
callbacks, `??_D` vbase-dtor helpers.  cl emits them alongside a class's vtable, but the
LINKER is free to place them nowhere near the `??1` they call (RoadVehicleActor's thunk sits
0x1f000 bytes from its destructor), so they never surface as a neighbour of anything already
transcribed and stay unmarked indefinitely.

Claiming one is a COMMENT-ONLY change -- a stacked marker line above the destructor's own --
so it is byte-neutral by construction.  Modes:

  (default)   report unclaimed COMDATs with a UNIQUE byte match, ready to mark
  --thunks    report unclaimed 30-byte image functions of the call-through `??_G` shape,
              resolved to the destructor they call and the src/ file that claims it

⚠ TWO TRAPS, both hit for real in v539:

  * A marker whose `??_G` COMDAT does NOT exist in that TU does not simply fail -- match.py
    falls back to POSITIONAL pairing and mispairs the rest of the file (-2383 B in one TU,
    -961 B in another).  Always confirm with tools/cc.sh after adding markers.
  * Every 30-byte thunk is byte-identical to every other once relocations are masked, because
    the whole body is two relocated calls.  Bytes therefore CANNOT disambiguate them -- use
    --thunks, which discriminates on the call TARGET, and never trust a multi-hit row here.
"""
import sys, re, glob, os, struct, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match

EXE = os.path.join(ROOT, "loco/Loco.exe")
OBJDUMP = "/opt/homebrew/opt/binutils/bin/objdump"
OPERATOR_DELETE = 0x465CD0
APP_REGION_END = 0x463800


def image_reader():
    d = open(EXE, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    optsz = struct.unpack_from("<H", d, pe + 20)[0]
    base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
    secs = []
    for i in range(nsec):
        o = pe + 24 + optsz + i * 40
        vsz, va, rawsz, rawptr = struct.unpack_from("<IIII", d, o + 8)
        secs.append((base + va, vsz, rawptr))

    def read(va, n):
        for sva, vsz, rawptr in secs:
            if sva <= va < sva + vsz:
                return d[rawptr + (va - sva):rawptr + (va - sva) + n]
        return None
    return read


def markers():
    """address -> (relpath, first real declaration line after the marker)."""
    out = {}
    for f in sorted(glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True)):
        lines = open(f, errors="replace").read().split("\n")
        for i, ln in enumerate(lines):
            m = re.search(r"FUNCTION:\s*LOCO\s+0x0*([0-9a-fA-F]+)", ln)
            if not m:
                continue
            decl = ""
            for j in range(i + 1, min(i + 40, len(lines))):
                s = lines[j].strip()
                if s and not s.startswith("//"):
                    decl = s
                    break
            out[int(m.group(1), 16)] = (os.path.relpath(f, ROOT), decl)
    return out


def unclaimed_funcs(marked):
    rows = []
    for line in open(os.path.join(ROOT, "toolchain/test/app_funcs.txt")):
        p = line.split()
        if len(p) < 2:
            continue
        try:
            va, size = int(p[0], 16), int(p[1])
        except ValueError:
            continue
        if va < APP_REGION_END and va not in marked:
            rows.append((va, size))
    return rows


def report_thunks(marked, unclaimed):
    """30-byte call-through thunks, resolved by CALL TARGET rather than by bytes."""
    print("unclaimed 30-byte `??_G`-shaped thunks, by the destructor they call:\n")
    n = 0
    for va, size in sorted(unclaimed):
        if size != 30:
            continue
        out = subprocess.run([OBJDUMP, "-d", "-M", "intel",
                              "--start-address=0x%x" % va,
                              "--stop-address=0x%x" % (va + size), EXE],
                             capture_output=True, text=True).stdout
        calls = [int(t, 16) for t in re.findall(r"call\s+0x([0-9a-f]+)", out)
                 if int(t, 16) != OPERATOR_DELETE]
        if len(calls) != 1:
            continue
        f, decl = marked.get(calls[0], ("-- dtor UNCLAIMED too", ""))
        print("0x%06x -> dtor 0x%06x  %-30s %s" % (va, calls[0], f, decl))
        n += 1
    print("\n%d thunk(s); mark one with a stacked hint line above its destructor's own marker:" % n)
    print("  // FUNCTION: LOCO 0x<thunk> (??_G<Class> scalar deleting dtor -- compiler-generated)")


def report_unique(marked, unclaimed):
    read = image_reader()
    src_all = "\n".join(open(f, errors="replace").read()
                        for f in glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True))
    hits = []
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        obj = os.path.join(ROOT, "build", os.path.basename(cpp)[:-4] + ".obj")
        if not os.path.exists(obj):
            continue
        funcs = match.coff_functions(obj)
        paired = {nm for _, nm, _, _ in match.pair_by_name(open(cpp, errors="replace").read(), funcs)}
        for nm, code, relocs in funcs:
            if nm in paired or nm.split("@@")[0] in src_all:
                continue
            n = match.trim_pad(code)
            if n < 5:
                continue
            mine = match.mask(code, relocs, n)
            found = [va for va, _ in unclaimed
                     if (read(va, n) or b"") and len(read(va, n)) == n
                     and match.mask(read(va, n), relocs, n) == mine]
            if len(found) == 1:
                hits.append((found[0], n, os.path.basename(cpp), nm))
    print("%d image address(es) with a UNIQUE unclaimed-COMDAT byte match\n" % len({h[0] for h in hits}))
    for va, n, cpp, nm in sorted(hits):
        print("0x%06x  %4d B  %-28s %s" % (va, n, cpp, nm))
    print("\n⚠ a multi-hit COMDAT is NOT reported: every 30-byte thunk masks to the same bytes.")
    print("  Use --thunks for those -- it discriminates on the call target.")


if __name__ == "__main__":
    marked = markers()
    unclaimed = unclaimed_funcs(set(marked))
    if "--thunks" in sys.argv:
        report_thunks(marked, unclaimed)
    else:
        report_unique(marked, unclaimed)
