// ⚠ DECLARATION-COUNT WARNING (measured 2026-07-26, and sharper than the older "adding decls
// here rotates other TUs" folklore): what this header's member-function declarations move in
// OTHER TUs is their COUNT, not which method or where in the class it sits. Adding
// TestOrClearFootprintMaybe cost src/WalkerActor.cpp's UpdateFacingFromStepDeltaMaybe (0x4331b0,
// 112 B) its EXACT; moving the same declaration to the end of the class changed nothing; and
// DELETING one unrelated, declared-only method (CaptureBoardToBitmap, see below) restored it
// exactly. Always re-run a FULL tools/progress.py after touching this list, and read the
// per-file table.
//
// ⭐ SHARPENED AGAIN 2026-07-27, and the earlier "the budget is fixed: to add a declaration,
// retire one" reading was WRONG in two ways:
//   (1) It is a PARITY BIT, not a budget. Adding a declaration flips it and so does removing
//       one -- either direction pays. TrackGraph_GetLinkCostAMaybe's new BODY in the .cpp cost
//       src/WorldBoardMaybe.cpp's own FindNearestObjOfCategoryMaybe (0x457ce0, 951 B) its EXACT
//       at 327/327 instructions with a pure eax<->ecx rename, and ADDING the
//       BuildTrackGraphsIfReadyMaybe declaration below restored it exactly. Prefer paying by
//       ADDING a real declaration: it costs nothing and documents one more method.
//   (2) The counter spans DECLARATIONS HERE AND FUNCTION DEFINITIONS IN THE .cpp ALIKE. A new
//       body in src/WorldBoardMaybe.cpp moves the bit exactly as a new member declaration does,
//       and its POSITION in the file is irrelevant (tried both the file tail and the new
//       function's own address-order slot -- identical result). What does NOT move it: a
//       FREE-function declaration (TrackGraph_GetLinkCostAMaybe's own, at the bottom of this
//       header, was measured inert). Member declarations only.
// ⚠ CaptureBoardToBitmap is NO LONGER a usable donor, whatever the 2026-07-26 note says: it has
// since acquired real callers in src/DPlaySessionMgr.cpp and src/ThumbnailBmp.cpp, and deleting
// its declaration turns BOTH of those TUs into COMPILE FAILED (measured, not predicted). Nothing
// else declared here is uncalled either -- checked all nine 2026-07-27. Pay by adding.
//
// ⭐ CORRECTED AGAIN 2026-07-27 (v480), and the "it is a PARITY BIT" reading above is ALSO wrong:
// it is a COUNTER, and different consumer TUs flip at DIFFERENT counts. Transcribing the
// track-graph cluster (seven new bodies in the .cpp, two new member declarations here) moved four
// consumers at once, and three states were measured against the session-open baseline by adding
// 0, 1 and 2 further declarations:
//     +0 decls:  ThumbnailBmp -520, TilePlacedObj -143, LocoBitmap +124, TutorialWnd +249  = -290
//     +1 decl:   ThumbnailBmp   0,  TilePlacedObj -143, LocoBitmap +124, TutorialWnd +249,
//                WidgetBase -407                                                          = -177
//     +2 decls:  ThumbnailBmp -520, TilePlacedObj -143, LocoBitmap   0,  TutorialWnd +249,
//                WidgetBase -407                                                          = -821
// No single state is neutral for everything, and a change that repairs one consumer can break a
// consumer that the previous state left alone (WidgetBase.cpp only appears at +1). So the rule is
// not "flip it back" -- it is MEASURE SEVERAL COUNTS AND TAKE THE BEST TOTAL. +1 is what is
// committed (EraseAlongDragSegmentMaybe below). The canary set to re-measure is
// src/ThumbnailBmp.cpp, src/TilePlacedObj.cpp, src/LocoBitmap.cpp, src/TutorialWnd.cpp and
// src/WidgetBase.cpp -- five cc.sh runs, far cheaper than a full progress.py per probe.
//
// WorldBoardMaybe (DAT_004aad08) -- the huge, mostly-unmapped gameplay world-board singleton
// (docs/subsystems.md). ONE canonical shared partial view: every consumer includes THIS header
// and extends it in place as more fields are read, rather than each TU declaring its own
// divergent local struct -- the old "N per-TU partial view" pattern is retired (see CLAUDE.md's
// "never duplicate a struct across TUs" rule, 2026-07-17). Still far from the full struct; only
// fields actually touched by src/ so far are modeled.
#pragma once

#include <windows.h>

struct TilePlacedObjPartial; // fwd -- each consumer TU defines/forward-declares its own opaque
                              // view; only used here as an opaque pointer type
class TilePlacedObj;         // fwd -- src/TilePlacedObj.h, the REAL modeled class; only used
                              // here as a pointer return type, so no include is needed
class TrackGraph;            // fwd -- src/TrackGraph.h
class BigObj;                // fwd -- src/CursorDesc.h, the full kind descriptor (its
                             // Obj0x4779e0 base carries the two occupancy tables); only used
                             // here as a pointer type

// The board's tile grid is 81 columns x 65 rows of 64-byte cells, stored FLAT and indexed
// `aTileCellsMaybe[col * BOARD_TILE_ROWS + row]` -- that index expression, not a 2-D subscript,
// is what the original emits (`(col * 65 + row) * 64`, one shift-add pair rather than two
// independent scales). 81 * 65 == 5265 cells == 0x52440 bytes, which is exactly the span between
// the array's own +0x44 and pDirtyBitmapMaybe at +0x52484, so the dimensions are pinned by the
// layout and not merely by the range checks.
#define BOARD_TILE_COLS 81
#define BOARD_TILE_ROWS 65

// One cell of that grid: two 7-deep planes of placed objects plus their occupancy counts.
// Plane A is the one gameplay reaches for by default (WorldBoardPartial::GetPlaneASlotMaybe and
// FindNearestObjOfCategoryMaybe both take slot 0 of it).
struct BoardTileCellMaybe {
    unsigned int dwUnk0x00Maybe;         // +0x0
    TilePlacedObj *aPlaneASlotsMaybe[7]; // +0x4
    TilePlacedObj *aPlaneBSlotsMaybe[7]; // +0x20
    // SIGNED (both): -1 is the "this plane is empty" resting value -- FlushQueuedBigObjMaybe's
    // per-cell scans start at the count and run down while `>= 0`, and its compaction step
    // decrements past 0 to -1 when the last slot empties. The original's `test bl,bl / jge` and
    // its 8-bit `cmp al,bl` against a `char` slot index both need these signed.
    char bPlaneBCountMaybe;              // +0x3c
    char bPlaneACountMaybe;              // +0x3d
    unsigned char pad0x3e[2];
};

struct WorldBoardPartial {
    unsigned char pad0x0[4];
    int dwViewportWidth;  // +0x4 -- src/WidgetPicker.cpp
    int dwViewportHeightMaybe; // +0x8 -- src/NameAnchorMaybe.cpp (AdvanceAlongTrackMaybe's
                               //   viewport-extend bound checks)
    RECT rcViewport;      // +0xc..+0x1c -- merged from 4 loose ints (dwUnk0xcMaybe/
                                // dwUnk0x10Maybe/dwGridWidthCopyMaybe/dwGridHeightCopyMaybe in
                                // the canonical Ghidra struct) once TWO independent consumers
                                // agreed it's really one RECT: LocoBitmap::Resize casts
                                // &dwUnk0xcMaybe directly to RECT* at two call sites
                                // (IntersectRect/CenterRectInRect), and TutorialWnd::Launch
                                // passes the same 4 ints as one by-value RECT argument to
                                // WorldBoardMaybe::MarkRectDirty. +0xc/+0x10 are always 0 (zeroed
                                // by Ddraw_InitTileGridExtent, never written elsewhere), so this
                                // is really {0,0,width,height} -- the full viewport rect.
    int dwScrollX;        // +0x1c -- current horizontal board-scroll offset (screen-space);
                                // PopupWndBase::RedrawSoftwareCursorOverBoard offsets its
                                // cached screen-bounds rect by this pair when a global "on map"
                                // flag is set.
    int dwScrollY;        // +0x20 -- see dwScrollX.
    int dwHalfWidth;      // +0x24 -- src/DSound.cpp
    int dwHalfHeight;     // +0x28 -- src/DSound.cpp
    int dwViewportCenterXMaybe; // +0x2c -- viewport-center x, read by AnimEffectObj0x477a90's
                                //   'C' placement mode (src/AnimEffectObj.cpp; Ghidra name)
    int dwViewportCenterYMaybe; // +0x30 -- see dwViewportCenterXMaybe
    // +0x34/+0x38 -- the SCREEN-pixel position of the last click ResolveWorldClickMaybe (0x455d60)
    // resolved while the bulldoze tool was down, together with bFlag0x3cMaybe below: the next
    // click more than 16 pixels away in either axis is treated as a DRAG, and everything on the
    // segment between the two points is erased (0x455960) rather than just the tile clicked.
    int dwLastBulldozeXMaybe;
    int dwLastBulldozeYMaybe;
    bool bFlag0x3cMaybe; // +0x3c -- "the pair above holds a live bulldoze anchor". Raised by
                         // ResolveWorldClickMaybe on every bulldoze click; cleared by
                         // ResetFlag0x3c (0x456140, below), whose only callers are
                         // PlacementCursorMaybe::TickMaybe's pending-placement commit paths --
                         // i.e. releasing the button ends the drag.
    unsigned char pad0x3d[0x3e - 0x3d];
    short wCols; // +0x3e -- board grid column count (copied into each provider slot
                 //          by DPlaySessionMgr::ResetProviders). SIGNED: the edge-placement
                 //          quartet (0x41d8f0..0x41d980, src/NetSessionEventQueue.cpp)
                 //          divides these by 2 and the original emits `sar`, not `shr`.
    short wRows; // +0x40 -- board grid row count (see wCols)
    unsigned char pad0x42[2]; // +0x42 -- alignment in front of the DWORD-aligned grid below
    // +0x44 .. +0x52483 -- the board tile array, see BOARD_TILE_COLS/BOARD_TILE_ROWS above.
    BoardTileCellMaybe aTileCellsMaybe[BOARD_TILE_COLS * BOARD_TILE_ROWS];
    unsigned char *pDirtyBitmapMaybe; // +0x52484 -- packed 1-bit-per-tile dirty bitmap
                                      // (bit row*wCols+col, mask table DAT_0047f108), allocated
                                      // by Ddraw_InitTileGridExtent; read + cleared by
                                      // UpdateDirtyTiles (0x456150)
    // +0x52488/+0x5248c -- the two node/edge graphs built over the placed tiles (src/
    // TrackGraph.h). Graph A is the one the ambient actors path across
    // (WalkerActor::ChooseNextStepTileMaybe, src/WalkerActor.cpp); both are (re)built by
    // BuildTrackGraphsIfReadyMaybe, which is also what raises bTrackGraphsBuiltFlag below.
    TrackGraph *pTrackGraphAMaybe;
    TrackGraph *pTrackGraphBMaybe;
    // +0x52490 -- byte 0 (bTrackGraphsBuiltFlag) is the REAL "both track graphs built" flag:
    // written 0 by FUN_00457320 (force-rebuild entry) and 1 by WorldBoardMaybe::
    // BuildTrackGraphsIfReadyMaybe (0x4573cf, not yet transcribed); read by
    // WorldIdleEventPumpThreadProc (0x42cc6e) plus 3 more untranscribed sites. Byte 1
    // (bBoardDirtyNeedsRebuildFlag) is a SEPARATE, independently-written flag -- CORRECTED
    // 2026-07-20 (was previously misnamed bTrackGraphsBuiltFlag and believed to be THE
    // track-graphs flag): BuildTool_SetAutoCurveConnectModeMaybe (0x4089d0) sets it entering
    // curve-connect draw mode, BuildToolButton::OnPressReleaseMaybe reads it on button-release
    // to decide whether to autosave, AppWindow_SetScreenState's case 4 clears it, and
    // LoadLayoutAndPopulateBoard (src/NetSessionEventQueue.cpp) sets it after a bulk board
    // load -- real semantic is "board dirty, needs rebuild+autosave". A genuine byte-level
    // alias of one dword (see DPlaySessionMgrProviderSlot's dwTailAliasMaybe for the same
    // pattern), so modeled as a union rather than two separately-offset fields.
    union {
        unsigned int dwFlagBytesAlias;  // +0x52490
        struct {
            unsigned char bTrackGraphsBuiltFlag;      // +0x52490
            unsigned char bBoardDirtyNeedsRebuildFlag; // +0x52491
            unsigned char bUnk0x52492;
            unsigned char bUnk0x52493;
        };
    };
    unsigned int aSurfaceDescScratch[31]; // +0x52494 -- a cached DDSurfaceDescPadded0x7c-shaped
                                        // scratch reused across calls (indices [4]/[9] are
                                        // exactly that struct's lPitch/lpSurface dword offsets)
    unsigned char bSurfaceLockGuard; // +0x52510 -- guard flag: the shared back-buffer
                                            // surface stays Locked between calls instead of
                                            // Lock/Unlock-ing every time, keyed off this flag

    // 0x456140 -- clears bFlag0x3cMaybe (+0x3c), ignores both stack args, returns false.
    // Defined in src/WorldBoardMaybe.cpp.
    bool ResetFlag0x3c(int, int);
    // Dirty-marks a rect region (0x455840, see docs/subsystems.md's WorldBoardMaybe entry).
    // Not yet transcribed.
    void MarkRectDirty(RECT rect);
    // 0x457320 -- the force-rebuild entry: clears bTrackGraphsBuiltFlag above so the next pass
    // through BuildTrackGraphsIfReadyMaybe rebuilds both track graphs from scratch. Called from
    // the build-mode entry path (AppWindow_EnterBuildMode) both when the world has just finished
    // loading and whenever bBoardDirtyNeedsRebuildFlag is up. Declared-only -- and see
    // UpdateDirtyTiles' note below on why declaring untranscribed siblings here is measured, not
    // free (v334 bisect). ⚠ This one is NOT free: adding it rotates src/WalkerActor.cpp and costs
    // WalkerActor::UpdateFacingFromStepDeltaMaybe (0x4331b0, 112 B) its exact match. Paid for by
    // swapping that function's two local declarations -- see its own in-source note. Verified in
    // v423 that the sibling declarations added to src/DecorObjMgrMaybe.h and
    // src/PeerTrainSlotQueueMaybe.h flip the same bit INDEPENDENTLY, so the parity is over the
    // TOTAL declaration count across the TU's headers, not over any one header.
    void FUN_00457320(); // TODO: idiom
    // Dirty-tile-rect coalescing + DDraw blit (0x456150, see docs/subsystems.md's
    // WorldBoardMaybe entry). Defined in src/WorldBoardMaybe.cpp (PARKED EFFECTIVE --
    // see the body comment/docs/PARKED.md; its untranscribed sibling callees
    // 0x456700/0x456c60/0x456d10 are TU-local methods-only views there, NOT declared
    // here -- declaring them in this header rotates DPlaySessionMgr.cpp, v334 bisect).
    void UpdateDirtyTiles(char bParam);
    // 0x457080 -- render the current board into the supplied LocoBitmap (as a raw 8bpp capture).
    // Used by DPlaySessionMgr::LayoutNet_SendCurrentLayoutBitmap to snapshot the live board for
    // the layout-bitmap network message, and by src/ThumbnailBmp.cpp for the save-game
    // thumbnail. Defined in src/WorldBoardMaybe.cpp. BOOL-sized return: the original's
    // epilogue is a bare `mov al,1` with the upper EAX bits left over from the row counter,
    // which only a `bool`/`char` return type emits. EFFECTIVE MATCH (two cross-jumped tail
    // pairs) -- see the body comment/docs/PARKED.md.
    bool CaptureBoardToBitmap(void *pBitmap, unsigned char bParam);

    // 0x4550c0 -- place a new tile object of the given kind at tile (col, row), stamping it
    // into every board cell its two occupancy tables claim. Defined in src/WorldBoardMaybe.cpp.
    // ⚠ Returns TilePlacedObjPartial*, not TilePlacedObj*, purely so src/NetSessionEventQueue.cpp
    // can keep holding the result in its own partial view -- the object really is a TilePlacedObj
    // and the definition casts on the way out.
    TilePlacedObjPartial *PlaceObject(unsigned int kindId, short col, short row,
                                      char bAlreadyHotspotAdjustedMaybe, unsigned int arg5);
    // 0x4553e0 -- PlaceObject's footprint legality pass: walks the same physical-footprint mask
    // and reports whether every tile it wants is free. bClearMaybe==0 is the pure test; ==1 also
    // EVICTS whatever it finds (a sound plus FlushQueuedBigObjMaybe per displaced object), which
    // is why PlaceObject runs it twice -- once to ask, once to take. Defined in
    // src/WorldBoardMaybe.cpp.
    char TestOrClearFootprintMaybe(char bClearMaybe, BigObj *pDesc, short col, short row,
                                   unsigned int arg5);
    // 0x455ab0 -- PlaceObject's exact inverse: clears pItem out of every board cell either of
    // its kind's two occupancy tables claimed, compacts both planes' high-water marks back down,
    // then hands the object to NetSessionEventQueue::BigObj_Remove. Returns 0 without touching
    // anything if pItem is null/not saveable/not valid. Defined in src/WorldBoardMaybe.cpp.
    char FlushQueuedBigObjMaybe(TilePlacedObj *pItem, unsigned int bSpawnEffect);
    // 0x455960 -- the BULLDOZE DRAG erase the dwLastBulldozeXMaybe/bFlag0x3cMaybe trio above
    // exists to drive: wipe everything on the segment between the previous bulldoze anchor and
    // the current point, rather than just the tile clicked. Declared only, and declared here
    // deliberately -- it is a real member the notes above already referenced, and adding it is
    // how this session paid the declaration-count parity back after transcribing the track-graph
    // cluster (see the header comment; measured, not assumed). Return type is Ghidra's
    // `undefined4` and has not been confirmed against the body.
    int EraseAlongDragSegmentMaybe(int x, int y, int xPrev, int yPrev);
    // 0x454fe0 -- reset every board tile. Not yet transcribed.
    void ResetAllTiles();
    // 0x457380 -- the "both track graphs built" step the header's flag notes above refer to:
    // while the app is in screen state 3, asks each of the two TrackGraphs that has any nodes at
    // all to rebuild all its routes, then raises bTrackGraphsBuiltFlag. Re-checks the screen
    // state between the two graphs, so a state change during graph A's rebuild leaves the flag
    // down and graph B untouched. Not yet transcribed.
    void BuildTrackGraphsIfReadyMaybe();
    // 0x4579d0 -- the placed-object pointer at the board tile adjacent to pItem in one of 4
    // directions (0-3), or NULL if there's no neighbor/pItem has no valid kind descriptor.
    // Large (0x174-byte), not independently transcribed -- see BigObj_Remove's own use.
    TilePlacedObjPartial *GetNeighborObject(TilePlacedObjPartial *pItem, int direction);
    // 0x454e60 (Ghidra: Ddraw::Ddraw_InitTileGridExtent) -- (re)derives the board's tile
    // grid extent and zeroes rcViewport's origin. Transcribed v510 (EFFECTIVE, DIFF(4) --
    // see the definition).
    void Ddraw_InitTileGridExtent(char bFlag);
    // 0x455620, defined in src/WorldBoardMaybe.cpp -- the plane-A slot iSlot (the
    // TilePlacedObj-family object placed at grid (x, y)), or NULL. src/PeerTrainNode.cpp
    // declares this same method on its own TU-local WorldBoardSlotPartial view with a
    // TrackConnectorTileObj* return -- same function, just pre-narrowed to the leaf that TU
    // uses. iSlot is a SHORT, not an int: the original indexes straight off the incoming stack
    // slot with `movsx edx, WORD PTR [esp+0x10]`.
    TilePlacedObj *GetPlaneASlotMaybe(short x, short y, short iSlot);
    // ⭐ The three below moved here from TWO duplicate TU-local views in v576, and moving them
    // was a CORRECTNESS fix, not tidying. They were declared on `WorldBoardClickView0x411000`
    // in src/PlacementCursorMaybe.cpp and DEFINED on `WorldBoardClickView0x455d60` in
    // src/WorldBoardMaybe.cpp -- two different spellings of one address, so every call from the
    // placement cursor targeted a symbol nothing defines. That is invisible to the byte-match
    // (relocations are masked) and to lint_ghidra_sync (both spellings resolve), and it is the
    // exact class tools/lint_alias.py exists for; only the PORT's runtime stub log caught it,
    // with all three answering from generated stubs -- which is why clicking to place an object
    // did nothing at all. The v407 note that kept them TU-local priced the move at -124 B
    // (LocoBitmap::Fill); re-measured from a clean v576 baseline it is BYTE-FREE. See
    // CODEGEN #214.
    //
    // 0x455d60 -- the board's own fallback click handler, reached only once every widget and
    // the ambient-actor manager have all declined the click. Returns whether it consumed the
    // click (CommitPendingRotateMaybe branches on it; CommitPendingCoupleMaybe discards it).
    char ResolveWorldClickMaybe(int x, int y);
    // 0x4556f0 -- the topmost plane-B object under the SCREEN pixel (x, y), or NULL; the
    // pixel-keyed companion of the grid-keyed GetPlaneASlotMaybe above.
    TilePlacedObj *GetPlaneBTopSlotAtPixelMaybe(int x, int y);
    // 0x455670 -- the TOPMOST occupied plane-A slot at grid (x, y): backward-scans the cell's
    // 7 slots and returns both the object and, via pOutIndex, the slot it sat in. NULL (and
    // pOutIndex untouched) when the cell is empty or (x, y) is off-board.
    TilePlacedObj *GetTopPlaneBSlotMaybe(short x, short y, short *pOutIndex);
    // 0x457ce0, extern -- the nearest placed object whose kind descriptor's own categoryByte
    // equals nCategory, searched outward in rings from the tile containing (x, y) and bounded
    // by nSearchRangeMaybe; NULL if none. Not yet transcribed.
    TilePlacedObj *FindNearestObjOfCategoryMaybe(unsigned short nCategory, int x, int y,
                                                 int nSearchRangeMaybe);
    // 0x456d90 -- "does this object's required ring of neighbours hold where it now stands?",
    // the placement precondition its kind descriptor's InsertSeq record encodes. Walks the
    // one-tile-wide perimeter around the object's footprint clockwise, consuming one InsertSeq
    // entry per tile. Defined in src/WorldBoardMaybe.cpp.
    unsigned char CheckInsertSeqPerimeterMaybe(TilePlacedObj *pObj);
    // 0x4573e0 / 0x4576b0, extern -- "is pTile a JUNCTION of the graph-A / graph-B network",
    // i.e. does it deserve a TrackGraphNode of its own rather than being a plain run of
    // path/road between two junctions. Both walk the tile's four neighbour directions THROUGH
    // any intervening plain runs and count the branches that survive; the A form additionally
    // takes the graph's own category selector so that a 4-way crossing of selector 7 can
    // discount its diagonal neighbours -- B accepts that selector too (both callers pass the
    // TrackGraph's own nGraphKindMaybe) but never reads it. Nonzero = node.
    char IsTrackGraphNodeAMaybe(TilePlacedObj *pTile, short nCategoryMaybe);
    char IsTrackGraphNodeBMaybe(TilePlacedObj *pTile, short nCategoryMaybe);
    // 0x457830 / 0x457900, extern -- fill pTile's four graph-A / graph-B neighbour slots
    // (TilePlacedObj::apNeighbourTileAMaybe/anNeighbourCostAMaybe and their B twins) by walking
    // each direction until the next junction, accumulating the run's cost on the way. Not yet
    // transcribed.
    void LinkTrackGraphNeighboursAMaybe(TilePlacedObj *pTile);
    void LinkTrackGraphNeighboursBMaybe(TilePlacedObj *pTile);
    // 0x457b60 -- the track-graph EDGE COST between two adjacent tile kinds, or -1 when the two
    // do not connect at all: the gate every one of the four graph walkers above runs before
    // accepting a neighbour, and the per-step weight LinkTrackGraphNeighbours*Maybe accumulates
    // along a run. Path-to-path costs 50, track-to-track 10, a recognised level crossing 10.
    // A MEMBER, not the free __stdcall function it was transcribed as: it never reads `this`, so
    // its own body is byte-identical either way (`ret 0x8` both times), but
    // IsTrackGraphNodeAMaybe's call site loads ecx with the enclosing board's `this` before
    // calling it -- which only a __thiscall member does. Defined in src/WorldBoardMaybe.cpp.
    int TrackGraph_GetLinkCostAMaybe(BigObj *pKindA, BigObj *pKindB);
    // 0x457c20 -- the same thing for graph B, the ROAD network. Every accepted edge costs a flat
    // 100 there (graph B has no rail-versus-foot preference to express), so it is a pure
    // connectivity table rather than a weighting. Also a member that never reads `this`.
    int TrackGraph_GetLinkCostBMaybe(BigObj *pKindA, BigObj *pKindB);
    // 0x454fa0 (Ghidra: Ddraw::Ddraw_RecenterViewportOffsetMaybe) -- re-derive just the viewport
    // CENTRE, without disturbing the scroll offset; what the scrollers and the display-mode
    // switch call after moving the view. Body in src/WorldBoardMaybe.cpp.
    // ⚠ PRICE, v577: this ONE declaration costs -484 B / -1 func (it flips this header's
    // declaration counter -- see the warning at the top). It is landed anyway. Until v577 the
    // address had THREE view-struct spellings (WorldBoardRecenterView0x454fa0 here,
    // src/AppWindow.cpp's WorldBoardDisplayModeView0x407d20, src/Main.cpp's
    // WorldBoardWndProcView0x4618c0), only one of which was ever DEFINED, so the other two TUs'
    // five call sites targeted symbols no TU defines and ran gen_stubs stubs in the port. A
    // single shared view struct in this header was measured byte-FREE and would also have fixed
    // the link, but a view struct is decompilation scaffolding, not what the original declared:
    // 0x454fa0 is __thiscall on the board, i.e. an ordinary member. Correct model wins.
    void Ddraw_RecenterViewportOffsetMaybe();
};

extern "C" extern WorldBoardPartial g_worldBoard; // DAT_004aad08
