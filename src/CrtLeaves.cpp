// CRT helper leaves -- small VC++ 5.0 C-runtime support routines whose callers are all
// CRT internals (confirmed via xrefs), transcribed for byte-match completeness. Not game
// code; no owning game class exists. Moved out of src/phase2_probe2.cpp 2026-07-22
// (v322).

// --- 0x470c30: free function, zero 3 dwords through a pointer ---
// Callers: CRT __ld12cvt (x3) -- the long-double conversion helper zeroes its working
// registers through the result pointer.
// PARKED (v2, re-probed v352): EFFECTIVE -- same eax/ecx symmetric-register-swap class as
// 0x45ca10 (src/UIResources.cpp). align=0, insns 6/6, DIFF(5): the original puts the
// POINTER in ecx and the zero in eax, this compile the reverse. v352 also tried the chained
// form `p[2] = p[1] = p[0] = 0;` (which keeps the constant live in the accumulator across
// all three stores, and whose evaluation order is ascending, matching the original's store
// order): byte-identical DIFF either way. See docs/PARKED.md.
// FUNCTION: LOCO 0x470c30
void ZeroThree0x470c30(int *p) {  // TODO: sync
    p[0] = 0;
    p[1] = 0;
    p[2] = 0;
}

// --- 0x46c480: free function, pop-and-advance a dword cursor ---
// Callers: CRT _output (printf core, x10) -- walks the varargs cursor.
// EXACT since v352 (was parked from v2 as an "eax/ecx/edx register choice and ADD-vs-LEA
// instruction selection" tie). It was neither: the original DESTROYS the old cursor value
// (`add ecx,4`) and then reads the popped element back as `[eax-4]`, so `p` is never
// separately live. The v2 source kept `p` and the advanced pointer live at once
// (`int *p = *cursor; *cursor = p + 1; return *p;`), which is what forced the
// non-destructive `lea edx,[eax+4]`. Advancing the cursor IN PLACE first and indexing
// back with [-1] reproduces the original exactly, first compile.
// FUNCTION: LOCO 0x46c480
int PopCursor0x46c480(int **cursor) {  // TODO: sync
    *cursor += 1;
    return (*cursor)[-1];
}

// --- 0x46a850: free function, set two fields via a raw pointer (plain RET -> __cdecl) ---
// Callers: CRT __getptd / _beginthreadex / __mtinit -- per-thread data (_tiddata-like,
// calloc(1,0x74)) initialization: +0x50 gets the CRT global at 0x4844e0, +0x14 = 1.
struct InitObj0x46a850 {
    char pad0x14[0x14];
    int m_0x14;
    char pad0x50[0x50 - 0x18];
    void *m_0x50;
};

// FUNCTION: LOCO 0x46a850
void InitFields0x46a850(InitObj0x46a850 *p) {
    p->m_0x50 = (void *)0x4844e0;
    p->m_0x14 = 1;
}
