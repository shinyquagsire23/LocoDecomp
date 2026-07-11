// RoadVehicleActor -- TileKind category 8, i.e. every `vehicles\*` kind (see DecorActor.h).
// This TU is the .text run 0x4533d0-0x454250, sitting on its own between WorldBoardMaybe's and
// LocoBitmap's.
//
// It is WalkerActor's road-going twin and reads as a literal copy-and-edit of that class: the
// same construction/teardown, per-tick brain, destination/step chooser pair, movement stepper
// and standability test, with `roads\*` (TileKind category 0xd, graph B) everywhere WalkerActor
// has `paths\*` (category 0xc, graph A). Two whole features are dropped in the edit -- there is
// no train boarding and no population-throttle follow stepper -- and three are added:
//
//   * every step is checked for a pixel-accurate collision against the other actors before it
//     is committed (DecorObjMgrMaybe::TestActorCollisionMaybe), so cars queue behind each other
//     instead of driving through one another;
//   * every chosen position is nudged off the tile centre by ApplyLaneOffsetMaybe, which is
//     what puts vehicles on the correct side of the road;
//   * the facing is a full 8-way compass rather than WalkerActor's 4.
//
// Its destinations are not buildings but roads: a placed instance of a randomly chosen road
// kind id, `0x3400 + rand() % 0x29` -- exactly the 41 category-0xd kinds in
// docs/TILEKIND_IDS.md. So a car simply drives from one random stretch of road to another
// forever, which is all the ambient traffic ever needed to do.
#include "DecorActor.h"
#include "DecorObjMgrMaybe.h"
#include "GeomUtil.h"
#include "PlacementCursorMaybe.h"
#include "TrackGraph.h"
#include "WorldBoardMaybe.h"

#include <stdlib.h> // rand, abs

extern unsigned int g_dwGameTick; // DAT_004a99b4

// Minimal extern view for the one singleton method this TU calls into, named distinctly from
// every other consumer's own view of DAT_004a9990 for the reason documented in
// src/WalkerActor.cpp -- these are method-only views with no fields, so there is no shared
// layout to drift.
struct NetSessionEventQueueRoadPartial {
    void *PickRandomBigObjByCategory(int category); // 0x41e1f0, extern
};
extern NetSessionEventQueueRoadPartial g_NetSessionEventQueueRoad; // DAT_004a9990

// The same TU-local (x, y) pair-equality predicate pair src/WalkerActor.cpp has, and for the
// same reason: every position compare in this TU materializes a branchy 0/1 bool
// (`mov r,1; jmp; xor r,r; test rl,rl; je`) rather than compiling to a plain `cmp; jne`, which
// is what an inline predicate returning `bool` gives and a bare inline `&&` does not. The
// original really does have BOTH polarities rather than negating one at the call site -- each
// site branches to its 1-result on the FIRST test that decides it, i.e. the || vs && short
// circuit showing through.
static inline bool ArePositionsEqualMaybe(int ax, int ay, int bx, int by) {
    return ax == bx && ay == by;
}

static inline bool ArePositionsDifferentMaybe(int ax, int ay, int bx, int by) {
    return ax != bx || ay != by;
}

// The world->tile-grid conversion, written out at each site in the original exactly as it is in
// src/WalkerActor.cpp: an arithmetic shift down by 4 (16 px per tile) with everything left of
// the origin collapsing onto the -1 sentinel row or column rather than wrapping.

// The two `paths\*` kinds a road vehicle is nonetheless allowed to drive across: `paths\zebral-h`
// and `paths\zebral-v` (docs/TILEKIND_IDS.md), the crossing markings painted across a road. They
// are path tiles by category but part of the road network for pathing purposes, so every
// on-the-network test in this TU accepts them alongside category 0xd.
#define KIND_PATHS_ZEBRAL_H 0x3001
#define KIND_PATHS_ZEBRAL_V 0x3002

// And the two `track\*` kinds it may stand on: `track\xsing1-v` / `track\xsing1-h`, the level
// crossings. The mirror of WalkerActor's own `track\zebras-h`/`-v` pair.
#define KIND_TRACK_XSING1_V 0xc4a
#define KIND_TRACK_XSING1_H 0xc4c

// FUNCTION: LOCO 0x4533d0
// The base ctor already sets nMoodMaybe to 4; this class redundantly sets it again, exactly as
// WalkerActor's own ctor does.
RoadVehicleActor::RoadVehicleActor(int kindId) : DecorActorBase(kindId) {
    nMoodMaybe = 4;
}

// FUNCTION: LOCO 0x4363e0 (??_GRoadVehicleActor scalar deleting dtor -- compiler-generated)
// FUNCTION: LOCO 0x4533f0
// Only one reference to drop, the placement cursor's hover slot -- a vehicle has no train seat
// to give up.
//
// The `??_G` thunk this definition also emits was unclaimed until now even though the dtor
// itself has been EXACT for a long time: the linker placed the two COMDATs 0x1f000 bytes apart
// (0x4363e0 vs 0x4533f0), so the thunk never showed up as a neighbour of anything in this TU.
RoadVehicleActor::~RoadVehicleActor() {
    if (PlacementCursorMaybe_004854c8.pHoverObjMaybe == this) {
        PlacementCursorMaybe_004854c8.SetHoverObjMaybe(NULL);
    }
}

// FUNCTION: LOCO 0x453450
// vtable slot 15 -- the per-tick brain. Two states rather than WalkerActor's three (there is no
// throttle path): keep driving while there is distance left to cover, and once there is not,
// pick a fresh stretch of road to drive to.
//
// The retarget is additionally suppressed while the player is dragging this vehicle around with
// the placement cursor, which WalkerActor instead handles inside its standability test.
void RoadVehicleActor::TickMaybe(DecorActorBase *pNextActor) {
    if (bSuspendedMaybe) {
        return;
    }
    DecorActorBase::TickMaybe(pNextActor);
    if (nStepDistanceMaybe != 0) {
        if (AdvanceMovementMaybe() == 0) {
            // A (-1,-1) step target means the chooser gave up, and there is then nothing to
            // have arrived at.
            if (!ArePositionsEqualMaybe(ptStepMaybe.x, ptStepMaybe.y, -1, -1)) {
                if (ArePositionsEqualMaybe(hotspotPosX, hotspotPosY, ptDestMaybe.x,
                                           ptDestMaybe.y)) {
                    OnArriveAtDestinationMaybe(nScheduleStateMaybe);
                } else {
                    ChooseNextStepTileMaybe(hotspotPosX, hotspotPosY);
                }
            }
        }
        if (nStepDistanceMaybe != 0) {
            return;
        }
    }
    if ((int)dwNextDecisionTickMaybe < (int)g_dwGameTick &&
        PlacementCursorMaybe_004854c8.pHoverObjMaybe != this) {
        TilePlacedObj *pRoad = (TilePlacedObj *)g_NetSessionEventQueueRoad.PickRandomBigObjByCategory(
            rand() % 0x29 + 0x3400);
        if (pRoad != 0) {
            if (ArePositionsDifferentMaybe(pRoad->hotspotPosX, pRoad->hotspotPosY,
                                           nPrevDestPosXMaybe, nPrevDestPosYMaybe)) {
                SetDestinationTileMaybe(pRoad->hotspotPosX, pRoad->hotspotPosY);
            }
        }
    }
}

// FUNCTION: LOCO 0x453570
// vtable slot 16 -- adopt (x, y) as the final destination and resolve it onto the roads\*
// network, then hand off to the step chooser.
//
// The one structural difference from WalkerActor::SetDestinationTileMaybe: because the
// destination handed in here is itself a road tile's position most of the time, this first
// looks at whatever is already under it and only runs the (much more expensive) nearest-tile
// search when that is NOT already part of the road network.
void RoadVehicleActor::SetDestinationTileMaybe(int x, int y) {
    dwLastRetargetTickMaybe = g_dwGameTick;
    if (ArePositionsEqualMaybe(x, y, -1, -1) || g_worldBoard.bTrackGraphsBuiltFlag == 0) {
        ComputeStepTargetMaybe(-1, -1, 1);
        return;
    }

    if (ArePositionsDifferentMaybe(ptDestMaybe.x, ptDestMaybe.y, x, y)) {
        nPrevDestPosXMaybe = ptDestMaybe.x;
        ptDestMaybe.x = x;
        nPrevDestPosYMaybe = ptDestMaybe.y;
        ptDestMaybe.y = y;
    }

    TilePlacedObj *pDestTile =
        g_worldBoard.GetPlaneASlotMaybe(WORLD_TO_TILE(ptDestMaybe.x), WORLD_TO_TILE(ptDestMaybe.y), 0);
    // sic: nCategory is a 16-BIT local, not a char and not an int -- the original widens the
    // byte field with `movzx dx,dl` and compares it as `cmp dx,0xd`. And the two extractions
    // are separate ternaries over the same null test, not one shared if/else, which is why the
    // original re-tests pKindDesc between them (`test ecx,ecx` twice, at 0x453640 and
    // 0x45364b).
    unsigned short nCategory = 0;
    int nKindId = 0;
    if (pDestTile != 0) {
        BigObj *pDesc = pDestTile->pKindDesc;
        nCategory = (pDesc == 0) ? (unsigned char)0 : pDesc->categoryByte;
        nKindId = (pDesc == 0) ? -1 : pDesc->resourceId;
    }
    if (nCategory != 0xd && nKindId != KIND_PATHS_ZEBRAL_H && nKindId != KIND_PATHS_ZEBRAL_V) {
        pDestTile = g_worldBoard.FindNearestObjOfCategoryMaybe(0xd, ptDestMaybe.x, ptDestMaybe.y, 0x30);
    }

    if (pDestTile != 0) {
        ptDestTileMaybe.x = pDestTile->hotspotPosX;
        ptDestTileMaybe.y = pDestTile->hotspotPosY;
        if (pDestTile->nGraphNodeIdBMaybe != -1) {
            nDestGraphNodeIdMaybe = pDestTile->nGraphNodeIdBMaybe;
            ChooseNextStepTileMaybe(hotspotPosX, hotspotPosY);
            return;
        }
        // A plain stretch of road between junctions is not itself a graph node, so path to the
        // cheapest-linked neighbour of it instead.
        unsigned char nDir = 0;
        for (unsigned char i = 1; i < 4; i++) {
            if (pDestTile->anNeighbourCostBMaybe[i] != 0 &&
                (pDestTile->anNeighbourCostBMaybe[i] <
                     pDestTile->anNeighbourCostBMaybe[nDir] ||
                 pDestTile->anNeighbourCostBMaybe[nDir] == 0)) {
                nDir = i;
            }
        }
        TilePlacedObj *pNeighbour = pDestTile->apNeighbourTileBMaybe[nDir];
        if (pNeighbour != 0) {
            nDestGraphNodeIdMaybe = pNeighbour->nGraphNodeIdBMaybe;
            ChooseNextStepTileMaybe(hotspotPosX, hotspotPosY);
            return;
        }
        nDestGraphNodeIdMaybe = 0xff;
    }
    ChooseNextStepTileMaybe(hotspotPosX, hotspotPosY);
}

// FUNCTION: LOCO 0x453790
// vtable slot 18 -- the driver, and at 1499 bytes the largest function in the app region. Given
// where the vehicle is standing NOW, decide the single next tile to drive to and write it into
// ptStepMaybe.
//
// The three cases are WalkerActor::ChooseNextStepTileMaybe's, in the same order and with the
// same shapes, over graph B rather than graph A. What is new is that this version also tracks
// TWO directions across all three -- the direction it is arriving on (nDirIn) and the direction
// it will leave on (nDirOut), both 0-3 with 0xff for "none" -- and feeds them to
// ApplyLaneOffsetMaybe at the very end, which is what puts the vehicle on the correct side of
// the road and rounds off its turns.
//
// sic: two CalcSqDist calls in here have their results thrown away. Both are the remains of a
// comparison WalkerActor still has and this copy dropped -- see docs/engine-bugs.md.
void RoadVehicleActor::ChooseNextStepTileMaybe(int x, int y) {
    // Locals up front: every branch converges on the one shared tail below, and VC5 rejects a
    // goto that skips an initializer.

    TilePlacedObj *pTile;
    TilePlacedObj *pEntry;
    TilePlacedObj *pNext;
    TilePlacedObj *pNextTile;
    TrackGraph *pGraph;
    TrackGraphEdge *pEdge;
    unsigned int nNextId;
    unsigned char nDir;
    // 16-bit category local and two separate ternaries -- see the note in
    // SetDestinationTileMaybe above.
    unsigned short nCategory = 0;
    int nKindId = 0;
    unsigned char nDirOut = 0xff;
    unsigned char nDirIn = 0xff;

    pTile = g_worldBoard.GetPlaneASlotMaybe(WORLD_TO_TILE(x), WORLD_TO_TILE(y), 0);
    if (pTile != 0) {
        BigObj *pDesc = pTile->pKindDesc;
        nCategory = (pDesc == 0) ? (unsigned char)0 : pDesc->categoryByte;
        nKindId = (pDesc == 0) ? -1 : pDesc->resourceId;
    }

    if (nCategory == 0xd || nKindId == KIND_PATHS_ZEBRAL_H || nKindId == KIND_PATHS_ZEBRAL_V) {
        if (pTile->nGraphNodeIdBMaybe == -1) {
            // On the network but BETWEEN graph nodes.
            if (ArePositionsDifferentMaybe(x, y, ptDestTileMaybe.x, ptDestTileMaybe.y)) {
                nDirIn = 0;
                for (nDir = 1; nDir < 4; nDir++) {
                    if (pTile->anNeighbourCostBMaybe[nDir] != 0 &&
                        (pTile->anNeighbourCostBMaybe[nDir] <
                             pTile->anNeighbourCostBMaybe[nDirIn] ||
                         pTile->anNeighbourCostBMaybe[nDirIn] == 0)) {
                        nDirIn = nDir;
                    }
                }
                pNext = pTile->apNeighbourTileBMaybe[nDirIn];
                if (pNext == 0) {
                    goto stop;
                }
                pGraph = g_worldBoard.pTrackGraphBMaybe;
                nDirOut = pGraph->GetStepDirectionMaybe(pNext->nGraphNodeIdBMaybe,
                                                        nDestGraphNodeIdMaybe);
                // The cheapest link points back the way the graph says to come FROM: prefer the
                // graph's own direction instead, but only when we are not already on the
                // straight line to the destination and that direction is actually linked.
                if (nDirOut == (unsigned char)((nDirIn - 2) & 3) &&
                    !IsPointOnSegmentMaybe(
                        ptDestTileMaybe.x, ptDestTileMaybe.y, pTile->hotspotPosX,
                        pTile->hotspotPosY,
                        pTile->apNeighbourTileBMaybe[nDirIn]->hotspotPosX,
                        pTile->apNeighbourTileBMaybe[nDirIn]->hotspotPosY) &&
                    pTile->apNeighbourTileBMaybe[nDirOut] != 0) {
                    nDirIn = nDirOut;
                } else {
                    nDirOut = (unsigned char)nDirIn;
                }
                pNextTile = pTile->apNeighbourTileBMaybe[nDirIn];
                ptStepMaybe.x = pNextTile->hotspotPosX;
                ptStepMaybe.y = pNextTile->hotspotPosY;
                nNextId = (unsigned int)pTile->apNeighbourTileBMaybe[nDirIn]
                              ->nGraphNodeIdBMaybe;
                if (nNextId == (unsigned int)nDestGraphNodeIdMaybe) {
                    // Last hop: if the destination tile sits on the straight line either side
                    // of that node, cut the corner and drive straight at it.
                    if (IsPointOnSegmentMaybe(ptDestTileMaybe.x, ptDestTileMaybe.y,
                                              pTile->hotspotPosX, pTile->hotspotPosY,
                                              pNextTile->hotspotPosX, pNextTile->hotspotPosY)) {
                        ptStepMaybe.x = ptDestTileMaybe.x;
                        ptStepMaybe.y = ptDestTileMaybe.y;
                        nDirOut = (unsigned char)nDestGraphNodeIdMaybe;
                        goto lastHop;
                    }
                    if (IsPointOnSegmentMaybe(pTile->hotspotPosX, pTile->hotspotPosY,
                                              ptDestTileMaybe.x, ptDestTileMaybe.y,
                                              ptStepMaybe.x, ptStepMaybe.y)) {
                        ptStepMaybe.x = ptDestTileMaybe.x;
                        ptStepMaybe.y = ptDestTileMaybe.y;
                        nDirIn = (unsigned char)(nDirOut - 2) & 3;
                    }
                }
            lastHop:
                nDirOut = pGraph->GetStepDirectionMaybe(nNextId, nDestGraphNodeIdMaybe);
                goto out;
            }
        } else if ((unsigned int)pTile->nGraphNodeIdBMaybe !=
                   (unsigned int)nDestGraphNodeIdMaybe) {
            // On the network, ON a graph node.
            pGraph = g_worldBoard.pTrackGraphBMaybe;
            nDirIn = pGraph->GetStepDirectionMaybe(pTile->nGraphNodeIdBMaybe,
                                                   nDestGraphNodeIdMaybe);
            if (nDirIn == 0x80) {
                goto out;
            }
            if (nDirIn == 0xff) {
                // Destination unreachable from this node -- adopt a reachable one and retry.
                nDestGraphNodeIdMaybe =
                    PickReachableDestNodeMaybe(pGraph, pTile->nGraphNodeIdBMaybe);
                pNextTile = pGraph->papNode[nDestGraphNodeIdMaybe]->pTile;
                ptDestTileMaybe.x = pNextTile->hotspotPosX;
                ptDestTileMaybe.y = pNextTile->hotspotPosY;
                ChooseNextStepTileMaybe(x, y);
                goto out;
            }
            pEdge = pGraph->papNode[pTile->nGraphNodeIdBMaybe]->apEdgeMaybe[nDirIn];
            if (pEdge == 0) {
                goto out;
            }
            // An edge stores both its endpoints, so pick whichever one is not us.
            nNextId = pEdge->nNodeIdA;
            if (nNextId == (unsigned int)pTile->nGraphNodeIdBMaybe) {
                nNextId = pEdge->nNodeIdB;
            }
            pNextTile = pGraph->papNode[nNextId]->pTile;
            ptStepMaybe.x = pNextTile->hotspotPosX;
            ptStepMaybe.y = pNextTile->hotspotPosY;
            if (nNextId == (unsigned int)nDestGraphNodeIdMaybe &&
                IsPointOnSegmentMaybe(ptDestTileMaybe.x, ptDestTileMaybe.y, pTile->hotspotPosX,
                                      pTile->hotspotPosY, pNextTile->hotspotPosX,
                                      pNextTile->hotspotPosY)) {
                ptStepMaybe.x = ptDestTileMaybe.x;
                ptStepMaybe.y = ptDestTileMaybe.y;
            }
            if (nNextId == (unsigned int)nDestGraphNodeIdMaybe) {
                // We are stepping onto the destination node itself, so there is no further edge
                // to ask the graph about -- take the direction straight off the geometry.
                if (ptDestTileMaybe.x < x) {
                    nDirOut = 0;
                } else if (x < ptDestTileMaybe.x) {
                    nDirOut = 2;
                } else if (ptDestTileMaybe.y < y) {
                    nDirOut = 3;
                } else {
                    nDirOut = (ptDestTileMaybe.y <= y) ? 0xff : 1;
                }
                goto out;
            }
            nDirOut = pGraph->GetStepDirectionMaybe(nNextId, nDestGraphNodeIdMaybe);
            goto out;
        }

        // Standing on the destination node/tile itself.
        if (ArePositionsDifferentMaybe(x, y, ptDestTileMaybe.x, ptDestTileMaybe.y)) {
            ptStepMaybe.x = ptDestTileMaybe.x;
            ptStepMaybe.y = ptDestTileMaybe.y;
            // Which way that last stretch runs, off the dominant axis of the offset between the
            // tile we are on and where we are headed.
            int nDeltaX = pTile->hotspotPosX - ptStepMaybe.x;
            int nDeltaY = pTile->hotspotPosY - ptStepMaybe.y;
            if (abs(nDeltaY) < abs(nDeltaX)) {
                nDirIn = (pTile->hotspotPosX <= ptStepMaybe.x) ? 2 : 0;
            } else {
                nDirIn = (pTile->hotspotPosY <= ptStepMaybe.y) ? 1 : 3;
            }
            // The lane offset is baked into the STORED destination, not just into this step, so
            // the arrival test later compares against the same offset point.
            if (ArePositionsEqualMaybe(ptDestTileMaybe.x, ptDestTileMaybe.y, ptDestMaybe.x,
                                       ptDestMaybe.y)) {
                ApplyLaneOffsetMaybe(&ptDestMaybe, nDirIn, 0xff);
            }
            ApplyLaneOffsetMaybe(&ptDestTileMaybe, nDirIn, 0xff);
            goto out;
        }
        // sic: the result is discarded. WalkerActor's own copy of this line is
        // `if (CalcSqDist(...) < 0x901) { step = dest; goto out; }` -- the road version kept the
        // call and lost the test, so a vehicle can never leave the road network for the last
        // stretch to its destination. See docs/engine-bugs.md.
        CalcSqDist(x, y, ptDestMaybe.x, ptDestMaybe.y);
    } else {
        // Off-network: get back onto a roads\* tile.
        // sic: discarded again -- WalkerActor compares this against the entry distance below to
        // decide whether to head straight for the destination instead. See docs/engine-bugs.md.
        CalcSqDist(x, y, ptDestMaybe.x, ptDestMaybe.y);
        pEntry = g_worldBoard.FindNearestObjOfCategoryMaybe(0xd, x, y, 0x30);
        if (pEntry != 0) {
            nNetEntryPosXMaybe = pEntry->hotspotPosX;
            nNetEntryPosYMaybe = pEntry->hotspotPosY;
            unsigned int nEntrySqDist = CalcSqDist(x, y, pEntry->hotspotPosX, pEntry->hotspotPosY);
            if (nEntrySqDist > 0x900) {
                goto stop;
            }
            ptStepMaybe.x = pEntry->hotspotPosX;
            ptStepMaybe.y = pEntry->hotspotPosY;
            goto out;
        }
    }
stop:
    ptStepMaybe.x = -1;
    ptStepMaybe.y = -1;
out:
    ApplyLaneOffsetMaybe(&ptStepMaybe, nDirIn, nDirOut);
    ComputeStepTargetMaybe(ptStepMaybe.x, ptStepMaybe.y, 1);
}

// FUNCTION: LOCO 0x453d70
// vtable slot 23 -- the road-lane offset table.
//
// Directions are the tile graph's own 0-3 (0 = -x/west, 1 = +y/south, 2 = +x/east, 3 = -y/north;
// reverse == (d + 2) & 3), and the eight offsets that actually fire encode LEFT-HAND traffic:
// driving north the vehicle sits to the west of the tile centre, driving south to the east,
// driving east to the north and driving west to the south. The magnitudes are asymmetric
// between an axis and its reverse because the position being adjusted is the sprite's own
// top-left anchor rather than its middle.
//
// nDirOut then applies a second, smaller nudge for the turn about to be taken. Either direction
// may be 0xff ("none"), which lands past every case and leaves that half alone, and a (-1,-1)
// point -- the chooser's "gave up" sentinel -- is left alone entirely.
void RoadVehicleActor::ApplyLaneOffsetMaybe(POINT *pPos, unsigned char nDirIn,
                                            unsigned char nDirOut) {
    if (ArePositionsEqualMaybe(pPos->x, pPos->y, -1, -1)) {
        return;
    }
    switch (nDirIn) {
    case 0:
        // sic: this first inner switch spells out the "carried straight on" case, which does
        // nothing; the three below just omit their own direction. That one extra label is
        // load-bearing -- four dense cases is what makes VC5 emit a jump table here where the
        // three-case switches below all compile to `dec/je` compare chains.
        pPos->y += 2;
        switch (nDirOut) {
        case 0:
            break;
        case 1:
            pPos->x += 12;
            break;
        case 2:
            pPos->y -= 8;
            break;
        case 3:
            pPos->x -= 4;
            break;
        }
        break;
    case 1:
        pPos->x += 8;
        switch (nDirOut) {
        case 0:
            pPos->y -= 4;
            break;
        case 2:
            pPos->y -= 12;
            break;
        case 3:
            pPos->x -= 8;
            break;
        }
        break;
    case 2:
        pPos->y -= 8;
        switch (nDirOut) {
        case 0:
            pPos->y += 2;
            break;
        case 1:
            pPos->x += 4;
            break;
        case 3:
            pPos->x -= 12;
            break;
        }
        break;
    case 3:
        pPos->x -= 8;
        switch (nDirOut) {
        case 0:
            pPos->y += 4;
            break;
        case 1:
            pPos->x += 8;
            break;
        }
        break;
    }
}

// FUNCTION: LOCO 0x453eb0
// vtable slot 22 -- one movement sub-step.
//
// Like WalkerActor::AdvanceMovementMaybe this only moves every bFootprintYSteps-th tick --
// that divider IS the vehicle's speed -- and then advances bFootprintXSteps units toward
// ptStepMaybe. Unlike it, this one COMMITS the move first and validates afterwards, because the
// collision query it has to run is a query about the rect the vehicle would occupy. If either
// the collision or the standability test refuses, the move is undone; and unless the collision
// code was 7 the vehicle also gives up its current target, which is what makes traffic that has
// been sitting nose-to-tail eventually re-route rather than deadlock.
int RoadVehicleActor::AdvanceMovementMaybe() {
    POINT ptPrev;
    POINT pt;
    bool bWasReady = bReady;
    nSubTickCounterMaybe++;
    if (bWasReady && nSubTickCounterMaybe >= pKindDesc->bFootprintYSteps) {
        ptPrev.x = rect.left;
        ptPrev.y = rect.top;
        nSubTickCounterMaybe = 0;
        pt = ComputeStepTargetMaybe(ptStepMaybe.x, ptStepMaybe.y,
                                    pKindDesc->bFootprintXSteps);
        int nStepX = pt.x;
        int nStepY = pt.y;
        RepositionWithHotspot(nStepX, nStepY);
        int nCollision = DecorObjMgrMaybe_00485448.TestActorCollisionMaybe(rect, this);
        if (CanStandAtMaybe(nStepX, nStepY) != 0 && nCollision == 0) {
            UpdateFacingFromStepDeltaMaybe();
            return nStepDistanceMaybe;
        }
        RepositionWithHotspot(ptPrev.x, ptPrev.y);
        if (nCollision != 7) {
            // Non-virtual: this class does not override slot 17, and the original's own
            // `call 0x434260` at 0x453f90 is a direct call, not a vtable dispatch.
            DecorActorBase::HeadForObjectMaybe(0);
        }
    }
    return nStepDistanceMaybe;
}

// FUNCTION: LOCO 0x453fb0
// The driving pose: a full 8-way compass off the SIGNS of the pending step's two axes, rather
// than WalkerActor's 4-way dominant-axis test. The eight even subframes are laid out
// W, SW, S, SE, E, NE, N, NW -- i.e. 0/8 are the two horizontals, 4/12 the two verticals and
// the odd multiples of 2 the diagonals.
void RoadVehicleActor::UpdateFacingFromStepDeltaMaybe() {
    if (nStepDeltaXMaybe > 0) {
        if (nStepDeltaYMaybe > 0) {
            if (nSubFrame != 6) {
                ReleaseChannelAndDispatch(6);
            }
        } else if (nStepDeltaYMaybe < 0) {
            if (nSubFrame != 10) {
                ReleaseChannelAndDispatch(10);
            }
        } else {
            if (nSubFrame != 8) {
                ReleaseChannelAndDispatch(8);
            }
        }
    } else if (nStepDeltaXMaybe < 0) {
        if (nStepDeltaYMaybe > 0) {
            if (nSubFrame != 2) {
                ReleaseChannelAndDispatch(2);
            }
        } else if (nStepDeltaYMaybe < 0) {
            if (nSubFrame != 14) {
                ReleaseChannelAndDispatch(14);
            }
        } else {
            if (nSubFrame != 0) {
                ReleaseChannelAndDispatch(0);
            }
        }
    } else {
        // sic: standing perfectly still keeps whatever facing it already had -- there is no
        // final else here, unlike both branches above.
        if (nStepDeltaYMaybe > 0) {
            if (nSubFrame != 4) {
                ReleaseChannelAndDispatch(4);
            }
        } else if (nStepDeltaYMaybe < 0) {
            if (nSubFrame != 12) {
                ReleaseChannelAndDispatch(12);
            }
        }
    }
}

// FUNCTION: LOCO 0x454050
// vtable slot 21 -- "may I stand with my sprite's top-left at (x, y)". Two probe points, the
// middle of the sprite's left edge and its hotspot-relative bottom-right corner, each of which
// must land on a tile the vehicle is allowed to occupy:
//
//   * a roads\* or paths\* tile (categories 0xd and 0xc) -- always fine;
//   * a building\* or scenery\* tile (2 and 4) -- fine only OUTSIDE its footprint rect;
//   * a track\* tile (3) -- fine only if it is a level crossing whose barrier subframe is not 0;
//   * anything else, including empty board -- refused.
//
// Unlike WalkerActor::CanStandAtMaybe there is no "only bother testing while heading somewhere
// specific" shortcut in front of this; a vehicle is checked on every single step.
//
// EXACT again as of v416, after being an EFFECTIVE MATCH since v402 -- and NOTHING in this file
// changed to fix it. The v402 note below was right about the mechanism and wrong about the cure:
// the residual was six instructions of ecx<->edx swap with no source shape behind it, and what
// put them back was the DECLARATION COUNT of a class this TU only sees through a header --
// src/DecorObjMgrMaybe.h's PlacedObjRegistryMaybe gained its slots 19/20, which is the same
// "declaration-count / include-set dial re-rolls the whole TU's allocator state" dial that broke
// it in v402, turned one notch further. Flipped OFF again by the v485 registry base/derived
// split in this same header, and back ON at v518 by the 0x434500 ctor/registry-model edits --
// same dial, more turns. Kept as a note because the lesson is the transferable
// part: a pure-declaration header edit is a real lever on OTHER TUs' allocator state, in both
// directions, which is exactly why a full progress.py after any header change is mandatory.
unsigned char RoadVehicleActor::CanStandAtMaybe(int x, int y) {
    // Locals up front and one shared tail, the way the original must have: the second probe's
    // two early exits both jump to the same `mov al,[esp+0x13]` epilogue, and VC5 rejects a goto
    // that skips an initializer.
    RECT rcFootprint;
    POINT pt;
    TilePlacedObj *pTile;
    BigObj *pDesc;
    int nProbeX;
    int nProbeY;
    int nKindId;
    unsigned char bCanStand = 1;

    nProbeX = x + 4;
    nProbeY = y + (pKindDesc->nativeHeight >> 1) - 1;
    pTile = g_worldBoard.GetPlaneASlotMaybe(WORLD_TO_TILE(nProbeX), WORLD_TO_TILE(nProbeY), 0);
    if (pTile == 0) {
        bCanStand = 0;
    } else {
        pDesc = pTile->pKindDesc;
        switch (pDesc == 0 ? (unsigned char)0 : pDesc->categoryByte) {
        case 2:
        case 4:
            pTile->GetFootprintRectMaybe(&rcFootprint);
            pt.x = nProbeX;
            pt.y = nProbeY;
            if (!PtInRect(&rcFootprint, pt)) {
                bCanStand = 0;
            }
            break;
        case 3:
            nKindId = (pDesc == 0) ? -1 : pDesc->resourceId;
            if ((nKindId == KIND_TRACK_XSING1_V || nKindId == KIND_TRACK_XSING1_H) &&
                pTile->nSubFrame != 0) {
                break;
            }
            bCanStand = 0;
            break;
        case 0xc:
        case 0xd:
            break;
        default:
            bCanStand = 0;
            break;
        }
    }

    pDesc = pKindDesc;
    nProbeX = x + pDesc->nativeWidth - pDesc->hotspotX - 4;
    nProbeY = y + pDesc->nativeHeight - pDesc->hotspotY - 4;
    // sic: the second probe's tile lookup runs even when the first probe already refused.
    pTile = g_worldBoard.GetPlaneASlotMaybe(WORLD_TO_TILE(nProbeX), WORLD_TO_TILE(nProbeY), 0);
    if (bCanStand == 0 || pTile == 0) {
        bCanStand = 0;
        goto done;
    }
    pDesc = pTile->pKindDesc;
    switch (pDesc == 0 ? (unsigned char)0 : pDesc->categoryByte) {
    case 2:
    case 4:
        pTile->GetFootprintRectMaybe(&rcFootprint);
        pt.x = nProbeX;
        pt.y = nProbeY;
        if (!PtInRect(&rcFootprint, pt)) {
            bCanStand = 0;
        }
        break;
    case 3:
        nKindId = (pDesc == 0) ? -1 : pDesc->resourceId;
        if ((nKindId == KIND_TRACK_XSING1_V || nKindId == KIND_TRACK_XSING1_H) &&
            pTile->nSubFrame != 0) {
            break;
        }
        bCanStand = 0;
        break;
    case 0xc:
    case 0xd:
        break;
    default:
        bCanStand = 0;
        break;
    }
done:
    return bCanStand;
}
