#!/usr/bin/env python3
"""lint_selfcontained.py -- is every src/*.h header compilable ON ITS OWN?

A header that names a type it never declares still compiles today, as long as every
TU that reaches it happens to include that type's provider FIRST. That is not a
property of the header; it is luck about include order, and it detonates the day an
unrelated TU adds an unrelated include. src/PostBag.h was exactly this bomb: it named
IDirectDrawSurface, and the moment src/CarNetObj.h started including CarNetState.h
(which pulls PostBag.h in), FOUR TUs that had never needed <ddraw.h> turned into
COMPILE FAILED at once -- with nothing in the build able to point at the cause.

Nothing else here catches it. tools/cc.sh only ever compiles a header in the company
of a real TU's include list, which is precisely the company that hides the defect.

The check: for each src/*.h, compile a one-line TU that includes ONLY that header,
with tools/cc.sh's own flags. Anything that fails is not self-contained.

INFORMATIONAL by default (exit 0), like tools/lint_names.py and tools/lint_alias.py;
--strict exits 1 on any finding. Needs the wine toolchain, so it is deliberately NOT
wired into verify.py/cc.sh (a header sweep is ~60 serialized wine invocations); run it
after adding a header, or after any header grows a new member type.

The fix is almost always one guarded #include at the top of the header. That is
byte-neutral BY CONSTRUCTION whenever the repo currently compiles: every TU reaching
the header already includes the provider ahead of it (any that did not would be
COMPILE FAILED today), so the provider's own include guard makes the new line expand
to nothing everywhere. Confirm with a full tools/progress.py per-file table diff
anyway -- that is what proved it for the v477 sweep, which found 3 of 62 headers bad
and fixed all three with a per-file table that came back byte-identical.
"""
import argparse
import glob
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")
CL = os.path.join(ROOT, "toolchain", "bin", "cl")

# Kept in sync with tools/cc.sh's FLAGS by hand -- same compiler, same defines, so a
# header that passes here passes in a real TU.
FLAGS = "/nologo /c /MT /W3 /O2 /Gy /GX /D WIN32 /D NDEBUG /D _WINDOWS".split()


def check(header, objdir):
    """Compile a TU containing only `#include "<header>"`. Returns [] or error lines."""
    stem = "sc_" + header[:-2]
    # The probe .cpp has to live in src/ so that quoted includes resolve the same way
    # they do for a real TU; it is removed again immediately.
    cpp = os.path.join(SRC, stem + ".cpp")
    with open(cpp, "w") as f:
        f.write('#include "%s"\n' % header)
    try:
        p = subprocess.run(
            [CL] + FLAGS + ["/Fo" + os.path.join(objdir, stem + ".obj"), stem + ".cpp"],
            cwd=SRC, capture_output=True, text=True)
    finally:
        os.unlink(cpp)
    if p.returncode == 0:
        return []
    out = []
    seen = set()
    for line in (p.stdout + p.stderr).splitlines():
        line = line.strip()
        if ": error" not in line and ": fatal error" not in line:
            continue
        # cl under wine prints Z:\<abs path>; make it repo-relative and readable.
        line = line.replace("\\", "/")
        for prefix in ("Z:" + SRC + "/", SRC + "/"):
            line = line.replace(prefix, "")
        if line in seen:
            continue
        seen.add(line)
        out.append(line)
    return out or ["compile failed with no parsable diagnostic"]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("headers", nargs="*",
                    help="header names or paths to check (default: every src/*.h)")
    ap.add_argument("--strict", action="store_true",
                    help="exit 1 when a header is not self-contained (default: exit 0)")
    ap.add_argument("--max-errors", type=int, default=6,
                    help="diagnostics to print per failing header (default 6)")
    args = ap.parse_args()

    if not os.path.exists(CL):
        print("toolchain missing (%s) -- skipping" % CL)
        return 2 if args.strict else 0

    if args.headers:
        headers = [os.path.basename(h) for h in args.headers]
    else:
        headers = sorted(os.path.basename(p) for p in glob.glob(os.path.join(SRC, "*.h")))

    bad = []
    with tempfile.TemporaryDirectory() as objdir:
        for h in headers:
            errs = check(h, objdir)
            if errs:
                bad.append((h, errs))

    for h, errs in bad:
        print("! NOT SELF-CONTAINED  src/%s" % h)
        for e in errs[:args.max_errors]:
            print("    " + e)
        if len(errs) > args.max_errors:
            print("    ... and %d more" % (len(errs) - args.max_errors))
        print()

    print("%d header(s) checked, %d NOT self-contained" % (len(headers), len(bad)))
    if bad:
        print("fix: add the missing provider as a guarded #include at the top of the header "
              "(byte-neutral while the repo compiles -- see this file's docstring), then "
              "confirm with a full tools/progress.py per-file table diff.")
    return 1 if (bad and args.strict) else 0


if __name__ == "__main__":
    sys.exit(main())
