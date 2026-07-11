// UIResources' own transcribed methods (the shared registry singleton DAT_004855e8, see
// UIResources.h).
#include "UIResources.h"

// FUNCTION: LOCO 0x445f70
// PARTIAL -- see the declaration's note in src/UIResources.h. The original's first act is a
// vtable store at +0x00 that this class cannot emit while pad0x0 stands in for the vptr, so
// this will not byte-match until that is resolved. Everything else is the original's own work,
// in the original's own order: chain m_rfIndex's ctor (implicit here, explicit at 0x445f77),
// zero the whole descriptor table, IDENTITY-fill the redirect table, then clear the four font
// handles from the top down.
UIResources::UIResources() {
    int i;

    memset(m_apKindDescriptors, 0, sizeof(m_apKindDescriptors));
    // 0x4000, not 0x4001: the tail slot both arrays carry is never a valid kindId (the range
    // check is `> 0x3fff`), and the original's loop counter is literally 0x4000.
    for (i = 0; i < 0x4000; i++) {
        m_pKindSlotPtrsMaybe[i] = &m_apKindDescriptors[i];
    }
    m_hFont24 = NULL;
    m_hFont16 = NULL;
    m_hFont14 = NULL;
    m_hFont12 = NULL;
}

// FUNCTION: LOCO 0x447290
// EXACT MATCH (moved out of src/phase2_probe4.cpp 2026-07-22, where it lived as the
// probe-local SetPtrObj0x447290::SetSlot). Owner identified via its sole caller
// (FUN_0041f970, the easter-egg date checker), which passes &DAT_004855e8 -- the
// UIResources singleton, same `this` as TileKind_GetOrLoadDescriptor (0x446ea0), whose own
// decompile confirms the +0x2c per-kindId pointer table and the +0x10030 backing array.
void UIResources::SetKindSlotPtrMaybe(int a, int b) {
    m_pKindSlotPtrsMaybe[a] = &m_apKindDescriptors[b];
}

// --- 0x446030: TileKind_GetCategory (already named in Ghidra; plain __cdecl), the
// TileKind-family helper that extracts the coarse category ((kindId>>10)&0xFF-ish, clamped
// < 0x10) -- PARKED, still commented out. Moved here from src/phase2_probe4.cpp 2026-07-22
// alongside SetKindSlotPtrMaybe: the whole TileKind family is backed by this same
// UIResources registry singleton. Not compiled today; NetSessionEventQueue.cpp calls it via
// its own extern declaration.
//
// The original, in full (22 B):
//     mov eax,[esp+4] ; sar eax,0xa ; mov BYTE [esp+4],al ; cmp al,0x10
//     mov ecx,[esp+4] ; sbb al,al   ; and eax,ecx        ; ret
//
// v356 RE-DIAGNOSED THE SPILL, and it is NOT a register-pressure spill at all: it is
// **VC5's byte -> int WIDENING idiom**, which is a memory round-trip, not `movzx`.
// Ground truth is 0x446883-0x44689b inside TileKind_CreateDescriptor, where the same
// clamp's result feeds a `switch` and so genuinely needs a full int:
//     mov [esp+0x24],al ; mov eax,[esp+0x24] ; and eax,0xff ; cmp eax,0xe ; ja ...
// i.e. store-byte / load-dword / `and 0xff`, with the `and 0xff` applied LAZILY -- only
// when a consumer actually needs the upper 24 bits zeroed. Inside the clamp itself the
// consumer is byte-wide, so the mask is omitted and only the store+reload pair survives.
// So the source feature to reproduce is "the byte variable is used as an int operand",
// not "something forced a spill".
//
// TWENTY-NINE source shapes are now probed (v355's eight: explicit mask-and in both
// operand orders, `c * (c<16)`, a byte-typed mask local, a signed-char mask local,
// `if (c>=16) c=0;`, the parameter self-assign, both return types; plus v356's twenty-one:
// `-(unsigned char)(c<16)` at both return widths, a separate mask local, an address-taken
// local, `mask=0; mask-=(c<16);` at char/int/unsigned widths, `(c>=16)?0:c`, `(c<16)?0xFF:0`,
// `~((c<16)-1)` at both widths, `bool`, `c <= 15`, an unsigned-borrow rewrite, and four
// EXPLICIT-widening forms -- `int v = c;`, `(int)c &`, `unsigned int v = c;`,
// `-b & (unsigned int)c` -- in both operand orders).
//
// BEST KNOWN SHAPE (the one below): `unsigned char b = (c < 16); return c & -b;`
// asmscore --len 22: total 28014, align=28 **reg_pen=0 identity_miss=0**, insns **8/8**.
// The prologue, the `cmp al,0x10`, the `and eax,ecx` OPERAND ORDER and the epilogue are
// byte-identical; every other shape scores worse (the `if (c>=16) c=0;` form v355 recorded
// as "closest at DIFF(7)" is an artifact of its 14-byte body being 8 B SHORTER than the
// original -- on the fair `--len 22` basis it is 32011 vs. this shape's 28014).
//
// The ONE residual is a single hard plateau, identical for all 29 shapes: VC5 always
// materializes the comparison as a 0/1 bool and then negates it
// (`sbb ecx,ecx ; neg ecx ; neg ecx`, a 32-bit round-trip) instead of emitting the direct
// -1/0 mask (`sbb al,al`). Because its mask never lands in AL it never clobbers the value's
// register, so it never needs the widening round-trip either -- the missing spill/reload
// pair is a CONSEQUENCE of the missing byte-width `sbb`, not an independent difference.
// Do NOT re-probe this on the operand-shape, mask-type or explicit-widening axes; the
// remaining question is what makes VC5 emit a byte-width `sbb r8,r8` at all.
// unsigned char __cdecl TileKind_GetCategory(int kindId) {
//     unsigned char nCategory = (unsigned char)(kindId >> 10);
//     unsigned char bInRange = (nCategory < 16);
//     return nCategory & -bInRange;
// }

// --- 0x45ca10: RFIndex's default constructor ----------------------------------------------
// IDENTIFIED v358, and it dissolves a long-standing mystery. This was parked since v2 as an
// anonymous "partial zero-init of UIResources +0x18/+0x1c/+0x24, skipping +0x20", modeled on a
// TU-local placeholder struct, with an EFFECTIVE autopsy blaming the symmetric-register-swap
// class (Yoda #29/#30) because "the original copies `this` to eax and zeroes via ecx".
//
// It is simply **`RFIndex::RFIndex()`**. Once RFIndex is modeled by value at UIResources +0x18
// (see UIResources.h), the three zeroed dwords are pFile / pRecords / pRfdPath0xc and the
// skipped one is Unk0x8 -- which RFIndex::Open zeroes itself. Its sole caller, 0x445f70, is
// UIResources' own constructor invoking it on this+0x18: an ordinary embedded-member ctor call.
// And the `mov eax,ecx` prologue that read as a register-allocation coin-flip is not one at all
// -- it is the MSVC CONSTRUCTOR RETURN CONVENTION (a ctor returns `this` in EAX), which no
// void-returning method can ever reproduce. Writing it as a real constructor makes it exact.
//
// General lesson: **a `mov eax,ecx` prologue plus every field store going through EAX plus a
// bare `ret` is the signature of a CONSTRUCTOR, not of an unlucky allocator.** Before parking
// any tiny this-zeroing function as a register-swap tie-break, check whether its caller is a
// constructor invoking it on an interior offset.
//
// (Moved out of src/phase2_probe2.cpp 2026-07-22 (v322, was ZeroObj0x45ca10); the placeholder
// struct UIResourcesHdr0x18Maybe it carried is retired. The marker stays in this TU for now --
// the function's real home is the RF-archive TU alongside RFIndex::Open/LoadResource.)
// FUNCTION: LOCO 0x45ca10
RFIndex::RFIndex() {
    pFile = 0;
    pRecords = 0;
    pRfdPath0xc = 0;
}

// --- 0x45caa0: RFIndex::Open -- the RF archive's index parser --------------------------
// Swaps the caller's path extension for ".RFH" and walks that file's
// [u32 namelen][name][u32 size][u32 flags] records, appending each to the singly-linked
// index list; then closes it and reopens the same base path as ".RFD", whose handle stays
// open for the lifetime of the game (RFIndex::LoadResource/0x45cd00 seeks into it).
// Sole caller: UIResources::Init (0x446050), itself reached once from 0x406ba0 -- the
// archive is opened exactly once, early, and there is no close path anywhere.
//
// EXACT. Two source shapes were load-bearing and neither is guessable from the decompile:
//  (1) `nFlags` doubles as the fread RETURN-COUNT scratch for the namelen read. The original
//      SPILLS that count to the stack (`mov [esp+0x14],eax`) before testing it, which /O2
//      would never do for a value used only by the following `test` -- unless the slot is
//      address-taken later. It is: the 4th fread reads the flags word back into the very same
//      slot. One variable, two uses; two separate locals do not reproduce the spill.
//  (2) `Unk0x8 = 0;` is duplicated into BOTH arms of the final if/else rather than written
//      once above it. cl hoists the common store back above the branch either way, so the
//      emitted store is identical -- but only the duplicated form leaves a zero REGISTER live
//      across the branch, which is what makes the exit `cmp ecx,eax` + two private epilogues
//      instead of `test eax,eax; setne al`. Written the natural way (store once, then
//      `if (pFile == NULL) return 0; return 1;`) cl folds both returns into a single setne
//      epilogue and the function is 8 instructions short -- and NO return-statement spelling
//      fixes it (7 probed: else-form, polarity flip, `return pFile != NULL`, `return IsOpen()`,
//      the ?: ternary, a bool local, and an explicit NULL local were all equally folded).
//      See docs/CODEGEN.md #18p.
#include <stdio.h>
#include <string.h>

// FUNCTION: LOCO 0x45caa0
unsigned char RFIndex::Open(char *path) {
    char szPath[400];
    char szName[400];

    if (pFile != NULL) {
        fclose(pFile);
        pFile = NULL;
        Unk0x8 = 0;
    }
    strcpy(szPath, path);

    char *pExt = szPath + strlen(szPath);
    while (*pExt != '.' && pExt != szPath) {
        --pExt;
    }
    if (pExt == szPath) {
        return 0;
    }
    ++pExt;

    sprintf(pExt, "RFH");
    pFile = fopen(szPath, "rb");
    if (pFile == NULL) {
        return 0;
    }

    while (!feof(pFile)) {
        unsigned int nNameLen;
        unsigned int nSize;
        unsigned int nFlags;

        nFlags = fread(&nNameLen, 1, 4, pFile); // reused as the read-count scratch, see note (1)
        if (nFlags != 0) {
            fread(szName, 1, nNameLen, pFile);
            fread(&nSize, 1, 4, pFile);
            fread(&nFlags, 1, 4, pFile);

            RFRecord *pRec = new RFRecord;
            pRec->pName = (char *)_malloc(nNameLen);
            strcpy(pRec->pName, szName);
            pRec->flags = nFlags;
            pRec->size = nSize;
            pRec->pNext = NULL;

            if (pRecords == NULL) {
                pRecords = pRec;
            } else {
                // sic: the tail is re-walked from the head for every record, making the
                // whole parse O(n^2) in the ~4000-entry index. An original characteristic.
                RFRecord *pTail = pRecords;
                while (pTail->pNext != NULL) {
                    pTail = pTail->pNext;
                }
                pTail->pNext = pRec;
            }
        }
    }
    fclose(pFile);

    sprintf(pExt, "RFD");
    pFile = fopen(szPath, "rb");
    unsigned int nRfdPathSize = strlen(szPath) + 1;
    pRfdPath0xc = (char *)_malloc(nRfdPathSize);
    strcpy(pRfdPath0xc, szPath);
    // The duplicated `Unk0x8 = 0;` is load-bearing -- see note (2) in this function's header.
    if (pFile == NULL) {
        Unk0x8 = 0;
        return 0;
    } else {
        Unk0x8 = 0;
        return 1;
    }
}

// --- 0x45cd00: RFIndex::LoadResource -- the archive's per-file reader ---------------------
// Walks the in-memory index list summing the sizes of every record BEFORE the match (the
// records are packed head-to-tail in the .RFD in index order, so that sum IS the file's data
// offset), fseek()s there, malloc()s the record size and fread()s it. An uncompressed record
// (flags == 0) is handed back as-is; a Huffman-compressed one is inflated into a second
// buffer sized by the uncompressed length the compressor stored as the blob's leading dword
// (DerefIntMaybe), and the compressed copy freed. outSize receives the size the CALLER
// should consume: the record size on the plain path, whatever the decompressor reports on
// the compressed one.
// EXACT. The one load-bearing shape: the scan is a single `while (pRec != NULL &&
// _stricmp(...) != 0)` -- an outer `if (pRec != NULL)` guard around any do/for(;;) form makes
// /Og peel the first iteration (a private second _stricmp call site) and tail-merge the
// in-loop null break straight into `return 0`, neither of which the original does.
// Callees: 0x471480 = the real CRT _stricmp; 0x468790/0x468610 = lock-wrapped fseek/fread;
// 0x45c830 = Rf_HuffmanDecompress (transcribed immediately below).
extern int DerefIntMaybe(int *p);                                             // 0x45c820 (src/GeomUtil.cpp)

// FUNCTION: LOCO 0x45c830
// The RF archive's Huffman decoder -- and the ONLY hand-written assembly found in the app
// region so far. It is not a codegen curiosity to be worked around: `cld`/`lodsd` string ops
// and the `sar`/`adc`/`lea` bit-extraction triple are constructs cl emits from no C whatever,
// so the original source is an `__asm` block and this is a verbatim transcription of it.
//
// Stream layout, cross-checked against loco/rf-extract.py's huff_decompress() (a working
// reference decoder for the same format): [0] u32 decompressed size, [4] u32 root node,
// [8] a 0x800-byte node table of u16 pairs, [0x808] the LSB-first bitstream. Each step
// indexes tree[node * 4 + bit * 2]; a node below 0x100 is a terminal and IS the literal byte,
// after which the walk restarts from the root. The one deviation from the reference: bits are
// refilled a DWORD at a time (`dec ecx` counting down from 32) rather than a byte at a time --
// same bit order, wider refill.
//
// Two idioms worth not re-deriving. `dec edx` in the preamble pre-decrements the write pointer
// so that the per-symbol `inc edx` at the top of the loop lands it on the first output byte
// (there is no separate increment after the store). And `mov bx, [edi+ebx]` writes only the low
// half of ebx, which is safe ONLY because the table is 0x800 bytes: node <= 0x1ff, so the
// `lea ebx,[ebx+ebx]` index stays under 0x800 and never leaves stale high bits behind to defeat
// the `cmp ebx, 100h` terminal test.
//
// The trailing `*pnOutSize` store is C, not asm: the compiler interleaves its own `pop edi/esi/
// ebx` between those instructions, which it would never do inside a verbatim __asm block.
void Rf_HuffmanDecompress(void *pCompressed, void *pOut, int *pnOutSize)
{
    int nRoot;
    int nRemaining;

    __asm {
            mov     esi, pCompressed
            mov     edx, pOut
            cld
            lodsd                       // u32 decompressed size
            mov     nRemaining, eax
            lodsd                       // u32 root node
            mov     nRoot, eax
            mov     edi, esi            // edi -> node table (pCompressed + 8)
            add     esi, 800h           // esi -> bitstream (pCompressed + 0x808)
            mov     ecx, 20h
            mov     eax, [esi]
            add     esi, 4
            dec     edx                 // pre-decrement; the inc below re-lands on byte 0
    next_symbol:
            mov     ebx, nRoot
            inc     edx
    next_bit:
            cmp     ebx, 100h
            jl      short emit_byte     // node < 0x100 -> terminal, bl IS the literal
            sar     eax, 1              // shift the next bit into CF
            adc     ebx, ebx            // ebx = node * 2 + bit
            lea     ebx, [ebx+ebx]      // ebx = node * 4 + bit * 2
            dec     ecx
            mov     bx, [edi+ebx]
            jne     short next_bit
            mov     ecx, 20h            // 32 bits consumed -> refill
            mov     eax, [esi]
            add     esi, 4
            jmp     short next_bit
    emit_byte:
            dec     nRemaining
            mov     [edx], bl
            jne     short next_symbol
    }

    *pnOutSize = *(int *)pCompressed;
}

// FUNCTION: LOCO 0x45cd00
void *RFIndex::LoadResource(const unsigned char *name, int *outSize) {
    if (pFile == NULL) {
        return 0;
    }
    RFRecord *pRec = pRecords;
    unsigned int nDataOffset = 0;
    while (pRec != NULL && _stricmp(pRec->pName, (const char *)name) != 0) {
        nDataOffset += pRec->size;
        pRec = pRec->pNext;
    }
    if (pRec == NULL) {
        return 0;
    }
    fseek(pFile, nDataOffset, SEEK_SET);
    void *pBuf = _malloc(pRec->size);
    if (pBuf == NULL) {
        return 0;
    }
    fread(pBuf, 1, pRec->size, pFile);
    if (pRec->flags == 0) {
        *outSize = pRec->size;
        return pBuf;
    }
    void *pOutBuf = _malloc(DerefIntMaybe((int *)pBuf));
    if (pOutBuf == NULL) {
        _free(pBuf);
        return 0;
    }
    Rf_HuffmanDecompress(pBuf, pOutBuf, outSize);
    _free(pBuf);
    return pOutBuf;
}

// --- 0x447400: TickStationClockChimeMaybe -- the station-clock chime tick ---------------
#include <errno.h>

#include "LocoBitmap.h"      // LocoBitmap::pPixels/width (the blit's dest base/pitch)
#include "WorldBoardMaybe.h" // g_worldBoard (rcViewport, dwHalfWidth/dwHalfHeight)
#include "DSound.h"          // g_pDSoundManager->AcquireChannelForSound

// The two lazily-preloaded station-clock chime sound-bank entries: DAT_004a64c8 is the
// full-hour chime (wav 0x53ab, fired on 5-minute step 0), DAT_004a6480 the quarter-hour
// chime (wav 0x5399, steps 3/6/9). NULL = not yet loaded, (SoundBankEntry *)-1 = load
// failed (error sentinel, sets errno = ENOENT).
extern SoundBankEntry *DAT_004a64c8;
extern SoundBankEntry *DAT_004a6480;

// RETIRED v577: `struct UIResourcesView0x447400` was a TU-local methods-only view carrying
// TickStationClockChimeMaybe plus the two UIResources methods it calls. It was redundant AND a
// live defect: TileKind_GetOrLoadDescriptor and SoundBank_PreloadWavRange were ALREADY declared
// on the shared UIResources (UIResources.h), and SoundBank_PreloadWavRange is DEFINED on it in
// this very file -- with a different return type (unsigned char, not the view's unsigned int).
// So every call through the view mangled to a class no TU defines and, in the port, ran a
// gen_stubs stub instead of the real body. The v340 rule the old comment cited ("ANY new method
// declaration in UIResources.h rotates DPlaySessionMgr.cpp's /Og TU state") was RE-MEASURED this
// session and is STALE: declaring TickStationClockChimeMaybe there is byte-free.

// TU-local methods-only callee view: LocoBitmap::FUN_0042c330 (0x42c330, not yet
// transcribed anywhere) -- a raw 8bpp color-key blit (source index 0 = transparent) with
// the same RECT-by-value idiom as LocoBitmap.h's PixelCopyBlit family
// (`(RECT destRect, void *pDestBase, unsigned int destPitch, RECT srcRect)` -- only
// destRect.left/top and all 4 srcRect fields are read by the body).
struct LocoBitmapBlitView0x447400 {
    void FUN_0042c330(RECT destRect, void *pDestBase, unsigned int destPitch, RECT srcRect);
};

// FUNCTION: LOCO 0x447400
// EFFECTIVE-PARKED, RE-SCORED v359 UNDER THE SP3 TOOLCHAIN -- and almost all of the old
// autopsy below is now STALE. v340 measured DIFF(863), len 1338 ours vs 1316 orig, align=194
// reg_pen=152 identity_miss=211 byte_diff=344, insns 448/445, and blamed two stacked /Og
// coin-flip classes. Under SP3 this is **DIFF(12), len 1316 == 1316, align=0 reg_pen=0
// identity_miss=0, insns 445/445** -- i.e. the v337 vtable-value-CSE class and the symmetric
// esi/edi swap class BOTH evaporated with the compiler change; they were never real source
// problems. (Textbook case of pickup item 2: a toolchain-constant residual is indistinguishable
// from an allocator coin-flip from inside the function.)
//
// The ENTIRE remaining residual is 12 displacement bytes from ONE swapped pair of spill slots:
// the original puts pClockDesc at [esp+0x10] and pFrame at [esp+0x14]; we place them the other
// way round. Every other byte, including both RECTs' slots and the whole frame layout, is
// identical. (The 0x3c/0x38 and 0x20/0x24 diff sites are the same two slots seen through the
// by-value RECT pushes shifting esp.)
//
// Probed under SP3 and CONFIRMED INERT -- do not re-probe: declaration order of the two
// pointers (compiles byte-identical apart from the slot itself, so slot assignment is
// allocator-internal, not decl-driven), and per-block scoping of pFrame (VC5 scopes the whole
// function body as one scope, so this is just the distinct-per-sprite-locals shape v340 already
// refuted). Untried: anything that changes the two locals' relative SPILL WEIGHT / live-range
// structure rather than their source position.
//
// Historical detail from the v340 autopsy, kept because the structural verification still
// stands: structure faithful end-to-end (verified block-by-block against the raw
// disasm): both chime arms (lazy SoundBank_PreloadWavRangeMaybe preload on the
// g_UIResources global, the (SoundBankEntry *)-1 load-failure sentinel + errno=ENOENT,
// the per-arm g_pDSoundManager/pEntry guards with the cross-jumped shared
// AcquireChannelForSound tail), the minute/hour 12-step index arithmetic (the hour index
// REUSES the nSeconds parameter -- the original overwrites param_1's own stack slot with
// it), all 4 clock-hand sprite blits (SetRect/CopyRect/OffsetRect pairs, the
// FUN_0042c330 raw color-key blit into the clock descriptor's own pOwnedObjA
// pixels/width), and the MarkRectDirty(rcViewport) tail. The two classes v340 blamed for the
// byte mass -- (1) the v337 vtable-value-CSE class (RTM cached each sprite descriptor's vtable
// pointer in a stack slot across the GetOrLoadFrameBitmap/ReleaseRef pair, growing the frame to
// 0x30 vs 0x2c and cascading a 4-byte shift into every stack reference) and (2) the symmetric
// esi/edi register-role swap -- are BOTH GONE under SP3; the frame is 0x2c and the register
// roles match. Levers that landed and are still kept: g_UIResources (not `this`) as the
// SoundBank_PreloadWavRangeMaybe receiver (the original's `mov ecx,0x4855e8`), the nSeconds
// param reuse, and the inverted half-hour branch (`>= 1800 ? +2 : +1`) for the original's jl
// polarity. NOTE: the function is a
// member of the TU-local UIResourcesView0x447400, not the shared UIResources -- ANY new
// method declaration in UIResources.h rotates DPlaySessionMgr.cpp's /Og TU state and
// breaks SelectGridCellFromPointMaybe's EXACT (v340 bisect; the +0x28 field addition
// alone is rotation-free and stays in the shared header).
void UIResources::TickStationClockChimeMaybe(int nSeconds, int bFlagMaybe) {
    RECT rectSrc;
    RECT rectDst;
    int nMinuteStep;
    SoundBankEntry *pEntry;
    CursorDesc *pClockDesc;
    CursorDesc *pDesc;
    LocoBitmap *pFrame;

    pClockDesc = TileKind_GetOrLoadDescriptor(0x842);
    if (pClockDesc->pOwnedObjA == NULL) {
        return;
    }
    nMinuteStep = (((nSeconds / 60) % 60) / 5 + 1) % 12;
    if (nSeconds % 3600 >= 1800) {
        nSeconds = (nSeconds / 3600 + 2) % 12;
    } else {
        nSeconds = (nSeconds / 3600 + 1) % 12;
    }
    if (nMinuteStep != m_nLastClockChimeStepMaybe) {
        if (nMinuteStep == 0) {
            m_nLastClockChimeStepMaybe = nMinuteStep;
            pEntry = DAT_004a64c8;
            if (pEntry == NULL) {
                g_UIResources.SoundBank_PreloadWavRange(0x53ab, 0x53ab);
                pEntry = DAT_004a64c8;
                if (pEntry == NULL) {
                    DAT_004a64c8 = (SoundBankEntry *)-1;
                    errno = ENOENT;
                }
            }
            if (pEntry == (SoundBankEntry *)-1) {
                errno = ENOENT;
                pEntry = NULL;
            }
            if (g_pDSoundManager != NULL && pEntry != NULL) {
                g_pDSoundManager->AcquireChannelForSound(pEntry, NULL, g_worldBoard.dwHalfWidth,
                                                         g_worldBoard.dwHalfHeight, 4, 0);
            }
        } else if (nMinuteStep == 3 || nMinuteStep == 6 || nMinuteStep == 9) {
            m_nLastClockChimeStepMaybe = nMinuteStep;
            pEntry = DAT_004a6480;
            if (pEntry == NULL) {
                g_UIResources.SoundBank_PreloadWavRange(0x5399, 0x5399);
                pEntry = DAT_004a6480;
                if (pEntry == NULL) {
                    DAT_004a6480 = (SoundBankEntry *)-1;
                    errno = ENOENT;
                }
            }
            if (pEntry == (SoundBankEntry *)-1) {
                errno = ENOENT;
                pEntry = NULL;
            }
            if (g_pDSoundManager != NULL && pEntry != NULL) {
                g_pDSoundManager->AcquireChannelForSound(pEntry, NULL, g_worldBoard.dwHalfWidth,
                                                         g_worldBoard.dwHalfHeight, 4, 0);
            }
        }
    }
    pDesc = TileKind_GetOrLoadDescriptor(0x3dad);
    pFrame = pDesc->GetOrLoadFrameBitmap(0, 0);
    SetRect(&rectSrc, 0, 0, pDesc->nativeWidth - 1, pDesc->nativeHeight - 1);
    CopyRect(&rectDst, &rectSrc);
    OffsetRect(&rectSrc, nMinuteStep * pDesc->nativeWidth, 0);
    OffsetRect(&rectDst, 0xf, 0x18);
    ((LocoBitmapBlitView0x447400 *)pFrame)->FUN_0042c330(rectDst, pClockDesc->pOwnedObjA->pPixels,
                                                         pClockDesc->pOwnedObjA->width, rectSrc);
    pDesc->ReleaseRef();
    pDesc = TileKind_GetOrLoadDescriptor(0x3dae);
    pFrame = pDesc->GetOrLoadFrameBitmap(0, 0);
    SetRect(&rectSrc, 0, 0, pDesc->nativeWidth - 1, pDesc->nativeHeight - 1);
    CopyRect(&rectDst, &rectSrc);
    OffsetRect(&rectSrc, nSeconds * pDesc->nativeWidth, 0);
    OffsetRect(&rectDst, 0xf, 0x18);
    ((LocoBitmapBlitView0x447400 *)pFrame)->FUN_0042c330(rectDst, pClockDesc->pOwnedObjA->pPixels,
                                                         pClockDesc->pOwnedObjA->width, rectSrc);
    pDesc->ReleaseRef();
    pClockDesc = TileKind_GetOrLoadDescriptor(0x843);
    if (pClockDesc->pOwnedObjA == NULL) {
        return;
    }
    pDesc = TileKind_GetOrLoadDescriptor(0x3db0);
    pFrame = pDesc->GetOrLoadFrameBitmap(0, 0);
    SetRect(&rectSrc, 0, 0, pDesc->nativeWidth - 1, pDesc->nativeHeight - 1);
    CopyRect(&rectDst, &rectSrc);
    OffsetRect(&rectSrc, nMinuteStep * pDesc->nativeWidth, 0);
    OffsetRect(&rectDst, 0x1f, 0x2a);
    ((LocoBitmapBlitView0x447400 *)pFrame)->FUN_0042c330(rectDst, pClockDesc->pOwnedObjA->pPixels,
                                                         pClockDesc->pOwnedObjA->width, rectSrc);
    pDesc->ReleaseRef();
    pDesc = TileKind_GetOrLoadDescriptor(0x3db1);
    pFrame = pDesc->GetOrLoadFrameBitmap(0, 0);
    SetRect(&rectSrc, 0, 0, pDesc->nativeWidth - 1, pDesc->nativeHeight - 1);
    CopyRect(&rectDst, &rectSrc);
    OffsetRect(&rectSrc, nSeconds * pDesc->nativeWidth, 0);
    OffsetRect(&rectDst, 0x1f, 0x2a);
    ((LocoBitmapBlitView0x447400 *)pFrame)->FUN_0042c330(rectDst, pClockDesc->pOwnedObjA->pPixels,
                                                         pClockDesc->pOwnedObjA->width, rectSrc);
    pDesc->ReleaseRef();
    g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
}

// --- The TU-local view carrying every UIResources method this file newly transcribes -------

#include "Ddraw.h"  // Ddraw_Teardown

// Kept OUT of UIResources.h for the documented reason (see the NOTE there), and RE-MEASURED
// in v356: declaring just the first two of these on the shared UIResources costs
// DPlaySessionMgr.cpp one EXACT (39+25 -> 38+26, 5111 B -> 4945 B), while every FIELD change
// made to that header in the same session was neutral to the byte. No data members, so this
// is UIResources' own layout -- not a second model of it.
//
// This is the file's THIRD such view, and deliberately the general one: the two older ones
// (UIResourcesView0x447400 and UIResourcesFactoryView0x446840) are each pinned to one parked
// EFFECTIVE residual, so folding them in here would rotate codegen this session has no
// measurement for. New work goes here; folding the other two in is its own measured pass.
// RETIRED: all four of this view's methods are now declared on UIResources itself. Same
// reason as the factory view above -- each of them is called from ANOTHER TU (Shutdown and
// Init from src/AppWindow.cpp, Locale_DetectLanguage from src/Main.cpp), and a TU-local view
// left every one of those calls pointing at a generated do-nothing stub.

// --- 0x4467e0: ReleaseAllCachedResources -- the registry's own cache teardown -------------
// Frees everything the two interning tables own. Both sweeps use the identical tri-state
// discipline the loaders establish: (CursorDesc *)-1 / (SoundBankEntry *)-1 is the
// "creation was attempted and failed" poison value (so the kind/sound is never retried), a
// non-NULL pointer is a live object, and NULL means "nothing here". The -1 slots are
// normalized to NULL *first* and then fall through the ordinary non-NULL test, which is why
// each slot is tested twice rather than as an if/else -- see the disasm's two consecutive
// `cmp`s against the same zero register.
//
// The kind sweep also clears the parallel alias table, whose slot for the same index sits
// exactly 0x10004 bytes lower in the object; VC5 strength-reduces the whole loop onto ONE
// pointer induction variable walking m_apKindDescriptors and reaches the alias slot as the
// negative displacement `[esi-0x10004]`.
//
// Only the kind table is swept in full. The sound sweep covers all 0x1061 slots, which is
// the WHOLE sound table -- it just looks partial in the raw disasm because the loader
// addresses that same table through the `- 0x5000` folded base (see UIResources.h).
// FUNCTION: LOCO 0x4467e0
void UIResources::ReleaseAllCachedResources() {
    int i;

    for (i = 0; i < 0x4001; i++) {
        if (m_apKindDescriptors[i] == (CursorDesc *)-1)
            m_apKindDescriptors[i] = 0;
        if (m_apKindDescriptors[i] != 0) {
            delete m_apKindDescriptors[i];
            m_apKindDescriptors[i] = 0;
        }
        m_pKindSlotPtrsMaybe[i] = 0;
    }
    for (i = 0; i < 0x1061; i++) {
        if (m_apSoundBankEntries[i] == (SoundBankEntry *)-1)
            m_apSoundBankEntries[i] = 0;
        if (m_apSoundBankEntries[i] != 0) {
            delete m_apSoundBankEntries[i];
            m_apSoundBankEntries[i] = 0;
        }
    }
}

// --- 0x446340: Shutdown -- the UI-resources subsystem teardown ----------------------------
// Runs the four owned subsystems down in order (channels, this registry's own caches,
// DirectDraw, DirectSound) and then deletes the five shared fonts. The five font blocks are
// laid out identically, and VC5 hoists the shared `DeleteObject` import thunk into EDI once
// for all of them (`mov edi, ds:[0x477048]` before the first block) -- which is what the
// import address being read into a register rather than called directly through
// `__imp__DeleteObject` looks like in the disasm.
//
// The unconditional `return 1` is genuine: the original's epilogue is a bare `mov al,1`, and
// the DeleteObject BOOL results are all discarded (Ghidra's `CONCAT31(BVar1 >> 8, 1)` is
// just the last call's return value still sitting in EAX's upper bytes -- the function's
// result is only ever AL).
// FUNCTION: LOCO 0x446340
unsigned char UIResources::Shutdown() {  // TODO: sync (TU-local view class; Ghidra has UIResources::Shutdown)
    if (g_pDSoundManager != NULL)
        g_pDSoundManager->ReleaseAllChannels();
    ReleaseAllCachedResources();
    Ddraw_Teardown();
    DSound_SaveVolumesAndShutdown();
    if (m_hFont12 != NULL) {
        DeleteObject(m_hFont12);
        m_hFont12 = NULL;
    }
    if (m_hFont14 != NULL) {
        DeleteObject(m_hFont14);
        m_hFont14 = NULL;
    }
    if (m_hFont16 != NULL) {
        DeleteObject(m_hFont16);
        m_hFont16 = NULL;
    }
    if (m_hFont24 != NULL) {
        DeleteObject(m_hFont24);
        m_hFont24 = NULL;
    }
    if (m_hFont20 != NULL) {
        DeleteObject(m_hFont20);
        m_hFont20 = NULL;
    }
    return 1;
}

// --- 0x446cc0: SoundBank_PreloadWavRange -- intern a whole WAV id range -------------------
// Walks [nFirstId, nLastId] and interns one SoundBankEntry per id into m_apSoundBankEntries,
// resolving each id's file path out of the string table. This is the loader half of the
// tri-state discipline ReleaseAllCachedResources tears down: a successfully constructed and
// loaded entry goes in the slot, and EVERY failure path -- no string resource, or an entry
// that constructed but reports bLoaded == 0 -- poisons the slot with -1 so the id is never
// retried. Returns whether the walk ran to completion (i.e. was not cut short by the
// shutdown gate).
//
// The locale remap is the same one TileKind_GetOrLoadDescriptor (0x446ea0) and its twin
// (0x4470b0) use: ids in the 100..500 "remappable" band get a per-language offset added, and
// if the remapped id has no string the raw id is retried. Case order below is the original's
// own code-layout order (v355's lever (a)); note the switch's `default` and the
// out-of-band path share one arm, which is why the original's range test jumps straight into
// the default arm rather than around the switch.
//
// ⚠ Two things in the disasm look stranger than the source is. (1) The range test reads
// `cmp ebp,0x190` / `cmp ebp,0x7d0` -- 400 and 2000, not 100 and 500 -- because VC5 folded
// the test onto the *4-scaled induction variable it already keeps for the array walk and
// scaled the constants to match. (2) The array store is `[ebx + ebp*1 + 0xc034]`, which is
// this same array with the `- 0x5000` index bias folded into the displacement
// (0x20034 - 0x5000*4); see UIResources.h. Both fall out of the natural source below.
//
// ⭐⭐ THE SHUTDOWN GATE CRACKS THE DOCUMENTED sete-MATERIALIZATION CLASS (v356). The
// original does not branch on the compare; it computes `g_nScreenState == 10` into a byte
// (`mov edx,[0x4851f4]; xor eax,eax; cmp edx,0xa; sete al; test al,al; jne`) and branches on
// THAT. Writing the test inline as `if (g_nScreenState == 10)` gives a plain `cmp; je` and
// never reproduces it -- which is exactly what has kept this class parked since v334
// (GameNet.cpp 0x4393d0, WorldBoardMaybe.cpp x2, WorldActionCursor.cpp, v335's LocoWinMain).
// The fix is a **byte-returning inline predicate**: the `unsigned char` return forces the
// value through AL, and the `if` on the returned byte is the `test al,al`. Dropping the
// predicate in here took this function from total 206415 to 149517 (DIFF 295 -> 113) and
// made the whole gate byte-identical. The shared global is read by ~40 sites at many
// different constants, so a predicate PER TESTED STATE is what the original source almost
// certainly had.
//
// EFFECTIVE-PARKED (asmscore --len 0x1e0: total 149517, align=148 reg_pen=13
// identity_miss=13 byte_diff=87, insns 140/139; candidate 472 B vs the original's 480 B =
// 437 code + 40 jump table + pad). Everything is byte-identical except ONE four-instruction
// cluster: the original CSEs the constant -1 into ESI (`or esi,0xffffffff`) and reuses that
// register for BOTH its own compiler-generated /GX EH-state restore and the source's slot
// poison, and it emits the array store BEFORE the EH-state restore rather than after. Probed
// without effect: naming the poison value as a local `SoundBankEntry *` initialized to -1,
// and folding the array store into the `new` full-expression
// (`m_apSoundBankEntries[nSlot] = pEntry = new ...`). The EH-state slot is compiler-owned,
// so the CSE that merges it with a source constant is not directly expressible.
//
// ⚠ `int nSlot` is load-bearing, not cosmetic: writing `m_apSoundBankEntries[nId - 0x5000]`
// at all four sites makes VC5 strength-reduce to a FULL element-address induction variable
// (`lea ebp,[this + i*4 + 0xc034]`), which ties up a register, spills `this` out of EBX and
// rotates the whole loop (total 206415 -> 271470). With the index in its own local, VC5 keeps
// the original's `i*4` induction variable instead -- which is also what lets it fold the
// 100..500 range test onto the same scaled register.
extern int g_nScreenState;  // app screen-state selector; 10 == shutting down (GameNetMsgQueue.h)

// The shutdown gate every loader in this subsystem opens with. Byte return type is
// load-bearing -- see the autopsy above.
inline unsigned char IsShuttingDownMaybe() { return g_nScreenState == 10; }

// FUNCTION: LOCO 0x446cc0
unsigned char UIResources::SoundBank_PreloadWavRange(
        unsigned int nFirstId, int nLastId) {
    // 0x108, not 0x104: the buffer is four bytes wider than the length cap handed to
    // LoadStringA. Ground truth, not a guess -- the same +4 shows up in all three loaders
    // in this TU, and it is exactly what makes TileKind_GetOrLoadDescriptorNoAlias (0x4470b0)
    // byte-EXACT. Reproduced verbatim rather than "fixed" to sizeof().
    char szPath[0x108];
    int nId;
    unsigned int nStringId;
    int nLen;
    int nSlot;
    SoundBankEntry *pEntry;

    if (nLastId >= 0x6060)
        nLastId = 0x6060;
    for (nId = nFirstId; nId <= nLastId; nId++) {
        if (IsShuttingDownMaybe())
            break;
        if (nId >= 100 && nId <= 500) {
            switch (m_nLocaleId) {
            case 0:
                nStringId = nId;
                break;
            case 1:
                nStringId = nId + 0x6cfc;
                break;
            case 2:
                nStringId = nId + 0x652c;
                break;
            case 4:
                nStringId = nId + 0x6338;
                break;
            case 5:
                nStringId = nId + 0x6144;
                break;
            case 6:
                nStringId = nId + 0x6914;
                break;
            case 7:
                nStringId = nId + 0x6720;
                break;
            case 8:
                nStringId = nId + 0x6ef0;
                break;
            case 9:
                nStringId = nId + 0x6b08;
                break;
            default:
                nStringId = nId;
                break;
            }
        } else {
            nStringId = nId;
        }
        nLen = LoadStringA(GetModuleHandleA(NULL), nStringId, szPath, 0x104);
        if (nStringId != (unsigned int)nId && nLen == 0)
            nLen = LoadStringA(GetModuleHandleA(NULL), nId, szPath, 0x104);
        nSlot = nId - 0x5000;
        if (nLen == 0) {
            m_apSoundBankEntries[nSlot] = (SoundBankEntry *)-1;
        } else {
            pEntry = new SoundBankEntry(nId, szPath);
            m_apSoundBankEntries[nSlot] = pEntry;
            if (pEntry != NULL && pEntry->bLoaded == 0) {
                delete pEntry;
                m_apSoundBankEntries[nSlot] = (SoundBankEntry *)-1;
            }
        }
    }
    return nId == nLastId + 1;
}

// --- 0x446840: TileKind_CreateDescriptor -- the per-kind descriptor factory --------------

#include "CarKindDesc.h"  // CarKindDesc -- the train/car kind descriptor (category 6)
#include "Obj0x478118.h"  // Obj0x478118 -- the small CursorDesc variant (categories 7/8)

// The coarse per-kind category: the kind id's top bits, clamped to 0 for anything past the
// 15 real categories. 0x446030 is this same expression's own out-of-line copy (PARKED, see
// the commented-out transcription near the top of this file) -- here it is inlined THREE
// times over (once for the switch, twice more in the accept/reject tail), which is what an
// `inline` definition in the TileKind header would do and an out-of-line call could not.
// The three expansions are the ONLY residual left in TileKind_CreateDescriptor below, and
// they are the SAME unsolved shape as the standalone 0x446030's own park: the original goes
// branchless (`mov [slot],al; cmp al,0x10; mov ecx,[slot]; sbb al,al; and eax,ecx` -- a
// byte-width sbb mask ANDed against a DWORD reload of the byte's own spill slot), while
// every source shape tried so far compiles to a real jae-branch or to a 32-bit
// `sbb eax,eax; neg; neg` bool round-trip. 29 shapes are now probed against 0x446030; the
// full list, the re-diagnosis of the "spill" as VC5's byte->int WIDENING idiom, and the
// standalone winner all live in that function's own autopsy near the top of this file.
//
// ⚠ THE STANDALONE AND IN-SITU OPTIMA ARE DIFFERENT SHAPES -- measured, v356. The mask form
// `unsigned char b = (c<16); return c & -b;` is the best STANDALONE (28014 at --len 22,
// reg_pen=0 identity_miss=0, insns 8/8) but is materially WORSE inlined here: 352802
// (insns 351/341) vs. the ternary below at 305655 (insns 357/341). `if (c>=16) c=0;` is
// worse still in situ (399236). So the ternary is KEPT here on in-situ evidence, and the
// mask form is recorded (commented out) at 0x446030 on standalone evidence. Score any
// future candidate for this expression BOTH ways -- a win at one site is not a win at the
// other, because inlining changes which register the value already occupies.
inline unsigned char TileKind_GetCategoryInline(int kindId) {
    unsigned char nCategory = (unsigned char)(kindId >> 10);
    return nCategory < 16 ? nCategory : 0;  // see the autopsy above: best IN SITU
}

// TU-local derived view carrying this method's declaration -- kept OUT of UIResources.h,
// where ANY new parameterized method declaration rotates DPlaySessionMgr.cpp's /Og TU state
// and breaks SelectGridCellFromPointMaybe's EXACT (the v340 bisect; plain field additions
// are inert, which is why m_apKindDescriptors' retype does live in the shared header). No
// data members, so this is UIResources' own layout -- not a second model of it.
// RETIRED (the descriptor-factory view): its five methods -- Init, TileKind_CreateDescriptor,
// TileKind_GetOrLoadDescriptor, TileKind_GetOrLoadDescriptorNoAlias and the shared
// TileKind_LoadDescriptorRange helper -- are now declared on UIResources itself in
// src/UIResources.h and defined below as UIResources:: members.
//
// The view was correct as long as nothing outside this TU called any of them. That stopped
// being true at the LINK, which is a level of checking the byte-match deliberately does not
// do: src/AppWindow.cpp calls Init and the other TUs call TileKind_GetOrLoadDescriptor, and
// against a TU-local view those calls resolved to a generated do-nothing stub rather than to
// the bodies sitting right here. Init's stub returning 0 is what made
// InitSubsystemsAndWindows fail and put up the fatal MessageBox instead of a frame.
//
// The declarations this adds to the shared header are the ones the v340/v356 notes there
// price -- so this trades byte-match progress for a build that can actually run. See the
// measurement recorded with this change.

// ALL FIVE ctor shims are RETIRED as of 2026-07-31. This block used to declare a TU-local
// `<Class>CtorShim0x446840` per descriptor class the factory can instantiate, on the theory
// that declaring those constructors on the real classes in the shared src/CursorDesc.h would
// rotate that header's consumers (v331/v333). The theory was half right and the conclusion was
// wrong: a shim's ctor is declared-never-defined, so every `new` below bound to a generated
// do-nothing stub instead of to the real body, and the descriptors came back unconstructed.
// The byte compare could never see it -- the emitted direct call is reloc-masked either way --
// which is exactly why this survived so long. See CODEGEN #161 and tools/lint_alias.py.
//
// The five are now CursorDesc (0x424af0, v557b), Obj0x478118 (0x436400), CarKindDesc
// (0x40e600), Obj0x4779e0 (0x41e570) and BigObj (0x44b190), each declared on its own real
// class and byte-EXACT. Measured price of the last four: ZERO.

// FUNCTION: LOCO 0x446840  // TODO: sync (Ghidra keeps TileKind::TileKind_CreateDescriptor;
// the UIResourcesFactoryView0x446840 view is TU-local, see above)
// EFFECTIVE-PARKED (v355, DIFF(809); asmscore --len 0x47c: align=304 reg_pen=14
// identity_miss=13 byte_diff=125, insns 357/341). The candidate's 1164-byte COMDAT is the
// original's own 1148-byte extent (1088 bytes of code + the 60-byte jump table) plus the 16
// bytes the three residual expansions add, so this is content-complete, not a partial draft
// -- and note --len must be 0x47c (derived from the next function's start, 0x446cc0), NOT
// Ghidra's `Body:` span, which stops at the last instruction and would clip the jump table.
// Everything from the prologue through the whole switch -- all 14 `new` arms, the /GX EH
// state ladder (states 0..0xd in code order, which is what pins the case ORDER in the source
// below), the jump-table dispatch, the cross-jumped `m_apKindDescriptors[kindId] = p` tail
// the 12 plain arms share, both special-cased store-then-flag arms, the `delete` tail and
// the epilogue -- is BYTE-IDENTICAL. The only residual is the three inlined
// TileKind_GetCategoryInline expansions (see that helper's own autopsy above); everything
// the dump reports past offset 0x440 is masked jump-table relocation noise, not a real
// disagreement. Do not re-grind this on the register axis.
//
// Interns the descriptor object for one kind id: picks the descriptor CLASS from the kind
// id's category (and, inside the paired categories, from whether the id itself is even or
// odd), constructs it from the kind's definition string, and stores it in
// m_apKindDescriptors[kindId]. Returns whether the slot ended up usable -- the three
// callers (TileKind_GetOrLoadDescriptor 0x446ea0, the resource-init pass 0x446050, and
// 0x4470b0) all accumulate the result as a "descriptors successfully created" count.
//
// A slot that already holds an object, or the failure sentinel, is left alone. Otherwise,
// after construction the object must report itself loaded (bLoadOkFlag) -- categories 1 and
// 15 are exempt, and categories 5/14 pre-set bReadyFlagMaybe so they pass -- else it is
// deleted again and the slot is poisoned with -1 so the same kind is not retried.
unsigned char UIResources::TileKind_CreateDescriptor(int kindId,
                                                                       char *pszDefinition) {
    CursorDesc *pDesc;
    unsigned char bResult = 1;

    if (m_apKindDescriptors[kindId] != NULL && m_apKindDescriptors[kindId] != (CursorDesc *)-1) {
        return 1;
    }

    switch (TileKind_GetCategoryInline(kindId)) {
    case 2:
    case 4:
        if (kindId % 2 != 0) {
            m_apKindDescriptors[kindId] = new CursorDesc(kindId, pszDefinition, 1);
        } else {
            m_apKindDescriptors[kindId] = new Obj0x4779e0(kindId, pszDefinition);
        }
        break;
    case 12:
    case 13:
        m_apKindDescriptors[kindId] = new Obj0x4779e0(kindId, pszDefinition);
        break;
    case 7:
    case 8:
        if (kindId % 2 != 0) {
            m_apKindDescriptors[kindId] = new CursorDesc(kindId, pszDefinition, 1);
        } else {
            m_apKindDescriptors[kindId] = new Obj0x478118(kindId, pszDefinition);
        }
        break;
    case 6:
        if (kindId == 0x1802) {
            break;
        }
        if (kindId < 0x1866 && kindId % 2 != 0) {
            m_apKindDescriptors[kindId] = new CursorDesc(kindId, pszDefinition, 1);
        } else {
            m_apKindDescriptors[kindId] = new CarKindDesc(kindId, pszDefinition);
        }
        break;
    case 3:
        if (kindId % 2 != 0) {
            m_apKindDescriptors[kindId] = new CursorDesc(kindId, pszDefinition, 1);
        } else {
            m_apKindDescriptors[kindId] = new BigObj(kindId, pszDefinition);
        }
        break;
    case 1:
        m_apKindDescriptors[kindId] = new CursorDesc(kindId, pszDefinition, 0);
        break;
    case 5:
        pDesc = new CursorDesc(kindId, pszDefinition, 0);
        m_apKindDescriptors[kindId] = pDesc;
        if (pDesc != NULL) {
            pDesc->bReadyFlagMaybe = 1;
        }
        break;
    case 9:
    case 10:
    case 11:
        m_apKindDescriptors[kindId] = new CursorDesc(kindId, pszDefinition, 0);
        break;
    case 14:
        pDesc = new CursorDesc(kindId, pszDefinition, 0);
        m_apKindDescriptors[kindId] = pDesc;
        if (kindId > 0x3801 && pDesc != NULL) {
            pDesc->bReadyFlagMaybe = 1;
        }
        break;
    case 0:
        break;
    default:
        m_apKindDescriptors[kindId] = new CursorDesc(kindId, pszDefinition, 0);
        break;
    }

    if (m_apKindDescriptors[kindId] != NULL && m_apKindDescriptors[kindId] != (CursorDesc *)-1 &&
        m_apKindDescriptors[kindId]->bLoadOkFlag != 1 &&
        TileKind_GetCategoryInline(kindId) != 1 && TileKind_GetCategoryInline(kindId) != 15) {
        delete m_apKindDescriptors[kindId];
        m_apKindDescriptors[kindId] = (CursorDesc *)-1;
        bResult = 0;
    }
    return bResult;
}

// --- 0x446ea0: TileKind_GetOrLoadDescriptor -- the interned per-kind descriptor lookup -----

#include <errno.h>

// FUNCTION: LOCO 0x446ea0  // TODO: sync (TU-local view class; Ghidra has
// UIResources::TileKind_GetOrLoadDescriptor)
// EFFECTIVE MATCH (asmscore --len 520: total 156287, reg_pen=2 identity_miss=2, byte_diff=67).
// The residual is ONE instruction: the original materializes `nLastId` into EDX before the
// clamp (`mov edx,eax ; cmp edx,0x4000 ; mov [esp+0x18],edx`) where this compile keeps the
// value in EAX, which is already nFirstId (`cmp eax,0x4000 ; mov [esp+0x18],eax`). That is
// 3 bytes shorter, so every downstream jump displacement shifts by 3 -- which is the whole of
// the raw DIFF; the first real disagreement is at +0x4c and there is not a second one.
// VC5 coalesced the range helper's by-value `nLastId` parameter with its `nFirstId` argument;
// the original did not. Probed without effect: writing the clamp as an if/else
// (`if (nFirstId >= 0x4000) nLastId = 0x4000; else nLastId = nFirstId;`) is strictly worse
// (176312), and hoisting the loop into the inline helper below vs. writing it flat inline
// compiles BYTE-IDENTICALLY -- so the helper split is free, and is kept because the twin
// 0x4470b0 shares it and goes byte-EXACT through it.
// The lazy half of the kind-descriptor cache. `kindId` is first routed through the alias
// table m_pKindSlotPtrsMaybe: its slot points AT an m_apKindDescriptors entry, which may
// belong to a different kind id (SetKindSlotPtrMaybe, 0x447290, is what installs such an
// alias), so the id actually loaded is recovered by subtracting the descriptor array's base
// from the slot pointer -- not by using `kindId` itself. A descriptor that is already
// interned is returned straight away; a NULL slot means "not built yet" and drives one pass
// of the same locale-remapped LoadStringA loader SoundBank_PreloadWavRange (0x446cc0) uses,
// after which the slot is re-read. Failure at any stage poisons the slot with
// (CursorDesc *)-1 so the kind is never retried, and reports errno 1 (bad kind id) or
// 2 (definition string missing / object failed to build).
//
// ⚠ The loader is written as a first..last RANGE pass even though `nLastId` can only ever
// equal `nId` here -- including the clamp, the `<=` guard and the `nCreated` accumulator,
// whose value is then discarded. That is what the original emits (the accumulator is
// genuinely dead: [esp+0x1c] is written and read only by the loop itself), and it reads as
// an inline range-loader helper instantiated with first == last -- the same helper
// SoundBank_PreloadWavRange is the wav-side copy of. Reproduced as-is.
inline int UIResources::TileKind_LoadDescriptorRange(int nFirstId,
                                                                             int nLastId) {
    char szDefinition[0x108];  // 0x108 vs the 0x104 cap -- see szPath's note above
    int nId;
    int nCreated;
    unsigned int nStringId;
    int nLen;

    nCreated = 0;
    if (nLastId >= 0x4000)
        nLastId = 0x4000;
    for (nId = nFirstId; nId <= nLastId; nId++) {
        if (IsShuttingDownMaybe())
            break;
        if (nId >= 100 && nId <= 500) {
            switch (m_nLocaleId) {
            case 0:
                nStringId = nId;
                break;
            case 1:
                nStringId = nId + 0x6cfc;
                break;
            case 2:
                nStringId = nId + 0x652c;
                break;
            case 4:
                nStringId = nId + 0x6338;
                break;
            case 5:
                nStringId = nId + 0x6144;
                break;
            case 6:
                nStringId = nId + 0x6914;
                break;
            case 7:
                nStringId = nId + 0x6720;
                break;
            case 8:
                nStringId = nId + 0x6ef0;
                break;
            case 9:
                nStringId = nId + 0x6b08;
                break;
            default:
                nStringId = nId;
                break;
            }
        } else {
            nStringId = nId;
        }
        nLen = LoadStringA(GetModuleHandleA(NULL), nStringId, szDefinition, 0x104);
        if (nStringId != (unsigned int)nId && nLen == 0)
            nLen = LoadStringA(GetModuleHandleA(NULL), nId, szDefinition, 0x104);
        if (nLen != 0)
            nCreated += TileKind_CreateDescriptor(nId, szDefinition);
        else
            m_apKindDescriptors[nId] = (CursorDesc *)-1;
    }
    return nCreated;
}

CursorDesc *UIResources::TileKind_GetOrLoadDescriptor(int kindId) {
    CursorDesc **ppSlot;
    CursorDesc *pDesc;
    int nFirstId;

    if (kindId < 0 || kindId >= 0x4000) {
        errno = 1;
    } else {
        ppSlot = m_pKindSlotPtrsMaybe[kindId];
        if (ppSlot != NULL) {
            pDesc = *ppSlot;
            if (pDesc == NULL) {
                nFirstId = ppSlot - m_apKindDescriptors;
                TileKind_LoadDescriptorRange(nFirstId, nFirstId);
                pDesc = *m_pKindSlotPtrsMaybe[kindId];
                if (pDesc == NULL) {
                    *m_pKindSlotPtrsMaybe[kindId] = (CursorDesc *)-1;
                    errno = 2;
                }
            }
            if (pDesc == (CursorDesc *)-1) {
                errno = 2;
                return 0;
            }
            return pDesc;
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x4470b0  // TODO: sync (TU-local view class; Ghidra has UIResources::FUN_004470b0)
// The unaliased twin of TileKind_GetOrLoadDescriptor above: identical contract and identical
// inlined range pass, but it indexes m_apKindDescriptors directly instead of routing kindId
// through the m_pKindSlotPtrsMaybe alias table -- so it can never resolve to another kind's
// descriptor, and it has no "no slot installed" early-out. Its five callers all sit in one
// function (0x44eac9..0x44ebc4). VC5 drops the range clamp here because kindId is already
// known < 0x4000 from the entry guard, and rotates the loop into a do/while for the same
// reason (first <= last is provable).
CursorDesc *UIResources::TileKind_GetOrLoadDescriptorNoAlias(int kindId) {
    CursorDesc *pDesc;

    if (kindId < 0 || kindId >= 0x4000) {
        errno = 1;
        return 0;
    }
    pDesc = m_apKindDescriptors[kindId];
    if (pDesc == NULL) {
        TileKind_LoadDescriptorRange(kindId, kindId);
        pDesc = m_apKindDescriptors[kindId];
        if (pDesc == NULL) {
            m_apKindDescriptors[kindId] = (CursorDesc *)-1;
            errno = 2;
        }
    }
    if (pDesc == (CursorDesc *)-1) {
        errno = 2;
        return 0;
    }
    return pDesc;
}

#include <string.h>   // strcmp, _strupr

#include "IniFile.h"  // g_pIniFile->ReadString

// FUNCTION: LOCO 0x4463c0  // TODO: sync (TU-local view class; Ghidra has
                            //   UIResources::Locale_DetectLanguage)
// Decides m_nLocaleId once, at startup, from LocoWinMain -- and it is what every other
// locale-aware loader in this file keys off. The ini wins if it names a language we ship:
// [Locale]/Language is read (defaulting to the deliberately unmatchable "NONE"), upcased in
// place, and compared against the nine shipped language names. Only if that fails -- no ini
// object at all, or a name we do not recognise -- does it fall back to the OS, mapping the
// primary language of GetSystemDefaultLCID() onto the same nine ids.
//
// The id space is 1..9 in the ini names' own alphabetical order (DANISH=1 .. SWEDISH=9), which
// is why ENGLISH's arm is the one that shares its tail with the OS switch's default: English
// is 3 either way, so an unrecognised ini name and an unsupported OS language both land on it.
//
// The nine comparisons are plain strcmp calls; VC5 inlines each one as the two-bytes-per-
// iteration `cmp/sbb/sbb` loop the disassembly shows, so all nine are open-coded.
void UIResources::Locale_DetectLanguage() {
    char szLanguage[1024];

    if (g_pIniFile != NULL) {
        g_pIniFile->ReadString("Locale", "Language", "NONE", szLanguage, sizeof(szLanguage));
        _strupr(szLanguage);
        if (strcmp(szLanguage, "DANISH") == 0) {
            m_nLocaleId = 1;
            return;
        }
        if (strcmp(szLanguage, "DUTCH") == 0) {
            m_nLocaleId = 2;
            return;
        }
        if (strcmp(szLanguage, "ENGLISH") == 0) {
            m_nLocaleId = 3;
            return;
        }
        if (strcmp(szLanguage, "FRENCH") == 0) {
            m_nLocaleId = 4;
            return;
        }
        if (strcmp(szLanguage, "GERMAN") == 0) {
            m_nLocaleId = 5;
            return;
        }
        if (strcmp(szLanguage, "ITALIAN") == 0) {
            m_nLocaleId = 6;
            return;
        }
        if (strcmp(szLanguage, "NORWEGIAN") == 0) {
            m_nLocaleId = 7;
            return;
        }
        if (strcmp(szLanguage, "SPANISH") == 0) {
            m_nLocaleId = 8;
            return;
        }
        if (strcmp(szLanguage, "SWEDISH") == 0) {
            m_nLocaleId = 9;
            return;
        }
    }
    switch (GetSystemDefaultLCID() & 0x3ff) {
    case LANG_DANISH:
        m_nLocaleId = 1;
        break;
    case LANG_DUTCH:
        m_nLocaleId = 2;
        break;
    case LANG_FRENCH:
        m_nLocaleId = 4;
        break;
    case LANG_GERMAN:
        m_nLocaleId = 5;
        break;
    case LANG_ITALIAN:
        m_nLocaleId = 6;
        break;
    case LANG_NORWEGIAN:
        m_nLocaleId = 7;
        break;
    case LANG_CATALAN:
    case LANG_SPANISH:
        m_nLocaleId = 8;
        break;
    case LANG_SWEDISH:
        m_nLocaleId = 9;
        break;
    // English is spelled out with its OWN duplicated body even though `default` already does the
    // same thing, and both halves of that are load-bearing (the rest of the function was EXACT
    // on the first attempt; this case is the whole of what DIFF(26) was). The original's byte
    // index table gives LANG_ENGLISH slot 3 and `default` slot 10, and both slots hold the same
    // block address -- so the label is really there, and it is NOT written as `case LANG_ENGLISH:
    // default:` sharing one body, which collapses the two slots into one and loses a jump-table
    // entry. Two separate `break`-terminated bodies is the only spelling that emits both slots
    // and then cross-jumps them onto one address.
    case LANG_ENGLISH:
        m_nLocaleId = 3;
        break;
    default:
        m_nLocaleId = 3;
        break;
    }
}

// FUNCTION: LOCO 0x447330
// The stand-alone locale-aware LoadStringA wrapper, and the third copy in this TU of the same
// remap-then-retry idiom the two descriptor loaders inline: string ids in [100,500] are shifted
// by a per-language offset, everything else is loaded raw, and a remapped id that comes back
// empty is retried unmapped (so a language whose table is incomplete falls back to English).
void UIResources::LoadLocaleString(UINT stringId, LPSTR buf, int bufSize) {
    UINT nMappedId;
    int nLen;

    if ((int)stringId >= 100 && (int)stringId <= 500) {
        switch (m_nLocaleId) {
        case 0:
            nMappedId = stringId;
            break;
        case 1:
            nMappedId = stringId + 0x6cfc;
            break;
        case 2:
            nMappedId = stringId + 0x652c;
            break;
        case 4:
            nMappedId = stringId + 0x6338;
            break;
        case 5:
            nMappedId = stringId + 0x6144;
            break;
        case 6:
            nMappedId = stringId + 0x6914;
            break;
        case 7:
            nMappedId = stringId + 0x6720;
            break;
        case 8:
            nMappedId = stringId + 0x6ef0;
            break;
        case 9:
            nMappedId = stringId + 0x6b08;
            break;
        default:
            nMappedId = stringId;
            break;
        }
    } else {
        nMappedId = stringId;
    }
    nLen = LoadStringA(GetModuleHandleA(NULL), nMappedId, buf, bufSize);
    if (nMappedId != stringId && nLen == 0) {
        LoadStringA(GetModuleHandleA(NULL), stringId, buf, bufSize);
    }
}

// FUNCTION: LOCO 0x446030
// The kind-id -> CATEGORY projection every descriptor consumer runs: the bits above the
// per-category 0x400-wide id block, clamped to "no category" once they run past 0xf. Four other
// TUs already declare it extern (src/CursorDesc.cpp, src/DecorActor.cpp, src/NameAnchorMaybe.cpp,
// src/WorldBoardMaybe.cpp); this is its definition, and it lands in THIS TU because 0x446030 sits
// directly in front of 0x446050 below.
//
// EFFECTIVE MATCH (asmscore --len 22: total 24336, align=24 reg_pen=3 identity_miss=2
// byte_diff=16, insns 7/8, 21 B vs 22). Semantically settled and structurally almost there --
// the original picks the same BRANCHLESS mask (`cmp al,0x10 / sbb al,al / and eax,ecx`, i.e.
// value & -(value < 0x10)), and that is what makes the parameter-reassignment spelling below the
// right one: a plain `unsigned char byCategory = ...; return byCategory < 0x10 ? byCategory : 0;`
// local instead compiles to a BRANCH (jae/xor, DIFF(21) at 28 B) and is refuted.
// The whole residual is one narrowing choice: the original keeps the projection as a BYTE in the
// parameter's own stack slot (`mov [esp+4],al`) and reloads the DWORD to AND against, where this
// build truncates in a register (`and ecx,0xff`) and never spills. That costs the reload, flips
// which register holds the mask (original: value in ECX, mask in AL; ours: mask in EAX) and drops
// one instruction.
// Eight spellings probed, all worse or equal, none reproducing the spill: `unsigned char` return
// with the ternary (DIFF 21) or with an if-zero statement (DIFF 7 but only 14 B -- a pure branch,
// not the original's shape); `byCategory * (byCategory < 0x10)` (DIFF 13, emits neg+imul);
// `byCategory & -(byCategory < 0x10)` in both byte and int return forms and with the operands
// swapped (DIFF 14 / DIFF 20, emits a redundant neg pair). The `unsigned char` return is
// well-supported by the image -- `and eax,ecx` leaves junk in the upper three bytes and every
// caller re-narrows with `(unsigned char)` -- but it changes the mangled name, so adopting it
// means touching all four consumer TUs' extern declarations AND their now-redundant casts; not
// worth spending on a 22-byte function until the spill itself is understood.
unsigned int __cdecl TileKind_GetCategory(unsigned int kindId)
{
    kindId = (unsigned char)((int)kindId >> 10);
    return kindId < 0x10 ? kindId : 0;
}

// --- 0x446050: Init -- the one-shot UI-resources bring-up pass ----------------------------

#include <string.h>
#include <time.h>

#include "Ddraw.h"    // Ddraw_Init
#include "IniFile.h"  // g_pIniFile->ReadString


// The seasonal / easter-egg unlock manager (DAT_004a99b0; polymorphic, its vtable dtor stub
// is 0x41f4d0). Owns a captured time_t (+0x4), a linked list of date-gated "unlock kind id
// K between dates A and B" records (+0x8) and the running unlocked-egg counter (+0x10) it
// mirrors into lego.ini's [EasterEggs] section. Opaque TU-local callee view -- neither
// method is transcribed yet, and both reach back into this registry
// (TileKind_GetOrLoadDescriptor / SetKindSlotPtrMaybe) to flip each unlocked kind's
// descriptor +0x163 flag.
struct EasterEggMgrMaybe {
    // 0x41f7e0 -- builds "<install><name>.ini" and reads the [EasterEggs] N=<kindId> list.
    void LoadUnlockTableMaybe(const char *pszIniBaseName);
    // 0x41f970 -- walks the date-gated record list against today's (optionally
    // g_forcedSeason-overridden) date and unlocks whatever is in season.
    void ApplySeasonalUnlocksMaybe();
};
extern EasterEggMgrMaybe g_easterEggMgrMaybe;  // DAT_004a99b0

// FUNCTION: LOCO 0x446050
// Brings the whole UI-resource layer up, once, from 0x406ba0 -- and is Shutdown's (0x446340)
// exact counterpart. In order: DirectDraw; the RF archive named by lego.ini's
// [DIRECTORIES]/ResFile; the five shared fonts; the easter-egg unlock table; every WAV in the
// 0x5000..0x6060 bank; every tile-kind descriptor from 0x400 up. Returns 0 if either of the
// two things it cannot run without -- DirectDraw and the archive -- failed.
//
// ⭐ The descriptor pass is TileKind_LoadDescriptorRange(0x400, 0x4000) -- the SAME inline
// range helper the two lazy descriptor loaders instantiate with first == last. That is the
// strongest evidence yet that the helper is real source and not a convenient fiction: here it
// is called with a genuine 15361-wide range, its `nCreated` accumulator is genuinely live
// inside the loop (`and eax,0xff; add [esp+0x14],eax` -- the created-count the lazy loaders
// compute and throw away), and its 0x4000 clamp folds out because the argument is already the
// clamp constant. The only codegen difference from the lazy loaders' expansion is that over a
// long range VC5 strength-reduces the poison store onto a pointer induction variable in a
// stack slot (`[esp+0x10]`) rather than indexing m_apKindDescriptors.
//
// Two more shapes here are documented tells rather than transcription choices. The archive
// check is the `setne`/`test` boolification (the byte-predicate class, v356) -- a bare
// `if (m_rfIndex.pFile == NULL)` compiles to a plain `cmp`/`je` and does not match. And the
// two `time()` calls are dead instrumentation: both results are discarded, and the elapsed
// time is never computed -- a load-timer whose readout was presumably cut. Reproduced as-is.
//
// The five CreateFontA calls all share one typeface buffer, which the original fills with an
// inline `strcpy` from the "Arial" literal (the `repnz scasb` + `rep movsd`/`rep movsb` pair
// at 0x4460c8 is /Oi's strcpy expansion, not hand-written code) -- so the face name was a
// local buffer in the source, not the literal passed straight through. VC5 hoists the shared
// CreateFontA import thunk into ESI across all five, and LoadStringA's into EBP across the
// whole descriptor loop.
unsigned char UIResources::Init() {  // TODO: sync (TU-local view)
    char szFontFace[52];
    char szResFile[0x104];
    time_t tLoadStart;
    time_t tLoadEnd;

    if (!Ddraw_Init()) {
        return 0;
    }
    g_pIniFile->ReadString("DIRECTORIES", "ResFile", "", szResFile, 0x104);
    m_rfIndex.Open(szResFile);
    if (!m_rfIndex.IsOpen()) {
        return 0;
    }
    strcpy(szFontFace, "Arial");
    m_hFont12 = CreateFontA(12, 0, 0, 0, 800, 0, 0, 0, DEFAULT_CHARSET, 0, 0, PROOF_QUALITY, 0,
                            szFontFace);
    m_hFont14 = CreateFontA(14, 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET, 0, 0, PROOF_QUALITY, 0,
                            szFontFace);
    m_hFont16 = CreateFontA(16, 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET, 0, 0, PROOF_QUALITY, 0,
                            szFontFace);
    m_hFont24 = CreateFontA(24, 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET, 0, 0, PROOF_QUALITY, 0,
                            szFontFace);
    m_hFont20 = CreateFontA(20, 0, 0, 0, 900, 0, 0, 0, DEFAULT_CHARSET, 0, 0, PROOF_QUALITY, 0,
                            szFontFace);
    g_easterEggMgrMaybe.LoadUnlockTableMaybe("ee");
    g_easterEggMgrMaybe.ApplySeasonalUnlocksMaybe();
    time(&tLoadStart);
    SoundBank_PreloadWavRange(0x5000, 0x6060);
    TileKind_LoadDescriptorRange(0x400, 0x4000);
    time(&tLoadEnd);
    m_nLastClockChimeStepMaybe = -1;
    return 1;
}

#include <errno.h>

#include "DSound.h"          // g_pDSoundManager->AcquireChannelForSound
#include "WorldBoardMaybe.h" // g_worldBoard.dwHalfWidth/dwHalfHeight

// FUNCTION: LOCO 0x4472b0
// Interning lookup for the shared sound bank: resolves a WAV resource id in the
// 0x5000..0x6060 band to its SoundBankEntry, loading it on first use via
// SoundBank_PreloadWavRange. Out-of-band ids set errno = EPERM; an id whose WAV cannot be
// loaded latches the (SoundBankEntry *)-1 failure sentinel into its slot so the load is
// only ever attempted once, and reports errno = ENOENT on this and every later call.
// ⚠ In the ORIGINAL, PlayUiSound/PlaySoundAtScreenPos below both inline this body -- which is
// why their copies address m_apSoundBankEntries absolutely off g_UIResources rather than off
// `this`. That cannot be reproduced here: VC5's /O2 implies /Ob1, so cl only inlines a
// function explicitly marked `inline`, and marking it so deletes this out-of-line COMDAT that
// ~30 other TUs genuinely call. See the park note below the definition.
SoundBankEntry *UIResources::SoundBank_LookupEntryById(unsigned int nSoundId)
{
    if ((int)nSoundId < 0x5000 || (int)nSoundId >= 0x6060) {
        errno = EPERM;
        return NULL;
    }
    SoundBankEntry *pEntry = m_apSoundBankEntries[nSoundId - 0x5000];
    if (pEntry == NULL) {
        SoundBank_PreloadWavRange(nSoundId, nSoundId);
        pEntry = m_apSoundBankEntries[nSoundId - 0x5000];
        if (pEntry == NULL) {
            m_apSoundBankEntries[nSoundId - 0x5000] = (SoundBankEntry *)-1;
            errno = ENOENT;
        }
    }
    if (pEntry == (SoundBankEntry *)-1) {
        errno = ENOENT;
        return NULL;
    }
    return pEntry;
}

// ⚠ PARKED, both functions below (0x447930 / 0x4479d0) -- and the park is a TOOLCHAIN
// contradiction, not a source-shape problem, so do NOT re-autopsy them as one.
// In the original both INLINE the SoundBank_LookupEntryById body above (their copies address
// m_apSoundBankEntries absolutely off g_UIResources and load ecx = 0x4855e8 explicitly for the
// SoundBank_PreloadWavRange call -- exactly what inlining a call whose `this` is the constant
// &g_UIResources produces). Under the locked flags the two requirements are mutually
// exclusive, measured both ways this session:
//   * definition left non-inline (what is checked in): 0x4472b0 is EXACT at its full 127 B,
//     but VC5 /O2 implies /Ob1 -- it will NOT auto-inline a function that is not marked
//     inline -- so these two emit a `call` and compile to 58/53 B against 153/149 B.
//   * definition marked `inline`/`__inline`: both callers compile to EXACTLY 153/149 B (right
//     lengths, DIFF(97)/DIFF(28) of ordinary residual), but cl then emits NO out-of-line
//     COMDAT for the helper at all -- and 0x4472b0 has ~30 real callers in other TUs
//     (EnsureSoundPlayingMaybe, PlaySoundById, GoToNextPage, ...), so that copy is genuinely
//     referenced and must exist. Its marker also loses its COMDAT and mis-pairs, silently
//     corrupting every later score in the file.
// The source kept here is the faithful one (a real call to the shared helper, which is what
// the original source said); only the expansion differs. 127 B of EXACT was taken over 302 B
// that would leave the helper unclaimed and the marker pairing broken.

// FUNCTION: LOCO 0x447930
// Fire-and-forget UI sound: plays nSoundId at the world board's own centre point
// (g_worldBoard.dwHalfWidth/dwHalfHeight) in channel category 4, non-looping. One of the
// "this-in-ecx but never read" members documented on src/UIResources.h -- the body reaches
// the singleton through g_UIResources explicitly, never through `this`.
void UIResources::PlayUiSound(unsigned nSoundId)
{
    SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(nSoundId);
    if (g_pDSoundManager != NULL && pEntry != NULL) {
        g_pDSoundManager->AcquireChannelForSound(pEntry, NULL, g_worldBoard.dwHalfWidth,
                                                 g_worldBoard.dwHalfHeight, 4, 0);
    }
}

// FUNCTION: LOCO 0x4479d0
// PlayUiSound's positioned twin: same interning lookup, but the caller supplies the pan/
// attenuation position and the channel category. Unlike PlayUiSound it does NOT re-check
// pEntry for NULL before the call -- that is not a bug: AcquireChannelForSound (0x413210)
// tests its own descriptor argument and bails to the failure return, so PlayUiSound's extra
// guard is merely redundant. Not an engine bug; do not tag it as one.
void UIResources::PlaySoundAtScreenPos(unsigned nSoundId, int x, int y, unsigned nCategory)
{
    SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(nSoundId);
    if (g_pDSoundManager != NULL) {
        g_pDSoundManager->AcquireChannelForSound(pEntry, NULL, x, y, nCategory, 0);
    }
}

// FUNCTION: LOCO 0x447a70
// The path-based cousin of PlaySoundAtScreenPos: instead of interning a resource id through
// the shared sound bank, it builds a THROWAWAY SoundBankEntry around a caller-supplied file
// path, plays it once at (x, y) in channel category nFlags, and deletes it again. Nothing is
// cached, so this is the expensive route -- used only for sounds whose path is data, e.g. the
// per-card identity clip in src/EditCardWnd.cpp and DPlaySessionMgr's peer-join chime.
// ⚠ EnsureLoaded() is invoked WITHOUT a NULL check on the freshly-allocated entry (an
// out-of-memory `new` leaves pEntry NULL and the original still loads ecx with it) -- faithful
// to the original, which only guards the pointer again at the `delete`. Same
// this-in-ecx-but-never-read member class as the two above.
void UIResources::Sound_PlayOneShotAtPosition(char *pszPath, int x, int y, unsigned nFlags)
{
    SoundBankEntry *pEntry = new SoundBankEntry(pszPath);
    if (g_pDSoundManager != NULL && pEntry->EnsureLoaded()) {
        g_pDSoundManager->AcquireChannelForSound(pEntry, NULL, x, y, nFlags, 0);
        pEntry->Release();
    }
    delete pEntry;
}
