// ThumbnailBmp -- the layout-preview/thumbnail bitmap class (ctor 0x447b20, dtor 0x447b90,
// 472 bytes). Its full internals (load/save/decode, see docs/subsystems.md's ThumbnailBmp entry)
// are still mostly unmodeled; only the handful of fields actually read by src/ consumers are
// named, the rest kept as sized pad so sizeof stays 472. ONE canonical shared definition (was
// previously a local class in src/WidgetPicker.h) -- WidgetPicker.h, DPlaySessionMgr.cpp, and
// NetSessionEventQueue.cpp (transitively via WidgetPicker.h) all use this header now, per
// CLAUDE.md's "never duplicate a struct" rule. The +0x4/+0xb0..+0x1c4 region (the streamed
// object/train-placement scratch + header) was previously a SECOND, richer duplicate view
// (NetSessionEventQueue.cpp's own now-retired ThumbnailBmpFieldsPartial, reinterpret_cast onto
// the same real object) -- merged in here 2026-07-21 so there's exactly one definition.
//
// EVERY method is now DEFINED in src/ThumbnailBmp.cpp (2026-07-27..28) -- the class is fully
// transcribed. Everything stays out of line here so every use site emits a genuine cross-TU
// call, matching the original's own.
#pragma once

#include <stddef.h>

#include "Pair16.h"

// <fstream.h>/<strstrea.h>, forward-declared to avoid pulling iostream.lib's headers into
// every consumer of this file -- exactly as src/CursorDesc.h, src/DSoundChannel.h and
// src/LocoBitmap.h each do.
class istream;
class ostream;

// One enumerated "car slot" within an ObjectPlacementRecord below (offsets confirmed directly
// from LoadLayoutAndPopulateBoard's own pointer arithmetic, via Ghidra's already-typed
// ushort*/int* decompile -- not guesswork): wCarKindId compared against 0 to gate the slot,
// szData compared against the literal "PARTY" and passed to the resolved car's own vtbl+0x34.
struct CarSlotRecord {
    unsigned short wCarKindId; // +0x0
    unsigned short pad0x2;
    unsigned int dwUnkMaybe;        // +0x4
    char szData[12];           // +0x8
};

// One streamed "placed object" record, filled by ThumbnailBmp::GetNextObjectRecord (0x447db0)
// from the loaded layout's own object list. 0x80 bytes total, confirmed via the array's own
// end-of-struct boundary (arrCarSlots ends exactly at +0x80) AND via GetNextObjectRecord's own
// raw disasm (reads exactly 0x80 bytes into this scratch region at ThumbnailBmp+0x4).
struct ObjectPlacementRecord {
    unsigned short wKindId;  // +0x0
    Pair16 offset;      // +0x2 (lo=sOffsetX, hi=sOffsetY -- confirmed a single packed
                                  // dword by SaveBoardLayout's own single-mov copy of it)
    unsigned char pad0x6[2];
    unsigned int dwArgAMaybe;     // +0x8, passed to the placed object's own vtbl+0x1c
    unsigned int dwArgBMaybe;     // +0xc, stored into the placed object's own dwPlacementArgB
    unsigned char szCategoryNameBlob[0xc]; // +0x10, opaque 12-byte blob passed to the placed object's vtbl+0x34
    CarSlotRecord arrCarSlots[5]; // +0x1c, 5*0x14 = 0x64 -> ends at 0x80
};

// One streamed "train" record, filled by ThumbnailBmp::GetNextTrainRecord (0x447df0) and
// written by ThumbnailBmp::WriteTrainRecord (ex-FUN_00447f80, see NetSessionEventQueue::
// SaveBoardLayout, whose own car-slot-writing loop ground-truths all 3 fields here). 44 bytes
// total, confirmed via SaveBoardLayout's own zero-init extent (0xb dwords) AND via
// GetNextTrainRecord's own raw disasm (reads exactly 0x2c bytes into this scratch region at
// ThumbnailBmp+0x84).
struct TrainPlacementRecord {
    unsigned int dwCarKindIds[4];    // +0x0, per-slot BigObj resourceId (train kind +
                                            // up to 3 car type ids, per
                                            // PeerTrainRoster_SpawnOrAssignRandomTrainMaybe's
                                            // own read shape, out of scope here)
    unsigned int dwCarCategories[4]; // +0x10, per-slot CarNetObj::nCarCategory
    unsigned char szCategoryNameBlob[0xc];       // +0x20, car-slot-0's own szCategoryName string
                                            // (opaque 12-byte blob passed to the newly spawned
                                            // train's own car-slot vtbl+0x34 by
                                            // LoadLayoutAndPopulateBoard)
};

class ThumbnailBmp {
public:
    // +0x0 -- the implicit vptr. The class's vtable (0x478274) has exactly ONE slot, the
    // compiler-generated `??_G` scalar deleting destructor at 0x447b60, so the class has
    // exactly one virtual: the destructor below. Modeled as a real `virtual ~ThumbnailBmp()`
    // 2026-07-27 (was an explicit `void *pVtbl` member) -- same layout either way, but only
    // the virtual form makes cl emit the ctor/dtor's own vtable store and the `??_G` thunk.
    ObjectPlacementRecord objRecord;           // +0x4, scratch filled by GetNextObjectRecord/
                                               // written by WriteObjectRecord -- never
                                               // zero-inited by the ctor (genuinely uninitialized
                                               // until first use, per the ctor's own disasm).
    TrainPlacementRecord trainRecord;          // +0x84, ditto for GetNextTrainRecord/WriteTrainRecord.
    // +0xb0 -- 0 = not loaded; ThumbnailBmp_IsLoaded() checks == 8 (likely a decoded-format
    // tag, e.g. a BMP bit-depth/compression code, doing double duty as a load-state flag --
    // same field two independently-hedged names converged on: WidgetPicker.cpp's own
    // (now-retired) ThumbnailBmpPartial called it wLoadState, NetSessionEventQueue.cpp's own
    // (now-retired) ThumbnailBmpFieldsPartial called it wFormatTag. Kept `short` (signed) here
    // to match DPlaySessionMgr::LayoutSet_LoadSlotBitmap's already-EXACT read of the sibling
    // wWidth/wHeight fields just below -- see CLAUDE.md's field-signedness lesson.
    short wFormatTag;                        // +0xb0
    short wWidth;                            // +0xb2 -- decoded bitmap width  (cols)
    short wHeight;                           // +0xb4 -- decoded bitmap height (rows)
    unsigned char pad0xb6[2];                 // +0xb6
    int nObjectCount;                         // +0xb8 -- streamed object-record count
    unsigned short wTrainCount;                // +0xbc -- streamed train-record count
    // +0xbe -- write dest for the backdrop-reload helper (LoadLayoutAndPopulateBoard) AND,
    // per SaveBoardLayout, a string-copy dest reaching all the way to the struct's own header
    // end (widened from an earlier 2-byte placeholder once a second consumer confirmed its
    // real extent -- CLAUDE.md's "second consumer confirms a merged field" rule). The whole
    // [wFormatTag, pPixels) span (0xb0..0x1c4, 0x114 bytes) is read/written as ONE blob via
    // ThumbnailBmp_Load/ThumbnailBmp_Save's own stream calls -- confirmed via
    // ThumbnailBmp_Load's `FUN_00463810(stream, this+0xb0, 0x114)` and SaveBoardLayout's own
    // raw disasm zeroing loop (0x45 dwords = 0x114 bytes starting at wFormatTag).
    unsigned char szReloadDestScratch[0x1c4 - 0xbe]; // +0xbe
    void *pPixels;                           // +0x1c4 -- decoded 8bpp pixel buffer
    // +0x1c8..+0x1d8 -- the four stream/resource slots. Named 2026-07-27 (were one
    // `pad0x1c8[0x10]` blob) off ThumbnailBmp_Load/_Save/_CloseStreams' own reads; `sizeof`
    // held at 472 across the change, on the ctor's `operator new(0x1d8)` authority.
    // The ctor zeroes all four; CloseStreams virtual-deletes both streams, `free`s
    // pResourceBuf and `operator delete`s pPixels, nulling each as it goes.
    istream *pInStream;      // +0x1c8 -- an istrstream over pResourceBuf when the RF archive
                              //           has the file, else an ifstream on the loose path
    ostream *pOutStream;     // +0x1cc -- the ofstream ThumbnailBmp_Save opens
    void *pResourceBuf;      // +0x1d0 -- RFIndex::LoadResource's blob, backing pInStream.
                              //           Freed with free(), NOT operator delete -- the
                              //           archive loader mallocs it.
    int nResourceSize;       // +0x1d4 -- its byte length, LoadResource's own out-param

    ThumbnailBmp();           // 0x447b20, src/ThumbnailBmp.cpp
    virtual ~ThumbnailBmp();  // 0x447b90 (??_G thunk 0x447b60), src/ThumbnailBmp.cpp
    // 0x447ba0 -- load+decode a layout bitmap from `path`; returns nonzero on success. On
    // success the decoded dimensions/pixels are readable via wWidth/wHeight/pPixels.
    char ThumbnailBmp_Load(char *path);
    // 0x447e30 -- write the current header fields + a fresh board-capture bitmap out to `path`.
    unsigned char ThumbnailBmp_Save(char *path);
    // 0x448030, EXACT match (src/phase2_probe.cpp's own Obj0xb2::IsVal0xb0Eight). Declared
    // (not defined) here so every consumer TU emits a genuine out-of-line call, matching the
    // original's real cross-TU calls -- an inline body would get /O2-inlined whenever the
    // trivial check is visible in the same TU, unlike the separately-compiled original.
    bool ThumbnailBmp_IsLoaded();
    // 0x447fb0 -- closes both streams + frees the resource buffer + pixel buffer, zeroing each
    // as it goes.
    char ThumbnailBmp_CloseStreams();
    // 0x447db0 -- reads the next 0x80-byte object record into objRecord; returns &objRecord (or
    // 0 on a short/failed read once a stream is open).
    ObjectPlacementRecord *GetNextObjectRecord();
    // 0x447df0 -- ditto for trainRecord (0x2c bytes).
    TrainPlacementRecord *GetNextTrainRecord();
    // 0x447f50 -- streams *pRec (0x80 bytes) out to the open output stream.
    unsigned char WriteObjectRecord(ObjectPlacementRecord *pRec);
    // 0x447f80 -- streams *pRec (0x2c bytes) out to the open output stream.
    unsigned char WriteTrainRecord(TrainPlacementRecord *pRec);
};
