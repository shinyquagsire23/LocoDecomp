// CarNetState -- the per-car network/identity state object (0x39c = 924 bytes; vtable
// 0x478264), and its wire twin CarNetStateAlt (0x390, vtable 0x478268) further down. Ghidra's
// own struct DB has the field layout too -- see docs/subsystems.md's `CarNetState` entry.
// The whole class is now transcribed in src/CarNetState.cpp: both constructors, the dtor, the
// .crd file round-trip, the stamp picker and the three decal-array methods.
#pragma once

#include <windows.h>   // LONG -- used directly by this header's own members (see PostBag.h's
                       //   note on why these headers carry their own <windows.h>)
#include "PostBag.h"

class CarNetStateAlt;

// ⭐ The two name fields are MEMBER CLASSES, not bare char arrays, and that is a byte-match
// PROOF, not a style choice. All three constructors in this cluster (CarNetState's two and
// CarNetStateAlt's one) emit `nameA[0]=0; nameA[20]=0; nameB[0]=0` BEFORE the compiler's
// implicit vptr store, and an inlined member default-ctor is the only construct in cl 11.00 that
// schedules stores ahead of the vptr -- a bare `char[21]` cannot, because an array element can
// never appear in a mem-initializer list. Writing the three clears as ordinary body statements
// instead puts the vptr FIRST and costs all three ctors their match (DIFF 9 / 20 / 10 at
// otherwise byte-identical, instruction-for-instruction bodies).
//
// They are TWO DISTINCT TYPES, which is the second half of the proof. A single two-name
// sub-object addresses nameB off the shared base (`[edi+0x15]`) and still misses; only separate
// members, each with its own `this`, reproduce the original's hoisted `lea eax,[edx+0x1d]` for
// nameB and its reuse as the memcpy destination further down. The asymmetric constructors are
// the original's own: nameA clears its LAST byte as well as its first, because sz[20] doubles
// as a flag byte (src/EditCardWnd.cpp's OnEditName writes `nameA[0x14] = 1`/`= 0` and
// src/DPlaySessionMgr.cpp tests it, both around a strcpy of a name that is short enough to
// leave it intact). nameB has no such flag and clears only sz[0].
//
// `operator char *` is why every existing `strcpy(pCard->nameA, ...)` / `pCard->nameB[0]` /
// `sizeof(state.nameA)` call site across the five consumer TUs still compiles untouched, and
// still to the same bytes.
struct CardNameA {
    char sz[21];
    CardNameA() { sz[0] = 0; sz[20] = 0; }
    operator char *() { return sz; }
};

// ⭐ CardNameB is TWENTY bytes, not 21 -- pinned by CarNetObj_ApplyNetState (0x40d770), whose
// body is the compiler-generated `CarNetState::operator=`. A memberwise copy-assignment copies
// exactly sizeof() per member and skips alignment padding, so the bytes it DOESN'T touch are a
// direct readout of the layout: nameA moves 21 bytes (5 dwords + 1 byte) but nameB moves only 20
// (5 dwords flat), and the byte at +0x39 is never copied because it is padding before
// wAttachmentId's 2-byte alignment at +0x3a.
//
// This retires four separate `// sic:` notes that all described the same phantom: the "21st byte
// of nameB is never copied/cleared" asymmetry was never an asymmetry at all -- there is no 21st
// byte. src/AlbumCardWnd.h's `char aSlotNames[6][20]` (strcpy'd straight from nameB) is the
// independent corroboration: the original programmer sized that destination at exactly 20.
// TrainSyncCarRecord (src/GameNet.h) and CarNetStateAlt agree too -- both put a pad byte after
// their own nameB for the same alignment reason.
struct CardNameB {
    char sz[20];
    CardNameB() { sz[0] = 0; }
    operator char *() { return sz; }
};

class CarNetState {
public:
    unsigned short wSignature;    // +0x04 -- .crd file signature, ctor sets 0x66; LoadCardFile
                                   //   validates it (== the wire-record mirror's own signature)
                                   // +0x06 -- IMPLICIT alignment padding, not a member: the
                                   //   memberwise operator= in CarNetObj_ApplyNetState (0x40d770)
                                   //   moves a WORD at +0x04 and then jumps straight to the dword
                                   //   at +0x08, so there is nothing declared in between. Was
                                   //   modeled as an `unsigned short Unk0x06` member until v474.
    unsigned int ownerClientId;
    unsigned int nPostSeqId;      // +0x0c -- post-sequence id: atoi'd from AllocNextPostSeqIdString,
                                   //   used as the .crd/.att filename and PostBagAlbumIndexRecord::nId
    CardNameA nameA;              // +0x10
    CardNameB nameB;              // +0x25
    unsigned short wAttachmentId; // +0x3a -- attachment file id (PostBag_BuildAttFilePath/
                                   //   PostBag_DeleteAttachmentFiles); nonzero gates the "has
                                   //   attachment" badge + an outbound appearance request
    unsigned int bAttachmentSoundPlayedMaybe; // +0x3c -- ctor default 1; DPlaySessionMgr.cpp's
                                   //   arrival-sound check reads it as "already announced" when
                                   //   0 && wAttachmentId != 0 (full dword, not a bool -- hedged)
    unsigned char byIdentityColorR; // +0x40 -- PostBag_ComputeTintColor(R,G,B) arg 1
    unsigned char byIdentityColorG; // +0x41 -- PostBag_ComputeTintColor(R,G,B) arg 2
    unsigned char byIdentityColorB; // +0x42 -- PostBag_ComputeTintColor(R,G,B) arg 3
    char szDescription[80]; // strcpy'd default text (g_szDefaultDescriptionMaybe) by RebuildLocalPlayerCard
    unsigned char byStampSlotB;     // +0x93 -- DrawCornerBadgeB's bySlotIndex arg
    unsigned char byStampSlotA;     // +0x94 -- DrawCornerBadgeA's bySlotIndex arg
    unsigned char byStampVariantA;  // +0x95 -- DrawCornerBadgeA's byVariant arg (same call site)
    // +0x96 -- 128 placed clip-art "decal" slots, scanned backward by
    // PostBagCacheBundle::DrawLastPlacedItem for the last occupied (placementSeq != 0)
    // slot and drawn via PostBagCacheBundle::DrawPlacedClipartItem (confirmed 2026-07-17).
    // Both constructors zero-init (or copy) the array; see src/CarNetState.cpp.
    DecalSlot decalSlots[128];    // +0x96 .. +0x396
                                   // +0x396 -- IMPLICIT alignment padding, not a member, on the
                                   //   same 0x40d770 evidence as +0x06 above: the memberwise
                                   //   copy skips these two bytes and resumes at +0x398. Was
                                   //   modeled as an `unsigned char pad0x396[2]` member until
                                   //   v474; a declared array member WOULD have been copied.
    unsigned int Unk0x398;

    // 0x442850 -- default ctor: a blank card owned by the local player. Defined in
    // src/CarNetState.cpp.
    CarNetState();

    // 0x4428e0 -- the RECEIVE half of the wire round-trip: rebuild a full card from a
    // CarNetStateAlt snapshot. The mirror image of CarNetStateAlt's own ctor below; between them
    // they pin CarNetStateAlt's entire layout. Defined in src/CarNetState.cpp.
    CarNetState(CarNetStateAlt *pWire);

    virtual ~CarNetState();

    // 0x442a70 -- transcribed in src/CarNetState.cpp (moved there from src/EditCardWnd.cpp).
    unsigned char SaveCardFile(char *pszDir);

    // Assigns the +0x94/+0x95 stamp slot/variant pair (nKind 0 clears, 1/2/3 select the variant
    // and slot, -1 randomizes). Transcribed in src/CarNetState.cpp. ⚠ src/DPlaySessionMgr.cpp
    // still carries a methods-only `CarNetStateEasterView0x43e900` view of this same method,
    // pre-existing debt to fold onto this declaration when 0x43e900 is next reopened.
    void AssignStampSlotVariantMaybe(int nKind, char nSlot); // 0x442bf0

    // Load counterpart to SaveCardFile: ReadFile's exactly 0x398 bytes (sizeof-4) into
    // wSignature.., then checks the leading word == 0x66 (the .crd file signature, same convention
    // as PostBag_ScanCategoryCrdFiles). CORRECTED (mis-boxing fix, v154): was wrongly
    // address-boxed under CarNetStateAlt. Sole caller: CarNetState_CreateFromFile
    // (0x444c70, src/EditCardWnd.cpp), which new_alloc(0x39c)s + default-ctors this before
    // calling here. Transcribed in src/CarNetState.cpp (moved there from src/EditCardWnd.cpp).
    unsigned char LoadCardFile(const char *pszPath); // 0x442b50

    // 0x442d30 -- back-to-front PtInRect over decalSlots (center = stored coord x2),
    // click-to-remove hit test against a point in the card-art-relative coordinate space
    // (see docs/subsystems.md's DecalSlot entry). Transcribed in src/CarNetState.cpp.
    unsigned char RemoveDecalAtPoint(LONG x, LONG y); // 0x442d30

    // 0x442c90 -- fixed-128 FIFO decal placement, drop-oldest-when-full (see docs/subsystems.md's
    // DecalSlot entry). byWidth/byHeight are read from the placed decal's thumbnail
    // LocoBitmap at a byte width/offset that doesn't match its own named fields cleanly (byWidth
    // reads only the low byte of LocoBitmap::height, byHeight reads LocoBitmap::bOwnsPalette) --
    // see the call site in src/EditCardWnd.cpp's OnLButtonDown for the `// sic:` note.
    // Transcribed in src/CarNetState.cpp.
    unsigned char AddDecal(char nSubkind, char nKind, unsigned char nSlotPlusOne, int x, int y, unsigned char byWidth, unsigned char byHeight); // 0x442c90

    // 0x442e00 -- restore the decal array's "occupied slots first, no gaps" invariant after a
    // removal or an eviction. Called only by the two above. Transcribed in src/CarNetState.cpp.
    void CompactDecals(); // 0x442e00
};

// Factory: new CarNetState() + LoadCardFile(pszPath); returns the loaded object, or
// NULL (deleting the failed object first) if the file doesn't exist/isn't a valid .crd. Plain
// __stdcall free function (not a real this-bound method despite the Ghidra CarNetState::
// namespace -- it never reads an implicit this). Used by PostBagFileCache::FindFirstLoadableCardAtOrAfterIndex
// (src/EditCardWnd.cpp) and several other card-loading call sites across the PostBag cluster.

// Factory: `new CarNetState(pWireRecord)`. Returns the initialized object, or NULL on alloc
// failure. __fastcall (wire-record pointer in ecx). Transcribed in src/CarNetState.cpp; the
// `void *` is the seam with GameNet.h's raw RosterCarStateBlockMaybe view of the same bytes.
// Consumer: GameNet_ReceiveRosterSnapshot (src/GameNet.cpp), one call per PlayerRosterWireMsg record.
extern CarNetState *__fastcall CarNetState_CreateFromWireRecord(void *pWireRecord); // 0x442fa0

// Returns &pCar->stateMaybe (the CarNetState embedded at CarNetObj+0x88) if the car's
// bStateAppliedMaybe flag (+0x424) is set, else NULL. __fastcall (single car pointer in ecx).
// Declared-only (extern); the car object itself is not modeled in src/, so it's passed opaquely.
extern CarNetState *__fastcall CarNetObj_GetAppliedState(void *pCar); // 0x40d750

// CarNetObj's own type-id accessor (0x1870/0x1871 = the two hand-off socket states; -1 unless
// the embedded AnimDescRefObj0x477488 base's own "ready" pointer is set). Declared as a free
// __fastcall (this-in-ecx), same convention as CarNetObj_GetAppliedState above -- redeclared here
// (also used by src/DPlaySessionMgr.cpp) rather than expanding CarNetObj's own class
// surface just for a plain accessor call.
extern int __fastcall CarNetObj_GetCarTypeId(void *pCar); // 0x40e0d0

// Sibling class of CarNetState (own vtbl 0x478268 vs CarNetState's 0x478264, size 0x390 vs
// 0x39c) -- the WIRE form of a card, NOT the same class. Discovered via the operator-new xref
// address-boxing survey 2026-07-12 and modeled opaquely (dtor only) until this session, when its
// two mirror ctors -- CarNetStateAlt(CarNetState*) at 0x442ec0 and CarNetState(CarNetStateAlt*)
// at 0x4428e0 -- pinned every field of it. Each ctor is a field-by-field copy in the opposite
// direction, so the two independently agree on every offset below, and the layout closes at
// exactly the 0x390 that its operator new(0x390) call site already proved.
//
// It is CarNetState minus the two purely local identity fields (ownerClientId, nPostSeqId --
// 8 bytes, which is why every field from nameA on sits 8 lower here), plus an explicit
// nDecalCount, minus CarNetState's 6-byte unused tail. GameNet.h's RosterCarStateBlockMaybe is
// a raw 0x390-byte view of exactly this object: GameNet_BroadcastPlayerRoster memcpy's a
// snapshot straight onto the msg 0x3ec wire, and GameNet_ReceiveRosterSnapshot hands the bytes
// straight back to CarNetState_CreateFromWireRecord. TODO: fold RosterCarStateBlockMaybe into
// this type so the wire message can declare `CarNetStateAlt records[1]` and the factory below
// can drop its `void *`.
class CarNetStateAlt {
public:
    unsigned short wSignature;    // +0x04 -- neither ctor touches it; the .crd signature word's
                                   //   slot, carried over positionally from CarNetState
    unsigned short Unk0x06;       // +0x06 -- likewise never read or written; alignment filler
    CardNameA nameA;              // +0x08
    CardNameB nameB;              // +0x1d
    unsigned short wAttachmentId; // +0x32
    unsigned int bAttachmentSoundPlayedMaybe; // +0x34
    unsigned char byIdentityColorR;   // +0x38
    unsigned char byIdentityColorG;   // +0x39
    unsigned char byIdentityColorB;   // +0x3a
    char szDescription[80];       // +0x3b -- strcpy'd, not block-copied, in both directions
    unsigned char byStampSlotB;   // +0x8b
    unsigned char byStampSlotA;   // +0x8c
    unsigned char byStampVariantA; // +0x8d
    unsigned short nDecalCount;   // +0x8e -- the field CarNetState has no counterpart for: the
                                   //   send side always writes the full 128, but the receive side
                                   //   honours whatever count arrives and zero-fills the rest
    DecalSlot decalSlots[128];    // +0x90 -- 0x300 bytes, closing the object at 0x390

    // 0x442ec0 -- the SEND half of the wire round-trip. Defined in src/CarNetState.cpp.
    CarNetStateAlt(CarNetState *pSrcState);

    // Defined INLINE, unlike CarNetState's own dtor right above -- and the asymmetry is the
    // image's, not a style choice. The wire twin owns nothing either, so both dtors are a bare
    // vptr store; the difference is that CarNetState's out-of-line body really exists at 0x442a00
    // and is really called from ~CarNetObj and five other sites, while NO out-of-line
    // ??1CarNetStateAlt exists anywhere in the image (searched the whole .text for
    // `mov [ecx],0x478268; ret` -- absent, where CarNetState's own `mov [ecx],0x478264; ret` is
    // right there at file offset 0x41e00). Nothing ever destroys one of these except `delete`
    // through the vtable, so the only body cl ever needed is the one folded into
    // ??_GCarNetStateAlt at 0x442ea0 -- which is exactly what an inline dtor emits, and what
    // makes that thunk byte-exact.
    virtual ~CarNetStateAlt() {}
};

// Factory: `new CarNetStateAlt(pSrcState)`. Returns the initialized object, or NULL on alloc
// failure. __fastcall (source state pointer in ecx). Transcribed in src/CarNetState.cpp.
// Sole caller: GameNet_BroadcastPlayerRoster (src/GameNet.cpp, msg 0x3ec send side).
extern CarNetStateAlt *__fastcall CarNetStateAlt_CreateFromState(CarNetState *pSrcState); // 0x442a10
