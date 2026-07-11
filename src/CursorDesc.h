// CursorDesc -- generic small-bitmap-resource holder; cursors are just its
// best-understood use (0x168 = 360 bytes; ctor 0x424af0, vtable 0x477c18). Ghidra's own
// struct DB already has the FULL field layout (see docs/subsystems.md's `CursorDesc`
// entry), but no consumer transcribed in src/ yet needs an individual field, so this header
// only models the virtual dispatch shape -- matching WindowBase.h/CarNetState.h's "only
// give real bodies to what's needed" precedent.
//
// The vtable is confirmed (xref-verified, not just a raw dword walk -- see CLAUDE.md's
// vtable-packing lesson) to be exactly 5 slots, ending in a zero-padding dword immediately
// before WindowBase's OWN unrelated vtable begins at 0x477c30 (docs/subsystems.md's earlier
// "16 slots, ends at +0x3c" claim for this vtable was a stale un-xref-confirmed dump that
// walked into WindowBase's table -- corrected 2026-07-16). Declaration order below must match
// this real slot order so any future real body slots into the right vtable position:
//   slot 0 (+0x0)  ~CursorDesc (scalar deleting dtor)
//   slot 1 (+0x4)  GetOrLoadFrameBitmap
//   slot 2 (+0x8)  ReleaseRef  -- the one WindowBase::~WindowBase calls
//   slot 3 (+0xc)  ParseTokenField
//   slot 4 (+0x10) Load
#pragma once

// Obj0x4779e0 embeds two of these BY VALUE at +0x534/+0x548, so the complete type is required
// here -- a forward declaration will not do. Adding this include was the v331-measured hazard
// that kept those embeds modeled as raw longs for 200-odd sessions; re-measured 2026-07-31 and
// it now costs nothing (see the +0x534 block's own note below).
#include "TimeOfDayMaybe.h"

class istream; // <fstream.h>/<strstrea.h>, forward-declared to avoid pulling iostream.lib's
                // headers into every consumer of this file (same precedent as DSoundChannel.h)
struct LocoBitmap; // src/LocoBitmap.h -- `struct`, matching its real definition (a `class`
                   // forward declaration warns C4099 in any TU that sees both).

// One entry of the raw per-frame/animation-set table paFrameEntries points at (0x18=24-byte
// stride, confirmed by 2 independent consumers -- TutorialWnd::FUN_00450520/0x437540 index it
// dynamically by Unk0x1e, BuildToolCursorWnd's StartSlotDAnimation/AdvanceSlotDAnimation index several FIXED
// entries -- see docs/subsystems.md's BuildToolCursorWnd entry for the full derivation of
// which entry index means what). Full 24-byte layout resolved 2026-07-18 while transcribing
// CursorDesc::ParseTokenField (the "cursor_frame_set"/"cursor/default_frame_set"
// token's own per-entry parse loop writes every byte of the stride) -- corrects an earlier
// session's `pad0xc[0x14-0xc]` (only 8 bytes, undersized -- the real tail is 12 bytes,
// +0xc..+0x17) which silently left this struct only 20 bytes, not the confirmed 24-byte stride.
struct CursorAnimFrameEntry {
    unsigned short nStartFrame; // +0x0 -- a frame index into the bitmap this animation set uses
    unsigned short nEndFrame; // +0x2 -- read as (count - 1) at 2 of the 3 entries actually
                                      // tested this way; purpose at the other entries unconfirmed
    unsigned short wFrameDivisor;       // +0x4 -- ParseTokenField defaults this to 1 if the ini
                                       // token parses as 0
    unsigned char pad0x6[2];          // +0x6 .. +0x7, unmodeled (never written by ParseTokenField)
    int nCooldownTicks;                 // +0x8 -- tested `>= 0` by TilePlacedObj's own ctor
                                      // to gate seeding a random anim-cooldown tick
                                      // (src/TilePlacedObj.cpp)
    short nBounceSoundId;      // +0xc -- self-index sentinel: ParseTokenField resets this to
                                // -1 when (nStartFrame==nEndFrame) and it equals this
                                // entry's own array index -- purpose (a "chained frame set" index?)
                                // not yet confirmed
    short nSoundBankEntryId;          // +0xe
    int nSoundRetriggerDelay;           // +0x10
    unsigned short nSoundCategory; // +0x14
    unsigned char Unk0x16Maybe;  // +0x16 -- low byte of a 2nd scratch ushort ini-token read
    unsigned char bDoubleSpeedFlag;  // +0x17 -- low byte of a 1st scratch ushort ini-token read
};

class CursorDesc {
public:
    // 0x424b40 -- the `??_G` scalar-deleting dtor with the body INLINED into it (the original has
    // no separate `??1CursorDesc@@UAE@XZ` COMDAT at all; contrast Obj0x478118/CarKindDesc, whose
    // separate ??1 + ??_G pair is the tell for an out-of-line body). TRANSCRIBED AND VERIFIED
    // 2026-07-31, then REVERTED -- the body below is byte-EXACT (92 B, 0 diffs, first compile),
    // but it cannot be defined here at an acceptable price. Definition PARKED, not unknown:
    //
    //     virtual ~CursorDesc() {
    //         bLoadOkFlag = 0;
    //         if (pOwnedObjA != NULL)     { delete pOwnedObjA;     pOwnedObjA = NULL; }
    //         if (paFrameEntries != NULL) { delete paFrameEntries; paFrameEntries = NULL; }
    //         if (pShadowBitmap != NULL)  { delete pShadowBitmap;  pShadowBitmap = NULL; }
    //     }
    //
    // Why it is parked: the two `delete`s dispatch through LocoBitmap's vtable (slot 0, arg 1),
    // so LocoBitmap must be COMPLETE in this header -- against the forward declaration above cl
    // silently emits a plain `operator delete` instead. `#include "LocoBitmap.h"` here is MEASURED
    // at -1956 B (Obj0x4779e0.cpp -1940, NameAnchorMaybe.cpp -456, PeerTrainNode.cpp +440), and
    // the dtor body on top of it a further -836 B, for -2792 B against a +92 B prize. Only ONE
    // consumer of this header (CreditsWnd.h) already includes LocoBitmap.h ahead of it, so the
    // usual "a guarded include is byte-neutral by construction" argument does not apply.
    // Deferring BOTH the include and the body to the very bottom of this header (an out-of-line
    // `inline CursorDesc::~CursorDesc()`) was measured too and is byte-for-byte the SAME -2792 B:
    // the cost is LocoBitmap.h's declaration content, not its position. See docs/PARKED.md.
    virtual ~CursorDesc();
    // 0x424af0 -- the base ctor of every descriptor family. Declared here, and Load below given
    // its real 2-arg signature, because this class is the KEYSTONE of the descriptor pipeline:
    // UIResources::TileKind_CreateDescriptor builds all 15361 tile-kind descriptors through it,
    // and while it lived on a TU-local ctor shim every one of those `new`s called a generated
    // do-nothing stub. The visible symptom was a null-pointer crash in SplashWnd::EnsureArtLoaded
    // (+0x26): kinds 0x403..0x40f are category 1, TileKind_GetOrLoadDescriptor handed back the
    // -1 poison, and the splash dereferenced it. See CODEGEN #161.
    //
    // The third argument is a genuine DEAD parameter -- the `ret 0xc` proves the slot is real
    // but the body never reads it. The factory passes 1 for exactly the odd-id fallbacks inside
    // the paired even/odd categories and 0 everywhere else.
    CursorDesc(int kindId, char *pszDefinition, int bOddKindMaybe);
    // 0x425670 -- real signature confirmed from EditCardWnd's own call site (0,0); not itself
    // transcribed yet. Return type confirmed LocoBitmap* (not just a plausible cast target) by
    // src/SplashWnd.cpp's own caller, which reads ->width/->height off the result and passes it
    // straight into LocoBitmap::BlitOntoBitmap as `this` -- a virtual's return type is a
    // compile-time-only annotation (still a 4-byte EAX pointer either way), so this is safe to
    // narrow from void* without affecting any already-matched caller's generated code.
    // 0x425670, src/CursorDesc.cpp -- the lazy bitmap realizer (see the body's own writeup).
    virtual LocoBitmap *GetOrLoadFrameBitmap(int nWidth, int nHeight);
    virtual void ReleaseRef();
    virtual unsigned char ParseTokenField(istream *pStream); // 0x424e00, src/CursorDesc.cpp
    // 0x424bf0 (Ghidra: CursorDesc::LoadMaybe) -- vtable slot 0x10, ground-truthed against the
    // vtable dword at 0x477c28. ⚠ The real signature is `Load(unsigned int kindId, char
    // *pszDefinition)`: the ctor at 0x424af0 tail-calls it with two callee-cleaned pushes, and the
    // body sprintf's "%s%s.bmp" from pszDefinition into szBmpPath at +0x48. TRANSCRIBED EXACT
    // 2026-07-29 (v498) in src/CursorDesc.cpp through a TU-local methods-only view
    // (`CursorDescLoadView0x424bf0`) -- the declaration here still deliberately stays MIS-DECLARED
    // as no-arg, and CursorDesc's own ctor is still deliberately NOT declared, because of this
    // header's position-sensitivity (see src/Obj0x4779e0.cpp's note): MEASURED 2026-07-27, either
    // change on its own -- widening this declaration, or adding `CursorDesc(int, char *, int)` --
    // rotates src/Obj0x4779e0.cpp and turns ParseEntryExitMaybe (0x41f0c0) from EXACT into
    // DIFF(19) at identical length, -489 B. BOTH were done anyway (see the ctor above): the price
    // is real but it is what a descriptor pipeline that actually runs costs, and the ctor calls
    // Load with two arguments, so the mis-declared arity stopped being free the moment the ctor
    // became real.
    virtual void Load(unsigned int kindId, char *pszDefinition);

    int resourceId;             // +4 -- command/resource id, e.g. tested against 0x2c02.. ranges
    unsigned char categoryByte; // +8 -- BigObj-family descriptor category (3 = track family,
                                      // 0xc = global track-link, etc.; see BigObj below)
    unsigned char pad0x9[3];    // +9 .. +0xb, unmodeled
    int nShadowId;         // +0xc -- ParseTokenField's "ShadowId" ini token (a numeric
                                 // resource id, read via istream::operator>>(long&) -- corrects
                                 // an earlier session's "pLoadedBitmapMaybe" pointer-shaped guess
                                 // for this same offset, refuted by the plain-integer read shape)
    // +0x10 -- an owned sub-object; non-null == "actually loaded" (tested by
    // ResourceRef::ReleaseRealized). Retyped 2026-07-20 from void*: MenuNodeObj0x477568::Draw's
    // FUN_0042b960/BlitOntoBitmap call dereferences this pointer's own +0x1c field as an
    // IDirectDrawSurface* -- exactly LocoBitmap::pSurface's offset -- confirming it's really a
    // LocoBitmap* (the resource's realized/loaded canvas bitmap).
    LocoBitmap *pOwnedObjA;
    unsigned short nativeWidth;  // +0x14 -- the resource's own natural width, read by
    unsigned short nativeHeight; // +0x16 -- EditCardWnd::RefreshClientClipRect to lay
                                       // out each icon/button's own rect (src/EditCardWnd.cpp)
    // +0x18 -- Ghidra's own pre-existing name; zeroed by the ctor (0x424af0) and forced to 1
    // by the descriptor factory (UIResources::TileKind_CreateDescriptor, 0x446840) for every
    // category-5 kind and for the category-14 kinds above 0x3801 -- i.e. the kinds that are
    // NOT required to have actually loaded before the factory accepts them.
    unsigned char bReadyFlagMaybe;
    unsigned char pad0x19;      // +0x19, unmodeled
    unsigned short nFrameSetCount; // +0x1a -- ParseTokenField's "number_of_frame_sets" ini
                                  // token; the paFrameEntries array's own element count
    unsigned short wDefaultFrameSetIndex; // +0x1c -- ParseTokenField's "cursor_frame_set"/
                                  // "cursor/default_frame_set" ini token 1st value (read via
                                  // istream::operator>>(short&) -- signed at the read site even
                                  // though stored unsigned, same convention as wActiveFrameSetIndex below)
    unsigned short wActiveFrameSetIndex; // +0x1e -- set from the "cursor_frame_set"/
                                 // "cursor/default_frame_set" ini token's 2nd value
                                 // (ParseTokenField); TutorialWnd::FUN_00450520 reads it as
                                 // a *24 (sizeof one raw-frame-table entry) stride into
                                 // paFrameEntries below; BuildToolCursorWnd's own
                                 // StartSlotDAnimation/AdvanceSlotDAnimation index several FIXED entries instead,
                                 // not this selector -- see this header's own top comment
    CursorAnimFrameEntry *paFrameEntries; // +0x20 -- raw per-frame/anim-set table, indexed by
                                            // wActiveFrameSetIndex or a fixed entry number
                                            // depending on caller
    LocoBitmap *pShadowBitmap; // +0x24 -- a 2nd owned sub-object (the "shadow" bitmap, loaded
                                   // lazily by ParseTokenField's own tail block)
    unsigned short wShadowFrameWidth;  // +0x28 -- set from the loaded shadow bitmap's own
                                   // (width / nButtonFrameCount), once it loads successfully
    unsigned short wShadowBitmapHeight;  // +0x2a -- set from the loaded shadow bitmap's own height
                                   // (low word only -- read via a `word*`, not the full `int`)
    unsigned short nButtonFrameCount;  // +0x2c -- ParseTokenField's "button" ini token 3rd value,
                                   // defaults to 3 if read as 0 (plausibly a 3-state button frame
                                   // count: normal/hover/pressed)
    short field_0x2eMaybe;        // +0x2e -- "button" token 1st value
    short field_0x30Maybe;        // +0x30 -- "button" token 2nd value
    short hotspotX; // +0x32 -- signed cursor hotspot/anchor offset (movsx-read), subtracted
    short hotspotY; // +0x34 -- from the mouse position by PopupWndBase::RedrawSoftwareCursor
    unsigned char pad0x36[2];    // +0x36 .. +0x37, unmodeled
    int nShadowOffsetX;     // +0x38 -- "ShadowOffset" ini token 1st value
    int nShadowOffsetY;     // +0x3c -- "ShadowOffset" ini token 2nd value
    // +0x40/+0x44 -- the "must/cant_have" ini token's two values, and they are TILEKIND IDS, not
    // bit masks (renamed from dwMustHaveMask/dwCantHaveMask 2026-07-26; audited per CLAUDE.md,
    // all 817 COMDATs byte-identical). DecorObjMgrMaybe::SpawnActorForKindMaybe (0x4349d0) is
    // what pins it: it resolves BOTH through TileKind_GetOrLoadDescriptor and then refuses to
    // spawn unless the must-have kind is currently alive somewhere in the world
    // (nLiveInstanceCountMaybe != 0, skipped entirely when the id is -1 "none") and the cant-have
    // kind is NOT. src/WorldActionCursor.cpp's own reading of the same pair agrees.
    int nMustHaveKindId;            // +0x40
    int nCantHaveKindId;            // +0x44
    // +0x48 -- this descriptor's own resolved bitmap path. Role pinned 2026-07-27 by reading
    // CursorDesc::Load (0x424bf0): it is the destination of
    // `sprintf(this + 0x48, "%s%s.bmp", g_pInstallPathPrefix, pszDefinition)` (raw
    // `lea ecx,[esi+0x48]` at 0x424cbc), built alongside a sibling ".dat" path that Load keeps
    // as a plain CHAR[264] LOCAL. That corrects this field's earlier name
    // (`szShadowBitmapBaseName`) and its earlier "shadow-bitmap base name fragment" reading --
    // ParseTokenField's shadow load reads this same already-built path, it does not author a
    // separate fragment here. src/NetSessionEventQueue.cpp reads it back to recover the
    // backdrop's install-relative name (its old TU-local CursorDescPathPartial view, retired
    // 2026-07-27, called it szBmpPath at this same offset).
    // ⚠ The SIZE is still a placeholder, and the sibling local's 264 CANNOT be it: szCategoryName
    // is pinned at +0x14d by Load's own `mov BYTE PTR [esi+0x14d],bl` (0x424c8f), so this buffer
    // can be at most 0x105 = 261 bytes. MAX_PATH (260) is the natural author choice and would
    // leave exactly one unexplained byte at +0x14c; not yet confirmed either way, so the
    // conservative 0x20 placeholder stays and the remainder is carried as pad below.
    char szBmpPath[0x20];
    unsigned char pad0x68[0xe5]; // +0x68 .. +0x14c, unmodeled (Ghidra's own DB has further
                                   // named-but-unread fields in this range -- see
                                   // docs/subsystems.md)
    char szCategoryName[10]; // +0x14d -- "has a printable per-instance category name" gate;
                                 // TilePlacedObj's own ctor only calls
                                 // AnimDescRefObj0x477488::SetCategoryIfPrintable when
                                 // this is nonzero (src/TilePlacedObj.cpp)
    unsigned char byNulTerminatorGuard; // +0x157 -- explicit forced NUL-terminator safety byte,
                                      // immediately after szCategoryName, not general pad
    // +0x158 -- how many instances of this kind are currently alive. Identified 2026-07-25 from
    // TilePlacedObj::SpawnOwnedActorMaybe (0x458430), which refuses to spawn another owned actor
    // of this kind once this reaches bBitmapOccupancyCols (+0x16b) -- it tests exactly that pair
    // on both the category-7 and the category-8 path. Distinct from nMaxInstances (+0x15c),
    // which is an ini-declared limit rather than a live counter.
    // ⚠ That comparison reads a field whose identity is settled and is NOT a cap: +0x16b is the
    // `bitmap_occupancy` token's COLUMN COUNT (see the field's own note below). An earlier draft
    // of this header described +0x16b as "the per-kind cap", which read backwards from this one
    // consumer and contradicted the ~20 geometric uses repo-wide; corrected 2026-07-31 (v553).
    // The engine really does bound a live-instance count by a footprint width here. Whether that
    // is a designer convention (a wide kind may have proportionally more instances) or a slip
    // for nMaxInstances (+0x15c) is NOT recoverable from the code, so it is reproduced verbatim
    // and left unjudged -- no docs/engine-bugs.md row, because "surprising" is not "wrong".
    unsigned short nLiveInstanceCountMaybe; // +0x158
    unsigned char pad0x15a[2]; // +0x15a .. +0x15b, unmodeled
    unsigned int nMaxInstances; // +0x15c -- ParseTokenField's "MaxInstances" ini token,
                                 // init -1
    unsigned short nTotalFrameCount; // +0x160 -- default 1 (ParseTokenField token); read as a
                                // frame/scale-variant COUNT by PopupWndBase::RedrawSoftwareCursor to bound
                                // nCursorFrameIndex (wraps to 0 when >= this)
    unsigned char bLoadOkFlag; // +0x162 -- Ghidra's own pre-existing struct DB already had
                                      // this name/offset (this function never touches it)
    // +0x163 -- the "ButtonVisible" .dat token, verbatim (Obj0x4779e0::ParseTokenField reads one
    // ushort and stores its low byte here). Was the placeholder Unk0x163Maybe until v553; nothing
    // in src/ reads it back yet, so the token string is the whole of what is known -- but it is
    // the author's own word for the field, so the name is certain even though the use is not.
    unsigned char bButtonVisible; // +0x163
    unsigned int dwRenderFlags;  // +0x164 -- a full DWORD despite only ever being touched via
                                   // its 2nd byte (`or ah,4`/`or al,2` on a loaded EAX) -- bitfield:
                                   // 0x400 = "semi-transparent" ini token, 0x2 = "shadows" ini
                                   // token; confirmed against Ghidra's own pre-existing struct DB
                                   // (an earlier draft this session wrongly modeled this as a
                                   // 2-byte word + 2 bytes pad, which compiled a structurally
                                   // different byte-sized OR instead of the original's dword
                                   // read-modify-write)
};

// Adds the BigObj-family's own footprint/occupancy-mask fields (unmodeled, see
// docs/subsystems.md's BigObj entry) plus one field needed by TilePlacedObj's own ctor.
// Direct base of BigObj below -- kept as its own class (not folded into BigObj) matching the
// real Ghidra struct chain (BigObj = Obj0x4779e0 base + 0xc own bytes, see docs/subsystems.md's
// "namespace/struct unification" note).
// ⭐ NAMING PROVENANCE for most of this class (v553, 2026-07-31): these fields are not named
// from a behavioural guess -- they are named by the .dat ini TOKEN that writes them, read out of
// Obj0x4779e0::ParseTokenField (0x41e9f0) and ParsePhysicalOccupancyMaybe (0x41efa0) in
// src/Obj0x4779e0.cpp. A token string in the shipped binary IS the original author's own word
// for the field, which is the strongest naming evidence this project can obtain, so every field
// below whose token is cited carries a CERTAIN name and deliberately no `Maybe` hedge. The
// token's operand ORDER pins which field is which:
//   "physical_occupancy" -> >> xSteps >> ySteps >> layerCount, then an [x][y][layer] grid
//   "bitmap_occupancy"   -> >> cols   >> rows,                 then a  [col][row]  grid
//   "MaxEmployees" / "PossibleEmployees" / "PossibleMinifigs" / "RMBSeq" / "ClosedFS" /
//   "EEReplayDelay" / "ButtonVisible" (CursorDesc::bButtonVisible, +0x163) / "FreeToRoam"
//     -> one field each, verbatim.
// Fields still carrying `Maybe` below are the ones NO token names (they are computed, or written
// from another field), and their hedge is real -- do not strip it by analogy with their neighbours.
class Obj0x4779e0 : public CursorDesc {
public:
    // 0x41e570 -- src/Obj0x4779e0.cpp. Declared HERE, on the real class, rather than on the
    // TU-local layout model it used to live on. That model was a full second definition of this
    // class's field block (exactly the duplicate-struct hazard CLAUDE.md warns about) and it
    // made the ctor unreachable from src/UIResources.cpp, whose factory `new`s this tier: the
    // call bound to a generated do-nothing stub, 175 times per run in link/stub_calls.log. It is
    // declarable here only because the +0x534/+0x548 embeds above are now REAL TimeOfDayMaybe
    // sub-objects -- the ctor's whole shape is base-ctor then those two embed ctors. CODEGEN #161.
    Obj0x4779e0(unsigned int kindId, char *pszName);
    // 0x41e620 / 0x41e600 (??_GObj0x4779e0) -- src/Obj0x4779e0.cpp. Declared HERE for the same
    // reason as the ctor: BigObj's own ctor/dtor pair chains to it, and while it lived on a
    // TU-local layout model that chain named the wrong function (an implicit dtor here would
    // chain straight to ~CursorDesc -- byte-identical under relocation masking, and wrong).
    // The member-then-base teardown it generates -- free the 3 owned arrays, then ~TimeOfDayMaybe
    // on +0x548 and +0x534, then ~CursorDesc -- is only correct because those embeds are typed.
    virtual ~Obj0x4779e0();
    // 0x41e9f0 -- this tier's own vtable slot 3 (+0xc) ParseTokenField override, the extended
    // keyword-dispatch pass. A real override of CursorDesc's virtual, so it keeps the same slot
    // and call shape. src/BigObj.cpp's own Load calls it by name.
    virtual unsigned char ParseTokenField(istream *pStream);
    unsigned char ParsePhysicalOccupancyMaybe(istream *pStream); // 0x41efa0
    unsigned char ParseEntryExitMaybe(istream *pStream);         // 0x41f0c0 -- entry_exit handler
    // 0x41f2b0 -- shared InsertSeq/MobileSeq/TotalVisits keyword-record parser (reads the lead
    // ulong + count + count longs into a BigObjSeqRecordMaybe, allocating paValues). Never
    // touches `this` (Ghidra types it __stdcall, but every call site sets up ecx -- it was
    // written as a member).
    unsigned char ParseSeqRecordMaybe(istream *pStream, unsigned long *pRec);
    // 0x41e6e0 -- really this tier's vtable slot 4 (+0x10) `Load` override, declared NON-virtual
    // for the same reason src/Obj0x478118.h and src/CarKindDesc.h document for their own: the
    // first parameter is the kind id, which this tier models as `void *` where CursorDesc::Load
    // models it as `unsigned int`, so it cannot be spelled as an override without changing one
    // of the two. It is HERE rather than on the TU-local ParsePartial view because the ctor
    // above calls it, and a ctor on the real class cannot reach a derived view's member.
    void LoadMaybe(unsigned int kindIdUnused, char *pszName);
    // +0x168/+0x169/+0x16a -- the "physical_occupancy" token's three leading extents, in its own
    // operand order; they bound the [x][y][layer] walk of aFootprintOccupancyMask below in both
    // the parser and every reader. AnimEffectObj0x477a90's ctor separately copies +0x168 into its
    // own bUnk0x94Maybe when the descriptor's categoryByte == 8 (src/AnimEffectObj.cpp).
    unsigned char bFootprintXSteps; // +0x168
    unsigned char bFootprintYSteps; // +0x169
    unsigned char bFootprintLayerCount; // +0x16a
    // +0x16b/+0x16c -- the "bitmap_occupancy" token's two leading extents, in its own operand
    // order (`>> cols >> rows`), bounding the [col][row] grid it then fills. Settled: ~20 reads
    // repo-wide use them as tile-space column/row counts (WorldBoardMaybe's end-cell computation,
    // Obj0x4779e0's own hotspot derivation, NameAnchorMaybe/PeerTrainNode's edge tests).
    // ⚠ +0x16b has ONE consumer that reads it as an instance cap instead --
    // TilePlacedObj::SpawnOwnedActorMaybe, see nLiveInstanceCountMaybe (+0x158) above. That is a
    // surprising use of this field, not evidence about its identity.
    unsigned char bBitmapOccupancyCols; // +0x16b
    unsigned char bBitmapOccupancyRows; // +0x16c
    unsigned char bFootprintHotspotEncodedMaybe; // +0x16d
    // +0x16e .. +0x4a0 (819 bytes) -- the kind's tile-footprint occupancy mask, indexed
    // [tileX][tileY][layer]. The 13x9x7 shape (v407) is exact, not a guess: 13*9*7 == 819 to
    // the byte, ParsePhysicalOccupancyMaybe (0x41efa0) zeroes precisely that span, and both
    // readers walk it with a 63-byte row stride and a 7-byte cell stride --
    // Obj0x4779e0.cpp's parser (which used to spell the index `[x*63 + y*7 + layer]` by hand)
    // and PlacementCursorMaybe::RefreshFootprintHighlightMaybe's own highlight stamp. The
    // per-axis extents in use are bFootprintXSteps / bFootprintYSteps above.
    // (Before v407 this was carried as a flat byte array, which made src/PeerTrainNode.cpp's
    // reads of the NEIGHBOURING +0x168 table look like unaligned loads INTO this one --
    // see src/CarKindDesc.h. They were a different class's field all along.)
    unsigned char aFootprintOccupancyMask[13][9][7]; // +0x16e
    // +0x4a1 -- the SECOND, independent occupancy table: which plane-B slot (1-based, 0 = "no
    // slot here") this kind claims at each tile of its BITMAP footprint, as opposed to the
    // physical footprint above. Reshaped from a flat [117] to [13][9] in 2026-07-26 by
    // WorldBoardMaybe::PlaceObject (0x4550c0), whose own plane-B pass walks it with a 9-byte
    // column stride and a 1-byte row step -- i.e. [tileX][tileY], the same axis order as
    // aFootprintOccupancyMask, and 13*9 == 117 to the byte. Its per-axis extents are
    // bBitmapOccupancyCols / bBitmapOccupancyRows above.
    // The "slot" reading is confirmed, not inferred: src/WorldBoardMaybe.cpp:1207 does
    // `char slot = pDesc->aBitmapOccupancySlotGrid[x][y] - 1;` after gating on `!= 0`.
    unsigned char aBitmapOccupancySlotGrid[13][9]; // +0x4a1
    unsigned char bMaxEmployees;    // +0x516 -- "MaxEmployees" token, clamped to 5 by the parser
    unsigned char pad0x517;              // +0x517
    // +0x518 -- "PossibleEmployees" token: exactly 5 kind ids. The parser rejects an id whose
    // category is 7 and whose value is odd, storing -1 ("none") in its place.
    short aPossibleEmployees[5];
    unsigned char bSpawnVariance; // +0x522 -- gates TilePlacedObj's own random spawn-
                                        // offset seed byte: 0 = no variance, else
                                        // rand() % bSpawnVariance + 1 (src/TilePlacedObj.cpp)
    unsigned char pad0x523;              // +0x523
    // +0x524 -- "PossibleMinifigs" token, same 5-entry shape and same odd-category-7 rejection as
    // aPossibleEmployees; additionally, a kind whose 5 entries are ALL -1 has bSpawnVariance
    // forced back to 0 at the end of the parse.
    short aPossibleMinifigs[5];
    short wRMBSeq;                  // +0x52e -- "RMBSeq" token
    short wClosedFS;                // +0x530 -- "ClosedFS" token
    short wUnk0x532Maybe;                // +0x532 -- Load inits 0
    // +0x534/+0x548: two 0x14-byte "shifts" sub-objects -- REAL TimeOfDayMaybe embeds as of
    // 2026-07-31. They spent v331..v557 modeled as ten raw longs because pulling
    // TimeOfDayMaybe.h into this shared header was measured to rotate DPlaySessionMgr.cpp's /Og
    // TU state and break SelectGridCellFromPointMaybe's EXACT (v331 bisect). That price is GONE
    // -- re-measured 2026-07-31 after v557b had already rotated this header, and the typed
    // embeds now cost nothing (CODEGEN #162: the declaration dial is a threshold, not a
    // per-declaration tariff, and this header was already over it).
    //
    // Typing them is what lets Obj0x4779e0's ctor (0x41e570) be declared on THIS class instead
    // of on a TU-local layout model: the original's base-ctor -> embed-ctor -> embed-ctor init
    // chain, with its EH state ladder (base 0, open 1, close 2), is emitted only if the embeds
    // are real sub-objects. It also retires five raw `(TimeOfDayMaybe *)&pDesc->lShiftOpenVptr`
    // casts at the three call sites, which were reaching exactly this type the hard way.
    // Only the "shifts" sscanf writes them (m_8/m_4 of each); everything else reads the PAIR
    // through TimeOfDay_IsTimeInWindowMaybe.
    //
    // ⭐ All ten names resolved v553 (they were six `lUnk0x…` placeholders and four
    // `lShiftA/B0x…Maybe`). Two independent facts pin them:
    //   1. WHICH RECORD IS WHICH -- all three call sites pass +0x534 as `pOpen` and +0x548 as
    //      `pClose` (src/DecorActor.cpp x2, src/TilePlacedObj.cpp), and TilePlacedObj names the
    //      result `bOpen` and falls back to wClosedFS ("closed frame set") when it is false.
    //   2. WHICH FIELD IS WHICH -- TimeOfDayMaybe's proven layout (vptr, m_4=minute, m_8=hour,
    //      m_c=day-of-month, m_10=month) mapped onto each 0x14-byte embed.
    // ⭐ And the mapping is now DOUBLY witnessed: the "shifts" sscanf reads
    // `&+0x53c, &+0x538, &+0x550, &+0x54c` -- i.e. hour before minute, twice -- so the .dat line
    // is "openHour openMin closeHour closeMin". That is an INDEPENDENT corroboration of
    // TimeOfDayMaybe's m_8=hour/m_4=minute assignment, which until now rested solely on ee.ini's
    // [TimeEvents] sscanf ordering (v494). See src/TimeOfDayMaybe.h.
    TimeOfDayMaybe shiftOpen;  // +0x534
    TimeOfDayMaybe shiftClose; // +0x548
    // +0x55c/+0x590/+0x5c4: three 0x34-byte InsertSeq/MobileSeq/TotalVisits keyword records
    // (lead long, element count capped at 0x2d by ParseSeqRecordMaybe, owned long-per-element
    // array; tail unmodeled). Kept as raw scalar fields -- a named record STRUCT in this
    // shared header rotates DPlaySessionMgr.cpp's /Og TU state (same v331 bisect).
    unsigned long lInsertSeqHeadMaybe;   // +0x55c -- record head (1st ulong read by ParseSeqRecordMaybe)
    unsigned long ulInsertSeqCountMaybe; // +0x560 -- element count
    long *paInsertSeqValues;             // +0x564 -- owned array, freed + reparsed by ParseTokenField
    long lInsertSeqUnk0x568Maybe;        // +0x568 -- Load inits -1 (ParseSeqRecordMaybe's pRec[3])
    // +0x56c .. +0x58f, the InsertSeq record's SPAWN payload, modeled 2026-07-29 for
    // ScriptEventLoader::ProcessInsertSeqSpawnsMaybe (0x420000, the only reader): when the
    // perimeter precondition (0x456d90) holds, +0x568 names a kind to place at this object's
    // front-row tile (offset by +0x574/+0x578 when +0x570 == -1), +0x56c is the slot-7 arg
    // handed to the placed object afterwards, and +0x57c names a SECOND kind -- spawned as a
    // 'W'-code effect at (+0x588/+0x584-mode-adjusted, +0x58c) with mobility +0x580 when its
    // category is 0xe, else placed at the front-row tile plus those offsets. Scalar fields
    // only, per the v331 bisect (a named record STRUCT here rotates DPlaySessionMgr.cpp).
    short wInsertSeqUnk0x56cMaybe;       // +0x56c
    unsigned char pad0x56e[2];           // +0x56e .. +0x56f, unmodeled
    long lInsertSeqUnk0x570Maybe;        // +0x570 -- Load inits -1 (ParseSeqRecordMaybe's pRec[5])
    short wInsertSeqUnk0x574Maybe;       // +0x574 -- x placement offset
    unsigned char pad0x576[2];           // +0x576 .. +0x577, unmodeled
    short wInsertSeqUnk0x578Maybe;       // +0x578 -- y placement offset
    unsigned char pad0x57a[2];           // +0x57a .. +0x57b, unmodeled
    long lInsertSeqUnk0x57cMaybe;        // +0x57c -- Load inits -1
    short wInsertSeqUnk0x580Maybe;       // +0x580 -- mobility flag for the category-0xe spawn
    unsigned char pad0x582[2];           // +0x582 .. +0x583, unmodeled
    long lInsertSeqUnk0x584Maybe;        // +0x584 -- spawn-position mode ('S' 0x53 = scroll-
                                         //   relative, 'W' 0x57 = absolute, else rect-relative)
    long lInsertSeqUnk0x588Maybe;        // +0x588 -- x offset
    long lInsertSeqUnk0x58cMaybe;        // +0x58c -- y offset
    unsigned long lMobileSeqHeadMaybe;   // +0x590
    unsigned long ulMobileSeqCountMaybe; // +0x594
    long *paMobileSeqValues;             // +0x598
    long lMobileSeqUnk0x59cMaybe;        // +0x59c -- Load inits -1
    unsigned char pad0x5a0[4];           // +0x5a0 .. +0x5a3, unmodeled
    long lMobileSeqUnk0x5a4Maybe;        // +0x5a4 -- Load inits -1
    unsigned char pad0x5a8[8];           // +0x5a8 .. +0x5af, unmodeled
    long lMobileSeqUnk0x5b0Maybe;        // +0x5b0 -- Load inits -1
    unsigned char pad0x5b4[0x5c4 - 0x5b4]; // +0x5b4 .. +0x5c3, unmodeled
    unsigned long lTotalVisitsHeadMaybe; // +0x5c4
    unsigned long ulTotalVisitsCountMaybe; // +0x5c8
    long *paTotalVisitsValues;           // +0x5cc
    long lTotalVisitsUnk0x5d0Maybe;      // +0x5d0 -- Load inits -1
    unsigned char pad0x5d4[4];           // +0x5d4 .. +0x5d7, unmodeled
    long lTotalVisitsUnk0x5d8Maybe;      // +0x5d8 -- Load inits -1
    unsigned char pad0x5dc[8];           // +0x5dc .. +0x5e3, unmodeled
    long lTotalVisitsUnk0x5e4Maybe;      // +0x5e4 -- Load inits -1
    unsigned char pad0x5e8[0x5f8 - 0x5e8]; // +0x5e8 .. +0x5f7, unmodeled
    long lEEReplayDelay;            // +0x5f8 -- EEReplayDelay token
    // +0x5fc..+0x61b: the 4-edge entry/exit record written by the "entry_exit" keyword
    // handler (0x41f0c0) -- pairs of longs per edge, -1 = unset.
    long aEntryExitMaybe[8];             // +0x5fc
    // +0x61c .. +0x62b -- ONE RECT, not four independent longs: this object kind's local
    // footprint box. TilePlacedObj::GetFootprintRectMaybe (0x4583c0) passes it to IsRectEmpty
    // BY ADDRESS and then copies it out WHOLE (four offset-relative dword moves off one shared
    // base register -- the documented struct-assignment shape, not four field assignments)
    // before OffsetRect'ing it into world pixels. Ghidra carries it as a real RECT at +0x61c.
    // Kept as four longs HERE only because this header deliberately does not include
    // <windows.h> and so has no RECT to name; merging it needs that include first, which is a
    // wide-fan-out change to make on its own and measure on its own. The four names were
    // A/B/C/D until v553; the "FreeToRoam" token reads them in exactly left/top/right/bottom
    // order (src/Obj0x4779e0.cpp), which is what retired the letters and the `Maybe`.
    long lFreeToRoamLeft;              // +0x61c  rect.left
    long lFreeToRoamTop;               // +0x620  rect.top
    long lFreeToRoamRight;             // +0x624  rect.right
    long lFreeToRoamBottom;            // +0x628  rect.bottom
    unsigned char bCountedInReadyBigObjCount; // +0x62c -- gates NetSessionEventQueue's own
                                  // g_dwReadyBigObjCountMaybe increment/decrement on
                                  // create/remove (src/NetSessionEventQueue.cpp)
    unsigned char pad0x62d[0x630 - 0x62d]; // +0x62d .. +0x62f, unmodeled
};

// The interned per-tile-kind descriptor -- see docs/subsystems.md's BigObj entry for the full
// writeup (doubles as the BigObj-family world-object class's own base, per that entry's
// "namespace/struct unification" note). m_type0x63a is the coarse per-kind classifier tested by
// the 3 already-byte-matched IsType0x63aInSet* predicates (src/phase2_probe.cpp).
class BigObj : public Obj0x4779e0 { // idiom-exempt: canonical (see phase2_probe.cpp's own frozen probe-local copy)
public:
    // 0x44b190 -- src/BigObj.cpp. Declared HERE, on the real class: the descriptor factory in
    // src/UIResources.cpp `new`s this tier for 12 of its 14 switch arms, and while the ctor lived
    // on a TU-local view every one of those calls bound to a generated do-nothing stub (45 per
    // run in link/stub_calls.log). Reachable only because Obj0x4779e0's own ctor/dtor above are
    // now real -- a mem-init list may name only a DIRECT base. CODEGEN #161.
    BigObj(unsigned int kindId, char *pszDefinition);
    // 0x44b220 / 0x44b200 (??_GBigObj) -- src/BigObj.cpp. Frees pSocketTable, then the compiler
    // chains ~Obj0x4779e0 (0x41e620). It must be declared rather than left implicit: an implicit
    // one would chain to the wrong base dtor, which is byte-identical under relocation masking
    // and completely wrong.
    virtual ~BigObj();
    // 0x44b290 -- vtable slot 4 (+0x10), this tier's per-kind .dat loader. A REAL override of
    // CursorDesc::Load now that that slot carries its true 2-arg signature; it used to need a
    // TU-local view because the base modelled the slot as a no-arg `virtual void Load()`.
    virtual void Load(unsigned int kindId, char *pszDefinition);
    short *pSocketTable;     // +0x630 -- packed {short x; short y;} connector/socket offsets,
                                  // count1+batch2 entries, owned (deleted+reparsed by
                                  // ParseTokenField's BigObj override, 0x44b4f0)
    unsigned char pad0x634[2];    // +0x634 .. +0x635, unmodeled
    unsigned short wSocketCount;    // +0x636
    unsigned short wSocketCountExt; // +0x638
    unsigned char m_type0x63a;    // +0x63a

    // 0x44b4f0, src/CursorDesc.cpp -- BigObj's override (vtable 0x478358 slot +0xc): frees and
    // reparses pSocketTable from the .dat stream, then the type-keyword tail (m_type0x63a).
    unsigned char ParseTokenField(istream *pStream);
    unsigned char IsType0x63aInSet();     // 0x44bd30, extern -- {7,8,9,0xa}
    unsigned char IsType0x63aInSet1234(); // 0x44bd10, extern -- {1,2,3,4}
    unsigned char IsType0x63aInSetE();    // 0x44bd50, extern -- {0xe,0xf}
    unsigned char IsType0x63aInSet10();   // 0x44bd70, extern -- {0x10,0x11}
    unsigned char IsType0x63aInSet12();   // 0x44bd90, extern -- {0x12,0x13}
    // 0x44bdb0 -- the companion of IsType0x63aInSet1234 just above: for a kind whose
    // m_type0x63a IS one of {1,2,3,4}, tests whether the tile at (col,row) sits on THAT type's
    // own board edge (1 = west, 3 = north, 2 = east, 4 = south). Deliberately NOT declared
    // here: transcribed in src/CursorDesc.cpp through a TU-local view, for exactly the
    // position-sensitivity reason documented on Load above -- MEASURED 2026-07-31, declaring it
    // on this class costs -2096 B (Obj0x4779e0.cpp's 0x41f0c0 -489 B, the same rotation Load's
    // note records, plus WorldBoardMaybe.cpp -951, RoadVehicleActor.cpp -504 and
    // PlacedObjRegistryMaybe.cpp -152). Hoist it here if that class ever cracks.
    // 0x41f430, extern -- "a kind that is a PLAIN RUN of track/path rather than something the
    // navigation graph should give a node of its own": a track-family piece (categoryByte 3)
    // whose m_type0x63a is a straight or a curve ({0xe,0xf} or {0x10,0x11}), or one of the two
    // plain path pieces (categoryByte 0xc, resourceId 0x3001/0x3002). This is the predicate
    // WorldBoardMaybe's four track-graph walkers use to decide which tiles to step THROUGH on
    // the way to the next junction. Defined in src/Obj0x4779e0.cpp (its address-order home).
    unsigned char IsPlainRunMaybe();
    // 0x44bcd0, extern (ex-FUN_0044bcd0, promoted v350 after reading the body) -- true when
    // nSocketIndex sits at an END of this kind's socket chain: index 0, or == wSocketCount, or
    // (only when wSocketCountExt is non-zero) == wSocketCount + 1 or == wSocketCountExt.
    // Consulted by NameAnchorMaybe::AdvanceAlongTrackMaybe before attempting a socket match,
    // and by ApplyDirectionReversalMaybe when re-seating the anchor after a reversal.
    unsigned char IsEndSocketIndexMaybe(short nSocketIndex);
};
