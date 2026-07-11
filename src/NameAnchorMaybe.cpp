// NameAnchorMaybe method bodies. See the header / docs/subsystems.md for the class.
#include "NameAnchorMaybe.h"
#include "TilePlacedObj.h"    // TrackTileObj + TrackConnectorTileObj/TrackDepotTileObj leaves
                              //   (brings in WidgetBase.h + CursorDesc.h's BigObj)
#include "PeerTrainNode.h"    // PeerTrainNodePartial
#include "WorldBoardMaybe.h"  // g_worldBoard (dwViewportWidth/dwViewportHeightMaybe)
#include "CarNetObj.h"        // CarNetObj

// TU-local view adding GetReversalTargetTileMaybe (0x44d4c0, src/PeerTrainNode.cpp) on top of
// PeerTrainNodeTrackPartial's own 3 methods -- AdvanceCarAnchorMaybe's only extra need beyond
// AdvanceAlongTrackMaybe's own param_1 surface.
struct PeerTrainNodeCarAdvancePartial : PeerTrainNodeTrackPartial {
    TrackTileObj *GetReversalTargetTileMaybe(); // 0x44d4c0, extern
};

// DecorObjMgrMaybe::TestRectAgainstAllActorsMaybe (0x435020, EXACT in src/DecorActor.cpp) --
// "is any ambient actor standing in this rect", answering 0 for clear or the blocking actor's
// own TileKind category (7 = walker, 8 = road vehicle). The hardcoded 0x485448 it is always
// called with is the DecorObjMgrMaybe singleton, and the by-value 16-byte argument is the
// tile's own inherited RectFlagObj0x477820::rect.
//
// CORRECTED 2026-07-26 (v415): docs/subsystems.md's v75 reading of this callee -- "a shared
// flush/sync-the-display-surface widget-subtree sweep whose body unconditionally returns 0" --
// was a Ghidra by-value-RECT framing artifact, and the `!= 0` arms below are NOT dead. They are
// what makes a train stop at a level crossing that a minifig or a car is standing on.
//
// TODO: idiom -- still declared TU-locally with a mimicking __fastcall signature rather than
// called as the real __thiscall member on DecorObjMgrMaybe_00485448. Retyping it means pulling
// src/DecorObjMgrMaybe.h into this TU, which is a measured change (this TU's /Og state has moved
// on an include before); see the pickup block in CLAUDE.md.
extern int __fastcall TestRectAgainstAllActorsMaybe(void *pDecorObjMgr, RECT rect);

// TileKind_GetCategory, 0x446030 (same decl as src/DecorActor.cpp / src/WorldBoardMaybe.cpp /
// src/Obj0x4779e0.cpp / src/Main.cpp) -- the kind id's coarse family; 3 = the track family.
extern unsigned int __cdecl TileKind_GetCategory(unsigned int kindId);

// Methods-only view of g_worldBoard for the one extra entry point TryMatchSocketMaybe needs.
// Deliberately NOT declared on the shared WorldBoardPartial in src/WorldBoardMaybe.h: its
// TileGridPos return would drag src/TilePlacedObj.h into that header, and the DECLARATION COUNT
// of these shared headers is itself load-bearing for other TUs' codegen (see that header's own
// UpdateDirtyTiles/FUN_00457320 notes and the v423 parity finding). Reached by casting the
// shared extern rather than declaring a second one, so there is still exactly one `g_worldBoard`
// symbol in this TU -- src/PlacementCursorMaybe.cpp's WorldBoardClickView0x411000 precedent.
struct WorldBoardSlotOriginView0x40b880 {
    // 0x4557c0, extern -- the GRID POSITION of whatever plane-A object occupies grid (x, y),
    // i.e. that object's own anchor/origin tile rather than the queried cell (a multi-tile
    // object answers the same origin from every cell it covers). Returns (-1, -1) when the cell
    // is empty or off-board. Returned BY VALUE: at 4 bytes with a nontrivial class this compiles
    // to the hidden-return-pointer convention, which is exactly what the call site emits.
    TileGridPos GetPlaneASlotGridPosMaybe(short x, short y, short plane);
};

// Same story for NameAnchorMaybe_ResolveTileAnchorMaybe's own extra board entry point, kept as
// its own consumer-named view for the same reason.
struct WorldBoardOccupantPosView0x40b740 : WorldBoardPartial {
    // 0x455740, DEFINED BELOW -- the grid position of the PLANE-B object occupying grid (x, y),
    // or (-1, -1) when that slot is empty or the cell is off-board. Plane-A's grid-keyed twin is
    // GetPlaneASlotGridPosMaybe above; returned by value under the same hidden-return-pointer
    // convention.
    TileGridPos GetPlaneBOccupantPositionMaybe(short x, short y, short iSlot);
};

// FUNCTION: LOCO 0x455740
// The PLANE-B twin of GetPlaneASlotGridPosMaybe (0x4557c0, defined in src/WorldBoardMaybe.cpp):
// where does the plane-B object in slot iSlot at grid (x, y) actually stand? (-1, -1) means the
// slot is empty or the tile is off-board.
//
// The two are byte-identical in the original except for ONE byte -- the scaled-index
// displacement, `+0x64` here against `+0x48` there, i.e. the cell's +0x20 plane-B array against
// its +0x4 plane-A one. Everything else, down to the register allocation and both epilogues,
// agrees instruction for instruction. That is what settles the long-open question of whether
// this name and 0x455670's were SWAPPED: they were not. This pair is a genuine plane-A/plane-B
// twin with both names already correct, and the only wrong name in the group was 0x455670's
// (it says plane A and reads plane B), corrected this session.
//
// ⚠ Defined HERE rather than beside its twin in src/WorldBoardMaybe.cpp, and that is measured,
// not stylistic. Landing this body in that TU costs WorldBoardMaybe_ResetAllTilesMaybe (0x454fe0)
// its whole 211-byte exact -- the v479 definition-count parity class that function's own note
// describes, re-rolled by any new body in the TU. The declaration alone there was free; the
// DEFINITION was the trigger (bisected). This TU already declared the method on this view and
// already calls it, so defining it here adds no declaration at all and lands it for free.
//
// sic: the slot is read BEFORE the bounds are checked, so an off-board (x, y) indexes the cell
// array out of range and only then has its result discarded. The `pObj = 0` / re-test pair is
// the original's own shape -- the bounds failure nulls the pointer rather than branching to the
// (-1, -1) return, which is why there are two consecutive null tests on the same variable.
TileGridPos WorldBoardOccupantPosView0x40b740::GetPlaneBOccupantPositionMaybe( // TODO: sync (TU-local view)
    short x, short y, short iSlot)
{
    TileGridPos pos;
    TilePlacedObj *pObj;

    pos.wPosX = -1;
    pos.wPosY = -1;
    pObj = this->aTileCellsMaybe[x * BOARD_TILE_ROWS + y].aPlaneBSlotsMaybe[iSlot];
    if (pObj != 0) {
        if (x < 0 || x > BOARD_TILE_COLS || y < 0 || y > BOARD_TILE_ROWS) {
            pObj = 0;
        }
        if (pObj != 0) {
            return pObj->pos;
        }
    }
    return pos;
}

// WORLD_TO_TILE, the project's standard pixel-to-tile-grid `< 0 ? -1 : >> 4` conversion, now
// lives in its documented home instead of in a TU-local copy here (2026-07-27).
#include "GeomUtil.h"

// This class's own single-slot vtable (dtor only) -- see the pVtbl note in the header for why
// the vptr is stored by hand rather than by a real virtual dtor.
extern void *g_vtable0x477564[];

// FUNCTION: LOCO 0x40b500 // EFFECTIVE MATCH -- 18/18 instructions, asmscore total 22343,
// byte_diff 13/66. Every store, its order and the branch polarity are right; the residual is one
// scheduling choice plus the register coin-flip it drags along: the original materializes -1 and
// 0 into edx/ecx FIRST and only then loads the bPeerOwned byte (mid-body, at 0x40b50d), while our
// compile loads the parameter into dl at offset 0 and folds -1 and 0 through a single reused ecx.
// Probed: duplicating the six common stores into BOTH arms of the if (so the parameter's first
// use moves to the top) -- byte-identical DIFF(17), no effect. Same intrinsic ctor
// constant-materialization/scheduling class as BuildToolCursorWnd's own ctor. See docs/PARKED.md.
//
// Seats a fresh anchor: unresolved anchor point (-1, -1), no tile, forward socket-step
// direction. bPeerOwned picks between the two mutually-exclusive extend-phase trackers,
// arming exactly one of them at phase 2 and idling the other -- the same 0/2 vs 2/0 split the
// owning CarNetObj/PeerTrainNode carries in its own dwModeAMaybe/dwModeBMaybe pair.
NameAnchorMaybe::NameAnchorMaybe(char bPeerOwned)
{
    nAnchorXMaybe = -1;
    nAnchorYMaybe = -1;
    pVtbl = g_vtable0x477564;
    pTileMaybe = NULL;
    dwUnk0x04 = 1;
    nSocketIndexMaybe = 0;
    if (bPeerOwned == 0) {
        dwViewportExtendPhaseMaybe = 0;
        dwTileExtendPhaseMaybe = 2;
    } else {
        dwViewportExtendPhaseMaybe = 2;
        dwTileExtendPhaseMaybe = 0;
    }
}

extern int g_nScreenState;              // app-state dword, see src/GameNetMsgQueue.h -- declared
                                        // file-locally the same way src/MailWnd.cpp does, rather
                                        // than pulling that whole header into this TU.

// The shared shutdown predicate, TU-local by the documented rule (docs/CODEGEN.md's
// sete-materialized-branch entry): it is the `xor/cmp/sete/test` shape in the guard below that
// pins it as an `unsigned char` inline rather than an open-coded `g_nScreenState == 10`.
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// FUNCTION: LOCO 0x40b5a0
// Give up this anchor's claim on its tile. The shutdown guard is the point: at g_nScreenState 10
// the tiles are already being torn down, so decrementing an occupant count through pTileMaybe
// would be a write into a freed object. The anchor still forgets the tile in the normal case --
// but during shutdown it does not even do that, because nobody will read it again.
void NameAnchorMaybe::ReleaseTileClaimMaybe()
{
    if (pTileMaybe != NULL && !IsNetShuttingDownMaybe()) {
        pTileMaybe->nOccupantRefCount--;
        pTileMaybe = NULL;
    }
}

// FUNCTION: LOCO 0x40b5d0
// Ex-CopyObj0x40b5d0::CopyTailFrom (phase2_probe5.cpp) -- owner resolved 2026-07-22 (v322)
// per docs/subsystems.md's v72 finding: CopyObj0x40b5d0 was a Phase-2-era duplicate model of
// NameAnchorMaybe, and this is NameAnchorMaybe::CopyTailFrom. Called from
// PeerTrainNode::ReverseDirectionMaybe to re-derive the train's tail-position reference from
// the car at the new leading end. Copies the 7 trailing fields, deliberately skipping field
// 0 (pVtbl) -- a partial copy-assignment that preserves the object's own vtable/identity.
void NameAnchorMaybe::CopyTailFrom(NameAnchorMaybe *src) {
    dwUnk0x04 = src->dwUnk0x04;
    nSocketIndexMaybe = src->nSocketIndexMaybe;
    nAnchorXMaybe = src->nAnchorXMaybe;
    nAnchorYMaybe = src->nAnchorYMaybe;
    pTileMaybe = src->pTileMaybe;
    dwViewportExtendPhaseMaybe = src->dwViewportExtendPhaseMaybe;
    dwTileExtendPhaseMaybe = src->dwTileExtendPhaseMaybe;
}

// FUNCTION: LOCO 0x40b610
// Re-derive which socket of the anchor's CURRENT tile a world-pixel point sits on, and snap the
// matching anchor coordinate onto it.
//
// The socket table is a packed {short x; short y;} run of tile-RELATIVE offsets, and a track
// tile's sockets always march along ONE axis -- so exactly one of the point's two coordinates
// carries the information. The leading test picks that axis: if the point's X already agrees
// with socket 0's X, the sockets differ in Y and the Y table column is scanned; otherwise the X
// column is. A point that agrees with socket 0 on BOTH axes carries no information at all and
// answers 0 without touching anything (it is already exactly socket 0, which the callers treat
// as "nothing to re-derive"), as does a tile whose kind declares no sockets.
//
// Only the scanned axis' anchor coordinate is written back -- the other one is already correct
// by construction, which is the whole reason the axis test exists.
unsigned char NameAnchorMaybe::ResolveSocketIndexAtPointMaybe(int nX, int nY)
{
    unsigned char bResolved = 0;
    TrackTileObj *pTile = this->pTileMaybe;
    if (pTile == NULL) {
        return 0;
    }

    BigObj *pKind = pTile->pKindDesc;
    short *pSockets = pKind->pSocketTable;
    if (nX != pSockets[0] + ((short)pTile->pos.wPosX << 4)) {
        int nRelX = nX - ((short)pTile->pos.wPosX << 4);
        for (unsigned int i = 0; i < pKind->wSocketCount; i++) {
            if (nRelX == pSockets[i * 2]) {
                this->nSocketIndexMaybe = i;
                this->nAnchorXMaybe =
                    pKind->pSocketTable[i * 2] + ((short)pTile->pos.wPosX << 4);
                bResolved = 1;
                break;
            }
        }
    } else if (nY != pSockets[1] + ((short)pTile->pos.wPosY << 4)) {
        int nRelY = nY - ((short)pTile->pos.wPosY << 4);
        for (unsigned int i = 0; i < pKind->wSocketCount; i++) {
            if (nRelY == pSockets[i * 2 + 1]) {
                this->nSocketIndexMaybe = i;
                this->nAnchorYMaybe =
                    pKind->pSocketTable[i * 2 + 1] + ((short)pTile->pos.wPosY << 4);
                bResolved = 1;
                break;
            }
        }
    }
    return bResolved;
}

// FUNCTION: LOCO 0x40b740 // EFFECTIVE MATCH -- 113/113 instructions at the original's exact
// 312-byte length, asmscore total 442, byte_diff 2. Every instruction, register and operand
// agrees except the SIB base/index ORDER on the function's last two loads: the original encodes
// `movsx ecx, [eax + ecx]` / `movsx eax, [eax + ecx + 2]` (the byte offset as base, the table
// pointer as index) where this compile encodes the same two registers the other way round. Both
// encodings are scale-1 and semantically identical; only the SIB byte differs. Note the loop's
// own `[ebx + ebp*4]` table access two blocks earlier matches EXACTLY, so this is not a
// modelling error about the table -- it is the scale-1 base/index coin flip that appears once
// VC5 materializes the byte offset into its own register (the index is used twice here, so the
// `* 2` is CSE'd out of the addressing mode; inside the loop it stays folded as a scale).
// Probed one compile at a time, all byte-neutral or worse: hoisting the doubled index into an
// `int nSocketOffset` local (DIFF(43), strictly worse -- it also stops the reload of
// pSocketTable the original issues twice); swapping the two addition operands so the tile base
// comes first (byte-identical DIFF(2)); writing the index as `2 * n` instead of `n * 2`
// (byte-identical DIFF(2)). See docs/PARKED.md.
//
// Seat this anchor on whatever track tile lies under a WORLD-PIXEL point, then derive the
// anchor's socket index and snapped anchor point from that tile's socket table.
//
// Fails (answering 0, with pTileMaybe left pointing at whatever the lookup found) when the cell
// is empty or holds a non-track object -- TileKind_GetCategory == 3 is the track family. On
// success the anchor always steps FORWARD (dwUnk0x04 = 1), and both anchor coordinates are
// rewritten from the resolved socket, unlike ResolveSocketIndexAtPointMaybe's single-axis
// update.
//
// ⚠ Two deliberate oddities, both faithful:
//   * nUnused is a real third stack slot (`ret 0xc`) the body never reads.
//   * the tile's OWN grid position is overwritten from the plane-B occupant's position at the
//     same cell before the socket math runs -- see the note in docs/engine-bugs.md.
unsigned char NameAnchorMaybe::NameAnchorMaybe_ResolveTileAnchorMaybe(int nX, int nY, int nUnused)
{
    this->pTileMaybe =
        (TrackTileObj *)g_worldBoard.GetPlaneASlotMaybe(WORLD_TO_TILE(nX), WORLD_TO_TILE(nY), 0);
    if (this->pTileMaybe == NULL) {
        return 0;
    }
    if ((unsigned char)TileKind_GetCategory(this->pTileMaybe->pKindDesc->resourceId) != 3) {
        return 0;
    }

    BigObj *pKind = this->pTileMaybe->pKindDesc;
    this->dwUnk0x04 = 1;
    // sic: re-seats the TILE's own grid position, not the anchor's.
    this->pTileMaybe->pos = ((WorldBoardOccupantPosView0x40b740 *)&g_worldBoard)
                                ->GetPlaneBOccupantPositionMaybe(WORLD_TO_TILE(nX),
                                                                 WORLD_TO_TILE(nY), 0);

    int nRelX = nX - ((short)this->pTileMaybe->pos.wPosX << 4);
    for (unsigned short i = 0; i < pKind->wSocketCount; i++) {
        if (nRelX == pKind->pSocketTable[i * 2]) {
            this->nSocketIndexMaybe = i;
            break;
        }
    }
    this->nAnchorXMaybe = pKind->pSocketTable[this->nSocketIndexMaybe * 2] +
                          ((short)this->pTileMaybe->pos.wPosX << 4);
    this->nAnchorYMaybe = pKind->pSocketTable[this->nSocketIndexMaybe * 2 + 1] +
                          ((short)this->pTileMaybe->pos.wPosY << 4);
    return 1;
}

// FUNCTION: LOCO 0x40b880
// Try to hand this anchor over to the track tile that sits under its CURRENT socket point.
//
// The anchor's socket point is the world-pixel position of socket nSocketIndexMaybe on its own
// tile. That point is converted to a board cell; the object occupying that cell answers its own
// ORIGIN, and the point is then tested against FOUR of that neighbour's sockets in turn -- its
// socket 1, its socket wSocketCount-1, its socket wSocketCount+2 and its socket
// wSocketCountExt-1 (the four ENDS of the neighbour's two socket chains, exactly the four
// indices BigObj::IsEndSocketIndexMaybe accepts). On a hit the anchor is re-seated: the old
// tile's occupant refcount drops, pTileMaybe becomes the neighbour, dwUnk0x04 is set to the
// step direction that walks AWAY from the matched end, nSocketIndexMaybe to that end's index,
// and the new tile's refcount rises.
//
// ⚠ RETURN POLARITY IS INVERTED from what the name suggests, and the callers depend on it:
// NULL means the hand-over SUCCEEDED, and a non-NULL tile means it did not -- either the
// neighbour that refused (the two extended-chain ends) or this anchor's own unchanged
// pTileMaybe (no cell, wrong category, or a points piece that refused). AdvanceAlongTrackMaybe's
// `matched != 0` arm is therefore the BLOCKED path, and it inspects that tile's dwTrackState to
// decide what blocked it.
//
// The two "chain ends coincide" guards (m_type0x63a == 0xb, a loop piece whose two chains close
// on the same point) exist so a closed loop does not hand the train straight back onto the chain
// it just left.
//
// PARKED (v432, first transcription). Structurally complete -- every block, branch, call and
// field access verified against the raw disasm over the full 0x40b880-0x40bbc3 span.
// `asmscore.py --len 836`: total 244937, align=234 reg_pen=95 identity_miss=118 byte_diff=257,
// insns 243/250 (ours 804 B vs the original's 836 B). Two source-shape findings were confirmed
// and are BAKED IN, each measured one compile at a time (total 616788 -> 244937):
//   (1) ⭐ The FOUR `return pNeighbour;` failure exits share ONE physical epilogue in the
//       original, reached by `jne`. Getting that requires the last socket block to be a NESTED
//       `if (wSocketCountExt != 0) { if (point matches) { ...; return 0; } }` that FALLS OUT to a
//       single trailing `return pNeighbour;` -- writing it as a flat `if (...) return pNeighbour;`
//       ladder makes VC5 emit an inline 6-instruction epilogue copy at every one of them
//       (616788 -> 361530, and 27 surplus instructions down to 8). The two `m_type0x63a == 0xb`
//       chain-end guards then have to be a single `if (A || B)` rather than two consecutive ifs
//       for the same reason (361530 -> 257497). The original still keeps TWO local duplicates of
//       that epilogue (0x40bace and 0x40bb73, both the fall-through of an inverted `je`) beside
//       the shared one at 0x40bbaf -- the documented VC5 "goto-sharing is LOCAL/trace-driven"
//       class, and this transcription now reproduces the same 3-copy count.
//   (2) `nY` is declared BEFORE `nX`, and `nOriginY` before `nOriginX` (257497 -> 244937). Local
//       declaration order is this compiler's tie-break for which value wins a callee-saved
//       register; same lever as WalkerActor::UpdateFacingFromStepDeltaMaybe's own swap.
// Tried and REFUTED (each strictly worse, reverted): spelling `pKind->pSocketTable` /
// `pKind->wSocketCountExt` out at every use instead of via the `pS`/`nExt` locals, to stop VC5
// CSE-ing the loads the original re-issues (270846); splitting `TileGridPos origin = f(...)` into
// a declaration plus an assignment, to reproduce the original's redundant copy-back of the
// hidden-return temp (265395); swapping nOriginX/nOriginY back after the nX/nY swap (248609).
// Swapping nTileX/nTileY is byte-neutral.
// Residual is a register-budget cascade, NOT a control-flow gap: the original pins `this` in esi
// and the neighbour in edi (this compile has them the other way round) and keeps nOriginX live in
// ebp for the whole body, paying for it by SPILLING nTileX and by re-loading pSocketTable /
// wSocketCountExt from memory at 5 sites that this compile CSEs into registers -- which is why
// ours is 7 instructions SHORTER than the original rather than longer. One callee-saved
// register's worth of budget, cascading; the same class as this TU's other three parks.
// Retry only if the VC5 /Og register-budget class cracks generally.
TrackTileObj *NameAnchorMaybe::TryMatchSocketMaybe()
{
    TrackTileObj *pTile = this->pTileMaybe;
    short *pSocket = &pTile->pKindDesc->pSocketTable[this->nSocketIndexMaybe * 2];
    // sic: nY before nX, and nOriginY before nOriginX below -- declaration order is load-bearing
    // for this function's register assignment, see finding (2) above.
    int nY = pSocket[1] + ((short)pTile->pos.wPosY << 4);
    int nX = pSocket[0] + ((short)pTile->pos.wPosX << 4);
    if (nX < 0) {
        return pTile;
    }
    if (nY < 0) {
        return pTile;
    }
    int nTileX = nX >> 4;
    int nTileY = nY >> 4;

    TileGridPos origin = ((WorldBoardSlotOriginView0x40b880 *)&g_worldBoard)
                             ->GetPlaneASlotGridPosMaybe((short)nTileX, (short)nTileY, 0);
    if ((short)origin.wPosX < 0) {
        return this->pTileMaybe;
    }
    int nOriginY = (short)origin.wPosY * 16;
    int nOriginX = (short)origin.wPosX * 16;

    TrackTileObj *pNeighbour =
        (TrackTileObj *)g_worldBoard.GetPlaneASlotMaybe((short)nTileX, (short)nTileY, 0);
    // sic: the category test dereferences pNeighbour before the null test that follows it --
    // reproduced in source order, see docs/engine-bugs.md.
    if ((unsigned char)TileKind_GetCategory(pNeighbour->pKindDesc->resourceId) != 3 ||
        pNeighbour == 0) {
        return this->pTileMaybe;
    }

    BigObj *pKind = pNeighbour->pKindDesc;
    if (pNeighbour->dwTrackTickState != 4 || pKind->m_type0x63a == 0xd) {
        // The neighbour's PRIMARY chain, near end (socket 1) and far end (socket
        // wSocketCount-1).
        short *pSockets = pKind->pSocketTable;
        if (nX == pSockets[2] + nOriginX && nY == pSockets[3] + nOriginY) {
            this->pTileMaybe->nOccupantRefCount--;
            this->pTileMaybe = pNeighbour;
            this->dwUnk0x04 = 1;
            this->nSocketIndexMaybe = 1;
            pNeighbour->nOccupantRefCount++;
            return 0;
        }
        short *pChainEnd = &pKind->pSocketTable[pKind->wSocketCount * 2];
        if (nX == pChainEnd[-2] + nOriginX && nY == pChainEnd[-1] + nOriginY) {
            if (pKind->m_type0x63a == 0xb && pNeighbour->dwTrackTickState == 4) {
                short *pS = pKind->pSocketTable;
                if (pS[0] != pS[pKind->wSocketCountExt * 2 + 2] ||
                    pS[1] != pS[pKind->wSocketCountExt * 2 + 3]) {
                    return this->pTileMaybe;
                }
            }
            this->pTileMaybe->nOccupantRefCount--;
            this->pTileMaybe = pNeighbour;
            this->dwUnk0x04 = 0;
            this->nSocketIndexMaybe = pKind->wSocketCount - 1;
            pNeighbour->nOccupantRefCount++;
            return 0;
        }
    }

    // The neighbour's SECOND (extended) chain, near end (socket wSocketCount+2).
    if (pKind->wSocketCountExt != 0 &&
        nX == pKind->pSocketTable[pKind->wSocketCount * 2 + 4] + nOriginX &&
        nY == pKind->pSocketTable[pKind->wSocketCount * 2 + 5] + nOriginY) {
        if (pKind->m_type0x63a == 0xb && pNeighbour->dwTrackTickState == 5) {
            short *pS = pKind->pSocketTable;
            if (pS[0] != pS[pKind->wSocketCountExt * 2 + 2] ||
                pS[1] != pS[pKind->wSocketCountExt * 2 + 3]) {
                return pNeighbour;
            }
        }
        this->pTileMaybe->nOccupantRefCount--;
        this->pTileMaybe = pNeighbour;
        this->dwUnk0x04 = 1;
        this->nSocketIndexMaybe = pKind->wSocketCount + 2;
        pNeighbour->nOccupantRefCount++;
        return 0;
    }

    // ...and that second chain's far end (socket wSocketCountExt-1). Note this re-tests
    // wSocketCountExt rather than nesting inside the block above -- the original's own
    // `je` from the previous guard lands on this test, not past it.
    if (pKind->wSocketCountExt != 0) {
        unsigned int nExt = pKind->wSocketCountExt;
        short *pS = pKind->pSocketTable;
        if (nX == pS[nExt * 2 - 2] + nOriginX && nY == pS[nExt * 2 - 1] + nOriginY) {
            if (pKind->m_type0x63a == 0xb && pNeighbour->dwTrackTickState == 5) {
                if (pS[0] != pS[nExt * 2 + 2] || pS[1] != pS[nExt * 2 + 3]) {
                    return pNeighbour;
                }
            }
            this->pTileMaybe->nOccupantRefCount--;
            this->pTileMaybe = pNeighbour;
            this->dwUnk0x04 = 0;
            this->nSocketIndexMaybe = pKind->wSocketCountExt - 1;
            pNeighbour->nOccupantRefCount++;
            return 0;
        }
    }
    return pNeighbour;
}

// FUNCTION: LOCO 0x40bbd0
// Advance this anchor one socket step along its pTileMaybe tile: try to match the next
// socket via TryMatchSocketMaybe (0x40b880), then branch on the matched neighbor tile's
// dwTrackState -- attach/detach, trigger PeerTrainNode's TryTransitionModeMaybe (direction
// reversal), or update PeerTrainNode's SetSoundStateMaybe; on no match, arm one of the two
// extend phases (dwViewportExtendPhaseMaybe viewport-side for kind-1..4 connector tiles, dwTileExtendPhaseMaybe tile-side
// for kind-7..0xa depot tiles). Returns a continue-flag the tick loop
// (PeerTrainNode's FUN_0044c3a0) and the reversal re-walk loop both consume. The original
// lays the matched-tile handling OUT OF LINE at the function's tail (the dispatch's
// `matched != 0` edge jump-threads straight there, past the entire main path), with the two
// extend-phase blocks between the main path and it; see docs/subsystems.md's
// NameAnchorMaybe entry for the phase/domain writeup.
// PARKED (v325): structure verified block-by-block against the raw disasm (every
// branch/call/field access; the NoMatch body is byte-identical to the original over its
// whole ~0x9a span). Residual: (1) an 8-byte guard block (`test matched,matched; je
// ContinueTick`) the original doesn't have -- VC5 keeps the second `if (matched == 0)`
// test instead of thread-eliminating it, and places the matched-tile handling right after
// it instead of at the function's tail; (2) downstream register/scheduling noise
// (CanReverse-fail park block out-lined vs inline-duplicated, bRet store-forwarding in the
// ViewportExtend cases). 12+ source-structure variants tried (plain goto, if/else-goto,
// double-if, ladder, label forms; VC5 probe-compiled at small scale to map its block-
// placement rules) -- the original's [dispatch, NoMatch, main path, extend, returns,
// matched-at-tail] layout was not reproducible: every construct that avoids the guard
// makes VC5 rotate the matched-handling up next to the dispatch. Same "block layout is
// trace-driven, mostly not source-steerable" class as docs/PARKED.md's DPlay_JoinOrHostSession
// entry. asmscore total 1919542 (align 1906, reg_pen 115, identity_miss 115, byte_diff 892,
// insns 682/655). Retry idea: revisit if the TU-context /Og layout class ever cracks
// (see the v323 0x45a880/0x45aa50 park note).
unsigned char NameAnchorMaybe::AdvanceAlongTrackMaybe(PeerTrainNodeTrackPartial *param_1) {
    TrackTileObj *tile = this->pTileMaybe;
    TrackTileObj *matched = 0;
    bool bNoSocketMatch = false;
    bool bRet = false;
    TrackTileObj *savedTile = tile;
    if (tile == 0 || tile->bValid != 1) {
        return false;
    }
    BigObj *kind = tile->pKindDesc;
    if (this->dwViewportExtendPhaseMaybe != 4 && this->dwViewportExtendPhaseMaybe != 2 &&
        this->dwTileExtendPhaseMaybe != 4 && this->dwTileExtendPhaseMaybe != 5) {
        if (kind->IsEndSocketIndexMaybe((short)this->nSocketIndexMaybe) == 1) {
            matched = this->TryMatchSocketMaybe();
            if (matched == 0) {
                bNoSocketMatch = true;
                kind = this->pTileMaybe->pKindDesc;
                if (kind->IsType0x63aInSet1234() == 1) {
                    TrackConnectorTileObj *leadTile = (TrackConnectorTileObj *)param_1->pNameMaybe->pTileMaybe;
                    if (leadTile->dwOccupancyClaim == 1 && param_1->dwModeAMaybe != 1) {
                        if (param_1->CanReverseDirectionMaybe()) {
                            param_1->TryTransitionModeMaybe(param_1->dwReversed == 0, 1);
                            return false;
                        }
                        param_1->SetSoundStateMaybe(1);
                        param_1->bUnk0x90 = 1;
                        param_1->wUnk0x36 = 2;
                        return false;
                    }
                    leadTile->dwOccupancyClaim = 1;
                    this->dwViewportExtendPhaseMaybe = 1;
                    param_1->dwModeAMaybe = 1;
                    // The documented dword-pair-copy idiom (docs/subsystems.md v72): the
                    // claimed tile's wPosX/wPosY pair lands in wSentinelC/D as ONE
                    // unaligned dword copy.
                    *(unsigned int *)&param_1->wSentinelCMaybe =
                        *(unsigned int *)&param_1->pNameMaybe->pTileMaybe->pos.wPosX;
                } else if (kind->IsType0x63aInSet() == 1) {
                    if (param_1->dwModeBMaybe == 0) {
                        if (((TrackDepotTileObj *)this->pTileMaybe)->TryClaimForTrainMaybe((PeerTrainNode *)param_1)) {
                            this->dwTileExtendPhaseMaybe = 1;
                        }
                    } else {
                        if (param_1->CanReverseDirectionMaybe()) {
                            param_1->TryTransitionModeMaybe(param_1->dwReversed == 0, 1);
                            return false;
                        }
                        param_1->SetSoundStateMaybe(1);
                        param_1->bUnk0x90 = 1;
                        param_1->wUnk0x36 = 2;
                        return false;
                    }
                }
                if (tile->pPendingCoupleWaiter == (PeerTrainNode *)param_1) {
                    tile->pPendingCoupleWaiter = 0;
                }
            }
        }
        if (matched == 0) {
ContinueTick:
            if (this->TryBeginCouplingWaitMaybe(param_1)) {
                return false;
            }
            if (param_1->dwSoundStateMaybe == 1) {
                TrackTileObj *tile2 = this->pTileMaybe;
                if (tile2 == 0) {
                    param_1->SetSoundStateMaybe(1);
                    return bRet;
                }
                switch (tile2->dwTrackState) {
                case 1:
                    param_1->ApplyLeadTileTickStateMaybe();
                    return bRet;
                case 2: {
                    if ((short)tile2->nOccupantRefCount > 1) {
                        return false;
                    }
                    BigObj *kind2 = tile2->pKindDesc;
                    // At one of the points tile's 4 branch entry/exit sockets?
                    if ((this->dwUnk0x04 == 1 && this->nSocketIndexMaybe == 1) ||
                        (this->dwUnk0x04 == 0 && this->nSocketIndexMaybe == kind2->wSocketCount - 1) ||
                        (this->dwUnk0x04 == 1 && this->nSocketIndexMaybe == kind2->wSocketCount + 2) ||
                        (this->dwUnk0x04 == 0 && this->nSocketIndexMaybe == kind2->wSocketCountExt - 1)) {
                        param_1->SetSoundStateMaybe(2);
                        return bRet;
                    }
                    if (tile2->dwTrackTickState == 4) {
                        tile2->dwTrackTickState = 5;
                        return bRet;
                    }
                    if (tile2->dwTrackTickState == 5) {
                        tile2->dwTrackTickState = 4;
                    }
                    return bRet;
                }
                case 7:
                    // Level crossing: if an ambient actor is standing on the crossing, leave it
                    // alone this tick; otherwise release via vtbl slot 7 and tick the state.
                    if (TestRectAgainstAllActorsMaybe((void *)0x485448, tile2->rect) != 0) {
                        return bRet;
                    }
                    this->pTileMaybe->dwTrackTickState = 5;
                    this->pTileMaybe->ReleaseChannelAndDispatch(0);
                    param_1->SetSoundStateMaybe(2);
                    return bRet;
                default:
                    if (bNoSocketMatch == true) {
                        param_1->SetSoundStateMaybe(2);
                    }
                    return bRet;
                }
            } else {
                TrackTileObj *tile2 = this->pTileMaybe;
                if (tile2 == 0) {
                    return bRet;
                }
                if (this->dwViewportExtendPhaseMaybe == 0 && this->dwTileExtendPhaseMaybe == 0) {
                    // Idle: normal table-driven single-socket-index step.
                    if (this->dwUnk0x04 == 1) {
                        this->nSocketIndexMaybe++;
                    } else {
                        this->nSocketIndexMaybe--;
                    }
                    bRet = true;
                    this->nAnchorXMaybe = kind->pSocketTable[this->nSocketIndexMaybe * 2] + ((short)tile2->pos.wPosX << 4);
                    this->nAnchorYMaybe = kind->pSocketTable[this->nSocketIndexMaybe * 2 + 1] + ((short)tile2->pos.wPosY << 4);
                } else if (this->dwViewportExtendPhaseMaybe == 1 || this->dwTileExtendPhaseMaybe == 1) {
                    // Socket match failed / new tile claimed: advance by the tile kind's
                    // fixed direction; the viewport-side phase escalates to 2 once its own
                    // bound is crossed.
                    BigObj *kind2 = tile2->pKindDesc;
                    switch (kind2->m_type0x63a) {
                    case 1: case 7: this->nAnchorXMaybe--; break;
                    case 2: case 8: this->nAnchorXMaybe++; break;
                    case 3: case 9: this->nAnchorYMaybe--; break;
                    case 4: case 10: this->nAnchorYMaybe++; break;
                    }
                    bRet = true;
                    if (this->dwViewportExtendPhaseMaybe == 1) {
                        BigObj *kind3 = tile2->pKindDesc;
                        switch (kind3->m_type0x63a) {
                        case 1: if (this->nAnchorXMaybe < 1) this->dwViewportExtendPhaseMaybe = 2; break;
                        case 2: if (this->nAnchorXMaybe > g_worldBoard.dwViewportWidth) this->dwViewportExtendPhaseMaybe = 2; break;
                        case 3: if (this->nAnchorYMaybe < 1) this->dwViewportExtendPhaseMaybe = 2; break;
                        case 4: if (this->nAnchorYMaybe >= g_worldBoard.dwViewportHeightMaybe) this->dwViewportExtendPhaseMaybe = 2; break;
                        }
                    }
                } else if (this->dwViewportExtendPhaseMaybe == 4) {
                    // Viewport-extend step, duplicated inline (see the extend-phase block).
                    BigObj *kind4 = tile2->pKindDesc;
                    switch (kind4->m_type0x63a) {
                    case 1: this->nAnchorXMaybe++; break;
                    case 2: this->nAnchorXMaybe--; break;
                    case 3: this->nAnchorYMaybe++; break;
                    case 4: this->nAnchorYMaybe--; break;
                    }
                    switch (kind4->m_type0x63a) {
                    case 1: if (this->nAnchorXMaybe >= 0) { this->dwViewportExtendPhaseMaybe = 0; bRet = true; } else { bRet = true; } break;
                    case 2: if (this->nAnchorXMaybe < g_worldBoard.dwViewportWidth) { this->dwViewportExtendPhaseMaybe = 0; bRet = true; } else { bRet = true; } break;
                    case 3: if (this->nAnchorYMaybe >= 0) { this->dwViewportExtendPhaseMaybe = 0; bRet = true; } else { bRet = true; } break;
                    case 4: if (this->nAnchorYMaybe <= g_worldBoard.dwViewportHeightMaybe) { this->dwViewportExtendPhaseMaybe = 0; bRet = true; } else { bRet = true; } break;
                    }
                    bRet = true;
                } else if (this->dwTileExtendPhaseMaybe == 4 || this->dwTileExtendPhaseMaybe == 5) {
                    if (this->ExtendAcrossTileBoundaryMaybe()) {
                        this->pTileMaybe->ReleaseChannelAndDispatch(0);
                    }
                    bRet = true;
                }
                if (this->pTileMaybe->dwTrackState == 4 &&
                    (param_1->dwModeBMaybe == 4 || param_1->dwModeBMaybe == 5) &&
                    ((TrackConnectorTileObj *)this->pTileMaybe)->dwOccupancyClaim != 0) {
                    this->pTileMaybe->ReleaseChannelAndDispatch(0);
                    ((TrackConnectorTileObj *)this->pTileMaybe)->dwOccupancyClaim = 0;
                }
                return bRet;
            }
        }
    } else {
        if (this->dwViewportExtendPhaseMaybe == 4) {
            // Extending across the VIEWPORT edge: nudge by the tile kind's fixed direction,
            // clear the phase once back inside the global viewport bound.
            switch (kind->m_type0x63a) {
            case 1: this->nAnchorXMaybe++; break;
            case 2: this->nAnchorXMaybe--; break;
            case 3: this->nAnchorYMaybe++; break;
            case 4: this->nAnchorYMaybe--; break;
            }
            switch (kind->m_type0x63a) {
            case 1:
                if (this->nAnchorXMaybe >= 0) { bRet = true; this->dwViewportExtendPhaseMaybe = 0; return bRet; }
                else { bRet = true; }
                return bRet;
            case 2:
                if (this->nAnchorXMaybe < g_worldBoard.dwViewportWidth) { bRet = true; this->dwViewportExtendPhaseMaybe = 0; return bRet; }
                else { bRet = true; }
                return bRet;
            case 3:
                if (this->nAnchorYMaybe >= 0) { bRet = true; this->dwViewportExtendPhaseMaybe = 0; return bRet; }
                else { bRet = true; }
                return bRet;
            case 4:
                if (this->nAnchorYMaybe <= g_worldBoard.dwViewportHeightMaybe) { bRet = true; this->dwViewportExtendPhaseMaybe = 0; return bRet; }
                else { bRet = true; }
                return bRet;
            }
            bRet = true;
            return bRet;
        } else if (this->dwTileExtendPhaseMaybe == 4 || this->dwTileExtendPhaseMaybe == 5) {
            if (this->ExtendAcrossTileBoundaryMaybe()) {
                this->pTileMaybe->ReleaseChannelAndDispatch(0);
            }
            bRet = true;
        }
        return bRet;
    }
    // Matched-tile handling: only ever reached with matched != 0 (the dispatch edge
    // thread-jumps straight here).
MatchedHandling:
    if (matched->dwTrackState == 8) {
        param_1->SetSoundStateMaybe(0);
        return false;
    }
    if (matched->dwTrackState == 2) {
        // Points/switch piece: TryAdvanceAcrossPointsMaybe toggles the tile's branch
        // (dwTrackTickState 4<->5) and retries the socket match.
        if (!this->TryAdvanceAcrossPointsMaybe(param_1, matched)) {
            return false;
        }
        goto ContinueTick;
    }
    if (matched->dwTrackState == 7) {
        // Level crossing: if an ambient actor is standing on it, back the train off (restore
        // the saved tile and go back to sound state 1); otherwise release via vtbl slot 7 and
        // retry the socket match.
        bool bRetry = false;
        if (TestRectAgainstAllActorsMaybe((void *)0x485448, matched->rect) != 0) {
            param_1->SetSoundStateMaybe(1);
            this->pTileMaybe = savedTile;
        } else {
            matched->ReleaseChannelAndDispatch(0);
            if (this->TryMatchSocketMaybe() != 0) {
                param_1->SetSoundStateMaybe(1);
            } else {
                param_1->SetSoundStateMaybe(2);
                bRetry = true;
            }
        }
        if (!bRetry) {
            return false;
        }
        goto ContinueTick;
    }
    if (matched->dwTrackState == 3) {
        if (param_1->CanReverseDirectionMaybe()) {
            param_1->TryTransitionModeMaybe(param_1->dwReversed == 0, 1);
            return false;
        }
        param_1->SetSoundStateMaybe(1);
        param_1->bUnk0x90 = 1;
        param_1->wUnk0x36 = 2;
        return false;
    }
    param_1->SetSoundStateMaybe(1);
    return false;
}

// FUNCTION: LOCO 0x40c3d0
// Called unconditionally near the tail of every AdvanceAlongTrackMaybe tick. Only does anything
// on the two uncharacterized kind ids 0x12/0x13 (hypothesized siding/buffer piece). If the train
// is already one tick from the end of a coupling wait, cancels the wait and reports "not
// waiting". Otherwise, if the train is carrying a category-2 car AND this anchor sits on the
// tile's FIRST or LAST socket (i.e. at a tile boundary, whichever end depends on the anchor's
// step direction), records the train as the tile's pending coupling waiter, arms a ~200-tick
// wait, plays the blocked/waiting sound, and reports "now waiting" -- on which the caller bails
// out of the rest of this tick.
//
// EFFECTIVE MATCH -- PARKED (asmscore --len 133: total 10002, align=10, reg_pen 0,
// identity_miss 0, byte_diff 2, insns 53/53, compiled 133 B = the original's exact length).
// ONE instruction, and it is a pure constant-materialization tie-break with no length cost:
// at the early-return epilogue the original spells the zero `mov al,bl` (reusing the result
// variable, which cl already keeps in BL and already uses for the `wUnk0x36 = 0` store two
// instructions earlier, as `mov [edi+0x36],bx`), where this compile emits `xor al,al`. Both
// encodings are 2 bytes, so the function is byte-length-identical and everything else pairs up.
// **Measured and INERT -- do NOT re-run:** spelling the early return `return bWaiting;` instead
// of `return 0;` scores an IDENTICAL 10002 -- cl constant-propagates the variable (it is
// provably still 0 on that path) and folds it back to `xor al,al` either way, so there is no
// source-level way to ask for the register read. Same family as this file's other
// zero-materialization residuals.
unsigned char NameAnchorMaybe::TryBeginCouplingWaitMaybe(PeerTrainNodeTrackPartial *pTrain)
{
    unsigned char bWaiting = 0;

    BigObj *pKindDesc = pTileMaybe->pKindDesc;
    unsigned char kind = pKindDesc->m_type0x63a;
    if (kind == 0x12 || kind == 0x13) {
        if (pTrain->wUnk0x36 == 1) {
            pTrain->wUnk0x36 = 0;
            return 0;
        }
        if (pTrain->HasCarCategory2Maybe() &&
            ((dwUnk0x04 == 0 && nSocketIndexMaybe == 1) ||
             (dwUnk0x04 == 1 && nSocketIndexMaybe == pKindDesc->wSocketCount - 1))) {
            pTileMaybe->pPendingCoupleWaiter = (PeerTrainNode *)pTrain;
            pTrain->wUnk0x36 = 200;
            pTrain->SetSoundStateMaybe(1);
            bWaiting = 1;
        }
    }
    return bWaiting;
}

// FUNCTION: LOCO 0x40c460
// The points/switch-piece arm of AdvanceAlongTrackMaybe's dwTrackState == 2 case (RESOLVED v73,
// transcribed v456). "The socket step wants to cross this points tile -- can it, and which way?"
//
// Bails (parking the train in the blocked/waiting sound state) when the tile is already occupied.
// Bails silently when the anchor is sitting on ONE OF THE TILE'S 4 BRANCH ENTRY/EXIT SOCKETS --
// the same four-way predicate the caller runs one socket further in (0/count/count+1/ext here vs.
// the caller's 1/count-1/count+2/ext-1), i.e. the anchor is AT a branch end, not crossing it.
// Otherwise it FLIPS the tile's branch selection (dwTrackTickState 4 <-> 5) and re-runs the
// socket hand-over: on success the anchor now sits on the neighbour, whose own branch selection
// is pushed through vtable slot 7; on failure the flip is reverted, leaving the tile as found.
unsigned char NameAnchorMaybe::TryAdvanceAcrossPointsMaybe(PeerTrainNodePartial *pTrain,
                                                           TrackTileObj *pTile)
{
    unsigned char bAdvanced = 0;

    if ((short)pTile->nOccupantRefCount >= 1) {
        pTrain->SetSoundStateMaybe(1);
        return 0;
    }

    BigObj *pKindDesc = pTile->pKindDesc;
    // At one of the points tile's 4 branch entry/exit sockets?
    if ((this->dwUnk0x04 == 1 && this->nSocketIndexMaybe == 0) ||
        (this->dwUnk0x04 == 0 && this->nSocketIndexMaybe == pKindDesc->wSocketCount) ||
        (this->dwUnk0x04 == 1 && this->nSocketIndexMaybe == pKindDesc->wSocketCount + 1) ||
        (this->dwUnk0x04 == 0 && this->nSocketIndexMaybe == pKindDesc->wSocketCountExt)) {
        return 0;
    }

    if (pTile->dwTrackTickState == 4) {
        pTile->dwTrackTickState = 5;
    } else if (pTile->dwTrackTickState == 5) {
        pTile->dwTrackTickState = 4;
    }

    if (this->TryMatchSocketMaybe() == 0) {
        TrackTileObj *pNewTile = this->pTileMaybe;
        if (pNewTile->dwTrackTickState == 4) {
            pNewTile->ReleaseChannelAndDispatch(1);
        } else if (pNewTile->dwTrackTickState == 5) {
            pNewTile->ReleaseChannelAndDispatch(0);
        }
        bAdvanced = 1;
    } else if (pTile->dwTrackTickState == 4) {
        pTile->dwTrackTickState = 5;
    } else if (pTile->dwTrackTickState == 5) {
        pTile->dwTrackTickState = 4;
    }
    return bAdvanced;
}

// FUNCTION: LOCO 0x40c580
// Per-car sibling of AdvanceAlongTrackMaybe (RESOLVED v74/v75, see docs/subsystems.md's
// NameAnchorMaybe entry): a trimmed-down version for one of a CarNetObj's own 2 anchors
// (pNameAMaybe/pNameBMaybe) -- same socket-step/extend shape as the lead-anchor version, but
// with NO points-crossing (TryAdvanceAcrossPointsMaybe) or coupling-wait
// (TryBeginCouplingWaitMaybe) sub-calls (those stay lead-anchor-only, off
// PeerTrainNode::pNameMaybe). Reads the TRAIN's own dwModeAMaybe/dwModeBMaybe (param_1) but
// writes the CAR's own dwModeAMaybe/dwModeBMaybe back (param_2) -- the per-car echo of the
// train-level mode state. dwViewportExtendPhaseMaybe==2 is handled specially at function ENTRY
// here (immediate tile-kind-direction step + return, no gating) -- AdvanceAlongTrackMaybe does
// NOT share this shape (there, phase==2 alone, without dwTileExtendPhaseMaybe in {4,5}, is a
// no-op early-return) -- a genuine behavioral difference between the lead and per-car anchors,
// not a register-allocation artifact. Every return in the original packs its upper 3 bytes
// with leftover register junk (CONCAT31(garbage,1)) -- the real return type is unsigned char,
// so every non-null-tile path is really just `return true`.
//
// PARKED (v343): structure verified block-by-block against the raw disasm (objdump, every
// branch/call/field access over the full 0x40c580-0x40ca37 span) -- content-complete, not an
// EFFECTIVE-tier match yet (asmscore --len 1207: total 1506886, align=1494 reg_pen=110
// identity_miss=119 byte_diff=696, insns 525/429; cc.sh DIFF(1027), ours 1372B vs orig 1207B).
// Confirmed real structural findings (not decompiler noise), each verified against raw bytes:
// (1) the original has MANY more physical return points than a naive transcription produces --
// nearly every distinct decision (each switch case's bound check, each phase transition) gets
// its OWN full "pop ebx/ebp/edi/esi; ret 8" epilogue rather than sharing one via a fallthrough
// return, so the source must place an explicit `return` at each such point rather than
// consolidating into one shared tail (this alone closed most of the initial byte_diff, from
// asmscore total 1748468 down to 1506886 across 2 rewrite passes); (2) a handful of shared
// exit points DO exist and are reached by real `goto`-equivalent jumps from multiple call
// sites -- `ViewportExtend4` (0x40c987, reached from both the function-ENTRY
// dwViewportExtendPhaseMaybe==4 check and a dead re-check of the same stale ecx register
// inside the dwTileExtendPhaseMaybe==4||5 branch), `TileBoundaryCheck`/`SharedExit`
// (0x40ca1b/0x40ca2f, reached from that same dead re-check's fallthrough AND from the
// post-Combine tail's own dwTileExtendPhaseMaybe==4||5 check); (3) a SEPARATE, non-sharing
// physical duplicate of the ViewportExtend4 shape exists at 0x40c7e8 (reached only from the
// post-Combine tail when dwViewportExtendPhaseMaybe==4) -- this one does NOT jump into
// ViewportExtend4/SharedExit despite being functionally identical, matching
// docs/CODEGEN.md's documented "goto-sharing is LOCAL/trace-driven -- 3+ call sites to the
// same logical tail CAN still compile to 2+ distinct physical copies" VC5 class (same class as
// `EditCardWnd::ImportDecalImageMaybe`'s v342 park); (4) within that SAME 0x40c7e8-sized
// TileBoundaryCheck logic, the dwTileExtendPhaseMaybe==4 sub-case shares 0x40ca28's epilogue
// but the ==5 sub-case does NOT (it has its own separate local return) -- an even finer-grained
// instance of the same non-uniform tail-merging behavior. Residual after modeling all 4
// findings: a register-allocation-tier mismatch, NOT a control-flow gap -- the compiled
// prologue differs (`push ecx,ebx,ebp,esi,edi` here vs the original's staged `push esi` then
// later `push edi,ebp,ebx` after the null check), consistent with this function's own
// register-pressure budget landing differently than the original's, the same class of
// residual its sibling AdvanceAlongTrackMaybe never closed either (see that function's own
// PARKED note below -- "TU-context /Og layout class", also unresolved after 12+ variants).
// Tried and no material effect: dropping the one-time-use `matched` local in favor of an
// inline `if (this->TryMatchSocketMaybe() == 0)` condition (identical score). Retry only if
// the shared TU-context /Og register-budget class ever cracks generally (see
// AdvanceAlongTrackMaybe's own park below and docs/PARKED.md).
unsigned char NameAnchorMaybe::AdvanceCarAnchorMaybe(PeerTrainNodeCarAdvancePartial *param_1, CarNetObj *param_2)
{
    TrackTileObj *tile = this->pTileMaybe;
    if (tile == 0) {
        return false;
    }
    if (this->dwViewportExtendPhaseMaybe == 2) {
        switch (tile->pKindDesc->m_type0x63a - 1) {
        case 0: case 6: this->nAnchorXMaybe--; break;
        case 1: case 7: this->nAnchorXMaybe++; break;
        case 2: case 8: this->nAnchorYMaybe--; break;
        case 3: case 9: this->nAnchorYMaybe++; break;
        }
        return true;
    }

    BigObj *kind = tile->pKindDesc;
    if (this->dwViewportExtendPhaseMaybe == 4) {
        goto ViewportExtend4;
    }
    if (this->dwTileExtendPhaseMaybe == 4 || this->dwTileExtendPhaseMaybe == 5) {
        if (this->dwViewportExtendPhaseMaybe == 4) {
            goto ViewportExtend4; // sic: dead here (already excluded above), mirrors the original's stale-register reuse
        }
        goto TileBoundaryCheck;
    }

    if (kind->IsEndSocketIndexMaybe((short)this->nSocketIndexMaybe) == 1) {
        TrackTileObj *pReversalTile = param_1->GetReversalTargetTileMaybe();
        if (this->TryMatchSocketMaybe() == 0) {
            BigObj *kind2 = tile->pKindDesc;
            if (param_1->dwModeAMaybe != 4 && kind2->IsType0x63aInSet1234()) {
                this->dwViewportExtendPhaseMaybe = 1;
                param_2->dwModeAMaybe = 1;
            } else if (param_1->dwModeBMaybe != 4 && param_1->dwModeBMaybe != 5 &&
                       kind2->IsType0x63aInSet()) {
                this->dwTileExtendPhaseMaybe = 1;
                param_2->dwModeBMaybe = 1;
            }
            if (pReversalTile != 0 && pReversalTile->nOccupantRefCount == 0) {
                pReversalTile->ReleaseChannelAndDispatch(1);
            }
        } else {
            switch (tile->pKindDesc->m_type0x63a) {
            case 1:
                if (this->nAnchorXMaybe < 1) this->dwViewportExtendPhaseMaybe = 2;
                break;
            case 2:
                if (this->nAnchorXMaybe > g_worldBoard.dwViewportWidth) this->dwViewportExtendPhaseMaybe = 2;
                break;
            case 3:
                if (this->nAnchorYMaybe < 1) this->dwViewportExtendPhaseMaybe = 2;
                break;
            case 4:
                if (g_worldBoard.dwViewportHeightMaybe <= this->nAnchorYMaybe) this->dwViewportExtendPhaseMaybe = 2;
                break;
            }
            if (this->dwViewportExtendPhaseMaybe != 2 && this->dwViewportExtendPhaseMaybe != 3) {
                param_1->SetSoundStateMaybe(1);
            } else {
                switch (tile->pKindDesc->m_type0x63a) {
                case 1: case 7: this->nAnchorXMaybe--; break;
                case 2: case 8: this->nAnchorXMaybe++; break;
                case 3: case 9: this->nAnchorYMaybe--; break;
                case 4: case 10: this->nAnchorYMaybe++; break;
                }
            }
        }
    }

Combine:
    // Socket-index nudge when idle & sound-state 2 (matches AdvanceAlongTrackMaybe's own
    // "Idle: normal table-driven single-socket-index step").
    if (param_1->dwSoundStateMaybe == 2 && this->pTileMaybe != 0 &&
        this->dwViewportExtendPhaseMaybe == 0 && this->dwTileExtendPhaseMaybe == 0) {
        if (this->dwUnk0x04 == 1) {
            this->nSocketIndexMaybe++;
        } else {
            this->nSocketIndexMaybe--;
        }
        this->nAnchorXMaybe = (short)kind->pSocketTable[this->nSocketIndexMaybe * 2] + (short)tile->pos.wPosX * 0x10;
        this->nAnchorYMaybe = (short)kind->pSocketTable[this->nSocketIndexMaybe * 2 + 1] + (short)tile->pos.wPosY * 0x10;
        return true;
    }

    if (this->dwViewportExtendPhaseMaybe == 1 || this->dwTileExtendPhaseMaybe == 1) {
        switch (tile->pKindDesc->m_type0x63a) {
        case 1: case 7: this->nAnchorXMaybe--; break;
        case 2: case 8: this->nAnchorXMaybe++; break;
        case 3: case 9: this->nAnchorYMaybe--; break;
        case 4: case 10: this->nAnchorYMaybe++; break;
        }
        switch (tile->pKindDesc->m_type0x63a) {
        case 1:
            if (this->nAnchorXMaybe < 1) goto SharedExit;
            this->dwViewportExtendPhaseMaybe = 0;
            return true;
        case 2:
            if (this->nAnchorXMaybe >= g_worldBoard.dwViewportWidth) goto SharedExit;
            this->dwViewportExtendPhaseMaybe = 0;
            return true;
        case 3:
            if (this->nAnchorYMaybe < 1) goto SharedExit;
            this->dwViewportExtendPhaseMaybe = 0;
            return true;
        case 4:
            if (this->nAnchorYMaybe > g_worldBoard.dwViewportHeightMaybe) goto SharedExit;
            this->dwViewportExtendPhaseMaybe = 0;
            return true;
        }
        goto SharedExit;
    }

    if (this->dwViewportExtendPhaseMaybe == 4) {
        // 0x40c7e8 -- a SEPARATE physical duplicate of the ViewportExtend4 shape below (own
        // returns, does not share ViewportExtend4/SharedExit's own epilogues) -- sic, matches
        // the original's own local goto-sharing/tail-merge decision at this specific call site.
        switch (tile->pKindDesc->m_type0x63a) {
        case 1: this->nAnchorXMaybe++; break;
        case 2: this->nAnchorXMaybe--; break;
        case 3: this->nAnchorYMaybe++; break;
        case 4: this->nAnchorYMaybe--; break;
        }
        switch (tile->pKindDesc->m_type0x63a) {
        case 1:
            if (this->nAnchorXMaybe >= 0) { this->dwViewportExtendPhaseMaybe = 0; return true; }
            break;
        case 2:
            if (this->nAnchorXMaybe < g_worldBoard.dwViewportWidth) { this->dwViewportExtendPhaseMaybe = 0; return true; }
            break;
        case 3:
            if (this->nAnchorYMaybe >= 0) { this->dwViewportExtendPhaseMaybe = 0; return true; }
            break;
        case 4:
            if (this->nAnchorYMaybe <= g_worldBoard.dwViewportHeightMaybe) { this->dwViewportExtendPhaseMaybe = 0; return true; }
            break;
        }
        goto SharedExit;
    }

TileBoundaryCheck: // 0x40ca1b -- reached both from here and from the top-level
                   // dwTileExtendPhaseMaybe==4||5 check above
    if (this->dwTileExtendPhaseMaybe == 4) {
        this->ExtendAcrossTileBoundaryMaybe();
        goto SharedExit;
    }
    if (this->dwTileExtendPhaseMaybe == 5) {
        this->ExtendAcrossTileBoundaryMaybe();
        return true; // sic: this specific call site does NOT share SharedExit's epilogue
    }
    goto SharedExit;

ViewportExtend4: // 0x40c987 -- reached from function-entry phase==4 AND the dead ecx==4 recheck above
    switch (kind->m_type0x63a) {
    case 1: this->nAnchorXMaybe++; break;
    case 2: this->nAnchorXMaybe--; break;
    case 3: this->nAnchorYMaybe++; break;
    case 4: this->nAnchorYMaybe--; break;
    }
    switch (kind->m_type0x63a) {
    case 1:
        if (this->nAnchorXMaybe < 0) goto SharedExit;
        this->dwViewportExtendPhaseMaybe = 0;
        return true;
    case 2:
        if (this->nAnchorXMaybe >= g_worldBoard.dwViewportWidth) goto SharedExit;
        this->dwViewportExtendPhaseMaybe = 0;
        return true;
    case 3:
        if (this->nAnchorYMaybe < 0) goto SharedExit;
        this->dwViewportExtendPhaseMaybe = 0;
        return true;
    case 4:
        if (this->nAnchorYMaybe > g_worldBoard.dwViewportHeightMaybe) goto SharedExit;
        this->dwViewportExtendPhaseMaybe = 0;
        return true;
    }
SharedExit: // 0x40ca2f
    return true;
}

// FUNCTION: LOCO 0x40cb10
//
// ⚠ EFFECTIVE MATCH (was EXACT v474..v475-open) -- 260 B against 260, DIFF(111), insns 98/99,
// total 208464 (align 204, reg_pen 39). NOT a source regression: nothing in this function
// changed. v475 promoted src/CarNetObj.h's class from the 0x430-byte CarNetObjPartial to the
// full 0x450-byte CarNetObj, which changed AdvanceCarAnchorMaybe's (0x40c580, above) parameter
// type and rotated the /Og state that carries forward into this function. The instruction
// sequence still lines up almost row-for-row; the whole residual is one whole-function register
// permutation (ebp/ebx/edi/esi), nearly every dump row a bare `r` rename.
// ⚠ Measured and deliberately NOT taken: re-declaring that parameter as an EMPTY
// `struct X : CarNetObj {}` restores this match exactly, and does so regardless of the type's
// name length (tested long and short). That makes the recovery a pure coin-flip artifact with no
// semantic content -- the original source named one class here. See docs/PARKED.md (v475) and
// docs/CODEGEN.md's inert-type-identity lever.
//
// The kind-7..0xa (straight-track family) tile-boundary extension step, called by
// AdvanceAlongTrackMaybe and AdvanceCarAnchorMaybe once both extend phases are armed. Advances
// the anchor one sub-unit further along the tile's own axis (X for kinds 7/8, Y for 9/0xa;
// 7/9 forward, 8/0xa backward), then re-tests the new position against THIS TILE's own footprint
// bound -- contrast the callers' inline dwViewportExtendPhaseMaybe==4 handling, which tests the
// global viewport bound instead. Still inside the tile: nothing happens and the function reports
// "not done". Bound crossed: re-derives the socket index at the new position and clears
// dwTileExtendPhaseMaybe, reporting "done" (on which the callers commit via the tile's vtable
// slot 7).
//
// ⚠ EFFECTIVE MATCH as of v456 -- PARKED, and this is a REGRESSION FROM v455's EXACT, caused
// entirely by TryAdvanceAcrossPointsMaybe (0x40c460) joining this TU (Yoda #23 carry-forward;
// this function's own source was not touched). asmscore --len 260: total 26770, reg_pen 22,
// insns 109/109, compiled 260 B = the original's exact length. Everything above offset 0xe1 that
// disagrees is an `r` row -- a pure EAX/ECX/EDX/EBX reassignment inside CASE 9's bound sum, plus
// the schedule swap of that case's two operand loads (the original evaluates
// bBitmapOccupancyRows's byte load first, matching source order; this compile hoists the
// `movsx` of pos.wPosY ahead of it). Cases 7, 8 and 10 pair up instruction-for-instruction, and
// the ternary below still does its job -- no `xor eax,eax` survives ahead of the `sete`, so the
// v455 lever is NOT what broke. Everything past 0xe1 is this function's own jump table decoding
// as masked-relocation noise, exactly like 0x40cc90's; judge it by the rows above that boundary.
// **Measured and INERT -- do NOT re-run:** flipping case 9's commutative operand order to
// `((short)pTile->pos.wPosY + pKindDesc->bBitmapOccupancyRows - 2)` scores IDENTICALLY,
// the same result v455 got flipping 0x40cc90's own two-term bound sums.
// PRICED AND ACCEPTED: 0x40c460 is +287 B against this function's -260 B = +27 B net.
//
// ⚠ The `(short)` casts on pos.wPosX/wPosY are load-bearing and faithful: the original
// sign-extends both (`movsx eax,WORD PTR [ecx+0x88]`) even though TileGridPos models them as
// `unsigned short`. Either the source casts, or those fields are really `short` -- a
// src/TilePlacedObj.h question not settled here.
//
// ⚠ The trailing `? true : false` is ALSO load-bearing and is the only thing between this
// function and its byte match (DIFF(13) -> EXACT, both at the original's exact 260 B). It is
// docs/CODEGEN.md's return-statement bool-materialization lever, and this site EXTENDS it twice
// over: the documented form is a `bool`-returning function returning a BYTE expression, whereas
// this one returns `unsigned char` from an `int == 0` comparison. The plain
// `return dwTileExtendPhaseMaybe == 0;` makes VC5 emit a full-width `xor eax,eax` ahead of the
// `sete` and evict the loaded field from EAX into ECX; the ternary drops the `xor` and reuses
// EAX, exactly like the original's `mov eax,[esi+0x1c]; cmp eax,ebp; sete al`.
unsigned char NameAnchorMaybe::ExtendAcrossTileBoundaryMaybe()
{
    TrackTileObj *pTile = pTileMaybe;
    BigObj *pKindDesc = pTile->pKindDesc;

    switch (pKindDesc->m_type0x63a) {
    case 7:
        nAnchorXMaybe++;
        break;
    case 8:
        nAnchorXMaybe--;
        break;
    case 9:
        nAnchorYMaybe++;
        break;
    case 10:
        nAnchorYMaybe--;
        break;
    }

    switch (pKindDesc->m_type0x63a) {
    case 7:
        if (nAnchorXMaybe >
            (pKindDesc->bBitmapOccupancyCols + (short)pTile->pos.wPosX) * 0x10 - 0x10) {
            ResolveSocketIndexAtPointMaybe(nAnchorXMaybe, nAnchorYMaybe);
            dwTileExtendPhaseMaybe = 0;
        }
        break;
    case 8:
        if (nAnchorXMaybe < ((short)pTile->pos.wPosX + 1) * 0x10) {
            ResolveSocketIndexAtPointMaybe(nAnchorXMaybe, nAnchorYMaybe);
            dwTileExtendPhaseMaybe = 0;
        }
        break;
    case 9:
        if (nAnchorYMaybe >
            (pKindDesc->bBitmapOccupancyRows - 2 + (short)pTile->pos.wPosY) * 0x10) {
            ResolveSocketIndexAtPointMaybe(nAnchorXMaybe, nAnchorYMaybe);
            dwTileExtendPhaseMaybe = 0;
        }
        break;
    case 10:
        if (nAnchorYMaybe < ((short)pTile->pos.wPosY + 1) * 0x10) {
            ResolveSocketIndexAtPointMaybe(nAnchorXMaybe, nAnchorYMaybe);
            dwTileExtendPhaseMaybe = 0;
        }
        break;
    }

    return dwTileExtendPhaseMaybe == 0 ? true : false;
}

// FUNCTION: LOCO 0x40cc20
// Out-of-line twin of the viewport-bound check AdvanceAlongTrackMaybe inlines (see its
// dwViewportExtendPhaseMaybe == 1 block above -- identical four comparison directions): for a
// kind-1..4 connector tile, test the anchor point against the global viewport bound belonging to
// that tile's own direction and, once crossed, arm dwViewportExtendPhaseMaybe's "bound crossed,
// settling" phase (2). Unlike the tile-side twin below this does NOT gate itself on a phase --
// the caller does. Each case compiles to its own `ret` (case 4 falls through to the shared one).
void NameAnchorMaybe::CheckAnchorCrossedViewportMaybe()
{
    switch (this->pTileMaybe->pKindDesc->m_type0x63a) {
    case 1: if (this->nAnchorXMaybe < 1) this->dwViewportExtendPhaseMaybe = 2; break;
    case 2: if (this->nAnchorXMaybe > g_worldBoard.dwViewportWidth) this->dwViewportExtendPhaseMaybe = 2; break;
    case 3: if (this->nAnchorYMaybe < 1) this->dwViewportExtendPhaseMaybe = 2; break;
    case 4: if (this->nAnchorYMaybe >= g_worldBoard.dwViewportHeightMaybe) this->dwViewportExtendPhaseMaybe = 2; break;
    }
}

// FUNCTION: LOCO 0x40cc90
// Tile-side twin of CheckAnchorCrossedViewportMaybe above: SELF-gated on
// dwTileExtendPhaseMaybe == 1, and for a kind-7..0xa depot tile it tests the anchor point
// against the TILE's OWN footprint bound -- the tile's grid position plus the kind descriptor's
// occupancy extent, in 16-pixel grid units -- rather than against the global viewport. Arms the
// tile-side phase (dwTileExtendPhaseMaybe = 2) instead of the viewport-side one. The same four
// comparison directions as CarNetObjAnchorPartial::CheckCarEnteredDepotMaybe (0x40e440,
// src/PeerTrainNode.cpp) applies to the car's rect, independently corroborating both -- but note
// the trailing constants genuinely differ between the two (here both the cols and rows cases are
// `* 16 - 16`; there the rows case is `- 2` INSIDE the multiply).
//
// ⚠ EFFECTIVE MATCH -- PARKED, and it is a REGRESSION: this function was EXACT through v453 and
// its source has NOT been touched. Cause bisected to the arrival of its own near-twin
// ExtendAcrossTileBoundaryMaybe (0x40cb10) in this TU -- Yoda #23 TU-context sensitivity. The
// two functions run the same four kind-7..0xa footprint-bound comparisons, and only one of them
// gets the register assignment it wants: with 0x40cb10 present, 0x40cc90's whole bound
// computation swaps EAX<->EDX against the original (asmscore --len 196: total 53902, align 52,
// reg_pen 17, identity_miss 17, byte_diff 32, all 13 real diff rows are `r`). ⚠ The headline
// `insns 63/56` is NOT a 7-instruction content gap -- everything from offset 0xb4 on is this
// function's own jump table decoding as masked-relocation noise on the candidate side.
//
// **The trade was measured and is NET POSITIVE, so it stands: +260 B (0x40cb10 EXACT) against
// this function's -196 B = +64 B.** Two recovery probes measured, do NOT re-run: (1) moving
// 0x40cb10 to the END of the TU instead of its address-order slot -- WORSE, 0x40cb10 itself
// drops to DIFF(111) and this function stays DIFF(13); (2) flipping the commutative operand
// order of the two-term bound sums here (`pos + cols` -> `cols + pos`) to compensate for the
// swap -- INERT, cl canonicalizes the add either way. Adding 0x40c3d0 to the TU is separately
// confirmed NEUTRAL for both. Retry if 0x40cb10's own residual class ever cracks.
void NameAnchorMaybe::CheckAnchorCrossedTileBoundMaybe()
{
    if (this->dwTileExtendPhaseMaybe == 1) {
        TrackTileObj *pTile = this->pTileMaybe;
        BigObj *pKind = pTile->pKindDesc;
        switch (pKind->m_type0x63a) {
        case 7:
            if (this->nAnchorXMaybe < ((short)pTile->pos.wPosX + pKind->bBitmapOccupancyCols) * 16 - 16)
                this->dwTileExtendPhaseMaybe = 2;
            break;
        case 8:
            if (this->nAnchorXMaybe > ((short)pTile->pos.wPosX + 1) * 16)
                this->dwTileExtendPhaseMaybe = 2;
            break;
        case 9:
            if (this->nAnchorYMaybe < ((short)pTile->pos.wPosY + pKind->bBitmapOccupancyRows) * 16 - 16)
                this->dwTileExtendPhaseMaybe = 2;
            break;
        case 10:
            if (this->nAnchorYMaybe > ((short)pTile->pos.wPosY + 1) * 16)
                this->dwTileExtendPhaseMaybe = 2;
            break;
        }
    }
}

// FUNCTION: LOCO 0x40cd60
// Apply a direction reversal to this anchor: flip its socket-step direction selector
// (dwUnk0x04), then re-seat BOTH extend phases -- the viewport-side one and the tile-side one --
// for the new direction. Called when the train reverses (the counterpart of the re-walk that
// PeerTrainNode::ReverseDirectionMaybe drives).
//
// Three details worth recording:
//  * IsEndSocketIndexMaybe is called TWICE in a row with the same argument and the first result is
//    discarded. Reproduced faithfully. Note this need NOT be a redundant call in the original
//    source: with /Gy the linker folds identical COMDATs, so two DISTINCT tiny socket predicates
//    (this codebase has a whole IsType0x63aInSet* family of them) can share one address. The
//    call target is a relocation, so the reloc-masked compare cannot tell the two readings apart.
//  * All three arms of that first if/else chain perform the SAME dwUnk0x04 toggle, each in its
//    own register (eax/ecx/edx) -- three separate source statements the optimizer did not merge,
//    not one hoisted toggle.
//  * The tile-kind switch that re-seats nSocketIndexMaybe writes `wSocketCount - 1` for case 1
//    and case 4 in two separate code blocks (cases 2 and 3 DO share one), so those stay separate
//    source cases rather than being folded into `case 1: case 4:`.
//
// PARKED (v350, first transcription). Structurally complete -- every block, branch, jump table
// and call verified against the raw disasm over the full 0x40cd60-0x40cf9b span (body + all five
// jump tables). `asmscore.py --len 572`: total 468196, align=464 reg_pen=36 identity_miss=40
// byte_diff=196, insns 228/224. Three source-shape findings were confirmed and are BAKED IN
// (each measured, one compile at a time, total 604568 -> 468196):
//   (1) BOTH phase switches emit `case 1` PHYSICALLY BEFORE `case 0` (jump tables 0x40cf30 and
//       0x40cf68 prove it), so the source lists them in that order -- case 0 then falls THROUGH
//       into the case 4/5 store it shares. Writing them in numeric order cost ~130k of score.
//   (2) The viewport-resolve step is a FLAT `if (phase == 0 && dwTrackState == 3) ... else if
//       (phase != 0)` chain, not nested ifs: only the flat form reproduces the original's
//       redundant re-test of the phase at 0x40ce6c (`test eax,eax; je`) on the mismatch path.
//   (3) Hoisting the phase into a local is byte-neutral -- VC5 already CSEs the field read.
// Residual is a single register-budget coin-flip, fully diagnosed and NOT source-steerable: the
// original pins the constants 1/4/3 in ebx/ebp/ecx and RELOADS the pTrain parameter from
// [esp+0x14] at both of its uses; this compile instead caches pTrain in ebp and therefore has to
// rematerialize the constant 1 into edx at FOUR sites (the whole 228-vs-224 instruction
// overshoot) and folds the two g_worldBoard reads into their `cmp` operands instead of loading
// them to a register first. One callee-saved register's worth of budget, cascading. A full
// tools/possweep.py TU-position sweep over all 6 slots in this .cpp is FLAT (spread 0), so this
// is measured-intrinsic, not position-sensitive. Retry only if the VC5 /Og register-budget class
// cracks generally -- same class as this TU's AdvanceAlongTrackMaybe/AdvanceCarAnchorMaybe parks.
void NameAnchorMaybe::ApplyDirectionReversalMaybe(PeerTrainNodePartial *pTrain)
{
    BigObj *pKind = this->pTileMaybe->pKindDesc;
    pKind->IsEndSocketIndexMaybe((short)this->nSocketIndexMaybe);
    if (pKind->IsEndSocketIndexMaybe((short)this->nSocketIndexMaybe) == 0) {
        this->dwUnk0x04 = (this->dwUnk0x04 == 0);
    } else if (this->TryMatchSocketMaybe() == 0) {
        this->dwUnk0x04 = (this->dwUnk0x04 == 0);
    } else {
        this->dwUnk0x04 = (this->dwUnk0x04 == 0);
    }

    switch (this->dwViewportExtendPhaseMaybe) {
    case 1:
    case 2:
        this->dwViewportExtendPhaseMaybe = 4;
        break;
    case 0:
        if (pTrain->dwModeAMaybe == 1 && this->pTileMaybe->dwTrackState == 3) {
            this->dwViewportExtendPhaseMaybe = 1;
        }
        break;
    case 4:
    case 5:
        this->dwViewportExtendPhaseMaybe = 1;
        break;
    }

    if (this->dwViewportExtendPhaseMaybe == 4) {
        // Already back inside the viewport for this tile's direction? Then the extend is over.
        switch (pKind->m_type0x63a) {
        case 1: if (this->nAnchorXMaybe >= 0) this->dwViewportExtendPhaseMaybe = 0; break;
        case 2: if (this->nAnchorXMaybe < g_worldBoard.dwViewportWidth) this->dwViewportExtendPhaseMaybe = 0; break;
        case 3: if (this->nAnchorYMaybe >= 0) this->dwViewportExtendPhaseMaybe = 0; break;
        case 4: if (this->nAnchorYMaybe <= g_worldBoard.dwViewportHeightMaybe) this->dwViewportExtendPhaseMaybe = 0; break;
        }
        // Flat `A && B` / `else if (A)` chain, not nested ifs: the original re-tests the phase
        // on the dwTrackState-mismatch path (a redundant `test eax,eax; je` at 0x40ce6c that
        // only a flattened chain produces).
        int nPhase = this->dwViewportExtendPhaseMaybe;
        if (nPhase == 0 && this->pTileMaybe->dwTrackState == 3) {
            this->ResolveSocketIndexAtPointMaybe(this->nAnchorXMaybe, this->nAnchorYMaybe);
        } else if (nPhase != 0) {
            // Still extending: re-seat the socket index at the far end for the new direction.
            switch (pKind->m_type0x63a) {
            case 1: this->nSocketIndexMaybe = pKind->wSocketCount - 1; break;
            case 2:
            case 3: this->nSocketIndexMaybe = 1; break;
            case 4: this->nSocketIndexMaybe = pKind->wSocketCount - 1; break;
            }
        }
    }

    switch (this->dwTileExtendPhaseMaybe) {
    case 1:
        this->dwTileExtendPhaseMaybe = 4;
        return;
    case 0:
        if (this->pTileMaybe->dwTrackState == 4) {
            int nMode = pTrain->dwModeBMaybe;
            if (nMode == 4 || nMode == 5) {
                this->dwTileExtendPhaseMaybe = 1;
                return;
            }
            if (nMode == 0) {
                // Depot-kind resource id decides which step direction re-arms the tile extend.
                BigObj *pDepotKind = this->pTileMaybe->pKindDesc;
                switch (pDepotKind == 0 ? -1 : pDepotKind->resourceId) {
                case 0xc54: if (this->dwUnk0x04 == 0) this->dwTileExtendPhaseMaybe = 1; return;
                case 0xc56:
                case 0xc5a: if (this->dwUnk0x04 == 1) this->dwTileExtendPhaseMaybe = 1; return;
                case 0xc58: if (this->dwUnk0x04 == 0) this->dwTileExtendPhaseMaybe = 1; return;
                }
            }
        }
        return;
    case 4:
    case 5:
        this->dwTileExtendPhaseMaybe = 1;
        return;
    }
}
