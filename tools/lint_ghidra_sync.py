#!/usr/bin/env python3
"""lint_ghidra_sync.py — check src/ names are in sync with the Ghidra DB.

The Ghidra project (ghidra-mcp at localhost:8089, program Loco.exe) and src/
carry the SAME names for the same addresses; a rename applied on one side but
not the other silently desyncs them (especially via file-local `extern` decls —
see tools/lint_idiom.py class I). This tool cross-checks every address-anchored
name in src/ against the live Ghidra DB:

  1. `// FUNCTION: LOCO 0x<addr>` markers — the function NAME in the definition
     that follows the marker must match Ghidra's (fully-qualified) name at that
     entry address. Markers with no source line of their own (compiler-generated
     `??_G` scalar dtors etc. — blank line follows) are skipped.
  2. Declarations tagged `// FUN_00xxxxxx` / `// DAT_00xxxxxx` — extern
     globals, extern function decls, and member-function prototypes whose
     trailing comment records the Ghidra address. The declared identifier must
     match Ghidra's symbol at that address.

Accept rules (per site, in order):
  - Ghidra fully-qualified name == src name           (methods: `Cls::Meth`)
  - FQN's last `::` component == src name             (free fn in a subsystem
    namespace, e.g. Ghidra `Wav::Wav_ParseAndLoad` vs src `Wav_ParseAndLoad`;
    member prototypes checked without class context)
  - FQN with `::` -> `_` == src name                  (boxed `DDraw::FUN_...`
    vs src `DDraw_FUN_...`)
  - data only: Ghidra name == the `DAT_00xxxxxx` from the comment itself
    (ALIAS — a raw struct-instance global can't carry a `g_` name in Ghidra,
    the set_global Hungarian linter has no prefix for "this IS the object";
    the src comment documents the alias, so it counts as in-sync)

Statuses: OK / ALIAS (in sync) — MISMATCH / MISSING / PARSEFAIL (findings,
exit 1 when UNTAGGED). MISSING = no function entry (markers) or no symbol
(decls) at the address; also fires when a marker's address is mid-function
(typo'd marker).

Tag: `// TODO: sync` on the checked line (the definition line under a
FUNCTION marker, or the declaration line itself) = known debt — counted
separately, doesn't fail the run. Use for the frozen phase2 probe TUs, whose
deliberately probe-local shortcut names (`Obj0x14::IsVal0x10One`) drifted
from Ghidra's later real names by design; burn down when those TUs are
consolidated. `--tag` appends the tag to every untagged finding.

This lint needs the LIVE Ghidra instance; when the server (or Loco.exe) is
unreachable it prints a warning and exits 0 so offline builds don't fail —
it is deliberately NOT wired into verify.py/cc.sh. `--strict` turns the
offline case into a hard failure (exit 2) — that's what the pre-commit hook
uses, so a commit can't silently skip the sync check just because Ghidra
isn't running. Run it after any Ghidra rename sweep too.

Usage:
  tools/lint_ghidra_sync.py             summary + untagged findings; exit 1 if any
  tools/lint_ghidra_sync.py --strict    also exit 2 when Ghidra is unreachable
  tools/lint_ghidra_sync.py --list      every checked site (incl. OK/ALIAS/tagged)
  tools/lint_ghidra_sync.py --tag       append '// TODO: sync' to untagged findings
  tools/lint_ghidra_sync.py src/Wav.cpp restrict to a file/dir
"""
import json
import re
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
GHIDRA = "http://localhost:8089"
PROGRAM = "Loco.exe"

MARKER_RX = re.compile(r"^\s*//\s*FUNCTION:\s*LOCO\s+0x([0-9a-fA-F]+)")
# the identifier heading a function definition/declaration: first `name(`
DEF_NAME_RX = re.compile(r"([A-Za-z_~][\w:~]*)\s*\(")
# a FUN_/DAT_ address token in a trailing comment (first one wins)
ADDR_COMMENT_RX = re.compile(r"\b(FUN_|DAT_)([0-9a-fA-F]{8})\b")
# a declaration's data identifier: last ident before optional [] and the ;
DATA_NAME_RX = re.compile(r"([A-Za-z_]\w*)\s*(?:\[[^\]]*\])*\s*(?:=[^;]*)?;")

CPP_KEYWORDS = {"if", "while", "for", "switch", "return", "sizeof", "catch",
                "throw", "new", "delete", "defined"}

TAG = "TODO: sync"


def strip_comments_file(path):
    """Yield (lineno, code, comment) per line, tracking /* */ and strings."""
    in_block = False
    for lineno, raw in enumerate(path.read_text().splitlines(), 1):
        code, comment = [], []
        in_str = None
        i, n = 0, len(raw)
        while i < n:
            c = raw[i]
            if in_block:
                comment.append(c)
                if raw.startswith("*/", i):
                    comment.append("/")
                    in_block = False
                    i += 2
                    continue
                i += 1
                continue
            if in_str:
                code.append(c)
                if c == "\\" and i + 1 < n:
                    code.append(raw[i + 1])
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
            if raw.startswith("//", i):
                comment.append(raw[i:])
                break
            if raw.startswith("/*", i):
                in_block = True
                comment.append("/*")
                i += 2
                continue
            code.append(c)
            i += 1
        yield lineno, "".join(code), "".join(comment)


def def_name(code):
    """Extract the defined/declared function name from a definition line."""
    for m in DEF_NAME_RX.finditer(code):
        name = m.group(1)
        last = name.rsplit("::", 1)[-1].lstrip("~")
        if last in CPP_KEYWORDS:
            continue
        return name
    return None


def collect_sites(paths):
    """Yield (path, lineno, tag_lineno, kind, addr, src_name, tagged) sites.

    kind: 'marker' (FUNCTION: LOCO), 'fn-decl' / 'data-decl' (addr comment).
    src_name None => PARSEFAIL; markers w/o a source line (??_G) are skipped.
    tag_lineno = the line `--tag` would append `// TODO: sync` to (the
    definition line for markers, the declaration line itself for decls).
    """
    files = sorted(SRC.rglob("*.cpp")) + sorted(SRC.rglob("*.h"))
    if paths:
        wanted = [Path(p).resolve() for p in paths]
        files = [f for f in files
                 if any(f.resolve() == w or w in f.resolve().parents
                        for w in wanted)]
    for path in files:
        lines = list(strip_comments_file(path))
        for idx, (lineno, code, comment) in enumerate(lines):
            m = MARKER_RX.match(comment) if not code.strip() else None
            if m:
                addr = int(m.group(1), 16)
                # find the definition line: skip comment-only lines; a blank
                # line or another marker first => no source line (??_G etc.)
                name, def_lineno, tagged = None, lineno, TAG in comment
                skipped = False
                for lno2, code2, comment2 in lines[idx + 1:]:
                    if not code2.strip():
                        if not comment2.strip() or MARKER_RX.match(comment2):
                            skipped = True
                            break
                        continue  # comment-only line
                    name = def_name(code2)
                    def_lineno = lno2
                    tagged = tagged or TAG in comment2
                    break
                else:
                    skipped = True
                if skipped:
                    continue
                yield path, lineno, def_lineno, "marker", addr, name, tagged
                continue
            # address-tagged declarations: code must be a declaration line
            am = ADDR_COMMENT_RX.search(comment)
            if not am or not code.strip():
                continue
            stripped = code.strip()
            if not stripped.endswith(";"):
                continue
            addr = int(am.group(2), 16)
            tagged = TAG in comment
            if "(" in stripped:
                # function decl/prototype (extern or member) — but not a
                # function POINTER (name inside parens) — keep those out
                if re.search(r"\(\s*\*", stripped):
                    continue
                yield path, lineno, lineno, "fn-decl", addr, \
                    def_name(stripped), tagged
            elif stripped.startswith("extern"):
                dm = DATA_NAME_RX.search(stripped)
                yield path, lineno, lineno, "data-decl", addr, \
                    (dm.group(1) if dm else None), tagged


def query_ghidra(addrs):
    """Batch-resolve addrs -> (kind, fully-qualified-name) via one
    run_script_inline call. kind: 'F' function entry, 'D' data/other symbol,
    'M' mid-function (inside a function body but not its entry), 'NONE'.
    Returns dict or None if the server/program is unreachable."""
    arr = ", ".join(f"0x{a:x}L" for a in sorted(addrs))
    code = (
        "long[] addrs = {%s};\n"
        "ghidra.program.model.listing.FunctionManager fm = currentProgram.getFunctionManager();\n"
        "ghidra.program.model.symbol.SymbolTable st = currentProgram.getSymbolTable();\n"
        "for (int i = 0; i < addrs.length; i++) {\n"
        "  ghidra.program.model.address.Address a = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(addrs[i]);\n"
        "  ghidra.program.model.listing.Function f = fm.getFunctionAt(a);\n"
        "  if (f != null) { println(\"SYNC \" + Long.toHexString(addrs[i]) + \" F \" + f.getSymbol().getName(true)); continue; }\n"
        "  ghidra.program.model.listing.Function fc = fm.getFunctionContaining(a);\n"
        "  if (fc != null) { println(\"SYNC \" + Long.toHexString(addrs[i]) + \" M \" + fc.getSymbol().getName(true)); continue; }\n"
        "  ghidra.program.model.symbol.Symbol s = st.getPrimarySymbol(a);\n"
        "  if (s != null) { println(\"SYNC \" + Long.toHexString(addrs[i]) + \" D \" + s.getName(true)); }\n"
        "  else { println(\"SYNC \" + Long.toHexString(addrs[i]) + \" NONE -\"); }\n"
        "}\n"
        "println(\"SYNC-DONE\");\n" % arr)
    req = urllib.request.Request(
        f"{GHIDRA}/run_script_inline?program={PROGRAM}",
        data=json.dumps({"code": code}).encode(),
        headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=120) as resp:
            text = resp.read().decode(errors="replace")
    except OSError as e:
        print(f"WARNING: Ghidra unreachable ({e}) — sync check skipped", file=sys.stderr)
        return None
    # the response echoes stale compile errors from old cached scripts —
    # only trust our own SYNC lines + the SYNC-DONE marker
    if "SYNC-DONE" not in text:
        print("ERROR: script did not complete (no SYNC-DONE marker) — is "
              f"{PROGRAM} open in Ghidra? Raw tail:\n" + text[-500:], file=sys.stderr)
        sys.exit(2)
    out = {}
    for line in text.splitlines():
        m = re.match(r"^SYNC ([0-9a-f]+) (F|M|D|NONE) (\S+)$", line)
        if m:
            out[int(m.group(1), 16)] = (m.group(2), m.group(3))
    return out


def judge(kind, src_name, ghidra):
    """Return (status, ghidra_name). Statuses: OK ALIAS MISMATCH MISSING PARSEFAIL."""
    if src_name is None:
        return "PARSEFAIL", ghidra[1] if ghidra else "-"
    gkind, gname = ghidra if ghidra else ("NONE", "-")
    if gkind == "NONE" or (kind == "marker" and gkind != "F") or \
            (kind == "fn-decl" and gkind not in ("F",)) :
        return "MISSING", gname
    last = gname.rsplit("::", 1)[-1]
    if gname == src_name or last == src_name or \
            gname.replace("::", "_") == src_name:
        return "OK", gname
    if kind == "data-decl" and gkind == "D" and re.fullmatch(r"DAT_[0-9a-f]{8}", gname):
        # documented alias: raw struct-instance globals keep their DAT_ name
        # in Ghidra (no Hungarian prefix fits "this IS the object")
        return "ALIAS", gname
    return "MISMATCH", gname


def main():
    import argparse
    ap = argparse.ArgumentParser(description="src/ <-> Ghidra name-sync lint")
    ap.add_argument("paths", nargs="*", help="restrict to these files/dirs")
    ap.add_argument("--list", action="store_true", dest="list_mode",
                    help="print every checked site, including OK/ALIAS/tagged")
    ap.add_argument("--tag", action="store_true",
                    help="append '// TODO: sync' to untagged finding lines")
    ap.add_argument("--strict", action="store_true",
                    help="treat an unreachable Ghidra server as a failure "
                         "(exit 2) instead of a warn-and-skip")
    a = ap.parse_args()

    sites = list(collect_sites(a.paths))
    if not sites:
        print("no checkable sites found")
        return 0
    addrs = {ad for _p, _l, _tl, _k, ad, _nm, _tg in sites}
    resolved = query_ghidra(addrs)
    if resolved is None:
        if a.strict:
            print("ERROR (--strict): the sync check REQUIRES the live Ghidra "
                  "instance — start ghidra-mcp with Loco.exe open, or bypass "
                  "the commit gate explicitly with `git commit --no-verify`.",
                  file=sys.stderr)
            return 2
        return 0  # offline — documented no-op

    counts = {}
    untagged = []          # rows
    tag_targets = {}       # path -> [tag_lineno]
    rows = []
    for path, lineno, tag_lineno, kind, addr, src_name, tagged in sites:
        status, gname = judge(kind, src_name, resolved.get(addr))
        is_finding = status not in ("OK", "ALIAS")
        key = status if not (is_finding and tagged) else "TODO"
        counts[key] = counts.get(key, 0) + 1
        rel = path.relative_to(ROOT)
        mark = " " if (tagged or not is_finding) else "!"
        row = (f"{mark} {status:<9} {rel}:{lineno} [{kind}] 0x{addr:x} "
               f"src={src_name or '?'} ghidra={gname}")
        rows.append(row)
        if is_finding and not tagged:
            untagged.append(row)
            tag_targets.setdefault(path, []).append(tag_lineno)

    if a.tag:
        n = 0
        for path, linenos in tag_targets.items():
            lines = path.read_text().splitlines(keepends=True)
            for lno in linenos:
                body = lines[lno - 1].rstrip("\n")
                lines[lno - 1] = body + "  // " + TAG + "\n"
                n += 1
            path.write_text("".join(lines))
        print(f"tagged {n} lines")
        return 0

    for row in (rows if a.list_mode else untagged):
        print(row)
    print(f"\n{len(sites)} sites checked: " +
          ", ".join(f"{k}={v}" for k, v in sorted(counts.items())) +
          f"  ({len(untagged)} UNTAGGED findings)")
    return 1 if untagged else 0


if __name__ == "__main__":
    sys.exit(main())
