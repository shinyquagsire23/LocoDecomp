// WalkerActor -- TileKind category 7, i.e. every `people\*` and `animals\*` kind (see
// DecorActor.h). This TU is the .text run 0x4326f0-0x433a20, which picks up exactly where
// MapWnd's own TU ends and runs until DecorActorBase's.
//
// The whole class lives here: construction/teardown, the per-tick brain (TickMaybe), the two
// destination/step choosers, the two movement steppers, the train boarding pair and the
// standability test.
#include "DecorActor.h"
#include "DecorObjMgrMaybe.h"
#include "GeomUtil.h"
#include "NameAnchorMaybe.h" // also pulls in PeerTrainNode.h (PeerTrainNodePartial)
#include "PlacementCursorMaybe.h"
#include "TrackGraph.h"
#include "WorldBoardMaybe.h"

#include <math.h>   // sqrt
#include <stdlib.h> // rand, abs

extern unsigned int g_dwGameTick; // DAT_004a99b4

// Per-consumer view adding the one extern train method this TU calls, kept OUT of the shared
// src/PeerTrainNode.h for the same reason as src/NameAnchorMaybe.h's own
// PeerTrainNodeTrackPartial: adding method declarations to that header rotated
// DPlaySessionMgr.cpp's TU codegen (the v325 bisect). Same layout, no fields added.
struct PeerTrainNodeWalkerPartial : PeerTrainNodePartial {
    unsigned char HasCarCategory2Maybe(); // 0x44c370, extern
};

// Minimal extern view for the one singleton method this TU calls into. Each consumer names
// its own view of DAT_004a9990 distinctly -- src/PeerTrainNode.cpp's
// NetSessionEventQueuePickPartial, src/DPlaySessionMgr.cpp's NetSessionEventQueueEdge,
// src/WidgetPicker.cpp's NetSessionEventQueuePartial, src/Main.cpp's
// NetSessionEventQueueWndProcView0x4618c0 -- since they are method-only views with no fields,
// so there is no shared layout to drift.
struct NetSessionEventQueueWanderPartial {
    void *PickRandomBigObjByCategory(int category); // 0x41e1f0, extern
};
extern NetSessionEventQueueWanderPartial g_NetSessionEventQueueWander; // DAT_004a9990

// The original's own (x, y) pair-equality predicate. Every site in this TU that compares two
// world positions -- and every "is this position the -1/-1 none sentinel" test -- goes through
// it, which is exactly what produces VC5's branchy 0/1 bool MATERIALIZATION at each one
// (`mov r,1; jmp; xor r,r; test rl,rl; je`) in place of the plain `cmp; jne` a bare inline
// `&&` compiles to. Same family as docs/CODEGEN.md's `sete`-materialized byte predicate,
// and kept TU-local for the same reason (hoisting predicates next to a shared header's extern
// rotates other TUs -- v340/v355/v356).
static inline bool ArePositionsEqualMaybe(int ax, int ay, int bx, int by) {
    return ax == bx && ay == by;
}

// Its inverse twin. The original really does have BOTH, rather than negating one at the call
// site: each site's materialization branches to its 1-result on the FIRST test that decides it
// (`jne` to 1 here, `jne` to 0 in the == predicate above), which is the || vs && short circuit
// showing through -- a `!` applied to the other predicate would instead invert an
// already-materialized 0/1.
static inline bool ArePositionsDifferentMaybe(int ax, int ay, int bx, int by) {
    return ax != bx || ay != by;
}

// The world->tile-grid conversion this TU does at 11 separate sites: an arithmetic shift down
// by 4 (16 px per tile) with everything left of the origin collapsing onto the -1 sentinel row
// or column rather than wrapping to a huge positive index. Written out at each site in the
// original -- see the `// sic:` note on the first use.

// FUNCTION: LOCO 0x4326f0
// The base ctor already sets nMoodMaybe to 4; this class redundantly sets it again, which is
// what makes its own store visible as a separate instruction.
WalkerActor::WalkerActor(int kindId) : DecorActorBase(kindId) {
    nMoodMaybe = 4;
    pRidingTrainMaybe = 0;
}

// FUNCTION: LOCO 0x432720 (??_GWalkerActor scalar dtor)

// FUNCTION: LOCO 0x432740
// Drops the two references the rest of the world can still be holding: the placement cursor's
// hover slot (picking a person up and deleting them mid-drag would otherwise dangle) and, if
// this actor is aboard a train, its seat in that train's passenger array.
WalkerActor::~WalkerActor() {
    if (PlacementCursorMaybe_004854c8.pHoverObjMaybe == this) {
        PlacementCursorMaybe_004854c8.SetHoverObjMaybe(NULL);
    }
    if (pRidingTrainMaybe != 0) {
        LeaveTrainMaybe();
    }
}

// FUNCTION: LOCO 0x4327b0
// vtable slot 15 -- this class's per-tick brain, and the only entry point
// DecorObjMgrMaybe::TickCategory7And8Maybe ever calls. pNextActor is the NEXT entry in the
// manager's own registry (see the base declaration): the tick loop walks the list pairwise so
// each actor can reach one neighbour's published trail anchor.
//
// Three states, in priority order: under population throttle just follow the neighbour; while
// still travelling (nStepDistanceMaybe != 0) keep stepping; otherwise, once the decision
// deadline has passed, ask the schedule where to go next.
void WalkerActor::TickMaybe(DecorActorBase *pNextActor) {
    if (bSuspendedMaybe) {
        return;
    }
    DecorActorBase::TickMaybe(pNextActor);
    if (nSubFrame > 7) {
        return;
    }
    if (DecorObjMgrMaybe_00485448.bThrottleMaybe && pNextActor != 0) {
        FollowLeaderStepMaybe(pNextActor);
    } else if (nStepDistanceMaybe != 0) {
        if (AdvanceMovementMaybe() == 0) {
            if (hotspotPosX == ptDestMaybe.x && hotspotPosY == ptDestMaybe.y) {
                OnArriveAtDestinationMaybe(nScheduleStateMaybe);
            } else {
                ChooseNextStepTileMaybe(hotspotPosX, hotspotPosY);
            }
        }
    } else if ((int)dwNextDecisionTickMaybe < (int)g_dwGameTick) {
        TilePlacedObj *pTarget;
        int nState = ChooseScheduleStateMaybe(g_dwGameTick);
        nScheduleStateMaybe = nState;
        switch (nState) {
        case 1:
            pTarget = pSpawnerObjMaybe;
            break;
        case 2:
            pTarget = pOwnerObjMaybe;
            break;
        case 3:
            // No shift is on, so wander: head for a random building\* (category 2) object, as
            // long as it is not the one we just came from.
            {
                TilePlacedObj *pWander =
                    (TilePlacedObj *)g_NetSessionEventQueueWander.PickRandomBigObjByCategory(2);
                if (pWander != 0) {
                    if (ArePositionsDifferentMaybe(pWander->hotspotPosX, pWander->hotspotPosY,
                                                   nPrevDestPosXMaybe, nPrevDestPosYMaybe)) {
                        SetDestinationTileMaybe(pWander->hotspotPosX, pWander->hotspotPosY);
                    }
                }
                // Nowhere reachable to wander to -- doze for 10-30 ticks and re-roll.
                if (ArePositionsEqualMaybe(ptStepMaybe.x, ptStepMaybe.y, -1, -1)) {
                    dwNextDecisionTickMaybe = rand() % 0x15 + 10 + g_dwGameTick;
                }
            }
            goto done;
        default:
            goto done;
        }
        // Non-virtual: WalkerActor does not override slot 17, and the original's own
        // `call 0x434260` at 0x432919 is a direct call, not a vtable dispatch.
        DecorActorBase::HeadForObjectMaybe(pTarget);
    }
done:
    if (nStepDistanceMaybe == 0 && bReady == true) {
        UpdateIdlePoseFromMoodMaybe();
    }
}

// FUNCTION: LOCO 0x432940
// vtable slot 16 -- adopt (x, y) as the final destination and resolve it onto the paths\*
// network. Records the previous destination first so a wander re-roll can avoid repeating it,
// then finds the nearest category-0xc tile to the destination and caches that tile's position
// and graph node id for ChooseNextStepTileMaybe to path against. A (-1,-1) destination, or a
// world whose track graphs are not built yet, just stops the actor where it stands.
void WalkerActor::SetDestinationTileMaybe(int x, int y) {
    dwLastRetargetTickMaybe = g_dwGameTick;
    if (ArePositionsEqualMaybe(x, y, -1, -1) || g_worldBoard.bTrackGraphsBuiltFlag == 0) {
        if (pRidingTrainMaybe == 0) {
            ComputeStepTargetMaybe(-1, -1, 1);
        }
        return;
    }

    if (ArePositionsDifferentMaybe(ptDestMaybe.x, ptDestMaybe.y, x, y)) {
        nPrevDestPosXMaybe = ptDestMaybe.x;
        ptDestMaybe.x = x;
        nPrevDestPosYMaybe = ptDestMaybe.y;
        ptDestMaybe.y = y;
    }

    TilePlacedObj *pDestTile =
        g_worldBoard.FindNearestObjOfCategoryMaybe(0xc, ptDestMaybe.x, ptDestMaybe.y, 0x900);
    if (pDestTile != 0) {
        ptDestTileMaybe.x = pDestTile->hotspotPosX;
        ptDestTileMaybe.y = pDestTile->hotspotPosY;
        if (pDestTile->nGraphNodeIdAMaybe == -1) {
            // A plain path tile between junctions is not itself a graph node, so path to the
            // cheapest-linked neighbour of it instead.
            unsigned char nDir = 0;
            for (unsigned char i = 1; i < 4; i++) {
                if (pDestTile->anNeighbourCostAMaybe[i] != 0 &&
                    (pDestTile->anNeighbourCostAMaybe[i] < pDestTile->anNeighbourCostAMaybe[nDir] ||
                     pDestTile->anNeighbourCostAMaybe[nDir] == 0)) {
                    nDir = i;
                }
            }
            if (pDestTile->apNeighbourTileAMaybe[nDir] == 0) {
                nDestGraphNodeIdMaybe = 0xff;
            } else {
                nDestGraphNodeIdMaybe = pDestTile->apNeighbourTileAMaybe[nDir]->nGraphNodeIdAMaybe;
            }
        } else {
            nDestGraphNodeIdMaybe = pDestTile->nGraphNodeIdAMaybe;
        }
    }

    if (pRidingTrainMaybe != 0) {
        BoardTrainMaybe((PeerTrainNodePartial *)pRidingTrainMaybe);
        return;
    }
    ChooseNextStepTileMaybe(hotspotPosX, hotspotPosY);
}

// FUNCTION: LOCO 0x432ae0
// vtable slot 18 -- the pathfinder, and the second-largest function in the app region. Given
// where the actor is standing NOW, decide the single next tile to walk to and write it into
// nStepPos{X,Y}.
//
// Three cases, in the order the body tests them:
//   * NOT on our own network -- the tile under (x, y) is neither a paths\* tile (category 0xc)
//     nor, while riding, a track\* tile (category 3). Find the nearest path tile, and head for
//     it unless the destination itself is closer than the way back onto the network.
//   * On the network, ON a graph node -- ask the graph which way to go and step to the node
//     across that edge. An 0xff verdict means the destination is unreachable from here, so
//     re-pick a destination node (slot 19) and start over.
//   * On the network, BETWEEN graph nodes -- no node id to ask the graph about, so pick the
//     cheapest-linked neighbour tile, then second-guess it: if heading that way would take us
//     back the way we came AND we are not already on the straight line to the destination,
//     take the reverse direction instead.
//
// Every branch ends at the same ComputeStepTargetMaybe call, which turns the chosen step
// target into the per-tick delta the movement steppers consume.
void WalkerActor::ChooseNextStepTileMaybe(int x, int y) {
    // sic: the -1 sentinel below is the original's own left-of-origin handling, repeated
    // verbatim at every world->tile conversion in this TU rather than factored into a helper.
    TilePlacedObj *pTile = g_worldBoard.GetPlaneASlotMaybe(WORLD_TO_TILE(x), WORLD_TO_TILE(y), 0);
    char nCategory;
    if (pTile == 0) {
        nCategory = 0;
    } else if (pTile->pKindDesc == 0) {
        nCategory = 0;
    } else {
        nCategory = pTile->pKindDesc->categoryByte;
    }

    if (nCategory != 0xc && (nCategory != 3 || pRidingTrainMaybe == 0)) {
        // Off-network: get back onto a paths\* tile, unless the destination is nearer.
        unsigned int nDestSqDist = CalcSqDist(x, y, ptDestMaybe.x, ptDestMaybe.y);
        TilePlacedObj *pEntry = g_worldBoard.FindNearestObjOfCategoryMaybe(0xc, x, y, 0x30);
        if (pEntry == 0) {
            goto stop;
        }
        nNetEntryPosXMaybe = pEntry->hotspotPosX;
        nNetEntryPosYMaybe = pEntry->hotspotPosY;
        unsigned int nEntrySqDist = CalcSqDist(x, y, pEntry->hotspotPosX, pEntry->hotspotPosY);
        if (nEntrySqDist > 0x900) {
            goto stop;
        }
        if (nDestSqDist < nEntrySqDist) {
            ptStepMaybe.x = ptDestMaybe.x;
            ptStepMaybe.y = ptDestMaybe.y;
        } else {
            ptStepMaybe.x = pEntry->hotspotPosX;
            ptStepMaybe.y = pEntry->hotspotPosY;
        }
    } else if (pTile->nGraphNodeIdAMaybe != -1) {
        // On a graph node.
        if ((unsigned int)pTile->nGraphNodeIdAMaybe != (unsigned int)nDestGraphNodeIdMaybe) {
            TrackGraph *pGraph = g_worldBoard.pTrackGraphAMaybe;
            unsigned int nDir = pGraph->GetStepDirectionMaybe(pTile->nGraphNodeIdAMaybe,
                                                              nDestGraphNodeIdMaybe);
            if (nDir == 0x80) {
                goto out;
            }
            if (nDir == 0xff) {
                // Destination unreachable from this node -- adopt a reachable one and retry.
                nDestGraphNodeIdMaybe = PickReachableDestNodeMaybe(pGraph, pTile->nGraphNodeIdAMaybe);
                TilePlacedObj *pNewDest = pGraph->papNode[nDestGraphNodeIdMaybe]->pTile;
                ptDestTileMaybe.x = pNewDest->hotspotPosX;
                ptDestTileMaybe.y = pNewDest->hotspotPosY;
                ChooseNextStepTileMaybe(x, y);
                goto out;
            }
            TrackGraphEdge *pEdge = pGraph->papNode[pTile->nGraphNodeIdAMaybe]->apEdgeMaybe[nDir];
            if (pEdge != 0) {
                // An edge stores both its endpoints, so pick whichever one is not us.
                unsigned int nNextId = pEdge->nNodeIdA;
                if (nNextId == (unsigned int)pTile->nGraphNodeIdAMaybe) {
                    nNextId = pEdge->nNodeIdB;
                }
                TilePlacedObj *pNextTile = pGraph->papNode[nNextId]->pTile;
                ptStepMaybe.x = pNextTile->hotspotPosX;
                ptStepMaybe.y = pNextTile->hotspotPosY;
                // Last hop: if the destination sits on the straight line from here to that
                // node, cut the corner and walk straight at it.
                if (nNextId == (unsigned int)nDestGraphNodeIdMaybe &&
                    IsPointOnSegmentMaybe(ptDestTileMaybe.x, ptDestTileMaybe.y,
                                          pTile->hotspotPosX, pTile->hotspotPosY,
                                          pNextTile->hotspotPosX, pNextTile->hotspotPosY)) {
                    ptStepMaybe.x = ptDestTileMaybe.x;
                    ptStepMaybe.y = ptDestTileMaybe.y;
                }
                if (nNextId != (unsigned int)nDestGraphNodeIdMaybe) {
                    pGraph->GetStepDirectionMaybe(nNextId, nDestGraphNodeIdMaybe);
                }
            }
            goto out;
        }
        goto atDestNode;
    } else {
        // On the network but between graph nodes.
        if (ArePositionsDifferentMaybe(x, y, ptDestTileMaybe.x, ptDestTileMaybe.y)) {
            unsigned char nDir = 0;
            for (unsigned char i = 1; i < 4; i++) {
                if (pTile->anNeighbourCostAMaybe[i] != 0 &&
                    (pTile->anNeighbourCostAMaybe[i] < pTile->anNeighbourCostAMaybe[nDir] ||
                     pTile->anNeighbourCostAMaybe[nDir] == 0)) {
                    nDir = i;
                }
            }
            TilePlacedObj *pNext = pTile->apNeighbourTileAMaybe[nDir];
            if (pNext == 0) {
                goto stop;
            }
            TrackGraph *pGraph = g_worldBoard.pTrackGraphAMaybe;
            unsigned int nGraphDir =
                pGraph->GetStepDirectionMaybe(pNext->nGraphNodeIdAMaybe, nDestGraphNodeIdMaybe);
            // The cheapest link points back the way the graph says to come FROM: prefer the
            // graph's own direction instead, but only when we are not already standing on the
            // straight line to the destination and that direction is actually linked.
            if (nGraphDir == (unsigned int)((nDir - 2) & 3) &&
                !IsPointOnSegmentMaybe(ptDestTileMaybe.x, ptDestTileMaybe.y,
                                       pTile->hotspotPosX, pTile->hotspotPosY,
                                       pTile->apNeighbourTileAMaybe[nDir]->hotspotPosX,
                                       pTile->apNeighbourTileAMaybe[nDir]->hotspotPosY) &&
                pTile->apNeighbourTileAMaybe[nGraphDir] != 0) {
                nDir = (unsigned char)nGraphDir;
            }
            TilePlacedObj *pStep = pTile->apNeighbourTileAMaybe[nDir];
            ptStepMaybe.x = pStep->hotspotPosX;
            ptStepMaybe.y = pStep->hotspotPosY;
            unsigned int nStepNodeId =
                (unsigned int)pTile->apNeighbourTileAMaybe[nDir]->nGraphNodeIdAMaybe;
            if (nStepNodeId == (unsigned int)nDestGraphNodeIdMaybe) {
                if (IsPointOnSegmentMaybe(ptDestTileMaybe.x, ptDestTileMaybe.y,
                                          pTile->hotspotPosX, pTile->hotspotPosY,
                                          pStep->hotspotPosX, pStep->hotspotPosY)) {
                    ptStepMaybe.x = ptDestTileMaybe.x;
                    ptStepMaybe.y = ptDestTileMaybe.y;
                } else if (IsPointOnSegmentMaybe(pTile->hotspotPosX, pTile->hotspotPosY,
                                                 ptDestTileMaybe.x, ptDestTileMaybe.y,
                                                 ptStepMaybe.x, ptStepMaybe.y)) {
                    ptStepMaybe.x = ptDestTileMaybe.x;
                    ptStepMaybe.y = ptDestTileMaybe.y;
                }
            }
            pGraph->GetStepDirectionMaybe(nStepNodeId, nDestGraphNodeIdMaybe);
            goto out;
        }
    atDestNode:
        // Standing on the destination tile itself.
        if (ArePositionsDifferentMaybe(x, y, ptDestTileMaybe.x, ptDestTileMaybe.y)) {
            ptStepMaybe.x = ptDestTileMaybe.x;
            ptStepMaybe.y = ptDestTileMaybe.y;
            goto out;
        }
        // Close enough to leave the network and walk the last stretch to the real destination.
        if (CalcSqDist(x, y, ptDestMaybe.x, ptDestMaybe.y) < 0x901) {
            ptStepMaybe.x = ptDestMaybe.x;
            ptStepMaybe.y = ptDestMaybe.y;
            goto out;
        }
    stop:
        ptStepMaybe.x = -1;
        ptStepMaybe.y = -1;
    }
out:
    ComputeStepTargetMaybe(ptStepMaybe.x, ptStepMaybe.y, 1);
}

// FUNCTION: LOCO 0x432fd0
// vtable slot 22 -- one movement sub-step.
//
// Aboard a train, the actor is simply pinned to car slot 0's position each tick, and once the
// train is running (sound state 0 or 1) and its lead anchor's tile is a station platform
// (m_type0x63a 0x12/0x13), it watches for the platform to come within 32 units of its own step
// target and gets off.
//
// On foot it only moves every bFootprintYSteps-th tick -- that divider IS the kind's
// walking speed -- and then advances by bFootprintXSteps units toward nStepPos. That new
// position has to clear BOTH gates to be taken: the standability test, and the track-tile gate
// declining to handle it itself. If either refuses, the actor just re-poses in place.
int WalkerActor::AdvanceMovementMaybe() {
    if (pRidingTrainMaybe == 0) {
        bool bWasReady = bReady;
        nSubTickCounterMaybe++;
        BigObj *pDesc = pKindDesc;
        if (bWasReady && nSubTickCounterMaybe >= pDesc->bFootprintYSteps) {
            nSubTickCounterMaybe = 0;
            POINT pt = ComputeStepTargetMaybe(ptStepMaybe.x, ptStepMaybe.y,
                                              pDesc->bFootprintXSteps);
            int nStepX = pt.x;
            int nStepY = pt.y;
            if (CanStandAtMaybe(nStepX, nStepY) == 1) {
                if (TryEnterTrackTileMaybe(nStepX, nStepY) == 0) {
                    UpdateFacingFromStepDeltaMaybe();
                    RepositionWithHotspot(nStepX, nStepY);
                    nTrailAnchorCounterMaybe++;
                    if (nTrailAnchorCounterMaybe > 10) {
                        nTrailAnchorPosXMaybe = hotspotPosX;
                        nTrailAnchorPosYMaybe = hotspotPosY;
                        nTrailAnchorCounterMaybe = 0;
                    }
                    return nStepDistanceMaybe;
                }
            }
            UpdateIdlePoseFromMoodMaybe();
        }
    } else {
        PeerTrainNodePartial *pTrain = (PeerTrainNodePartial *)pRidingTrainMaybe;
        // carSlots really holds CarNetObj*, which shares AnimDescRefObj0x477488's own layout
        // prefix -- so its hotspot position reads at the same offsets.
        TilePlacedObj *pCar = (TilePlacedObj *)pTrain->carSlots[0];
        RepositionWithHotspot(pCar->hotspotPosX, pCar->hotspotPosY);
        if (pTrain->dwSoundStateMaybe == 0 || pTrain->dwSoundStateMaybe == 1) {
            TrackTileObj *pAnchorTile = pTrain->pNameMaybe->pTileMaybe;
            if (pAnchorTile != 0 && pAnchorTile->pKindDesc->IsType0x63aInSet12()) {
                pAnchorTile = pTrain->pNameMaybe->pTileMaybe;
                nStepDistanceMaybe = (int)sqrt((double)CalcSqDist(ptStepMaybe.x, ptStepMaybe.y,
                                                                  pAnchorTile->hotspotPosX,
                                                                  pAnchorTile->hotspotPosY));
                if (nStepDistanceMaybe <= 0x20) {
                    LeaveTrainMaybe();
                }
            }
        }
    }
    return nStepDistanceMaybe;
}

// FUNCTION: LOCO 0x433160
// The idle/standing pose: a mood readout, drawn straight onto the minifig's own sprite strip.
// Dispatched through slot 7 (ReleaseChannelAndDispatch), which drops any playing sound before
// forwarding the new subframe on to slot 14.
void WalkerActor::UpdateIdlePoseFromMoodMaybe() {
    // sic: the three tests are written as unsigned `<=` relationals, not as `== 0` / `< 3` /
    // `< 5` -- VC5 emits comparisons literally, so the first one is a `test al,al; ja` rather
    // than the `jne` an equality test would give (docs/CODEGEN.md).
    unsigned char nMood = nMoodMaybe;
    if (nMood <= 0) {
        if (nSubFrame != 5) {
            ReleaseChannelAndDispatch(5);
        }
    } else if (nMood <= 2) {
        if (nSubFrame != 6) {
            ReleaseChannelAndDispatch(6);
        }
    } else if (nMood <= 4) {
        if (nSubFrame != 4) {
            ReleaseChannelAndDispatch(4);
        }
    } else {
        if (nSubFrame != 7) {
            ReleaseChannelAndDispatch(7);
        }
    }
}

// FUNCTION: LOCO 0x4331b0
// The walking pose: face along whichever axis of the pending step is the larger, which is what
// makes a diagonal walk render as the dominant compass direction. Subframes 0-3 are the four
// walk cycles (left, down, right, up).
// ⚠ The declaration order of the two locals is LOAD-BEARING -- it decides which delta VC5 loads
// first and therefore which of the two inlined abs() expansions gets esi vs edi. The WRONG order
// is always DIFF(10) at insns 58/58: identical content, one hoisted `mov esi,[ecx+0xd4]` and an
// esi/edi swap. But WHICH order is right is not a property of this function -- it is a parity bit
// flipped by the declaration count of every header this TU includes (see the MEASURED DIAL notes
// in src/AppWindow.h / src/MailWnd.h and docs/CODEGEN.md). It has now rotated twice:
//   v418  Y-then-X, after one added member-function declaration in src/WorldBoardMaybe.h
//   v423  X-then-Y, after four more (2 in DecorObjMgrMaybe.h, 1 in PeerTrainSlotQueueMaybe.h,
//         1 in WorldBoardMaybe.h) -- and each of those three headers flips it INDEPENDENTLY,
//         so the parity is over the total, not over any one header.
// If this ever reads DIFF(10) again, just swap these two lines rather than re-deriving anything.
// Swapping the COMPARISON instead -- abs(nDeltaY) < abs(nDeltaX) -- does not fix it (DIFF(2) at
// best): the dial is the load order, not the compare order.
void WalkerActor::UpdateFacingFromStepDeltaMaybe() {
    int nDeltaX = nStepDeltaXMaybe;
    int nDeltaY = nStepDeltaYMaybe;
    if (abs(nDeltaX) > abs(nDeltaY)) {
        if (nDeltaX > 0) {
            if (nSubFrame != 2) {
                ReleaseChannelAndDispatch(2);
            }
        } else {
            if (nSubFrame != 0) {
                ReleaseChannelAndDispatch(0);
            }
        }
    } else {
        if (nDeltaY > 0) {
            if (nSubFrame != 1) {
                ReleaseChannelAndDispatch(1);
            }
        } else {
            if (nSubFrame != 3) {
                ReleaseChannelAndDispatch(3);
            }
        }
    }
}

// FUNCTION: LOCO 0x433220
// vtable slot 23 -- the population-throttle stepper. Instead of any path work of its own, this
// actor steps one unit toward pLeader's published trail anchor, which is how a crowded board
// degrades into conga lines rather than into a hundred concurrent path searches.
//
// Unlike AdvanceMovementMaybe the two axes are committed SEPARATELY, each gated on its own
// standability test, so an actor blocked on one axis still slides along the other. If neither
// axis was accepted it nudges itself one unit off any axis it is exactly aligned on, which is
// what stops two followers from deadlocking on the same square.
int WalkerActor::FollowLeaderStepMaybe(DecorActorBase *pLeader) {
    bool bMoved = false;
    bool bWasReady = bReady;
    nSubTickCounterMaybe++;
    if ((!bWasReady && pRidingTrainMaybe == 0) || nSubTickCounterMaybe < 3) {
        return 1;
    }
    nSubTickCounterMaybe = 0;
    bReady = true;

    if (ArePositionsEqualMaybe(pLeader->nTrailAnchorPosXMaybe, pLeader->nTrailAnchorPosYMaybe,
                               -1, -1)) {
        // The leader has not published an anchor yet -- just re-pose in place.
        UpdateIdlePoseFromMoodMaybe();
    } else {
        POINT pt = ComputeStepTargetMaybe(pLeader->nTrailAnchorPosXMaybe,
                                          pLeader->nTrailAnchorPosYMaybe, 1);
        int nStepX = pt.x;
        int nStepY = pt.y;
        UpdateFacingFromStepDeltaMaybe();
        int nTop = rect.top;
        if (CanStandAtMaybe(nStepX, nTop) == 1) {
            RepositionWithHotspot(nStepX, nTop);
            bMoved = true;
        }
        int nLeft = rect.left;
        if (CanStandAtMaybe(nLeft, nStepY) == 1) {
            RepositionWithHotspot(nLeft, nStepY);
            bMoved = true;
        }
        if (!bMoved) {
            if (rect.left == nStepX) {
                RepositionWithHotspot(rect.left + 1, rect.top);
            }
            if (rect.top == nStepY) {
                RepositionWithHotspot(rect.left, rect.top + 1);
            }
        }
    }

    int nDistance = nStepDistanceMaybe;
    if (nDistance > 0) {
        nTrailAnchorCounterMaybe++;
    }
    if (nTrailAnchorCounterMaybe > 10) {
        nTrailAnchorPosXMaybe = hotspotPosX;
        nTrailAnchorPosYMaybe = hotspotPosY;
        nTrailAnchorCounterMaybe = 0;
    }
    return nDistance;
}

// FUNCTION: LOCO 0x433370
// The track-tile gate: everything special that happens when a minifig's next step lands on a
// track\* tile. Returns 1 for "handled, do not take the ordinary step", 0 for "carry on".
//
//   * a zebra crossing (`track\zebras-h`/`-v`, kind ids 0xc6c/0xc6e) -- walk straight over it,
//     unless its subframe is 1, which is the barrier-down/train-coming state;
//   * a station platform (m_type0x63a 0x12 horizontal / 0x13 vertical) -- board the train
//     standing there if there is one running, otherwise walk onto the platform: square up on
//     the platform's own axis (jittering to a random spot along it if already squared up) and
//     close in 4 units at a time on the other;
//   * anything else -- refuse, and let the caller step normally.
unsigned char WalkerActor::TryEnterTrackTileMaybe(int x, int y) {
    unsigned char bHandled = 0;
    TilePlacedObj *pTile = FindFootprintTileOfCategoryMaybe(3, x, y);
    if (pTile == 0) {
        return bHandled;
    }

    BigObj *pDesc = pTile->pKindDesc;
    bHandled = 1;
    int nKindId = (pDesc == 0) ? -1 : pDesc->resourceId;
    if (nKindId == 0xc6c || nKindId == 0xc6e) {
        if (pTile->nSubFrame == 1) {
            bHandled = 0;
        }
        return bHandled;
    }

    unsigned char nType = pDesc->m_type0x63a;
    if (nType == 0x12) {
        // Horizontal platform: line up on the platform's X, approach along Y.
        PeerTrainNodeWalkerPartial *pTrain =
            (PeerTrainNodeWalkerPartial *)((TrackTileObj *)pTile)->pPendingCoupleWaiter;
        if (pTrain != 0 &&
            (pTrain->dwSoundStateMaybe == 0 || pTrain->dwSoundStateMaybe == 1) &&
            pTrain->HasCarCategory2Maybe()) {
            BoardTrainMaybe(pTrain);
            return 1;
        }
        if (hotspotPosX == pTile->hotspotPosX) {
            ptStepMaybe.x = pTile->hotspotPosX +
                             (rand() % 2 ? -1 : 1) * (rand() % 0xb + 0xf);
        } else {
            ptStepMaybe.x = pTile->hotspotPosX;
        }
        if (hotspotPosY < pTile->hotspotPosY) {
            ptStepMaybe.y = hotspotPosY - 4;
        } else {
            ptStepMaybe.y = hotspotPosY + 4;
        }
        return 1;
    }
    if (nType == 0x13) {
        // Vertical platform: the same thing with the axes swapped.
        PeerTrainNodePartial *pTrain =
            (PeerTrainNodePartial *)((TrackTileObj *)pTile)->pPendingCoupleWaiter;
        if (pTrain != 0 &&
            (pTrain->dwSoundStateMaybe == 0 || pTrain->dwSoundStateMaybe == 1)) {
            BoardTrainMaybe(pTrain);
            return 1;
        }
        if (hotspotPosY == pTile->hotspotPosY) {
            ptStepMaybe.y = pTile->hotspotPosY - 5 + (rand() % 2 ? -1 : 1) * 0x12;
        } else {
            ptStepMaybe.y = pTile->hotspotPosY;
        }
        if (hotspotPosX < pTile->hotspotPosX) {
            ptStepMaybe.x = hotspotPosX - 4;
        } else {
            ptStepMaybe.x = hotspotPosX + 4;
        }
        return 1;
    }
    return bHandled;
}

// FUNCTION: LOCO 0x433530
// Board pTrain: claim the first of its 8 passenger slots that is free (or already ours). A
// null pTrain means "get off instead". Boarding cheers the minifig up by one and hides it (the
// train draws its own passengers); failing to find a free seat costs it a point of mood.
//
// Once aboard it walks the step chooser forward from the lead car's position for as long as
// that keeps landing on a paths\* tile and keeps producing a new position -- i.e. it pre-plans
// where it will get off before the train has moved at all.
unsigned char WalkerActor::BoardTrainMaybe(PeerTrainNodePartial *pTrain) {
    if (pTrain == 0) {
        if (pRidingTrainMaybe != 0) {
            LeaveTrainMaybe();
        }
        return 0;
    }

    for (unsigned char i = 0; i < 8; i++) {
        if (pTrain->apPassengerMaybe[i] == 0 || pTrain->apPassengerMaybe[i] == (int)this) {
            pTrain->apPassengerMaybe[i] = (int)this;
            pRidingTrainMaybe = (PeerTrainNode *)pTrain;
            break;
        }
    }

    unsigned char nMood = nMoodMaybe;
    if (pRidingTrainMaybe == 0) {
        if (nMood != 0) {
            nMoodMaybe = nMood - 1;
        }
        return 0;
    }

    if (nMood < 7) {
        nMoodMaybe = nMood + 1;
    }
    bReady = false;
    TilePlacedObj *pAnchorTile =
        ((PeerTrainNodePartial *)pRidingTrainMaybe)->pNameMaybe->pTileMaybe;
    if (pAnchorTile == 0) {
        return 0;
    }

    int nPrevX = -1;
    int nPrevY = -1;
    ptStepMaybe.x = pAnchorTile->hotspotPosX;
    ptStepMaybe.y = pAnchorTile->hotspotPosY;
    char nCategory = 3;
    TilePlacedObj *pTile;
    do {
        if (nCategory != 3) {
            break;
        }
        if (!ArePositionsDifferentMaybe(nPrevX, nPrevY, ptStepMaybe.x, ptStepMaybe.y)) {
            break;
        }
        nPrevY = ptStepMaybe.y;
        nPrevX = ptStepMaybe.x;
        ChooseNextStepTileMaybe(nPrevX, nPrevY);
        pTile = g_worldBoard.GetPlaneASlotMaybe(WORLD_TO_TILE(ptStepMaybe.x),
                                                WORLD_TO_TILE(ptStepMaybe.y), 0);
        if (pTile == 0) {
            break;
        }
        if (pTile->pKindDesc == 0) {
            nCategory = 0;
        } else {
            nCategory = pTile->pKindDesc->categoryByte;
        }
    } while (pTile != 0);
    return 1;
}

// FUNCTION: LOCO 0x4336a0
// Get off the train: release our passenger slot, then work out where to stand. The drop-off
// point is the lead anchor's tile offset by our own sprite hotspot, jittered along the
// platform for the two platform orientations; if that lands on anything but a paths\* tile we
// fall back to pSpawnerObjMaybe's position instead of stranding the minifig on the track.
void WalkerActor::LeaveTrainMaybe() {
    PeerTrainNodePartial *pTrain = (PeerTrainNodePartial *)pRidingTrainMaybe;
    if (pTrain == 0) {
        return;
    }

    for (unsigned char i = 0; i < 8; i++) {
        if (pTrain->apPassengerMaybe[i] == (int)this) {
            pTrain->apPassengerMaybe[i] = 0;
            break;
        }
    }

    int nDropX;
    int nDropY;
    TilePlacedObj *pAnchorTile =
        ((PeerTrainNodePartial *)pRidingTrainMaybe)->pNameMaybe->pTileMaybe;
    if (pAnchorTile == 0) {
        nDropX = ptStepMaybe.x - pKindDesc->hotspotX;
        nDropY = ptStepMaybe.y - pKindDesc->hotspotY;
    } else {
        unsigned char nType = pAnchorTile->pKindDesc->m_type0x63a;
        if (nType == 0x12) {
            nDropX = (rand() % 2 ? -1 : 1) * (rand() % 0xb + 0xf) - pKindDesc->hotspotX +
                     ptStepMaybe.x;
            nDropY = ptStepMaybe.y - pKindDesc->hotspotY;
        } else if (nType == 0x13) {
            nDropX = ptStepMaybe.x - pKindDesc->hotspotX;
            // sic: the `* 15` really is INSIDE the test, where it cannot change the result --
            // `rand() % 2 * 15` is 0 or 15, so this is just a coin flip. The original emits it
            // (`lea eax,[eax+eax*2]; lea eax,[eax+eax*4]; neg; sbb; and al,0xfe; inc`) before
            // folding to +/-1 and landing in `lea ebp,[eax+ecx*1-0x5]`, so it is in the source.
            // Note the jitter here is only +/-1 either side of a 5-unit step back, NOT the
            // +/-18 of the same platform orientation's own case in TryEnterTrackTileMaybe.
            nDropY = (ptStepMaybe.y - pKindDesc->hotspotY) - 5 + (rand() % 2 * 15 ? -1 : 1);
        } else {
            nDropX = ptStepMaybe.x - pKindDesc->hotspotX;
            nDropY = ptStepMaybe.y - pKindDesc->hotspotY;
        }
    }

    TilePlacedObj *pDropTile =
        g_worldBoard.GetPlaneASlotMaybe(WORLD_TO_TILE(nDropX), WORLD_TO_TILE(nDropY), 0);
    char nCategory = 0;
    if (pDropTile != 0) {
        if (pDropTile->pKindDesc == 0) {
            nCategory = 0;
        } else {
            nCategory = pDropTile->pKindDesc->categoryByte;
        }
    }
    if (nCategory != 0xc) {
        TilePlacedObj *pFallback = pSpawnerObjMaybe;
        if (pFallback != 0) {
            nDropX = pFallback->hotspotPosX;
            nDropY = pFallback->hotspotPosY;
        }
    }

    RepositionWithHotspot(nDropX, nDropY);
    int nStepY = ptStepMaybe.y;
    int nStepX = ptStepMaybe.x;
    pRidingTrainMaybe = 0;
    ChooseNextStepTileMaybe(nStepX, nStepY);
    bReady = true;
}

// FUNCTION: LOCO 0x433860
// vtable slot 21 -- "may I stand with my sprite's top-left at (x, y)". Two probe points are
// tested, the middle of the sprite's left edge and its bottom-right corner: each must land
// either on a tile with no descriptor, on a paths\* tile (category 0xc), or OUTSIDE the
// footprint rect of whatever category-2/3/4 (building/track/scenery) tile it hits.
//
// The whole test is skipped -- and 1 returned outright -- unless the actor is heading for its
// network entry point, heading for its final destination, or being dragged by the placement
// cursor, which is what lets a minifig walk through scenery on an ordinary path step.
unsigned char WalkerActor::CanStandAtMaybe(int x, int y) {
    // Locals declared up front, the way the original must have: every early exit jumps to the
    // one shared `done` tail (the original's own `jne 0x1ac`/`je 0x1ac` into a single
    // `mov al,[esp+0x13]; pop..; ret 8`), and VC5 rejects a goto that skips an initializer.
    RECT rcFootprint;
    POINT pt;
    TilePlacedObj *pTile;
    BigObj *pDesc;
    int nProbeX;
    int nProbeY;
    int nCategory;
    unsigned char bCanStand = 1;

    if (!ArePositionsEqualMaybe(ptStepMaybe.x, ptStepMaybe.y,
                                nNetEntryPosXMaybe, nNetEntryPosYMaybe)) {
        if (!ArePositionsEqualMaybe(ptStepMaybe.x, ptStepMaybe.y,
                                    ptDestMaybe.x, ptDestMaybe.y)) {
            if (PlacementCursorMaybe_004854c8.pHoverObjMaybe != this) {
                goto done;
            }
            if (!PlacementCursorMaybe_004854c8.bHoverActiveMaybe) {
                goto done;
            }
        }
    }

    nProbeX = x + 4;
    nProbeY = y + (pKindDesc->nativeHeight >> 1) - 1;
    pTile = g_worldBoard.GetPlaneASlotMaybe(WORLD_TO_TILE(nProbeX), WORLD_TO_TILE(nProbeY), 0);
    if (pTile != 0) {
        // sic: the category byte is widened to a SIGNED int before the range tests -- the
        // original's `and ecx,0xff; cmp ecx,2; jl` is a signed compare on a zero-extended
        // byte, which is what an `int` local holding a byte field gives.
        if (pTile->pKindDesc == 0) {
            nCategory = 0;
        } else {
            nCategory = pTile->pKindDesc->categoryByte;
        }
        if (nCategory >= 2) {
            if (nCategory > 4) {
                if (nCategory == 0xc) {
                    goto secondProbe;
                }
            } else {
                pTile->GetFootprintRectMaybe(&rcFootprint);
                pt.x = nProbeX;
                pt.y = nProbeY;
                if (PtInRect(&rcFootprint, pt)) {
                    goto secondProbe;
                }
            }
        }
        bCanStand = 0;
    }

secondProbe:
    pDesc = pKindDesc;
    nProbeY = y + pDesc->nativeHeight - 4;
    nProbeX = x + pDesc->nativeWidth - 4;
    pTile = g_worldBoard.GetPlaneASlotMaybe(WORLD_TO_TILE(nProbeX), WORLD_TO_TILE(nProbeY), 0);
    if (pTile != 0) {
        if (pTile->pKindDesc == 0) {
            nCategory = 0;
        } else {
            nCategory = pTile->pKindDesc->categoryByte;
        }
        if (nCategory >= 2) {
            if (nCategory > 4) {
                if (nCategory == 0xc) {
                    goto done;
                }
            } else {
                pTile->GetFootprintRectMaybe(&rcFootprint);
                pt.x = nProbeX;
                pt.y = nProbeY;
                if (PtInRect(&rcFootprint, pt)) {
                    goto done;
                }
            }
        }
        bCanStand = 0;
    }
done:
    return bCanStand;
}
