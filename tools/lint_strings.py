#!/usr/bin/env python3
"""Sweep src/ string literals against Loco.exe's raw bytes.

Catches the v395/v396/v489/v490 bug class: a string literal that is WRONG but
byte-INVISIBLE.  Nothing else in this toolchain can see it -- string CONTENT is
never compared, only the masked relocation to it, so a function carrying a wrong
literal byte-matches EXACTLY.  Not the compiler, not any other lint, not
progress.py.

Two sources of the error, both confirmed live in this repo:
  * A Ghidra auto-label MANGLES every non-alphanumeric character in a string to
    '_', so a literal copied out of an `s_`-prefixed label instead of the image's
    own bytes is silently wrong.  `s__curr_0047e2a0` labels "~curr", not "_curr"
    (v395); `s_cursor_default_frame_set` labels "cursor/default_frame_set";
    `busy_ani` labels "busy.ani" (v490).
  * A plain transcription slip in either direction -- "WINDOW ATTRIBUTES" for the
    image's "WINDOW_ATTRIBUTES" (v489), or a DROPPED character: "%sCURSORS\\%s"
    for the image's "%s\\CURSORS\\%s" (v490).

Method: every string literal in src/ must appear VERBATIM in Loco.exe.  For any
that does not, search the image for every EDIT-DISTANCE-1 neighbour -- one
substitution, one insertion, or one deletion, at any position, over the printable
set.  A neighbour that DOES appear in the image is almost certainly the real
string.  Those are reported as LIKELY WRONG and should be confirmed against a raw
byte dump before fixing.

  ! Earlier revisions of this tool only tried substituting '_' with another
    character.  That is one third of one direction: it structurally could not see
    v489's space-for-underscore (the reverse substitution) nor v490's dropped
    backslash (an insertion).  Both were found by hand.  Hence the general form.

Literals absent from the image with no near-miss are listed under -v and are
usually benign: comment prose the filter did not strip, or strings this project
builds rather than pools.  Run from the repo root.

Usage:  tools/lint_strings.py [-v]
Status: informational, like tools/lint_names.py -- always exits 0, never wired
        into cc.sh or the pre-commit hook.
"""
import re, sys, pathlib

IMG = pathlib.Path("loco/Loco.exe").read_bytes()

LIT = re.compile(r'"((?:[^"\\\n]|\\.)*)"')

# Printable set a dropped/mangled/mistyped character could have been.
CANDS = (" ~!@#$%^&*()-+=[]{};:'\",.<>/?|`\\\n\t_"
         "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")


def unescape(s):
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == '\\' and i + 1 < len(s):
            n = s[i + 1]
            m = {'n': '\n', 't': '\t', 'r': '\r', '0': '\0',
                 '\\': '\\', '"': '"', "'": "'"}
            if n in m:
                out.append(m[n]); i += 2; continue
            if n == 'x':
                j = i + 2
                while j < len(s) and s[j] in '0123456789abcdefABCDEF':
                    j += 1
                out.append(chr(int(s[i + 2:j], 16))); i = j; continue
            out.append(n); i += 2; continue
        out.append(c); i += 1
    return ''.join(out)


def strip_comment(line):
    """Cut a trailing // comment, ignoring one inside a string/char literal.

    Comment prose is the dominant false positive -- it is full of English that
    looks like a literal but was never meant to be in the image.
    """
    i, n = 0, len(line)
    while i < n:
        c = line[i]
        if c == '\\':
            i += 2; continue
        if c in '"\'':
            q = c; i += 1
            while i < n and line[i] != q:
                i += 2 if line[i] == '\\' else 1
            i += 1; continue
        if c == '/' and i + 1 < n and line[i + 1] in '/*':
            return line[:i]
        i += 1
    return line


def near_misses(s):
    """Every edit-distance-1 neighbour of s that appears in the image."""
    hits = set()
    for i in range(len(s)):                       # substitution
        for c in CANDS:
            if c == s[i]:
                continue
            cand = s[:i] + c + s[i + 1:]
            if cand.encode("latin-1", "replace") in IMG:
                hits.add(cand)
    for i in range(len(s) + 1):                   # insertion (src dropped a char)
        for c in CANDS:
            cand = s[:i] + c + s[i:]
            if cand.encode("latin-1", "replace") in IMG:
                hits.add(cand)
    for i in range(len(s)):                       # deletion (src gained a char)
        cand = s[:i] + s[i + 1:]
        if len(cand) >= 3 and cand.encode("latin-1", "replace") in IMG:
            hits.add(cand)
    return hits


suspects = []
for p in sorted(pathlib.Path("src").rglob("*.cpp")) + sorted(pathlib.Path("src").rglob("*.h")):
    for lineno, line in enumerate(p.read_text(errors="replace").splitlines(), 1):
        st = line.strip()
        if st.startswith("#include") or st.startswith("//") or st.startswith("*"):
            continue
        for m in LIT.finditer(strip_comment(line)):
            raw = m.group(1)
            if not raw:
                continue
            s = unescape(raw)
            if len(s) < 3 or s.encode("latin-1", "replace") in IMG:
                continue
            suspects.append((str(p), lineno, s, near_misses(s)))

hasalt = [x for x in suspects if x[3]]
noalt = [x for x in suspects if not x[3]]

print("=== LIKELY WRONG (an edit-distance-1 neighbour DOES appear in the image) ===")
for p, ln, s, alts in hasalt:
    print(f"{p}:{ln}: {s!r}  ->  {sorted(alts)!r}")
print(f"\n({len(hasalt)} likely wrong, {len(noalt)} other literals absent from image)")

if "-v" in sys.argv:
    print("\n=== ABSENT FROM IMAGE (no near-miss; may be format strings, built "
          "strings, comment prose, or genuinely absent) ===")
    for p, ln, s, _ in noalt:
        print(f"{p}:{ln}: {s!r}")
