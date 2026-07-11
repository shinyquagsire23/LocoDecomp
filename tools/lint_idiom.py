#!/usr/bin/env python3
"""lint_idiom.py — find non-idiomatic C++ (raw offset math) in src/.

The standing rule (CLAUDE.md): matched C++ must be REAL, idiomatic source —
`z->width`, not `*(short*)((char*)z + 0xc)`. Raw offset access means a struct
field is still unmodeled. This tool catalogs the debt so cleanup sessions can
burn it down, and gates new code (untagged findings = failure).

Adapted for this repo: flat src/ layout (a class's home is the same-STEM file, 
src/<Class>.h/.cpp, not a per-TU directory), no `C`-prefix class-name convention 
(any uppercase-initial name), and NO vtable-slot-1 exemption.

Pattern classes:
  A  char-cast offset arithmetic:      (char*)p + 0x58   /  (char*)p + 8
     — including the scaled form:      (char*)p + idx * 0x18
     (an index expression in front of the magic number is the same raw
     byte-offset math, just better hidden)
  B  deref of raw pointer + offset:    *(int*)(p + 0x438)
  C  offset math on Unk pointer field: p->pUnk0x44 + 0xa8
  D  read/write through a pad name:    pApp->pad0x8[0] = 0
     (declaring `char pad0x8[8];` is fine — ACCESS through it means the field
     wearing a padding name is real and still unmodeled)
  E  duplicate class definition: a class/struct DEFINED (with body) outside its
     home file (src/<Class>.h or .cpp), when >=2 definitions exist repo-wide.
     Per-TU "local shortcut models" are the sizeof-drift regression hazard;
     consolidate toward one canonical model per class. A canonical-by-convention
     site in a sibling's header gets '// idiom-exempt: canonical'.
  F  raw vtable-slot dispatch:          (*(VFn13**)pObj)[13](pObj)
     = an unmodeled VIRTUAL METHOD (engine classes: model the virtual; COM
     interfaces (DirectDraw/DirectSound/DirectPlay): use the real SDK headers'
     interface, never a hand-rolled slot call).
     ALSO F: any raw cast of `this`:     ((T*)this)->Method(...)
     — the "probe struct" evasion of the slot dispatch above: the slot math is
     just hidden inside the probe type's layout instead of written at the call
     site. Same debt, same fix: declare the virtual on the class's real vtable
     (in its header) and call it by name.
  G  function-local typedef in a .cpp:  typedef void (*VFn13)(void*);
     (an indented `typedef` inside a function body — hoist it to the TU header;
     purely compile-time, so the hoist is always codegen-neutral. Most are the
     VFn companions of class-F dispatches and retire together with them.)
  H  memcpy/memset/memmove whose size argument has no `sizeof`:
     memset(p, 0, 0x100)  — HIGH PRIORITY. A bare byte-count magic number is
     exactly as fragile as a raw struct offset (silently wrong if the real
     struct/array resizes); prefer `sizeof(*p)`/`sizeof(SomeType)`. Declarations
     (`void* memcpy(void*, const void*, unsigned long);`) are not flagged. A
     genuinely runtime-computed length (e.g. `width * height`-derived) has no
     compile-time size to name — those get `// idiom-exempt: runtime length`.
  I  extern "C" linkage in a .cpp:  extern "C" { ... }  /  extern "C" int g_x;
     File-local extern declarations dodge the shared subsystem header, so two
     TUs can declare the SAME global/function under different names or types
     and silently desync from each other and from the Ghidra DB when a symbol
     is renamed (tools/lint_ghidra_sync.py catches the Ghidra side of that
     drift). Declare in the shared header instead — the header itself may use
     extern "C" freely (the C linkage is often load-bearing under /GX: calls
     to extern "C" functions are treated as non-throwing, see src/DSound.cpp's
     file-header notes — this class is about WHERE the decl lives, not about
     the linkage itself).

Severity: A-E, H-I > F-G. A-D (raw offsets), E (duplicate models), H
(memcpy/memset sizing), and I (file-local extern decls) are NON-LOCAL hazards
or correctness smells — a wrong hand-computed offset/size, a drifted sizeof,
or a desynced per-TU extern decl silently regresses OTHER functions/TUs or
masks a wrong-size bug. F-G are self-contained: the slot index / typedef is
local to the call site and that site's own byte-match verifies it, so they're
idiom debt but not a correctness risk. Clean up A-E and H-I first.

Not flagged (established idioms): (char*)"literal" casts, *(const float*)&x
bit-reinterprets, new char[] buffers, `// sic` lines still need a tag (sic
documents original-engine behavior, not our modeling debt — they're separate).

Tags:
  // TODO: idiom   — known debt (counted separately; the cleanup backlog)
  // idiom-exempt  — reviewed, deliberately kept (skipped entirely; add a reason)

⚠ **A raw hex/decimal offset (classes A/B/C) is NEVER exempt-able.** `+ 0x30`-
style pointer arithmetic always means a struct field is unmodeled — there is
no legitimate reason to leave it as magic-number pointer math permanently.
`// idiom-exempt` on a line matching A/B/C is IGNORED (the finding still
fires, and still needs a real fix or at worst a `// TODO: idiom` backlog tag).
The fix is ALWAYS a real struct field access — `this->field` — never a
literal `+ 0x30`, and never a `((OtherType*)this)` cast either (ANY raw cast
of `this` is now a class-F finding — for a shared/embedded object, give it a
named typed MEMBER and go through that):
  - if the field is safe to model as a genuine TYPED member, use
    `&this->field` (ordinary member access), or
  - if it must stay untyped/raw (an offset that doesn't cleanly map to one
    named field yet), give it its OWN exactly-positioned `Unk0xNN`/`UnkNN`
    placeholder field (per CLAUDE.md's uncertainty ladder — known size,
    unknown purpose is a legitimate rung) and reference THAT field directly,
    not `(char*)p + 0xNN`.
  `offsetof(...)` is reserved for ONE narrow case: naming the SIZE argument of
  a class-H memcpy/memset/memmove call (see class H above) when the copied
  region starts at a named field but doesn't align with one single field's
  own address — e.g. `memset((char*)this + offsetof(T, tailField), 0,
  sizeof(T) - offsetof(T, tailField))`. It is NOT a general substitute for
  `&this->field` outside that context — if a plain named-field reference
  already gives the exact address you need (the common case), use that
  instead of reaching for `offsetof`.

Usage:
  tools/lint_idiom.py                      table per file + totals; exit 1 if UNTAGGED
  tools/lint_idiom.py src/LocoBitmap.cpp   LIST that file/dir's findings (path => list mode)
  tools/lint_idiom.py --only EF [path...]  restrict to classes (e.g. E+F only)
  tools/lint_idiom.py --list               every finding, file:line: [classes] code
  tools/lint_idiom.py --doc                markdown catalog (for docs/IDIOM_CLEANUP.md)
  tools/lint_idiom.py --tag [path...]      append '// TODO: idiom' to untagged lines
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

NUM = r"(?:0x[0-9a-fA-F]+|\d+)"

# Classes whose match is a raw hex/decimal offset — CLAUDE.md/this tool's policy:
# these can never be silenced by `// idiom-exempt`, only by an actual fix (or,
# failing that, `// TODO: idiom` backlog tagging).
OFFSET_CLASSES = {"A", "B", "C"}

PATTERNS = [
    # A: (char*)expr + numeric-offset (any deref/address-of context); the offset
    # may be scaled: `+ idx * 0x18` / `+ (short)p->idx * 0x18` is the same raw
    # byte-offset math with an index expression in front of the magic number.
    ("A", re.compile(
        r"\(\s*(?:unsigned\s+|signed\s+)?char\s*\*\s*\)\s*"   # (char*) cast
        r"[&(]*[A-Za-z_][\w.>()-]*\s*"                        # the base expr
        r"\+\s*(?:(?:\([^()]*\)\s*)?[\w.>()-]*\s*\*\s*)?" + NUM)),
    # B: *(T*)(ident + numeric-offset)
    ("B", re.compile(
        r"\*\s*\(\s*[A-Za-z_][\w ]*\*+\s*(?:const\s*)?\)\s*"  # *(T*) / *(T**)
        r"\(\s*[A-Za-z_][\w.>-]*\s*\+\s*" + NUM + r"\s*\)")),
    # C: ->pUnk.../->Unk... + offset  (pointer field to an unmodeled struct)
    ("C", re.compile(
        r"(?:->|\.)\s*p?\w*Unk\w*\s*\+\s*" + NUM)),
    # D: access through a pad-named field (Loco convention pad0xNN; also _pad)
    ("D", re.compile(
        r"(?:->|\.)\s*_?pad(?:0x[0-9a-fA-F]+|\w*)")),
]

# F: (*(T**)expr)[N](...) — raw vtable-slot call (every slot; no MSVC slot-1
# deleting-dtor idiom here)
VTBL_RX = re.compile(
    r"\(\s*\*\s*\(\s*[A-Za-z_]\w*\s*\*\*\s*\)\s*[^)]*\)\s*\[\s*(\d+)\s*\]\s*\(")

# F (cont.): ((T*)this) — a raw cast of `this` to a per-TU "probe" type
# (e.g. `((TutorialWndVtblProbe *)this)->RefreshClientRect();`). This is the
# same unmodeled-virtual debt as a raw slot dispatch, with the slot math
# hidden inside the probe struct's layout instead of written at the call
# site — model the virtual on the class's REAL vtable and call it by name.
THIS_CAST_RX = re.compile(
    r"\(\s*\(\s*(?:const\s+)?[A-Za-z_]\w*\s*\*+\s*\)\s*this\s*\)")

# H: memcpy/memset/memmove — flag if the SIZE (last) arg has no `sizeof`.
MEMFUNC_CALL_RX = re.compile(r"(?<!\w)(?:__builtin_)?mem(?:cpy|set|move)\s*\(")

# I: extern "C" linkage (block opener or one-line decl) — .cpp files only;
# shared headers are the right home for cross-TU extern decls.
EXTERN_C_RX = re.compile(r'extern\s*"C"')

TAG = "TODO: idiom"
EXEMPT = "idiom-exempt"


def _split_top_level_args(s):
    """Split a parenthesized call's argument-list text on top-level commas."""
    args, depth, cur = [], 0, []
    for ch in s:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            args.append("".join(cur))
            cur = []
        else:
            cur.append(ch)
    if cur or not args:
        args.append("".join(cur))
    return args


def has_unsized_memfunc_call(code):
    """True if a mem*() CALL (not a declaration/prototype) on this line has a
    size argument that doesn't mention `sizeof`. Multi-line calls (unbalanced
    parens on this one line) are skipped — can't safely analyze in isolation."""
    for m in MEMFUNC_CALL_RX.finditer(code):
        # A declaration/prototype has the return type `void *` directly before
        # the function name (this repo's established extern-decl style); a
        # call never does (the name starts the statement or follows an
        # operator/keyword instead).
        if re.search(r"void\s*\*\s*$", code[:m.start()]):
            continue
        depth = 1
        i = m.end()
        while i < len(code) and depth > 0:
            if code[i] == "(":
                depth += 1
            elif code[i] == ")":
                depth -= 1
            i += 1
        if depth != 0:
            continue  # unbalanced on this line -- multi-line call, skip
        args = _split_top_level_args(code[m.end():i - 1])
        if len(args) < 2:
            continue
        if "sizeof" not in args[-1]:
            return True
    return False


def strip_comments(line, in_block):
    """Single pass over one line tracking strings, //, and /* */.

    Returns (code, comment, in_block): code = non-comment text, comment =
    everything from the first comment marker on, in_block = carry state.
    """
    code, comment = [], []
    in_str = None
    i, n = 0, len(line)
    while i < n:
        c = line[i]
        if in_block:
            comment.append(c)
            if line.startswith("*/", i):
                comment.append("/")
                in_block = False
                i += 2
                continue
            i += 1
            continue
        if in_str:
            code.append(c)
            if c == "\\" and i + 1 < n:
                code.append(line[i + 1])
                i += 2
                continue
            if c == in_str:
                in_str = None
            i += 1
            continue
        if c in "\"'":
            in_str = c
            code.append(c)
            i += 1
            continue
        if line.startswith("//", i):
            comment.append(line[i:])
            break
        if line.startswith("/*", i):
            in_block = True
            comment.append("/*")
            i += 2
            continue
        code.append(c)
        i += 1
    return "".join(code), "".join(comment), in_block


def scan_file(path):
    """Yield (lineno, classes, code, tagged) for each finding line."""
    in_block = False
    for lineno, raw in enumerate(path.read_text().splitlines(), 1):
        code, comment, in_block = strip_comments(raw, in_block)
        if not code.strip():
            continue
        classes = {cls for cls, rx in PATTERNS if rx.search(code)}
        if VTBL_RX.search(code) or THIS_CAST_RX.search(code):
            classes.add("F")
        if path.suffix == ".cpp" and re.match(r"\s+typedef\b", code):
            classes.add("G")
        if has_unsized_memfunc_call(code):
            classes.add("H")
        if path.suffix == ".cpp" and EXTERN_C_RX.search(code):
            classes.add("I")
        if not classes:
            continue
        # `// idiom-exempt` silences a finding EXCEPT a raw-offset class (A/B/C)
        # — those are never exempt-able (CLAUDE.md policy; see module docstring).
        if EXEMPT in comment and not (classes & OFFSET_CLASSES):
            continue
        yield lineno, sorted(classes), raw, (TAG in comment)


# a class/struct DEFINITION line (body follows), uppercase-initial names only
DEF_RX = re.compile(r"^\s*(?:typedef\s+)?(?:struct|class)\s+([A-Z]\w*)\b(.*)$")


def scan_defs(path):
    """Yield (lineno, name, raw, tagged, exempt) for class/struct definitions."""
    in_block = False
    for lineno, raw in enumerate(path.read_text().splitlines(), 1):
        code, comment, in_block = strip_comments(raw, in_block)
        m = DEF_RX.match(code)
        if not m:
            continue
        rest = m.group(2).strip()
        if rest.endswith(";"):          # forward decl / variable decl
            continue
        if rest and not (rest.startswith("{") or rest.startswith(":")):
            continue
        yield lineno, m.group(1), raw, (TAG in comment), (EXEMPT in comment)


def is_home(path, name):
    """True if this file is the class's own home file (flat src/ layout:
    src/<Class>.h or src/<Class>.cpp)."""
    return path.stem == name


def collect_dups():
    """Class-E findings: non-home, non-exempt definition sites of classes that
    are defined 2+ times repo-wide. Returns {path: [(lineno, raw, tagged)]},
    plus {name: [all site strings]} for reporting."""
    defs = {}  # name -> [(path, lineno, raw, tagged, exempt)]
    for path in sorted(SRC.rglob("*.h")) + sorted(SRC.rglob("*.cpp")):
        for lineno, name, raw, tagged, exempt in scan_defs(path):
            defs.setdefault(name, []).append((path, lineno, raw, tagged, exempt))
    findings, sites_by_name = {}, {}
    for name, sites in defs.items():
        if len(sites) < 2:
            continue
        sites_by_name[name] = [f"{p.relative_to(ROOT)}:{ln}" for p, ln, *_ in sites]
        for path, lineno, raw, tagged, exempt in sites:
            if exempt or is_home(path, name):
                continue
            findings.setdefault(path, []).append((lineno, raw, tagged))
    return findings, sites_by_name


def collect():
    findings = {}  # path -> list of (lineno, classes, raw, tagged)
    for path in sorted(SRC.rglob("*.cpp")) + sorted(SRC.rglob("*.h")):
        rows = list(scan_file(path))
        if rows:
            findings[path] = rows
    dup_findings, _ = collect_dups()
    for path, rows in dup_findings.items():
        merged = findings.setdefault(path, [])
        merged += [(lineno, ["E"], raw, tagged) for lineno, raw, tagged in rows]
        merged.sort(key=lambda r: r[0])
    return findings


def suggest(classes):
    """A one-line fix hint for classes where the tool can suggest something
    concrete (raw-offset A/B/C; the memcpy/memset-sizing class H)."""
    s = set(classes)
    if s & OFFSET_CLASSES:
        return ("suggest: &this->field (real typed member), or a properly-sized "
                "Unk0xNN/UnkNN placeholder field referenced directly -- not "
                "offsetof() (reserved for memcpy/memset size args only)")
    if "H" in s:
        return "suggest: sizeof(*dst)/sizeof(SomeType) instead of the bare byte count"
    if "F" in s:
        return ("suggest: model the virtual on the class's real vtable and call "
                "it by name — not a raw slot index, not a ((ProbeT*)this) cast")
    if "I" in s:
        return ("suggest: move the extern decls to the shared subsystem header "
                "(extern \"C\" there is fine) so every TU + Ghidra share one name")
    return None


def filter_findings(findings, paths, only):
    """Restrict to files under any of `paths` and to finding classes in `only`."""
    if paths:
        wanted = [Path(p).resolve() for p in paths]
        findings = {f: rows for f, rows in findings.items()
                    if any(f.resolve() == w or w in f.resolve().parents
                           for w in wanted)}
    if only:
        keep = set(only.upper())
        findings = {f: r for f, rows in findings.items()
                    if (r := [row for row in rows if keep & set(row[1])])}
    return findings


def main():
    import argparse
    ap = argparse.ArgumentParser(
        description="non-idiomatic C++ lint (see module docstring for classes)")
    ap.add_argument("paths", nargs="*",
                    help="restrict to these files/dirs (e.g. src/LocoBitmap.cpp); "
                         "with a path and no mode flag, findings are LISTED")
    ap.add_argument("--list", action="store_true", dest="list_mode",
                    help="print every finding as file:line: [classes] code")
    ap.add_argument("--doc", action="store_true",
                    help="markdown catalog (for docs/IDIOM_CLEANUP.md)")
    ap.add_argument("--tag", action="store_true",
                    help="append '// TODO: idiom' to untagged finding lines")
    ap.add_argument("--only", metavar="CLASSES", default="",
                    help="restrict to finding classes, e.g. --only EF")
    a = ap.parse_args()
    mode = ("--tag" if a.tag else "--doc" if a.doc else
            "--list" if a.list_mode or a.paths or a.only else "")

    findings = filter_findings(collect(), a.paths, a.only)
    total = sum(len(v) for v in findings.values())
    untagged = sum(1 for v in findings.values() for r in v if not r[3])

    if mode == "--tag":
        n = 0
        for path, rows in findings.items():
            lines = path.read_text().splitlines(keepends=True)
            for lineno, _classes, _raw, tagged in rows:
                if tagged:
                    continue
                idx = lineno - 1
                body = lines[idx].rstrip("\n")
                lines[idx] = body + "  // " + TAG + "\n"
                n += 1
            path.write_text("".join(lines))
        print(f"tagged {n} lines (of {total} findings)")
        return 0

    if mode == "--doc":
        print("# Non-idiomatic C++ cleanup catalog")
        print()
        print("Generated by `tools/lint_idiom.py --doc`. Raw offset access =")
        print("an unmodeled struct field; each fix is: model the field (src/ header")
        print("+ Ghidra struct), rewrite the access, confirm `tools/progress.py`")
        print("shows no regression. Classes: A=(char*)+off (incl. scaled `+ idx*0x18`)")
        print("B=*(T*)(p+off)")
        print("C=Unk-ptr+off  D=pad-field access  E=duplicate class definition")
        print("(consolidate to the canonical model; see the section below)")
        print("F=raw vtable-slot dispatch or any raw cast of `this` ((T*)this)->M()")
        print("(= an unmodeled virtual)")
        print("G=function-local typedef in a .cpp (hoist to the TU header)")
        print("H=memcpy/memset/memmove with no `sizeof` in the size arg (prefer")
        print("sizeof(*dst)/sizeof(T) over a bare byte count)")
        print("I=extern \"C\" in a .cpp (file-local extern decls — move to the")
        print("shared subsystem header so every TU + Ghidra share one name).")
        print()
        print("**Severity: A-E and H-I first** — those are non-local hazards or")
        print("correctness smells (a wrong offset/size or drifted sizeof silently")
        print("regresses OTHER TUs, or masks a wrong-size bug). F-G are")
        print("self-contained (the site's own byte-match verifies the slot);")
        print("idiom debt only, clean up last or opportunistically.")
        print()
        print("⚠ **A/B/C (raw offset) findings can NEVER be `// idiom-exempt`'d** —")
        print("only fixed (`&this->field`, or a properly-sized/positioned named")
        print("`Unk0xNN`/`UnkNN` placeholder field for an offset that doesn't map")
        print("to one field yet — see the module docstring; `offsetof` is reserved")
        print("for memcpy/memset size args, not a general substitute) or, failing")
        print("that, tagged `// TODO: idiom`.")
        print()
        print(f"**{total} findings across {len(findings)} files** "
              f"({untagged} untagged).")
        print()
        _, sites_by_name = collect_dups()
        if sites_by_name:
            print(f"## Duplicate class definitions ({len(sites_by_name)} names)")
            print()
            print("Consolidation targets — each class should end with ONE")
            print("canonical model (in its home file src/<Class>.h, or a site")
            print("marked `// idiom-exempt: canonical`); all other definitions")
            print("are class-E findings in the per-file lists below.")
            print()
            for name, sites in sorted(sites_by_name.items(),
                                      key=lambda kv: -len(kv[1])):
                print(f"- **{name}** ({len(sites)}): " +
                      ", ".join(f"`{s}`" for s in sites))
            print()
        for path, rows in sorted(findings.items(), key=lambda kv: -len(kv[1])):
            rel = path.relative_to(ROOT)
            print(f"## {rel} ({len(rows)})")
            print()
            for lineno, classes, raw, _tagged in rows:
                snippet = raw.strip()
                if len(snippet) > 100:
                    snippet = snippet[:97] + "..."
                hint = suggest(classes)
                hint_s = f" — {hint}" if hint else ""
                print(f"- `{rel}:{lineno}` [{','.join(classes)}] `{snippet}`{hint_s}")
            print()
        return 0

    if mode == "--list":
        for path, rows in sorted(findings.items()):
            rel = path.relative_to(ROOT)
            for lineno, classes, raw, tagged in rows:
                mark = " " if tagged else "!"
                hint = suggest(classes)
                hint_s = f"  — {hint}" if hint else ""
                print(f"{mark} {rel}:{lineno}: [{','.join(classes)}] {raw.strip()}{hint_s}")
        print(f"\n{total} findings, {untagged} untagged ('!')")
        return 1 if untagged else 0

    # default: per-file table
    print(f"{'file':<32} {'A':>4} {'B':>4} {'C':>4} {'D':>4} {'E':>4} {'F':>4} {'G':>4} {'H':>4} {'I':>4} {'tot':>5} {'untag':>6}")
    for path, rows in sorted(findings.items(), key=lambda kv: -len(kv[1])):
        rel = str(path.relative_to(ROOT))
        by = {c: 0 for c in "ABCDEFGHI"}
        un = 0
        for _lineno, classes, _raw, tagged in rows:
            for c in classes:
                by[c] += 1
            if not tagged:
                un += 1
        print(f"{rel:<32} {by['A']:>4} {by['B']:>4} {by['C']:>4} {by['D']:>4}"
              f" {by['E']:>4} {by['F']:>4} {by['G']:>4} {by['H']:>4} {by['I']:>4} {len(rows):>5} {un:>6}")
    print(f"\nTOTAL: {total} findings in {len(findings)} files; "
          f"{untagged} UNTAGGED (must be 0 before commit)")
    return 1 if untagged else 0


if __name__ == "__main__":
    sys.exit(main())
