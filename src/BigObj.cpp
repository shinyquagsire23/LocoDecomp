// BigObj -- the interned per-tile-kind descriptor's own .obj. See docs/subsystems.md's BigObj
// entry for the class writeup; the class itself is declared in src/CursorDesc.h (its address-order
// home is here, but the type has to live beside its CursorDesc/Obj0x4779e0 bases so the whole
// 5-tier descriptor family is one definition).
//
// This TU was opened 2026-07-31 for the 0x44b030..0x44bd90 contiguous BigObj block, which was the
// largest fully-unclaimed app region left. Its neighbours 0x44b4f0 (ParseTokenField) and 0x44bdb0
// (IsAtMatchingBoardEdgeMaybe) already live in src/CursorDesc.cpp and stay there -- moving them
// would be a measured change for no gain.
#include "CursorDesc.h"
#include "DSoundChannel.h" // RFIndex/g_RFIndex/g_pInstallPathPrefix/_free

#include <fstream.h>
#include <strstrea.h>
#include <stdio.h>

// FUNCTION: LOCO 0x44b190
// BigObj::BigObj -- the BigObj tier's ctor. Constructs the Obj0x4779e0 base with the SAME kind
// id but a NULL definition name, so the base's own loader is a no-op and the .dat file is read
// exactly once, here, by this tier's Load. Then stamps vtable 0x478358, NULLs the owned socket
// table, and runs the full three-block parse.
BigObj::BigObj(unsigned int kindId, char *pszDefinition) : Obj0x4779e0(kindId, NULL) {
    pSocketTable = NULL;
    Load(kindId, pszDefinition);
}

// FUNCTION: LOCO 0x44b200 (??_GBigObj scalar deleting dtor -- cl auto-emits it from the virtual
// dtor below; no source of its own)

// FUNCTION: LOCO 0x44b220
// BigObj::~BigObj -- frees the owned socket table, then the compiler's own teardown chains
// ~Obj0x4779e0 (0x41e620). The scalar `delete` (plain operator delete, no vector cookie) is the
// original's: pSocketTable holds packed shorts, not objects.
BigObj::~BigObj() {
    if (pSocketTable != NULL) {
        delete pSocketTable;
        pSocketTable = NULL;
    }
}

// FUNCTION: LOCO 0x44b290
// BigObj::Load -- vtable slot 4 (+0x10), the per-kind .dat loader of the BigObj tier. Structurally
// the same loader as its CursorDesc (0x424bf0) and Obj0x4779e0 (0x41e6e0) ancestors: build
// "<prefix><name>.dat" (a local, kept only for the loose-file fallback) and "<prefix><name>.bmp"
// (into the persistent szBmpPath at +0x48), try the RF archive first under the install-relative
// "<name>.dat" key, and fall back to a loose ifstream when the archive parse never set
// bLoadOkFlag.
//
// What is specific to this tier is the PARSE, which runs THREE record blocks over one stream
// rather than one or two -- and strictly base-to-derived, because that is the order the blocks sit
// in the .dat file: Obj0x4779e0's extended keyword pass, then CursorDesc's own, then this tier's
// (through the virtual, so a CarKindDesc/Obj0x478118 leaf gets its own). Each stage is gated on
// the previous one succeeding, and bLoadOkFlag is rewritten after every stage rather than once at
// the end -- that repeated store is the original's, and it is what leaves the flag meaningful if a
// later stage aborts.
//
// ⚠ kindId is a genuine DEAD parameter -- the body never reads it, but `ret 8` proves the slot is
// real. Unlike CursorDesc::Load, this override does NOT seed resourceId/categoryByte from it; the
// caller has already done that through the base.
void BigObj::Load(unsigned int kindId, char *pszDefinition) { // TODO: sync
    int nSize;
    void *pRfBuf;
    istrstream *pRfStream;
    ifstream fileStream;
    char szDatPath[264];
    char szRfName[264];

    m_type0x63a = 0;
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
            if (pRfStream != NULL && !pRfStream->bad()) {
                bLoadOkFlag = Obj0x4779e0::ParseTokenField(pRfStream);
                bLoadOkFlag =
                    (bLoadOkFlag != 0 && CursorDesc::ParseTokenField(pRfStream) != 0) ? 1 : 0;
                bLoadOkFlag = (bLoadOkFlag != 0 && ParseTokenField(pRfStream) != 0) ? 1 : 0;
                delete pRfStream;
            }
            _free(pRfBuf);
        }
    }
    if (bLoadOkFlag == 0) {
        fileStream.open(szDatPath, ios::nocreate);
        if (!fileStream.bad()) {
            bLoadOkFlag = Obj0x4779e0::ParseTokenField(&fileStream);
            bLoadOkFlag =
                (bLoadOkFlag != 0 && CursorDesc::ParseTokenField(&fileStream) != 0) ? 1 : 0;
            bLoadOkFlag = (bLoadOkFlag != 0 && ParseTokenField(&fileStream) != 0) ? 1 : 0;
            fileStream.close();
        }
    }
}

// FUNCTION: LOCO 0x44bcd0
// "Is nSocketIndex one of the ENDS of this kind's socket chain?" -- index 0 always, the last
// index of the primary run (wSocketCount), and, only for a kind that actually has the second
// batch, that batch's own two ends (wSocketCount + 1 and wSocketCountExt). The two socket runs
// are stored back to back in pSocketTable, which is why the second batch's first index is the
// first batch's last plus one rather than a separate base.
//
// The `wSocketCountExt != 0` guard is what makes this four tests and not two: a single-run kind
// must NOT accept wSocketCount + 1, because that index is past the end of its table.
unsigned char BigObj::IsEndSocketIndexMaybe(short nSocketIndex)
{
    if (nSocketIndex == 0 || nSocketIndex == wSocketCount ||
        (wSocketCountExt != 0 &&
         (nSocketIndex == wSocketCount + 1 || nSocketIndex == wSocketCountExt))) {
        return 1;
    }
    return 0;
}
