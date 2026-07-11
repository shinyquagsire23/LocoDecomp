#!/usr/bin/env python3
"""SMOKE-TEST SCAFFOLDING ONLY -- generate a COFF i386 .obj that DEFINES the
symbols whose real bodies live in not-yet-transcribed TUs (declared-only
virtuals, Partial-class methods, cross-TU globals), so tools/link_check.sh can
close the link.  Not part of the byte-match sources.

Usage: gen_stubs.py <symbol-file> <out.obj>
  <symbol-file>: one symbol per line; lines starting with '__imp__' are ignored
  (imports must come from real import libraries).  Symbols that look like code
  (mangled C++ '?...' or stdcall '_name@N') go in .text, everything else in .bss.

TWO DEFECTS THIS FILE USED TO HAVE, both invisible to the link itself:

1. EVERY data symbol was an alias of ONE shared zero dword.  So `g_worldBoard`
   (337172 bytes in the original), `g_UIResources`, `g_RFIndex` and 151 others
   all named the same 4 bytes, and the first write through any of them scribbled
   over all the rest.  Fixed by laying the data stubs out inside a MIRROR of the
   original image's 0x477000..0x501000 span (.rdata + .data + bss), each at its
   true offset.  That gets every stub its real size and preserves real aliasing
   -- g_RFIndex genuinely IS g_UIResources+0x18, an embedded member, and the
   mirror reproduces that for free.  Addresses come from the symbol name itself
   (DAT_00xxxxxx / g_vtable0xXXXXXX) or from gen_syms_addrs.txt.
   ⚠ Caveat that no mirror can fix: relationships between a MIRRORED stub and a
   global a real TU defines are still whatever the linker chooses.  The mirror
   only makes the stubs self-consistent.

2. EVERY code symbol was one bare `ret` (0xC3).  Correct for __cdecl, WRONG for
   every callee-cleanup convention -- a __stdcall/__thiscall callee must pop its
   own arguments, so calling a `ret`-only stub for `_Foo@16` leaves 16 bytes of
   garbage on the stack and the caller returns into hyperspace.  Fixed first for
   the `_name@N` form, whose N is right there in the name, and now for the
   mangled C++ ones too -- `mangled_pop()` below parses the MSVC parameter list.
   That was not a cosmetic gap: 141 of the 244 mangled code stubs pop a nonzero
   number of bytes, and 55 of those are `UAEJPAXIIJ@Z` -- a virtual
   WndProc(HWND, UINT, WPARAM, LPARAM) -- so before this fix EVERY window message
   delivered to a stubbed handler unbalanced the stack by 16 bytes.

   Validated against the original image, which is the only real oracle here: for
   every stub symbol Ghidra can resolve to a real address, the computed pop count
   equals the `ret imm16` the original function actually executes (7/7, plus
   0x42c330's `ret 0x28` = 40, which exercises a back-reference).  The 8th,
   LocoBitmap::HasOpaquePixelInWorkSurfaceRect, disagrees for a reason that is a
   REAL src/ defect rather than a parser bug -- see the note in src/LocoBitmap.h.

   Stubs also zero eax before returning.  A stub cannot know the right answer, but
   NULL/0 at least tends to hit callers' explicit null checks, where leftover
   garbage becomes a wild pointer.
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# The mirrored span: .rdata start .. .rsrc start in the original image.
MIRROR_BASE = 0x477000
MIRROR_END = 0x501000
OVERFLOW_STRIDE = 64  # per-symbol slot for data stubs with no known address

# COFF i386 relocation types, and the fixed symbol-table slots the emitted stub
# bodies relocate against (see the symbol table built in main(): the .text/.bss/
# .rdata section symbols occupy two slots each, so _Stub_Report lands at 6).
REL_DIR32 = 6
REL_REL32 = 20
SYM_RDATA = 4
SYM_REPORT = 6


def load_addr_table():
    """bare-name -> original address, from gen_syms_addrs.txt."""
    table = {}
    path = os.path.join(HERE, "gen_syms_addrs.txt")
    if not os.path.exists(path):
        return table
    for line in open(path):
        line = line.split("#", 1)[0].split()
        if len(line) == 2:
            table[line[0]] = int(line[1], 16)
    return table


def bare_name(sym):
    """Strip C decoration / C++ mangling down to the identifier."""
    if sym.startswith("?"):
        return sym[1:].split("@@")[0]
    return sym[1:] if sym.startswith("_") else sym


def data_addr(sym, table):
    """Original address of a data symbol, or None."""
    b = bare_name(sym)
    m = re.match(r"DAT_(00[0-9a-fA-F]{6})$", b)
    if m:
        return int(m.group(1), 16)
    m = re.match(r"g_vtable0x([0-9a-fA-F]{6})$", b)
    if m:
        return int(m.group(1), 16)
    return table.get(b)


# In an MSVC mangled name the character right after the `@@` that closes the
# qualified name says what the symbol IS: 0-4 are DATA (0-2 static class members,
# 3 a global, 4 a local static), while the function forms are letters (Q/U/A/S/Y
# ... encoding access + calling convention).  Reading every `?...` as code is how
# 128 mangled DATA globals -- ?DAT_004ff0f8@@3PAUIUnknown@@A and friends -- used
# to end up as aliases of the shared `ret` in .text: writable game state pointing
# into the code section.
MANGLED_DATA = re.compile(r"@@[0-4][A-Z_]")


# A C-linkage `_name` with no `@N` suffix is __cdecl, and the decoration alone cannot say
# whether it is a FUNCTION or a VARIABLE -- both mangle identically.  Reading every such
# symbol as data (which this did until v566) hands a __cdecl function a slot in the all-zero
# .bss mirror instead of a reporting code stub, so calling it jumps into zeroed BSS.  That is
# strictly worse than the reverse: a code stub returns 0 and says so in stub_calls.log, while a
# data stub is silent by construction.  `_DDraw_CreateSurfaceFromFile` was one, on the path
# every non-8bpp bitmap asset takes; `_DSoundChannel_ConstructThunkMaybe` and
# `_TrainSyncCarRecord_DestructThunkMaybe` still are.  Decide by this project's own naming
# convention (CLAUDE.md: `g_` for globals, `DAT_` for unnamed data), which every data symbol
# here obeys, and default the rest to code.
CDECL_DATA_PREFIX = re.compile(r"^(DAT_|g_)")


def is_code(sym):
    if sym.startswith("??_7") or sym.startswith("??_8") or sym.startswith("??_C"):
        return False  # vftable / vbtable / string literal -- all data
    if sym.startswith("?"):
        return not MANGLED_DATA.search(sym)
    if not sym.startswith("_"):
        return False
    tail = sym.rsplit("@", 1)[-1]
    if "@" in sym and tail.isdigit():
        return True  # `_name@N` -- stdcall, unambiguously a function
    return not CDECL_DATA_PREFIX.match(sym[1:])


def stdcall_pop(sym):
    """Byte count a `_name@N` stdcall stub must pop, or None for plain `ret`."""
    if sym.startswith("?") or "@" not in sym:
        return None
    tail = sym.rsplit("@", 1)[-1]
    if not tail.isdigit():
        return None
    n = int(tail)
    return n if 0 < n <= 0xFFFF else None


# ---------------------------------------------------------------------------
# MSVC mangled-name parameter-list parser, just enough of the grammar to total up
# a callee's stack footprint.  Anything it cannot size raises Unparsed and the
# caller falls back to a bare `ret`, which is the old (unsafe but no worse)
# behaviour -- this must never be the reason a link stops closing.
# ---------------------------------------------------------------------------

PRIM = {"X": 0,                       # void
        "D": 4, "E": 4,               # char, unsigned char
        "F": 4, "G": 4,               # short, unsigned short
        "H": 4, "I": 4,               # int, unsigned int
        "J": 4, "K": 4,               # long, unsigned long
        "M": 4,                       # float
        "N": 8, "O": 8}               # double, long double (MSVC: both 8)
PRIM2 = {"J": 8, "K": 8, "N": 4, "W": 4}   # __int64, unsigned __int64, bool, wchar_t

# sizeof for the by-value aggregates that actually reach a stub's parameter list.
# Only needed for BY-VALUE use -- behind a pointer or reference the size is
# irrelevant and the parser deliberately does not ask for it.
UDT_SIZE = {"tagRECT": 16, "tagPAINTSTRUCT": 64, "Pair16": 4, "TileGridPos": 4}

STATIC_ACCESS = set("CDKLST")   # static member functions: no cv char follows
FREE_ACCESS = set("YZ")         # free functions: no cv char follows
CALLCONV = {"A": "cdecl", "B": "cdecl", "C": "pascal", "D": "pascal",
            "E": "thiscall", "F": "thiscall", "G": "stdcall", "H": "stdcall",
            "I": "fastcall", "J": "fastcall"}


class Unparsed(Exception):
    pass


class _Mangled(object):
    def __init__(self, s):
        self.s, self.i = s, 0
        self.backrefs = []          # types encoded in >1 char, in order of first use

    def peek(self):
        return self.s[self.i] if self.i < len(self.s) else ""

    def get(self):
        c = self.peek()
        if not c:
            raise Unparsed("ran off the end of %r" % self.s)
        self.i += 1
        return c

    def qname(self):
        """Consume a qualified name terminated by '@@'; return its leaf identifier."""
        end = self.s.find("@@", self.i)
        if end < 0:
            raise Unparsed("unterminated name in %r" % self.s[self.i:])
        leaf = self.s[self.i:end].split("@")[0]
        self.i = end + 2
        return leaf

    def type(self, record=True, need_size=True):
        """Return (stack_size, register_eligible).

        `need_size` is False whenever the type is only being consumed to find
        where it ends -- a pointee, a reference target, a function-pointer's own
        return type.  An aggregate with no known sizeof is fine there.
        """
        c = self.get()
        if c.isdigit():                       # back-reference to an earlier type
            n = int(c)
            if n >= len(self.backrefs):
                raise Unparsed("back-ref %d with only %d recorded" % (n, len(self.backrefs)))
            return self.backrefs[n]
        start = self.i - 1
        if c == "?":                          # cv-qualified non-pointer type
            self.get()
            return self.type(record, need_size)
        if c in PRIM:
            return (PRIM[c], PRIM[c] <= 4)    # single-char primitives are never back-refs
        if c == "_":
            c2 = self.get()
            if c2 not in PRIM2:
                raise Unparsed("unknown extended primitive _%s" % c2)
            r = (PRIM2[c2], PRIM2[c2] <= 4)
        elif c in "PQRS":                     # pointer, in its various cv flavours
            k = self.get()
            if k in "678":                    # pointer to (member) function
                if k == "8":
                    self.qname()              # owning class
                    self.get()                # cv char
                self.get()                    # calling convention
                self.type(record=False, need_size=False)   # return type
                self.args()
            else:
                self.type(record=False, need_size=False)   # pointee
            r = (4, True)
        elif c in "AB":                       # reference
            self.get()                        # cv char
            self.type(record=False, need_size=False)
            r = (4, True)
        elif c in "TUV":                      # union / struct / class, BY VALUE
            nm = self.qname()
            if not need_size:
                r = (0, False)
            elif nm in UDT_SIZE:
                r = ((UDT_SIZE[nm] + 3) & ~3, False)   # aggregates never take a register
            else:
                raise Unparsed("no sizeof for by-value aggregate %r" % nm)
        elif c == "W":                        # enum
            self.get()                        # underlying-type digit (4 = int)
            self.qname()
            r = (4, True)
        else:
            raise Unparsed("unknown type code %r in %r" % (c, self.s))
        if record and self.i - start > 1:
            self.backrefs.append(r)
        return r

    def args(self):
        """Parse a parameter list; return [(size, register_eligible), ...]."""
        if self.peek() == "X":                # void: encoded bare, with no '@'
            self.get()
            if self.peek() == "Z":
                self.get()
            return []
        out = []
        while self.peek() and self.peek() not in "@Z":
            out.append(self.type())
        if self.peek() == "Z":                # ellipsis
            self.get()
        if self.peek() == "@":
            self.get()
        if self.peek() == "Z":
            self.get()
        return out


def mangled_pop(sym):
    """Bytes a stub for a mangled `sym` must pop.  Raises Unparsed if unsure."""
    end = sym.find("@@")
    if not sym.startswith("?") or end < 0:
        raise Unparsed("not a mangled function name")
    p = _Mangled(sym[end + 2:])
    access = p.get()
    if access not in FREE_ACCESS and access not in STATIC_ACCESS:
        p.get()                               # cv char: non-static members only
    cc = CALLCONV.get(p.get())
    if cc is None:
        raise Unparsed("unknown calling convention in %r" % sym)
    if p.peek() == "@":                       # constructor / destructor: no return type
        p.get()
        hidden = 0
    else:
        rsz = p.type(record=False)[0]
        # A by-value aggregate return that is not 1/2/4/8 bytes comes back through
        # a caller-pushed hidden pointer, which a callee-cleanup callee also pops.
        hidden = 4 if rsz not in (0, 1, 2, 4, 8) else 0
    args = p.args()
    if cc == "cdecl":
        return 0                              # caller cleans up
    if cc == "fastcall":
        regs, pushed = 2, 0
        for sz, eligible in args:
            if eligible and sz <= 4 and regs:
                regs -= 1                     # goes in ecx, then edx
            else:
                pushed += sz
        return pushed + hidden
    return sum(sz for sz, _ in args) + hidden


def callee_pop(sym):
    """Bytes any code stub for `sym` must pop; 0 (a bare `ret`) when unknown."""
    if sym.startswith("?"):
        try:
            return mangled_pop(sym)
        except Unparsed as e:
            sys.stderr.write("gen_stubs: bare ret for %s (%s)\n" % (sym, e))
            return 0
    return stdcall_pop(sym) or 0


def coff_name(name, strings):
    b = name.encode("ascii", "strict")
    if len(b) <= 8:
        return b.ljust(8, b"\0")
    off = 4 + sum(len(s) + 1 for s in strings)
    strings.append(b)
    return struct.pack("<II", 0, off)


def main():
    sym_file, out_obj = sys.argv[1], sys.argv[2]
    with open(sym_file) as f:
        names = [l.strip() for l in f if l.strip() and not l.startswith("__imp__")]
    table = load_addr_table()

    # --- .text: one body PER CODE SYMBOL, so each can name itself when called:
    #
    #       mov  eax, [esp]          ; caller's return address
    #       push eax
    #       push offset <name>       ; -> .rdata, DIR32 relocation
    #       call _Stub_Report        ; -> external, REL32 relocation
    #       add  esp, 8
    #       xor  eax, eax            ; a stub cannot know the answer; 0 at least
    #       ret [N]                  ;   tends to hit callers' null checks
    #
    # Sharing one body per pop count would be smaller, but this obj is scaffolding
    # that never ships -- knowing WHICH missing function the app just asked for is
    # worth far more than its size.
    text = bytearray()
    rdata = bytearray()
    text_relocs = []        # (offset_in_text, symbol_index, type)
    code_off = {}
    for n in names:
        if not is_code(n):
            continue
        code_off[n] = len(text)
        name_off = len(rdata)
        rdata += n.encode("ascii", "replace") + b"\0"
        text += b"\x8b\x04\x24"                    # mov eax, [esp]
        text += b"\x50"                            # push eax
        text += b"\x68" + struct.pack("<I", name_off)   # push offset name
        text_relocs.append((len(text) - 4, SYM_RDATA, REL_DIR32))
        text += b"\xe8" + struct.pack("<I", 0)     # call _Stub_Report
        text_relocs.append((len(text) - 4, SYM_REPORT, REL_REL32))
        text += b"\x83\xc4\x08"                    # add esp, 8
        text += b"\x33\xc0"                        # xor eax, eax
        pop = callee_pop(n)
        text += b"\xc3" if pop == 0 else struct.pack("<BH", 0xC2, pop)

    # --- .bss: the mirror, plus an overflow area for unplaceable data stubs.
    placed, unplaced = [], []
    for n in names:
        if is_code(n):
            continue
        a = data_addr(n, table)
        if a is not None and MIRROR_BASE <= a < MIRROR_END:
            placed.append((n, a - MIRROR_BASE))
        else:
            unplaced.append(n)
    bss_size = MIRROR_END - MIRROR_BASE
    for i, n in enumerate(unplaced):
        placed.append((n, bss_size + i * OVERFLOW_STRIDE))
    bss_size += len(unplaced) * OVERFLOW_STRIDE
    data_off_of = dict(placed)

    # --- Symbol table.  The three section symbols come first (each burns TWO
    #     slots: the symbol plus its aux record), then the external _Stub_Report,
    #     then one symbol per stub -- the relocations above index into this by
    #     SYM_RDATA / SYM_REPORT, so the order here is load-bearing.
    strings, syms = [], []
    nsyms = [0]             # entries, NOT list elements: an aux record counts as one

    def section_symbol(name, index, size, nreloc):
        syms.append(coff_name(name, strings)
                    + struct.pack("<IhHBB", 0, index, 0, 3, 1)      # IMAGE_SYM_CLASS_STATIC
                    + struct.pack("<IHHIhh2x", size, nreloc, 0, 0, 0, 0))
        nsyms[0] += 2

    section_symbol(".text", 1, len(text), len(text_relocs))
    section_symbol(".bss", 2, bss_size, 0)
    section_symbol(".rdata", 3, len(rdata), 0)
    syms.append(coff_name("_Stub_Report", strings)
                + struct.pack("<IhHBB", 0, 0, 0x20, 2, 0))          # external, function
    nsyms[0] += 1

    for n in names:
        if is_code(n):
            sec, val = 1, code_off[n]
        else:
            sec, val = 2, data_off_of[n]
        syms.append(coff_name(n, strings) + struct.pack("<IhHBB", val, sec, 0, 2, 0))
        nsyms[0] += 1

    nsec = 3
    hdr_size = 20 + 40 * nsec
    text_off = hdr_size                     # .bss is uninitialised: no raw bytes
    reloc_off = text_off + len(text)
    rdata_off = reloc_off + 10 * len(text_relocs)
    sym_off = rdata_off + len(rdata)
    strtab = (struct.pack("<I", 4 + sum(len(s) + 1 for s in strings))
              + b"".join(s + b"\0" for s in strings))

    out = struct.pack("<HHIIIHH", 0x14C, nsec, 0, sym_off, nsyms[0], 0, 0)
    # .text: code, execute|read, 16-byte aligned
    out += b".text\0\0\0" + struct.pack("<IIIIIIHHI", 0, 0, len(text), text_off,
                                        reloc_off, 0, len(text_relocs), 0, 0x60500020)
    # .bss: uninitialised data, read|write, 16-byte aligned (PointerToRawData 0)
    out += b".bss\0\0\0\0" + struct.pack("<IIIIIIHHI", 0, 0, bss_size, 0,
                                         0, 0, 0, 0, 0xC0500080)
    # .rdata: initialised data, read-only -- the stub name strings
    out += b".rdata\0\0" + struct.pack("<IIIIIIHHI", 0, 0, len(rdata), rdata_off,
                                       0, 0, 0, 0, 0x40300040)
    out += bytes(text)
    for off, sym_index, rtype in text_relocs:
        out += struct.pack("<IIH", off, sym_index, rtype)
    out += bytes(rdata) + b"".join(syms) + strtab
    with open(out_obj, "wb") as f:
        f.write(out)
    code_syms = [n for n in names if is_code(n)]
    popping = sum(1 for n in code_syms if callee_pop(n))
    print("gen_stubs: %d symbols -> %s (%d code, %d data in a %d-byte mirror; "
          "%d code stubs pop a nonzero amount, %d data stubs with no known "
          "address; every code stub reports itself to Stub_Report)"
          % (len(names), out_obj, len(code_syms),
             len(names) - len(code_syms), bss_size,
             popping, len(unplaced)))


if __name__ == "__main__":
    main()
