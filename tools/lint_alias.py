#!/usr/bin/env python3
"""lint_alias.py -- find BYTE-INVISIBLE WRONG CALL TARGETS.

The defect class (first found by hand in v445 at 0x411fb0, generalized in v446):
one original function is TRANSCRIBED in some TU under name N, while another TU
declares the same address under a DIFFERENT name M and calls M. The emitted call
targets a symbol that exists nowhere in the project.

Nothing else in this repo can see it:
  * verify.py / match.py MASK relocations, so a call to the wrong target is
    byte-identical to a call to the right one -- both sides still MATCH;
  * we never link, so the undefined symbol never surfaces;
  * lint_idiom.py, lint_calls.py and lint_ghidra_sync.py are all blind to it
    (each declaration is individually well-formed, and each name may
    individually agree with Ghidra -- lint_ghidra_sync accepts both the
    fully-qualified name and the "::"->"_" flattened spelling, which is exactly
    how `WindowBase::CenterRectInRect` and `WindowBase_CenterRectInRect` could
    both pass while disagreeing with each other).

The oracle used here is the only one available: an address that carries a
`// FUNCTION: LOCO 0x...` marker HAS a definition, so any OTHER spelling of that
same address is a call that cannot reach it.

Deliberately NOT reported (checked and benign):
  * `X` vs `~X` at one address -- a class declaration line and its own dtor
    definition, not two names;
  * an address with many declarations, e.g. 0x422ea0 -- the ICF-folded
    `WindowBase_DefWindowProcStub` that ~18 distinct default vtable slots share.
    Those are real distinct virtual slots that happen to fold to one body;
  * `_vNN` placeholder slots, which name a vtable ordinal, not a callee.

Two marker-pairing artifacts produced spurious findings until v448 (both fixed in
`_definition_after`, which is now the single place a marker is paired to its
definition):
  * an `inline` HELPER written between a marker and its real function -- 0x446ea0
    used to pair to `TileKind_LoadDescriptorRange` instead of
    `TileKind_GetOrLoadDescriptor`, so every `g_UIResources.TileKind_GetOrLoad-
    Descriptor(...)` call in the repo read as an alias. An inline definition can
    still legitimately BE the marker's function (a self-recursive inline emits one
    out-of-line copy -- 0x45d810 TrackGraph::FreeRouteTreeMaybe), so the rule is
    not "skip inline": skip its body and take a later definition only if one
    arrives before the next marker.
  * a marker with NO source line of its own (a compiler-generated `??_G` scalar
    deleting dtor, which carries its mangled-name hint in parentheses) used to
    pair to the NEXT marker's function -- 0x4203a0 `??_GSplashWnd` claimed
    `SplashWnd::Create`, which is 0x4204d0. Scanning now stops at a marker line.
21 of the 831 marker addresses are that second kind and are deliberately left
unpaired; `match.py`'s own `_want_key` resolves them from the parenthetical hint.

Exit code is always 0 -- this is a burn-down tracker like lint_names.py, not a
gate. A finding is fixed by deleting the local alias and including the header
that already declares the transcribed name.
"""
import collections
import glob
import os
import re
import sys

MARKER = re.compile(r'//\s*FUNCTION:\s*LOCO\s+0x(4[0-9a-f]{5})')
DECL = re.compile(r'(?<![~\w])([A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_~][A-Za-z0-9_]*)*)\s*\(')
ADDR = re.compile(r'(?:FUN_00|0x)(4[0-9a-f]{5})\b')
PLACEHOLDER = re.compile(r'^_v[0-9a-f]+$')

# An address whose declaration count exceeds this is an ICF-folded shared stub
# (many genuinely different vtable slots sharing one body), not an alias bug.
FOLD_THRESHOLD = 3


KEYWORDS = ('if', 'while', 'for', 'switch', 'return', 'sizeof', 'catch')


def _next_definition(lines, start):
    """The first definition line at or after `start`, as (name, lineno_0based), or None.
    Stops at a marker line (that definition belongs to the NEXT marker, not this one)."""
    for j in range(start, min(start + 400, len(lines))):
        if MARKER.search(lines[j]):
            return None
        nxt = lines[j].strip()
        if not nxt or nxt.startswith(('//', '/*', '*', '#')):
            continue
        d = DECL.search(nxt)
        if d and not nxt.endswith(';') and d.group(1) not in KEYWORDS:
            return (d.group(1), j)
        return None
    return None


def _definition_after(lines, marker_line):
    """Pair a marker to its definition. An `inline` definition may be either the marker's own
    function (a self-recursive inline still emits one out-of-line copy -- 0x45d810
    TrackGraph::FreeRouteTreeMaybe) or a helper written between the marker and its real
    function (0x446ea0's TileKind_LoadDescriptorRange). Disambiguate by skipping the inline
    body and looking past it: if another definition follows before the next marker, the inline
    one was a helper."""
    got = _next_definition(lines, marker_line + 1)
    if got is None:
        return None
    name, j = got
    if not re.match(r'^\s*inline\b', lines[j]):
        return (name, None, j + 1)
    depth, seen = 0, False
    for k in range(j, min(j + 400, len(lines))):
        depth += lines[k].count('{') - lines[k].count('}')
        if lines[k].count('{'):
            seen = True
        if seen and depth <= 0:
            after = _next_definition(lines, k + 1)
            return (after[0], None, after[1] + 1) if after else (name, None, j + 1)
    return (name, None, j + 1)


def scan(root):
    defined = {}                                 # addr -> (name, path, line)
    decls = collections.defaultdict(list)        # addr -> [(name, path, line)]
    files = sorted(glob.glob(os.path.join(root, '*.h')) +
                   glob.glob(os.path.join(root, '*.cpp')))
    for path in files:
        with open(path) as fh:
            lines = fh.read().split('\n')
        for i, ln in enumerate(lines):
            m = MARKER.search(ln)
            if m:
                # Walk forward to this marker's definition. An `inline` definition may be
                # either the marker's own function (a self-recursive inline still emits one
                # out-of-line copy -- 0x45d810 TrackGraph::FreeRouteTreeMaybe) or a helper
                # written between the marker and its function (0x446ea0's
                # TileKind_LoadDescriptorRange). Disambiguate by looking past it: if another
                # definition follows BEFORE the next marker, the inline one was the helper.
                pick = _definition_after(lines, i)
                if pick:
                    defined[m.group(1)] = (pick[0], path, pick[2])
                continue
            if '//' not in ln:
                continue
            code, _, cmt = ln.partition('//')
            if '(' not in code or ';' not in code:
                continue
            a = ADDR.search(cmt)
            if not a:
                continue
            d = DECL.search(code)
            if d and d.group(1) not in ('if', 'while', 'for', 'switch',
                                        'return', 'sizeof'):
                decls[a.group(1)].append((d.group(1), path, i + 1))
    return defined, decls


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else 'src'
    defined, decls = scan(root)
    findings = []
    for addr, (dname, dpath, dline) in sorted(defined.items()):
        others = decls.get(addr, [])
        if len(others) > FOLD_THRESHOLD:
            continue                              # ICF-folded shared stub
        short = dname.split('::')[-1]
        for name, path, line in others:
            alias = name.split('::')[-1]
            if alias == short or alias == '~' + short or short == '~' + alias:
                continue
            if PLACEHOLDER.match(alias):
                continue
            findings.append((addr, dname, dpath, dline, name, path, line))

    for addr, dname, dpath, dline, name, path, line in findings:
        print('0x%s  %s' % (addr, dname))
        print('%12s defined  %s:%d' % ('', dpath, dline))
        print('%12s aliased  %s:%d  as `%s`' % ('', path, line, name))
    print('\n%d byte-invisible wrong-call-target finding(s)' % len(findings))
    return 0


if __name__ == '__main__':
    sys.exit(main())
