// TrackGraph -- the node/edge graph the world board builds over the placed tiles so the ambient
// actors and the trains can path across them. WorldBoardMaybe owns TWO of them
// (pTrackGraphAMaybe / pTrackGraphBMaybe at +0x52488/+0x5248c, src/WorldBoardMaybe.h); the
// actors only ever consult graph A.
//
// ONE canonical shared partial view (CLAUDE.md's "never duplicate a struct across TUs" rule).
// Only the fields the current consumers touch are modeled -- the real Ghidra struct is 44
// bytes with several more pointers past +0x8. Field identification comes from three
// independent readers: TrackGraph::GetStepDirectionMaybe (0x45dd80, the adjacency lookup),
// TrackGraph::BuildAdjacencyBMaybe (0x45d1c0, which mallocs papNode as nNodeCount dwords), and
// TrackGraph::FUN_0045d980 (the recursive cheapest-route walk, which is what pins the node's
// own +0x8/+0x18 quads apart).
#pragma once

class TilePlacedObj; // src/TilePlacedObj.h -- only ever used here as an opaque pointer type

// One graph edge. Shared by the two nodes it joins, so a node reached through
// apEdgeMaybe[dir] has to compare BOTH endpoint ids against its own to learn which end it is
// -- exactly what WalkerActor::ChooseNextStepTileMaybe does.
struct TrackGraphEdge {
    unsigned int nCostMaybe; // +0x0 -- traversal cost, summed by FUN_0045d980's route walk
    unsigned int Unk0x4;     // +0x4 -- zeroed by hand right after each `new`, NOT by a ctor (see
                             //   below); PropagateDistanceMaybe borrows it as a per-edge
                             //   "already taken on this descent" mark, saving and restoring the
                             //   old value around the recursion
    unsigned int nNodeIdA;   // +0x8 -- endpoint node indices into TrackGraph::papNode
    unsigned int nNodeIdB;   // +0xc
};
// NO constructor: both `new TrackGraphEdge` sites (0x45d055 and its graph-B twin) run the
// `Unk0x4 = 0` store UNCONDITIONALLY, straight off the register operator new returned. A ctor
// -- even a one-store inline one -- would have made VC5 wrap it in the mandatory
// `test eax,eax / je` null guard, which neither site has. Carrying one here cost DIFF(457)
// worth of layout in BuildAdjacencyAMaybe until v431 replaced it with the plain statement.

// 0x2c bytes -- pinned by BuildAdjacencyAMaybe's own `new TrackGraphNode` and by
// PropagateDistanceMaybe's whole-struct copy, both of which move exactly 11 dwords.
struct TrackGraphNode {
    unsigned int nNodeIdMaybe;               // +0x0 -- this node's own index in papNode
    TilePlacedObj *pTile;                    // +0x4 -- the board tile this node stands for;
                                             //   consumers read its hotspotPosX/Y for the
                                             //   node's world position
    TrackGraphEdge *apEdgeMaybe[4];          // +0x8 .. +0x14 -- per-direction edge, 0 if none
    TrackGraphNode *apNextNodeMaybe[4];      // +0x18 .. +0x24 -- the node across that edge
    unsigned char bBestDirMaybe;             // +0x28 -- FUN_0045d980 writes the direction of
                                             //   the cheapest onward route here (0xff = none)
};

// 0x2c bytes. The trailing five members (+0x10 .. +0x28) are pure SCRATCH: they only ever hold
// allocations for the duration of BuildAllRoutesMaybe / SolveRouteMaybe and are freed again
// before those return, which is why the actors' read-only view of the class never saw them.
class TrackGraph {
public:
    unsigned int nNodeCount;      // +0x0
    // +0x4 -- a packed LOWER-TRIANGULAR direction matrix, indexed
    // `paDirMatrix[hi*(hi-1)/2 + lo]` for hi = max(from,to), lo = min(from,to). Each byte
    // holds the two directions of one node pair: bits 0-1 for the hi->lo traversal and bits
    // 2-3 for lo->hi, with 0xff meaning "unreachable" and 0x80 a distinct reserved verdict.
    unsigned char *paDirMatrix;
    TrackGraphNode **papNode;     // +0x8 -- nNodeCount entries, malloc'd by BuildAdjacencyBMaybe
    // +0xc -- WHICH graph this is, handed to the ctor and forwarded to the board's node
    // predicate. Pinned by WorldBoardMaybe's own construction site (0x454d34 / 0x454d69, both
    // `new TrackGraph(N)` off a `push 0x2c` that also pins this class at 44 bytes): graph A gets
    // **7** and graph B gets **8**. So it is NOT the TileKind category (0xc/0xd) the two
    // networks are made of -- it is a selector of its own. BuildAdjacencyBMaybe carries a
    // `nGraphKindMaybe == 7` branch that stamps the tile's graph-A node-id slot instead of its
    // graph-B one; since only graph B ever runs that function and graph B is always 8, the
    // branch is DEAD -- a leftover of the copy-and-edit that made B out of A.
    short nGraphKindMaybe;
    char *pabVisitedMaybe;        // +0x10 -- scratch, nNodeCount bytes
    int nRouteStepCountMaybe;     // +0x14 -- scratch, the length of the route being recorded
    char *pabRouteDirMaybe;       // +0x18 -- scratch, nRouteStepCountMaybe bytes
    unsigned int *panRouteNodeIdMaybe; // +0x1c -- scratch, nRouteStepCountMaybe dwords
    unsigned int *panBestCostMaybe;    // +0x20 -- scratch, nNodeCount dwords
    TrackGraphNode **papRouteTreeMaybe;       // +0x24 -- scratch, nNodeCount entries
    TrackGraphNode **papRouteTreeParentMaybe; // +0x28 -- scratch, nNodeCount entries

    TrackGraph(short nCategory);  // 0x45cdf0
    ~TrackGraph();                // 0x45ce10
    // 0x45ce40 / 0x45d1c0 -- rebuild the whole node/edge graph from the board's currently placed
    // tiles. Twins: A walks the tile's graph-A neighbour slot, B its graph-B one.
    void BuildAdjacencyAMaybe();
    void BuildAdjacencyBMaybe();
    void BuildAllRoutesMaybe();                        // 0x45d560
    void BuildRoutesFromNodeMaybe(unsigned int nFrom); // 0x45d5f0
    // 0x45d6c0 -- depth-first cheapest-cost relaxation from nNode, building the spanning tree of
    // TrackGraphNode COPIES that SolveRouteMaybe then walks. Returns the subtree it allocated.
    TrackGraphNode *PropagateDistanceMaybe(unsigned int nNode, int nDepth, unsigned int nCost,
                                           TrackGraphNode *pParent);
    void FreeRouteTreeMaybe(TrackGraphNode *pNode);    // 0x45d810
    void FreeRouteSubtreeMaybe(TrackGraphNode *pNode); // 0x45d850
    void ClearMaybe();                                 // 0x45d8c0
    // 0x45d980 -- recursive argmin over the spanning tree: the cheapest total cost from pNode to
    // node nTo without exceeding nBest, or -1. Stamps the winning direction into bBestDirMaybe.
    int FindCheapestRouteMaybe(TrackGraphNode *pNode, unsigned int nFrom, int nTo,
                               unsigned int nBest, unsigned int nCostSoFar);
    void CountRouteStepsMaybe(TrackGraphNode *pNode, int nTo);  // 0x45da40
    void RecordRouteStepsMaybe(TrackGraphNode *pNode, int nTo); // 0x45da70
    void StampRoutePairsMaybe();                                // 0x45dad0
    // 0x45dbc0 -- resolve the (nFrom, nTo) pair and every pair along the winning route.
    unsigned char SolveRouteMaybe(TrackGraphNode *pRoot, unsigned int nFrom, unsigned int nTo);
    // 0x45dd80, extern -- unpacks paDirMatrix for the (nFrom, nTo) pair: the direction 0-3 of
    // the first step to take at nFrom when heading for nTo, or 0xff / 0x80. Returns 0xff for
    // nFrom == nTo and for any out-of-range id.
    unsigned char GetStepDirectionMaybe(unsigned int nFrom, unsigned int nTo);
    // 0x45dde0 -- the write side of the same packed matrix. A 0xff direction stamps the whole
    // byte unreachable; otherwise the direction is OR'd into the pair's own nibble.
    void SetStepDirectionMaybe(unsigned int nFrom, unsigned int nTo, unsigned char bDir);
};
