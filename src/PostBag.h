#pragma once

// ⚠ Self-contained on purpose (v477): this header declares RECT-typed members, so it needs
// <windows.h> of its own rather than relying on every consumer to have included it first. That
// reliance held only by luck -- see the IDirectDrawSurface note just below for the day it did
// not. Byte-neutral by construction: every TU that reaches this header already includes
// <windows.h> ahead of it (any that did not would be COMPILE FAILED today), so the include
// guard makes this expand to nothing everywhere. Verified with a full per-file table diff.
#include <windows.h>

// Pointer-only use below, so a forward declaration is enough and this header stops depending on
// its consumers to have included <ddraw.h> first. Added v474: CarNetState.h pulls this header in,
// so the moment src/CarNetObj.h started including CarNetState.h, four TUs that had never needed
// ddraw.h (DPlaySessionMgr, NameAnchorMaybe, PeerTrainNode, PeerTrainSlotQueueMaybe) became
// COMPILE FAILED. ⚠ The tag must be `struct` -- that is how <ddraw.h> line 54 declares it, and
// VC5 takes a type's default access from the LAST-SEEN class/struct tag (docs/CODEGEN.md).
struct IDirectDrawSurface;

// Global PostBag/clipart-bitmap-cache/easter-name-cache singleton (g_pPostBagCache,
// ex-DAT_004fd3b0). Allocated 0xbe4 bytes in Config_FUN_00406ba0; ctor PostBag_CacheCtorMaybe
// (0x443000), dtor PostBag_CacheDtorMaybe (0x4431f0, vtbl 0x47826c). That vtable's sole REAL
// slot is the dtor -- the raw dwords immediately following it (0x478270/0x478274/0x478278)
// belong to three wholly unrelated classes' own vtables that the linker happened to pack right
// after it (CLAUDE.md's vtable-overread gotcha; re-confirmed here via a per-slot get_xrefs_to:
// each of those addresses has its own independent ctor/dtor xrefs, none touching this class).
//
// Bundles 3 unrelated caches behind one allocation (own TU still unknown -- see
// docs/subsystems.md's PostBag family entry):
//   - a 256-slot LRU clip-art LocoBitmap* cache, keyed by 3 bytes (kind/subkind/index) --
//     ClipartBitmapCache_GetOrLoad (0x4442b0, this-typed in Ghidra, not yet transcribed)
//     and PostBag_ReleaseCachedBitmaps below.
//   - a lazily-loaded 16-entry easter-name table read from PostBag\Easter\<Lang>\easter.usr --
//     PostBag_LoadEasterNameCache below.
//   - a cached PostBag category (.crd) file count (PostBag_GetCategoryFileCountCached,
//     0x443670, this-typed, not yet transcribed) and a lazily-loaded "badge" TileKind
//     descriptor (tile id 0x3cbd, via TileKind_GetOrLoadDescriptor) used by the
//     network-game-count badge icon draw at FUN_004436c0/FUN_004437c0 (both this-typed in
//     Ghidra, not yet transcribed).
struct PostBagCacheSlotKey {
    unsigned char kindHi;
    unsigned char kindLo;
    unsigned char slotIndex;
};

struct LocoBitmap;
class CursorDesc;
class CarNetState;

// One entry of CarNetState's 128-slot, 6-byte-stride decalSlots array -- the car
// sticker/decal system (networked so peers see the same placements; car customization, not
// track telemetry). Struct/field names as already established in docs/subsystems.md's own
// `DecalSlot` writeup (writer-side: AddDecal/0x442c90 fixed-128 FIFO,
// RemoveDecalAtPoint/0x442d30 click-to-remove, CompactDecals/0x442e00 -- all three byte-matched
// in src/CarNetState.cpp as of v445). placementSeq (nonzero = occupied, an incrementing per-editor counter doubling as
// placement order) is ALSO confirmed (2026-07-17, PostBagCacheBundle::DrawPlacedClipartItem/
// DrawLastPlacedItem) to double as ClipartBitmapCache_GetOrLoad's slotIndex arg (which
// numbered decal-graphic bitmap file to load, e.g. "S0%03d.bmp") and as the reader side's own
// "slot occupied" scan test byte -- reading side and writer side agree on this field.
// xHalf/yHalf confirmed *2 (a half-grid-cell dest offset) by the same reader
// functions; width/height (hit-rect size, from the thumbnail cache per the writer-side
// investigation) unread by the reader side.
struct DecalSlot {
    unsigned char packedKind;    // +0x0 -- kind = val>>3, subkind = (val&7)+1
    unsigned char placementSeq;  // +0x1 -- nonzero = occupied; also ClipartBitmapCache_GetOrLoad's slotIndex arg
    unsigned char xHalf;         // +0x2
    unsigned char width;         // +0x3 -- hit-rect width, from the thumbnail cache
    unsigned char yHalf;         // +0x4
    unsigned char height;        // +0x5 -- hit-rect height, from the thumbnail cache
};

struct PostBagCrdFileNode;

struct PostBagCacheBundle {
    // +0x000 -- the implicit vptr. Was modeled as a manual `void *vtable;` member until this
    // session; the class's real slot-0 dtor is 0x4431f0 (vtbl 0x47826c), whose `(flag & 1) ->
    // operator delete` shape is the compiler-generated `??_G` thunk over a real
    // `~PostBagCacheBundle()`. Declared-only -- the body lives in an untranscribed TU, but the
    // declaration is what lets `delete g_pPostBagCache` compile to the original's
    // `mov edx,[ecx]; push 1; call [edx]` in SaveWindowAndCleanExit (0x4077a0).
    // 0x443000 (Ghidra: PostBagCacheBundle::PostBag_CacheCtorMaybe) -- zeroes the 256 slot
    // arrays and interns the badge/frame descriptors. Declared only; it is what
    // `new PostBagCacheBundle` in AppWindow's bootstrap (0x406ba0) dispatches to.
    PostBagCacheBundle();                          // 0x443000
    // ⚠ 0x4431f0 (109 B) IS BLOCKED ON A HEADER DEPENDENCY, not on being understood. Body read
    // in full: release pCachedBadgeTileDesc through its vtable slot 2 (ReleaseRef) and null both
    // it and pRealizedBadgeFrame; then walk paBitmapSlots with a pointer induction variable
    // (`lea esi,[edi+4]` / `add esi,4` / `dec ebx`, 0x100 iterations), `delete` each non-NULL
    // LocoBitmap and null the slot; then PostBag_ClearWorkingFolders(); then the delete-flag
    // tail. Like PostBagFileCache's below it has exactly ONE dtor COMDAT, so it has to be
    // written IN-CLASS -- and an in-class body needs CursorDesc AND LocoBitmap COMPLETE here,
    // i.e. `#include "CursorDesc.h"` + `#include "LocoBitmap.h"` in this header. This header has
    // no includes at all today and is pulled in by 8 files, SIX of which see neither of those
    // two headers right now (AppWindow.cpp, GameNet.cpp, ScreenSaver.cpp, LocalPlayerIdentity.cpp,
    // AlbumCardWnd.h, CarNetState.h).
    //
    // ⛔ MEASURED AND REVERTED (v483) -- DO NOT RE-RUN. Adding both includes here and writing the
    // dtor in-class WORKS: the body is an EXACT MATCH (109 B) on the first compile, spelled
    // exactly as described above. It is the COLLATERAL that kills it. Full per-file table diff:
    //   src/EditCardWnd.cpp    +109 B  (the dtor itself)
    //   src/PeerTrainNode.cpp  +406 B  (a bonus exact, purely from the shifted symbol count)
    //   src/NameAnchorMaybe.cpp -456 B (2 exacts lost)
    //   src/TutorialWnd.cpp     -249 B (1 exact lost)
    //   ---------------------------------------------------------------------------
    //   net -190 B, -1 exact function
    // That +2/-3 TRADE is the fingerprint of a codegen-DIAL move, not of a recovered fact: these
    // two includes push ~2000 file-scope symbols into six TUs the ORIGINAL's PostBag TU never
    // shared a header with, so the shape being reproduced is ours, not the game's. See
    // docs/CODEGEN.md's "the dial is an instrument, not a knob" entry. Revisit only once the
    // repo is fully transcribed and this header's real consumer set is known.
    virtual ~PostBagCacheBundle();                 // 0x4431f0
    LocoBitmap *paBitmapSlots[256];           // +0x004
    unsigned int nNextAccessCounter;          // +0x404
    unsigned int aSlotAccessOrder[256];       // +0x408
    PostBagCacheSlotKey aSlotKeys[256];  // +0x808
    CursorDesc *pCachedBadgeTileDesc;    // +0xb08 -- TileKind::TileKind_GetOrLoadDescriptor(&g_UIResources, 0x3cbd);
                                                    // real runtime type is BigObj (see EditCardWnd.cpp's own pPreviewIconDesc
                                                    // precedent), typed to the CursorDesc base -- only the inherited
                                                    // GetOrLoadFrameBitmap slot is ever called through this field.
    void *pRealizedBadgeFrame;                // +0xb0c -- pCachedBadgeTileDesc->GetOrLoadFrameBitmap(0, 0)'s
                                                    // own return value, same idiom as EditCardWnd's pPreviewIconRealized
    short nCachedCategoryFileCount;           // +0xb10 -- -1 = uncached, see PostBag_GetCategoryFileCountCached (0x443670)
    unsigned char bEasterNameCacheLoaded;     // +0xb12
    char aEasterNames[16][13];                // +0xb13

    short PostBag_GetCategoryFileCountCached(); // 0x443670
    // 0x445170 -- the uncached category-2 recount. Declared here as the member it really is
    // (Ghidra retyped it __thiscall on this class in v362) now that its first caller,
    // MailWnd::EndActiveSession, is transcribed and needs the `mov ecx,g_pPostBagCache` the
    // member call shape produces. Its DEFINITION stays free-shaped in src/EditCardWnd.cpp --
    // the body never reads `this`, so the two spellings compile to the same bytes; see the
    // free declaration further down this header.
    short PostBag_RecountCategoryOutFiles();
    LocoBitmap *ClipartBitmapCache_GetOrLoad(unsigned char kindHi, unsigned char kindLo,
                                                   unsigned char slotIndex, char bSuppressEvictMaybe); // 0x4442b0
    // Builds one of 8 category folder paths (nCategory 0-7, see PostBag_ScanCategoryCrdFiles
    // below), saves pCard into it (CarNetState::SaveCardFile), refreshes the cached
    // Sort\Out file count (always rescans category 2 regardless of nCategory -- sic, matches
    // PostBag_GetCategoryFileCountCached's own hardcoded category 2), and for nCategory==0
    // (Album) also inserts a PostBagFileCache index entry. NOT a folder-ensure/init function
    // despite its ex-name (PostBag_InitCategoryFolderMaybe) -- corrected 2026-07-17, see
    // docs/subsystems.md's PostBag family entry.
    unsigned char PostBag_SaveCardToCategory(CarNetState *pCard, int nCategory,
                                                   char *pszSubDir); // 0x444d00
    // Deletes pszPath, then (only if that succeeded) refreshes nCachedCategoryFileCount by
    // rescanning category 2 (Sort\Out) -- same scan+free+tally idiom as
    // PostBag_GetCategoryFileCountCached. Used by the various card-delete UI handlers.
    void DeleteCardFileAndRefreshCount(const char *pszPath); // 0x444fb0
    // Builds pCard's own .crd path (same category switch as PostBag_BuildCrdPath, keyed by
    // nCategory, using pCard->nPostSeqId as the id) and deletes it via
    // DeleteCardFileAndRefreshCount's own delete+rescan+tally idiom. The .crd-delete
    // counterpart to PostBag_SaveCardToCategory.
    void DeleteCardById(CarNetState *pCard, int nCategory); // 0x445000

    // Deletes every non-dotfile in pszDirWithWildcard's directory (a "<dir>\*.*" pattern).
    // `this` is a real PostBagCacheBundle* (confirmed via its sole caller,
    // PostBag_ClearWorkingFolders below, which forwards its OWN this unchanged) but is
    // never read in the body -- ecx just needs to carry it through per this cluster's
    // already-documented "this-in-ecx but never read" calling-convention class (see CLAUDE.md).
    void PostBag_ClearFolder(char *pszDirWithWildcard); // 0x443550

    // Clears the 4 working folders that accumulate transient per-session files: Sort\In,
    // Sort\Out, Att_Out, Att_In. `this` forwarded unchanged into each PostBag_ClearFolder
    // call, same "this-in-ecx but never read" class as that method. Sole caller:
    // PostBag_CacheDtorMaybe (0x4431f0).
    void PostBag_ClearWorkingFolders(); // 0x443470

    // Shared placed-item draw primitive for DrawLastPlacedItem/FUN_004437c0
    // (DrawCardThumbnail, untranscribed): unpacks pRecord's packed kind/subkind byte +
    // variant/grid-offset bytes, resolves the clip-art LocoBitmap via
    // ClipartBitmapCache_GetOrLoad, computes a centered dest rect (clipped against
    // rectClip) and the matching source-crop rect, then RestoreOverlapBlt + Release.
    // nUnused: a genuine 7th stack argument, confirmed via the caller's own push sequence
    // and this function's `ret 0x1c` purge (28 bytes = 7 dwords, one more than the 6 originally
    // shown by Ghidra's analyzer) -- but Ghidra's dataflow found ZERO reads of it anywhere in
    // the body, so it's a dead/vestigial trailing parameter (possibly used by a future/other
    // caller), not a decompiler under-analysis gap. Corrects the prior "already clean, no
    // unresolved args" assumption for this function -- see docs/subsystems.md.
    void DrawPlacedClipartItem(IDirectDrawSurface *pTargetSurface, RECT rectClip,
                                     DecalSlot *pRecord,
                                     unsigned int nUnused); // 0x4440a0

    // Draws the single MOST-RECENTLY-placed clip-art decal on pCard (a backward scan of
    // decalSlots[127..0] for the first slot with placementSeq != 0 -- despite scanning
    // backward this finds "last non-empty slot in array order", not necessarily the highest
    // placement sequence number), via DrawPlacedClipartItem, then frames rectClip with a
    // BLACK_BRUSH highlight border. Lazily realizes the shared "network game count" badge tile
    // (pCachedBadgeTileDesc/pRealizedBadgeFrame, tile id 0x3cbd) the same way
    // DrawCardThumbnail does, and bumps the shared LRU access-stamp counter
    // (nNextAccessCounter) exactly like that sibling too -- both blocks are duplicated
    // inline rather than factored into a shared helper, matching this cluster's established
    // "category switch" duplication pattern (docs/subsystems.md). Sole caller:
    // EditCardWnd_HandleLButtonDownMaybe (0x41c416).
    void DrawLastPlacedItem(CarNetState *pCard, IDirectDrawSurface *pTargetSurface,
                                  RECT rectClip, unsigned int nUnused); // 0x4436c0

    // Corner "postmark stamp" badge draws for the postcard/album thumbnail (sole caller:
    // FUN_004437c0/DrawCardThumbnail, untranscribed). Both share ClipartBitmapCache_
    // GetOrLoadMaybe's own kindHi 30/31 file families ("R%01d%03d.bmp"/"S0%03d.bmp") and the
    // same 7/6-dword-plus-trailing-unused calling convention as DrawPlacedClipartItem --
    // recovered by tracing the caller's own push sequence against each function's `ret`
    // cleanup size, since Ghidra's own analyzer had misread the by-value RECT as loose
    // in_stack_* scalars (same decompiler-failure class as DrawCardThumbnail itself).
    // A positions the badge inset (-10,+10) from rectClip's top-right corner, using all 4
    // fields of rectClip; B (kindHi 31, no kindLo needed -- the "S0%03d.bmp" format string
    // doesn't reference it) pins exactly at rectClip's top-right corner (1px inset), reading
    // only rectClip.top/.right -- rectClip.left/.bottom occupy real stack slots (the caller
    // passes the same by-value RECT to both) but are genuinely unread by B, confirmed via
    // Ghidra's own "undefined4"-typed param_2/param_5 before the prototype fix.
    void DrawCornerBadgeA(IDirectDrawSurface *pTargetSurface, RECT rectClip,
                                unsigned char bySlotIndex, unsigned char byVariant,
                                unsigned int nUnused); // 0x443f00
    void DrawCornerBadgeB(IDirectDrawSurface *pTargetSurface, RECT rectClip,
                                unsigned char bySlotIndex,
                                unsigned int nUnused); // 0x443ff0

    // Draws one card's full postbag/album thumbnail: either a flat tint-color fill
    // (bTintFillOnly, using PostBag_ComputeTintColor on pCard's byIdentityColorR/G/B RGB
    // bytes) or the full layout (white background, optional caller hover-highlight rect,
    // pCard->szDescription word-wrapped description, then a divider-line grid with the name
    // (nameA, falling back to a shared default string when empty), the 2 locale-string
    // labels (ids 100/101) and nameB). Lazily realizes the shared "network game count"
    // badge tile and bumps the shared LRU access-stamp exactly like DrawLastPlacedItem
    // (both blocks duplicated inline, same pattern). Bracketed by pTargetSurface->GetDC/
    // ReleaseDC. bTintFillOnly is ALSO reused after the ReleaseDC to pick which decal
    // draw happens: true draws the up-to-128-slot DrawPlacedClipartItem loop (a forward
    // scan, stopping at the first EMPTY slot -- unlike DrawLastPlacedItem's backward
    // "last occupied slot" scan), false draws the 2 corner "postmark" badges (DrawCornerBadgeB
    // then A, using pCard->byStampSlotB/94/95 as their selector bytes). Then, if pCard->wAttachmentId !=
    // 0, stamps the same realized badge tile via RestoreOverlapBlt at a fixed (rectClip.left+
    // 20, rectClip.top-11) offset. Finally re-acquires the DC for one last FrameRect(&rectClip,
    // BLACK_BRUSH) highlight (same idiom as DrawLastPlacedItem's own trailing frame) and
    // deletes the background brush. Called from 3 different windows (init-area/EditCardWnd-
    // area/MailWnd-area, all untranscribed) -- see docs/subsystems.md for the full raw-disasm
    // derivation of this signature and layout.
    void DrawCardThumbnail(unsigned char bTintFillOnly, CarNetState *pCard,
                                 IDirectDrawSurface *pTargetSurface, RECT rectClip,
                                 unsigned int nUnused,
                                 RECT *pHighlightRect); // 0x4437c0

    // Builds "<install>PostBag<category>\<nId as %08d>.att" into pszOut. Genuine __thiscall
    // method that never reads `this` (every call site loads ecx=g_pPostBagCache; Ghidra
    // types the shared body __stdcall, and the free extern below mirrors that for the definition
    // in src/EditCardWnd.cpp) -- modeled as a member here so call sites reproduce the ecx load.
    // nId is a 16-bit param at the ABI (call sites push it via `mov ax,..; push eax` with no
    // zero-extend -- Yoda lesson #14); the free extern's `unsigned int` only drives the definition.
    void PostBag_BuildAttFilePath(unsigned short nId, int nCategory, char *pszOut); // 0x445400

    // Builds "<install>Clipart\<filename>" into pszOut. Same "this-in-ecx but never read"
    // class as PostBag_BuildAttFilePath above (all 4 call sites -- 3 in
    // NetResource_RequestMissingAppearances, 1 in GameNet_DispatchMessage -- load
    // ecx=g_pPostBagCache immediately before the call; confirmed via raw disasm at
    // 0x438ed6/0x438fe6/0x4390db/0x439bf4). Ghidra keeps the shared body __stdcall (never
    // reads `this`); the free extern below mirrors that for the definition in
    // src/EditCardWnd.cpp -- modeled as a member here so call sites reproduce the ecx load.
    void PostBag_BuildClipartFilePath(unsigned char bDescByte, unsigned char nIndex, char *pszOut); // 0x445700

    // 0x445510 -- the ".dat" sibling of PostBag_BuildAttFilePath above, same
    // "this-in-ecx but never read" class and the same 16-bit nId at the ABI (its two call sites
    // in MailWnd::OpenAttachmentMaybe both push it via `mov dx,[eax+0x3a]; push edx` with no
    // zero-extend). Declared as a member so those call sites reproduce the ecx load; the free
    // extern below still drives the definition in src/EditCardWnd.cpp.
    void PostBag_BuildDatFilePath(unsigned short nId, int nCategory, char *pszOut); // 0x445510

    // 0x445910 -- packs a (kind, subkind) pair into the single DecalSlot::packedKind byte
    // (`(nSubkind - 1) | (nKind << 3)`, the inverse of DecalSlot's own documented unpack).
    // Same "this-in-ecx but never read" class as the path builders around it: both call sites
    // (0x438fbe / 0x4390b0, in NetResource_RequestMissingAppearances) load
    // `mov ecx,[g_pPostBagCache]` immediately before the call, and the load is otherwise DEAD
    // there (BuildClipartFilePath re-loads ecx a few instructions later) -- which is exactly
    // what makes it the `this` rather than a stray global reload. Declared as a member so those
    // two sites reproduce the ecx load; the body itself is defined free-shaped __stdcall in
    // src/NetResource.cpp, which is why it byte-matched as a free function.
    unsigned char PostBag_PackDecalKind(char nKind, char nSubkind); // 0x445910

    // 0x445a40 -- reads a category .dat sidecar into pOutBuf. Same "this-in-ecx but never read"
    // class and the same 16-bit nId at the ABI as the two members above: its sole call site
    // (MailWnd::PromptForAttachmentSavePathMaybe) does `mov dx,[ecx+0x3a]; mov ecx,[DAT_004fd3b0];
    // push edx` with no zero-extend. Declared as a member so that call site reproduces the ecx
    // load; the free extern below still drives the definition in src/EditCardWnd.cpp, whose own
    // `nId & 0xffff` is what makes the body read the full 32-bit slot.
    void PostBag_ReadDatFile(unsigned short nId, int nCategory, void *pOutBuf); // 0x445a40

    // Same "this-in-ecx but never read" class as the two members above. Confirmed via raw disasm
    // at GameNet_HandleTrainStateSync's own call site (0x43b43e): `mov ecx,[DAT_004fd3b0]; call
    // 0x445f20`. Ghidra keeps the shared body __stdcall (never reads `this`); the free extern
    // below mirrors that for the definition in src/EditCardWnd.cpp.
    unsigned short PostBag_AllocNextAttId(); // 0x445f20

    // Three more of the "this-in-ecx but never read" class, promoted from free __stdcall
    // externs to the members they really are (v412): the ecx-establishment sweep found that
    // ALL 25 direct call sites of these eight PostBag_* functions load `mov ecx,[g_pPostBagCache]`
    // immediately before the call, so the whole cluster is one class. Their bodies never read
    // `this`, which is why Ghidra keeps them __stdcall and why the free spelling byte-matched.
    void PostBag_DeleteAttachmentFiles(int nCategory, unsigned short nId);   // 0x4451a0
    void PostBag_BuildEasterCardPath(char *pszName, char bCrd, char *pszOut); // 0x445620
    unsigned int PostBag_ImportAttachmentFile(int nCategory, char *pszSrcPath); // 0x445bd0

    // Same "this-in-ecx but never read" class as the three members above -- and the reason
    // DeleteCardFileAndRefreshCount's `mov ecx,ebx` is NOT a dead reload. EVERY call site in the
    // binary passes ecx: the ~15 outside this class load it from the g_pPostBagCache global
    // (0x42d8cb, 0x42d9a9, 0x42da31, 0x42dad8, 0x42db51, 0x42dbf8, 0x42dc75, 0x42dcf7, 0x42dd6a,
    // 0x42df8e, 0x42e00b, 0x42e2de, 0x42f63d, 0x43088c, 0x4308c8, 0x430900, 0x4309d7, 0x440b25),
    // and the in-class ones pass `this` -- reloading it (0x444fc9, after DeleteFileA clobbered
    // ecx) or relying on it still being live from the prologue (0x443686, 0x445177). Ghidra keeps
    // the shared body __stdcall (it never reads `this`); the free extern below mirrors that for
    // the definition in src/EditCardWnd.cpp.
    PostBagCrdFileNode *PostBag_ScanCategoryCrdFiles(int nCategory, const char *pszSubDir); // 0x4446f0

    // Two more of the same "this-in-ecx but never read" class, both reached from
    // PostBagFileCache::FindFirstLoadableCardAtOrAfterIndex (src/EditCardWnd.cpp), whose
    // `mov ecx,[g_pPostBagCache]` pair was autopsied from v168 to v361 as two "unrelated, unused
    // global reloads". They are the two calls' `this`. 0x445930 has exactly ONE call site in the
    // binary (0x401c4d) and it loads ecx; all ~10 sites of 0x444c70 (0x401c60, 0x42d8dc, 0x42da46,
    // 0x42db66, 0x42dc86, 0x42dd7f, 0x42ddba, 0x42dde4, 0x4309ec, 0x43ec83) load ecx too. Ghidra
    // keeps both bodies __stdcall (neither reads `this`); the free externs elsewhere mirror that
    // for the definitions in src/EditCardWnd.cpp.
    void PostBag_BuildCrdPath(int nId, int nCategory, char *pszOut);   // 0x445930
    CarNetState *CarNetState_CreateFromFile(const char *pszPath);      // 0x444c70

    // Same class again: exactly ONE call site in the binary (0x4438cb, inside this class's own
    // DrawCardThumbnail) and it passes `this` -- `mov ecx,esi` where the prologue's `mov esi,ecx`
    // makes esi `this`. Ghidra keeps the body __stdcall (it never reads `this`); the free extern
    // below mirrors that for the definition in src/EditCardWnd.cpp.
    unsigned int PostBag_ComputeTintColor(unsigned char byR, unsigned char byG, unsigned char byB); // 0x4441c0
};

extern PostBagCacheBundle *g_pPostBagCache; // DAT_004fd3b0

// Uncached recount of category-2 (Sort\Out) .crd files: walks
// PostBag_ScanCategoryCrdFiles(2,0)'s node list, frees each node, returns the tally -- some
// callers discard the return value, using the call purely for its node-list side effect after a
// client-identity change (e.g. LocalPlayerIdentity::SetNameMaybe).
//
// ⚠ This comment used to claim "A free function (no `this`/cache)". That is REFUTED (v362,
// lever 3): all FOUR call sites in the binary (0x42f7e1, 0x440c1d, 0x452f7c, 0x45309f) load
// `mov ecx,[g_pPostBagCache]` immediately before the call, so it is a PostBagCacheBundle member
// of the same "this-in-ecx but never read" class as the members declared above -- the body just
// never reads `this`, which is precisely why its own bytes look like a free function. Ghidra was
// retyped __thiscall on PostBagCacheBundle in v362. Kept declared free HERE only because the
// definition in src/EditCardWnd.cpp is free-shaped (same dual model as the members above) and
// NONE of its four callers are transcribed yet -- when the first one is, call it through
// `g_pPostBagCache->` and add the member declaration to the class above.

// One 24-byte sorted-index entry: a card's name (raw case, NOT the uppercased bucket key) plus
// its post-sequence id (CarNetState::nPostSeqId). PostBagFileCache::IndexCard builds one
// per Album save; InsertRecord (0x401690, src/EditCardWnd.cpp) grows/memmoves the owning
// bucket's array to keep it sorted by szName.
struct PostBagAlbumIndexRecord {
    char szName[20];
    unsigned int nId;
};

// The alphabetical-index side of the PostBag album (0x18 bytes, operator-new-confirmed --
// Config_FUN_00406ba0/0x406d1b allocates g_pPostBagFileCache via new_alloc(0x18); ctor
// 0x401620, vtbl 0x4773e8, literals "PostBag"/"\\AlbIndex" -- see docs/subsystems.md's PostBag
// family entry). Holds ONE bucket's records in memory at a time (9 buckets total, keyed by the
// first letter of the card's uppercased name: A-C/D-F/G-J/K-M/N-Q/R-T/U-W/X-Z/other); switching
// buckets (LoadIndexedFile) flushes the current one to "<install>PostBag\AlbIndex\<player
// id><bucket>.ind" first (SaveIndexFile, 0x401c90) if dirty.
struct PostBagFileCache {
    // +0x000 -- the implicit vptr (was a manual `void *vtable;` member).
    // 0x401620 (Ghidra: PostBagFileCache::Ctor) -- declared only; what `new PostBagFileCache`
    // in AppWindow's bootstrap (0x406ba0) dispatches to.
    PostBagFileCache();                        // 0x401620

    // ⚠⚠ 0x401650 IS DELIBERATELY LEFT UNTRANSCRIBED -- MEASURED AND REJECTED (v444), do not
    // re-probe it. Its whole body is `SaveIndexFile();`, and because 0x401650 is this class's
    // ONLY dtor COMDAT (the `??_G` scalar deleting form with the dtor merged in) it has to be
    // written IN-CLASS, i.e. `virtual ~PostBagFileCache() { SaveIndexFile(); }` right here.
    // Spelled that way it IS byte-EXACT at 36 B on the first compile -- and it costs
    // src/TutorialWnd.cpp's RestorePresenterBackdrop (0x452b00) its full 249 B, MATCH -> DIFF at
    // unchanged length. TutorialWnd.cpp reaches this header through AlbumCardWnd.h. Full-repo
    // measurement: 122091/517 with the in-class body vs 122055/516 without, i.e. +36 B here and
    // -249 B there for a NET -213 B. Bisected by reverting this one declaration alone.
    // Fourth member of the v442/v443 in-class-dtor cluster (see docs/PARKED.md); every one so
    // far has traded a small local win for a larger loss in a TU downstream of the header.
    // ** v449 re-measured this inside the full five-lever repo-wide sweep those rows deferred to:
    // +36 B here against the same fixed -249 B, which every lever pays independently and
    // identically. Cluster gain +99 B vs 249 -- no subset tips positive, so the "net-positive
    // taken together" hypothesis is CLOSED, not deferred. Do not re-probe. **
    //
    // ⭐ RE-PRICED v546 and the verdict has MOVED A LOT -- still negative, but now by 54 B, not
    // 213. Re-measured because v545 recovered 0x452b00 through an unrelated lever (declaring
    // AlbumCardWnd's two ICF-folded slots), which proves the repo-wide parity this answers to has
    // shifted since v449, and a "spent" verdict only ever covers the knobs that were tried.
    // Current price, from a clean baseline:
    //     +36 B  0x401650 itself, byte-EXACT on the first compile as before
    //     +159 B CarNetState::AddDecal (0x442c90) flips DIFF -> MATCH. NEW -- v449 never saw
    //            this; PostBag.h is upstream of CarNetState.cpp via CarNetState.h.
    //     -249 B src/TutorialWnd.cpp's 0x452b00, the same fixed toll, still paid in full.
    //     = -54 B net.
    // Two spellings were measured and are byte-identical to each other, so the class-body form is
    // not the variable: `virtual ~PostBagFileCache() { SaveIndexFile(); }` in-class, and
    // `virtual ~PostBagFileCache();` plus an `inline PostBagFileCache::~PostBagFileCache()
    // { SaveIndexFile(); }` after the struct. Both give exactly 171839 B.
    // ⇒ ONE more parity move that recovers 0x452b00 makes this lever net POSITIVE (+195 B) and
    // claims 0x401650 for good. That, not another spelling of the dtor, is what to hunt next.
    // The obvious candidate is a real declaration added to a header TutorialWnd.cpp reaches;
    // WorldBoardMaybe.h's undeclared `virtual ~WorldBoardMaybe()` (vtable 0x478520 slot 0 =
    // ??_G 0x454db0, dtor 0x454dd0) is one, but WorldBoardPartial still models its vptr as
    // pad0x0[4], so that is its own piece of work -- same shape as the carried UIResources item.
    virtual ~PostBagFileCache();
    int nLoadedBucket;                    // +0x04, -1 = none loaded
    PostBagAlbumIndexRecord *pRecords; // +0x08
    unsigned int nRecordsBytes;           // +0x0c, byte length of pRecords, multiple of 0x18
    int Unk0x10;                               // +0x10 -- last InsertRecord insertion position, write-only here
    int Unk0x14;                               // +0x14 -- nLoadedBucket stashed at insert time, write-only here

    void LoadIndexedFile(int nBucket); // 0x401df0, src/EditCardWnd.cpp
    void SaveIndexFile(); // 0x401c90, src/EditCardWnd.cpp
    void InsertRecord(int nPos, PostBagAlbumIndexRecord *pRecord); // 0x401690, src/EditCardWnd.cpp

    // Delete-record half of InsertRecord's sorted-array maintenance -- see
    // src/EditCardWnd.cpp for the full behavioral summary. Only caller: PurgeDuplicateIndexEntry.
    void RemoveRecordAtIndex(int nIndex); // 0x401760, src/EditCardWnd.cpp

    void IndexCard(CarNetState *pCard); // 0x401850, PARKED -- see docs/PARKED.md

    // Read-by-index counterpart to IndexCard/InsertRecord. Loads nBucket, then walks
    // the records array starting at nStartIndex (byte-offset style -- nRecordsBytes is a
    // raw byte count, not a record count), building each record's .crd path
    // (PostBag_BuildCrdPath on nId) and trying to load it
    // (CarNetState_CreateFromFile). A missing/corrupt file (NULL) advances to the next
    // record instead of failing outright -- this is really find-first-loadable-card-at-or-after-
    // index, not a strict by-index lookup. Returns NULL once past the loaded records.
    CarNetState *FindFirstLoadableCardAtOrAfterIndex(int nStartIndex, int nBucket); // 0x401c10

    // Re-derives pCard's OWN name-bucket (same A-Z mapping as IndexCard), loads THAT
    // bucket's index, and checks whether a record with pCard's own id (nPostSeqId) is already
    // present in it -- i.e. "is this card correctly indexed under its own name's bucket". If
    // found, also removes that index record (RemoveRecordAtIndex). AlbumCardWnd::
    // PurgeDuplicateCards's sweep deletes any visible card this returns true for (a
    // duplicate/stale entry). Transcribed, see src/EditCardWnd.cpp for match status.
    unsigned char PurgeDuplicateIndexEntry(CarNetState *pCard); // 0x401aa0

    // Ensures the given bucket index is the currently-loaded one (LoadIndexedFile if not),
    // then returns its record count (nRecordsBytesMaybe / sizeof(PostBagAlbumIndexRecord) --
    // same divide as GetCountDiv24, just after a conditional bucket switch). Called by
    // AlbumCardWnd::OnLButtonDown's page-back handler with the new nStartIndex candidate's
    // bucket, for the wraparound check. Not yet transcribed -- declared only.
    unsigned int LoadBucketAndGetRecordCount(int param_1); // 0x401820

    // Loaded-bucket record count: nRecordsBytes / sizeof(PostBagAlbumIndexRecord).
    // Defined in src/EditCardWnd.cpp (moved out of src/phase2_probe2.cpp 2026-07-22,
    // where it was matched as the probe-local DivObj0x401810).
    unsigned int FUN_401810_GetCountDiv24(); // 0x401810
};

extern PostBagFileCache *g_pPostBagFileCache; // DAT_004fd3b4, ctor 0x401620 via new_alloc(0x18) in Config_FUN_00406ba0

// One node of the singly-linked ".crd" file list PostBag_ScanCategoryCrdFiles (0x4446f0,
// src/EditCardWnd.cpp) builds: a 0x504-byte path buffer immediately followed by a "next"
// pointer, allocated as one 0x508-byte block per node (new_alloc(0x508) at the scan site).
struct PostBagCrdFileNode {
    char szPath[0x504];
    PostBagCrdFileNode *pNext;
};

// nCategory: 0=Album, 1=Sort\In, 2=Sort\Out, 3=Sort\Bag, 4=Att_Out, 5=Att_In, 6=Easter\<Lang>
// (g_nEasterLocaleId-selected), 7=Design. pszSubDir: NULL for the top-level category
// dir, else an extra subfolder (e.g. a design name) inserted before the client-id glob.

// Same list-node shape reused by an unrelated scanner: SaveGame_ScanSavFiles (0x448390,
// src/EditCardWnd.cpp) walks "<install>SaveGame\*.sav"/"<install>ScrSaver\*.sav" and returns a
// PostBagCrdFileNode list of bare filenames (no directory prefix concatenated, unlike the
// PostBag scan above).
//
// ⚠ v366: 0x448390 is really a `this`-ignoring MEMBER of ScreenSaver -- it sits inside that
// class's contiguous .text run (0x448040..0x44898f) and its only caller,
// ScreenSaver::GetLayoutFileName (0x4481b0), reaches it with `mov ecx,esi`. v367 declared it
// where it belongs (src/ScreenSaver.h) so that call site gets the ecx load for free; the BODY
// stays in src/EditCardWnd.cpp, since it is already EXACT there and moving it would rotate that
// TU's /Og state. See docs/subsystems.md's ScreenSaver entry.

// Builds "<install>PostBag\<category>\<nId as %07d>.crd" (nCategory: same 0-7 mapping as
// PostBag_ScanCategoryCrdFiles's own comment above) into pszOut. Transcribed
// src/EditCardWnd.cpp -- see docs/subsystems.md's PostBag.cpp cluster catalog.

// Deletes both the .att/.dat sidecar files for an attachment id in a category folder (same
// category switch as PostBag_BuildCrdPath above, duplicated verbatim). No rescan/count
// refresh, unlike PostBagCacheBundle::DeleteCardById. Transcribed src/EditCardWnd.cpp.

// Builds "<install>PostBag<category>\<nId as %08d>.att"/".dat" into pszOut (same category
// switch as PostBag_BuildCrdPath above). Callers: multiplayer attachment file-transfer
// subsystem (GameNet_*/NetFile_*). Transcribed src/EditCardWnd.cpp.

// Builds "<install>PostBag\Easter\<Lang><pszName><ext>" into pszOut, ext = ".crd" if bCrd else
// ".rsp" -- always targets the Easter locale subfolder, no outer category switch. Sole caller:
// LoadOrCreateEasterCard. Transcribed src/EditCardWnd.cpp.

// Builds "<install>Clipart\<filename>" into pszOut, creating the Clipart\ directory first if
// missing. Same 32-way kindHi-range switch as PostBagCacheBundle::ClipartBitmapCache_
// GetOrLoadMaybe. bDescByte packs kindHi (upper 5 bits) and kindLo-1 (lower 3 bits). Callers:
// network resource-sync. Transcribed src/EditCardWnd.cpp.

// Reads up to 0x400 bytes of a category .dat sidecar file into pOutBuf (same category switch +
// path format as PostBag_BuildDatFilePath). Sole caller: FUN_0042eea0 (MailWnd/
// AlbumCardWnd-area). Transcribed src/EditCardWnd.cpp.

// Standalone version of the "[POSTCARD] NextAttId" ini-counter alloc/wrap: reads the counter
// (default 1), wraps to 1 past 0x7ffc, writes counter+1 back, returns the pre-increment value.
// Sole caller: GameNet_HandleTrainStateSync. Transcribed src/EditCardWnd.cpp.

// Allocates a new attachment id (inline duplicate of PostBag_AllocNextAttId's own
// ini-counter logic), writes a .dat placeholder whose content IS pszSrcPath itself (zero-padded
// to 0x400 bytes), then CopyFileA's the real source into the new .att path on success. Returns
// the new id, or 0 on failure. Sole caller: EditCardWnd_ImportDecalImageMaybe. Transcribed
// src/EditCardWnd.cpp.

// Trivial leaf: computes a 3-channel additive/clamp tint color from 3 input bytes (not real
// HSV/blend math). byG dominates (sets the R/G/B baseline), byB nudges toward red, byR nudges
// toward blue; each channel clamped to [0,255]. Returns packed 0x00RRGGBB. Sole caller is
// DrawCardThumbnail, a PostBagCacheBundle method at 0x4437c0 (untranscribed) that tints
// the card background fill when a flag byte is set. Transcribed src/EditCardWnd.cpp.
