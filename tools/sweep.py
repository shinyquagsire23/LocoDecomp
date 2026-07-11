#!/usr/bin/env python3
"""Sweep: for every function symbol in every member of a LIB, reverse-scan
Loco.exe (reloc-masked, anchored on the first >=8-byte unmasked run) and print
candidate locations. Known-named exe functions are reported too (caller must
filter)."""
import os, struct, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT + "/tools")
from extract_lib_member import parse_archive

exe = open(ROOT + "/loco/Loco.exe", "rb").read()
TEXT_OFF = 0x400
TEXT_LEN = len(exe) - TEXT_OFF

def trim(b):
    while b and b[-1] in (0xCC, 0x90):
        b = b[:-1]
    return b

def anchor(code, masked):
    """longest unmasked run >= 6 bytes; returns bytes pattern or None."""
    best = b""
    cur = bytearray()
    for k in range(len(code)):
        if not masked[k]:
            cur.append(code[k])
        else:
            if len(cur) > len(best):
                best = bytes(cur)
            cur = bytearray()
    if len(cur) > len(best):
        best = bytes(cur)
    return best if len(best) >= 6 else None

def sweep(libpath):
    data, members, symtab = parse_archive(libpath)
    for name, off, size in members:
        if not name.lower().endswith(".obj"):
            continue
        obj = data[off:off+size]
        if obj[:2] == b"!<":
            continue
        nsec = struct.unpack_from("<H", obj, 2)[0]
        opt = struct.unpack_from("<H", obj, 16)[0]
        symoff = struct.unpack_from("<I", obj, 8)[0]
        nsym = struct.unpack_from("<I", obj, 12)[0]
        sh = 20 + opt
        strtab = symoff + nsym * 18
        secs = []
        for i in range(nsec):
            o = sh + i*40
            nm = obj[o:o+8].rstrip(b"\0").decode("latin1")
            v = struct.unpack_from("<IIIIIIHHI", obj, o+8)
            secs.append((nm, v[2], v[3], v[4], v[6]))
        def symname(rec):
            if rec[:4] == b"\0\0\0\0":
                o = struct.unpack_from("<I", rec, 4)[0]
                e = obj.index(b"\0", strtab+o)
                return obj[strtab+o:e].decode("latin1")
            return rec.rstrip(b"\0").decode("latin1")
        i = 0
        while i < nsym:
            rec = obj[symoff+i*18:symoff+i*18+18]
            val, secn, typ, scl, naux = struct.unpack_from("<IhHBB", rec, 8)
            if typ == 0x20 and secn > 0:
                nm = symname(rec)
                snm, rawsz, rawptr, relptr, nrel = secs[secn-1]
                if snm.startswith(".text"):
                    code = trim(obj[rawptr+val:rawptr+rawsz])
                    if len(code) >= 8:
                        relocs = [struct.unpack_from("<IIH", obj, relptr+r*10)[0]-val for r in range(nrel)]
                        tl = len(code)
                        masked = bytearray(tl)
                        for r in relocs:
                            for k in range(max(r, 0), min(r+4, tl)):
                                masked[k] = 1
                        pat = anchor(code, masked)
                        if pat:
                            pos = 0
                            while True:
                                j = exe.find(pat, TEXT_OFF + pos)
                                if j < 0:
                                    break
                                start = j - TEXT_OFF - (code.find(pat))
                                if 0 <= start <= TEXT_LEN - tl:
                                    cb = exe[TEXT_OFF+start:TEXT_OFF+start+tl]
                                    for k in range(tl):
                                        if not masked[k] and cb[k] != code[k]:
                                            break
                                    else:
                                        print("0x%x %s %s" % (0x401000+start, nm, name.split("\\")[-1]))
                                pos = j - TEXT_OFF + 1
            i += 1 + naux

sweep(sys.argv[1])
