// CarNetState method bodies -- see CarNetState.h for the class writeup.
#include <windows.h>
#include <ddraw.h>
#include <stdlib.h>
#include "CarNetState.h"
#include "LocalPlayerIdentity.h"

// FUNCTION: LOCO 0x442850
// Default-construct a blank card: both name fields emptied (nameA is emptied at BOTH ends --
// its first byte and its last, index 20 -- while nameB only gets its first byte, an asymmetry
// the original really does have), no attachment, black identity tint, the .crd signature word,
// and the local player stamped in as the owner. bAttachmentSoundPlayedMaybe starts at 1 so a
// freshly built card never re-announces an attachment it never had.
//
// The 128 decal slots are cleared two bytes at a time -- only packedKind and placementSeq, not
// the four coordinate bytes -- which is all the rest of the class needs: every reader gates on
// placementSeq != 0 before it looks at a slot's geometry.
//
// The three name clears the original opens with are NOT in this body: they are CardNameA's and
// CardNameB's own inlined default constructors, which is the only way to get them ahead of the
// implicit vptr store. That is what closed this ctor -- see the CardNameA/CardNameB writeup in
// src/CarNetState.h for the full derivation.
CarNetState::CarNetState()
{
    this->byIdentityColorR = 0;
    this->byIdentityColorG = 0;
    this->byIdentityColorB = 0;
    this->wAttachmentId = 0;
    this->bAttachmentSoundPlayedMaybe = 1;
    this->szDescription[0] = 0;
    this->wSignature = 0x66;
    this->byStampSlotB = 0;
    this->nPostSeqId = 0;
    this->ownerClientId = g_pLocalPlayerIdentity->clientId;
    this->byStampSlotA = 0;
    this->byStampVariantA = 0;
    for (int i = 0; i < 128; i++) {
        this->decalSlots[i].packedKind = 0;
        this->decalSlots[i].placementSeq = 0;
    }
}

// FUNCTION: LOCO 0x4428e0
// Rebuild a full card from a wire snapshot -- the receive half of the CarNetState <-> the
// CarNetStateAlt round-trip, and the exact mirror of CarNetStateAlt's own ctor at the bottom of
// this file. Everything the wire carries is copied straight across; the two fields it does NOT
// carry (ownerClientId and nPostSeqId) are simply left uninitialized, which is safe only because
// the sole consumer immediately overwrites the name and applies the card onto a car it already
// owns (GameNet_ReceiveRosterSnapshot, src/GameNet.cpp).
//
// The decal array is the one place the two directions disagree: the sender always ships all 128
// slots, but this side honours whatever nDecalCount arrived and zero-fills the remainder, so a
// short record still leaves a well-formed compacted array.
//
// The three name clears CardNameA/CardNameB contribute here are redundant with the memcpys that
// immediately follow them -- the original really does zero nameA's first and last byte and
// nameB's first, then block-copy straight over all three. That redundancy is itself the evidence
// the names are member classes; see src/CarNetState.h.
CarNetState::CarNetState(CarNetStateAlt *pWire)
{
    memcpy(this->nameA, pWire->nameA, sizeof(this->nameA));
    memcpy(this->nameB, pWire->nameB, sizeof(this->nameB));
    strcpy(this->szDescription, pWire->szDescription);
    this->byStampSlotB = pWire->byStampSlotB;
    this->byStampSlotA = pWire->byStampSlotA;
    this->byStampVariantA = pWire->byStampVariantA;
    this->wAttachmentId = pWire->wAttachmentId;
    this->bAttachmentSoundPlayedMaybe = pWire->bAttachmentSoundPlayedMaybe;
    this->byIdentityColorR = pWire->byIdentityColorR;
    this->byIdentityColorG = pWire->byIdentityColorG;
    this->byIdentityColorB = pWire->byIdentityColorB;

    int i;
    for (i = 0; i < pWire->nDecalCount; i++) {
        this->decalSlots[i] = pWire->decalSlots[i];
    }
    for (; i < 128; i++) {
        this->decalSlots[i].packedKind = 0;
        this->decalSlots[i].placementSeq = 0;
    }
    this->wSignature = 0x66;
}

// FUNCTION: LOCO 0x4428c0 (??_GCarNetState scalar deleting dtor -- compiler-generated)
// PARTIAL, DIFF(22) at 30 B against 32. Structurally identical except for ONE thing: the original
// INLINES the dtor body (`mov dword ptr [esi],0x478264`) into the thunk, where ours emits
// `call ??1CarNetState` to the out-of-line body below.
//
// ⚠ PRICED AND WITHHELD (v546) -- do NOT re-run. The obvious fix, defining the dtor inline as
// `virtual ~CarNetState() {}` in src/CarNetState.h, DOES make this exact at 32 B, but it is a net
// -177 B repo-wide: cl then inlines the same body at the member-destruction site inside
// CarNetObj::~CarNetObj (0x40d680), which costs that function its whole 206 B exact (the inlined
// store also dissolves the `xor ebx,ebx` zero-register the original spends on four more
// operands), and GameNet.cpp loses a further 8 B. Measured from a clean baseline, both directions.
//
// The image itself settles which model is right, and it is the CURRENT one: 0x40d680+0xa4 really
// does `lea ecx,[esi+0x88]; call 0x442a00`, and Ghidra's xrefs to 0x442a00 list ~CarNetObj,
// GameNet_HandleTrainStateSync, FUN_0043b230 and three EH unwind funclets. An inline dtor cannot
// produce an out-of-line call at any of them. So the dtor is out-of-line, exactly as written
// below, and this 2-byte thunk difference is cl declining to inline a body it inlined in 1998 --
// an intrinsic residual, not a source-shape one.

// FUNCTION: LOCO 0x442a00
// Bare vtable-dtor stub (mov [ecx],&vtbl 0x478264; ret) -- CarNetState has no owned
// sub-objects to release. Moved out of src/phase2_probe2.cpp 2026-07-22 (v322, where it
// lived as the probe-local VtblStub0x442a00; docs/subsystems.md's CarNetState entry had
// already identified the owner).
CarNetState::~CarNetState() {}  // TODO: sync

// FUNCTION: LOCO 0x442a70
// Serialize this card to "<pszDir>\<seqid>.crd", where <seqid> is a freshly minted post-sequence
// id string (which also bumps and re-saves the local player's profile). The numeric form of that
// same id is stamped into nPostSeqId FIRST, so the id written into the file agrees with the file's
// own name.
//
// The write is a raw dump of everything after the vptr -- signature word through the trailing
// dword -- which is why wSignature has to be the first real member and why LoadCardFile below can
// read the record straight back over the live object. The size is confirmed exact by
// field-by-field sizing: 2+2+4+4+21+21+2+4+1+1+1+80+1+1+1+0x300+2+4 = 0x398.
//
// CORRECTED 2026-07-17: this was wrongly address-boxed under CarNetStateAlt (a same-address-range
// gap-fill false positive -- it sits between CarNetStateAlt's own factory/ctor at 0x442a10 and
// 0x442ec0). The WriteFile size proves it is really a CarNetState method: 0x398 bytes from +4
// spans exactly to +0x39c, CarNetState's own size, not CarNetStateAlt's real 0x390.
// FUN_00401850 (PostBagFileCache's record-insert helper, src/PostBag.h) independently
// corroborates it: that reads the SAME object's +0xc (dword) and +0x25 (string) -- exactly
// CarNetState's own nPostSeqId and nameB offsets.
//
// MOVED here from src/EditCardWnd.cpp this session: it had been transcribed there (and matched
// there), but 0x442a70 sits inside the contiguous 0x442850..0x442fff CarNetState .obj run while
// EditCardWnd.cpp's own PostBag cluster starts at 0x444c70. Two TUs were carrying the same
// definition, so progress.py only ever counted it once.
unsigned char CarNetState::SaveCardFile(char *pszDir)
{
    DWORD dwWritten = 0;
    char szPath[0x504] = "";
    char *pszSeqId = AllocNextPostSeqIdString(g_pLocalPlayerIdentity);
    this->nPostSeqId = atoi(pszSeqId);
    wsprintfA(szPath, "%s\\%s.crd", pszDir, pszSeqId);
    HANDLE hFile = CreateFileA(szPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                               FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }
    if (!WriteFile(hFile, &this->wSignature, sizeof(CarNetState) - 4, &dwWritten, NULL)) {
        CloseHandle(hFile);
        return 0;
    }
    CloseHandle(hFile);
    return 1;
}

// FUNCTION: LOCO 0x442b50
// Load counterpart to SaveCardFile: read the whole post-vptr record back over this object, then
// validate the .crd signature word.
//
// ⚠ The signature check writes BEFORE it fails: a file whose leading word isn't 0x66 has already
// been splattered over the live object by the ReadFile above, and all this does is put the
// signature back so the half-loaded wreck at least looks like a card again. The caller
// (CarNetState_CreateFromFile, 0x444c70 in src/EditCardWnd.cpp) deletes the object on the 0
// answer -- self-destructing through its own vtbl slot 0 -- so nothing observes the damage. The
// repair is still a `sic:`-grade tell that the author knew the read was destructive.
//
// Same mis-boxing correction and same MOVED-from-src/EditCardWnd.cpp note as SaveCardFile above.
unsigned char CarNetState::LoadCardFile(const char *pszPath)
{
    DWORD dwRead = 0;
    if (pszPath == NULL) {
        return 0;
    }
    HANDLE hFile = CreateFileA(pszPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }
    if (!ReadFile(hFile, &this->wSignature, sizeof(CarNetState) - 4, &dwRead, NULL)) {
        CloseHandle(hFile);
        return 0;
    }
    if (this->wSignature != 0x66) {
        this->wSignature = 0x66;  // sic: the bad record is already in the object; see above
        CloseHandle(hFile);
        return 0;
    }
    CloseHandle(hFile);
    return 1;
}

// FUNCTION: LOCO 0x442bf0
// Pick the card's corner stamp. nKind 0 clears the stamp entirely; 1, 2 and 3 each select a
// variant, and 3 additionally pins the slot. For kinds 1 and 2 the caller may pass nSlot == -1 to
// mean "choose for me": kind 1 flips a coin between slots 1 and 2, kind 2 always lands on 2.
// Any other nSlot is taken literally, which is the shared tail both of those cases fall out to.
void CarNetState::AssignStampSlotVariantMaybe(int nKind, char nSlot)
{
    switch (nKind) {
    case 0:
        this->byStampSlotA = 0;
        this->byStampVariantA = 0;
        return;
    case 1:
        this->byStampVariantA = 1;
        if (nSlot == -1) {
            this->byStampSlotA = (unsigned char)(rand() % 2 + 1);
            return;
        }
        break;
    case 2:
        this->byStampVariantA = 2;
        if (nSlot == -1) {
            this->byStampSlotA = 2;
            return;
        }
        break;
    case 3:
        this->byStampVariantA = 2;
        this->byStampSlotA = 1;
        return;
    default:
        return;
    }
    this->byStampSlotA = nSlot;
}

// FUNCTION: LOCO 0x442c90
// Place one clip-art decal on this card. The 128 slots are kept COMPACTED (every occupied slot
// precedes every free one -- see CompactDecals below), so "find the first free slot" is a
// forward scan for the first zero placementSeq, and a full array means the oldest decal is
// evicted: slot 0 is freed, the array is compacted down over it, and the new decal lands in the
// now-free slot 127. The return value distinguishes the two: 1 = placed into genuinely spare
// capacity, 0 = something had to be dropped to make room. The caller (EditCardWnd's
// OnLButtonDown, edit mode 2) uses the 0 answer to force a full preview repaint instead of the
// cheap incremental "draw just the last placed item" path -- because after an eviction every
// slot has shifted and the incremental draw would be wrong.
//
// x/y arrive in card-art-relative pixels and are stored HALVED (xHalf/yHalf), the same *2
// convention the reader side applies; the halving is a signed `/ 2`, so a click above/left of
// the art origin rounds toward zero rather than down.
//
// ⚠ The first two parameters are named per the call sites in src/EditCardWnd.cpp /
// src/GameNet.cpp, but the packing here is `(arg1 << 3) | (arg2 - 1)` while DecalSlot's own
// writeup (docs/subsystems.md, from the reader side) decodes packedKind as `kind = val >> 3`
// and `subkind = (val & 7) + 1`. One of the two naming sets is inverted -- reader and writer
// agree on the BITS, only the labels disagree. Left alone here rather than renaming the two
// call sites' locals mid-transcription; TODO for a naming session.
//
// EFFECTIVE MATCH (DIFF(7), insns 55/55, at the original's exact 159 B). The whole residual is
// the last two instructions before the packedKind store: the original loads nKind into cl and
// nSubkind into dl (`dec cl; shl dl,3`), this compiles the two the other way round
// (`shl cl,3; dec dl`). Pure symmetric-register-swap -- and it is INDUCED, not intrinsic to the
// source: this function matched EXACT until CardNameA/CardNameB landed in src/CarNetState.h and
// rotated the TU. Three spellings probed, all byte-identical to each other, so cl canonicalizes
// them before allocation: operand order (`(nKind-1) | (nSubkind<<3)`), and hoisting `nKind-1`
// into its own local first. Worth 159 B if the swap class ever cracks. See docs/PARKED.md.
unsigned char CarNetState::AddDecal(char nSubkind, char nKind, unsigned char nSlotPlusOne, int x,
                                    int y, unsigned char byWidth, unsigned char byHeight)
{
    int nSlot = -1;
    unsigned char bHadRoom = 1;
    for (int i = 0; i < 128; i++) {
        if (this->decalSlots[i].placementSeq == 0) {
            nSlot = i;
            break;
        }
    }
    if (nSlot < 0) {
        this->decalSlots[0].placementSeq = 0;
        this->CompactDecals();
        nSlot = 127;
        bHadRoom = 0;
    }

    this->decalSlots[nSlot].placementSeq = nSlotPlusOne;
    this->decalSlots[nSlot].xHalf = (unsigned char)(x / 2);
    this->decalSlots[nSlot].width = byWidth;
    this->decalSlots[nSlot].yHalf = (unsigned char)(y / 2);
    this->decalSlots[nSlot].height = byHeight;
    this->decalSlots[nSlot].packedKind = (unsigned char)((nSubkind << 3) | (nKind - 1));
    return bHadRoom;
}

// FUNCTION: LOCO 0x442d30
// Click-to-remove hit test: walk the decals BACK to front (127 down to 0, i.e. newest first, so
// the decal drawn on top is the one you pick up) and delete the first whose hit rect contains
// the point. The rect is reconstituted from the packed slot -- centre at (xHalf * 2, yHalf * 2),
// size (width, height) -- and then clamped so it never starts left of or above the card art,
// which means a decal placed half off the top-left edge stays clickable over its visible part.
//
// x/y are card-art-relative, the same space AddDecal stored. Answers 1 when a decal was removed
// (the array is re-compacted first, so the caller can redraw straight away), 0 when the click
// missed every decal.
//
// ⭐ `rc` and `pt` are declared at FUNCTION scope, not inside the loop, and that is load-bearing
// -- with them declared inside the `if` this compiles to DIFF(135) at 176 B (70 insns vs the
// original's 76). VC5 hoists the whole rect out to the same stack slots either way, but the
// declaration scope decides how conservative it is about the two `&rc`-escaping stores: at
// function scope it keeps `rc.left = xHalf * 2;` and `rc.left -= width / 2;` as two real stores
// and RE-READS `width` (and `height`) for the following `rc.right`/`rc.bottom`, exactly as the
// original does; at inner scope it proves the byte loads cannot alias the not-yet-escaped local,
// CSEs each of them to one load and collapses each pair of stores into one -- 6 instructions
// short. See docs/CODEGEN.md.
unsigned char CarNetState::RemoveDecalAtPoint(LONG x, LONG y)
{
    RECT rc;
    POINT pt;
    for (int i = 127; i >= 0; i--) {
        if (this->decalSlots[i].placementSeq != 0) {
            rc.left = this->decalSlots[i].xHalf * 2;
            rc.left -= this->decalSlots[i].width / 2;
            rc.right = rc.left + this->decalSlots[i].width;
            rc.top = this->decalSlots[i].yHalf * 2;
            rc.top -= this->decalSlots[i].height / 2;
            rc.bottom = rc.top + this->decalSlots[i].height;
            if (rc.left < 0) {
                rc.left = 0;
            }
            if (rc.top < 0) {
                rc.top = 0;
            }
            pt.x = x;
            pt.y = y;
            if (PtInRect(&rc, pt)) {
                this->decalSlots[i].placementSeq = 0;
                this->CompactDecals();
                return 1;
            }
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x442e00
// Close the gaps in the decal array, restoring the "every occupied slot precedes every free one"
// invariant that AddDecal's first-free-slot scan and the reader side's
// PostBagCacheBundle::DrawLastPlacedItem backward scan both depend on.
//
// Selection-style rather than a stable shift: for each free slot, find the NEXT occupied slot
// after it and move that one down, leaving its old home free. Order is preserved because the
// donor is always the first occupied slot to the right. Stops as soon as a free slot has no
// occupied slot after it -- at that point the array is already compact.
void CarNetState::CompactDecals()
{
    for (int i = 0; i < 128; i++) {
        if (this->decalSlots[i].placementSeq == 0) {
            int j;
            for (j = i + 1; j < 128; j++) {
                if (this->decalSlots[j].placementSeq != 0) {
                    this->decalSlots[i].placementSeq = this->decalSlots[j].placementSeq;
                    this->decalSlots[i].xHalf = this->decalSlots[j].xHalf;
                    this->decalSlots[i].width = this->decalSlots[j].width;
                    this->decalSlots[i].yHalf = this->decalSlots[j].yHalf;
                    this->decalSlots[i].height = this->decalSlots[j].height;
                    this->decalSlots[i].packedKind = this->decalSlots[j].packedKind;
                    this->decalSlots[j].placementSeq = 0;
                    break;
                }
            }
            if (j == 128) {
                return;
            }
        }
    }
}

// FUNCTION: LOCO 0x442ea0 (??_GCarNetStateAlt scalar deleting dtor -- compiler-generated from the
// INLINE `virtual ~CarNetStateAlt() {}` in src/CarNetState.h; see that declaration for why this
// class's dtor is inline where its twin CarNetState's is not)

// FUNCTION: LOCO 0x442ec0
// Snapshot this card into its wire form -- the send half of the round-trip, and the exact mirror
// of CarNetState::CarNetState(CarNetStateAlt*) near the top of this file. Unlike the receive
// side, this one is unconditional: it always declares all 128 decal slots and copies all 128,
// occupied or not. wSignature, Unk0x06 and the receive side's own nDecalCount honouring are the
// only asymmetries between the two directions.
CarNetStateAlt::CarNetStateAlt(CarNetState *pSrcState)
{
    memcpy(this->nameA, pSrcState->nameA, sizeof(this->nameA));
    memcpy(this->nameB, pSrcState->nameB, sizeof(this->nameB));
    strcpy(this->szDescription, pSrcState->szDescription);
    this->byStampSlotB = pSrcState->byStampSlotB;
    this->byStampSlotA = pSrcState->byStampSlotA;
    this->byStampVariantA = pSrcState->byStampVariantA;
    this->wAttachmentId = pSrcState->wAttachmentId;
    this->bAttachmentSoundPlayedMaybe = pSrcState->bAttachmentSoundPlayedMaybe;
    this->byIdentityColorR = pSrcState->byIdentityColorR;
    this->byIdentityColorG = pSrcState->byIdentityColorG;
    this->byIdentityColorB = pSrcState->byIdentityColorB;
    this->nDecalCount = 128;
    for (int i = 0; i < 128; i++) {
        this->decalSlots[i] = pSrcState->decalSlots[i];
    }
}

// FUNCTION: LOCO 0x442a10
// Heap-allocate a wire snapshot of pSrcState. Plain `new` -- the /GX alloc-protection frame and
// the null-guarded ctor dispatch are the compiler's, not the author's.
CarNetStateAlt *__fastcall CarNetStateAlt_CreateFromState(CarNetState *pSrcState)
{
    return new CarNetStateAlt(pSrcState);
}

// FUNCTION: LOCO 0x442fa0
// Heap-allocate a full card from a wire snapshot. The `void *` on the parameter is the seam
// between this class pair and GameNet.h's raw RosterCarStateBlockMaybe view of the very same
// bytes; see the TODO on CarNetStateAlt in src/CarNetState.h.
CarNetState *__fastcall CarNetState_CreateFromWireRecord(void *pWireRecord)
{
    return new CarNetState((CarNetStateAlt *)pWireRecord);
}
