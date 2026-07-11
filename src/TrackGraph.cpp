// TrackGraph -- the navigation graph the world board keeps over its placed tiles. This TU is
// the .text run 0x45cdf0-0x45de40, sitting between the world-loader helpers below it and
// App_LoadWorldThreadProcMaybe above it.
//
// The board owns TWO instances (WorldBoardMaybe::pTrackGraphAMaybe / pTrackGraphBMaybe,
// +0x52488/+0x5248c) and constructs them side by side in WorldBoardMaybe_InitFieldsMaybe as
// `new TrackGraph(7)` and `new TrackGraph(8)` -- graph A over the `paths\*` network WalkerActor
// walks, graph B over the `roads\*` network RoadVehicleActor drives.
//
// The model is a JUNCTION graph, not a tile graph: only tiles the board calls a junction get a
// TrackGraphNode, and a plain unbranching run of path/road between two junctions collapses into
// a single TrackGraphEdge whose nCostMaybe is the run's length. That is why every placed tile
// carries a node-id slot that is -1 most of the time (TilePlacedObj::nGraphNodeIdAMaybe /
// nGraphNodeIdBMaybe) -- a -1 is exactly what sends both pathfinders to their per-tile cost
// argmin instead of to this graph.
//
// Routing is precomputed, not searched at run time. BuildAllRoutesMaybe runs a full
// all-pairs pass -- for every node, a depth-first cheapest-cost relaxation
// (PropagateDistanceMaybe) builds a spanning tree of node COPIES, then SolveRouteMaybe walks
// that tree once per destination and stamps the first step of the winning route into
// paDirMatrix. Afterwards each actor only ever calls GetStepDirectionMaybe, which is a single
// packed-matrix lookup. paDirMatrix is LOWER-TRIANGULAR and holds BOTH directions of a node pair
// in one byte (bits 0-1 and bits 2-3), so an N-node graph costs N*(N-1)/2 bytes rather than N^2.
#include "GameWindowWidgetList.h"
#include "Obj0x477798Family.h"
#include "TilePlacedObj.h"
#include "TrackGraph.h"
#include "WorldBoardMaybe.h"

#include <windows.h> // OutputDebugStringA
#include <stdlib.h>  // malloc, free
#include <string.h>  // memset

extern int g_nScreenState; // DAT_004851f4

// The reverse of a path direction. Never emitted standalone -- it is inlined at all three of its
// call sites (both BuildAdjacency twins and StampRoutePairsMaybe), each of which reproduces the
// same four-case jump-free sequence and the same OutputDebugStringA on the impossible default.
// REFUTED PROBE (v431, and RE-RUN and re-refuted v482 once lever #36 had changed the inner
// loop's shape -- the two are independent). Do NOT run it a third time: the parameter LOOKS
// int-wide -- every inlined copy
// range-checks a raw dword (`mov eax,[esp+0x14]; cmp eax,3; ja`) with no `movzx`/`and` in
// front of it, while this byte spelling costs one extra `and eax,0xff` per copy. But widening
// it to `int` and dropping the two `(unsigned char)` casts below makes VC5 abandon the inner
// direction loop's POINTER strength-reduction in both BuildAdjacency twins -- the original's
// `lea ebx,[eax+0xc4]` / `lea esi,[ecx+8]` / `add ebx,4` / `add esi,4` collapses into scaled
// `[eax+esi*4+0xc4]` indexing, and it rotates ebx/ebp globally. Net: 169236 -> 289823 on
// 0x45ce40. The byte parameter is worth one masked instruction to keep the loop shape.
static inline unsigned char GetOppositePathMaybe(unsigned char bDir) {
    unsigned char bOpposite;
    switch (bDir) {
    case 0:
        bOpposite = 2;
        break;
    case 1:
        bOpposite = 3;
        break;
    case 2:
        bOpposite = 0;
        break;
    case 3:
        bOpposite = 1;
        break;
    default:
        OutputDebugStringA("ERROR: Invalid path in GetOppositePath\n");
        bOpposite = 0xff;
    }
    return bOpposite;
}

// FUNCTION: LOCO 0x45cdf0
TrackGraph::TrackGraph(short nGraphKind) {
    nNodeCount = 0;
    paDirMatrix = 0;
    papNode = 0;
    nGraphKindMaybe = nGraphKind;
}

// FUNCTION: LOCO 0x45ce10
TrackGraph::~TrackGraph() {
    ClearMaybe();
    if (paDirMatrix != 0) {
        free(paDirMatrix);
        paDirMatrix = 0;
    }
}

// FUNCTION: LOCO 0x45d8c0
// Tear the graph back down to empty. Edges are SHARED by the two nodes they join, so each one
// has to be unhooked from its far end before it is deleted or the second visit would double-free
// it -- that is what the inner rescan of the far node's four slots is for.
void TrackGraph::ClearMaybe() {
    unsigned int i;

    if (papNode != 0) {
        for (i = 0; i < nNodeCount; i++) {
            for (unsigned int dir = 0; dir < 4; dir++) {
                TrackGraphEdge *pEdge = papNode[i]->apEdgeMaybe[dir];
                if (pEdge != 0) {
                    unsigned int nFar = pEdge->nNodeIdA;
                    if (nFar == i) {
                        nFar = pEdge->nNodeIdB;
                    }
                    for (unsigned int dirFar = 0; dirFar < 4; dirFar++) {
                        if (papNode[nFar]->apEdgeMaybe[dirFar] == pEdge) {
                            papNode[nFar]->apEdgeMaybe[dirFar] = 0;
                        }
                    }
                    delete pEdge;
                }
            }
        }
        for (i = 0; i < nNodeCount; i++) {
            delete papNode[i];
            papNode[i] = 0;
        }
        free(papNode);
        nNodeCount = 0;
        papNode = 0;
    }
}

// PARKED at DIFF(317)/DIFF(316) (v482), insns 283/274 on the A twin -- and the residual is now
// down to THREE instructions on a 274-instruction function. The two scratch-collection expansions
// this note used to blame (the ctor's inlined from-empty alloc, and Add() emitted as a call) were
// BOTH fixed in v431 by moving the ctor body to `{ m_count = 0; m_ptr = 0; ReserveMaybe(n); }`
// and Add() in-class in src/Obj0x477798Family.h; that lever is SPENT, and this note claimed
// otherwise for three sessions. Do not go looking for it again.
//
// What v482 fixed: the inner direction loop needed CODEGEN LEVER #36 -- `dir` is still live in the
// body (GetOppositePathMaybe reads it), so cl will not synthesise the reversed counter and the
// source must carry an explicit second countdown (`dir = 0; for (nDir = 4; nDir != 0; nDir--)`).
// Worth DIFF(384)->DIFF(317) on A and DIFF(388)->DIFF(316) on B, and the twins agree again.
//
// What is LEFT, all three individually diagnosed as intrinsic:
//   1. `and eax,0xff` -- one masked instruction the original does not have. It is the documented
//      price of GetOppositePathMaybe's BYTE parameter; see the REFUTED PROBE above it, which
//      v482 RE-RAN on the premise that #36 had changed the loop shape. It has not: an `int`
//      parameter still collapses the original's `lea ebx,[eax+0xc4]`/`lea esi,[ecx+8]` pointer
//      induction into scaled `[eax+esi*4+0xc4]` indexing AND rotates ebx/ebp globally
//      (317 -> 566). The mask is cheaper than the rotation. CLOSED, twice.
//   2. `cmp eax,ebx` vs our `test eax,eax` at +0xdb -- the zero-register class (the original
//      keeps 0 live in ebx for the neighbouring field stores and compares against it).
//   3. one scheduling swap at +0x238 (`mov edx,[esi]` / `mov eax,[esp+0x14]` ordering).
// The bodies are complete and in the right registers. Do NOT re-grind this file.
// FUNCTION: LOCO 0x45ce40
// Rebuild graph A from the board's currently placed tiles. Four passes: collect the junction
// tiles into a scratch collection (resetting every tile's node id on the way), allocate the node
// array, let the board relink each tile's four neighbour slots, then turn those slots into
// shared edges. Finally the direction matrix is reallocated to fit and stamped all-0x80 -- the
// "not solved yet" verdict BuildAllRoutesMaybe later overwrites.
void TrackGraph::BuildAdjacencyAMaybe() {
    Obj0x477758 nodeTiles(10);
    // The read side of the scratch collection goes through this BASE REFERENCE, and that
    // is load-bearing, not decoration: VC5 devirtualizes a call written on the object
    // itself (`list.GetAt(i)` compiles to a direct `call ?GetAt@Obj0x477758Base`),
    // where the original re-reads the vtable at all three sites -- `mov edx,[esp+0x2c];
    // call [edx+0x20]` for the two GetAt uses and `call [eax+0xc]` for RemoveAt. Reaching
    // them through a reference (or an equivalent base pointer -- both score identically)
    // suppresses that and is worth 149234 -> 112983 on this function alone.
    // `Add` below deliberately does NOT go through it: Add is in-class, so the direct
    // object call is what lets VC5 inline its body the way the original does. Routing Add
    // through the reference too costs 277969 (insns 260/273 -- the body stops inlining).
    Obj0x477758Base &list = nodeTiles;
    unsigned int i;

    ClearMaybe();

    for (i = 0; i < g_gameWindowWidgetList.nItemCount; i++) {
        TilePlacedObj *pTile =
            (TilePlacedObj *)((GameWindowWidgetListProbe *)&g_gameWindowWidgetList)->GetItemImpl(i);
        pTile->nGraphNodeIdAMaybe = -1;
        if (g_worldBoard.IsTrackGraphNodeAMaybe(pTile, nGraphKindMaybe)) {
            nodeTiles.Add(pTile);
        }
    }

    nNodeCount = nodeTiles.m_0c;
    if (nNodeCount > 0) {
        papNode = (TrackGraphNode **)malloc(nNodeCount * sizeof(TrackGraphNode *));
        memset(papNode, 0, nNodeCount * sizeof(TrackGraphNode *));
    }

    for (i = 0; i < nNodeCount; i++) {
        papNode[i] = new TrackGraphNode;
        memset(papNode[i], 0, sizeof(TrackGraphNode));
        papNode[i]->nNodeIdMaybe = i;
        ((TilePlacedObj *)list.GetAt(i))->nGraphNodeIdAMaybe = i;
    }

    for (i = 0; i < g_gameWindowWidgetList.nItemCount; i++) {
        g_worldBoard.LinkTrackGraphNeighboursAMaybe(
            (TilePlacedObj *)((GameWindowWidgetListProbe *)&g_gameWindowWidgetList)
                ->GetItemImpl(i));
    }

    for (i = 0; i < nNodeCount; i++) {
        TilePlacedObj *pTile = (TilePlacedObj *)list.GetAt(i);
        TrackGraphNode *pNode = papNode[i];
        pNode->pTile = pTile;
        int dir = 0;
        for (int nDir = 4; nDir != 0; nDir--) {
            if (pTile->apNeighbourTileAMaybe[dir] == 0) {
                pNode->apNextNodeMaybe[dir] = 0;
                pNode->apEdgeMaybe[dir] = 0;
            } else {
                unsigned int nFar = pTile->apNeighbourTileAMaybe[dir]->nGraphNodeIdAMaybe;
                if (nFar < nNodeCount) {
                    pNode->apNextNodeMaybe[dir] = papNode[nFar];
                    if (pNode->apEdgeMaybe[dir] == 0) {
                        pNode->apEdgeMaybe[dir] = new TrackGraphEdge;
                        pNode->apEdgeMaybe[dir]->Unk0x4 = 0;
                        pNode->apEdgeMaybe[dir]->nCostMaybe = pTile->anNeighbourCostAMaybe[dir];
                        pNode->apEdgeMaybe[dir]->nNodeIdA = i;
                        pNode->apEdgeMaybe[dir]->nNodeIdB = nFar;
                        papNode[nFar]->apEdgeMaybe[GetOppositePathMaybe((unsigned char)dir)] =
                            pNode->apEdgeMaybe[dir];
                    }
                }
            }
            dir++;
        }
    }

    while (nodeTiles.m_0c != 0) {
        list.RemoveAt(nodeTiles.m_0c - 1);
    }

    if (paDirMatrix != 0) {
        free(paDirMatrix);
        paDirMatrix = 0;
    }
    if (nNodeCount > 0) {
        unsigned int nEntries = (nNodeCount - 1) * nNodeCount / 2;
        paDirMatrix = (unsigned char *)malloc(nEntries);
        memset(paDirMatrix, 0x80, nEntries); // idiom-exempt: byte matrix -- nEntries IS the byte count
    }
}

// FUNCTION: LOCO 0x45d1c0
// Graph B's twin of BuildAdjacencyAMaybe -- the same code against the tile's graph-B neighbour
// slot and node-id, with the board's B-side predicate and relink helper. See the dead
// nGraphKindMaybe == 7 branch noted in src/TrackGraph.h.
void TrackGraph::BuildAdjacencyBMaybe() {
    Obj0x477758 nodeTiles(10);
    // The read side of the scratch collection goes through this BASE REFERENCE, and that
    // is load-bearing, not decoration: VC5 devirtualizes a call written on the object
    // itself (`list.GetAt(i)` compiles to a direct `call ?GetAt@Obj0x477758Base`),
    // where the original re-reads the vtable at all three sites -- `mov edx,[esp+0x2c];
    // call [edx+0x20]` for the two GetAt uses and `call [eax+0xc]` for RemoveAt. Reaching
    // them through a reference (or an equivalent base pointer -- both score identically)
    // suppresses that and is worth 149234 -> 112983 on this function alone.
    // `Add` below deliberately does NOT go through it: Add is in-class, so the direct
    // object call is what lets VC5 inline its body the way the original does. Routing Add
    // through the reference too costs 277969 (insns 260/273 -- the body stops inlining).
    Obj0x477758Base &list = nodeTiles;
    unsigned int i;

    ClearMaybe();

    for (i = 0; i < g_gameWindowWidgetList.nItemCount; i++) {
        TilePlacedObj *pTile =
            (TilePlacedObj *)((GameWindowWidgetListProbe *)&g_gameWindowWidgetList)->GetItemImpl(i);
        pTile->nGraphNodeIdBMaybe = -1;
        if (g_worldBoard.IsTrackGraphNodeBMaybe(pTile, nGraphKindMaybe)) {
            nodeTiles.Add(pTile);
        }
    }

    nNodeCount = nodeTiles.m_0c;
    if (nNodeCount > 0) {
        papNode = (TrackGraphNode **)malloc(nNodeCount * sizeof(TrackGraphNode *));
        memset(papNode, 0, nNodeCount * sizeof(TrackGraphNode *));
    }

    for (i = 0; i < nNodeCount; i++) {
        papNode[i] = new TrackGraphNode;
        memset(papNode[i], 0, sizeof(TrackGraphNode));
        papNode[i]->nNodeIdMaybe = i;
        if (nGraphKindMaybe == 7) {
            ((TilePlacedObj *)list.GetAt(i))->nGraphNodeIdAMaybe = i; // sic: dead branch
        } else {
            ((TilePlacedObj *)list.GetAt(i))->nGraphNodeIdBMaybe = i;
        }
    }

    for (i = 0; i < g_gameWindowWidgetList.nItemCount; i++) {
        g_worldBoard.LinkTrackGraphNeighboursBMaybe(
            (TilePlacedObj *)((GameWindowWidgetListProbe *)&g_gameWindowWidgetList)
                ->GetItemImpl(i));
    }

    for (i = 0; i < nNodeCount; i++) {
        TilePlacedObj *pTile = (TilePlacedObj *)list.GetAt(i);
        TrackGraphNode *pNode = papNode[i];
        pNode->pTile = pTile;
        int dir = 0;
        for (int nDir = 4; nDir != 0; nDir--) {
            if (pTile->apNeighbourTileBMaybe[dir] == 0) {
                pNode->apNextNodeMaybe[dir] = 0;
                pNode->apEdgeMaybe[dir] = 0;
            } else {
                unsigned int nFar = pTile->apNeighbourTileBMaybe[dir]->nGraphNodeIdBMaybe;
                if (nFar < nNodeCount) {
                    pNode->apNextNodeMaybe[dir] = papNode[nFar];
                    if (pNode->apEdgeMaybe[dir] == 0) {
                        pNode->apEdgeMaybe[dir] = new TrackGraphEdge;
                        pNode->apEdgeMaybe[dir]->Unk0x4 = 0;
                        pNode->apEdgeMaybe[dir]->nCostMaybe = pTile->anNeighbourCostBMaybe[dir];
                        pNode->apEdgeMaybe[dir]->nNodeIdA = i;
                        pNode->apEdgeMaybe[dir]->nNodeIdB = nFar;
                        papNode[nFar]->apEdgeMaybe[GetOppositePathMaybe((unsigned char)dir)] =
                            pNode->apEdgeMaybe[dir];
                    }
                }
            }
            dir++;
        }
    }

    while (nodeTiles.m_0c != 0) {
        list.RemoveAt(nodeTiles.m_0c - 1);
    }

    if (paDirMatrix != 0) {
        free(paDirMatrix);
        paDirMatrix = 0;
    }
    if (nNodeCount > 0) {
        unsigned int nEntries = (nNodeCount - 1) * nNodeCount / 2;
        paDirMatrix = (unsigned char *)malloc(nEntries);
        memset(paDirMatrix, 0x80, nEntries); // idiom-exempt: byte matrix -- nEntries IS the byte count
    }
}

// FUNCTION: LOCO 0x45d810
// INLINE, and defined here rather than at its address-order position further down, because
// BuildRoutesFromNodeMaybe below carries one level of it expanded in place: that call site ends
// in the loop body plus a direct `operator delete`, with only the RECURSIVE step left as a call
// to this out-of-line copy -- which is exactly what an inline self-recursive function produces.
inline void TrackGraph::FreeRouteTreeMaybe(TrackGraphNode *pNode) {
    if (pNode != 0) {
        for (int dir = 0; dir < 4; dir++) {
            if (pNode->apNextNodeMaybe[dir] != 0) {
                FreeRouteTreeMaybe(pNode->apNextNodeMaybe[dir]);
            }
        }
        delete pNode;
    }
}

// FUNCTION: LOCO 0x45d560
// The all-pairs pass: one scratch set for the whole run, one relaxation per source node, then
// the scratch goes away again. Nothing here is reentrant -- the five scratch members live only
// between these mallocs and frees.
void TrackGraph::BuildAllRoutesMaybe() {
    pabVisitedMaybe = (char *)malloc(nNodeCount);
    panBestCostMaybe = (unsigned int *)malloc(nNodeCount * sizeof(unsigned int));
    papRouteTreeMaybe = (TrackGraphNode **)malloc(nNodeCount * sizeof(TrackGraphNode *));
    papRouteTreeParentMaybe = (TrackGraphNode **)malloc(nNodeCount * sizeof(TrackGraphNode *));

    for (unsigned int i = 0; i < nNodeCount; i++) {
        BuildRoutesFromNodeMaybe(i);
    }

    free(panBestCostMaybe);
    free(papRouteTreeMaybe);
    free(papRouteTreeParentMaybe);
    free(pabVisitedMaybe);
}

// FUNCTION: LOCO 0x45d5f0
// Everything reachable from nFrom, in one relaxation plus one route solve per destination.
void TrackGraph::BuildRoutesFromNodeMaybe(unsigned int nFrom) {
    unsigned int i;

    for (i = 0; i < nNodeCount; i++) {
        panBestCostMaybe[i] = 0xffffffff;
    }
    memset(papRouteTreeMaybe, 0, nNodeCount * sizeof(TrackGraphNode *));
    memset(papRouteTreeParentMaybe, 0, nNodeCount * sizeof(TrackGraphNode *));
    memset(pabVisitedMaybe, 0, nNodeCount); // idiom-exempt: byte array -- nNodeCount IS the byte count

    TrackGraphNode *pRoot = PropagateDistanceMaybe(nFrom, 0, 0, 0);
    for (i = 0; i < nNodeCount; i++) {
        SolveRouteMaybe(pRoot, nFrom, i);
    }
    FreeRouteTreeMaybe(pRoot);
}

// FUNCTION: LOCO 0x45d6c0
// Depth-first relaxation from nNode. Each improvement clones the real node into a fresh
// spanning-tree node, then descends through every edge whose far side has not already been taken
// on this path -- the "already taken" mark is the edge's own Unk0x4, saved into a stack array and
// put back on the way out so the sibling descents see a clean edge set.
TrackGraphNode *TrackGraph::PropagateDistanceMaybe(unsigned int nNode, int nDepth,
                                                   unsigned int nCost, TrackGraphNode *pParent) {
    unsigned int aSavedMark[4];
    int dir;

    if (g_nScreenState == 3 && nNode <= nNodeCount) {
        pabVisitedMaybe[nNode] = 1;
        if (nCost < panBestCostMaybe[nNode]) {
            panBestCostMaybe[nNode] = nCost;

            TrackGraphNode *pTreeNode = new TrackGraphNode;
            *pTreeNode = *papNode[nNode];
            if (papRouteTreeMaybe[nNode] != 0) {
                FreeRouteSubtreeMaybe(papRouteTreeMaybe[nNode]);
            }
            papRouteTreeMaybe[nNode] = pTreeNode;
            papRouteTreeParentMaybe[nNode] = pParent;

            for (dir = 0; dir < 4; dir++) {
                aSavedMark[dir] = 0;
                if (pTreeNode->apEdgeMaybe[dir] != 0) {
                    aSavedMark[dir] = pTreeNode->apEdgeMaybe[dir]->Unk0x4;
                    if (aSavedMark[dir] != 0) {
                        pTreeNode->apNextNodeMaybe[dir] = 0;
                    }
                }
            }
            for (dir = 0; dir < 4; dir++) {
                if (pTreeNode->apNextNodeMaybe[dir] != 0) {
                    pTreeNode->apEdgeMaybe[dir]->Unk0x4 = 1;
                    unsigned int nFar = pTreeNode->apEdgeMaybe[dir]->nNodeIdA;
                    if (nFar == nNode) {
                        nFar = pTreeNode->apEdgeMaybe[dir]->nNodeIdB;
                    }
                    pTreeNode->apNextNodeMaybe[dir] = PropagateDistanceMaybe(
                        nFar, nDepth + 1, nCost + pTreeNode->apEdgeMaybe[dir]->nCostMaybe,
                        pTreeNode);
                }
            }
            for (dir = 0; dir < 4; dir++) {
                if (pTreeNode->apEdgeMaybe[dir] != 0) {
                    pTreeNode->apEdgeMaybe[dir]->Unk0x4 = aSavedMark[dir];
                }
            }
            return pTreeNode;
        }
    }
    return 0;
}

// FUNCTION: LOCO 0x45d850
// FreeRouteTreeMaybe's unlinking twin -- used when a node is being REPLACED by a cheaper copy
// mid-relaxation, so the scratch tables and the surviving parent must forget it first.
void TrackGraph::FreeRouteSubtreeMaybe(TrackGraphNode *pNode) {
    if (pNode != 0) {
        papRouteTreeMaybe[pNode->nNodeIdMaybe] = 0;
        for (unsigned int dir = 0; dir < 4; dir++) {
            if (papRouteTreeParentMaybe[pNode->nNodeIdMaybe]->apNextNodeMaybe[dir] == pNode) {
                papRouteTreeParentMaybe[pNode->nNodeIdMaybe]->apNextNodeMaybe[dir] = 0;
            }
        }
        for (int dirChild = 0; dirChild < 4; dirChild++) {
            if (pNode->apNextNodeMaybe[dirChild] != 0) {
                FreeRouteSubtreeMaybe(pNode->apNextNodeMaybe[dirChild]);
            }
        }
        delete pNode;
    }
}

// FUNCTION: LOCO 0x45d980
// The cheapest total cost from pNode down to node nTo, or -1 if nTo isn't under pNode within the
// nBest budget. Also stamps the winning direction into pNode->bBestDirMaybe on the way out,
// which is what turns the tree into a followable route.
int TrackGraph::FindCheapestRouteMaybe(TrackGraphNode *pNode, unsigned int nFrom, int nTo,
                                       unsigned int nBest, unsigned int nCostSoFar) {
    int nCheapest = -1;
    unsigned char bBestDir = 0xff;

    if (pNode == 0) {
        return -1;
    }
    if ((int)pNode->nNodeIdMaybe == nTo) {
        return 0;
    }
    if (nCostSoFar >= nBest) {
        return -1;
    }

    unsigned char dir = 0;
    do {
        if (pNode->apEdgeMaybe[dir] != 0) {
            int nSub = FindCheapestRouteMaybe(pNode->apNextNodeMaybe[dir], nFrom, nTo, nBest,
                                              nCostSoFar + pNode->apEdgeMaybe[dir]->nCostMaybe);
            if (nSub != -1) {
                int nTotal = nSub + pNode->apEdgeMaybe[dir]->nCostMaybe;
                if (nCheapest == -1 || nTotal < nCheapest) {
                    nCheapest = nTotal;
                    bBestDir = dir;
                }
            }
        }
        dir++;
    } while (dir < 4);

    pNode->bBestDirMaybe = bBestDir;
    return nCheapest;
}

// FUNCTION: LOCO 0x45da40
// Length of the already-solved route from pNode to node nTo, counted into nRouteStepCountMaybe
// so RecordRouteStepsMaybe knows how much to allocate.
void TrackGraph::CountRouteStepsMaybe(TrackGraphNode *pNode, int nTo) {
    nRouteStepCountMaybe++;
    while ((int)pNode->nNodeIdMaybe != nTo) {
        pNode = pNode->apNextNodeMaybe[pNode->bBestDirMaybe];
        nRouteStepCountMaybe++;
    }
}

// FUNCTION: LOCO 0x45da70
// The same walk again, this time writing each step's node id and outgoing direction into the two
// scratch arrays StampRoutePairsMaybe then reads.
void TrackGraph::RecordRouteStepsMaybe(TrackGraphNode *pNode, int nTo) {
    panRouteNodeIdMaybe[nRouteStepCountMaybe] = pNode->nNodeIdMaybe;
    pabRouteDirMaybe[nRouteStepCountMaybe] = (char)pNode->bBestDirMaybe;
    nRouteStepCountMaybe++;
    while ((int)pNode->nNodeIdMaybe != nTo) {
        pNode = pNode->apNextNodeMaybe[pNode->bBestDirMaybe];
        panRouteNodeIdMaybe[nRouteStepCountMaybe] = pNode->nNodeIdMaybe;
        pabRouteDirMaybe[nRouteStepCountMaybe] = (char)pNode->bBestDirMaybe;
        nRouteStepCountMaybe++;
    }
}

// FUNCTION: LOCO 0x45dad0
// Solving one route solves every pair ALONG it: the sub-route from step i to step j is by
// construction the cheapest one too, so both directions of every such pair get stamped here
// rather than being re-searched later. Only pairs still sitting at the 0x80 "unsolved" verdict
// are touched.
void TrackGraph::StampRoutePairsMaybe() {
    for (unsigned int i = 0; i < (unsigned int)nRouteStepCountMaybe - 1; i++) {
        unsigned int nFrom = panRouteNodeIdMaybe[i];
        for (unsigned int j = i + 1; j < (unsigned int)nRouteStepCountMaybe; j++) {
            unsigned int nTo = panRouteNodeIdMaybe[j];
            if (GetStepDirectionMaybe(nFrom, nTo) == 0x80) {
                SetStepDirectionMaybe(nFrom, nTo, (unsigned char)pabRouteDirMaybe[i]);
                SetStepDirectionMaybe(
                    nTo, nFrom, GetOppositePathMaybe((unsigned char)pabRouteDirMaybe[j - 1]));
            }
        }
    }
}

// FUNCTION: LOCO 0x45dbc0
// Resolve the (nFrom, nTo) pair off the spanning tree rooted at pRoot: pick the cheapest of the
// root's four onward routes, then walk the winner once to size the scratch arrays, once to fill
// them, and hand them to StampRoutePairsMaybe. An unreachable destination stamps 0xff.
unsigned char TrackGraph::SolveRouteMaybe(TrackGraphNode *pRoot, unsigned int nFrom,
                                          unsigned int nTo) {
    unsigned int anCost[4];
    unsigned int nBest;

    unsigned char bBestDir = GetStepDirectionMaybe(nFrom, nTo);
    if (bBestDir != 0x80) {
        return 0xff;
    }
    // Shaped as the decompiled original is -- the whole solve is the BODY of this test and the
    // unreachable case falls out the bottom. NOTE (v430 probe): the inverted spelling (two early
    // returns in front of a flat body) compiles to BYTE-IDENTICAL code, so this is a faithfulness
    // choice, not a lever. See the residual note at the end of this file.
    if (pabVisitedMaybe[nTo] != 0 && nFrom != nTo) {
        nBest = 0xffffffff;
        for (int dir = 0; dir < 4; dir++) {
            anCost[dir] = FindCheapestRouteMaybe(pRoot->apNextNodeMaybe[dir], nFrom, nTo, nBest, 0);
            if (anCost[dir] != 0xffffffff &&
                (nBest == 0xffffffff || (int)anCost[dir] < (int)nBest)) {
                nBest = anCost[dir];
            }
        }

        bBestDir = 0;
        for (unsigned char bDir = 1; bDir < 4; bDir++) {
            if (anCost[bDir] != 0xffffffff &&
                (anCost[0] == 0xffffffff || (int)anCost[bDir] < (int)anCost[0])) {
                anCost[0] = anCost[bDir];
                bBestDir = bDir;
            }
        }

        unsigned int nWinner = anCost[bBestDir];
        if (nWinner != 0xffffffff) {
            pRoot->bBestDirMaybe = bBestDir;
            nRouteStepCountMaybe = 1;
            if (pRoot->nNodeIdMaybe != nTo) {
                CountRouteStepsMaybe(pRoot->apNextNodeMaybe[pRoot->bBestDirMaybe], nTo);
            }
            pabRouteDirMaybe = (char *)malloc(nRouteStepCountMaybe);
            panRouteNodeIdMaybe =
                (unsigned int *)malloc(nRouteStepCountMaybe * sizeof(unsigned int));
            nRouteStepCountMaybe = 0;
            panRouteNodeIdMaybe[0] = pRoot->nNodeIdMaybe;
            pabRouteDirMaybe[nRouteStepCountMaybe] = (char)pRoot->bBestDirMaybe;
            nRouteStepCountMaybe++;
            if (pRoot->nNodeIdMaybe != nTo) {
                RecordRouteStepsMaybe(pRoot->apNextNodeMaybe[pRoot->bBestDirMaybe], nTo);
            }
            StampRoutePairsMaybe();
            free(pabRouteDirMaybe);
            free(panRouteNodeIdMaybe);
        }
        if (nWinner == 0xffffffff) {
            SetStepDirectionMaybe(nFrom, nTo, 0xff);
            bBestDir = 0xff;
        }
        return bBestDir;
    }
    SetStepDirectionMaybe(nFrom, nTo, 0xff);
    return 0xff;
}

// FUNCTION: LOCO 0x45dd80
unsigned char TrackGraph::GetStepDirectionMaybe(unsigned int nFrom, unsigned int nTo) {
    if (nFrom != nTo && nFrom < nNodeCount && nTo < nNodeCount) {
        unsigned int nHi;
        unsigned int nLo;
        char bSwapped;
        if (nFrom < nTo) {
            nHi = nTo;
            nLo = nFrom;
            bSwapped = 1;
        } else {
            nHi = nFrom;
            nLo = nTo;
            bSwapped = 0;
        }
        unsigned char bPair = paDirMatrix[(nHi - 1) * nHi / 2 + nLo];
        if (bPair != 0xff) {
            if (bPair == 0x80) {
                return 0x80;
            }
            if (bSwapped) {
                bPair >>= 2;
            }
            bPair &= 3;
            return bPair;
        }
    }
    return 0xff;
}

// FUNCTION: LOCO 0x45dde0
void TrackGraph::SetStepDirectionMaybe(unsigned int nFrom, unsigned int nTo, unsigned char bDir) {
    if (nFrom != nTo) {
        unsigned int nHi;
        unsigned int nLo;
        char bSwapped;
        if (nFrom < nTo) {
            nHi = nTo;
            nLo = nFrom;
            bSwapped = 1;
        } else {
            nHi = nFrom;
            nLo = nTo;
            bSwapped = 0;
        }
        unsigned int nPair = (nHi - 1) * nHi / 2 + nLo;
        unsigned char bPair = paDirMatrix[nPair];
        if (bPair == 0x80) {
            bPair = 0;
        }
        if (bDir == 0xff) {
            paDirMatrix[nPair] = 0xff;
            return;
        }
        if (bSwapped) {
            bDir <<= 2;
        }
        paDirMatrix[nPair] = bPair | bDir;
    }
}

// ---------------------------------------------------------------------------------------------
// Residual notes for the four functions in this TU that are not byte-exact.
//
// 0x45ce40 / 0x45d1c0 BuildAdjacencyA/BMaybe -- see the autopsy above them. One shared cause,
//   one cross-TU fix, deliberately not attempted here.
//
// 0x45d6c0 PropagateDistanceMaybe -- EFFECTIVE-adjacent, insns 120/118. Two stacked known
//   classes and nothing source-shaped left: (a) the `xor eax,eax / cmp / sete al / test al,al`
//   prologue on the g_nScreenState == 3 guard, the same sete-prologue class already autopsied on
//   0x456150 and 0x462e90; (b) the original keeps nNode as ONE index used both byte-scaled
//   (`mov byte ptr [ecx+esi],1`) and dword-scaled (`lea eax,[edx+esi*4]`), where VC5 here
//   strength-reduces it to a precomputed `nNode * 4` held in a stack slot. Neither is reachable
//   from the source text.
//
// 0x45dbc0 SolveRouteMaybe -- EFFECTIVE. reg_pen 2, identity_miss 2: every instruction is
//   present in the same register. The entire difference is BLOCK LAYOUT -- the original leaves
//   both early-exit epilogues inline at the top of the function, VC5 sinks them to the bottom and
//   cross-jumps them together. PROBED and refuted (v430): writing the body as the `if` branch
//   with the unreachable case falling out the bottom, exactly as the original reads, compiles to
//   byte-identical code. Same VC5 cold-path-sinking class as 0x44a250 / 0x44ce10.
//
// 0x45dd80 / 0x45dde0 Get/SetStepDirectionMaybe -- EFFECTIVE. insns 43/43 and 38/40, identical
//   structure; GetStepDirectionMaybe now compiles to the original's exact 90-byte length. What is
//   left is register-assignment coin-flips (the original threads the pair through edi/esi where
//   VC5 picks esi/edx/ebx, and pops in the other order) plus one instruction-selection tie-break
//   in Set: on the bDir == 0xff path the original tail-merges the store into the shared
//   `or al,cl; mov [esi],al` (having proved bPair | 0xff == 0xff) where VC5 emits the shorter
//   direct `mov [ecx],dl`. Cheap probes already spent and refuted: the swap flag as a one-armed
//   `if` vs a real if/else (if/else is right, and took Get from DIFF(87) to DIFF(35)); the pair
//   byte reached through a cached `unsigned char *` vs a re-subscripted array (array is right);
//   an explicit `(unsigned char)` cast on the `& 3` return vs a compound `&=` (compound is
//   right, and closed the last length gap). Do not re-run those three.
