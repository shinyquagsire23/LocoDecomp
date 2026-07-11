// Obj0x478118 -- the MINIFIG/PERSON kind descriptor (vtable 0x478118), a plain CursorDesc
// subclass with a 0x10-byte tail. This TU is the class's whole non-ctor surface: the .dat
// loader (slot 4), its own ini-keyword parser (slot 3 override) and the destructor pair.
//
// The ctor (0x436400) is here, and as of 2026-07-31 it is a REAL `Obj0x478118::Obj0x478118`
// declared on the class in src/Obj0x478118.h. It spent one session on a TU-local ctor view
// instead, to avoid declaring `CursorDesc(int, char *, int)` on the shared src/CursorDesc.h
// (which rotates src/Obj0x4779e0.cpp's /Og state for a measured -489 B). That dodge was a live
// runtime defect, not a saving: the factory's `new`s live in src/UIResources.cpp, so against a
// TU-local view they bound to a generated do-nothing stub and this tier's descriptors came back
// unconstructed. CursorDesc's ctor is now declared on the real class and its price already
// paid, so naming it from here is free. See CODEGEN #161.
#include "Obj0x478118.h"

#include "DSoundChannel.h"  // RFIndex/g_RFIndex/g_pInstallPathPrefix/_free, SoundBankEntry::EnsureLoaded
#include "UIResources.h"    // g_UIResources -- GetOrLoadFrameBitmap's sound-bank lookup

#include <fstream.h>
#include <strstrea.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// FUNCTION: LOCO 0x436400
// Obj0x478118::Obj0x478118 -- runs CursorDesc's 3-arg ctor in place, stamps vtable 0x478118,
// and hands straight off to this tier's own loader. No field initialisation of its own: every
// default in the 0x10-byte tail is seeded by LoadMaybe's own opening run, not here.
Obj0x478118::Obj0x478118(unsigned int kindId, char *pszDefinition)
    : CursorDesc(kindId, NULL, 1) {
    LoadMaybe(kindId, pszDefinition);
}

// FUNCTION: LOCO 0x436480
// Obj0x478118::~Obj0x478118 -- stamps this tier's vtable back over the object (the standard
// MSVC "destructor re-seats its own vtable" prologue) and runs ~CursorDesc. Kept OUT OF LINE
// deliberately: the original has ??1 (here) and ??_G (below) as two separate COMDATs, which is
// the tell that the body was not written inside the class declaration.
Obj0x478118::~Obj0x478118() {
}

// FUNCTION: LOCO 0x436460 (??_GObj0x478118 scalar deleting dtor)

// FUNCTION: LOCO 0x436490
// Obj0x478118::LoadMaybe -- really vtable slot 4 (+0x10), the per-kind .dat loader (see the
// header's note on why it is declared non-virtual). Seeds every field of this tier's own
// 0x10-byte tail with its default, then -- only when a definition name was supplied -- builds
// "<prefix><name>.dat"/"<prefix><name>.bmp" and tries the RF archive first (an istrstream over
// the loaded resource), feeding whichever stream it gets through this tier's virtual
// ParseTokenField followed by CursorDesc's own. Unlike Obj0x4779e0's Load (0x41e6e0) the loose
// -file fallback here is UNCONDITIONAL -- it is not gated on the archive parse having failed --
// and it re-runs both parsers over the disk copy whenever the file opens. Finally clamps the
// first 4 frame sets' cooldown ticks to <= 0, normalises the first 8 frame sets' self-index
// sentinel, and seeds an unset cursor hotspot to (0, 8).
void Obj0x478118::LoadMaybe(unsigned int kindId, char *pszDefinition) {
    int nSize;
    void *pRfBuf;
    istrstream *pRfStream;
    ifstream fileStream;
    char szDatPath[264];
    char szRfName[264];

    bWalkSpeedMaybe = 0;
    bWalkStepCountMaybe = 0;
    nPickUpSoundId = 0;
    dwSex = 'M';
    bGroundWidth = 8;
    bSpawnLimit = 0xff;
    bEmployable = 0;
    bLoadOkFlag = 0;
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
                bLoadOkFlag = (bLoadOkFlag != 0 && CursorDesc::ParseTokenField(pRfStream) != 0) ? 1 : 0;
                delete pRfStream;
            }
            _free(pRfBuf);
        }
    }
    fileStream.open(szDatPath, ios::nocreate);
    if (fileStream.is_open()) {
        bLoadOkFlag = ParseTokenField(&fileStream);
        bLoadOkFlag = (bLoadOkFlag != 0 && CursorDesc::ParseTokenField(&fileStream) != 0) ? 1 : 0;
        fileStream.close();
    }

    for (short i = 0; i < nFrameSetCount; i++) {
        if (i >= 4) {
            break;
        }
        if (paFrameEntries[i].nCooldownTicks > 0) {
            paFrameEntries[i].nCooldownTicks = 0;
            bLoadOkFlag = 0;
        }
    }
    for (short j = 0; j < nFrameSetCount; j++) {
        if (j >= 8) {
            break;
        }
        if (paFrameEntries[j].nBounceSoundId != j && paFrameEntries[j].nBounceSoundId != -1) {
            paFrameEntries[j].nBounceSoundId = j;
        }
    }
    if (IsHotspotUnsetMaybe()) {
        hotspotX = 0;
        hotspotY = 8;
    }
}

// FUNCTION: LOCO 0x436960
// Vtable slot 1 (+0x4) -- this class's override of CursorDesc::GetOrLoadFrameBitmap. The whole
// addition is the two lines above the chained call: look this kind's PickUpSoundId up in the
// shared sound bank and force it resident, so the pick-up sound is already loaded the first time
// the player grabs a minifig. Exactly the idiom the base body runs over its per-frame-set sound
// ids (src/CursorDesc.cpp), applied to the one sound id only this tier has.
//
// The null guard is on the LOOKUP result, not on nPickUpSoundId: a 0 id simply finds no entry, so
// the "no sound" default falls out of the same test rather than needing its own. The base's result
// is returned unexamined.
LocoBitmap *Obj0x478118::GetOrLoadFrameBitmap(int nWidth, int nHeight) {
    SoundBankEntry *pSound = g_UIResources.SoundBank_LookupEntryById(nPickUpSoundId);
    if (pSound != 0) {
        pSound->EnsureLoaded();
    }
    return CursorDesc::GetOrLoadFrameBitmap(nWidth, nHeight);
}

// FUNCTION: LOCO 0x4369a0
// Vtable slot 2 (+0x8) -- this class's override of CursorDesc::ReleaseRef, and the release half
// of the pair GetOrLoadFrameBitmap above opens: drop the reference this tier took on its
// PickUpSoundId, then chain to the base so the frame-set sounds and the bitmap go the same way.
// Same shape as the acquire side, same null-guard-on-the-lookup reasoning, and the base call is
// likewise made unconditionally.
void Obj0x478118::ReleaseRef() {
    SoundBankEntry *pSound = g_UIResources.SoundBank_LookupEntryById(nPickUpSoundId);
    if (pSound != 0) {
        pSound->Release();
    }
    CursorDesc::ReleaseRef();
}

// FUNCTION: LOCO 0x436750
// Obj0x478118::ParseTokenField -- vtable slot 3 (+0xc) override, this tier's own ini-keyword
// handler. Reads "keyword value..." tokens until the "-9" record-end sentinel or eof; every
// keyword it knows writes one field of the 0x10-byte tail. An UNRECOGNISED keyword is simply
// skipped (unlike CursorDesc::ParseTokenField, which breaks out of the loop on one) -- this
// tier runs first and leaves the base parser to handle the tokens it does not own. Returns 0
// when the stream was already bad on entry, or when the loop ended on something other than the
// sentinel.
unsigned char Obj0x478118::ParseTokenField(istream *pStream) {
    unsigned short tmp;
    unsigned char bResult = 0;
    char szTok[264];

    if (!pStream->bad()) {
        bResult = 1;
        *pStream >> szTok;
        while (_stricmp(szTok, "-9") != 0 && !pStream->eof()) {
            if (_stricmp(szTok, "walk_speed") == 0) {
                *pStream >> tmp;
                bWalkSpeedMaybe = (unsigned char)tmp;
                *pStream >> tmp;
                bWalkStepCountMaybe = (unsigned char)tmp;
            } else if (_stricmp(szTok, "Employable") == 0) {
                *pStream >> tmp;
                bEmployable = (unsigned char)tmp;
            } else if (_stricmp(szTok, "sex") == 0) {
                *pStream >> szTok;
                if (toupper(szTok[0]) == 'M') {
                    dwSex = 'M';
                } else {
                    dwSex = 'F';
                }
            } else if (_stricmp(szTok, "groundwidth") == 0) {
                *pStream >> tmp;
                bGroundWidth = (unsigned char)tmp;
            } else if (_stricmp(szTok, "SpawnLimit") == 0) {
                *pStream >> tmp;
                bSpawnLimit = (unsigned char)tmp;
            } else if (_stricmp(szTok, "PickUpSoundId") == 0) {
                *pStream >> nPickUpSoundId;
            }
            *pStream >> szTok;
        }
        if (_stricmp(szTok, "-9") != 0) {
            bResult = 0;
        }
    }
    return bResult;
}
