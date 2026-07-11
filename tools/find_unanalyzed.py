#!/usr/bin/env python3
"""List candidate function entries in .text that no Ghidra function body covers.

    usage: tools/find_unanalyzed.py <bodies.txt>

where bodies.txt is `<lo>,<hi>,<entry>` hex triples, one per Ghidra function body range:

    curl -s -X POST 'localhost:8089/run_script_inline?program=Loco.exe' \
      -H 'Content-Type: application/json' -d '{"code":"StringBuilder sb=new StringBuilder();
      java.util.Iterator it=currentProgram.getFunctionManager().getFunctions(true);
      while(it.hasNext()){ ghidra.program.model.listing.Function
      f=(ghidra.program.model.listing.Function)it.next();
      ghidra.program.model.address.AddressRangeIterator ri=f.getBody().getAddressRanges();
      while(ri.hasNext()){ ghidra.program.model.address.AddressRange r=ri.next();
      sb.append(Long.toHexString(r.getMinAddress().getOffset())).append(\",\")
      .append(Long.toHexString(r.getMaxAddress().getOffset())).append(\",\")
      .append(Long.toHexString(f.getEntryPoint().getOffset())).append(\"\\n\"); } }
      println(\"BODYDUMP_START\\n\"+sb.toString()+\"BODYDUMP_END\");"}'

Feed the output back to Ghidra with disassemble()+createFunction() and REPEAT until it comes
back empty -- each newly created function exposes the gap that follows it.

⚠ WHY THIS EXISTS (v367). A call/jmp-target sweep CANNOT find this code and returned zero
orphans over the whole binary: the functions Ghidra missed are exactly the ones reached only
through a DATA pointer -- WNDPROCs handed to RegisterClass/CreateWindowEx, vtable slots,
thread procs passed to ThreadWrapper::Start, CRT atexit thunks. The two biggest app-region
functions in Loco.exe (AppWindow_WndProc at 0x4618c0, 5351 B, and 0x4028b0, 4833 B) were both
invisible for 366 sessions for that reason.

A "gap" is a maximal .text range covered by no function body. Inside a gap, a RUN is a
stretch of content ending where >=6 consecutive alignment bytes (0x90/0xcc) begin -- 0x00
is NOT a run terminator, since zero bytes appear constantly inside immediates (getting this
wrong truncates every run at its first zero byte and hides the whole CRT init-thunk table).
Every 16-byte-aligned run start is a candidate entry (VC5 aligns function starts to 16 and
pads with 0x90), minus two classes of false positive:
  * jump tables (leading dword is a .text address, or >=70% of dwords are) -- note a VC5
    two-level table starts with real dword entries and continues with BYTE indices, so the
    leading-dword test is the load-bearing one, not the ratio
  * any run after the first in its gap that does not decode cleanly to a ret/jmp -- that
    filter keeps a switch table living inside a gap from being mistaken for a stub run.
"""
import struct, sys, capstone

EXE = "/Users/maxamillion/workspace/LocoDecomp/loco/Loco.exe"
d = open(EXE, "rb").read()
pe = struct.unpack_from("<I", d, 0x3c)[0]
nsec = struct.unpack_from("<H", d, pe + 6)[0]
optsz = struct.unpack_from("<H", d, pe + 20)[0]
for i in range(nsec):
    o = pe + 24 + optsz + i * 40
    nm = d[o:o+8].rstrip(b"\0").decode()
    vsz, va, rsz, ra = struct.unpack_from("<IIII", d, o + 8)
    if nm == ".text":
        LO, HI, RA = 0x400000 + va, 0x400000 + va + vsz, ra
def B(va, n): return d[RA + (va - LO): RA + (va - LO) + n]

_md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
def clean(buf, va):
    n, last = 0, None
    for i in _md.disasm(buf, va):
        n += i.size; last = i.mnemonic
    return n == len(buf) and last in ("ret", "jmp")

rs = []
for l in open(sys.argv[1]):
    lo, hi, e = (int(x, 16) for x in l.split(","))
    rs.append((lo, hi))
rs.sort()
merged = []
for lo, hi in rs:
    if merged and lo <= merged[-1][1] + 1:
        merged[-1][1] = max(merged[-1][1], hi)
    else:
        merged.append([lo, hi])
gaps, prev = [], LO
for lo, hi in merged:
    if lo > prev: gaps.append((prev, lo - 1))
    prev = max(prev, hi + 1)
if prev < HI: gaps.append((prev, HI - 1))

ALIGN = (0x90, 0xcc)
MINPAD = 6

for glo, ghi in gaps:
    n = ghi - glo + 1
    b = B(glo, n)
    if all(x in ALIGN or x == 0 for x in b):
        continue
    runs, i = [], 0
    while i < n:
        if b[i] in ALIGN or b[i] == 0:
            i += 1; continue
        j = i
        while j < n:
            if b[j] in ALIGN:
                k = j
                while k < n and b[k] in ALIGN: k += 1
                if k - j >= MINPAD or k == n:
                    break
                j = k
            else:
                j += 1
        runs.append((glo + i, j - i))
        i = j
    for idx, (st, ln) in enumerate(runs):
        if ln < 6 or st % 16: continue
        if idx > 0 and not clean(B(st, ln), st): continue
        if LO <= struct.unpack("<I", B(st, 4))[0] < HI: continue
        k = min(16, max(1, ln // 4))
        good = sum(1 for i2 in range(k)
                   if LO <= struct.unpack("<I", B(st + i2*4, 4))[0] < HI)
        if good / float(k) >= 0.7: continue
        print("0x%06x %d" % (st, ln))
