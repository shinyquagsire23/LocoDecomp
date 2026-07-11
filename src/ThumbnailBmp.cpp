// ThumbnailBmp's own transcribed methods (canonical class in ThumbnailBmp.h). Kept as a
// separate TU so every consumer still emits a genuine out-of-line call, matching the
// original's own cross-TU calls (see ThumbnailBmp.h's declared-only note).
#include <fstream.h>
#include <strstrea.h>
#include <string.h>

#include "ThumbnailBmp.h"
#include "DSoundChannel.h"  // RFIndex/g_RFIndex/g_pInstallPathPrefix/_free
#include "LocoBitmap.h"      // the capture bitmap ThumbnailBmp_Save snapshots the board into
#include "WorldBoardMaybe.h" // g_worldBoard, CaptureBoardToBitmap

// FUNCTION: LOCO 0x448030
// EXACT MATCH (moved out of src/phase2_probe.cpp 2026-07-22, where it lived as the
// probe-local Obj0xb2::IsVal0xb0Eight). Load-validity check on the +0xb0 state word
// (0 = not loaded, 8 = loaded ok, per docs/subsystems.md).
bool ThumbnailBmp::ThumbnailBmp_IsLoaded() { return wFormatTag == 8; }

// FUNCTION: LOCO 0x447b20
// Zeroes the pixel buffer, the resource-blob slot and the header word triple, then both
// stream slots. The record scratch at +0x4/+0x84 is deliberately NOT zeroed -- it is genuinely
// uninitialized until the first GetNext*Record call.
ThumbnailBmp::ThumbnailBmp()
{
    pPixels = NULL;
    pResourceBuf = NULL;
    nResourceSize = 0;
    wFormatTag = 0;
    wWidth = 0;
    wHeight = 0;
    pInStream = NULL;
    pOutStream = NULL;
}

// EFFECTIVE MATCH -- 30 B vs 36, DIFF(19). The original INLINES the dtor's two statements into
// this thunk (`mov [esi],vtbl; call CloseStreams`) where ours CALLS ??1ThumbnailBmp, which is
// what an IN-CLASS dtor definition produces -- the same shape src/GameNetMsgQueue.h's parked
// ??_GNetSettings row describes. **Measured and it DOES close: moving `~ThumbnailBmp() {
// ThumbnailBmp_CloseStreams(); }` into the class body in src/ThumbnailBmp.h makes 0x447b60
// EXACT at 36 B and this file 8/8.** Parked anyway, because the in-class form also makes every
// consumer TU emit its own ThumbnailBmp dtor COMDAT, and that reshuffle costs
// src/DPlaySessionMgr.cpp its `ApplSetupWnd::SendSelectRequestMaybe` (0x40ac50) match --
// 345 B EXACT -> DIFF(4) at 324 B. Net for the in-class form is +414 - 345 = +69 B against
// this out-of-line form's +389 B, so the out-of-line form stays. Retry once DPlaySessionMgr.cpp
// is split (it is carrying the whole ApplSetupWnd middle block; see src/ApplSetupWnd.cpp).
//
// FUNCTION: LOCO 0x447b60 (??_GThumbnailBmp scalar deleting dtor -- compiler-generated around
// ~ThumbnailBmp() below; no source of its own)

// FUNCTION: LOCO 0x447b90
ThumbnailBmp::~ThumbnailBmp()
{
    ThumbnailBmp_CloseStreams();
}

// EFFECTIVE MATCH -- DIFF(2) of 520 bytes, insns 150/150, align 0, reg_pen 0, identity_miss 0.
// The ONLY disagreement is the two displacement bytes at 0x447d05/0x447d0c, i.e. WHICH of the two
// 16-bit loads feeding the `imul` lands in the destination register: the original does
// `mov di,[esi+0xb4]; mov ax,[esi+0xb2]; imul edi,eax` (wHeight in the dest) where cl here emits
// the operands the other way round. Every other byte of the function, funclets included, is
// identical. Probed and CLOSED as intrinsic: nine source shapes all produce the SAME two bytes --
// `w*h`, `h*w`, `n=h; n*=w;`, `n=w; n*=h;`, a hoisted `unsigned int nPixels;` declaration with
// either order, `int nPixels`, and an `unsigned short` temp for either operand. Repeating the size
// expression at all three use sites instead of holding it in a local is much WORSE (DIFF 127 at
// 564 B -- cl recomputes rather than CSEs), so the explicit local is confirmed correct.
// Root cause isolated with a standalone probe: cl respects the source operand order for
// `(unsigned short)a * (unsigned short)b` in a plain /GX try-block frame, but the `throw 1` on the
// HEADER read's gcount check -- i.e. an EH edge INSIDE the same try, ahead of the multiply --
// makes it canonicalize the commutative operands by ascending field offset and ignore the source
// order entirely. See docs/CODEGEN.md's commutative-operand bullet.
//
// FUNCTION: LOCO 0x447ba0
// Opens `path` -- preferring the RF archive (an istrstream over a LoadResource blob, with the
// install-path prefix stripped off the front to get an archive-relative name) and falling back to
// a loose ifstream -- then reads the 0x114-byte header blob and the raw 8bpp pixels. Returns
// nonzero only if a stream opened cleanly AND every read completed; the three throw sites all
// land in this function's own two catch clauses, which reset the flag and release everything.
// Catch types/parameters are ground-truthed from the image's own __ehfuncinfo at 0x47b958:
// one try state with catchHigh 3 and two handlers, `.H` (int) at 0x447d84 and `.PAD` (char *) at
// 0x447d96, both with a NONZERO catch-object displacement -- i.e. genuinely NAMED (if unused)
// catch parameters, not the bare `catch (int)` form, which would leave dispCatchObj 0.
char ThumbnailBmp::ThumbnailBmp_Load(char *path)
{
    char bLoaded = 0;

    ThumbnailBmp_CloseStreams();
    if (g_RFIndex.IsOpen()) {
        pResourceBuf = g_RFIndex.LoadResource(
            (const unsigned char *)(path + strlen(g_pInstallPathPrefix)), &nResourceSize);
        if (pResourceBuf != 0) {
            pInStream = new istrstream((char *)pResourceBuf, nResourceSize);
        }
    }
    if (pInStream == 0) {
        pInStream = new ifstream(path, ios::nocreate | ios::binary);
    }
    if (pInStream != 0 && pInStream->good()) {
        bLoaded = 1;
        try {
            pInStream->read((char *)&wFormatTag, 0x114);
            if (pInStream->gcount() != 0x114) {
                throw 1;
            }
            unsigned int nPixels = (unsigned short)wWidth * (unsigned short)wHeight;
            pPixels = new char[nPixels];
            if (pPixels == 0) {
                throw "Unable to allocate thumbnail memory";
            }
            pInStream->read((char *)pPixels, nPixels);
            if ((unsigned int)pInStream->gcount() != nPixels) {
                throw 3;
            }
        } catch (int nErrCode) {
            bLoaded = 0;
            ThumbnailBmp_CloseStreams();
        } catch (char *pszError) {
            bLoaded = 0;
            ThumbnailBmp_CloseStreams();
        }
    }
    return bLoaded;
}

// FUNCTION: LOCO 0x447fb0
// Tears down everything the load/save paths allocate, nulling each slot as it goes so a repeat
// call is harmless -- which is what makes ThumbnailBmp_Load able to open with it.
char ThumbnailBmp::ThumbnailBmp_CloseStreams()
{
    if (pInStream != NULL) {
        delete pInStream;
        pInStream = NULL;
    }
    if (pOutStream != NULL) {
        delete pOutStream;
        pOutStream = NULL;
    }
    if (pResourceBuf != NULL) {
        _free(pResourceBuf);
        pResourceBuf = NULL;
    }
    if (pPixels != NULL) {
        delete pPixels;
        pPixels = NULL;
    }
    return 1;
}

// FUNCTION: LOCO 0x447db0
// Reads the next object record off the open input stream. With no stream open it hands back the
// scratch record unread -- that is the original's own behaviour, not an oversight: the caller
// drives the record count off nObjectCount, which only a real load ever sets.
ObjectPlacementRecord *ThumbnailBmp::GetNextObjectRecord()
{
    if (pInStream != NULL) {
        pInStream->read((char *)&objRecord, sizeof(objRecord));
        if (pInStream->gcount() != sizeof(objRecord)) {
            return NULL;
        }
    }
    return &objRecord;
}

// FUNCTION: LOCO 0x447df0
TrainPlacementRecord *ThumbnailBmp::GetNextTrainRecord()
{
    if (pInStream != NULL) {
        pInStream->read((char *)&trainRecord, sizeof(trainRecord));
        if (pInStream->gcount() != sizeof(trainRecord)) {
            return NULL;
        }
    }
    return &trainRecord;
}

// FUNCTION: LOCO 0x447f50
unsigned char ThumbnailBmp::WriteObjectRecord(ObjectPlacementRecord *pRec)
{
    if (pOutStream != NULL && (pOutStream->rdstate() & ios::badbit) == 0) {
        pOutStream->write((char *)pRec, sizeof(*pRec));
        return 1;
    }
    return 0;
}

// FUNCTION: LOCO 0x447f80
unsigned char ThumbnailBmp::WriteTrainRecord(TrainPlacementRecord *pRec)
{
    if (pOutStream != NULL && (pOutStream->rdstate() & ios::badbit) == 0) {
        pOutStream->write((char *)pRec, sizeof(*pRec));
        return 1;
    }
    return 0;
}

// EFFECTIVE/PARTIAL -- 446 B vs 288, insns 132/76, reg_pen 3, total 388539. The ENTIRE 56
// -instruction gap is the `capture` local's destructor: cl INLINES LocoBitmap's dtor at both
// exits (the `cmp al,1` / free-palette / free-pixels / virtual-release blocks) because
// src/LocoBitmap.h defines it IN-CLASS, where the original emits a plain out-of-line
// `lea ecx,[esp+8]; call LocoBitmap::~LocoBitmap` at each (0x447f01, 0x447f26). Everything
// above that -- the CloseStreams call, the `new ofstream(path, 0x92, 0x1a4)`, the badbit
// guard, the board capture and both writes -- lines up. Closing it means moving LocoBitmap's
// dtor out of line, which is a REPO-WIDE header experiment (that header is included by ~20
// TUs and its inline dtor is load-bearing for matches elsewhere), not a local fix. Left as a
// faithful PARTIAL rather than distorted to chase the inline.
//
// FUNCTION: LOCO 0x447e30
// Snapshots the live world board into a fresh 8bpp capture and writes the header blob plus the
// raw pixels out to `path`. The capture bitmap is a plain local, so /GX wraps the whole body in
// its unwind funclet -- that scaffolding is the compiler's, not the source's.
unsigned char ThumbnailBmp::ThumbnailBmp_Save(char *path)
{
    LocoBitmap capture;

    ThumbnailBmp_CloseStreams();
    pOutStream = new ofstream(path, ios::out | ios::binary | ios::trunc, filebuf::openprot);
    if (pOutStream != NULL && (pOutStream->rdstate() & ios::badbit) == 0) {
        g_worldBoard.CaptureBoardToBitmap(&capture, 0);
        pOutStream->write((char *)&wFormatTag, 0x114);
        pOutStream->write((char *)capture.pPixels,
                          (unsigned short)wWidth * (unsigned short)wHeight);
        return 1;
    }
    return 0;
}
