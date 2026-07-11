// DecorActorBase -- the shared base of the ambient world actors (see DecorActor.h for the
// class/category writeup). This TU is the .text run starting at 0x433a20, which holds the base
// class and then DecorObjMgrMaybe's own methods; only the base's own members live here.
//
// The whole base class is transcribed; what is left in this TU is DecorObjMgrMaybe's own tail.
#include <math.h>   // sqrt
#include <stdlib.h>
#include <string.h> // _stricmp
#include <time.h>  // localtime

#include "AppWindow.h"
#include "BigObjSeqRecordMaybe.h"
#include "CursorDesc.h"
#include "DecorActor.h"
#include "DecorObjMgrMaybe.h"
#include "EffectSpawner.h"
#include "GameWindowWidgetList.h"
#include "LocoBitmap.h"
#include "TimeOfDayMaybe.h"
#include "Obj0x478118.h"  // the minifig/person kind descriptor tier -- its `sex` field
#include "GeomUtil.h"        // CalcSqDist
#include "PlacementCursorMaybe.h"
#include "RandRange.h"       // RAND_RANGE_MAYBE
#include "TilePlacedObj.h"
#include "TrackGraph.h"
#include "UIResources.h"
#include "WorldActionCursor.h"
#include "WorldBoardMaybe.h"

extern unsigned int g_dwGameTick; // DAT_004a99b4
extern int g_nScreenState; // the app screen-state selector, see src/GameNetMsgQueue.h
// TileKind_GetCategory, 0x446030 (same decl as src/WorldBoardMaybe.cpp / src/Obj0x4779e0.cpp /
// src/NetSessionEventQueue.cpp -- see src/UIResources.cpp for the body and for why the family
// still has no shared home).
extern unsigned int __cdecl TileKind_GetCategory(unsigned int kindId);

// TU-local byte-returning predicate, per CLAUDE.md/docs/CODEGEN.md's sete-materialization
// lesson -- the original's `xor eax,eax; cmp [0x4851f4],3; sete al; test al,al; je` at 0x434e4f
// is what an inlined `unsigned char` predicate compiles to, not a plain `if (g == 3)`.
inline unsigned char IsInGameModeMaybe() { return g_nScreenState == 3; }

// "(-1, -1) means no target" -- the sentinel test the movement primitive gates its stop path on.
// Written as an inline predicate rather than a bare `&&` in the `if` because the original
// MATERIALIZES the bool (`mov eax,1`/`jmp`/`xor eax,eax`, then `test al,al`/`je`) before
// branching on it, which a plain short-circuit condition folds away into two direct `jne`s.
inline bool IsNoTargetMaybe(int x, int y) { return x == -1 && y == -1; }

// The same TU-local position predicate src/WalkerActor.cpp and src/RoadVehicleActor.cpp already
// carry (see WalkerActor.cpp's note on why the family is kept per-TU rather than hoisted into a
// shared header: hoisting a predicate next to a shared header's extern rotates other TUs).
static inline bool ArePositionsEqualMaybe(int ax, int ay, int bx, int by) {
    return ax == bx && ay == by;
}

// "Which category registry does this actor belong to" -- the already-resolved-descriptor
// counterpart of TileKind::TileKind_GetCategory(kindId) (0x446030), which the SPAWN side has to
// use because it only has the kind id. Byte-returning, like that one: the caller widens the
// result with the `and eax,0xff` MSVC emits for a char-returning call assigned to an int, never
// a `movzx` off the field itself. TU-local rather than a DecorActorBase member for the same
// reason IsInGameModeMaybe above is -- DeregisterEntryMaybe is its only consumer so far, and
// adding method decls to the widely-included DecorActor.h has rotated /Og state before.
//
// The `(unsigned char)` on the ZERO arm is LOAD-BEARING: with a plain `0` the ternary promotes
// to int and VC5 replaces the whole thing with `je` past the load, reusing the already-zero
// pointer register as the result (same 9 bytes, different instructions). Casting the literal
// keeps the merge 8-bit and gives the original's `jne`/`xor al,al`/`jmp` shape.
inline unsigned char GetActorCategoryMaybe(AnimDescRefObj0x477488 *pObj) {
    return pObj->pKindDesc == 0 ? (unsigned char)0 : pObj->pKindDesc->categoryByte;
}

// Its null-tolerant wrapper, for the world-board probes below: a missing TILE reads as category
// 0 exactly like a missing descriptor does. Widened to unsigned short because every caller
// compares the result against a `unsigned short nCategory` parameter.
inline unsigned short GetTileCategoryMaybe(TilePlacedObj *pTile) {
    if (pTile != 0) {
        return GetActorCategoryMaybe(pTile);
    }
    return 0;
}

// src/WalkerActor.cpp's own macro: an arithmetic shift down by 4 (16 px per tile) with
// everything left of the origin collapsing onto the -1 sentinel row/column rather than wrapping.

// FUNCTION: LOCO 0x433a20
// Every field this class adds starts at its "nothing yet" value: no spawner, no workplace, no
// destination, no step, no trail anchor (-1/-1 everywhere a position lives), a mood of 4 and the
// game tick as a birth stamp.
//
// The tail is this actor's NAME. A kind descriptor that carries a per-instance category name of
// its own (BigObj::szCategoryName, +0x14d -- e.g. the "PARTY" guests) hands that name straight
// down, and two side effects hang off it: a category-7 arrival re-sorts the walker registry, and
// a "PARTY" arrival raises the manager's population throttle. A descriptor with NO name of its
// own instead gets a RANDOM one out of the string table -- 49 male names (ids 2..0x32) when the
// kind's `sex` field says 'M', else 11 female ones (ids 0x33..0x3d).
//
// Those two LoadStringA calls are written as TWO WHOLE CALLS in an if/else, not as one call with
// a ternary in the id argument -- that spelling is load-bearing, DIFF(66) vs EXACT. VC5
// cross-jumps the two identical call tails back together and hoists only the common `push 0xa`
// above the branch, which is why the original pushes the buffer TWICE (once per arm) but calls
// LoadStringA once. The ternary form instead evaluates the whole id expression before starting
// the push sequence, so the buffer push appears once, after the branch.
DecorActorBase::DecorActorBase(int kindId) : AnimDescRefObj0x477488(kindId, -1, 0, 0) {
    bSuspendedMaybe = false;
    nMoodMaybe = 4;
    nSubTickCounterMaybe = 0;
    dwSpawnTickMaybe = g_dwGameTick;
    BigObj *pKind = pKindDesc;
    nSpawnDescriptorIdMaybe = kindId;
    dwSeqRewardUntilMaybe = 0;
    pSpawnerObjMaybe = 0;
    pOwnerObjMaybe = 0;
    nStepDistanceMaybe = 0;
    nTrailAnchorPosXMaybe = -1;
    nTrailAnchorPosYMaybe = -1;
    nStepDeltaXMaybe = -1;
    nStepDeltaYMaybe = -1;
    ptStepMaybe.x = -1;
    ptStepMaybe.y = -1;
    ptDestMaybe.x = -1;
    ptDestMaybe.y = -1;
    nPrevDestPosXMaybe = -1;
    nPrevDestPosYMaybe = -1;
    nNetEntryPosXMaybe = -1;
    nNetEntryPosYMaybe = -1;
    nTrailAnchorCounterMaybe = 0;
    dwNextDecisionTickMaybe = 0;
    dwLastRetargetTickMaybe = 0;
    nScheduleStateMaybe = 0;
    if (pKind != 0) {
        if (pKind->szCategoryName[0] != '\0') {
            SetCategoryIfPrintable(pKind->szCategoryName);
            if (GetActorCategoryMaybe(this) == 7) {
                DecorObjMgrMaybe_00485448.TickCategory7OnlyMaybe();
            }
            if (_stricmp(pKind->szCategoryName, "PARTY") == 0) {
                DecorObjMgrMaybe_00485448.bThrottleMaybe = true;
                DecorObjMgrMaybe_00485448.dwLastTickMaybe = g_dwGameTick;
            }
        } else {
            if (((Obj0x478118 *)(CursorDesc *)pKind)->dwSex == 'M') {
                LoadStringA(g_pApp->hInstance, rand() % 0x31 + 2, szCategoryName, 10);
            } else {
                LoadStringA(g_pApp->hInstance, rand() % 0xb + 0x33, szCategoryName, 10);
            }
        }
    }
}

// FUNCTION: LOCO 0x433bc0 (??_GDecorActorBase scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x433be0
// EFFECTIVE MATCH -- structurally identical, DIFF(68), insns 26/25. The residual is one extra
// callee-saved register: our candidate hoists `pOwner->apSpawnedActorMaybe` into ebx (costing a
// `push ebx`/`pop ebx` pair) and peels the loop's first compare, while the original keeps the
// walk entirely in edx/ecx and rotates the loop the other way. It also emits `dec byte ptr
// [eax+0x8e]` where the original does the load/dec/store triple `mov dl,[eax+0x8e]; dec dl;
// mov [eax+0x8e],dl`. Probed (v390, both no-ops on the score): the explicit
// `x = x - 1` spelling of the decrement, and re-reading `pOwnerObjMaybe` instead of the cached
// `pOwner` local when seeding the walk pointer. Register-allocation tie-break class; PARKED.
//
// Unhooks this actor from the building that spawned it before the base chain runs: scan that
// building's 5 occupant slots for `this`, clear the one that matches and decrement its count.
//
// sic: the "not found" path (all 5 slots scanned without a hit) still nulls pOwnerObjMaybe but
// leaves the building's bSpawnedActorCountMaybe untouched, so a desynced slot array permanently
// leaks a unit of occupancy. Reproduced, not fixed -- see docs/engine-bugs.md.
DecorActorBase::~DecorActorBase() {
    TilePlacedObj *pOwner = pOwnerObjMaybe;
    if (pOwner != 0) {
        unsigned int nSlot = 0;
        DecorActorBase **ppSlot = pOwner->apSpawnedActorMaybe;
        while (*ppSlot != this) {
            nSlot++;
            ppSlot++;
            if (4 < nSlot) {
                pOwnerObjMaybe = 0;
                return;
            }
        }
        pOwner->apSpawnedActorMaybe[nSlot] = 0;
        pOwnerObjMaybe->bSpawnedActorCountMaybe--;
        pOwnerObjMaybe = 0;
    }
}

// FUNCTION: LOCO 0x433c50
// vtable slot 15's base body, called first by both leaves' own TickMaybe. Advances the sprite,
// then -- only once the actor has been idle for 0xb4 (180) ticks since its last retarget --
// decays its mood by one, clears the decision deadline, makes it visible again and re-runs
// slot 17 with no target.
void DecorActorBase::TickMaybe(DecorActorBase * /*pNextActor*/) {
    AnimDescRefObj0x477488::AdvanceAnimFrameMaybe();
    if (dwLastRetargetTickMaybe != 0 && (int)(dwLastRetargetTickMaybe + 0xb4) < (int)g_dwGameTick) {
        if (nMoodMaybe > 0) {
            nMoodMaybe--;
        }
        dwNextDecisionTickMaybe = 0;
        bReady = true;
        HeadForObjectMaybe(0);
    }
}

// FUNCTION: LOCO 0x433ca0
// vtable slot 20 -- "go find yourself a job". Resigns from the workplace this actor currently
// holds (pOwnerObjMaybe), then scans the whole game-window widget list for the FIRST placed
// object that is hiring: one whose own occupancy (bSpawnedActorCountMaybe) is still below its
// kind descriptor's bMaxEmployees AND whose descriptor's 5-entry aPossibleEmployees
// list names this actor's own kind id. Hired == claim a free apSpawnedActorMaybe slot, bump the
// count, and store the building in pOwnerObjMaybe -- which is also the loop's own exit test, so
// the scan stops at the first success. See DecorActor.h's pSpawnerObjMaybe/pOwnerObjMaybe
// writeup: this is the ONLY writer of +0x90 in the binary.
void DecorActorBase::ActivateMaybe() {
    unsigned int i;
    TilePlacedObj *pOwner = pOwnerObjMaybe;
    if (pOwner != 0) {
        for (i = 0; i < 5; i++) {
            if (pOwner->apSpawnedActorMaybe[i] == this) {
                pOwner->apSpawnedActorMaybe[i] = 0;
                pOwnerObjMaybe->bSpawnedActorCountMaybe--;
                break;
            }
        }
        pOwnerObjMaybe = 0;
    }
    unsigned int nItem;
    for (nItem = 0; nItem < g_gameWindowWidgetList.nItemCount; nItem++) {
        if (pOwnerObjMaybe != 0) {
            return;
        }
        TilePlacedObj *pObj = (TilePlacedObj *)((GameWindowWidgetListProbe *)&g_gameWindowWidgetList)
                                  ->GetItemImpl(nItem);
        if (pObj != 0 && pObj->bSpawnedActorCountMaybe < pObj->pKindDesc->bMaxEmployees) {
            unsigned char bWantsMyKindMaybe = 0;
            for (i = 0; i < 5; i++) {
                if (pObj->pKindDesc->aPossibleEmployees[i] ==
                    (pKindDesc == 0 ? -1 : pKindDesc->resourceId)) {
                    bWantsMyKindMaybe = 1;
                    break;
                }
            }
            if (bWantsMyKindMaybe != 0) {
                for (i = 0; i < 5; i++) {
                    if (pObj->apSpawnedActorMaybe[i] == 0) {
                        pObj->apSpawnedActorMaybe[i] = this;
                        pObj->bSpawnedActorCountMaybe++;
                        pOwnerObjMaybe = pObj;
                        break;
                    }
                }
            }
        }
    }
}

// FUNCTION: LOCO 0x433dc0
// EFFECTIVE MATCH -- DIFF(42), insns 96/96, len 256/256, align=30 reg_pen=9 identity_miss=34.
// EVERY instruction sits at the SAME byte offset as the original's and every operand agrees
// except for one esi/edi coin flip: the original parks `this` in edi and nTargetX in esi, ours
// does the reverse (nTargetY is ebp in both). The two `S` rows in the stop path fall straight
// out of that -- with `this` in the register that dies at the end of the block, our candidate
// reloads hotspotPosY into esi (killing `this`) one instruction earlier than the original,
// which still needs edi and so borrows edx. Nothing in the source can name a callee-saved
// register. Probed and score-neutral: `unsigned char` instead of `bool` on IsNoTargetMaybe.
// Probed and much worse (DIFF 189): hoisting `pt.x = hotspotPosX` above the three zero stores.
// Register coin-flip class; PARKED, do not re-grind.
//
// The movement primitive, shared by both leaves' steppers. Recomputes nStepDelta{X,Y} as
// (target - hotspot) and nStepDistanceMaybe as their euclidean length, then RETURNS the
// position to move to: the sprite's own rect origin advanced toward the target by at most
// nMaxStep on each axis independently. A (-1,-1) target means "stop" -- it zeroes all three
// fields and hands back the CURRENT hotspot position unchanged (note: the hotspot, not the
// rect origin the moving path returns, so the two exits are not in the same coordinate space).
POINT DecorActorBase::ComputeStepTargetMaybe(int nTargetX, int nTargetY, int nMaxStep) {
    POINT pt;
    if (IsNoTargetMaybe(nTargetX, nTargetY)) {
        nStepDeltaXMaybe = 0;
        nStepDeltaYMaybe = 0;
        nStepDistanceMaybe = 0;
        pt.x = hotspotPosX;
        pt.y = hotspotPosY;
        return pt;
    }
    int nDeltaX = nTargetX - hotspotPosX;
    int nDeltaY = nTargetY - hotspotPosY;
    nStepDeltaXMaybe = nDeltaX;
    nStepDeltaYMaybe = nDeltaY;
    nStepDistanceMaybe = (int)sqrt((double)(nDeltaX * nDeltaX + nDeltaY * nDeltaY));
    if (nDeltaX < 0) {
        pt.x = rect.left - (nMaxStep < abs(nDeltaX) ? nMaxStep : abs(nDeltaX));
    } else {
        pt.x = rect.left + (nMaxStep < abs(nDeltaX) ? nMaxStep : abs(nDeltaX));
    }
    if (nDeltaY < 0) {
        pt.y = rect.top - (nMaxStep < abs(nDeltaY) ? nMaxStep : abs(nDeltaY));
    } else {
        pt.y = rect.top + (nMaxStep < abs(nDeltaY) ? nMaxStep : abs(nDeltaY));
    }
    return pt;
}

// FUNCTION: LOCO 0x433ec0
// EFFECTIVE MATCH -- DIFF(4), insns 144/144, len 353/353, align=0, byte_diff=4. The ENTIRE
// residual is two adjacent prologue loads emitted in the opposite order: the original hoists
// `mov ebx,[esp+0x18]` (x) then `mov esi,[esp+0x1c]` (y), ours the same two loads into the same
// two registers the other way round. Nothing downstream differs. Scheduling coin-flip; PARKED.
//
// Two source shapes ARE load-bearing, both in the GetTileCategoryMaybe wrapper above, and both
// were found by bisecting this residual down from 220536:
//   * the wrapper written with two RETURNS rather than one ternary -- the ternary promotes the
//     inner `unsigned char` to int and emits `and ecx,0xff` where the original widens straight
//     to 16 bits with `movzx cx,cl` (220536 -> 200521);
//   * the null test written POSITIVELY (`if (pTile != 0) return …; return 0;`) so the zero arm
//     lands out of line at the bottom. With `if (pTile == 0) return 0;` the zero block is inline
//     and the two descriptor legs share one merged widening instead of each carrying its own
//     `movzx`/`jmp` pair, which is a 12-instruction structural gap (200521 -> 224).
//
// "Is there a tile of kind CATEGORY nCategory under my sprite" -- up to three sample points down
// the footprint, first hit wins. The leading extra probe (the sprite's own hotspot row) only runs
// for a category-8 actor, i.e. a RoadVehicleActor, which is what lets a vehicle detect the road
// it is sitting ON rather than only the one under its front bumper; the two shared probes are the
// half-height and full-height rows of the descriptor's native sprite box.
TilePlacedObj *DecorActorBase::FindFootprintTileOfCategoryMaybe(unsigned short nCategory, int x,
                                                                int y) {
    TilePlacedObj *pTile;
    if (GetActorCategoryMaybe(this) == 8) {
        pTile = g_worldBoard.GetPlaneASlotMaybe(WORLD_TO_TILE(x + 4), WORLD_TO_TILE(y), 0);
        if (GetTileCategoryMaybe(pTile) == nCategory) {
            return pTile;
        }
    }
    pTile = g_worldBoard.GetPlaneASlotMaybe(WORLD_TO_TILE(x + 4),
                                            WORLD_TO_TILE(y + (pKindDesc->nativeHeight >> 1)), 0);
    if (GetTileCategoryMaybe(pTile) == nCategory) {
        return pTile;
    }
    pTile = g_worldBoard.GetPlaneASlotMaybe(WORLD_TO_TILE(x + pKindDesc->nativeWidth - 4),
                                            WORLD_TO_TILE(y + pKindDesc->nativeHeight), 0);
    return GetTileCategoryMaybe(pTile) == nCategory ? pTile : 0;
}

// FUNCTION: LOCO 0x434040
// The schedule chooser, called once per idle tick by both leaves' TickMaybe. Runs the wall clock
// against the two attached buildings' opening-hours windows and returns the verdict the arrival
// handler later replays out of nScheduleStateMaybe:
//   0  not yet -- either the decision deadline has not passed, or the player is dragging us
//   2  the WORKPLACE (pOwnerObjMaybe) is open right now      -> go to work
//   1  the SPAWNER (pSpawnerObjMaybe) is CLOSED right now    -> go home
//   3  neither applies                                       -> wander
// Note the asymmetry: state 2 wants its building OPEN, state 1 wants its building SHUT.
//
// The "not yet" exits also make this the place an actor un-hides itself: any tick that gets past
// the deadline test forces bReady back on.
int DecorActorBase::ChooseScheduleStateMaybe(unsigned int dwTick) {
    if ((int)dwNextDecisionTickMaybe > (int)dwTick) {
        return 0;
    }
    if (PlacementCursorMaybe_004854c8.pHoverObjMaybe == this &&
        PlacementCursorMaybe_004854c8.bHoverActiveMaybe) {
        return 0;
    }
    if (bReady == false) {
        bReady = true;
    }
    tm *pNow = localtime((time_t *)&dwTick);
    // The two "shifts" records are raw longs in src/CursorDesc.h (that header deliberately does
    // not pull in TimeOfDayMaybe.h -- see its own note), hence the casts.
    if (pOwnerObjMaybe != 0) {
        BigObj *pDesc = pOwnerObjMaybe->pKindDesc;
        if (TimeOfDay_IsTimeInWindowMaybe(pNow, &pDesc->shiftOpen,
                                            &pDesc->shiftClose)) {
            return 2;
        }
    }
    if (pSpawnerObjMaybe != 0) {
        BigObj *pDesc = pSpawnerObjMaybe->pKindDesc;
        if (TimeOfDay_IsTimeInWindowMaybe(pNow, &pDesc->shiftOpen,
                                            &pDesc->shiftClose) == 0) {
            return 1;
        }
    }
    return 3;
}

// FUNCTION: LOCO 0x434100
// EFFECTIVE MATCH -- DIFF(265), insns 103/104, reg_pen=24 identity_miss=22. ONE root cause: our
// compile allocates a dedicated ZERO REGISTER (`xor ecx,ecx` in the prologue) and the original
// does not, which then accounts for every remaining row. Each of the original's nine separate
// zero uses is an immediate or a fresh 8-bit zero (`mov al,[mem]`/`test al,al` for the three
// byte-flag tests, `mov dword ptr [esi+0xa4],0`, `mov byte ptr [esi+0x24],0`, `xor al,al` for
// the early return); ours spends `cl` at all of them. It also costs us ecx as a scratch, which
// is why BOTH ArePositionsEqualMaybe sites sink their y-coordinate loads below the x compare
// (the original has four free registers and hoists all four loads above it) and why the tail's
// two nPrevDest stores come out in the other order. Documented zero-register residency class
// (docs/CODEGEN.md); no source shape can name a register.
//
// Two source shapes WERE load-bearing and are kept:
//   * `case 0: break;` -- with only cases 1/2/3, VC5 drops the jump table and emits a
//     `dec eax`/`je` comparison chain instead (score 252664 -> 214739 when case 0 was added).
//   * the re-decide line DUPLICATED into each case rather than sitting after the switch behind a
//     `goto`. The single-copy `goto doneMaybe` form is much worse (390122) because it takes the
//     jump table away again; VC5 cross-jumps the three copies back into the one block at
//     0x4341fe by itself.
// Probed and inert: `!bReady`/`!bThrottleMaybe` instead of `== false`; `unsigned char` instead of
// `bool` on ArePositionsEqualMaybe. PARKED, do not re-grind.
//
// The arrival handler: replays the verdict ChooseScheduleStateMaybe last returned (parked in
// nScheduleStateMaybe) now that the actor has reached its destination, then drops the
// destination -- remembering it in nPrevDest{X,Y} so the next re-roll can avoid repeating it.
// Returns 0 when it declined to run (hidden, or being dragged by the player), 1 otherwise.
//
// Arriving at either scheduled building raises the mood and hides the actor (bReady = false, i.e.
// "gone indoors"); only the WORKPLACE case additionally pins the next decision a full 0xe10
// (3600) ticks out, which is the shift length. Every other outcome re-decides in 10..30 ticks --
// except under the population throttle, where a wanderer deliberately gets no new deadline at all
// so it stays parked until something else wakes it.
unsigned char DecorActorBase::OnArriveAtDestinationMaybe(int nScheduleState) {
    if (bReady == false || (PlacementCursorMaybe_004854c8.pHoverObjMaybe == this &&
                            PlacementCursorMaybe_004854c8.bHoverActiveMaybe)) {
        return 0;
    }
    dwLastRetargetTickMaybe = 0;
    switch (nScheduleState) {
    case 0:
        break;
    case 1:
        if (ArePositionsEqualMaybe(hotspotPosX, hotspotPosY, pSpawnerObjMaybe->hotspotPosX,
                                   pSpawnerObjMaybe->hotspotPosY)) {
            if (nMoodMaybe <= 6) {
                nMoodMaybe++;
            }
            bReady = false;
        }
        dwNextDecisionTickMaybe = rand() % 0x15 + 10 + g_dwGameTick;
        break;
    case 2:
        if (pOwnerObjMaybe != 0) {
            if (ArePositionsEqualMaybe(hotspotPosX, hotspotPosY, pOwnerObjMaybe->hotspotPosX,
                                       pOwnerObjMaybe->hotspotPosY)) {
                if (nMoodMaybe <= 6) {
                    nMoodMaybe++;
                }
                bReady = false;
                dwNextDecisionTickMaybe = g_dwGameTick + 0xe10;
            } else {
                dwNextDecisionTickMaybe = rand() % 0x15 + 10 + g_dwGameTick;
            }
        } else {
            // Lost our workplace while walking to it -- go and apply somewhere else instead.
            ActivateMaybe();
        }
        break;
    case 3:
        if (nMoodMaybe <= 6) {
            nMoodMaybe += 2;
        }
        if (DecorObjMgrMaybe_00485448.bThrottleMaybe == false) {
            dwNextDecisionTickMaybe = rand() % 0x15 + 10 + g_dwGameTick;
        }
        break;
    }
    nPrevDestPosXMaybe = ptDestMaybe.x;
    nPrevDestPosYMaybe = ptDestMaybe.y;
    ptDestMaybe.x = -1;
    ptDestMaybe.y = -1;
    return 1;
}

// FUNCTION: LOCO 0x434260
// Slot 17 -- "go stand somewhere on that object". The destination is NOT the object's own
// anchor but a uniformly random point inside its footprint rect, drawn per axis, so a crowd
// heading for the same building spreads across its whole floor instead of stacking on one pixel.
//
// Three ways out short of that:
//   - no target, an invalid target, or no track graphs yet (nothing to path across) drops the
//     step target to -1/-1 and, if the actor is ready, re-poses it in place;
//   - a target with no footprint rect, or one whose kind is not counted in the ready-object
//     census, falls back to heading for the anchor point itself.
//
// The per-axis draw is the shared RAND_RANGE_MAYBE guard (src/RandRange.h), and the rect's
// edges are NOT assumed ordered -- each axis tests which edge is the low one first. That is the
// same lo/hi-swap pair AnimEffectObj0x477a90's own ctor draws its viewport positions with.
//
// EFFECTIVE MATCH -- PARKED (asmscore --len 331: total 36355, reg_pen 3, byte_diff 25, insns
// 123/125, compiled 315 B against ~327 B of original code). ONE cluster, and it is exactly the
// two missing instructions: in the idle path the original RE-READS ptStepMaybe.x/.y out of the
// object to build the ComputeStepTargetMaybe argument list, where this compile forwards the
// `or eax,0xffffffff` it just stored into both members. The cause is visible in the schedule --
// this build hoists the `bReady` load ABOVE the two -1 stores (offset 0x10c vs the original's
// 0x11b), which is what lets store-to-load forwarding fire; the original loads it after, so the
// reload survives. Everything else -- all three early-out arms, both RAND_RANGE_MAYBE pairs
// including their lo/hi swaps, and the duplicated slot-16 call tail VC5 emits for the y arm's
// zero-span guard -- pairs instruction-for-instruction. Same no-caching family as Yoda lesson
// #19, but here it is the SCHEDULER rather than the source shape that decides, so there is
// nothing at source level left to flip.
void DecorActorBase::HeadForObjectMaybe(TilePlacedObj *pTarget)
{
    if (pTarget == 0 || pTarget->bValid != true || g_worldBoard.bTrackGraphsBuiltFlag == 0) {
        ptStepMaybe.x = -1;
        ptStepMaybe.y = -1;
        if (bReady != false) {
            ComputeStepTargetMaybe(ptStepMaybe.x, ptStepMaybe.y, 0);
        }
        return;
    }
    RECT rectFootprint;
    if (pTarget->GetFootprintRectMaybe(&rectFootprint) == 0 ||
        pTarget->pKindDesc->bCountedInReadyBigObjCount == 0) {
        SetDestinationTileMaybe(pTarget->hotspotPosX, pTarget->hotspotPosY);
        return;
    }
    int nX;
    if (rectFootprint.left <= rectFootprint.right) {
        nX = RAND_RANGE_MAYBE(rectFootprint.left, rectFootprint.right);
    } else {
        nX = RAND_RANGE_MAYBE(rectFootprint.right, rectFootprint.left);
    }
    int nY;
    if (rectFootprint.top <= rectFootprint.bottom) {
        nY = RAND_RANGE_MAYBE(rectFootprint.top, rectFootprint.bottom);
    } else {
        nY = RAND_RANGE_MAYBE(rectFootprint.bottom, rectFootprint.top);
    }
    SetDestinationTileMaybe(nX, nY);
}

// Same TU-local-view dodge src/TilePlacedObj.cpp uses for TrackTileObj's own slot overrides, and
// for the same measured reason: 0x4343b0 really is DecorActorBase::BlitAnimFrameMaybe, but
// declaring it on the class in src/DecorActor.h costs 1455 B across src/WorldBoardMaybe.cpp and
// src/RoadVehicleActor.cpp -- see the priced ⚠ note at the (deliberately absent) declaration
// there. This view IS slot 11, so unlike the base-reaching views it must redeclare the method,
// and it reaches the base body by explicit qualification.
struct DecorActorBaseBlitView0x4343b0 : DecorActorBase {
    void BlitAnimFrameMaybe(RECT rect, char flag, unsigned int flags); // 0x4343b0, slot 11 (+0x2c)
};

// FUNCTION: LOCO 0x4343b0
// Slot 11 (+0x2c) -- DecorActorBase's override of AnimDescRefObj0x477488::BlitAnimFrameMaybe, and
// it does nothing but chain the base. All three actor vtables (0x477eb8, 0x477f18, 0x4780b8) carry
// this address at +0x2c where the two non-actor tables carry the base's own 0x405e60, so the
// override really is here and really is empty -- an actor draws exactly like any other
// descriptor-backed object.
//
// The whole 50-byte body is the by-value RECT being rebuilt for the forwarded call (`sub esp,0x10`
// plus four stores, the copy that pins `rect` as one struct argument rather than four ints -- see
// the slot-11 note in src/WidgetBase.h), so there is nothing here a programmer wrote beyond the
// one qualified call.
void DecorActorBaseBlitView0x4343b0::BlitAnimFrameMaybe(RECT rect, char flag, // TODO: sync (TU-local view)
                                                       unsigned int flags) {
    AnimDescRefObj0x477488::BlitAnimFrameMaybe(rect, flag, flags);
}

// FUNCTION: LOCO 0x4343f0
// Slot 19 -- the "current destination is unreachable" recovery. Walks every node of the graph
// and returns whichever REACHABLE one (GetStepDirectionMaybe reports a real direction, i.e.
// neither 0x80 nor 0xff) lies closest to the destination the actor still wants; if none beats
// the actor's own present distance to that destination, the caller's node id comes back
// unchanged. Distances are compared as UNSIGNED squared magnitudes -- the original's `jae` --
// so no square root is ever taken.
//
// ⚠ Two spellings in the loop are load-bearing and were each worth a large step:
//   - the reachability test is a SWITCH, not `bDir != 0x80 && bDir != 0xff`. The original's
//     `and eax,0xff; sub eax,0x80; je; sub eax,0x7f; je` is VC5's sequential-compare switch
//     lowering (the second immediate is the DELTA, 0x80 + 0x7f = 0xff); the two-compare form
//     gives a plain `cmp al,0x80 / cmp al,0xff` pair instead. DIFF 67 -> 26.
//   - the direction must land in a named `unsigned char` LOCAL before the switch. That is what
//     produces the original's byte-store/dword-reload round trip through the stack slot
//     (`mov byte [esp+0x18],al; mov eax,[esp+0x18]`); switching directly on the call expression
//     keeps it in a register and drops those two instructions. Length 148 -> 165 = the
//     original's exact code length.
//
// EFFECTIVE MATCH -- PARKED (asmscore --len 165: total 25464, reg_pen 13, byte_diff 34, insns
// 64/64, compiled 165 B = the original's exact code length). The whole residual is one cluster:
// building CalcSqDist's argument list, the original loads pGraph->papNode BEFORE ptDestMaybe.y
// and finishes the papNode[i]->pTile chain between the two destination pushes, where this
// compile front-loads both destination components instead -- a 3-register (EAX/ECX/EDX) rename
// across ten otherwise instruction-for-instruction loads and pushes.
// **Measured and REJECTED -- do NOT re-run:** inlining the `pTile` local into the call
// expression (`pGraph->papNode[i]->pTile->hotspotPosX, ...->hotspotPosY`) is IDENTICAL --
// DIFF(26) either way, cl CSEs the repeated chain to the same schedule, so the local is not
// what is steering the order.
int DecorActorBase::PickReachableDestNodeMaybe(TrackGraph *pGraph, int nFromNodeId)
{
    unsigned int nBestSqDist = CalcSqDist(hotspotPosX, hotspotPosY, ptDestMaybe.x, ptDestMaybe.y);
    unsigned int nBestNodeId = nFromNodeId;
    unsigned int i;
    for (i = 0; i < pGraph->nNodeCount; i++) {
        unsigned char bDir = pGraph->GetStepDirectionMaybe(nFromNodeId, i);
        switch (bDir) {
        case 0x80: // no route from here
        case 0xff: // not a node of this graph
            break;
        default:
            TilePlacedObj *pTile = pGraph->papNode[i]->pTile;
            unsigned int nSqDist =
                CalcSqDist(pTile->hotspotPosX, pTile->hotspotPosY, ptDestMaybe.x, ptDestMaybe.y);
            if (nSqDist < nBestSqDist) {
                nBestSqDist = nSqDist;
                nBestNodeId = i;
            }
            break;
        }
    }
    return nBestNodeId;
}

// The manager's own vtable (5 slots), stamped manually since the class models it as a plain
// pointer field (see src/DecorObjMgrMaybe.h). Same extern shape as src/Obj0x477798Family.cpp.
extern void *g_vtable0x477f70[];

// FUNCTION: LOCO 0x434500 (DecorObjMgrMaybe::DecorObjMgrMaybe)
// The real constructor (SEH-framed -- one /GX state per member with a non-trivial dtor: lockA,
// lockB, regCategory7, regCategory8), reached from the CRT init-term of the DAT_00485448
// singleton below. The member-init list runs the two in-class registry ctors (base reserve for
// 100 slots + sort-key zeroing, the store order in src/DecorObjMgrMaybe.h); the body then stamps
// the manager vtable, zeroes the three scalars, and configures each registry's sort key through
// a REAL virtual dispatch on slot 19 (`call [vtbl+0x4c]` -- VC5 does not devirtualize a call on
// a fully-constructed MEMBER object the way it does a call inside the member's own ctor):
// (0x7c, 10) for category 7 (a 10-byte memcmp key), (0xc, -4) for category 8 (a 4-byte int).
DecorObjMgrMaybe::DecorObjMgrMaybe()
    : regCategory7Maybe(100), regCategory8Maybe(100) {
    pVtblMaybe = g_vtable0x477f70;
    nActiveCategory7Maybe = 0;
    nActiveCategory8Maybe = 0;
    bThrottleMaybe = 0;
    regCategory7Maybe.SetSortParamsAndSortMaybe(0x7c, 10);
    regCategory8Maybe.SetSortParamsAndSortMaybe(0xc, -4);
}

// FUNCTION: LOCO 0x4345d0 (??_GDecorObjMgrMaybe scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x4345f0 (DecorObjMgrMaybe::~DecorObjMgrMaybe)
// The real destructor (SEH-framed -- one /GX state per member with a non-trivial dtor),
// reached from the CRT atexit-term of the DAT_00485448 singleton below. The body is only the
// manager-vtable re-stamp; the members then destruct in reverse declaration order and do the
// rest: each registry runs the INLINED in-class ~PlacedObjCollectionMaybe (zero both counts,
// free the entry array, null it -- the derived half's implicit dtor dead-store-eliminates
// down to that same base re-stamp, which is why the original shows only the two BASE vtable
// stores), then the two locks go to the out-of-line LockableMaybe dtor (0x4493f0).
//
// EFFECTIVE MATCH -- PARKED (asmscore --len 160: total 20144, reg_pen 1, byte_diff 34, insns
// 46/46, compiled 160 B = the original's exact code length). The whole residual is TWO /Og
// scheduling tie-breaks, instruction-for-instruction identical otherwise: (a) BOTH registry
// teardowns emit the compiler's vptr re-store BEFORE the body's first field zero where the
// original schedules the nCount store first, and (b) the category-8 teardown hoists the
// state-2 store above the array-null CMP where the original does them in the other order.
// **Measured and REJECTED -- do NOT re-run:** (1) moving the teardown to a user dtor on the
// DERIVED registry class (empty virtual dtor on the base) -- +36 B, DIFF(135): a second /GX
// state per registry member, the derived vptr store NOT dead-store-eliminated, saved-EDI +
// lea reloads; (2) swapping the nCount/nCapacity body order -- DIFF(23), worse than 21;
// (3) dropping the pArray-hoist local (plain `if (pArrayMaybe != 0)`) -- byte-identical to
// the kept shape, DIFF(21) either way.
DecorObjMgrMaybe::~DecorObjMgrMaybe() {
    pVtblMaybe = g_vtable0x477f70;
}

// DAT_00485448 -- the singleton. The definition (not just the ctor) is what makes this TU emit
// 0x434500 at all: until v518 the class had no user ctor and the object was never defined in src.
DecorObjMgrMaybe DecorObjMgrMaybe_00485448;

// FUNCTION: LOCO 0x434690
// Re-drive every registered actor's own destination virtual with the destination it already
// holds -- i.e. put each one back on the path it was following. Called from the build-mode
// entry path after a board rebuild, which is why the whole body is gated on being in game mode:
// out of it there is no board to walk back onto.
void DecorObjMgrMaybe::RestoreEntryPositionsMaybe() {
    if (IsInGameModeMaybe()) {
        unsigned int i;
        PlacedObjRegistryMaybe &reg7 = regCategory7Maybe;
        for (i = 0; i < reg7.Count(); i++) {
            DecorActorBase *pActor = (DecorActorBase *)reg7.GetAt(i);
            pActor->SetDestinationTileMaybe(pActor->ptDestMaybe.x, pActor->ptDestMaybe.y);
        }
        PlacedObjRegistryMaybe &reg8 = regCategory8Maybe;
        for (i = 0; i < reg8.Count(); i++) {
            DecorActorBase *pActor = (DecorActorBase *)reg8.GetAt(i);
            pActor->SetDestinationTileMaybe(pActor->ptDestMaybe.x, pActor->ptDestMaybe.y);
        }
    }
}

// FUNCTION: LOCO 0x434970
// Hand out workplaces: run the "activate" virtual over every category-7 actor that has not got
// one yet. Called immediately after RestoreEntryPositionsMaybe above at both of its call sites,
// and gated on the same screen state for the same reason -- ActivateMaybe scans the board for a
// building with room, so there has to be a board.
//
// Two things make an entry eligible, and both are skips rather than failures:
//   - pOwnerObjMaybe is still null, i.e. the actor has no workplace. ActivateMaybe is what fills
//     that field in, so this is exactly "has this actor already been placed";
//   - its kind declares a bitmap footprint (bBitmapOccupancyRows != 0). A kind with no
//     footprint occupies no tile, so there is nothing for the workplace search to stand on.
//
// Only the category-7 registry is walked -- category 8 is the road vehicles, which have no
// workplace concept at all (nothing ever writes their +0x90).
void DecorObjMgrMaybe::ActivateEligibleEntriesMaybe() {
    if (IsInGameModeMaybe()) {
        unsigned int i;
        PlacedObjRegistryMaybe &reg7 = regCategory7Maybe;
        for (i = 0; i < reg7.Count(); i++) {
            DecorActorBase *pActor = (DecorActorBase *)reg7.GetAt(i);
            if (pActor->pOwnerObjMaybe == 0 && pActor->pKindDesc->bBitmapOccupancyRows != 0) {
                pActor->ActivateMaybe();
            }
        }
    }
}

// FUNCTION: LOCO 0x434800
// Dirty-mark every registered actor, in both categories -- the full-repaint hammer the
// build-mode entry and screen-state change paths reach for. Same two-registry walk as
// RestoreEntryPositionsMaybe above, minus the in-game-mode gate: this one runs whatever
// screen we are on, because the screen is exactly what just changed.
void DecorObjMgrMaybe::MarkAllEntriesDirtyMaybe(int bUnusedFlagMaybe) {
    unsigned int i;
    PlacedObjRegistryMaybe &reg7 = regCategory7Maybe;
    for (i = 0; i < reg7.Count(); i++) {
        ((DecorActorBase *)reg7.GetAt(i))->MarkDirty();
    }
    PlacedObjRegistryMaybe &reg8 = regCategory8Maybe;
    for (i = 0; i < reg8.Count(); i++) {
        ((DecorActorBase *)reg8.GetAt(i))->MarkDirty();
    }
}

// FUNCTION: LOCO 0x4349d0
// EFFECTIVE MATCH -- DIFF(245), insns 133/129, align=58, reg_pen=0, identity_miss=0. Every
// instruction, operand, branch target and register assignment agrees; the ENTIRE residual is one
// root cause, the documented zero-register residency class (docs/CODEGEN.md, v375/v360): the
// original keeps `pActor`'s 0 resident in esi and spends it as a comparison operand and as a
// store value -- `cmp eax,esi` at the pCantHave/`operator new` null checks (mine: `test eax,eax`,
// same 2 bytes), `mov [esp+0x18],esi` for case 7's EH state (mine: the 8-byte `mov …,0`), and no
// re-zeroing at all on the failed-`new` path (mine adds `jmp`/`xor esi,esi` twice, 4 instructions
// / 8 bytes). Probed and REFUTED this session: hoisting the `DecorActorBase *pActor = 0;`
// declaration to the top of the function (strictly worse, 119748 -- it costs esi its pKind
// tenancy), and the implicit-bool null-test spelling `if (pCantHave)` / `if (pActor)` (bit-for-bit
// inert, exactly as docs/CODEGEN.md predicts). Three source shapes WERE load-bearing and are
// already applied: the must-have guard is ONE `if` with `||` (so both its failure legs cross-jump
// into the pCantHave `return 0`), the live-count test on the cant-have side is `> 0` and not
// `!= 0` (that is what makes it `jbe` rather than `je`), and each arm's validity test is written
// NEGATED with the delete first (`if (bValid != 1) { delete } else { … }`). A separate typed local
// per arm (`WalkerActor *pWalker = new …; … pActor = pWalker;`) was also tried and is much worse
// (96820) -- it spills pActor to the stack. PARKED (docs/PARKED.md).
//
// The one and only ambient-actor factory: build the right leaf for kindId, hand it the building
// that asked for it, place it at world (x, y) and register it in the matching category registry.
// The caller (TilePlacedObj::SpawnOwnedActorMaybe) already holds that registry's lock.
//
// The two ini-declared prerequisite kinds are checked FIRST, and both descriptors are resolved
// unconditionally before either is tested -- nMustHaveKindId/nCantHaveKindId are KIND IDS, not bit
// masks: the must-have kind has to be alive somewhere in the world (unless it is -1, i.e. absent)
// and the cant-have kind must not be.
DecorActorBase *DecorObjMgrMaybe::SpawnActorForKindMaybe(int kindId, TilePlacedObj *pOwner, int x,
                                                        int y) {
    CursorDesc *pKind = g_UIResources.TileKind_GetOrLoadDescriptor(kindId);
    CursorDesc *pMustHave = g_UIResources.TileKind_GetOrLoadDescriptor(pKind->nMustHaveKindId);
    CursorDesc *pCantHave = g_UIResources.TileKind_GetOrLoadDescriptor(pKind->nCantHaveKindId);
    if (pKind->nMustHaveKindId != -1 &&
        (pMustHave == 0 || pMustHave->nLiveInstanceCountMaybe == 0)) {
        return 0;
    }
    DecorActorBase *pActor = 0;
    if (pCantHave != 0 && pCantHave->nLiveInstanceCountMaybe > 0) {
        return 0;
    }
    switch ((unsigned char)TileKind_GetCategory(kindId)) {
    case 7:
        pActor = new WalkerActor(kindId);
        if (pActor != 0) {
            if (pActor->bValid != 1) {
                delete pActor;
                pActor = 0;
            } else {
                pActor->pSpawnerObjMaybe = pOwner;
                pActor->RepositionWithHotspot(x, y);
                regCategory7Maybe.InsertInSortedPositionMaybe(pActor);
                nActiveCategory7Maybe++;
            }
        }
        break;
    case 8:
        pActor = new RoadVehicleActor(kindId);
        if (pActor != 0) {
            if (pActor->bValid != 1) {
                delete pActor;
                pActor = 0;
            } else {
                pActor->pSpawnerObjMaybe = pOwner;
                pActor->RepositionWithHotspot(x, y);
                regCategory8Maybe.InsertInSortedPositionMaybe(pActor);
                nActiveCategory8Maybe++;
            }
        }
        break;
    }
    return pActor;
}

// FUNCTION: LOCO 0x434720
// The per-frame actor tick (sole caller: FrameDriver_TickMaybe). First clears the
// population-pressure throttle once 300 ticks have passed since it was raised, then ticks
// every actor in both category registries. Each entry is ticked with its SUCCESSOR in
// registry order as the TickMaybe argument (the walker trail chain: FollowLeaderStepMaybe
// chases that entry's published trail anchor); the walk's `i < Count()` condition
// re-calls Count() every iteration, so the last entry is handed the past-the-end GetAt's
// NULL. Category-8 entries are ticked only while bValid == 1. The walk is a
// pre-incrementing while (`i = 0; while (i < Count()) { i++; pNext = GetAt(i); ... }`),
// not a for -- the original enters it with EBX=0 and INCs at the top of the body.
// Finishes with the category-8 registry's >= 2-gated resort under lockBMaybe, the exact
// shape TickCategory7OnlyMaybe has below.
//
// EFFECTIVE MATCH (v517, DIFF(75), 212/212 B, insns 93/93). Everything pairs
// instruction-for-instruction; the WHOLE residual is ONE register-role ripple: the
// original keeps `this` in EBP (EBX = the loop counter), this build keeps it in EBX
// (counter in EDI) -- every differing byte is that one assignment propagating through the
// two registry walks (pNext lands in EDI vs EBX, the counter in EBX vs EDI/EBP, and the
// prologue's push/spill order follows). Levers baked in: the pre-incrementing while (a
// `for (i = 1; i <= Count(); i++)` starts the counter at 1, not the original's EBX=0 +
// top-of-body INC), Count() spelled as the loop condition so it is re-called every
// iteration, and the `(unsigned int)... >= 2` resort gate. Probe refuted (WORSE, DIFF
// 171): hoisting the `i = 0` declaration above pActor's. Parked in docs/PARKED.md.
void DecorObjMgrMaybe::TickCategory7And8Maybe() {
    if (bThrottleMaybe && dwLastTickMaybe + 300 < (int)g_dwGameTick) {
        bThrottleMaybe = false;
    }
    DecorActorBase *pActor = (DecorActorBase *)regCategory7Maybe.GetAt(0);
    unsigned int i = 0;
    while (i < regCategory7Maybe.Count()) {
        i++;
        DecorActorBase *pNext = (DecorActorBase *)regCategory7Maybe.GetAt(i);
        pActor->TickMaybe(pNext);
        pActor = pNext;
    }
    pActor = (DecorActorBase *)regCategory8Maybe.GetAt(0);
    i = 0;
    while (i < regCategory8Maybe.Count()) {
        i++;
        DecorActorBase *pNext = (DecorActorBase *)regCategory8Maybe.GetAt(i);
        if (pActor->bValid == 1) {
            pActor->TickMaybe(pNext);
        }
        pActor = pNext;
    }
    if ((unsigned int)nActiveCategory8Maybe >= 2) {
        lockBMaybe.Lock();
        regCategory8Maybe.SortAllMaybe();
        lockBMaybe.Unlock();
    }
}

// FUNCTION: LOCO 0x434870
// Re-sort the walker registry, under its own lock, whenever the walker population's sort key can
// have changed -- an actor leaving (DeregisterEntryMaybe) or a newly built one turning out to
// carry a per-instance category name (DecorActorBase's ctor). The count test is UNSIGNED, and
// spelled `>= 2` rather than `> 1` -- the two are not the same to VC5, which emits the compare
// against the literal as written (`cmp ...,2; jb` vs `cmp ...,1; jbe`), DIFF(2) apart. It mirrors
// the registry's own slot-20 guard, which makes the same "more than one entry" test.
//
// The vehicle registry has no counterpart: nothing ever re-sorts it outside its own inserts.
void DecorObjMgrMaybe::TickCategory7OnlyMaybe() {
    if ((unsigned int)nActiveCategory7Maybe >= 2) {
        lockAMaybe.Lock();
        regCategory7Maybe.SortAllMaybe();
        lockAMaybe.Unlock();
    }
}

// FUNCTION: LOCO 0x4348a0 (DecorObjMgrMaybe::BlitActorsInRectMaybe)
// Paint every registered actor whose sprite intersects the flushed dirty tile rect: for each
// registry, walk it front to back and dispatch each entry's own BlitAnimFrameMaybe (root
// vtable slot 11) with the rect forwarded BY VALUE and the caller's bFlag. Runs only on paint
// pass 0 (the iPlaneMaybe < 1 gate) -- the caller (WorldBoardMaybe's dirty-tile repaint,
// 0x456700) invokes it on both passes and the actors simply sit pass 1 out. The vehicle
// registry (category 8, kept in screen-Y order by its rect.top sort key) gets an early-out:
// the first entry whose top edge is already below the dirty rect's bottom ends the walk. The
// walker registry (category 7, sorted alphabetically) has no such shortcut and is walked to
// the end. Also this manager's OWN vtable slot 4 (0x477f80).
void DecorObjMgrMaybe::BlitActorsInRectMaybe(short iPlaneMaybe, RECT rect, int bFlag) {
    if (iPlaneMaybe <= 0) {
        PlacedObjRegistryMaybe &reg8 = regCategory8Maybe;
        int nBottom = rect.bottom;
        for (unsigned int i = 0; i < reg8.Count(); i++) {
            DecorActorBase *pActor = (DecorActorBase *)reg8.GetAt(i);
            pActor->BlitAnimFrameMaybe(rect, bFlag, 0);
            if (pActor->rect.top > nBottom) break;
        }
        PlacedObjRegistryMaybe &reg7 = regCategory7Maybe;
        for (unsigned int j = 0; j < reg7.Count(); j++) {
            DecorActorBase *pActor = (DecorActorBase *)reg7.GetAt(j);
            pActor->BlitAnimFrameMaybe(rect, bFlag, 0);
        }
    }
}

// FUNCTION: LOCO 0x434b60
// Unregister pActor from whichever of the two category registries its own kind descriptor's
// category byte selects, decrement that category's live count, and destroy it.
//
// sic: the registry mutation (slot 14 index-of + slot 3 remove-at) happens BEFORE the matching
// lock is taken -- only the count decrement and the delete are actually inside the critical
// section. Reproduced, not fixed.
void DecorObjMgrMaybe::DeregisterEntryMaybe(DecorActorBase *pActor, char bDeleteMaybe) {
    if (pActor == 0) {
        return;
    }
    int nCategory = GetActorCategoryMaybe(pActor);
    DecorActorBase *pRemoved;
    int *pnActive;
    if (nCategory == 7) {
        pRemoved = (DecorActorBase *)regCategory7Maybe.RemoveAtShiftingTail(
            regCategory7Maybe.IndexOfMaybe(pActor));
        pnActive = &nActiveCategory7Maybe;
        lockAMaybe.Lock();
    } else if (nCategory == 8) {
        pRemoved = (DecorActorBase *)regCategory8Maybe.RemoveAtShiftingTail(
            regCategory8Maybe.IndexOfMaybe(pActor));
        pnActive = &nActiveCategory8Maybe;
        lockBMaybe.Lock();
    } else {
        return;
    }
    if (pRemoved == pActor) {
        (*pnActive)--;
        if (IsInGameModeMaybe() && bDeleteMaybe == 1) {
            DAT_004fd220.EffectSpawner_SpawnAtPositionMaybe(0x3860, 0, 'W', pActor->hotspotPosX,
                                                            pActor->hotspotPosY, 1);
        }
        delete pActor;
    }
    if (nCategory == 7) {
        lockAMaybe.Unlock();
        return;
    }
    if (nCategory == 8) {
        lockBMaybe.Unlock();
    }
}

// FUNCTION: LOCO 0x434c50
// "Did a click at screen (x, y) land on one of my actors": walks registry 7 and then registry 8,
// stopping at the first entry whose own Contains says yes, and routes it into whichever of
// PlacementCursorMaybe's two pending actions is armed -- A hands the actor to the drag cursor,
// B selects it. Returns non-zero when an actor consumed the click.
//
// The two registry passes are deliberately NOT symmetric: only the walker pass (7) skips null
// slots and un-bReady actors, and only the vehicle pass (8) additionally refuses to start a drag
// while the cursor's snap lock is held.
char DecorObjMgrMaybe::ResolveClickMaybe(int x, int y) {
    char bHandled = 0;
    if (IsInGameModeMaybe()) {
        unsigned int i;
        PlacedObjRegistryMaybe &reg7 = regCategory7Maybe;
        for (i = 0; i < reg7.Count(); i++) {
            if (bHandled != 0) {
                break;
            }
            DecorActorBase *pActor = (DecorActorBase *)reg7.GetAt(i);
            if (pActor != 0 && pActor->bReady == true && pActor->Contains(x, y) != 0) {
                if (PlacementCursorMaybe_004854c8.bPendingActionAMaybe == true) {
                    PlacementCursorMaybe_004854c8.SetHoverObjMaybe(pActor);
                }
                if (PlacementCursorMaybe_004854c8.bPendingActionBMaybe == true) {
                    SelectedObjWidgetMaybe_004852a0.SelectObjMaybe((int)pActor);
                }
                bHandled = 1;
            }
        }
        // sic: no null/bReady guard on this side, so a registry-8 slot that is still empty
        // dispatches a virtual call through a NULL vptr. Reproduced, not fixed.
        PlacedObjRegistryMaybe &reg8 = regCategory8Maybe;
        for (i = 0; i < reg8.Count(); i++) {
            if (bHandled != 0) {
                break;
            }
            DecorActorBase *pActor = (DecorActorBase *)reg8.GetAt(i);
            if (pActor->Contains(x, y) != 0) {
                if (PlacementCursorMaybe_004854c8.bPendingActionAMaybe == true &&
                    PlacementCursorMaybe_004854c8.bSnapLockMaybe == false) {
                    PlacementCursorMaybe_004854c8.SetHoverObjMaybe(pActor);
                }
                if (PlacementCursorMaybe_004854c8.bPendingActionBMaybe == true) {
                    SelectedObjWidgetMaybe_004852a0.SelectObjMaybe((int)pActor);
                }
                bHandled = 1;
            }
        }
    }
    return bHandled;
}

// FUNCTION: LOCO 0x434d70
// One placed object's per-tick goal-rule pass, driving the two BigObjSeqRecordMaybe records the
// object's own kind descriptor carries -- MobileSeq (+0x590) and TotalVisits (+0x5c4). See
// docs/subsystems.md's seq-record writeup for the record layout and for the two reward halves
// on TilePlacedObj this fires.
//
// sic: only element [0] of a record's paValues array is ever read, even though the parser fills
// (and ulValueCount counts) up to 0x2d of them -- so a kind list with more than one entry
// silently only ever matches its first. Reproduced, not fixed.
unsigned char DecorObjMgrMaybe::TickObjSeqGoalsMaybe(TilePlacedObj *pObj) {
    unsigned char bMobileGoalMet = 0;
    unsigned char bTotalVisitsGoalMet = 0;

    if (pObj->bValid != 1) {
        return 0;
    }

    BigObj *pKind = pObj->pKindDesc;
    // src/CursorDesc.h deliberately keeps the three seq-record spans FLAT (a named record type
    // in that shared header rotates DPlaySessionMgr.cpp's codegen -- the v331 bisect), so the
    // records are reached as the address of their own leading named field.
    BigObjSeqRecordMaybe *pMobileRec = (BigObjSeqRecordMaybe *)&pKind->lMobileSeqHeadMaybe;
    if (pKind->lMobileSeqHeadMaybe <= 0 && pKind->lTotalVisitsHeadMaybe <= 0) {
        return 0;
    }
    // The cooldown gate is ONE test with two halves sharing a single `return 0` (the original
    // has exactly three return-0 epilogues, and this is the third): a negative EEReplayDelay
    // means "never replay" once the object has fired at least once, and otherwise the
    // descriptor's own delay buys back part of the 60-tick lockout stamped below.
    if ((pObj->dwSeqGoalCooldownUntilMaybe != 0 && pKind->lEEReplayDelay < 0) ||
        (int)pObj->dwSeqGoalCooldownUntilMaybe >
            pKind->lEEReplayDelay + (int)g_dwGameTick) {
        return 0;
    }

    int nPosX = (short)pObj->pos.wPosX;
    int nPosY = (short)pObj->pos.wPosY;
    RECT rc;
    SetRect(&rc, (nPosX << 4) - 0x10,
            (nPosY - pKind->bFootprintYSteps + pKind->bBitmapOccupancyRows - 2) << 4,
            (pKind->bBitmapOccupancyCols + nPosX + 1) << 4,
            (nPosY + pKind->bBitmapOccupancyRows + 1) << 4);

    if (IsInGameModeMaybe()) {
        if ((pKind->lMobileSeqHeadMaybe > 0 && pKind->paMobileSeqValues != 0) ||
            (pKind->lTotalVisitsHeadMaybe > 0 && pKind->paTotalVisitsValues != 0)) {
            PlacedObjRegistryMaybe &reg = regCategory7Maybe;
            unsigned short nMobileMatches = 0;
            unsigned short i;
            for (i = 0; i < reg.Count(); i++) {
                DecorActorBase *pActor = (DecorActorBase *)reg.GetAt(i);
                unsigned char bInRect =
                    (unsigned char)PtInRect(&rc, *(POINT *)&pActor->hotspotPosX);
                if (pKind->paMobileSeqValues != 0 && bInRect &&
                    (pActor->pKindDesc->resourceId == pKind->paMobileSeqValues[0] ||
                     pKind->paMobileSeqValues[0] == -1)) {
                    nMobileMatches++;
                    if (nMobileMatches >= pKind->lMobileSeqHeadMaybe) {
                        bMobileGoalMet = 1;
                    }
                }
                if (pKind->paTotalVisitsValues != 0 && bInRect &&
                    (pActor->pKindDesc->resourceId == pKind->paTotalVisitsValues[0] ||
                     pKind->paTotalVisitsValues[0] == -1)) {
                    pObj->dwPlacementArgB++;
                }
            }
            // The MobileSeq tally is per-tick and tested `>=` INSIDE the loop; the TotalVisits
            // one accumulates across ticks in the object itself and is tested `>` here.
            if (pObj->dwPlacementArgB > pKind->lTotalVisitsHeadMaybe) {
                bTotalVisitsGoalMet = 1;
                pObj->dwPlacementArgB = 0;
            }
            BigObjSeqRecordMaybe *pTotalVisitsRec =
                (BigObjSeqRecordMaybe *)&pKind->lTotalVisitsHeadMaybe;
            if (bMobileGoalMet == 1) {
                pObj->dwSeqGoalCooldownUntilMaybe = g_dwGameTick + 0x3c;
                pObj->ApplySeqRecordChangeMaybe(pMobileRec);
                pObj->SpawnSeqRecordEffectMaybe(pMobileRec);
                ApplySeqRecordToActorsMaybe(rc, pMobileRec);
            }
            if (bTotalVisitsGoalMet == 1) {
                pObj->dwSeqGoalCooldownUntilMaybe = g_dwGameTick + 0x3c;
                pObj->ApplySeqRecordChangeMaybe(pTotalVisitsRec);
                pObj->SpawnSeqRecordEffectMaybe(pTotalVisitsRec);
                ApplySeqRecordToActorsMaybe(rc, pTotalVisitsRec);
            }
        }
    }
    return bMobileGoalMet;
}

// FUNCTION: LOCO 0x435020
// The pSelf == 0 tail call out of TestActorCollisionMaybe: "is ANYTHING standing in rcNew".
// Same two registry passes and the same intersect + localise + mask test per candidate, but with
// no self to exclude there is no Y-distance filter and no second (self-side) mask test either --
// so a single opaque pixel of any ready actor inside the overlap answers the query. Returns the
// same category codes its caller does, 7 or 8, or 0 for "clear".
int DecorObjMgrMaybe::TestRectAgainstAllActorsMaybe(RECT rcNew) {
    int nResult = 0;
    unsigned int i;
    RECT rcOther;
    RECT rcHit;
    PlacedObjRegistryMaybe &reg7 = regCategory7Maybe;
    for (i = 0; i < reg7.Count(); i++) {
        if (nResult != 0) {
            break;
        }
        DecorActorBase *pOther = (DecorActorBase *)reg7.GetAt(i);
        if (pOther->bReady && IntersectRect(&rcHit, &rcNew, &pOther->rect)) {
            rcOther = rcHit;
            OffsetRect(&rcOther, -pOther->rect.left, -pOther->rect.top);
            OffsetRect(&rcOther, pOther->rectViewport.left, 0);
            if (pOther->pKindDesc->pOwnedObjA->HasOpaquePixelInRect(rcOther)) {
                nResult = 7;
            }
        }
    }
    PlacedObjRegistryMaybe &reg8 = regCategory8Maybe;
    for (i = 0; i < reg8.Count(); i++) {
        if (nResult != 0) {
            break;
        }
        DecorActorBase *pOther = (DecorActorBase *)reg8.GetAt(i);
        if (pOther->bReady && IntersectRect(&rcHit, &rcNew, &pOther->rect)) {
            rcOther = rcHit;
            OffsetRect(&rcOther, -pOther->rect.left, -pOther->rect.top);
            OffsetRect(&rcOther, pOther->rectViewport.left, 0);
            if (pOther->pKindDesc->pOwnedObjA->HasOpaquePixelInRect(rcOther)) {
                nResult = 8;
            }
        }
    }
    return nResult;
}

// FUNCTION: LOCO 0x435200
// "What would pSelf hit standing in rcNew" -- the movement veto both actor leaves consult before
// committing a step. Two independent registry passes, and the RETURN VALUE is the colliding
// actor's own category (7 or 8), which is what lets RoadVehicleActor::AdvanceMovementMaybe treat
// a walker collision differently from a vehicle one.
//
// The two passes use DIFFERENT mask tests, and only the vehicle one is actually pixel-correct:
// the walker pass asks each of the two sprites SEPARATELY whether it has any opaque pixel inside
// the overlap rect (so two sprites can "collide" on pixels that never line up), while the vehicle
// pass runs the real per-pixel TestPixelCollision over both masks at once. Not tagged `sic:` --
// there is no way to tell an optimisation from a bug here, only that the two disagree.
int DecorObjMgrMaybe::TestActorCollisionMaybe(RECT rcNew, DecorActorBase *pSelf) {
    int nResult = 0;
    if (pSelf == 0) {
        return TestRectAgainstAllActorsMaybe(rcNew);
    }
    unsigned char nMaxDeltaY = pSelf->pKindDesc->bFootprintLayerCount;
    unsigned int i;
    RECT rcOther;
    RECT rcSelf;
    RECT rcHit;
    PlacedObjRegistryMaybe &reg7 = regCategory7Maybe;
    for (i = 0; i < reg7.Count(); i++) {
        if (nResult != 0) {
            break;
        }
        DecorActorBase *pOther = (DecorActorBase *)reg7.GetAt(i);
        if (pOther->bReady && pOther != pSelf &&
            abs(pOther->hotspotPosY - pSelf->hotspotPosY) < nMaxDeltaY &&
            IntersectRect(&rcHit, &rcNew, &pOther->rect)) {
            rcOther = rcHit;
            OffsetRect(&rcOther, -pOther->rect.left, -pOther->rect.top);
            OffsetRect(&rcOther, pOther->rectViewport.left, 0);
            if (pOther->pKindDesc->pOwnedObjA->HasOpaquePixelInRect(rcOther)) {
                rcSelf = rcHit;
                OffsetRect(&rcSelf, -pSelf->rect.left, -pSelf->rect.top);
                OffsetRect(&rcSelf, pSelf->rectViewport.left, 0);
                if (pSelf->pKindDesc->pOwnedObjA->HasOpaquePixelInRect(rcSelf)) {
                    nResult = 7;
                }
            }
        }
    }
    PlacedObjRegistryMaybe &reg8 = regCategory8Maybe;
    for (i = 0; i < reg8.Count(); i++) {
        if (nResult != 0) {
            break;
        }
        DecorActorBase *pOther = (DecorActorBase *)reg8.GetAt(i);
        if (pOther->bReady && pOther != pSelf &&
            abs(pOther->hotspotPosY - pSelf->hotspotPosY) < nMaxDeltaY &&
            IntersectRect(&rcHit, &rcNew, &pOther->rect)) {
            rcOther = rcHit;
            OffsetRect(&rcOther, -pOther->rect.left, -pOther->rect.top);
            OffsetRect(&rcOther, pOther->rectViewport.left, 0);
            rcSelf = rcHit;
            OffsetRect(&rcSelf, -pSelf->rect.left, -pSelf->rect.top);
            OffsetRect(&rcSelf, pSelf->rectViewport.left, 0);
            if (pSelf->pKindDesc->pOwnedObjA->TestPixelCollision(
                    rcSelf, pOther->pKindDesc->pOwnedObjA, rcOther)) {
                nResult = 8;
            }
        }
    }
    return nResult;
}

// FUNCTION: LOCO 0x435580 // EFFECTIVE MATCH -- 299 B, insns 102/102, DIFF(4)
// (asmscore 444, align=0 reg_pen=4 identity_miss=4). Every instruction, operand and
// branch agrees; the ONLY residual is an ecx<->edx naming swap across the four-instruction
// PtInRect argument setup (the original loads rect.top into ecx and takes &rc into edx,
// this compile does the reverse). Probed, both no-ops on the score: spelling the point as
// `*(POINT *)&pActor->rect.left` instead of `&pActor->rect`, and hoisting it into its own
// named POINT local. Intrinsic allocator tie-break; PARKED (docs/PARKED.md).
// The ORIGINAL's `test al,al` on the PtInRect result IS source-steerable and is already
// applied -- the byte-typed local below is what produces it (a plain `if (PtInRect(...))`
// tests the full eax and costs one more byte).
// The ACTOR half of "a seq/visits goal in pRec was just met", and the mirror image of
// TilePlacedObj::ApplySeqRecordChangeMaybe (src/TilePlacedObj.cpp) -- that one retargets the
// building itself off the record's FIRST descriptor id (+0xc), this one retargets every
// category-7 actor standing in the fired rect off its SECOND, minifig-side id (+0x14, which
// the parser forces to -1 unless its tile category is 7).
//
// A matching actor is skipped while its own dwSeqRewardUntilMaybe deadline is still armed, so each actor can
// only be claimed by one goal at a time; the reward re-arms it for pRec->lUnk0x1cMaybe ticks.
// If the retarget invalidates the actor, its original spawn descriptor (nSpawnDescriptorIdMaybe) is put back
// and no deadline is armed.
//
// The UI-feedback guard is duplicated into both arms of the retarget branch, exactly as in
// 0x458820 -- ordinary VC5 tail duplication of a small block, not two source copies.
void DecorObjMgrMaybe::ApplySeqRecordToActorsMaybe(RECT rc, BigObjSeqRecordMaybe *pRec) {
    PlacedObjRegistryMaybe &reg = regCategory7Maybe;
    unsigned int i;
    for (i = 0; i < reg.Count(); i++) {
        DecorActorBase *pActor = (DecorActorBase *)reg.GetAt(i);
        if (pActor->pKindDesc->resourceId != pRec->paValues[0] && pRec->paValues[0] != -1) {
            continue;
        }
        unsigned char bInRect = (unsigned char)PtInRect(&rc, *(POINT *)&pActor->rect);
        if (!bInRect) {
            continue;
        }
        if (pActor->dwSeqRewardUntilMaybe != 0) {
            continue;
        }
        if (pRec->lTileIdB0x14Maybe == -1) {
            continue;
        }
        if (pRec->lTileIdB0x14Maybe != 0) {
            pActor->SetDescriptor(pRec->lTileIdB0x14Maybe, pRec->wUnk0x18Maybe, 0);
            if (pActor->bValid == 1) {
                pActor->dwSeqRewardUntilMaybe = pRec->lUnk0x1cMaybe + g_dwGameTick;
            } else {
                pActor->SetDescriptor(pActor->nSpawnDescriptorIdMaybe, -1, 0);
            }
            if (SelectedObjWidgetMaybe_004852a0.bActive) {
                continue;
            }
            if (g_worldActionCursor.bActive == 1 &&
                g_worldActionCursor.nModeMaybe == 3) {
                continue;
            }
        } else {
            pActor->ReleaseChannelAndDispatch(pRec->wUnk0x18Maybe);
            pActor->dwSeqRewardUntilMaybe = pRec->lUnk0x1cMaybe + g_dwGameTick;
            if (SelectedObjWidgetMaybe_004852a0.bActive) {
                continue;
            }
            if (g_worldActionCursor.bActive == 1 &&
                g_worldActionCursor.nModeMaybe == 3) {
                continue;
            }
        }
        g_UIResources.PlayUiSound(0x571e);
        g_worldActionCursor.SelectDecorObjAndDispatchModeMaybe(pActor);
    }
}

#ifdef LOCO_PORT
// ─── PORT SCAFFOLDING (no original counterpart) ────────────────────────────────
// XC 5 of 13: DecorObjMgrMaybe_00485448 (DAT_00485448), DecorObjMgrMaybe::DecorObjMgrMaybe (0x434500).
//
// The original constructs this global from the CRT's C++ dynamic-initializer table (.CRT$XC),
// which the port's zero-filled .bss mirror has no equivalent of. Declared in
// port/PortGlobalCtors.h, called from link/init_globals.cpp -- see either for the full story.
#include <new.h>
#include "PortGlobalCtors.h"

void Port_Construct_DecorObjMgr(void) {
    new (&DecorObjMgrMaybe_00485448) DecorObjMgrMaybe();
}
#endif // LOCO_PORT
