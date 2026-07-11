// CursorDesc -- vtable slot 3, the shared ee.ini-style token-field parser used by all 5
// factory siblings' own Load (CursorDesc itself, Obj0x4779e0, BigObj, Obj0x478118,
// CarKindDesc). Reads whitespace-delimited "keyword value..." tokens from a real statically-
// linked istream (either an ifstream disk fallback or an istrstream over an RF-archived
// buffer -- both passed here polymorphically, see Load's own 2 ctor call sites) until the
// "-9" record-end sentinel or eof, then consumes the remainder of that line and skips forward
// past a following block of "/"-prefixed lines to reach the trailing shadow-bitmap path text.
#include "CursorDesc.h"
#include "LocoBitmap.h"
#include "UIResources.h"    // g_UIResources -- the sound-bank + station-clock singleton
#include "DSoundChannel.h"  // SoundBankEntry::EnsureLoaded
#include "EasterEggMgr.h"
#include "WorldBoardMaybe.h" // g_worldBoard -- wCols/wRows, the board edges IsAtMatchingBoardEdgeMaybe tests
#include "DPlaySessionMgr.h" // g_pDPlaySessionMgr -- IsItemAvailableMaybe's connectionMode gate

#ifdef LOCO_PORT
#include "PortMode.h"  // PORT ONLY -- Port_Tracef boot diagnostic
#endif

#include <fstream.h>
#include <strstrea.h>
#include <stdio.h>
#include <string.h>

extern unsigned int g_dwGameTick; // DAT_004a99b4

extern unsigned int __cdecl TileKind_GetCategory(unsigned int kindId); // 0x446030, see src/UIResources.cpp

// TU-local methods-only view for Load (0x424bf0, vtable slot 4): the shared header
// deliberately keeps Load mis-declared no-arg, because widening the declaration there rotates
// src/Obj0x4779e0.cpp's /Og TU state and costs a measured -489 B (see CursorDesc.h's own note).
// Same rule and same shape as this file's UIResourcesDescView0x425670 below.
struct CursorDescLoadView0x424bf0 : CursorDesc {
    void Load(unsigned int kindId, char *pszDefinition);
};

// TU-local methods-only view, same rule and same shape as src/Main.cpp's
// UIResourcesWndProcView0x4618c0: TickStationClockChimeMaybe (0x447400) must not be declared on
// the shared UIResources.h, because ANY new method declaration there rotates
// DPlaySessionMgr.cpp's /Og TU state and breaks SelectGridCellFromPointMaybe's EXACT (bisected
// v340). GetOrLoadFrameBitmap below is the third TU that needs it.
// RETIRED v577 -- see src/UIResources.cpp: TickStationClockChimeMaybe is declared on the
// shared UIResources now (the v340 price was re-measured this session and is stale).

// TU-local methods-only view for BigObj::IsAtMatchingBoardEdgeMaybe (0x44bdb0), same rule and
// same shape as the two views above: declaring this method on the shared BigObj in
// CursorDesc.h is measured at -2096 B (see that declaration's note for the victim list).
struct BigObjBoardEdgeView0x44bdb0 : BigObj {
    unsigned char IsAtMatchingBoardEdgeMaybe(short col, short row);
};

// FUNCTION: LOCO 0x424af0
// The base ctor of every descriptor family: zeroes the eight fields that must not be garbage
// before a parse, installs the vtable, and hands straight off to Load. The third parameter is
// dead (see the declaration in CursorDesc.h); the field write ORDER below is the original's own.
CursorDesc::CursorDesc(int kindId, char *pszDefinition, int bOddKindMaybe)
{
    (void)bOddKindMaybe;
    pOwnedObjA = NULL;         // +0x10
    pShadowBitmap = NULL;      // +0x24
    paFrameEntries = NULL;     // +0x20
    bReadyFlagMaybe = 0;       // +0x18
    nLiveInstanceCountMaybe = 0; // +0x158
    dwRenderFlags = 0;         // +0x164
    hotspotX = 0;              // +0x32
    hotspotY = 0;              // +0x34
    Load(kindId, pszDefinition);
}

// FUNCTION: LOCO 0x424bf0
// CursorDesc::Load -- vtable slot 4 (+0x10), the per-kind .dat loader of the BASE tier (its
// Obj0x4779e0/Obj0x478118/CarKindDesc siblings override it and chain CursorDesc::ParseTokenField
// after their own; here the one virtual ParseTokenField call IS the whole parse). Initializes
// the descriptor's parse-owned fields, then builds "<prefix><name>.dat" (a local, kept only for
// the loose-file fallback) and "<prefix><name>.bmp" (into the persistent szBmpPath at +0x48),
// tries the RF archive first under the install-relative "<name>.dat" key, and falls back to a
// loose ifstream when the archive parse never set bLoadOkFlag. Written through the TU-local view
// above (see CursorDesc.h for why the real 2-arg signature cannot live on the shared header).
void CursorDesc::Load(unsigned int kindId, char *pszDefinition) {
    int nSize;
    void *pRfBuf;
    istrstream *pRfStream;
    ifstream fileStream;
    char szDatPath[264];
    char szRfName[264];

    resourceId = kindId;
    categoryByte = (unsigned char)TileKind_GetCategory(kindId);
    nShadowId = 0;
    nShadowOffsetX = 0;
    nShadowOffsetY = 0;
    nativeWidth = 0;
    nativeHeight = 0;
    nTotalFrameCount = 1;
    nFrameSetCount = 0;
    wDefaultFrameSetIndex = 0;
    wActiveFrameSetIndex = 0;
    wShadowFrameWidth = 0;
    wShadowBitmapHeight = 0;
    nButtonFrameCount = 0;
    nMustHaveKindId = -1;
    nCantHaveKindId = -1;
    bReadyFlagMaybe = 0;
    bButtonVisible = 1;
    bLoadOkFlag = 0;
    szCategoryName[0] = 0;
    nMaxInstances = -1;
    if (pszDefinition == NULL) {
        return;
    }
    sprintf(szDatPath, "%s%s.dat", g_pInstallPathPrefix, pszDefinition);
    sprintf(szBmpPath, "%s%s.bmp", g_pInstallPathPrefix, pszDefinition);
    if (g_RFIndex.IsOpen()) {
        sprintf(szRfName, "%s.dat", pszDefinition);
        pRfBuf = g_RFIndex.LoadResource((const unsigned char *)szRfName, &nSize);
        if (pRfBuf != NULL) {
            pRfStream = new istrstream((char *)pRfBuf, nSize);
            if (pRfStream != NULL) {
                bLoadOkFlag = ParseTokenField(pRfStream);
                delete pRfStream;
            }
            _free(pRfBuf);
        }
    }
    if (bLoadOkFlag == 0) {
        fileStream.open(szDatPath, ios::nocreate);
        if (fileStream.is_open()) {
            bLoadOkFlag = ParseTokenField(&fileStream);
            fileStream.close();
        }
    }
#ifdef LOCO_PORT
    // PORT ONLY -- temporary boot diagnostic, byte-neutral for the match build.
    Port_Tracef("desc %04x cat=%u rf=%d buf=%d ok=%d dat=%s\n", kindId,
                (unsigned)categoryByte, (int)g_RFIndex.IsOpen(), (int)(pRfBuf != NULL),
                (int)bLoadOkFlag, szDatPath);
#endif
}

// FUNCTION: LOCO 0x424e00
unsigned char CursorDesc::ParseTokenField(istream *pStream) {
    unsigned char bResult = 1;
    char szTok[264];

    *pStream >> szTok;
    while (_stricmp(szTok, "-9") != 0 && !pStream->eof()) {
        if (_stricmp(szTok, "button") == 0) {
            // ⚠ This branch has FOUR reads, and the first one is a DISCARD: the real line is
            // `button offset 158 92 2`, so the sub-keyword "offset" is read into szTok and
            // thrown away before the 3 fields. Ground-truthed from the raw disasm's own call
            // sequence at 0x424e80: operator>>(char*) once (0x4649f0), then 0x464bc0 twice
            // and 0x464750 once for the three numerics.
            //
            // ⚠ It then FALLS THROUGH to the shared bottom-of-loop read like every other
            // branch -- 0x424eb5 is `jmp 0x42536a`, and 0x42536a IS that shared read (call
            // 0x4649f0) followed by the `-9` loop test. It is NOT a `continue`, which would
            // target the test at 0x425380 instead. Writing it as `continue` (what this
            // carried until v559) left szTok holding the discarded "offset", so the very next
            // loop iteration matched no keyword, hit the `else break`, and failed the trailing
            // `_stricmp(szTok, "-9")` -- returning 0 from EVERY descriptor whose .dat opens
            // with a `button` line. See docs/engine-bugs.md's neighbours: this one was ours,
            // not the game's.
            *pStream >> szTok;
            *pStream >> field_0x2eMaybe >> field_0x30Maybe >> nButtonFrameCount;
            if (nButtonFrameCount == 0) {
                nButtonFrameCount = 3;
            }
        } else if (_stricmp(szTok, "Name") == 0) {
            pStream->getline(szTok, 0x104, '\n');
            // &szTok[1], not szTok: getline picks the line up from just after the "Name"
            // keyword, so szTok[0] is the separating space. Ground-truthed at 0x424f22 --
            // `lea eax,[esp+0x19]` against szTok's own `lea …,[esp+0x18]` elsewhere.
            strncpy(szCategoryName, &szTok[1], 10);
            byNulTerminatorGuard = 0;
            // trim up to 2 trailing CR/LF bytes off the fixed 10-byte name (handles a
            // trailing "\r", "\n", or "\r\n" pair) -- literal repeated strlen() calls per the
            // raw disasm, not a cached length local (Yoda lesson #19's no-caching family)
            if (szCategoryName[strlen(szCategoryName) - 1] == '\r' ||
                szCategoryName[strlen(szCategoryName) - 1] == '\n') {
                szCategoryName[strlen(szCategoryName) - 1] = 0;
            }
            if (szCategoryName[strlen(szCategoryName) - 1] == '\r' ||
                szCategoryName[strlen(szCategoryName) - 1] == '\n') {
                szCategoryName[strlen(szCategoryName) - 1] = 0;
            }
        } else if (_stricmp(szTok, "hotspot") == 0) {
            *pStream >> hotspotX;
            *pStream >> hotspotY;
        } else if (_stricmp(szTok, "ShadowId") == 0) {
            *pStream >> nShadowId;
        } else if (_stricmp(szTok, "ShadowOffset") == 0) {
            *pStream >> nShadowOffsetX;
            *pStream >> nShadowOffsetY;
        } else if (_stricmp(szTok, "animation") == 0) {
            // recognized token, no fields of its own
        } else if (_stricmp(szTok, "semi-transparent") == 0) {
            dwRenderFlags = dwRenderFlags | 0x400;
        } else if (_stricmp(szTok, "shadows") == 0) {
            dwRenderFlags = dwRenderFlags | 2;
        } else if (_stricmp(szTok, "must/cant_have") == 0) {
            *pStream >> nMustHaveKindId >> nCantHaveKindId;
        } else if (_stricmp(szTok, "MaxInstances") == 0) {
            *pStream >> nMaxInstances;
        } else if (_stricmp(szTok, "total_number_of_frames") == 0) {
            *pStream >> nTotalFrameCount;
            if (nTotalFrameCount == 0) {
                nTotalFrameCount = 1;
            }
        } else if (_stricmp(szTok, "number_of_frame_sets") == 0) {
            *pStream >> nFrameSetCount;
            if (nFrameSetCount > 0) {
                paFrameEntries = (CursorAnimFrameEntry *)::operator new(nFrameSetCount * sizeof(CursorAnimFrameEntry));
                if (paFrameEntries == NULL) {
                    return 0;
                }
            }
        } else if (_stricmp(szTok, "cursor_frame_set") == 0 || _stricmp(szTok, "cursor/default_frame_set") == 0) {
            *pStream >> (short &)wDefaultFrameSetIndex >> (short &)wActiveFrameSetIndex;
            if ((short)wDefaultFrameSetIndex != -1 && (short)wDefaultFrameSetIndex >= (int)nFrameSetCount) {
                bResult = 0;
            }
            if ((short)wActiveFrameSetIndex != -1 && (short)wActiveFrameSetIndex >= (int)nFrameSetCount) {
                bResult = 0;
            }
            for (int i = 0; i < (int)nFrameSetCount; i++) {
                unsigned short tmp;

                memset(&paFrameEntries[i], 0, sizeof(CursorAnimFrameEntry));
                *pStream >> szTok;
                *pStream >> paFrameEntries[i].nStartFrame;
                *pStream >> paFrameEntries[i].nEndFrame;
                *pStream >> paFrameEntries[i].wFrameDivisor;
                *pStream >> tmp;
                paFrameEntries[i].bDoubleSpeedFlag = (unsigned char)tmp;
                *pStream >> paFrameEntries[i].nCooldownTicks;
                *pStream >> paFrameEntries[i].nBounceSoundId;
                *pStream >> paFrameEntries[i].nSoundBankEntryId;
                *pStream >> paFrameEntries[i].nSoundRetriggerDelay;
                *pStream >> paFrameEntries[i].nSoundCategory;
                *pStream >> tmp;
                paFrameEntries[i].Unk0x16Maybe = (unsigned char)tmp;

                if (paFrameEntries[i].wFrameDivisor <= 0) {
                    paFrameEntries[i].wFrameDivisor = 1;
                }
                if (paFrameEntries[i].nStartFrame == paFrameEntries[i].nEndFrame &&
                    paFrameEntries[i].nBounceSoundId == i) {
                    paFrameEntries[i].nBounceSoundId = -1;
                }
                if (paFrameEntries[i].nStartFrame >= nTotalFrameCount ||
                    paFrameEntries[i].nEndFrame >= nTotalFrameCount) {
                    bResult = 0;
                }
                if (paFrameEntries[i].nBounceSoundId >= (int)nFrameSetCount) {
                    bResult = 0;
                }
            }
        } else {
            break;
        }
        *pStream >> szTok;
    }

    if (_stricmp(szTok, "-9") != 0) {
        bResult = 0;
    }
    pStream->getline(szTok, 0x104, '\n');

    // Skip forward to a "/"-prefixed line, then past the whole following block of them --
    // real purpose (a comment/section-header block ahead of the shadow-bitmap path text)
    // not yet confirmed.
    while (szTok[0] != '/' && !pStream->eof()) {
        pStream->getline(szTok, 0x104, '\n');
    }
    while (szTok[0] == '/' && !pStream->eof()) {
        pStream->getline(szTok, 0x104, '\n');
    }

    if (strlen(szBmpPath) > 2) {
        // 263: back-derived from the original's own stack-frame size (sub esp,0x218) once
        // paFrameEntries's per-field loop below stopped needing its own cached-pointer local
        // -- not confirmed against szBmpPath's real size.
        char szFullPath[263];
        // The shadow/button artwork lives beside the main sprite under the ".but" extension
        // (173 such members in loco/rfh.txt, e.g. "roads\half-hwint.but"). szBmpPath was built
        // as "<prefix><name>.bmp", so the last TWO characters are overwritten -- "…bmp" becomes
        // "…but" -- NOT appended to. ⚠ This was a strcat("ut") until v576, which produced
        // "…bmput", a name that exists in no archive; every descriptor's Load then failed and
        // pShadowBitmap stayed NULL, which is CursorDesc_IsItemAvailableMaybe's first guard --
        // so no build-toolbox icon was ever available. Raw disasm at 0x425503: strlen into edx,
        // `lea eax,[szFullPath]; sub eax,2; add edx,eax`, then the copy from 0x47e7a0 ("ut").
        strcpy(szFullPath, szBmpPath);
        strcpy(&szFullPath[strlen(szFullPath) - 2], "ut");

        pShadowBitmap = new LocoBitmap();
        if (pShadowBitmap != NULL) {
            pShadowBitmap->Load(szFullPath, 0, 0, 0);
            if (pShadowBitmap->pPixels == NULL && pShadowBitmap->pSurface == NULL) {
                delete pShadowBitmap;
                pShadowBitmap = NULL;
            }
        }
        if (pShadowBitmap != NULL && nButtonFrameCount != 0) {
            wShadowFrameWidth = (unsigned short)((unsigned int)pShadowBitmap->width / nButtonFrameCount);
            wShadowBitmapHeight = (unsigned short)pShadowBitmap->height;
        }
    }

    return bResult;
}

// FUNCTION: LOCO 0x4255f0
// "May this palette/menu item be offered to the player right now?" -- the shared gate both the
// build-tool picker (src/WidgetPicker.cpp) and the world action cursor (src/WorldActionCursor.cpp)
// run over every candidate descriptor before drawing it as available. Four independent reasons to
// refuse, in order:
//   - the kind was never realized, so there is nothing to draw (bButtonVisible / pShadowBitmap /
//     nButtonFrameCount are all set by GetOrLoadFrameBitmap's own success path below);
//   - the kind declares a "must_have" prerequisite kind that is not currently alive anywhere in
//     the world. The -1 "none" sentinel skips the test, but ⚠ NOT the lookup -- the original
//     calls TileKind_GetOrLoadDescriptor unconditionally and only then checks for the sentinel,
//     so a -1 must-have still pays for one (failing) descriptor resolve;
//   - the kind declares a "cant_have" exclusion kind that IS alive;
//   - and finally one hard-coded special case: kind id 0xc42 is withheld outright in
//     connectionMode 2 (see src/DPlaySessionMgr.h), i.e. it is a single-player-only item.
//
// Defined as a free `__fastcall` taking the descriptor, NOT as a member and NOT through a
// TU-local view like Load and IsAtMatchingBoardEdgeMaybe above. Two reasons, and the second is
// the one that decided it:
//   - it must not be declared on CursorDesc.h, whose declaration count is the measured -2096 B
//     dial those two views exist to dodge;
//   - src/WidgetBase.cpp, src/WidgetPicker.cpp and src/WorldActionCursor.cpp ALREADY reach this
//     address through exactly this free-function spelling. A view here would have been a FOURTH
//     name for one function, and landing the definition under it would turn three
//     previously-harmless declarations into three live tools/lint_alias.py findings -- calls
//     compiled against a symbol nothing defines, which relocation masking makes invisible to
//     verify.py. Matching the spelling the callers already use costs nothing (measured
//     byte-neutral against the view form, both 121 B EXACT) and keeps the alias count at zero
//     for this address.
unsigned char __fastcall CursorDesc_IsItemAvailableMaybe(CursorDesc *pDesc) // TODO: sync
{
    CursorDesc *pKindDesc;

    if (pDesc->bButtonVisible == 0 || pDesc->pShadowBitmap == NULL ||
        pDesc->nButtonFrameCount == 0) {
        return 0;
    }
    pKindDesc = g_UIResources.TileKind_GetOrLoadDescriptor(pDesc->nMustHaveKindId);
    if (pDesc->nMustHaveKindId != -1 &&
        (pKindDesc == NULL || pKindDesc->nLiveInstanceCountMaybe == 0)) {
        return 0;
    }
    pKindDesc = g_UIResources.TileKind_GetOrLoadDescriptor(pDesc->nCantHaveKindId);
    if (pKindDesc != NULL && pKindDesc->nLiveInstanceCountMaybe > 0) {
        return 0;
    }
    if (pDesc->resourceId == 0xc42 && g_pDPlaySessionMgr->connectionMode == 2) {
        return 0;
    }
    return 1;
}

// FUNCTION: LOCO 0x425670
// Vtable slot 2 -- the descriptor's lazy bitmap realizer, and the one place a CursorDesc goes
// from "parsed" to "loaded". Constructs pOwnedObjA on first call and loads szBmpPath into it at
// the caller's requested (width, height), then derives the per-FRAME dimensions: the .bmp is one
// horizontal strip of nTotalFrameCount frames, so nativeWidth is the strip width DIVIDED by that
// count while nativeHeight is the strip height as-is. That division is the second site (after
// WidgetPickerObj0x477cc8::ReloadActiveSaveState) that pins LocoBitmap::width as UNSIGNED -- the
// original emits `xor edx,edx; div edi`, where a signed width would give `cdq; idiv`.
//
// A load that produces neither a raw pixel buffer nor a DDraw surface is treated as a failure:
// the bitmap is deleted, pOwnedObjA is put back to null, and the next call retries from scratch.
//
// Two side effects ride along on a successful load, both of them once-per-kind bookkeeping
// rather than anything to do with bitmaps:
//   - the first realization of a kind records it as a discovered easter egg (bButtonVisible is
//     that "already recorded" latch, set by the recorder itself once the ini write succeeds);
//   - every frame set's own sound-bank entry is forced resident, so the animation never has to
//     wait on a .wav load mid-play.
LocoBitmap *CursorDesc::GetOrLoadFrameBitmap(int nWidth, int nHeight)
{
    if (nTotalFrameCount == 0) {
        return 0;
    }
    if (pOwnedObjA == 0) {
        pOwnedObjA = new LocoBitmap;
        if (pOwnedObjA == 0) {
            return 0;
        }
        pOwnedObjA->Load(szBmpPath, 0, nWidth, nHeight);
    }
    if (pOwnedObjA->pPixels == 0 && pOwnedObjA->pSurface == 0) {
        delete pOwnedObjA;
        pOwnedObjA = 0;
        return 0;
    }
    nativeWidth = (unsigned short)(pOwnedObjA->width / nTotalFrameCount);
    nativeHeight = (unsigned short)pOwnedObjA->height;
    nLiveInstanceCountMaybe++;
    if (bButtonVisible == 0) {
        g_easterEggMgrMaybe.RecordEasterEggUnlockMaybe(resourceId);
    }
    unsigned int i;
    for (i = 0; i < nFrameSetCount; i++) {
        SoundBankEntry *pSound =
            g_UIResources.SoundBank_LookupEntryById(paFrameEntries[i].nSoundBankEntryId);
        if (pSound != 0) {
            pSound->EnsureLoaded();
        }
    }
    // The station clock is the one kind whose realization also has to catch its chime up to the
    // current game time -- everything else just gets its bitmap.
    if (resourceId == 0x842) {
        g_UIResources.TickStationClockChimeMaybe(g_dwGameTick, 0);
    }
    return pOwnedObjA;
}

// FUNCTION: LOCO 0x4257f0
// Vtable slot 2, and the exact mirror image of GetOrLoadFrameBitmap (0x425670) directly above:
// where that one takes the reference (nLiveInstanceCountMaybe++, then EnsureLoaded on every
// frame set's sound), this one gives it back (--, then Release over the same walk). It is what
// WindowBase::~WindowBase calls on its three owned CursorDesc pointers instead of a plain
// delete -- the descriptor is interned in UIResources' m_apKindDescriptors and outlives any one
// window, so only the BITMAP and the sound-bank references are dropped here, never the object.
//
// The decrement is guarded (`if (count != 0)`) rather than unconditional, so the refcount
// saturates at 0 instead of wrapping -- an unsigned short, so an unguarded -- would wrap to
// 0xffff and strand the bitmap forever. Both the guard and the release condition test the same
// field, which is why the emitted code re-reads +0x158 rather than reusing the decremented
// value in the register.
//
// bReadyFlagMaybe == 1 VETOES the release: a descriptor still being realized keeps its bitmap
// even at refcount 0.
void CursorDesc::ReleaseRef() {
    if (nLiveInstanceCountMaybe > 0) {
        nLiveInstanceCountMaybe--;
    }
    if (nLiveInstanceCountMaybe == 0 && pOwnedObjA != 0 && bReadyFlagMaybe != 1) {
        delete pOwnedObjA;
        pOwnedObjA = 0;
        unsigned int i;
        for (i = 0; i < nFrameSetCount; i++) {
            SoundBankEntry *pSound =
                g_UIResources.SoundBank_LookupEntryById(paFrameEntries[i].nSoundBankEntryId);
            if (pSound != 0) {
                pSound->Release();
            }
        }
    }
}

// BigObj's coarse per-kind classifier predicates (m_type0x63a) -- MOVED IN 2026-07-22 (v322)
// from src/phase2_probe.cpp's frozen probe-local BigObj copy (now retired; this class is the
// canonical site). `unsigned char` return + `if (cond) return 1; return 0;` shape (NOT
// `bool`/`return cond;`) -- the bool form widens the compare to full EAX and breaks the
// byte-match (see the recurring "bool-return register width" class note in docs/PARKED.md).

// FUNCTION: LOCO 0x44b4f0
// BigObj::ParseTokenField -- vtable slot +0xc override (vtable 0x478358, Ghidra:
// BigObj_ParseSocketTableAndTypeKeywordMaybe). Reads the socket-point stream from the kind's
// .dat: a discarded lead token, then count1 + count2 (longs); count1 {x,y} short pairs into a
// freshly-allocated pSocketTable, one discarded separator short, then count2 more pairs
// (batch 2, the points/switch tile's third rail connection). wSocketCount = count1-1 and
// wSocketCountExt = (count2==0 ? 0 : count1+count2-1) are the raw indices of the LAST point of
// batch 1 / of the combined sequence (see docs/subsystems.md's BigObj entry). A trailing short
// sentinel of -9 opens the type-keyword loop: strcmp-matched keyword (+optional direction word)
// assigns m_type0x63a, looping until the stream's eofbit sets or an empty token read.
unsigned char BigObj::ParseTokenField(istream *pStream) {
    char szLeadToken[264];
    char szKeyword[20];
    int nSentinel = 0;
    char szDirection[20];
    int nCount1 = 0;
    int nCount2 = 0;
    int i;

    if (pSocketTable != NULL) {
        delete pSocketTable;
        pSocketTable = NULL;
    }
    *pStream >> szLeadToken >> nCount1 >> nCount2;
    wSocketCount = (unsigned short)(nCount1 - 1);
    if (nCount2 != 0) {
        wSocketCountExt = (unsigned short)(nCount2 + nCount1 - 1);
    } else {
        wSocketCountExt = 0;
    }
    if (nCount1 > 0 || nCount2 > 0) {
        pSocketTable = new short[(nCount1 + nCount2) * 2];
    }
    for (i = 0; i < nCount1; i++) {
        *pStream >> pSocketTable[i * 2] >> pSocketTable[i * 2 + 1];
    }
    *pStream >> (short &)nSentinel;
    if (nCount2 != 0) {
        for (i = nCount1; i < nCount1 + nCount2; i++) {
            *pStream >> pSocketTable[i * 2] >> pSocketTable[i * 2 + 1];
        }
    }
    *pStream >> (short &)nSentinel;
    if ((short)nSentinel != -9) {
        return 0;
    }

    szKeyword[0] = 0;
    while (!pStream->eof()) {
        *pStream >> szKeyword;
        if (szKeyword[0] == 0) {
            break;
        }
        if (strcmp(szKeyword, "tunnel") == 0) {
            *pStream >> szDirection;
            if (strcmp(szDirection, "left") == 0) {
                m_type0x63a = 1;
            } else if (strcmp(szDirection, "top") == 0) {
                m_type0x63a = 3;
            } else if (strcmp(szDirection, "right") == 0) {
                m_type0x63a = 2;
            } else if (strcmp(szDirection, "bottom") == 0) {
                m_type0x63a = 4;
            }
        } else if (strcmp(szKeyword, "depot") == 0) {
            *pStream >> szDirection;
            if (strcmp(szDirection, "left") == 0) {
                m_type0x63a = 7;
            } else if (strcmp(szDirection, "top") == 0) {
                m_type0x63a = 9;
            } else if (strcmp(szDirection, "right") == 0) {
                m_type0x63a = 8;
            } else if (strcmp(szDirection, "bottom") == 0) {
                m_type0x63a = 10;
            }
        } else if (strcmp(szKeyword, "bridge") == 0) {
            *pStream >> szDirection;
            if (strcmp(szDirection, "horizontal") == 0) {
                m_type0x63a = 5;
            } else if (strcmp(szDirection, "vertical") == 0) {
                m_type0x63a = 6;
            }
        } else if (strcmp(szKeyword, "points") == 0) {
            m_type0x63a = 0xb;
        } else if (strcmp(szKeyword, "switch") == 0) {
            m_type0x63a = 0xc;
        } else if (strcmp(szKeyword, "crosstrack") == 0) {
            m_type0x63a = 0xd;
        } else if (strcmp(szKeyword, "levelcrossing") == 0) {
            *pStream >> szDirection;
            if (strcmp(szDirection, "path-x-h") == 0) {
                m_type0x63a = 0xe;
            } else if (strcmp(szDirection, "path-x-v") == 0) {
                m_type0x63a = 0xf;
            } else if (strcmp(szDirection, "road-x-h") == 0) {
                m_type0x63a = 0x10;
            } else if (strcmp(szDirection, "road-x-v") == 0) {
                m_type0x63a = 0x11;
            }
        } else if (strcmp(szKeyword, "station") == 0) {
            *pStream >> szDirection;
            if (strcmp(szDirection, "station-h") == 0) {
                m_type0x63a = 0x12;
            } else if (strcmp(szDirection, "station-v") == 0) {
                m_type0x63a = 0x13;
            }
        }
    }
    return 1;
}

// FUNCTION: LOCO 0x44bd10
unsigned char BigObj::IsType0x63aInSet1234() {
    unsigned char t = m_type0x63a;
    if (t == 1 || t == 3 || t == 2 || t == 4) return 1;
    return 0;
}

// FUNCTION: LOCO 0x44bd30
unsigned char BigObj::IsType0x63aInSet() {
    unsigned char t = m_type0x63a;
    if (t == 7 || t == 9 || t == 8 || t == 0xa) return 1;
    return 0;
}

// FUNCTION: LOCO 0x44bd50
unsigned char BigObj::IsType0x63aInSetE() {
    unsigned char t = m_type0x63a;
    if (t == 0xe || t == 0xf) return 1;
    return 0;
}

// FUNCTION: LOCO 0x44bd70
unsigned char BigObj::IsType0x63aInSet10() {
    unsigned char t = m_type0x63a;
    if (t == 0x10 || t == 0x11) return 1;
    return 0;
}

// FUNCTION: LOCO 0x44bd90
unsigned char BigObj::IsType0x63aInSet12() {
    unsigned char t = m_type0x63a;
    if (t == 0x12 || t == 0x13) return 1;
    return 0;
}

// FUNCTION: LOCO 0x44bdb0
// The four "board edge" kinds, one arm each -- see the declaration's note. The north arm's
// bound is the footprint's own row extent minus its Y step count (a byte subtraction, kept
// truncated to a byte before the widening compare, exactly as the original does it); the east
// and south arms add the kind's occupancy extent to the incoming column/row and test against
// the board's own dimensions.
//
// PARTIAL, compiled 137 B vs the original's 151, insns 50/49 -- CONTENT-COMPLETE and
// structurally paired arm for arm (every asmscore dump row across all four arms is an `r`
// register-rename, in the same order). The whole residual is a SPILL decision: the original
// allocates two stack bytes (`sub esp,8`) and keeps BOTH locals in memory -- nNorthEdgeRow at
// [esp+0x14], reloaded as a dword and masked (`mov ebp,[esp+0x14]; and ebp,0xff`), and bResult
// at [esp+0x13], live in `al` across the arms and reloaded after the east arm clobbers eax --
// burning a fifth callee-saved register (ebp, as a general register under /Oy) to do it. This
// compile enregisters nNorthEdgeRow in dl/bl and keeps bResult purely in its stack slot,
// loading it once at the tail: one more instruction, 14 fewer bytes.
// Measured and REJECTED (do not re-run): explicit int-width casts on the north arm's compare
// (`(int)row == (int)nNorthEdgeRow`, no-ops after promotion); `char` vs `unsigned char` return
// and accumulator type; declaring bResult before nNorthEdgeRow. All three were byte-identical
// at 137 B / DIFF(127). The `||`-chain funnel itself IS load-bearing and must not be undone --
// four separate `if (...) return 1;` statements emit four separate epilogues instead of the
// original's one shared `mov al,1` tail (151 B but DIFF(137), insns 61/49).
unsigned char BigObjBoardEdgeView0x44bdb0::IsAtMatchingBoardEdgeMaybe(short col, short row) // TODO: sync (TU-local view)
{
    unsigned char nNorthEdgeRow = bBitmapOccupancyRows - bFootprintYSteps;
    unsigned char bResult = 0;

    if ((m_type0x63a == 1 && col == 0) ||
        (m_type0x63a == 3 && row == nNorthEdgeRow) ||
        (m_type0x63a == 2 && bBitmapOccupancyCols + col == g_worldBoard.wCols) ||
        (m_type0x63a == 4 && row + bBitmapOccupancyRows == g_worldBoard.wRows)) {
        bResult = 1;
    }
    return bResult;
}
