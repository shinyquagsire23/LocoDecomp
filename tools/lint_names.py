#!/usr/bin/env python3
"""lint_names.py — catalog uncertainty-ladder NAMING debt in src/.

The uncertainty ladder (CLAUDE.md "Naming conventions"): recovered names climb
`FUN_<addr>`/`DAT_<addr>` (unread) -> `...Related` (touches a known subsystem) ->
`...Maybe` (behavior clear, purpose hypothesized) -> a certain name. This tool
surfaces every rung still below "certain" so a naming grind can burn it down,
the same way `tools/lint_idiom.py` tracks raw-offset debt. It is INFORMATIONAL
(a burn-down tracker) — it never fails a build.

What it flags, in SEVERITY order (highest first):
  U   FUN_/DAT_ identifier      — wholly unresolved (an unread function body or
                                  an unnamed global); `FUN_0042c3d0`, `DAT_00485254`,
                                  `thunk_FUN_...`. Highest priority: no behavior
                                  recovered at all.
  FM  function name w/ Maybe/Related  — a named function whose PURPOSE is still
                                  hypothesized (`BuildPaletteLUTMaybe`, method or
                                  free). Detected by a `(` following the name.
  GM  global variable w/ Maybe/Related — a `g_`-prefixed or `extern`-declared
                                  file-scope datum (`g_pSharedPaletteMaybe`).
  MM  member (struct/class field) w/ Maybe/Related — a field inside a type body
                                  (`int nCategoryMaybe;`, `bValidMaybe`).
  TM  type name w/ Maybe/Related — a struct/class/union whose identity is still
                                  hypothesized (`struct IniFileMaybe`). Added as a
                                  5th category (below the four above) so a type
                                  name isn't miscounted as the global/function that
                                  merely USES it (`extern IniFileMaybe *g_p;`, a
                                  ctor `IniFileMaybe(...)`); a type registry folds
                                  every def/use/ctor of one type into a single TM.
  UF  Unk<hex/dec> field DEFINITION — a placeholder-named struct/class member
                                  (known offset/size, unknown purpose): `Unk0xb0`,
                                  `bUnk11`. LOWEST priority, reported separately
                                  from U/FM/GM/MM/TM above.
                                  DECLARATIONS only — a field's own name
                                  inside its struct/class body; a `->Unk0xb0` USE
                                  elsewhere is not itself a new naming-debt site
                                  (lint_idiom.py's class C already flags Unk-field
                                  USES that do raw offset math on top).

Names WITHOUT Maybe/Related, WITHOUT a FUN_/DAT_ prefix, and WITHOUT an Unk<hex/dec>
field name are considered certain and are not flagged. Debt is counted by DISTINCT
NAME per category (one name = one thing to resolve), not per occurrence — a
function called from ten sites is one FM finding, listing every site.

Names appearing only inside comments or string literals are stripped before
scanning and never flagged.

Usage:
  tools/lint_names.py                 summary table + per-category totals
  tools/lint_names.py --list          every distinct name, severity-ordered, w/ sites
  tools/lint_names.py src/Foo.cpp     the distinct names that appear in that file/dir
  tools/lint_names.py --dashboard     one-line total (for progress.py)
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
sys.path.insert(0, str(Path(__file__).resolve().parent))
from lint_idiom import strip_comments  # comment-aware line stripper (keeps strings)

# Severity-ordered categories (highest first) and their one-line descriptions.
# UF sits LAST (lowest priority) -- adapted to Loco's Unk0xNN/UnkNN convention 
# (see this file's docstring).
CATS = [
    ("U", "FUN_/DAT_ unresolved (unread function / unnamed global)"),
    ("FM", "function name with Maybe/Related (purpose hypothesized)"),
    ("GM", "global variable with Maybe/Related"),
    ("MM", "struct/class member with Maybe/Related"),
    ("TM", "type (struct/class) name with Maybe/Related"),
    ("UF", "Unk<hex/dec> struct/class field DEFINITION (placeholder name)"),
]
CAT_ORDER = {c: i for i, (c, _) in enumerate(CATS)}

# struct/class/union NAME at a definition or forward-decl (uppercase or not).
TYPEDEF_RX = re.compile(r"\b(?:struct|class|union)\s+([A-Za-z_]\w*)")

# One combined token scan so brace/scope state and candidate names are processed
# strictly in source-position order (a `{` mid-line changes the scope of names
# after it on the same line).
TOKEN_RX = re.compile(
    r"(?P<open>\{)"
    r"|(?P<close>\})"
    r"|(?P<semi>;)"
    r"|(?P<kw>\b(?:struct|class|union)\b)"
    # Optional `Subsystem_` prefix (e.g. `DDraw_FUN_00401000`, a cross-TU opaque
    # helper kept in a subsystem namespace per CLAUDE.md's naming convention) --
    # without this, the bare `\b` anchor never fires between the prefix's own
    # trailing `_` and `FUN`/`DAT` (both word characters, no boundary between
    # them), so such names silently escaped U-tier debt counting entirely.
    r"|(?P<fun>\b(?:[A-Za-z]\w*_)?(?:thunk_)?(?:FUN|DAT)_[0-9a-fA-F]{3,}\b)"
    r"|(?P<ident>\b[A-Za-z_]\w*(?:Maybe|Related)\w*\b)"
    # UF: an Unk<hex/dec> placeholder name (`Unk0xb0`, `bUnk11`) NOT already caught
    # above by `ident` (a name with a Maybe/Related suffix wins there first, since
    # it's tried earlier in this alternation) -- requires a digit run immediately
    # after "Unk" so COM `IUnknown`/`pUnkOuter`-style names never match.
    r"|(?P<unk>\b\w*Unk(?:0x[0-9a-fA-F]+|\d+)\w*\b)"
)


def _blank_literals(code):
    """Blank string/char literal CONTENTS (strip_comments keeps them in `code`)."""
    code = re.sub(r'"(?:\\.|[^"\\])*"', '""', code)
    code = re.sub(r"'(?:\\.|[^'\\])*'", "''", code)
    return code


def _code_lines(path):
    """Yield (lineno, comment-stripped, literal-blanked) code lines."""
    in_block = False
    for lineno, raw in enumerate(path.read_text().splitlines(), 1):
        code, _comment, in_block = strip_comments(raw, in_block)
        yield lineno, _blank_literals(code)


def collect_type_names(files):
    """Repo-wide set of struct/class/union names containing Maybe/Related."""
    names = set()
    for path in files:
        for _lineno, code in _code_lines(path):
            for m in TYPEDEF_RX.finditer(code):
                if "Maybe" in m.group(1) or "Related" in m.group(1):
                    names.add(m.group(1))
    return names


def scan_file(path, type_names):
    """Yield (category, name, lineno) for every naming-debt token in one file.

    `type_names` folds a type's definition, uses, and constructor into one TM so
    an `extern IniFileMaybe *g_p;` or a `IniFileMaybe(...)` ctor is not miscounted
    as a global/function that merely names the type.
    """
    scope_is_type = []      # one bool per open brace: True = inside a struct/class body
    pending_type = False    # saw struct/class/union, awaiting its opening brace
    for lineno, code in _code_lines(path):
        for m in TOKEN_RX.finditer(code):
            if m.lastgroup == "kw":
                pending_type = True
            elif m.lastgroup == "open":
                scope_is_type.append(pending_type)
                pending_type = False
            elif m.lastgroup == "close":
                if scope_is_type:
                    scope_is_type.pop()
            elif m.lastgroup == "semi":
                pending_type = False   # forward decl / struct-typed var: no body
            elif m.lastgroup == "fun":
                yield "U", m.group(), lineno
            elif m.lastgroup == "ident":
                name = m.group()
                if name in type_names:
                    yield "TM", name, lineno          # def, use, or ctor of a type
                    continue
                nxt = code[m.end():].lstrip()[:2]
                if nxt.startswith("("):
                    yield "FM", name, lineno
                elif nxt.startswith("::"):
                    continue           # a type/namespace qualifier, caught at its def
                elif scope_is_type and scope_is_type[-1]:
                    yield "MM", name, lineno
                elif name.startswith("g_") or re.search(r"\bextern\b", code):
                    yield "GM", name, lineno
                # else: a function-body local — outside the taxonomy, skipped
            elif m.lastgroup == "unk":
                # Unk<hex/dec> placeholder: only a struct/class-body DECLARATION
                # counts (same scope_is_type test MM uses) -- a `this->Unk0xb0`
                # USE inside a method body sits outside any type scope and is
                # skipped
                if scope_is_type and scope_is_type[-1]:
                    yield "UF", m.group(), lineno


def _resolve(paths):
    files = []
    for p in paths:
        p = Path(p).resolve()
        files += [p] if p.is_file() else sorted(p.rglob("*.h")) + sorted(p.rglob("*.cpp"))
    return files


def collect(paths=None):
    """Return {(cat, name): sorted[(relpath, lineno)]} across src/ (or `paths`)."""
    scope_files = sorted(SRC.rglob("*.h")) + sorted(SRC.rglob("*.cpp"))
    type_names = collect_type_names(scope_files)   # registry is always repo-wide
    files = _resolve(paths) if paths else scope_files
    debt = {}
    for path in files:
        try:
            rel = path.relative_to(ROOT)
        except ValueError:
            rel = path
        for cat, name, lineno in scan_file(path, type_names):
            debt.setdefault((cat, name), []).append((str(rel), lineno))
    return debt


def _by_cat(debt):
    out = {c: [] for c, _ in CATS}
    for (cat, name), sites in debt.items():
        out[cat].append((name, sites))
    for cat in out:
        out[cat].sort(key=lambda t: t[0].lower())
    return out


def dashboard_line(debt):
    by = _by_cat(debt)
    total = len(debt)
    parts = " ".join("%s=%d" % (c, len(by[c])) for c, _ in CATS)
    return "naming debt %d names (%s)  <- tools/lint_names.py; grind to certain names" % (total, parts)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    flags = {a for a in sys.argv[1:] if a.startswith("--")}
    debt = collect(args or None)
    by = _by_cat(debt)

    if "--dashboard" in flags:
        print(dashboard_line(debt))
        return

    if "--list" in flags or args:
        for cat, desc in CATS:
            items = by[cat]
            if not items:
                continue
            print("\n[%s] %s — %d name%s" % (cat, desc, len(items), "" if len(items) == 1 else "s"))
            for name, sites in items:
                where = sites[0]
                extra = "" if len(sites) == 1 else "  (+%d more)" % (len(sites) - 1)
                print("  %-40s %s:%d%s" % (name, where[0], where[1], extra))
        print("\n%s" % dashboard_line(debt))
        return

    # default: compact per-category summary table
    print("naming debt by category (severity high -> low):")
    for cat, desc in CATS:
        print("  %-3s %4d   %s" % (cat, len(by[cat]), desc))
    print("  ----")
    print("  %s" % dashboard_line(debt))


if __name__ == "__main__":
    main()
