#!/usr/bin/env python3
"""lint_ghidra_types.py — every Ghidra CLASS namespace must have a same-named Structure.

CLAUDE.md's struct rule: "The Ghidra namespace MUST equal a same-named Structure —
that's how auto-`this` gets typed; no matching struct ⇒ `this` degrades to `void*`."
Nothing else in tools/ enforces it. lint_ghidra_sync.py checks FUNCTION and GLOBAL
names against src/; it never looks at TYPE names, so a half-applied class rename can
sit in the DB indefinitely while every lint stays green.

Both defects this tool was written for had done exactly that:

  - TimeOfDayMaybe (found v553b) — renamed from the address-based `Obj0x477840` in
    v482 on the namespace side only. The struct kept the old name, so the link broke.
  - CarKindDesc (found v553c) — the same v482 rename, half-applied the same way, but
    with the nastier failure mode: Ghidra had auto-created a 1-byte PlaceHolder Class
    Structure under the class's name, so `this` was not `void*` but a 1-byte struct.
    That LOOKS typed in the decompiler and in get_function_by_address's signature
    line, while every one of the real struct's 0x7ac bytes still decompiles as raw
    offset math. See docs/GHIDRA_RECIPES.md's placeholder note.

So the check is deliberately TWO checks: a namespace with no struct at all, and a
namespace whose struct is a suspiciously small placeholder (<= PLACEHOLDER_MAX bytes)
while a plausible real struct exists elsewhere in the DB.

INFORMATIONAL like lint_names.py / lint_alias.py — always exit 0, so it is safe to
run anywhere; `--strict` exits 1 on findings (and 2 when Ghidra is unreachable), for
use in a hook. Deliberately NOT wired into verify.py/cc.sh: it needs a live Ghidra,
and the failure it catches is a DB-hygiene problem, not a build problem.

usage: lint_ghidra_types.py [--strict] [--list]
"""
import json
import re
import sys
import urllib.request

GHIDRA = "http://localhost:8089"
PROGRAM = "Loco.exe"
PLACEHOLDER_MAX = 1  # Ghidra's auto class-structure placeholder is 1 byte

QUERY = r"""
java.util.HashMap<String,Integer> structs = new java.util.HashMap<String,Integer>();
ghidra.program.model.data.DataTypeManager dtm = currentProgram.getDataTypeManager();
java.util.Iterator<ghidra.program.model.data.Structure> sit = dtm.getAllStructures();
while (sit.hasNext()) { ghidra.program.model.data.Structure s = sit.next();
  structs.put(s.getName(), s.getLength()); }
// getAllStructures() omits placeholder class structures, so ask the type manager
// directly for anything sharing a class namespace's name.
java.util.HashMap<String,Integer> anyType = new java.util.HashMap<String,Integer>();
java.util.Iterator<ghidra.program.model.data.DataType> ait = dtm.getAllDataTypes();
while (ait.hasNext()) { ghidra.program.model.data.DataType d = ait.next();
  if (!anyType.containsKey(d.getName())) anyType.put(d.getName(), d.getLength()); }
java.util.HashMap<String,Integer> nsFuncs = new java.util.HashMap<String,Integer>();
java.util.Iterator<ghidra.program.model.listing.Function> fit =
    currentProgram.getFunctionManager().getFunctions(true);
while (fit.hasNext()) {
  ghidra.program.model.listing.Function f = fit.next();
  ghidra.program.model.symbol.Namespace ns = f.getParentNamespace();
  if (ns == null || ns.isGlobal()) continue;
  if (ns.getSymbol().getSymbolType() != ghidra.program.model.symbol.SymbolType.CLASS) continue;
  String n = ns.getName();
  nsFuncs.put(n, (nsFuncs.containsKey(n) ? nsFuncs.get(n) : 0) + 1);
}
java.util.ArrayList<String> keys = new java.util.ArrayList<String>(nsFuncs.keySet());
java.util.Collections.sort(keys);
for (String k : keys) {
  int slen = structs.containsKey(k) ? structs.get(k) : -1;
  int alen = anyType.containsKey(k) ? anyType.get(k) : -1;
  println("TYPES NS " + k + " members=" + nsFuncs.get(k) + " struct=" + slen + " any=" + alen);
}
println("TYPES-DONE");
"""


def query():
    req = urllib.request.Request(
        f"{GHIDRA}/run_script_inline?program={PROGRAM}",
        data=json.dumps({"code": QUERY}).encode(),
        headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=120) as resp:
            text = resp.read().decode(errors="replace")
    except OSError as e:
        print(f"WARNING: Ghidra unreachable ({e}) — type check skipped", file=sys.stderr)
        return None
    # the response echoes stale compile errors from old cached scripts —
    # only trust our own TYPES lines + the TYPES-DONE marker
    if "TYPES-DONE" not in text:
        print("ERROR: script did not complete (no TYPES-DONE marker) — is "
              f"{PROGRAM} open in Ghidra? Raw tail:\n" + text[-500:], file=sys.stderr)
        return "ERROR"
    rows = []
    for line in text.splitlines():
        m = re.match(r"^TYPES NS (\S+) members=(\d+) struct=(-?\d+) any=(-?\d+)$", line)
        if m:
            rows.append((m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4))))
    return rows


def main():
    strict = "--strict" in sys.argv
    listing = "--list" in sys.argv
    rows = query()
    if rows is None:
        sys.exit(2 if strict else 0)
    if rows == "ERROR":
        sys.exit(2 if strict else 0)

    findings = []
    for name, members, slen, alen in rows:
        if slen < 0 and alen < 0:
            findings.append(("NOSTRUCT", name, members,
                             "class namespace has no data type of its own name"))
        elif slen < 0 and 0 <= alen <= PLACEHOLDER_MAX:
            findings.append(("PLACEHOLDER", name, members,
                             f"only a {alen}-byte placeholder class structure — `this` "
                             f"looks typed but carries no fields"))
        elif 0 <= slen <= PLACEHOLDER_MAX:
            findings.append(("STUB", name, members,
                             f"struct is only {slen} byte(s); a real one may be parked "
                             f"under an address-based name"))
        elif listing:
            print(f"  ok          {name:<40} members={members:<3} struct={slen}")

    for kind, name, members, why in findings:
        print(f"  {kind:<11} {name:<40} members={members:<3} {why}")

    print()
    print(f"{len(rows)} class namespaces checked; {len(findings)} without a usable struct "
          f"(auto-`this` degraded)")
    if findings and not strict:
        print("  ^ fix: rename the real struct to the class name (and remove any 1-byte "
              "placeholder first) — see docs/GHIDRA_RECIPES.md")
    sys.exit(1 if (findings and strict) else 0)


if __name__ == "__main__":
    main()
