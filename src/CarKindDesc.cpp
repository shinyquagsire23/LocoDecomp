// CarKindDesc -- the TRAIN/CAR kind descriptor (vtable 0x477610), a CursorDesc sibling of
// Obj0x4779e0/Obj0x478118 with a 0x644-byte tail that is almost entirely the two per-heading
// {dx,dy} offset tables. This TU is the class's whole non-ctor surface.
//
// The ctor (0x40e600) IS here as of 2026-07-31. It was parked on the grounds that reaching
// CursorDesc's 3-arg ctor needs a `CursorDesc(int, char *, int)` declaration in
// src/CursorDesc.h, which costs a measured -489 B elsewhere -- true, but the shared header is
// not the only way to name a base ctor. The TU-local shim below does it for nothing, exactly as
// src/Obj0x4779e0.cpp has always done for its own base ctor.
#include "CarKindDesc.h"

#include "DSoundChannel.h"  // RFIndex/g_RFIndex/g_pInstallPathPrefix/_free

#include <fstream.h>
#include <strstrea.h>
#include <stdio.h>
#include <string.h>

// FUNCTION: LOCO 0x40e600
// CarKindDesc::CarKindDesc -- byte-for-byte the same ctor as its two sibling tiers
// (Obj0x478118 0x436400, and BigObj 0x44b190 modulo that tier's one field init): run
// CursorDesc's 3-arg ctor in place, stamp vtable 0x477610, hand off to this tier's loader. The
// car-id pair this tier owns is zeroed by LoadMaybe, not here.
CarKindDesc::CarKindDesc(unsigned int kindId, char *pszDefinition)
    : CursorDesc(kindId, NULL, 1) {
    LoadMaybe(kindId, pszDefinition);
}

// FUNCTION: LOCO 0x40e680
// CarKindDesc::~CarKindDesc -- re-seats this tier's vtable and runs ~CursorDesc. Out of line:
// the original keeps ??1 and ??_G as two separate COMDATs.
CarKindDesc::~CarKindDesc() {
}

// FUNCTION: LOCO 0x40e660 (??_GCarKindDesc scalar deleting dtor)

// FUNCTION: LOCO 0x40e950
// CarKindDesc::LoadHeadingOffsetTablesMaybe -- (re)loads the SHARED per-heading offset tables.
// Unlike the rest of this class these do not come from the kind's own .dat: every train/car
// kind reads the same "trains\train.dat", so this runs once per descriptor and fills both
// tables identically. Zeroes all 400 shorts of each table first, then reads 160 rows of four
// shorts, interleaved one row at a time across the two tables (near-end {dx,dy} into
// aHeadingOffsetTableMaybe, far-end {dx,dy} into aOppositeEndHeadingOffsetTableMaybe) -- which
// is why only 160 of each table's 200 pairs are ever written. The RF archive is tried first
// (an istrstream over the loaded blob, looked up under the install-prefix-stripped name); a
// loose binary ifstream over the full path is the fallback. Returns 0 if no stream could be
// opened, if the stream was not good on entry, or if any of the 640 extractions failed.
unsigned char CarKindDesc::LoadHeadingOffsetTablesMaybe() {
    unsigned char bResult = 1;
    istream *pStream = NULL;
    void *pRfBuf = NULL;
    int nSize = 800;
    char szPath[256];

    memset(aHeadingOffsetTableMaybe, 0, sizeof(aHeadingOffsetTableMaybe));
    memset(aOppositeEndHeadingOffsetTableMaybe, 0, sizeof(aOppositeEndHeadingOffsetTableMaybe));
    sprintf(szPath, "%s%s\\%s", g_pInstallPathPrefix, "trains", "train.dat");
    if (g_RFIndex.IsOpen()) {
        // the archive is keyed on the install-relative name, so skip the prefix sprintf just
        // wrote rather than building a second string
        pRfBuf = g_RFIndex.LoadResource(
            (const unsigned char *)(szPath + strlen(g_pInstallPathPrefix)), &nSize);
        if (pRfBuf != NULL) {
            pStream = new istrstream((char *)pRfBuf, nSize);
        }
    }
    if (pStream == NULL) {
        pStream = new ifstream(szPath, ios::binary | ios::nocreate);
    }
    if (pStream == NULL || !pStream->good()) {
        bResult = 0;
    } else {
        for (int i = 0; i < 160; i++) {
            *pStream >> aHeadingOffsetTableMaybe[i * 2];
            if (pStream->fail()) {
                bResult = 0;
                break;
            }
            *pStream >> aHeadingOffsetTableMaybe[i * 2 + 1];
            if (pStream->fail()) {
                bResult = 0;
                break;
            }
            *pStream >> aOppositeEndHeadingOffsetTableMaybe[i * 2];
            if (pStream->fail()) {
                bResult = 0;
                break;
            }
            *pStream >> aOppositeEndHeadingOffsetTableMaybe[i * 2 + 1];
            if (pStream->fail()) {
                bResult = 0;
                break;
            }
        }
    }
    if (pStream != NULL) {
        delete pStream;
    }
    if (pRfBuf != NULL) {
        _free(pRfBuf);
    }
    return bResult;
}

// FUNCTION: LOCO 0x40e8d0
// CarKindDesc::ParseTokenField -- vtable slot 3 (+0xc) override (Ghidra:
// ParseHeaderAndLoadTablesMaybe). This tier owns no ini keywords: its whole record is one
// positional header line -- a discarded lead token, the two car-id endpoints, and the "-9"
// sentinel -- read as a single chained extraction. The heading tables are then loaded
// UNCONDITIONALLY, even when the header was missing or malformed, so a kind with a broken .dat
// still gets valid geometry.
unsigned char CarKindDesc::ParseTokenField(istream *pStream) {
    int nSentinel = 0;
    unsigned char bResult = 0;
    char szTok[264];

    if (!pStream->bad()) {
        bResult = 1;
        *pStream >> szTok >> wCarIdAMaybe >> wCarIdBMaybe >> nSentinel;
        if (nSentinel != -9) {
            bResult = 0;
        }
    }
    LoadHeadingOffsetTablesMaybe();
    return bResult;
}

// FUNCTION: LOCO 0x40e690
// CarKindDesc::LoadMaybe -- really vtable slot 4 (+0x10), the per-kind .dat loader (see the
// header's note on why it is declared non-virtual). Same RF-archive-then-loose-file shape as
// its two siblings: builds "<prefix><name>.dat"/"<prefix><name>.bmp", tries the archive, and
// feeds whichever stream parses through this tier's virtual ParseTokenField followed by
// CursorDesc's own. Matches Obj0x4779e0::LoadMaybe (0x41e6e0) rather than
// Obj0x478118::LoadMaybe (0x436490) in gating the loose-file fallback on the archive parse
// having failed. This tier's own state is just the car-id pair, zeroed up front.
void CarKindDesc::LoadMaybe(unsigned int kindId, char *pszDefinition) {
    int nSize;
    void *pRfBuf;
    istrstream *pRfStream;
    ifstream fileStream;
    char szDatPath[264];
    char szRfName[264];

    // `nSize = 0` is a STATEMENT, not an `int nSize = 0` initializer: written as an initializer
    // VC5 hoists the store above the `ifstream fileStream` constructor call, which is 29 bytes
    // off the original. See docs/CODEGEN.md.
    nSize = 0;
    wCarIdAMaybe = 0;
    wCarIdBMaybe = 0;
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
    if (bLoadOkFlag == 0) {
        fileStream.open(szDatPath, ios::nocreate);
        if (fileStream.is_open()) {
            bLoadOkFlag = ParseTokenField(&fileStream);
            bLoadOkFlag = (bLoadOkFlag != 0 && CursorDesc::ParseTokenField(&fileStream) != 0) ? 1 : 0;
            fileStream.close();
        }
    }
}

// FUNCTION: LOCO 0x40eb60
// Kind id -> the small category enum a CarNetObj stores in nCarCategory (its only caller inside
// this subsystem is CarNetObj::SetCarTypeAndCategory, 0x40e0f0). Physically the LAST COMDAT of
// this .obj, which is what puts it here rather than beside its callers.
//
// The ids are dense enough over 0x1804..0x1871 for cl to emit its two-level form -- a 0x6e-byte
// case-index byte table plus a 5-entry jump table, both trailing the 56 bytes of code inside the
// same COMDAT (so the whole extent is 0xd0, not 0x38; see CLAUDE.md on deriving `--len` for
// switch-heavy functions).
int MapCarTypeIdToCategoryMaybe(int nCarTypeId)
{
    switch (nCarTypeId) {
    case 0x1804:
    case 0x1806:
    case 0x1808:
        return 1;
    case 0x1866:
    case 0x1868:
    case 0x186a:
        return 2;
    case 0x186c:
    case 0x186e:
        return 3;
    case 0x1870:
    case 0x1871:
        return 4;
    default:
        return 0;
    }
}
