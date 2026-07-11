// WorldActionCursor method bodies. See the header / docs/subsystems.md for the class.
#include "WorldActionCursor.h"
#include "CarNetObj.h"               // CarNetObjVtblProbe (the car/decor SetNameImpl slot-0xd calls)

#include "AppWindow.h"              // g_pApp (hwndOwner, for the drag cursor warp)
#include "DecorObjMgrMaybe.h"       // DecorObjMgrMaybe_00485448.lockAMaybe (category-7 lock)
#include "Ddraw.h"                // g_pDDrawWorkSurface (the blit pair)
#include "DSoundChannel.h"         // DSoundChannel::Release
#include "LocoBitmap.h"            // LocoBitmap (LoadCategoryIconsMaybe's icon canvas)
#include "PlacementCursorMaybe.h"  // PlacementCursorMaybe_004854c8 singleton
#include "PeerTrainNode.h"         // PeerTrainNodePartial
#include "PeerTrainSlotQueueMaybe.h" // g_PeerTrainSlotQueue singleton (DAT_004a98b0)
#include "RandRange.h"             // RAND_RANGE_MAYBE
#include "ThreadWrapper.h"         // g_worldLoadThread -- WorldIdleEventPumpThreadProc's own thread
#include "TutorialWnd.h"           // g_pTutorialWnd->NotifyOrLaunch
#include "UIResources.h"           // g_UIResources singleton (DAT_004855e8)
#include "WorldBoardMaybe.h"       // g_worldBoard.MarkRectDirty

// TU-local externs (same pattern as src/Main.cpp / src/WorldBoardMaybe.cpp).
extern int g_nScreenState;              // app-state dword (3 = in-game), see src/GameNetMsgQueue.h
// The in-game app-state gate. The `unsigned char` return type is LOAD-BEARING: it is what
// reproduces the original's sete-materialized branch instead of a plain `cmp; je`. Cracked in
// v356 (SelectDecorObjAndDispatchModeMaybe: total 513668 -> 479365); see docs/CODEGEN.md. Kept
// TU-local because adding declarations to a shared header rotates other TUs (v340/v355/v356).
inline unsigned char IsInGameModeMaybe() { return g_nScreenState == 3; }
extern void *g_pActiveTabWidgetMaybe; // DAT_004fd3e0, see src/WidgetPicker.cpp // TODO: idiom
extern int g_bCmdlineSFlagSet;        // DAT_004a9918 -- SelectObjMaybe's deselect gate
extern unsigned int g_dwGameTick;     // DAT_004a99b4
// 0x485220 (tagRECT_00485220) -- the app's client-area bounds; same TU-local spelling
// src/PopupWndBase.cpp and src/WorldBoardMaybe.cpp use.
extern RECT g_rectAppClientBounds;
extern ThreadWrapper g_worldLoadThread; // DAT_004a9ad0, see src/FrameDriver.cpp

// 0x478538 -- the constant outer rect CenterModeAnimOverWidgetMaybe centers each mode icon
// within (left=7, top=70, right=164, bottom=224). Exactly 16 bytes in the original: a vtable
// begins at 0x478548. `extern` is repeated on the definition to keep the external linkage the
// header's declaration promises -- a bare file-scope `const` array would go internal and leave
// the symbol undefined.
//
// Defined rather than merely declared for the same reason as src/WorldBoardMaybe.cpp's
// DAT_0047f108 (v566): undefined data symbols become link/gen_stubs.py data stubs in an
// all-zero .bss mirror, so the port would center every mode icon inside an EMPTY rect.
extern const int g_anDecorCenterBoundsMaybe[4] = {7, 70, 164, 224};

// CursorDesc::IsItemAvailableMaybe (0x4255f0, ex-FUN_004255f0, named v349; untranscribed) -- the
// "is this palette/menu item offerable right now" predicate, read in full for the naming pass.
// True only when ALL of: bButtonVisible is set; pShadowBitmap (+0x24) is loaded; nButtonFrameCount
// (+0x2c) is non-zero (it has an icon to draw); the descriptor's nMustHaveKindId (+0x40) is either
// the -1 "no prerequisite" sentinel or names a TileKind whose own +0x158 is non-zero (the
// prerequisite is present); its nCantHaveKindId (+0x44) does NOT name such a TileKind (the
// exclusion is absent); and it is not the one hard-coded special case, resourceId 0xc42 while
// the DPlay session is in state 2. So the ini "must_have"/"cant_have" token pair is a live
// per-item gating rule, not just parse-time metadata. Declared free (not on CursorDesc.h, which has 4 OTHER
// consumers -- same header-rotation hazard as UIResources.h/WorldBoardMaybe.h) per the
// CarNetState.h/GameNetMsgQueue.h precedent for a single-register-arg __fastcall callee
// (byte-identical to Ghidra's own "this"-in-ecx-but-explicit-param rendering).
extern unsigned char __fastcall CursorDesc_IsItemAvailableMaybe(CursorDesc *pDesc); // 0x4255f0

// InitTrainCouplingMenuIconsMaybe (0x458c90) and its GetOrCreateIconItemMaybe helper are real
// WorldActionCursor members, declared on the shared header. They used to live on a TU-local
// WorldActionCursorView0x458c90 because a v341-era measurement found that adding those 2 method
// decls to the header rotated the OTHER 4 already-EXACT sibling functions in THIS SAME TU.
// ⚠ That price is STALE and was refuted when re-measured in v448: at the current dial position
// the promotion is not merely free, it is POSITIVE. Nothing in this TU moved (3/8, 1376 B before
// and after), src/LoadingScreen.cpp did not move (4/5, 984 B), and across the other 7
// WorldActionCursor.h consumers exactly one row changed -- src/TilePlacedObj.cpp GAINED
// GetFrontRowTilePosMaybe (0x458310) back, +60 B / +1 func. That is the same 60-byte
// sub-edx-eax/sub-eax-edx coin flip v442-v447 kept paying to this header family, landing on the
// good side for once. The promotion also fixes a byte-invisible wrong call target: LoadingScreen
// had been calling a free `WorldActionCursor_InitTrainCouplingMenuIconsMaybe` spelling that no
// definition anywhere in src/ provides (tools/lint_alias.py).

// SelectedObjWidgetMaybe::CanSelectDecorObjMaybe (0x42cf90) -- __cdecl predicate gating
// SelectDecorObjAndDispatchModeMaybe's select path: answers whether the candidate decor
// object's descriptor category (2/3/4/6/7/8/0xc, with per-category extra tests) may be
// selected in the current context. Declared TU-locally (free-function form of the Ghidra
// namespace name) so the shared header doesn't rotate (v333/v334/v336 lessons).
unsigned char SelectedObjWidgetMaybe_CanSelectDecorObjMaybe(AnimDescRefObj0x477488 *pDecor);

// TU-local method view carrying SelectedObjWidgetMaybe's slot-21 body (0x42d400) -- the
// class's OWN new virtual past the base's slot-20 tail. Slots 17/20 were folded onto the real
// class in v550; this third one is DELIBERATELY still here, because folding all THREE flips
// TilePlacedObj::GetFrontRowTilePosMaybe (0x458310) out of its 60 B EXACT and folding TWO is
// free -- this header's dial is a PARITY bit on that function, not a budget (v450/v550).
// The original v506 note follows, kept for the history:
// SelectedObjWidgetMaybe's vtable tail (slot 17 0x42d6b0, slot 20 0x42d770, slot 21 0x42d400)
// lived on a TU-local view struct here until v550, on v506's measurement that the shared header
// admitted exactly ONE new declaration before TilePlacedObj::SpawnSeqRecordEffectMaybe
// (0x4588b0) lost its 143 B EXACT. That budget has not been binding since v533; the three
// declarations were folded onto the real class in src/WorldActionCursor.h and measured free.
// +0xc of the decor-category manager sub-object at DAT_00485494 (the header's methods-only
// DecorCategoryMgrMaybe_00485494 probe is data-free): its registrant count, read by the
// mode-7 index clamp.
extern unsigned int g_nDecorCategory7CountMaybe; // DAT_004854a0 // TODO: sync

// Methods-only probe extending the header's DecorCategoryMgrVtblProbe with the vtbl+0x40
// slot this function calls (maps a decor object to its candidate index within the category;
// binary-searches [nLo, nHi]). Kept TU-local; used via cast on the shared extern.
struct DecorCategoryMgrProbe0x459180 {
    virtual void *_v00(); virtual void *_v04(); virtual void *_v08(); virtual void *_v0c();
    virtual void *_v10(); virtual void *_v14(); virtual void *_v18(); virtual void *_v1c();
    virtual void *_v20(); virtual void *_v24(); virtual void *_v28(); virtual void *_v2c();
    virtual void *_v30(); virtual void *_v34(); virtual void *_v38(); virtual void *_v3c();
    virtual int FindDecorObjIndexMaybe(AnimDescRefObj0x477488 *pDecor, int nLo, int nHi); // vtbl+0x40
};

// Methods-only probe for the one UIResources entry point this TU calls: 0x447400, the
// station-clock chime tick (computes the 5-minute clock position from the game tick and
// plays the quarter-hour sounds). Kept TU-local so the widely-included UIResources.h
// doesn't rotate its consumer TUs (v333/v334/v336 position-sensitivity lessons).
// RETIRED v577 -- see src/UIResources.cpp. This probe also spelled the second parameter list
// differently (unsigned int nTick vs the real int nSeconds), so it was a lint_desync-class
// disagreement on top of the unlinked-call defect.

// TU-local view of SelectedObjWidgetMaybe_004852a0 for the one byte this TU reads: the
// base-class bActive at +0x88. Predates the header's real-member modeling (v503/v504) and
// kept because folding it onto the class would rotate this knife-edged header family for
// zero byte gain.
struct SelectedObjWidgetActiveView0x459180 {
    unsigned char Unk0x00[0x88];
    bool bActive; // +0x88
};

// Padded-vtable probe to reach AnimDescRefObj0x477488's slot 7 (+0x1c) through a VIRTUAL
// dispatch (`mov eax,[ecx]; call [eax+0x1c]` at the 0x3868/0x3869 call sites). The slot's
// real body is ReleaseChannelAndDispatch (0x405a20, src/WidgetBase.cpp), but WidgetBase.h
// declares that as an ordinary member -- its other call sites use qualified dispatch --
// and slot 7 itself as a bare _v07(). Methods-only probe, no data layout duplicated
// (CarNetObjVtblProbe precedent, src/CarNetObj.h).
struct AnimDescRefSlot7Probe {
    virtual void *_v00(); virtual void *_v04(); virtual void *_v08(); virtual void *_v0c();
    virtual void *_v10(); virtual void *_v14(); virtual void *_v18();
    virtual void ReleaseChannelAndDispatch(unsigned int nFrame); // vtbl+0x1c
};

// Methods-only probe for the three PeerTrainNode entry points this TU calls -- NOT added to
// src/PeerTrainNode.h because new member declarations on the shared PeerTrainNodePartial flip
// DPlaySessionMgr.cpp's SelectGridCellFromPointMaybe EXACT->DIFF(130) and shift
// LayoutNet_SendCurrentLayoutBitmap's DIFF via that TU's documented position sensitivity
// (field additions inside existing padding are safe; member declarations are not -- same
// methods-only-view workaround as DPlaySessionMgr.cpp's PeerTrainSlotQueueEdgePartial).
struct PeerTrainNodeMethodsProbe {
    void ClaimDecorObjMaybe(void *pDecorObj, char bFlag);   // 0x44c170
    void TryTransitionModeMaybe(int nMode, char bFlag);     // 0x44d5e0
    void SetSoundStateIfChangedMaybe(int nState);           // 0x44d720
};


// FUNCTION: LOCO 0x45a880
// Test half of the slot 17/20 menu-command pair (execute half below). Answers "may this
// command run / what node-state transition does it arm": gates on bVisible + the Contains
// hit-test, then dispatches the same command-id ranges as the execute half. Most paths set
// the node's state to 2 (armed) and load a wSelIndexMaybe countdown (1/3/6); the 0x2c07/08
// candidate-cycle commands return 0 (XOR AL,AL) after arming with countdown 3.
//
// PARKED -- EFFECTIVE MATCH (asmscore byte_diff 83, insns 151/149, align 142, reg_pen 10).
// Structure is faithful: every case/compare/call/store matches, including the switch's
// split dispatch (cmp-chain for 0x2c07-0x3803, sub/dec chain for 0x380e-0x3810), the
// default-in-the-middle layout, and the shared setState2/setSel1 tails. The residual is
// /Og GLOBAL-ALLOCATION churn, not source structure: (a) the original keeps constant 1 in
// ebx across each whole case body (`mov ebx,1` at case top, `cmp [esi+0x48],bx`,
// `mov [esi+0x54],bx`, `mov al,bl` forms) while our compile re-materializes it per
// sub-block; (b) pCand lives in eax in the original, ecx in ours, with the train loads
// swirled to match; (c) our /O2 cross-jumps 0x380f's identical SetNodeState(2)/wSel=1/ret
// tail into the shared setState2 block, the original keeps a full copy. Our cl 11.00 DOES
// produce the ebx=1 form elsewhere in this same function, just with different region
// boundaries -- the region/global-register outcome is TU-context-sensitive (proven: two
// dummy functions prepended to this TU measurably shift this file's codegen; Yoda lesson
// #23's class). The original TU held ~10 earlier WorldActionCursor methods, unreproducible
// until those are transcribed. NOT source-fixable after probes: nOne source local
// (constant-folded away), if/else-if vs switch dispatch, label/CFG restructuring.
char WorldActionCursor::HitTestNodeSecondary(MenuNodeObj0x477568 *pNode, int x, int y)
{
    PeerTrainNodePartial *pTrain;
    unsigned short wState;

    if (pNode == 0 || !pNode->bVisible) {
        return 0;
    }
    if (!pNode->Contains(x, y)) {
        return 0;
    }
    int nCmd = pNode->pIconDesc->resourceId;
    switch (nCmd) {
    case 0x2c09: {
        UiIconListItem *pCand = this->pActiveCandidateNodeMaybe;
        if (pNode == pCand) {
            if (this->nModeMaybe != 6) {
                pCand->bTextRedrawEnabled = true;
                return 1;
            }
            pTrain = (PeerTrainNodePartial *)this->pSelectedDecorObjMaybe[8].rect.top;
            if (pTrain != 0 && pTrain->nDiscardFlag == 0) {
                pCand->bTextRedrawEnabled = true;
                return 1;
            }
            return 1;
        }
        if (pNode->wState == 1) {
            pNode->SetNodeState(2);
            pNode->wSelIndexMaybe = 1;
            return 1;
        }
        return 1;
    }
    case 0x2c07:
    case 0x2c08:
        if (pNode->wState != 1) {
            return 0;
        }
        pNode->SetNodeState(2);
        pNode->wSelIndexMaybe = 3;
        return 0;
    case 0x3803:
        wState = pNode->wState;
        if (wState == 1) {
            goto setState2;
        }
        if (wState != 2) {
            goto setSel1;
        }
        pNode->SetNodeState(1);
        pNode->wSelIndexMaybe = 1;
        return 1;
    default:
        if (pNode->wState == 1) {
            pNode->SetNodeState(2);
            pNode->wSelIndexMaybe = 6;
            return 1;
        }
        return 1;
    case 0x380f:
        if (pNode->wState == 1) {
            pNode->SetNodeState(2);
            pNode->wSelIndexMaybe = 1;
            return 1;
        }
        goto setSel1;
    case 0x380e:
    case 0x3810:
        wState = pNode->wState;
        if (wState == 1) {
            goto setState2;
        }
        if (wState == 2) {
            pNode->SetNodeState(3);
            pNode->wSelIndexMaybe = 1;
            return 1;
        }
        if (wState != 3) {
            goto setSel1;
        }
        goto setState2;
    }
setState2:
    pNode->SetNodeState(2);
setSel1:
    pNode->wSelIndexMaybe = 1;
    return 1;
}

// FUNCTION: LOCO 0x45aa50
// Execute half of the slot 17/20 menu-command pair (see the header's method comment). One
// big switch on the node icon's command id: the sparse low cluster (0x2c07..0x3802) compiles
// to a subtract/compare chain, the dense 0x3803..0x3869 run to a byte-map+dword-table jump
// table. Case declaration order below is recovered from the originals' .text layout (CLAUDE.md's
// jump-table source-order lesson): chain bodies 0x2c09/0x2c08/0x2c07/0x3802, then table bodies
// 0x3803/0x3865/0x3864/0x3867/0x3866/0x3868/0x3869/0x380e/0x380f/0x3810; the 0x2c09 inner
// mode-switch bodies in order 5, 0/2, 4.
//
// PARKED -- EFFECTIVE MATCH (asmscore byte_diff 766, insns 750/754, align 1576, reg_pen 68).
// Structure is faithful end-to-end: both jump tables (byte-map+dword for 0x3803..0x3869,
// 6-entry for the nModeMaybe-2 inner switch) reproduce with the exact case->body mapping,
// the chain dispatch, every call target/arg order, both shared goto tails (selectGlobal/
// attachPending/refreshMenu), and the 0x380e/0x3810 cross-case shared labels (selOtherCar,
// trans/upd tail-merge) all match the original's CFG. The residual is ONE systemic /Og
// global-allocation divergence with local cross-jump fallout, not source structure:
// (a) the original loads constant 1 into ebx once at dispatch top and uses bl/bx forms for
// all ~45 literal-1 pushes/stores/compares/returns (freeing ebp only for the case-4 loop
// index, 0x3865's wTrainId, and 0x3866/7's panSel); our compile never hoists it, keeps
// immediates, and leaves ebp unused entirely -- every downstream register role shifts;
// (b) our /O2 cross-jumps the identical ComputeLocalPos+Contains+SetNodeState tails of
// cases 0x2c07/0x2c08 into one shared block, the original keeps two full copies (its
// copies' register swirl, itself downstream of (a), blocks the merge); (c) block PLACEMENT
// inside the 0x380e/0x3810 region interleaves differently (our 0x3810 !=2 body lands
// before 0x380e's). Probes that did NOT move (a): an `int nOne = 1` local used at all ~30
// sites (constant-folded away), removing the TickAdvanceFrame virtual (vptr-in-ebx is not
// the blocker), +8 artificial extra 1-uses, five mini-replica TUs of increasing fidelity
// (none hoist). Probes that DID land real structure: while-form loops (do-while first-
// iteration peel, CLAUDE.md's peel lesson), function-scope aiStack pair (0x10 locals),
// ComputeLocalPos result consumed through its returned pointer, `if (*pIn != 0)` scratch-
// loop polarity + `&anTrainSelScratchMaybe[1]` (not panSel+1), direct arr[i].field
// subscripting in 0x3868/9 (subscript-vs-hoisted-pointer lesson), virtual dispatch for the
// decor-category manager via a local pointer (devirtualization of &global), the 380e/3810
// else-before-==1 statement order. The residual class is TU-position-sensitive /Og global
// state (proven: prepending two dummy functions to this TU shifts this function's codegen;
// Yoda lesson #23) -- the original TU held ~10 earlier WorldActionCursor methods. Retry
// when those siblings (esp. the ctor and RefreshTrainCouplingMenuMaybe/0x4597e0) are
// transcribed into this TU ahead of it.
char WorldActionCursor::HandleMenuCommandMaybe(MenuNodeObj0x477568 *pNode)
{
    AnimDescRefObj0x477488 *pSel;
    PeerTrainNodePartial *pTrain;
    int *panSel;
    unsigned int *pIn; // AnimDescRefObj0x477488::nAnimValueCache is unsigned (src/WidgetBase.h)
    int *pOut;
    int n;
    int nState;
    int nReversed;
    unsigned short wId;

    if (pNode == 0) {
        return 0;
    }
    if (pNode->wSelIndexMaybe >= 0) {
        pNode->wSelIndexMaybe--;
    }
    pNode->TickAdvanceFrame();
    if (pNode->wSelIndexMaybe != 0) {
        return 0;
    }
    switch (pNode->pIconDesc->resourceId) {
    case 0x2c09:
        if (pNode->wState == 2) {
            pNode->SetNodeState(1);
            switch (this->nModeMaybe - 2) {
            case 5: {
                unsigned char i = 0;
                while (i < 8) {
                    if (this->pDecorMenuIconsMaybe[i] == pNode) {
                        // Called through a local pointer: a direct &global call gets
                        // devirtualized by VC5 (exact dynamic type), the original's
                        // `call [edx+0x20]` needs the indirect form.
                        DecorCategoryMgrVtblProbe *pMgr = &DecorCategoryMgrMaybe_00485494;
                        SelectedObjWidgetMaybe_004852a0.SelectObjMaybe(
                            pMgr->GetCategoryObjByIndexMaybe(this->nCandidateBaseMaybe + i));
                        return 1;
                    }
                    i++;
                }
                return 1;
            }
            case 0:
            case 2: {
                unsigned char i = 0;
                while (i < 8) {
                    if (this->pDecorMenuIconsMaybe[i] == pNode) {
                        // +0x90 into the selected decor object = [1].rect.left onward.
                        SelectedObjWidgetMaybe_004852a0.SelectObjMaybe(
                            ((int *)&this->pSelectedDecorObjMaybe[1].rect)[i]);
                        return 1;
                    }
                    i++;
                }
                return 1;
            }
            case 4: {
                pTrain = (PeerTrainNodePartial *)this->pSelectedDecorObjMaybe[8].rect.top;
                unsigned char i = 0;
                while (i < 8) {
                    if (this->pDecorMenuIconsMaybe[i] == pNode) {
                        SelectedObjWidgetMaybe_004852a0.SelectObjMaybe(pTrain->apPassengerMaybe[i]);
                        return 1;
                    }
                    i++;
                }
                return 1;
            }
            }
        }
        break;
    case 0x2c08:
        if (pNode->wState == 2) {
            pNode->SetNodeState(1);
            if (this->nModeMaybe == 7) {
                this->RefreshDecorCategoryCandidatesMaybe(this->nCandidateBaseMaybe + 1);
                if (pNode->wState == 1) {
                    POINT ptLocalA = this->ComputeLocalPos(
                        PlacementCursorMaybe_004854c8.lastResolvedPosX,
                        PlacementCursorMaybe_004854c8.lastResolvedPosY);
                    if (pNode->Contains(ptLocalA.x, ptLocalA.y) != 0 &&
                        PlacementCursorMaybe_004854c8.bFlagE6Maybe) {
                        pNode->SetNodeState(2);
                        pNode->wSelIndexMaybe = 6;
                        return 1;
                    }
                }
            }
        }
        break;
    case 0x2c07:
        if (pNode->wState == 2) {
            pNode->SetNodeState(1);
            if (this->nModeMaybe == 7) {
                this->RefreshDecorCategoryCandidatesMaybe(this->nCandidateBaseMaybe - 1);
                if (pNode->wState == 1) {
                    POINT ptLocalB = this->ComputeLocalPos(
                        PlacementCursorMaybe_004854c8.lastResolvedPosX,
                        PlacementCursorMaybe_004854c8.lastResolvedPosY);
                    if (pNode->Contains(ptLocalB.x, ptLocalB.y) != 0 &&
                        PlacementCursorMaybe_004854c8.bFlagE6Maybe) {
                        pNode->SetNodeState(2);
                        pNode->wSelIndexMaybe = 6;
                        return 1;
                    }
                }
            }
        }
        break;
    case 0x3802:
        if (pNode->wState == 2) {
            MenuNodeObj0x477568 *pLast;
            pNode->SetNodeState(1);
            pLast = this->pLastActivatedNode;
            if (pLast != 0 && pLast->wState == 2) {
                pLast->SetNodeState(1);
            }
            if (this->animMaybe6.bReady) {
                this->SelectDecorObjAndDispatchModeMaybe(this->pSelectedDecorObjMaybe);
                return 1;
            }
            this->SelectDecorObjAndDispatchModeMaybe(0);
            return 1;
        }
        break;
    case 0x3803:
        if (pNode->wState == 1) {
            this->bAttachMenuToggleMaybe = false;
            this->RefreshTrainCouplingMenuMaybe();
            return 1;
        }
        if (pNode->wState == 2) {
            this->bAttachMenuToggleMaybe = true;
            this->RefreshTrainCouplingMenuMaybe();
            return 1;
        }
        goto refreshMenu;
    case 0x3865:
        pNode->SetNodeState(1);
        pSel = this->pSelectedDecorObjMaybe;
        if (pSel == 0) {
            goto refreshMenu;
        }
        pTrain = (PeerTrainNodePartial *)pSel[2].rect.right;
        if (pTrain == 0) {
            goto refreshMenu;
        }
        {
            char bOwner = pTrain->bOwnerByteA;
            unsigned short wTrainId = pTrain->wTrainId;
            g_PeerTrainSlotQueue.FreeQueuedTrainCarSlots(pTrain);
            g_PeerTrainSlotQueue.DetachFromBoardMaybe((PeerTrainNodePartial *)pSel[2].rect.right);
            g_PeerTrainSlotQueue.ReleaseOrForwardMatchingSlotMaybe(wTrainId, bOwner, '\x01');
        }
        pSel[2].bValid = true;
        this->RefreshTrainCouplingMenuMaybe();
        return 1;
    case 0x3864:
        pNode->SetNodeState(1);
        pSel = this->pSelectedDecorObjMaybe;
        if (pSel != 0) {
            pTrain = (PeerTrainNodePartial *)pSel[2].rect.right;
            if (pTrain != 0) {
                ((PeerTrainNodeMethodsProbe *)pTrain)->ClaimDecorObjMaybe(pSel, '\x01');
                if (pTrain->dwReversed == 0) {
                    SelectedObjWidgetMaybe_004852a0.SelectObjMaybe((int)pTrain->carSlots[0]);
                    return 1;
                }
                SelectedObjWidgetMaybe_004852a0.SelectObjMaybe(
                    (int)pTrain->carSlots[pTrain->wCarSlotCount - 1]);
                return 1;
            }
        }
        break;
    case 0x3867:
        pNode->SetNodeState(1);
        if (this->animMaybe6.bReady) {
            panSel = this->anTrainSelScratchMaybe;
            pIn = &this->animArrayMaybe[1].nAnimValueCache;
            n = 3;
            panSel[0] = this->animArrayMaybe[0].nAnimValueCache * 2 + 0x1804;
            pOut = &this->anTrainSelScratchMaybe[1];
            do {
                if (*pIn != 0) {
                    *pOut = *pIn * 2 + 0x1864;
                } else {
                    *pOut = 0;
                }
                if (*pOut == 0x1871) {
                    *pOut = 0x1870;
                }
                pOut++;
                pIn += 0x22;
                n--;
            } while (n != 0);
            pSel = this->pSelectedDecorObjMaybe;
            if (pSel != 0 && pSel[2].rect.right == 0) {
                g_PeerTrainSlotQueue.SpawnOrAssignRandomTrain(pSel, panSel);
                g_worldActionCursor.SelectDecorObjAndDispatchModeMaybe(0);
                return 1;
            }
            goto selectGlobal;
        }
        goto attachPending;
    case 0x3866:
        pNode->SetNodeState(1);
        if (this->animMaybe6.bReady) {
            panSel = this->anTrainSelScratchMaybe;
            pIn = &this->animArrayMaybe[1].nAnimValueCache;
            n = 3;
            panSel[0] = this->animArrayMaybe[0].nAnimValueCache * 2 + 0x1804;
            pOut = &this->anTrainSelScratchMaybe[1];
            do {
                if (*pIn != 0) {
                    *pOut = *pIn * 2 + 0x1864;
                } else {
                    *pOut = 0;
                }
                if (*pOut == 0x1871) {
                    *pOut = 0x1870;
                }
                pOut++;
                pIn += 0x22;
                n--;
            } while (n != 0);
            pSel = this->pSelectedDecorObjMaybe;
            if (pSel != 0 && pSel[2].rect.right != 0) {
                g_PeerTrainSlotQueue.RebuildCarSlotsFromSelectionMaybe((TrackDepotTileObj *)pSel, panSel);
            }
        selectGlobal:
            g_worldActionCursor.SelectDecorObjAndDispatchModeMaybe(0);
            return 1;
        }
        goto attachPending;
    attachPending:
        this->bAttachPendingMaybe = true;
    refreshMenu:
        this->RefreshTrainCouplingMenuMaybe();
        return 1;
    case 0x3868:
        pNode->SetNodeState(1);
        {
            unsigned char i = 0;
            while (i < 4) {
                if (this->pCandidateVariantPrevBtnMaybe[i] == pNode) {
                    AnimDescRefObj0x477488 *pAnim = &this->animArrayMaybe[i];
                    if (pAnim->nSubFrame > 0) {
                        ((AnimDescRefSlot7Probe *)pAnim)->ReleaseChannelAndDispatch(
                            pAnim->nSubFrame - 1);
                        return 1;
                    }
                    ((AnimDescRefSlot7Probe *)pAnim)->ReleaseChannelAndDispatch(
                        pAnim->pKindDesc->nFrameSetCount - 1);
                    return 1;
                }
                i++;
            }
        }
        return 1;
    case 0x3869:
        pNode->SetNodeState(1);
        {
            unsigned char i = 0;
            while (i < 4) {
                if (this->pCandidateVariantNextBtnMaybe[i] == pNode) {
                    AnimDescRefObj0x477488 *pAnim = &this->animArrayMaybe[i];
                    if (pAnim->nSubFrame < pAnim->pKindDesc->nFrameSetCount - 1) {
                        ((AnimDescRefSlot7Probe *)pAnim)->ReleaseChannelAndDispatch(
                            pAnim->nSubFrame + 1);
                        return 1;
                    }
                    ((AnimDescRefSlot7Probe *)pAnim)->ReleaseChannelAndDispatch(0);
                    return 1;
                }
                i++;
            }
        }
        return 1;
    case 0x380e:
        pTrain = (PeerTrainNodePartial *)this->pSelectedDecorObjMaybe[8].rect.top;
        if (pTrain == 0) {
            break;
        }
        nState = pTrain->dwSoundStateMaybe;
        nReversed = pTrain->dwReversed;
        wId = pTrain->wSelectedCarId;
        if (nState == 2) {
            if (nReversed != 1) {
                goto selOtherCar;
            }
            if (wId == pTrain->wSelectedCarIdAMaybe) {
                pTrain->PeerTrainNode_UpdateSelectedCar(pTrain->wSelectedCarIdBMaybe);
                return 1;
            }
            break;
        }
        if (nState == 1) {
            if (nReversed == 1) {
                break;
            }
            ((PeerTrainNodeMethodsProbe *)pTrain)->SetSoundStateIfChangedMaybe(0);
            ((PeerTrainNodeMethodsProbe *)pTrain)->TryTransitionModeMaybe(1, '\0');
            pTrain->PeerTrainNode_UpdateSelectedCar(pTrain->wSelectedCarIdAMaybe);
            ((PeerTrainNodeMethodsProbe *)pTrain)->SetSoundStateIfChangedMaybe(2);
            break;
        }
        ((PeerTrainNodeMethodsProbe *)pTrain)->TryTransitionModeMaybe(1, '\0');
        pTrain->PeerTrainNode_UpdateSelectedCar(pTrain->wSelectedCarIdAMaybe);
        ((PeerTrainNodeMethodsProbe *)pTrain)->SetSoundStateIfChangedMaybe(2);
        break;
    case 0x380f:
        pTrain = (PeerTrainNodePartial *)this->pSelectedDecorObjMaybe[8].rect.top;
        if (pTrain != 0 &&
            (pTrain->dwSoundStateMaybe == 2 || pTrain->dwSoundStateMaybe == 1)) {
            ((PeerTrainNodeMethodsProbe *)pTrain)->SetSoundStateIfChangedMaybe(0);
            return 1;
        }
        break;
    case 0x3810:
        pTrain = (PeerTrainNodePartial *)this->pSelectedDecorObjMaybe[8].rect.top;
        if (pTrain == 0) {
            break;
        }
        nState = pTrain->dwSoundStateMaybe;
        nReversed = pTrain->dwReversed;
        wId = pTrain->wSelectedCarId;
        if (nState == 2) {
            if (nReversed != 0) {
                goto selOtherCar;
            }
            if (wId == pTrain->wSelectedCarIdAMaybe) {
                pTrain->PeerTrainNode_UpdateSelectedCar(pTrain->wSelectedCarIdBMaybe);
                return 1;
            }
            break;
        }
        goto checkState1;
    selOtherCar:
        if (wId == pTrain->wSelectedCarIdBMaybe) {
            pTrain->PeerTrainNode_UpdateSelectedCar(pTrain->wSelectedCarIdAMaybe);
            return 1;
        }
        ((PeerTrainNodeMethodsProbe *)pTrain)->SetSoundStateIfChangedMaybe(0);
        return 1;
    checkState1:
        if (nState == 1) {
            if (nReversed == 0) {
                break;
            }
            ((PeerTrainNodeMethodsProbe *)pTrain)->SetSoundStateIfChangedMaybe(0);
            ((PeerTrainNodeMethodsProbe *)pTrain)->TryTransitionModeMaybe(0, '\0');
            pTrain->PeerTrainNode_UpdateSelectedCar(pTrain->wSelectedCarIdAMaybe);
            ((PeerTrainNodeMethodsProbe *)pTrain)->SetSoundStateIfChangedMaybe(2);
            break;
        }
        ((PeerTrainNodeMethodsProbe *)pTrain)->TryTransitionModeMaybe(0, '\0');
        pTrain->PeerTrainNode_UpdateSelectedCar(pTrain->wSelectedCarIdAMaybe);
        ((PeerTrainNodeMethodsProbe *)pTrain)->SetSoundStateIfChangedMaybe(2);
        break;
    }
    return 1;
}

// FUNCTION: LOCO 0x459180
// Select/deselect entry point (header comment has the semantics). Layout notes recovered
// from the original's .text: the select path falls through first (the deselect block is
// out-of-line at 0x45965b, so the source is `if (select-conditions) {...} else {...}` with
// two separate `return this->bActive;` tails); the mode switch's case declaration order
// follows the original's body order (7, 8, 2/4, 6, 0xc, 3 -- CLAUDE.md's jump-table
// source-order lesson); the mode-6/mode-0xc cases share ONE center-anim block (0x459536,
// reached by fallthrough from 0xc and a goto from 6).
//
// PARKED -- EFFECTIVE MATCH (asmscore --len 1388: byte_diff 371, insns 418/405, align 468,
// reg_pen 173). Structure is faithful end-to-end: the mode-3 prologue fixup, the 4-deep
// &&-guard, the whole select-path statement order, the jump table (0x4596ec, 11 entries,
// modes 2..0xc) with the exact case->body mapping, the mode-8 do-while over the 8 decor
// icons, all 5 inlined center-anim blocks, the shared 6/0xc block, the neg/sbb/and bActive
// ternary in the deselect path, and both MarkDirty/return tails all match the original's
// CFG and instruction selection. The residual is THREE stacked documented /Og coin-flip
// classes, not source structure: (1) the sete-materialization class (0x4393d0, shared with
// the v334/v335 parks -- ⭐ FIXED in v356 by the byte-predicate lever, total 513668 -> 479365):
// the original materializes (g_nScreenState == 3) via
// `mov edx,[mem]; xor eax,eax; cmp edx,ebp; sete al; test al,al` mid-&&-chain, ours folds
// to `cmp [mem],ebp; jne` -- a `bool bInGame = ...` probe DID materialize the sete but
// hoisted it ahead of the two null guards (short-circuit order lost), reverted;
// (2) an /Og vtable-value-CSE coin-flip on animMaybe5: per case our compile hoists
// `mov ebp,[esi+0x3a0]` once and calls [ebp+0x18]/[ebp+0xc], the original never caches the
// vtable -- it keeps only the ADDRESS in edi and reloads ([esi+0x3a0] for SetDescriptor,
// [edi] after CenterRectInRect); this cascades into the `mov ecx,edi` / `add esp,8` /
// arg-sum scheduling of all 5 center-anim tails (~half the dump rows) and the case-3
// bAttachMenuToggleMaybe store order; (3) 16-bit partial-reg coin flips (cx/ax vs the
// original's dx forms on the mode-word moves, cl vs al on the bActive ternary). Levers
// that DID land (kept): unsigned `> 0` on the category-count guard (jbe, not je); clamp
// written `base > nIndex` (cmp ecx,eax/jle, not the decompile's `nIndex < base`); if/else
// (NOT ternary) for the categoryByte mode byte -- produces the original's
// `xor r16,r16; mov r16l,al` zero-extend; the inline helper CenterModeAnimOverWidgetMaybe
// for the 5 shared center-anim blocks (byte-identical output to repeated blocks; kept as
// the plausible original shape); TU-local probes/views for the category-mgr vtbl+0x40 slot,
// the UIResources clock tick (0x447400), SelectedObjWidgetMaybe's bActive, and the
// 0x478538/0x4854a0 externs. A minimal-TU probe (this function + decls alone,
// parked siblings removed) reproduces the IDENTICAL score -- unlike the two parked
// siblings above, the residual is INTRINSIC to this function's own /Og compilation, not
// TU context (same finding as v336's 0x43e900). Retry only if the 0x4393d0
// sete-materialization class or the vtable-CSE coin-flip class cracks.
char WorldActionCursor::SelectDecorObjAndDispatchModeMaybe(AnimDescRefObj0x477488 *pDecor)
{
    char bOk;

    if (this->nModeMaybe == 3 && this->pSelectedDecorObjMaybe != 0 &&
        this->pSelectedDecorObjMaybe->bValid == true &&
        this->pSelectedDecorObjMaybe[2].rect.right == 0) {
        this->pSelectedDecorObjMaybe[2].rect.top = 0;
        this->pSelectedDecorObjMaybe[2].bValid = false;
        this->pSelectedDecorObjMaybe->ReleaseChannelAndDispatch(0);
    }
    if (pDecor != 0 && pDecor->pKindDesc != 0 && IsInGameModeMaybe() &&
        SelectedObjWidgetMaybe_CanSelectDecorObjMaybe(pDecor)) {
        this->bActive = true;
        this->nModePrevMaybe = this->nModeMaybe;
        {
            unsigned char bMode;
            if (pDecor->pKindDesc == 0) {
                bMode = 0;
            } else {
                bMode = pDecor->pKindDesc->categoryByte;
            }
            this->nModeMaybe = bMode;
        }
        g_pActiveTabWidgetMaybe = this;
        this->pSelectedDecorObjMaybe = pDecor;
        this->Unk0x8c = g_dwGameTick;
        this->bAttachPendingMaybe = false;
        if (this->nModePrevMaybe != this->nModeMaybe) {
            this->bAttachMenuToggleMaybe = false;
        }
        if (this->animMaybe5.pDSoundChannel != 0) {
            this->animMaybe5.pDSoundChannel->Release();
        }
        switch (this->nModeMaybe) {
        case 7: {
            int nIndex = -1;
            if (g_nDecorCategory7CountMaybe > 0) {
                nIndex = ((DecorCategoryMgrProbe0x459180 *)&DecorCategoryMgrMaybe_00485494)
                             ->FindDecorObjIndexMaybe(pDecor, 0,
                                                      g_nDecorCategory7CountMaybe - 1);
            }
            if (this->nCandidateBaseMaybe < nIndex - 6 ||
                this->nCandidateBaseMaybe > nIndex) {
                this->nCandidateBaseMaybe = nIndex;
            }
            this->RefreshDecorCategoryCandidatesMaybe(this->nCandidateBaseMaybe);
            bOk = this->animMaybe5.SetDescriptor(
                this->pSelectedDecorObjMaybe->pKindDesc->resourceId + 1,
                ((unsigned char)this->pSelectedDecorObjMaybe[1].nTypeTag) >> 1, 0);
            if (bOk == 1) {
                this->CenterModeAnimOverWidgetMaybe();
            }
            break;
        }
        case 8: {
            MenuNodeObj0x477568 **ppNode = this->pDecorMenuIconsMaybe;
            int n = 8;
            do {
                (*ppNode)->SetNodeState(3);
                ((UiIconListItem *)*ppNode)->SetLabelText("");
                (*ppNode)->Draw();
                ppNode++;
                n--;
            } while (n != 0);
            this->pCandidateUpMaybe->SetNodeState(3);
            this->pCandidateDownMaybe->SetNodeState(3);
            bOk = this->animMaybe5.SetDescriptor(
                this->pSelectedDecorObjMaybe->pKindDesc->resourceId + 1, -1, 0);
            if (bOk == 1) {
                this->CenterModeAnimOverWidgetMaybe();
            }
            break;
        }
        case 2:
        case 4:
            this->RefreshVariantMenuIconsMaybe();
            bOk = this->animMaybe5.SetDescriptor(
                this->pSelectedDecorObjMaybe->pKindDesc->resourceId + 1, -1, 0);
            if (bOk == 1) {
                this->CenterModeAnimOverWidgetMaybe();
            }
            g_UIResources.TickStationClockChimeMaybe(g_dwGameTick, 1);
            break;
        case 6:
            this->RefreshCategoryMenuIconsMaybe();
            bOk = this->animMaybe5.SetDescriptor(
                this->pSelectedDecorObjMaybe->pKindDesc->resourceId + 1, -1, 0);
            if (bOk != 1) {
                break;
            }
            if (this->animMaybe5.pDSoundChannel != 0) {
                this->animMaybe5.pDSoundChannel->Release();
            }
            goto centerAnim;
        case 0xc:
            this->RefreshVariantMenuIconsMaybe();
            bOk = this->animMaybe5.SetDescriptor(
                this->pSelectedDecorObjMaybe->pKindDesc->resourceId, -1, 0);
            if (bOk != 1) {
                break;
            }
        centerAnim:
            this->CenterModeAnimOverWidgetMaybe();
            break;
        case 3:
            this->bAttachMenuToggleMaybe = true;
            bOk = this->animMaybe5.SetDescriptor(
                this->pSelectedDecorObjMaybe->pKindDesc->resourceId + 1, -1, 0);
            if (bOk == 1) {
                this->CenterModeAnimOverWidgetMaybe();
            }
            this->pSelectedDecorObjMaybe[2].rect.top = 1;
            this->pSelectedDecorObjMaybe[2].bValid = true;
            this->pSelectedDecorObjMaybe->ReleaseChannelAndDispatch(1);
            break;
        }
        this->RefreshTrainCouplingMenuMaybe();
        this->ClampRectIntoViewMaybe();
        this->MarkDirty();
        return this->bActive;
    }
    this->MarkDirty();
    bOk = this->bDraggingMaybe;
    this->bActive = false;
    this->nModeMaybe = 0;
    if (bOk != 0) {
        this->bDraggingMaybe = 0;
    }
    g_pActiveTabWidgetMaybe =
        ((SelectedObjWidgetActiveView0x459180 *)&SelectedObjWidgetMaybe_004852a0)->bActive
            ? (void *)&SelectedObjWidgetMaybe_004852a0
            : 0;
    if (this->animMaybe5.pDSoundChannel != 0) {
        this->animMaybe5.pDSoundChannel->Release();
    }
    this->animMaybe5.SetDescriptor(0, -1, 0);
    g_worldBoard.MarkRectDirty(this->rect);
    return this->bActive;
}

// FUNCTION: LOCO 0x4597e0
// RefreshTrainCouplingMenuMaybe -- refreshes the train-coupling submenu from the current
// mode/attach state: swaps the widget's own descriptor icon (0x3800/0x3801) on the
// attach-menu toggle, retitles the active-candidate item from the selected decor object
// (or the selected train's locomotive in mode 6), sets the mode-6 visibility group, drives
// the attach/spawn item's state (2 = attach-toggle on without pending, 3 = hidden, 1 =
// normal), the candidate page buttons + 8 decor icons (visible only while the attach menu
// is toggled on outside mode 3), the mutually-exclusive couple-choice buttons (new-train vs
// attach-existing at the same on-screen slot, repositioned per branch), the 4 variant
// prev/next button pairs, and the per-car anim subframes (slots 1-3 follow the attached
// PeerTrainNode's carSlots; each car's descriptor resourceId picks the frame). Ends by
// re-anchoring the widget rect to its own origin (slot 3).
//
// PARKED -- EFFECTIVE MATCH (asmscore --len 1375: byte_diff 165, insns 390/394, align 192,
// reg_pen 58). Structure is faithful end-to-end: the toggle-dispatched SetDescriptor pair,
// the mode6/mode3 bool pair, the SetLabelText source select (locomotive vs decor object),
// the attach/spawn state machine (3/2 vs 3/1 branches), the candidate-visibility trio, the
// 8-icon do-while, both couple-choice branches incl. the shared reposition coords and the
// IsSlotCountOutOfRangeMaybe/dwModeBMaybe state demotions, the shared 4-Draw tail, the
// variant prev/next do-while, the bReady walk (0x88 stride), the mode-3 per-car subframe
// loop (carSlots[0..3], -1 sentinel on null pKindDesc, (id - 0x1804)/2 and
// (id - 0x1866)/2 + 1), and the RepositionWithHotspot tail all match the original's CFG
// and instruction selection. The residual is FOUR documented /Og coin-flip classes, not
// source structure: (1) cross-jump/tail-merge -- the original keeps TWO full SetDescriptor
// call sequences (separate vtable reloads, pushes of 0x3800/0x3801) and sinks the
// SetLabelText arg's add+push into each select branch; our cl merges both (probe: the
// ptr-select and char*-select source forms compile identically; the per-branch-call probe
// on the attach/spawn SetNodeState site DID fire the original's cross-jump form and was
// kept -- the single-call sites here won't); (2) EAX-wide vs AL-wide bool materialization
// (4 sites: bReady5 + the three bCandVis/bVis computations) -- original mov eax,1/xor
// eax,eax, ours mov al,1/xor al,al; if/else vs &&-expression source forms compile
// identically -> intrinsic; (3) the 0x4393d0 sete-materialization class variant -- original
// sete direct to [esp+0x13] for bMode3, ours sete cl + byte store; (4) the v337
// vtable-value-CSE class at the RepositionWithHotspot tail -- original reloads mov
// edx,[esi], ours reuses the prologue's spilled vtable. Plus the usual reg coin-flips
// (edi/ecx pTrain residency, ebp/edi constant-3, variant-loop eax/cl/edx swaps, the
// original's re-fetch of pTrain from [esi+0x538]+0x120 for the dwModeBMaybe test, mov
// ecx,4 loop-init scheduling). Levers that landed (kept): per-branch SetNodeState calls
// (cross-jump fires, reproducing the original's push-per-branch/shared-call shape);
// car-subframe loop initialized pAnim = &animArrayMaybe[1] with the non-null path first
// (matches the original's fallthrough layout exactly -- the whole 0x459cd3..0x459d26
// region is byte-faithful); &&-expression bool temps (same output as if/else, matches
// Ghidra's bVar3 shape). Retry only if the 0x4393d0 sete class or the v337
// vtable-value-CSE class cracks.
void WorldActionCursor::RefreshTrainCouplingMenuMaybe()
{
    if (this->bAttachMenuToggleMaybe) {
        this->SetDescriptor(0x3800, 0, 1);
    } else {
        this->SetDescriptor(0x3801, 0, 1);
    }
    bool bMode6 = this->nModeMaybe == 6;
    bool bMode3 = this->nModeMaybe == 3;
    char *pszLabel;
    if (bMode6 && this->pSelectedDecorObjMaybe != 0) {
        pszLabel = ((AnimDescRefObj0x477488 *)((PeerTrainNodePartial *)
                        this->pSelectedDecorObjMaybe[8].rect.top)->carSlots[0])
                       ->szCategoryName;
    } else {
        pszLabel = this->pSelectedDecorObjMaybe->szCategoryName;
    }
    this->pActiveCandidateNodeMaybe->SetLabelText(pszLabel);
    this->pActiveCandidateNodeMaybe->bTextRedrawEnabled = true;
    this->pActiveCandidateNodeMaybe->bVisible = !this->bAttachPendingMaybe;
    this->pActiveCandidateNodeMaybe->Draw();
    bool bReady5 = !this->bAttachPendingMaybe && !bMode6;
    this->animMaybe5.bReady = bReady5;
    this->animMaybe7.bReady = bMode6;
    this->pIconStateTargetAMaybe->bVisible = bMode6;
    this->pIconStateTargetAMaybe->Draw();
    this->pIconStateTargetBMaybe->bVisible = bMode6;
    this->pIconStateTargetBMaybe->Draw();
    this->pIconStateTargetCMaybe->bVisible = bMode6;
    this->pIconStateTargetCMaybe->Draw();
    this->pDetachMenuItemMaybe->bVisible = true;
    this->pDetachMenuItemMaybe->Draw();
    this->pAttachOrSpawnMenuItemMaybe->bVisible = true;
    if (this->bAttachMenuToggleMaybe) {
        if (this->bAttachPendingMaybe) {
            this->pAttachOrSpawnMenuItemMaybe->SetNodeState(3);
        } else {
            this->pAttachOrSpawnMenuItemMaybe->SetNodeState(2);
        }
    } else if (this->nModeMaybe == 8 ||
               ((this->nModeMaybe == 0xc || this->nModeMaybe == 4 ||
                 this->nModeMaybe == 2) &&
                ((unsigned char *)&this->pSelectedDecorObjMaybe[1].nTypeTag)[1] == 0)) {
        this->pAttachOrSpawnMenuItemMaybe->SetNodeState(3);
    } else {
        this->pAttachOrSpawnMenuItemMaybe->SetNodeState(1);
    }
    this->pAttachOrSpawnMenuItemMaybe->Draw();
    bool bCandVis = this->bAttachMenuToggleMaybe && !bMode3;
    this->pCandidateUpMaybe->bVisible = bCandVis;
    this->pCandidateUpMaybe->Draw();
    {
        MenuNodeObj0x477568 **ppNode = this->pDecorMenuIconsMaybe;
        int n = 8;
        do {
            bool bVis = this->bAttachMenuToggleMaybe && !bMode3;
            (*ppNode)->bVisible = bVis;
            (*ppNode)->Draw();
            ppNode++;
            n--;
        } while (n != 0);
    }
    bCandVis = this->bAttachMenuToggleMaybe && !bMode3;
    this->pCandidateDownMaybe->bVisible = bCandVis;
    this->pCandidateDownMaybe->Draw();
    if (this->bAttachPendingMaybe != false) {
        this->pCoupleChoiceDetachMaybe->bVisible = false;
        this->pCoupleChoiceDetachMaybe->Draw();
        this->pCoupleChoiceNewTrainMaybe->bVisible =
            this->pSelectedDecorObjMaybe[2].rect.right == 0;
        this->pCoupleChoiceNewTrainMaybe->RepositionWithHotspot(0x8c, 0xb8);
        this->pCoupleChoiceNewTrainMaybe->Draw();
        this->pCoupleChoiceAttachExistingMaybe->bVisible =
            this->pSelectedDecorObjMaybe[2].rect.right != 0;
        this->pCoupleChoiceAttachExistingMaybe->RepositionWithHotspot(0x8c, 0xb8);
        this->pCoupleChoiceAttachExistingMaybe->Draw();
        this->pCoupleChoiceAMaybe->bVisible = false;
    } else {
        this->pCoupleChoiceDetachMaybe->bVisible = bMode3;
        this->pCoupleChoiceDetachMaybe->Draw();
        this->pCoupleChoiceNewTrainMaybe->bVisible = bMode3;
        this->pCoupleChoiceNewTrainMaybe->RepositionWithHotspot(0xa6, 0x33);
        this->pCoupleChoiceNewTrainMaybe->Draw();
        this->pCoupleChoiceAttachExistingMaybe->bVisible = bMode3;
        this->pCoupleChoiceAttachExistingMaybe->RepositionWithHotspot(0xec, 0x33);
        this->pCoupleChoiceAttachExistingMaybe->Draw();
        this->pCoupleChoiceAMaybe->bVisible = bMode3;
        this->pCoupleChoiceAMaybe->Draw();
        if (bMode3) {
            PeerTrainNodePartial *pTrain =
                (PeerTrainNodePartial *)this->pSelectedDecorObjMaybe[2].rect.right;
            if (pTrain == 0) {
                if (g_PeerTrainSlotQueue.IsSlotCountOutOfRangeMaybe() == 0) {
                    if (this->pCoupleChoiceNewTrainMaybe->wState == 3) {
                        this->pCoupleChoiceNewTrainMaybe->SetNodeState(1);
                    }
                } else {
                    this->pCoupleChoiceNewTrainMaybe->SetNodeState(3);
                }
                this->pCoupleChoiceDetachMaybe->SetNodeState(3);
                this->pCoupleChoiceAttachExistingMaybe->SetNodeState(3);
                this->pCoupleChoiceAMaybe->SetNodeState(3);
            } else {
                this->pCoupleChoiceNewTrainMaybe->SetNodeState(3);
                if (pTrain->dwModeBMaybe == 2) {
                    if (this->pCoupleChoiceDetachMaybe->wState == 3) {
                        this->pCoupleChoiceDetachMaybe->SetNodeState(1);
                    }
                    if (this->pCoupleChoiceAttachExistingMaybe->wState == 3) {
                        this->pCoupleChoiceAttachExistingMaybe->SetNodeState(1);
                    }
                    if (this->pCoupleChoiceAMaybe->wState == 3) {
                        this->pCoupleChoiceAMaybe->SetNodeState(1);
                    }
                } else {
                    this->pCoupleChoiceDetachMaybe->SetNodeState(3);
                    this->pCoupleChoiceAttachExistingMaybe->SetNodeState(3);
                    this->pCoupleChoiceAMaybe->SetNodeState(3);
                }
            }
            this->pCoupleChoiceNewTrainMaybe->Draw();
            this->pCoupleChoiceDetachMaybe->Draw();
            this->pCoupleChoiceAttachExistingMaybe->Draw();
            this->pCoupleChoiceAMaybe->Draw();
        }
    }
    {
        MenuNodeObj0x477568 **ppNext = this->pCandidateVariantNextBtnMaybe;
        int n = 4;
        do {
            ppNext[-4]->bVisible = this->bAttachPendingMaybe;
            ppNext[-4]->Draw();
            (*ppNext)->bVisible = this->bAttachPendingMaybe;
            (*ppNext)->Draw();
            ppNext++;
            n--;
        } while (n != 0);
    }
    this->animMaybe6.bReady = this->bAttachPendingMaybe;
    {
        bool *pbReady = &this->animArrayMaybe[0].bReady;
        int n = 4;
        do {
            *pbReady = this->bAttachPendingMaybe;
            pbReady += 0x88; // stride = sizeof(AnimDescRefObj0x477488); bool is 1 byte
            n--;
        } while (n != 0);
    }
    if (bMode3 && this->bAttachPendingMaybe != false) {
        PeerTrainNodePartial *pTrain =
            (PeerTrainNodePartial *)this->pSelectedDecorObjMaybe[2].rect.right;
        if (pTrain != 0) {
            AnimDescRefObj0x477488 *pLoco =
                (AnimDescRefObj0x477488 *)pTrain->carSlots[0];
            int nFrame = pLoco->pKindDesc == 0 ? -1 : pLoco->pKindDesc->resourceId;
            this->animArrayMaybe[0].ReleaseChannelAndDispatch((nFrame - 0x1804) / 2);
            AnimDescRefObj0x477488 *pAnim = &this->animArrayMaybe[1];
            TilePlacedObjPartial **ppCar = &pTrain->carSlots[1];
            int n = 3;
            do {
                AnimDescRefObj0x477488 *pCar = (AnimDescRefObj0x477488 *)*ppCar;
                if (pCar != 0) {
                    nFrame = pCar->pKindDesc == 0 ? -1 : pCar->pKindDesc->resourceId;
                    pAnim->ReleaseChannelAndDispatch((nFrame - 0x1866) / 2 + 1);
                } else {
                    pAnim->ReleaseChannelAndDispatch(0);
                }
                ppCar++;
                pAnim++;
                n--;
            } while (n != 0);
        }
    }
    this->RepositionWithHotspot(this->rect.left, this->rect.top);
}

// Every call site in this TU passes the same 0 mode-flags word, so the inherited
// WidgetBaseObj0x4784c8::GetOrCreateMenuIconItemMaybe (0x4546d0, src/WidgetBase.cpp) is wrapped
// once here to keep them short. Until 2026-07-26 this wrapper had to launder `this` through a
// TU-local probe struct, because 0x4546d0's body lived on a TU-local derived view in
// WidgetBase.cpp rather than on the real class; promoting it to a WidgetBase.h member was
// measured byte-neutral repo-wide, so the cast (and its idiom debt) is gone.
inline MenuNodeObj0x477568 *WorldActionCursor::GetOrCreateIconItemMaybe(CursorDesc *pDesc, int nTextLen) {
    return GetOrCreateMenuIconItemMaybe(pDesc, 0, nTextLen);
}

// FUNCTION: LOCO 0x458c90
// Icon-construction pass: builds every train-coupling-submenu icon (8 decor candidates,
// detach/attach-or-spawn, candidate up/down, 4 couple-choice buttons, 4 variant prev/next
// buttons, 3 icon-state targets), then returns whether the widget's own descriptor plus its
// 3 anim sub-icons (animMaybe7/animMaybe0/animMaybe6) and all 4 animArrayMaybe entries are
// ready. The final boolean chain's seed value is the RESULT of the very first SetDescriptor
// call at function entry (0x3801/0/1) -- ground-truthed against the raw disasm: that result
// is stored to a stack slot at function entry ([esp+0x17] there == [esp+0x13] at the tail,
// same physical slot, esp having shifted from intervening pushes) and re-read as the AND-
// chain's initial term, NOT a fresh/independent computation. Every TileKind lookup here
// reuses the shared g_UIResources.TileKind_GetOrLoadDescriptor + CursorDesc_IsItemAvailableMaybe
// ready-check idiom already established elsewhere (UIResources.cpp etc.).
// EXACT MATCH (1257 B, v358) -- and a two-step lesson in what a big residual can be hiding.
// This was parked at DIFF(986) with a four-item autopsy calling every item an ordinary
// reg-alloc tie-break. Both halves of that were wrong:
//   1. ~980 of those bytes were the RTM-vs-SP3 toolchain error (see toolchain/README.md).
//      Switching to SP3 dropped it to FIVE bytes without touching this file.
//   2. The remaining five were all `test al,al` vs our `test eax,eax` at the SetDescriptor
//      result checks -- not a "partial- vs full-register width tie-break" but the plain
//      statement that the CALLEE RETURNS A BYTE. WidgetBase::SetDescriptor was declared
//      `virtual unsigned`; it is `virtual unsigned char`. Fixing the declaration closed it,
//      and changed nothing anywhere else in the repo.
// Read operand WIDTH in the disasm as declared-type evidence, exactly the way `sar` vs `shr`
// and `jl` vs `jb` are read as signedness evidence -- and never paper it over with a cast at
// the call site. Structure was already verified faithful end-to-end against the raw disasm
// (every TileKind lookup, every null-check-or-not on the RepositionWithHotspot calls, the
// exact field writes, and the final AND-chain's stack-spilled seed value).
char WorldActionCursor::InitTrainCouplingMenuIconsMaybe() {
    char bReady0Maybe = (char)this->SetDescriptor(0x3801, 0, 1);
    this->bAttachMenuToggleMaybe = false;

    CursorDesc *pDesc;
    MenuNodeObj0x477568 *pNode;
    MenuNodeObj0x477568 **ppSlot;
    int nX;
    int n;

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x2c09);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        this->pActiveCandidateNodeMaybe = (UiIconListItem *)this->GetOrCreateIconItemMaybe(pDesc, 10);
        if (this->pActiveCandidateNodeMaybe != NULL) {
            this->pActiveCandidateNodeMaybe->bTextRedrawEnabled = false;
            this->pActiveCandidateNodeMaybe->RepositionWithHotspot(0x10, 0x1a);
        }
        nX = 0x33;
        ppSlot = this->pDecorMenuIconsMaybe;
        n = 8;
        do {
            pNode = this->GetOrCreateIconItemMaybe(pDesc, 10);
            *ppSlot = pNode;
            if (pNode != NULL) {
                pNode->RepositionWithHotspot(0xa6, nX);
            }
            ppSlot++;
            nX += 0x19;
            n--;
        } while (n != 0);
    }

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3868);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        nX = 0x10;
        ppSlot = this->pCandidateVariantPrevBtnMaybe;
        n = 4;
        do {
            pNode = this->GetOrCreateIconItemMaybe(pDesc, 0);
            *ppSlot = pNode;
            pNode->RepositionWithHotspot(nX, 0x3b);
            ppSlot++;
            nX += 0x32;
            n--;
        } while (n != 0);
    }

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3869);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        nX = 0x10;
        ppSlot = this->pCandidateVariantNextBtnMaybe;
        n = 4;
        do {
            pNode = this->GetOrCreateIconItemMaybe(pDesc, 0);
            *ppSlot = pNode;
            pNode->RepositionWithHotspot(nX, 0x95);
            ppSlot++;
            nX += 0x32;
            n--;
        } while (n != 0);
    }

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3802);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        pNode = this->GetOrCreateIconItemMaybe(pDesc, 0);
        this->pDetachMenuItemMaybe = pNode;
        this->pBaseCandidateDown = pNode;
    }

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3803);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        pNode = this->GetOrCreateIconItemMaybe(pDesc, 0);
        this->pAttachOrSpawnMenuItemMaybe = pNode;
        this->pBaseCandidateUp = pNode;
    }

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x2c07);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        pNode = this->GetOrCreateIconItemMaybe(pDesc, 0);
        this->pCandidateUpMaybe = pNode;
        if (pNode != NULL) {
            pNode->RepositionWithHotspot(0xa6, 0x1a);
        }
    }

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x2c08);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        pNode = this->GetOrCreateIconItemMaybe(pDesc, 0);
        this->pCandidateDownMaybe = pNode;
        if (pNode != NULL) {
            pNode->RepositionWithHotspot(0xa6, 0xfb);
        }
    }

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3864);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        pNode = this->GetOrCreateIconItemMaybe(pDesc, 0);
        this->pCoupleChoiceAMaybe = pNode;
        if (pNode != NULL) {
            pNode->RepositionWithHotspot(0xec, 0x97);
        }
    }

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3865);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        pNode = this->GetOrCreateIconItemMaybe(pDesc, 0);
        this->pCoupleChoiceDetachMaybe = pNode;
        if (pNode != NULL) {
            pNode->RepositionWithHotspot(0xa6, 0x97);
        }
    }

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3867);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        pNode = this->GetOrCreateIconItemMaybe(pDesc, 0);
        this->pCoupleChoiceNewTrainMaybe = pNode;
        if (pNode != NULL) {
            pNode->RepositionWithHotspot(0xa6, 0x33);
        }
    }

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3866);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        pNode = this->GetOrCreateIconItemMaybe(pDesc, 0);
        this->pCoupleChoiceAttachExistingMaybe = pNode;
        if (pNode != NULL) {
            pNode->RepositionWithHotspot(0xec, 0x33);
        }
    }

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x380e);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        this->pIconStateTargetAMaybe = this->GetOrCreateIconItemMaybe(pDesc, 0);
    }

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x380f);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        this->pIconStateTargetBMaybe = this->GetOrCreateIconItemMaybe(pDesc, 0);
    }

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3810);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        this->pIconStateTargetCMaybe = this->GetOrCreateIconItemMaybe(pDesc, 0);
    }

    char bReadyMaybe = (this->animMaybe7.SetDescriptor(0x3809, 0xffffffff, 0) != 0) && (bReady0Maybe != 0) ? 1 : 0;
    bReadyMaybe = (this->animMaybe0.SetDescriptor(0x2402, 0xffffffff, 0) != 0) && (bReadyMaybe != 0) ? 1 : 0;
    bReadyMaybe = (this->animMaybe6.SetDescriptor(0x386a, 0xffffffff, 0) != 0) && (bReadyMaybe != 0) ? 1 : 0;
    char bAllReadyMaybe = (this->animArrayMaybe[0].SetDescriptor(0x386b, 0xffffffff, 0) != 0) && (bReadyMaybe != 0) ? 1 : 0;
    for (int i = 1; i < 4; i++) {
        bAllReadyMaybe = (this->animArrayMaybe[i].SetDescriptor(0x386c, 0xffffffff, 0) != 0) && (bAllReadyMaybe != 0) ? 1 : 0;
    }
    this->RepositionWithHotspot(10, 10);
    return bAllReadyMaybe;
}

// FUNCTION: LOCO 0x459da0
// EFFECTIVE MATCH -- asmscore 45028, insns 307/307, and the first 679 of 1010 bytes (everything
// through the menu-node dispatch loop and the mode-3 block) are byte-identical. The whole
// residual is ONE register-allocation coin-flip inside the mode-6 block, where three locals
// (bCoupled, nReversed, wSelectedCarId) compete for two callee-saved registers: esi/edi are
// already pTrain/this and ebp takes nReversed in BOTH builds, so exactly one of bCoupled and
// wSelectedCarId must live on the stack. The original gives ebx to wSelectedCarId (`mov bx,
// [esi+0x58]` once, then two 16-bit `cmp bx,[esi+0x24]`) and spills bCoupled to [esp+0x13]
// (one `mov [esp+0x13],al` store + two reloads); we give ebx to bCoupled (`sete bl` / `test
// bl,bl`) and spill wSelectedCarId. Instruction COUNT is identical either way -- our
// xor+store+reload trio exactly replaces the original's store+reload+reload -- which is what
// makes it a tie-break rather than a shape difference. Refuted probes, one compile each:
// declaring bCoupled first vs. last (identical 45028 both ways -- source order does not drive
// this allocator); `char` instead of `bool` for it (identical 45028); and dropping the named
// local entirely so the three tests re-read pTrain->dwSoundStateMaybe (WORSE, 108718 -- the
// compiler does NOT CSE a member read across the intervening SetNodeState calls, it reloads
// from [esi+0x5c], so the original's `sete`-materialized byte in a stack slot proves a real
// named local, not a CSE temp). See docs/PARKED.md.
//
// The widget's per-frame tick, driven by the world board's own widget-list pass. Five stages,
// all gated on the widget being active AND its selected world object still valid (a stale
// selection deselects outright and returns):
//   1. the animMaybe0 hover icon tracks the cursor -- its subframe follows "is the cursor
//      inside me", dirty-marking its rect on every transition;
//   2. while animMaybe6 is ready and its anim value is non-zero, all four variant-selector
//      icons re-roll to a random frame of their own descriptor's frame set;
//   3. a drag in flight (the base's bDraggingMaybe triple) just repositions and returns;
//   4. otherwise every menu node gets dispatched through slot 20, and the mode-feedback icon
//      animMaybe5 advances (in mode 7 it first re-seats its subframe from the selected
//      object's own packed tag byte);
//   5. modes 3 and 6 each refresh their menu and notify the tutorial (codes 0xc / 0xb) --
//      mode 6 additionally drives the three coupling icons from the selected car's train
//      state and deselects when that train has been handed off or torn down.
void WorldActionCursor::AdvanceAnimFrameMaybe()
{
    if (!this->bActive) {
        return;
    }
    if (this->pSelectedDecorObjMaybe->bValid != true) {
        this->SelectDecorObjAndDispatchModeMaybe(0);
        return;
    }

    // Taken by pointer, not as `this->animMaybe0.Contains(...)`: every dispatch here is a real
    // vtable call in the original, which a call on the embedded object itself would not be.
    AnimDescRefObj0x477488 *pHoverIcon = &this->animMaybe0;
    if (pHoverIcon->Contains(PlacementCursorMaybe_004854c8.lastResolvedPosX,
                             PlacementCursorMaybe_004854c8.lastResolvedPosY) != 0 &&
        this->animMaybe0.nSubFrame != 1) {
        pHoverIcon->ReleaseChannelAndDispatch(1);
        g_worldBoard.MarkRectDirty(this->animMaybe0.rect);
    }
    if (pHoverIcon->Contains(PlacementCursorMaybe_004854c8.lastResolvedPosX,
                             PlacementCursorMaybe_004854c8.lastResolvedPosY) == 0 &&
        this->animMaybe0.nSubFrame != 0) {
        pHoverIcon->ReleaseChannelAndDispatch(0);
        g_worldBoard.MarkRectDirty(this->animMaybe0.rect);
    }

    if (this->animMaybe6.bReady != false) {
        this->animMaybe6.AdvanceAnimFrameMaybe();
        if (this->animMaybe6.nAnimValueCache != 0) {
            for (int i = 0; i < 4; i++) {
                // The nFrameSetCount-1 sign test is the RAND_RANGE_MAYBE call-site idiom, not
                // defensive coding: the macro does not order its own arguments, so a site whose
                // bounds can invert branches and passes them swapped (see src/RandRange.h).
                int nFrame;
                if (this->animArrayMaybe[i].pKindDesc->nFrameSetCount - 1 >= 0) {
                    nFrame = RAND_RANGE_MAYBE(
                        0, this->animArrayMaybe[i].pKindDesc->nFrameSetCount - 1);
                } else {
                    nFrame = RAND_RANGE_MAYBE(
                        this->animArrayMaybe[i].pKindDesc->nFrameSetCount - 1, 0);
                }
                this->animArrayMaybe[i].ReleaseChannelAndDispatch(nFrame);
            }
        }
    }

    if (this->bDraggingMaybe == true) {
        this->RepositionWithHotspot(
            PlacementCursorMaybe_004854c8.lastResolvedPosX - this->nDragGrabOffsetXMaybe,
            PlacementCursorMaybe_004854c8.lastResolvedPosY - this->nDragGrabOffsetYMaybe);
        return;
    }

    // Computed and discarded -- the original builds the localized point and never reads it.
    POINT ptLocal = this->ComputeLocalPos(PlacementCursorMaybe_004854c8.lastResolvedPosX,
                                          PlacementCursorMaybe_004854c8.lastResolvedPosY);
    for (MenuNodeObj0x477568 *pNode = this->pMenuListHead; pNode != 0; pNode = pNode->pNext) {
        this->HandleMenuCommandMaybe(pNode);
    }

    if (this->animMaybe5.pKindDesc != 0) {
        if (this->nModeMaybe == 7) {
            short nSubFrame = (short)((unsigned char)this->pSelectedDecorObjMaybe[1].nTypeTag >> 1);
            if (this->animMaybe5.nSubFrame != nSubFrame) {
                this->animMaybe5.ReleaseChannelAndDispatch(nSubFrame);
            }
        }
        this->animMaybe5.AdvanceAnimFrameMaybe();
    }

    if (this->nModeMaybe == 3) {
        if (this->Unk0x8c < (int)g_dwGameTick && this->animMaybe6.bReady != true) {
            this->RefreshTrainCouplingMenuMaybe();
            this->Unk0x8c = g_dwGameTick;
        }
        g_pTutorialWnd->NotifyOrLaunch(0xc, 0);
    }

    if (this->nModeMaybe == 6) {
        PeerTrainNodePartial *pTrain =
            (PeerTrainNodePartial *)this->pSelectedDecorObjMaybe[8].rect.top;
        if (pTrain != 0) {
            bool bCoupled = pTrain->dwSoundStateMaybe == 2;
            int nReversed = pTrain->dwReversed;
            // UNSIGNED, matching the two members it is compared against: a `short` local here
            // makes both sides promote to int and costs a 16-bit `cmp bx, [esi+0x24]` its
            // register, spilling the whole comparison out to the stack.
            unsigned short wSelectedCarId = pTrain->wSelectedCarId;
            if (bCoupled && nReversed == 1) {
                if (wSelectedCarId == pTrain->wSelectedCarIdAMaybe) {
                    this->pIconStateTargetAMaybe->SetNodeState(2);
                    this->animMaybe7.ReleaseChannelAndDispatch(1);
                } else {
                    this->pIconStateTargetAMaybe->SetNodeState(3);
                    this->animMaybe7.ReleaseChannelAndDispatch(0);
                }
            } else {
                this->pIconStateTargetAMaybe->SetNodeState(1);
            }
            if (bCoupled && nReversed == 0) {
                if (wSelectedCarId == pTrain->wSelectedCarIdAMaybe) {
                    this->pIconStateTargetCMaybe->SetNodeState(2);
                    this->animMaybe7.ReleaseChannelAndDispatch(3);
                } else {
                    this->pIconStateTargetCMaybe->SetNodeState(3);
                    this->animMaybe7.ReleaseChannelAndDispatch(4);
                }
            } else {
                this->pIconStateTargetCMaybe->SetNodeState(1);
            }
            // Written negated (unlike the two blocks above): the original lays the
            // not-coupled arm out FIRST and branches `jne` to the coupled one.
            if (!bCoupled) {
                this->pIconStateTargetBMaybe->SetNodeState(2);
                this->animMaybe7.ReleaseChannelAndDispatch(2);
            } else {
                this->pIconStateTargetBMaybe->SetNodeState(1);
            }
            if (pTrain->dwSoundStateMaybe == 0 || pTrain->dwSoundStateMaybe == 1) {
                this->RefreshCategoryMenuIconsMaybe();
            }
            if (pTrain->nDeferredMoveStateMaybe == 2 || pTrain->dwModeAMaybe != 0 ||
                pTrain->dwModeBMaybe == 2) {
                this->SelectDecorObjAndDispatchModeMaybe(0);
            }
        }
        g_pTutorialWnd->NotifyOrLaunch(0xb, 0);
    }
}

// FUNCTION: LOCO 0x459d40
// Slot 1, this leaf's own dirty-mark. Chains the WidgetBase half (which covers `rect` plus the
// two auxiliary rects) and then dirty-marks the +0xe0 sub-icon in its own right, for the same
// reason Contains below has to union that icon's rect in: animMaybe0 hangs
// OUTSIDE the widget's rect, so the base pass never reaches it. The sub-icon's own MarkDirty is
// reached VIRTUALLY (`mov eax,[esi+0xe0]; lea ecx,[esi+0xe0]; call [eax+4]`) -- VC5 does not
// devirtualize a call on an embedded member here, exactly as at 0x42d040's own
// `animDescMaybe.MarkDirty()` tail.
void WorldActionCursor::MarkDirty()
{
    WidgetBaseObj0x4784c8::MarkDirty();
    this->animMaybe0.MarkDirty();
}

// FUNCTION: LOCO 0x459d60
// The widget's full hit area is the union of its own rect and the mode-feedback icon's, because
// the icon hangs outside `rect`. The first test is the root's own Contains, called
// class-qualified (non-virtual) so this leaf's own slot-2 override -- which walks the menu-node
// list -- is deliberately bypassed; the second goes through animMaybe0's OWN vtable, which is
// the base's plain rect test again but against the icon's rect.
char WorldActionCursor::Contains(int x, int y)
{
    return RectFlagObj0x477820::Contains(x, y) || animMaybe0.Contains(x, y);
}

// FUNCTION: LOCO 0x459720
// EFFECTIVE MATCH -- DIFF(10) at 180/180 bytes and 73/73 identical instructions. The ONLY
// disagreement is where the `g_rectAppClientBounds.right`/`.bottom` load is scheduled inside
// each else-branch (the original slots it in BEFORE the `sub eax,<width>`, reusing the now-dead
// pKindDesc register edx; the candidate emits it after, in ebx) -- a scheduler/allocator coin
// flip with no source lever. PROBED v450: the `int nLeft`/`int nScrollX` temps ARE load-bearing
// and correct (they are what puts the rect read ahead of the scroll-global read -- without them
// the two loads swap, DIFF(17)); writing the max expression inline in an `else if` instead of a
// temp breaks the two Reposition calls' cross-jump merge outright (212 B, DIFF(112)).
// Keeps the action cursor on-screen. Two independent shoves, applied in order:
//   1. if the widget's rect overlaps EITHER of the selected-object widget's two rects, park it
//      in the bottom-right corner of the client area (so the two never obscure each other);
//   2. clamp the (possibly just-moved) rect back inside the world board's scrolled viewport,
//      one axis at a time, against the descriptor's own native width/height.
// Every move goes through slot 3, so the leaf's own reposition override does the real work.
void WorldActionCursor::ClampRectIntoViewMaybe()
{
    RECT rcOverlap;
    if (IntersectRect(&rcOverlap, &this->rect, &SelectedObjWidgetMaybe_004852a0.rect) ||
        IntersectRect(&rcOverlap, &this->rect,
                      &SelectedObjWidgetMaybe_004852a0.animDescMaybe.rect)) {
        this->RepositionWithHotspot(g_rectAppClientBounds.right - this->rect.right,
                                    g_rectAppClientBounds.bottom - this->rect.bottom);
    }
    int nLeft = this->rect.left;
    int nScrollX = g_worldBoard.dwScrollX;
    if (nLeft < nScrollX) {
        this->RepositionWithHotspot(nScrollX, this->rect.top);
    } else {
        int nMaxX = nScrollX - this->pKindDesc->nativeWidth + g_rectAppClientBounds.right;
        if (nLeft > nMaxX) {
            this->RepositionWithHotspot(nMaxX, this->rect.top);
        }
    }
    int nTop = this->rect.top;
    int nScrollY = g_worldBoard.dwScrollY;
    if (nTop < nScrollY) {
        this->RepositionWithHotspot(this->rect.left, nScrollY);
    } else {
        int nMaxY = nScrollY - this->pKindDesc->nativeHeight + g_rectAppClientBounds.bottom;
        if (nTop > nMaxY) {
            this->RepositionWithHotspot(this->rect.left, nMaxY);
        }
    }
}

// FUNCTION: LOCO 0x45a1a0
// The widget's paint-pass fan-out, called directly by WorldBoardMaybe's paint pass
// (0x456700) with the dirty rect pushed BY VALUE. When the widget is active: its own
// composite blit first (class-qualified to WidgetBaseObj0x4784c8's slot-11 override --
// the original emits a DIRECT call to 0x454900 there), then slot-11 blits of every live
// sub-icon: animMaybe0 always; animMaybe5 only when it has a descriptor and is valid,
// immediately followed by its slot-12 overlay blit; animMaybe6 and the four
// animArrayMaybe variant icons only while animMaybe6 is ready; and animMaybe7 last,
// unconditionally.
void WorldActionCursor::RepositionSubIconsMaybe(RECT rect, int bFlag)
{
    if (this->bActive) {
        WidgetBaseObj0x4784c8::BlitAnimFrameMaybe(rect, bFlag, 0);
        this->animMaybe0.BlitAnimFrameMaybe(rect, bFlag, 0);
        if (this->animMaybe5.pKindDesc != 0 && this->animMaybe5.bValid == true) {
            this->animMaybe5.BlitAnimFrameMaybe(rect, bFlag, 0);
            this->animMaybe5.BlitOverlayFrameMaybe(rect, bFlag, 0);
        }
        if (this->animMaybe6.bReady) {
            this->animMaybe6.BlitAnimFrameMaybe(rect, bFlag, 0);
            AnimDescRefObj0x477488 *pAnim = this->animArrayMaybe;
            int nCount = 4;
            do {
                pAnim->BlitAnimFrameMaybe(rect, bFlag, 0);
                pAnim++;
                nCount--;
            } while (nCount != 0);
        }
        this->animMaybe7.BlitAnimFrameMaybe(rect, bFlag, 0);
    }
}

// FUNCTION: LOCO 0x45a330
// Repaints the 8 decor candidate icons for the page starting at nBase, under the decor
// manager's category-7 lock: each icon is relabelled from its candidate object's
// szCategoryName (or blanked when the slot is empty or the object is not valid yet),
// highlighted (state 2) when it IS the current selection, and redrawn. The new page base is
// latched into nCandidateBaseMaybe before the lock is dropped.
//
// The two page-scroll arrows are then greyed (state 3) at the ends of the range: "up" when the
// page base is already 0, "down" when the LAST slot of the page came back empty -- pDecor
// deliberately survives the loop for that second test, which is why it is declared at function
// scope rather than inside it.
void WorldActionCursor::RefreshDecorCategoryCandidatesMaybe(unsigned int nBase)
{
    AnimDescRefObj0x477488 *pDecor;
    // Through a pointer, not the global directly: naming the concrete object lets cl
    // devirtualize slot 8 into a direct call, where the original really does dispatch
    // (`mov edx,[0x485494]; mov ecx,0x485494; call [edx+0x20]`). Same idiom as
    // HitTestNodeSecondary's own use of this probe above.
    DecorCategoryMgrVtblProbe *pMgr = &DecorCategoryMgrMaybe_00485494;

    DecorObjMgrMaybe_00485448.lockAMaybe.Lock();
    for (unsigned int i = 0; i < 8; i++) {
        pDecor = (AnimDescRefObj0x477488 *)pMgr->GetCategoryObjByIndexMaybe(nBase + i);
        if (pDecor == this->pSelectedDecorObjMaybe) {
            this->pDecorMenuIconsMaybe[i]->SetNodeState(2);
        } else {
            this->pDecorMenuIconsMaybe[i]->SetNodeState(1);
        }
        if (pDecor != NULL && pDecor->bValid == true) {
            ((UiIconListItem *)this->pDecorMenuIconsMaybe[i])->SetLabelText(pDecor->szCategoryName);
        } else {
            ((UiIconListItem *)this->pDecorMenuIconsMaybe[i])->SetLabelText("");
        }
        this->pDecorMenuIconsMaybe[i]->Draw();
    }
    this->nCandidateBaseMaybe = nBase;
    DecorObjMgrMaybe_00485448.lockAMaybe.Unlock();
    if (this->nCandidateBaseMaybe > 0) {
        this->pCandidateUpMaybe->SetNodeState(1);
    } else {
        this->pCandidateUpMaybe->SetNodeState(3);
    }
    if (pDecor != NULL) {
        this->pCandidateDownMaybe->SetNodeState(1);
    } else {
        this->pCandidateDownMaybe->SetNodeState(3);
    }
}

// FUNCTION: LOCO 0x45a400
// EFFECTIVE MATCH -- DIFF(3) at 125/125 bytes and 45/45 identical instructions. The original
// loads pSelectedDecorObjMaybe into a scratch register and LEAs the +0x90 (`mov eax,[..];
// lea edi,[eax+0x90]`); the candidate loads straight into the induction register and adds
// (`mov edi,[..]; add edi,0x90`). PROBED v450 across three source shapes -- indexed loop,
// indexed loop through an explicit `AnimDescRefObj0x477488 *pSel` temp, and this induction-
// pointer form -- all three give the identical 3-byte residual, so it is not source-steerable.
// (Declaring `i` FIRST is load-bearing and not cosmetic: with the pointer declared first the
// counter's `xor bl,bl` sinks below both leas, DIFF(19).) The induction-pointer form is kept
// because it is this TU's own house style for the same 8-icon walk (see the 0x45aa50 case-8
// and 0x4597e0 loops).
// Mode 0/2 (decor-variant) candidate refresh: relabels the 8 decor candidate icons from the
// selected object's own 5-entry variant list at +0x90. An occupied entry lights up (state 1)
// and takes that variant's category string; the three slots past the end -- and any empty
// entry -- grey out (state 3) with a blank label. The page up/down buttons are always parked
// in state 3 here: a variant list is never longer than one page.
void WorldActionCursor::RefreshVariantMenuIconsMaybe()
{
    // +0x90 into the selected decor object = [1].rect.left onward -- the same variant list
    // HandleMenuCommandMaybe's 0x2c09 mode-0/2 case indexes.
    unsigned char i = 0;
    MenuNodeObj0x477568 **ppNode = this->pDecorMenuIconsMaybe;
    int *pnVariants = (int *)&this->pSelectedDecorObjMaybe[1].rect;
    do {
        if (i < 5 && *pnVariants != 0) {
            (*ppNode)->SetNodeState(1);
            ((UiIconListItem *)*ppNode)
                ->SetLabelText(((AnimDescRefObj0x477488 *)*pnVariants)->szCategoryName);
        } else {
            (*ppNode)->SetNodeState(3);
            ((UiIconListItem *)*ppNode)->SetLabelText("");
        }
        (*ppNode)->Draw();
        i++;
        pnVariants++;
        ppNode++;
    } while (i < 8);
    this->pCandidateUpMaybe->SetNodeState(3);
    this->pCandidateDownMaybe->SetNodeState(3);
}

// FUNCTION: LOCO 0x45a480
// Mode 4 (train-passenger) candidate refresh: the same 8 icons, relabelled from the attached
// train's 8 passenger slots instead. No train attached => nothing to show and the whole body
// is skipped, leaving the icons however the previous mode left them.
void WorldActionCursor::RefreshCategoryMenuIconsMaybe()
{
    PeerTrainNodePartial *pTrain =
        (PeerTrainNodePartial *)this->pSelectedDecorObjMaybe[8].rect.top;
    if (pTrain == 0) {
        return;
    }
    for (int i = 0; i < 8; i++) {
        if (pTrain->apPassengerMaybe[i] != 0) {
            this->pDecorMenuIconsMaybe[i]->SetNodeState(1);
            ((UiIconListItem *)this->pDecorMenuIconsMaybe[i])
                ->SetLabelText(((AnimDescRefObj0x477488 *)pTrain->apPassengerMaybe[i])
                                   ->szCategoryName);
        } else {
            this->pDecorMenuIconsMaybe[i]->SetNodeState(3);
            ((UiIconListItem *)this->pDecorMenuIconsMaybe[i])->SetLabelText("");
        }
        this->pDecorMenuIconsMaybe[i]->Draw();
    }
    this->pCandidateUpMaybe->SetNodeState(3);
    this->pCandidateDownMaybe->SetNodeState(3);
}

// FUNCTION: LOCO 0x45a500
// Real vtable slot 3 -- the widget's reposition override, and the whole reason this class's
// on-screen furniture stays glued together. The requested origin is first clamped into the
// world board's viewport (never negative, never past viewport extent minus the widget's own
// rectViewport size), the direct base moves `rect` there, and then every owned sub-icon is
// dragged to its own fixed offset from the new position: the command icon sits 0x38 to the
// right normally but 0x83 to the right once the attach menu is toggled open (it has to clear
// the wider menu), the mode-feedback icon re-centers itself in the constant decor bounds, and
// the four variant icons march across at a 50-pixel pitch.
//
// The tail is the drag case, and it is the same idiom BuildToolButton::RepositionWithHotspot
// uses: if the widget is being dragged and the CLAMP actually moved it (the clamped origin
// differs from what the caller asked for), the OS cursor is warped so the grab point stays
// pinned under the pointer -- otherwise the pointer would slide off a widget that just hit
// the edge of the board. The placement cursor's own packed client-space position is then
// re-derived rather than reused, because ClientToScreen has already overwritten the POINT.
void WorldActionCursor::RepositionWithHotspot(int x, int y)
{
    int nX = x;
    if (nX < 0) {
        nX = 0;
    }
    int nLimitX = g_worldBoard.dwViewportWidth - this->rectViewport.right;
    if (nX > nLimitX) {
        nX = nLimitX;
    }
    int nY = y;
    if (nY < 0) {
        nY = 0;
    }
    int nLimitY = g_worldBoard.dwViewportHeightMaybe - this->rectViewport.bottom;
    if (nY > nLimitY) {
        nY = nLimitY;
    }
    WidgetBaseObj0x4784c8::RepositionWithHotspot(nX, nY);
    // ⚠ The ternary has to span the WHOLE expression, not just the two constants. Written as
    // `nX + (b ? 0x83 : 0x38)` cl 5.0 sees two constants a constant apart and goes branchless
    // (`neg dl; sbb edx,edx; and edx,0x4b; add edx,0x38`); written as an if/else over a named
    // `int nIconX` it branches but keeps the result in its own register (`lea eax,[edi+0x83]`).
    // The original does neither -- `test al,al; je; add edi,0x83; jmp; add edi,0x38` ADDS INTO
    // nX's own register, which cl only does for a temporary consumed immediately by the push
    // (nX itself survives in its stack slot, which is where the drag test below re-reads it).
    this->animMaybe0.RepositionWithHotspot(
        this->bAttachMenuToggleMaybe ? nX + 0x83 : nX + 0x38, nY - 0xf);
    if (this->animMaybe5.bValid != false && this->animMaybe5.pKindDesc != NULL) {
        // ⛔ PARKED, 5 instructions. The original has a live mode-6 test INSIDE this centering
        // call that the helper's five other call sites do not have: `cmp word ptr
        // [esi+0x398],6` at 0x45a597, with the `jne` at 0x45a5ba straddling two pushes of the
        // SAME stack slot in different registers (`lea eax,[esp+0x18]` vs `lea ecx,[esp+0x18]`)
        // before a shared `push 0x478538; call CenterRectInRect`. Since arguments push
        // right-to-left, the branch sits on the SECOND argument -- the inner rect -- and there
        // is exactly one call, one rect copy (shared, ahead of the branch) and one reposition.
        //
        // Three source shapes were measured and NONE reproduces it (all three land on the same
        // 165/171, total 159765): a ternary `nModeMaybe == 6 ? &rc : (RECT *)&rc.left`, the
        // same choice made by an if/else over a `RECT *prc`, and two full inline expansions of
        // the helper in an if/else -- that last one does not merge at all but DUPLICATES
        // (+105 bytes), which is independent evidence that cl 5.0 does not cross-jump here.
        // cl folds any two expressions naming one compile-time-known stack address, so no
        // same-slot spelling can survive; reproducing this needs a second rect that is really
        // a different object and merely shares the slot, which nothing in the body supplies
        // yet. Left as the plain helper call until that object is identified.
        this->CenterModeAnimOverWidgetMaybe();
    }
    if (this->animMaybe6.bValid != false) {
        this->animMaybe6.RepositionWithHotspot(this->rect.left + 0xe6, this->rect.top + 0x22);
    }
    if (this->animMaybe7.bValid != false) {
        this->animMaybe7.RepositionWithHotspot(this->rect.left + 0x10, this->rect.top + 0x46);
    }
    // Hand-rolled induction pair (offset + pointer), this TU's house style for its fixed-count
    // icon walks: the original steps a 50-pixel x offset to 200 and an 0x88-stride pointer in
    // lockstep, with an UNSIGNED loop bound (`cmp edi,0xc8; jb`).
    unsigned int nOffsetX = 0;
    AnimDescRefObj0x477488 *pAnim = this->animArrayMaybe;
    do {
        pAnim->RepositionWithHotspot(this->rect.left + 0x10 + nOffsetX, this->rect.top + 0x5e);
        nOffsetX += 50;
        pAnim++;
    } while (nOffsetX < 200);
    if (this->bDraggingMaybe && (nX != x || nY != y)) {
        POINT ptCursor;
        ptCursor.x = nX - g_worldBoard.dwScrollX + this->nDragGrabOffsetXMaybe;
        ptCursor.y = nY - g_worldBoard.dwScrollY + this->nDragGrabOffsetYMaybe;
        ClientToScreen(g_pApp->hwndOwner, &ptCursor);
        SetCursorPos(ptCursor.x, ptCursor.y);
        PlacementCursorMaybe_004854c8.packedMousePosMaybe =
            MAKELONG((short)(nX - g_worldBoard.dwScrollX) + (short)this->nDragGrabOffsetXMaybe,
                     (short)(nY - g_worldBoard.dwScrollY) + (short)this->nDragGrabOffsetYMaybe);
        PlacementCursorMaybe_004854c8.FUN_00410a20();
    }
}

// FUNCTION: LOCO 0x45a740
// Real vtable slot 4 -- the widget's click handler (see the header note: this address IS the
// slot, it is simply declared as an ordinary member so its one direct call site keeps its
// direct call).
//
// An inactive widget declines outright, and a widget already being DRAGGED consumes the click
// and only ends the drag -- the release IS the click, exactly as in the sibling
// SelectedObjWidgetMaybe::TryInvokeCallbackA below. A press on the command icon instead
// STARTS a drag, recording where inside `rect` the cursor grabbed it (which is what slot 3's
// tail then keeps pinned). A press on animMaybe6 releases that icon's sound channel and
// consumes.
//
// Failing all of those, the candidate node's text-redraw gate is dropped and the coupling icon
// gets a two-zone hit test: the LEFT half of it (up to half the descriptor's own native width)
// arms pIconStateTargetAMaybe, the right half pIconStateTargetCMaybe -- one icon, two commands.
// Either way the family base's ordinary per-node walk gets the final say, which is why the
// left-half path duplicates that tail call rather than falling through.
char WorldActionCursor::TryInvokeCallbackA(int x, int y)
{
    if (this->bActive != true) {
        return 0;
    }
    if (this->bDraggingMaybe != false) {
        this->bDraggingMaybe = false;
        return 1;
    }
    if (this->animMaybe0.Contains(x, y)) {
        this->nDragGrabOffsetXMaybe =
            PlacementCursorMaybe_004854c8.resolvedPosAX - this->rect.left;
        this->nDragGrabOffsetYMaybe =
            PlacementCursorMaybe_004854c8.resolvedPosAY - this->rect.top;
        this->bDraggingMaybe = true;
        return 1;
    }
    if (this->animMaybe6.bReady != false) {
        if (this->animMaybe6.Contains(x, y)) {
            this->animMaybe6.ReleaseChannelAndDispatch(1);
            return 1;
        }
    }
    this->pActiveCandidateNodeMaybe->bTextRedrawEnabled = false;
    if (this->animMaybe7.bReady != false && this->animMaybe7.pKindDesc != NULL) {
        if (this->animMaybe7.Contains(x, y)) {
            if (x < this->animMaybe7.rect.left +
                        (this->animMaybe7.pKindDesc->nativeWidth >> 1)) {
                this->pIconStateTargetAMaybe->wSelIndexMaybe = 1;
                return WidgetBaseObj0x4784c8::TryInvokeCallbackA(x, y);
            }
            this->pIconStateTargetCMaybe->wSelIndexMaybe = 1;
        }
    }
    return WidgetBaseObj0x4784c8::TryInvokeCallbackA(x, y);
}

// TU-local methods-only views for the two singletons WorldIdleEventPumpThreadProc below drives
// that this TU does not otherwise model. Same rule and same shape as src/Main.cpp's
// EasterEggMgrWndProcView0x4618c0 / src/CursorDesc.cpp's three views:
//   - the 0x4a99b0 singleton is fully modeled as `ScriptEventLoader` in
//     src/ScriptEventLoader.cpp, but that class is TU-local there (see src/EasterEggMgr.h's own
//     fold note -- moving it into a header its consumers can include is a measured-parity change,
//     not a free one), so this is the fourth partial view of the same object;
//   - ActivateEligibleEntriesMaybe is deliberately absent from DecorObjMgrMaybe.h, measured this
//     session at -152 B if declared there (see src/DecorActor.cpp's own view of the same method).
struct EasterEggMgrIdlePumpView0x42cc60 {
    void TickWorldIdleMaybe(); // 0x41fd00, src/ScriptEventLoader.cpp // TODO: sync
};
extern EasterEggMgrIdlePumpView0x42cc60 g_easterEggMgrMaybe; // DAT_004a99b0

// FUNCTION: LOCO 0x42cc60
// The world idle thread body -- launched once by SelectedObjWidgetMaybe's own entry path via
// g_worldLoadThread.Start, and the ONLY caller of ScriptEventLoader::TickWorldIdleMaybe.
//
// It drops itself to THREAD_PRIORITY_LOWEST first: everything it does is catch-up work that must
// never compete with the frame loop. Then, ONCE per world (bTrackGraphsBuiltFlag is the latch,
// raised inside BuildTrackGraphsIfReadyMaybe itself), it finishes bringing the loaded world up --
// build the track graphs, put every actor back on its path, and hand the unemployed ones a
// workplace. That triple is the deferred tail of world load, which is why it runs on a background
// thread at all rather than in the loader.
//
// After that it is a plain pump: tick the world once a second for as long as the app stays in
// screen state 3, and fall off the end (ending the thread) the moment it leaves. The 999 ms --
// not 1000 -- is the original's, and the cadence is deliberately just under the one-second tick
// the pump's own modulo tests key off.
void __cdecl WorldIdleEventPumpThreadProc(void *pArg)
{
    g_worldLoadThread.SetPriority(THREAD_PRIORITY_LOWEST);
    if (g_worldBoard.bTrackGraphsBuiltFlag == 0) {
        g_worldBoard.BuildTrackGraphsIfReadyMaybe();
        DecorObjMgrMaybe_00485448.RestoreEntryPositionsMaybe();
        DecorObjMgrMaybe_00485448.ActivateEligibleEntriesMaybe();
    }
    while (IsInGameModeMaybe()) {
        g_easterEggMgrMaybe.TickWorldIdleMaybe();
        Sleep(999);
    }
}

// FUNCTION: LOCO 0x42cce0
// The singleton's ctor, driven by the atexit-registered ctor/dtor thunk pair at
// 0x45c710/0x45c730. The base-class chain and the embedded animDescMaybe's construction are
// compiler-generated (that component's declared -1,-1,0,0 defaults are exactly the four
// values the original pushes), as is the vtable store and the /GX frame; the source body is
// just this class's own four field stamps. bSuppressRectBMaybe is the base's flag, cleared by
// the base ctor a few instructions earlier and re-armed here.
SelectedObjWidgetMaybe::SelectedObjWidgetMaybe()
{
    nTypeTag = 0xe;
    pSelectedObjMaybe = NULL;
    bSuppressRectBMaybe = true;
    pIconToolboxBitmapMaybe = NULL;
}

// FUNCTION: LOCO 0x42cd60 (??_GSelectedObjWidgetMaybe scalar deleting dtor -- compiler-generated
// around ~SelectedObjWidgetMaybe() below; no source of its own)

// FUNCTION: LOCO 0x42cd80
// vtable slot 0. Empty body: the vtable re-stamp, animDescMaybe's destruction (SEH state 0)
// and the WidgetBaseObj0x4784c8 base chain are all compiler-generated under /GX.
SelectedObjWidgetMaybe::~SelectedObjWidgetMaybe()
{
}

// FUNCTION: LOCO 0x42cdd0
// Close/tear-down for the selection widget: deletes the icon-toolbox canvas if one was
// allocated, unloads BOTH descriptors (the companion animDescMaybe's first, then the
// widget's own -- the original's order) with the (0,-1,0) "clear" triple, and finally
// clears the base class's owned-child list. This IS vtable slot 15 -- the class's override of
// WidgetBaseObj0x4784c8::ClearOwned (v550; it was a free __fastcall
// SelectedObjWidgetMaybe_CloseMaybe declared file-locally in src/AppWindow.cpp until then).
// EFFECTIVE MATCH (v505): instruction-for-instruction identical, 24/24; the whole residual
// is ONE /Og scheduling swap -- the original places `mov ecx,esi` for the own-descriptor
// SetDescriptor call AFTER its three pushes, this build hoists it ahead of them (the
// animDescMaybe call two calls earlier keeps its lea ecx BEFORE the pushes in BOTH builds,
// so the asymmetry is the scheduler's, not the source's). ClearOwned must be base-qualified
// -- unqualified it dispatches virtually through slot 15 where the original emits a direct
// call. Parked in docs/PARKED.md.
void SelectedObjWidgetMaybe::ClearOwned()
{
    if (pIconToolboxBitmapMaybe != NULL) {
        delete pIconToolboxBitmapMaybe;
    }
    animDescMaybe.SetDescriptor(0, -1, 0);
    SetDescriptor(0, -1, 0);
    WidgetBaseObj0x4784c8::ClearOwned();
}

// FUNCTION: LOCO 0x42ce10
// The widget half of the cold-start category-icon bring-up (the world-load thread's last
// subsystem step before the screen-state wait, reached from LoadingScreen.cpp's
// App_LoadWorldThreadProcMaybe). Each of the three TileKinds gets the shared
// TileKind_GetOrLoadDescriptor + CursorDesc_IsItemAvailableMaybe ready-check idiom before
// its menu node is created; the 0x3807/0x3808 nodes double as the base class's
// pBaseCandidateUp/Down. The two toolbox SetDescriptor loads are AND-ed the way the
// original materializes them (bl := (own load && companion load)), and only then is the
// canvas bitmap allocated and filled -- the EH frame in the prologue is that `new`'s
// unwind cleanup.
char SelectedObjWidgetMaybe::LoadCategoryIconsMaybe()
{
    CursorDesc *pDesc;
    char bDescriptorsReady;

    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3807);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        MenuNodeObj0x477568 *pNode = this->GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
        this->pCategoryIconNode0x3807Maybe = pNode;
        this->pBaseCandidateUp = pNode;
    }
    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3808);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        MenuNodeObj0x477568 *pNode = this->GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
        this->pCategoryIconNode0x3808Maybe = pNode;
        this->pBaseCandidateDown = pNode;
    }
    pDesc = g_UIResources.TileKind_GetOrLoadDescriptor(0x3806);
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        this->pCategoryIconNode0x3806Maybe = this->GetOrCreateMenuIconItemMaybe(pDesc, 0, 0);
    }

    if (this->SetDescriptor(0x3805, -1, 0) == 0 ||
        this->animDescMaybe.SetDescriptor(0x3804, -1, 0) == 0) {
        bDescriptorsReady = 0;
    } else {
        bDescriptorsReady = 1;
    }
    if (bDescriptorsReady != 0) {
        LocoBitmap *pBitmap = new LocoBitmap();
        this->pIconToolboxBitmapMaybe = pBitmap;
        pBitmap->CreateAndFill(this->animDescMaybe.pKindDesc->pOwnedObjA->width,
                               this->animDescMaybe.pKindDesc->pOwnedObjA->height, 1, 0, 0);
        SetRect(&this->rectIconToolboxMaybe, 0, 0, this->pIconToolboxBitmapMaybe->width,
                this->pIconToolboxBitmapMaybe->height);
    }
    return bDescriptorsReady;
}

// FUNCTION: LOCO 0x42cf90
// The select-gate predicate: may this decor object be selected right now? Category 7 is
// always selectable; categories 8/2/6 defer to the object's own bReady; category 4 needs
// the descriptor counted in the ready-BigObj count; category 3 needs the track-family
// type set; category 0xc needs a resourceId above 0x300f. The mode word is held in a
// WORD register throughout (the original's `movzx si, al` / `cmp si, N`), so the local
// is a short, not a char.
unsigned char SelectedObjWidgetMaybe_CanSelectDecorObjMaybe(AnimDescRefObj0x477488 *pDecor)
{
    short nCategory = 0;
    if (pDecor != NULL && pDecor->bValid == true) {
        unsigned char bCategory;
        if (pDecor->pKindDesc == NULL) {
            bCategory = 0;
        } else {
            bCategory = pDecor->pKindDesc->categoryByte;
        }
        nCategory = bCategory;
    }
    if (nCategory == 0) {
        return 0;
    }
    if (nCategory == 7) {
        return 1;
    }
    if (nCategory != 8 && nCategory != 2 && nCategory != 6) {
        if (nCategory == 4 && pDecor->pKindDesc->bCountedInReadyBigObjCount == 1) {
            return 1;
        }
        if (nCategory == 3 && pDecor->pKindDesc->IsType0x63aInSet() != 0) {
            return 1;
        }
        if (nCategory == 0xc &&
            (pDecor->pKindDesc == NULL ? -1 : pDecor->pKindDesc->resourceId) >= 0x3010) {
            return 1;
        }
        return 0;
    }
    if (pDecor->bReady == true) {
        return 1;
    }
    return 0;
}

// FUNCTION: LOCO 0x42d040
// Selects the given world object (cast back off the load-bearing `int` spelling the callers
// use), or clears the selection on NULL / any failed gate. The guard chain is
// short-circuited exactly in the original's order: non-null, in-game (the sete-materialized
// IsInGameModeMaybe idiom), CanSelectDecorObjMaybe, and not g_bCmdlineSFlagSet. The select
// tail mirrors SelectDecorObjAndDispatchModeMaybe's own select path (bActive, the
// categoryByte if/else producing the `xor r16,r16; mov r16l,al` zero-extend into the mode
// word, the pActiveTabWidget handoff) and then forwards the selection to it; the deselect
// tail mirrors ITS deselect tail, handing g_pActiveTabWidgetMaybe back to the action
// cursor iff THAT is the active one. Returns bActive either way.
char SelectedObjWidgetMaybe::SelectObjMaybe(int nObj)
{
    AnimDescRefObj0x477488 *pObj = (AnimDescRefObj0x477488 *)nObj;
    if (pObj != 0 && IsInGameModeMaybe() &&
        SelectedObjWidgetMaybe_CanSelectDecorObjMaybe(pObj) && g_bCmdlineSFlagSet != 1) {
        this->bActive = true;
        {
            unsigned char bMode;
            if (pObj->pKindDesc == 0) {
                bMode = 0;
            } else {
                bMode = pObj->pKindDesc->categoryByte;
            }
            this->nModeMaybe = bMode;
        }
        if (g_worldActionCursor.bActive == 0) {
            g_pActiveTabWidgetMaybe = this;
        }
        this->pSelectedObjMaybe = pObj;
        this->RepositionWithHotspot(
            pObj->rect.left + ((pObj->rect.right - pObj->rect.left) >> 1),
            pObj->rect.top + ((pObj->rect.bottom - pObj->rect.top) >> 1));
        if (this->nModeMaybe == 6) {
            this->pCategoryIconNode0x3806Maybe->SetNodeState(1);
        } else {
            this->pCategoryIconNode0x3806Maybe->SetNodeState(3);
        }
        this->pCategoryIconNode0x3806Maybe->Draw();
        g_worldBoard.MarkRectDirty(this->rect);
        g_worldActionCursor.SelectDecorObjAndDispatchModeMaybe(this->pSelectedObjMaybe);
        return this->bActive;
    }
    this->bActive = false;
    this->nModeMaybe = 0;
    g_pActiveTabWidgetMaybe =
        g_worldActionCursor.bActive == true ? (void *)&g_worldActionCursor : 0;
    this->MarkDirty();
    this->animDescMaybe.MarkDirty();
    return this->bActive;
}

// FUNCTION: LOCO 0x42d1a0
// EFFECTIVE MATCH (v505) -- DIFF(98) at 205/209 bytes but 71/71 identical instructions,
// total 24018 / align=24. Three residuals, all /Og lottery: (a) the original loads
// +0x194 into ecx ahead of the second center compare while this build memory-compares it;
// (b) the bMoved materialization is full-width `xor eax,eax`/`mov eax,1` there vs
// byte-width `xor al,al`/`mov al,1` here (same register, pure instruction-selection tie --
// `int`/`char` spellings of the local all score WORSE, refuted); (c) the epilogue is
// split into two rets here vs one shared pop-tail there. Parked in docs/PARKED.md.
// This class's own vtable slot 10 override (the base's AdvanceAnimFrameMaybe slot,
// repurposed): the per-frame follow-the-selection tick. While active it (a) deselects a
// mode-6 selection whose object has gone not-ready, (b) recomputes the selected object's
// rect center and re-anchors the widget through slot 3 only when the center MOVED (the
// +0x190/+0x194 cache), (c) runs the cursor-position localization (result deliberately
// discarded -- the call itself is the point, sic), (d) walks the menu-node list through
// slot 20, and (e) dirty-marks the companion icon. Note the center recompute dereferences
// pSelectedObjMaybe UNCONDITIONALLY right after a deselect that may have cleared the
// selection -- the original reloads [esi+0xe0] with no null check (sic).
void SelectedObjWidgetMaybe::AdvanceAnimFrameMaybe()
{
    if (this->bActive != false) {
        if (this->nModeMaybe == 6 && this->pSelectedObjMaybe->bReady == false) {
            this->SelectObjMaybe(0);
        }
        AnimDescRefObj0x477488 *pObj = this->pSelectedObjMaybe;
        int nCenterX = ((pObj->rect.right - pObj->rect.left) >> 1) + pObj->rect.left;
        int nCenterY = ((pObj->rect.bottom - pObj->rect.top) >> 1) + pObj->rect.top;
        bool bMoved;
        if (this->nFollowCenterXMaybe == nCenterX && this->nFollowCenterYMaybe == nCenterY) {
            bMoved = false;
        } else {
            bMoved = true;
        }
        if (bMoved) {
            this->RepositionWithHotspot(nCenterX, nCenterY);
            this->nFollowCenterXMaybe = nCenterX;
            this->nFollowCenterYMaybe = nCenterY;
        }
        this->ComputeLocalPos(PlacementCursorMaybe_004854c8.lastResolvedPosX,
                              PlacementCursorMaybe_004854c8.lastResolvedPosY);
        for (MenuNodeObj0x477568 *pNode = this->pMenuListHead; pNode != NULL;
             pNode = pNode->pNext) {
            this->HandleMenuCommandMaybe(pNode);
        }
        this->animDescMaybe.MarkDirty();
    }
}

// FUNCTION: LOCO 0x42d280
// MATCHES EXACT (278 B) as a real member -- unparked v552, and a second confirmation of
// CODEGEN #149: from v505 to v551 this was the free __fastcall form and sat EFFECTIVE-parked
// at DIFF(10)/86-of-86 instructions on one /Og scheduling swap (`lea edi,[esi+0x180]` landing
// 7 bytes early). Nothing about the body changed to fix it -- only the calling convention the
// declaration implies. The free form was itself forced by this header's dial (the member
// declarations cost TilePlacedObj 0x4588b0 its 143 B, v505 and re-priced v552); +278 for -143
// is the trade, and it also makes the model true.
// The clipped half of the paint-pass blit pair (both take the flushed dirty RECT by value
// and both ignore it -- the signature is the paint pass's, not the body's). Computes a
// clip rect from the companion animDescMaybe's rect inset by a quarter of its viewport
// (or a fixed 0x90/0x8c margin in mode 7), intersects it against the board viewport,
// Blts the work surface into the toolbox canvas through that clip, then composites the
// descriptor's realized canvas back over it with RestoreOverlapBlt -- flag 0x20 (the
// mirrored-source variant) when the WIDGET's own current subframe entry has
// Unk0x16Maybe set (sic: the entry table is animDescMaybe's descriptor's, the index is
// the widget's).
void SelectedObjWidgetMaybe::BlitIconToolboxClippedMaybe(RECT rect)
{
    unsigned int flags = 0;
    int nInsetX;
    int nInsetY;
    if (this->nModeMaybe == 7) {
        nInsetX = this->animDescMaybe.rectViewport.right - 0x90;
        nInsetY = this->animDescMaybe.rectViewport.bottom - 0x8c;
    } else {
        nInsetX = this->animDescMaybe.rectViewport.right >> 2;
        nInsetY = this->animDescMaybe.rectViewport.bottom >> 2;
    }
    RECT rcClip;
    rcClip.left = this->animDescMaybe.rect.left + nInsetX;
    rcClip.right = this->animDescMaybe.rect.right - nInsetX;
    rcClip.top = this->animDescMaybe.rect.top + nInsetY;
    rcClip.bottom = this->animDescMaybe.rect.bottom - nInsetY;
    IntersectRect(&rcClip, &rcClip, &g_worldBoard.rcViewport);
    IDirectDrawSurface *pSurface = this->pIconToolboxBitmapMaybe->pSurface;
    RECT *pToolboxRect = &this->rectIconToolboxMaybe;
    pSurface->Blt(pToolboxRect, g_pDDrawWorkSurface, &rcClip, DDBLT_WAIT, NULL);
    if (this->animDescMaybe.pKindDesc->paFrameEntries[this->nSubFrame].Unk0x16Maybe == 1) {
        flags = 0x20;
    }
    this->animDescMaybe.pKindDesc->pOwnedObjA->RestoreOverlapBlt(
        *pToolboxRect, this->pIconToolboxBitmapMaybe->pSurface, *pToolboxRect, flags);
}

// FUNCTION: LOCO 0x42d3a0
// Real member since v552, folded alongside 0x42d280 above: this header's dial is a staircase
// and a step is FLAT once entered, so the pair's SECOND declaration was free (measured -- one
// declaration and two cost the identical 143 B). Matched as the free __fastcall form too; the
// body is a single forwarding call, which is why the convention does not show here.
// The unclipped half of the pair: composites the toolbox canvas straight onto the work
// surface at animDescMaybe's rect, sourcing from animDescMaybe's viewport rect, with the
// 0x40 clip-against-surface flag.
void SelectedObjWidgetMaybe::BlitIconToolboxMaybe(RECT rect)
{
    this->pIconToolboxBitmapMaybe->RestoreOverlapBlt(this->animDescMaybe.rect,
                                                     g_pDDrawWorkSurface,
                                                     this->animDescMaybe.rectViewport, 0x40);
}

// FUNCTION: LOCO 0x42d400
// Real vtable slot 21 (the class's only NEW slot past the base's 21; WorldBoardMaybe.cpp's
// paint pass calls it DIRECTLY with the dirty rect BY VALUE through its TU-local view).
// When active, chains the AnimDescRefObj0x477488 slot-11 blit body qualified (the original
// emits a direct call to 0x405e60, not a slot dispatch).
void SelectedObjWidgetMaybe::HideChildIfBaseFlagMaybe(RECT rect, int bFlag)
{
    if (bActive) {
        AnimDescRefObj0x477488::BlitAnimFrameMaybe(rect, (char)bFlag, 0);
    }
}

// FUNCTION: LOCO 0x42d440
// EFFECTIVE MATCH -- DIFF(277) at 550/548 bytes but 186/186 identical instructions, total
// 11554 / align=10. Two residual classes, neither source-steerable:
//   (a) an EAX<->EBP coin flip in the `nSpan` computation. Both descriptor pointers load in
//       the original's own order; only which width lands in which register differs, and cl
//       normalizes the commutative add -- `(animW >> 1) + ownW` and a cached `nOwnWidth` temp
//       both score an identical 11554.
//   (b) `add ecx,ebp` vs `lea edx,[ebp+ecx]` for `rcBoard.left + nSpan` (the +2 bytes): the
//       original clobbers the reloaded rect field as a dead temp, the candidate makes a fresh
//       one. Same family as 0x45a500's `add edi,0x83` above.
//
// SelectedObjWidgetMaybe's own vtable slot 3, transcribed here because this header is that
// class's home (same reason as HitTestConsumeMaybe below, its slot-4 neighbour).
//
// The widget and its companion animDescMaybe icon straddle a world point: normally the widget
// body sits to the LEFT of the point and the icon is centered on it. `nSpan` is how far left
// of the point the body reaches -- its own full width plus half the icon's -- and the board
// rect is the app client area pushed into world space by the current scroll, so the two
// comparisons are "is there still room on this side".
//
// When there is not, the whole assembly MIRRORS to the other side, which is what
// bSuppressRectBMaybe latches for this class. A mirror is not just a move: every menu node's
// rect has to be re-laid from its icon descriptor (measured from the widget's right edge
// inward when mirrored, from the left edge outward when not) and redrawn, and slot 7 is
// re-dispatched on the widget and the icon with the new state. The two tests are deliberately
// separate statements rather than an if/else -- unmirroring in the first can immediately make
// the second one true, and the original re-reads the flag between them to allow exactly that.
void SelectedObjWidgetMaybe::RepositionWithHotspot(int x, int y)
{
    RECT rcBoard = g_rectAppClientBounds;
    OffsetRect(&rcBoard, g_worldBoard.dwScrollX, g_worldBoard.dwScrollY);
    int nSpan = this->pKindDesc->nativeWidth + (this->animDescMaybe.pKindDesc->nativeWidth >> 1);
    if (x < 0) {
        x = 0;
    }
    if (this->bSuppressRectBMaybe && x > rcBoard.right - nSpan) {
        this->bSuppressRectBMaybe = false;
        this->ReleaseChannelAndDispatch(0);
        for (MenuNodeObj0x477568 *pNode = this->pMenuListHead; pNode != NULL;
             pNode = pNode->pNext) {
            CursorDesc *pIcon = pNode->pIconDesc;
            pNode->rect.left = this->pKindDesc->nativeWidth - pIcon->wShadowFrameWidth -
                               pIcon->field_0x2eMaybe;
            pNode->rect.right = this->pKindDesc->nativeWidth - pIcon->field_0x2eMaybe;
            pNode->Draw();
        }
        this->animDescMaybe.ReleaseChannelAndDispatch(this->bSuppressRectBMaybe);
    }
    if (!this->bSuppressRectBMaybe && x < rcBoard.left + nSpan) {
        this->bSuppressRectBMaybe = true;
        this->ReleaseChannelAndDispatch(1);
        for (MenuNodeObj0x477568 *pNode = this->pMenuListHead; pNode != NULL;
             pNode = pNode->pNext) {
            CursorDesc *pIcon = pNode->pIconDesc;
            pNode->rect.left = pIcon->field_0x2eMaybe;
            pNode->rect.right = pIcon->wShadowFrameWidth + pIcon->field_0x2eMaybe;
            pNode->Draw();
        }
        this->animDescMaybe.ReleaseChannelAndDispatch(this->bSuppressRectBMaybe);
    }
    if (this->bSuppressRectBMaybe) {
        AnimDescRefObj0x477488::RepositionWithHotspot(
            x + (this->animDescMaybe.pKindDesc->nativeWidth >> 1) - 3,
            y - (this->pKindDesc->nativeHeight >> 1));
        this->animDescMaybe.RepositionWithHotspot(
            x - (this->animDescMaybe.pKindDesc->nativeWidth >> 1),
            y - (this->animDescMaybe.pKindDesc->nativeHeight >> 1));
        return;
    }
    AnimDescRefObj0x477488::RepositionWithHotspot(x - nSpan + 3,
                                                  y - (this->pKindDesc->nativeHeight >> 1));
    this->animDescMaybe.RepositionWithHotspot(
        x - (this->animDescMaybe.pKindDesc->nativeWidth >> 1),
        y - (this->animDescMaybe.pKindDesc->nativeHeight >> 1));
}

// FUNCTION: LOCO 0x42d670
// SelectedObjWidgetMaybe's own click handler, transcribed here because this header is that
// class's home; the original's .obj for it is around 0x42d000 and is not claimed by any TU yet.
//
// An inactive widget declines outright. A widget that was being DRAGGED consumes the click and
// only ends the drag -- the release is the click, so it must not also be routed to a menu node.
// Otherwise the family base's ordinary node walk decides.
char SelectedObjWidgetMaybe::TryInvokeCallbackA(int x, int y)
{
    if (bActive != true) {
        return 0;
    }
    if (bDraggingMaybe != false) {
        bDraggingMaybe = false;
        return 1;
    }
    return WidgetBaseObj0x4784c8::TryInvokeCallbackA(x, y);
}

// FUNCTION: LOCO 0x42d6b0
// Real vtable slot 17 -- the test half of this class's category-icon menu-command pair
// (Ghidra: TestCoupleMenuNodeMaybe). Gates on bVisible + the node's own slot-2 Contains, then
// switches over the icon's resourceId: 0x3806 (deselect category) and 0x3808 arm state 2 with
// a 6-tick wSelIndexMaybe countdown; 0x3807 (re-select category) re-dispatches the current
// selection (state 1) or deselects through WorldActionCursor::SelectDecorObjAndDispatchModeMaybe.
char SelectedObjWidgetMaybe::HitTestNodeSecondary(MenuNodeObj0x477568 *pNode, int x, int y)
{
    if (pNode == NULL || pNode->bVisible == 0 || pNode->Contains(x, y) == 0) {
        return 0;
    }
    switch (pNode->pIconDesc->resourceId) {
    case 0x3806:
        if (pNode->wState == 1) {
            pNode->SetNodeState(2);
            pNode->wSelIndexMaybe = 6;
        }
        break;
    case 0x3807:
        if (pNode->wState == 1) {
            g_worldActionCursor.SelectDecorObjAndDispatchModeMaybe(this->pSelectedObjMaybe);
        } else {
            g_worldActionCursor.SelectDecorObjAndDispatchModeMaybe(NULL);
        }
        break;
    case 0x3808:
        if (pNode->wState == 1) {
            pNode->SetNodeState(2);
            pNode->wSelIndexMaybe = 6;
        }
        break;
    }
    return 1;
}

// FUNCTION: LOCO 0x42d770
// Real vtable slot 20 -- the execute half (Ghidra: HandleCoupleMenuNodeMaybe). Counts down an
// armed node's wSelIndexMaybe, then switches over the icon's resourceId: 0x3806, on countdown
// expiry from state 2, detaches the selected train from the board (releasing its queued car
// slots) after deselecting; 0x3807 re-dispatches the selection and parks the countdown, then
// mirrors the action cursor's bActive into the node state; 0x3808, same expiry gate, plain
// deselect.
char SelectedObjWidgetMaybe::HandleMenuCommandMaybe(MenuNodeObj0x477568 *pNode)
{
    if (pNode == NULL) {
        return 0;
    }
    if (pNode->wSelIndexMaybe >= 0) {
        pNode->wSelIndexMaybe--;
    }
    switch (pNode->pIconDesc->resourceId) {
    case 0x3806:
        if (pNode->wSelIndexMaybe == 0 && pNode->wState == 2) {
            PeerTrainNodePartial *pTrain;
            pNode->SetNodeState(1);
            pTrain = (PeerTrainNodePartial *)this->pSelectedObjMaybe[8].rect.top;
            if (pTrain != NULL) {
                char bOwner;
                unsigned short wTrainId;
                this->SelectObjMaybe(0);
                g_worldActionCursor.SelectDecorObjAndDispatchModeMaybe(NULL);
                bOwner = pTrain->bOwnerByteA;
                wTrainId = pTrain->wTrainId;
                g_PeerTrainSlotQueue.FreeQueuedTrainCarSlots(pTrain);
                g_PeerTrainSlotQueue.DetachFromBoardMaybe(pTrain);
                g_PeerTrainSlotQueue.ReleaseOrForwardMatchingSlotMaybe(wTrainId, bOwner, 1);
            }
        }
        break;
    case 0x3807:
        if (pNode->wSelIndexMaybe >= 0) {
            g_worldActionCursor.SelectDecorObjAndDispatchModeMaybe(this->pSelectedObjMaybe);
            pNode->wSelIndexMaybe = -1;
        }
        if (g_worldActionCursor.bActive) {
            pNode->SetNodeState(2);
        } else {
            pNode->SetNodeState(1);
        }
        return 1;
    case 0x3808:
        if (pNode->wSelIndexMaybe == 0 && pNode->wState == 2) {
            pNode->SetNodeState(1);
            this->SelectObjMaybe(0);
            return 1;
        }
        break;
    }
    return 1;
}

// FUNCTION: LOCO 0x4589b0 (Ghidra: WorldActionCursor::WorldActionCursor)
// Constructor. Chains the WidgetBaseObj0x4784c8 base implicitly, default-constructs the
// eleven AnimDescRefObj0x477488 sub-icons (the animArrayMaybe[4] element thunk passes
// (-1,-1,0,0) -- the pin for the WidgetBase.h default args), then zero-stores the 14
// pointer/index fields in the original's exact order: pActiveCandidateNodeMaybe first, then
// the two menu-item groups and the couple-choice/icon-state-target groups each in REVERSE
// declaration order, then pSelectedDecorObjMaybe/nCandidateBaseMaybe. No member-init list;
// nTypeTag=0xd is a plain tail store (the base ctor already ran). Landed v512 as a BUNDLE
// with the header decl + WidgetBase.h default args + slot-16 retype (CODEGEN #78).
WorldActionCursor::WorldActionCursor()
{
    pActiveCandidateNodeMaybe = NULL;
    pCandidateDownMaybe = NULL;
    pCandidateUpMaybe = NULL;
    pAttachOrSpawnMenuItemMaybe = NULL;
    pDetachMenuItemMaybe = NULL;
    pCoupleChoiceAttachExistingMaybe = NULL;
    pCoupleChoiceNewTrainMaybe = NULL;
    pCoupleChoiceDetachMaybe = NULL;
    pCoupleChoiceAMaybe = NULL;
    pIconStateTargetCMaybe = NULL;
    pIconStateTargetBMaybe = NULL;
    pIconStateTargetAMaybe = NULL;
    pSelectedDecorObjMaybe = NULL;
    nCandidateBaseMaybe = 0;
    nTypeTag = 0xd;
}

// FUNCTION: LOCO 0x458ad0 (??_GWorldActionCursor scalar deleting dtor -- compiler-generated
// around ~WorldActionCursor() below; no source of its own)

// The `??_F` default-construction closure cl emits for the embedded AnimDescRefObj0x477488
// sub-icons: the ctor above default-constructs eight of them, and the four-element
// animArrayMaybe among them needs a vector-constructor iterator, whose per-element callback
// is this closure. Compiler-generated, no source line of its own.
//
// FUNCTION: LOCO 0x458af0 (??_FAnimDescRefObj0x477488 default-ctor closure)

// FUNCTION: LOCO 0x458b00 (Ghidra: WorldActionCursor::~WorldActionCursor)
// vtable slot 0. The only explicit statement is the ClearOwned() call -- inside a dtor
// MSVC 5 knows the dynamic type and devirtualizes it into the direct `call 0x458bb0` the
// original shows (the same fold BuildToolButton's dtor documents for SetDescriptor). The
// vtable re-stamp, the eight embedded sub-icon destructions in reverse declaration order
// (animMaybe7/6/5, the animArrayMaybe[4] vector destruction, animMaybe0) and the
// WidgetBaseObj0x4784c8 base chain are all compiler-generated under /GX.
WorldActionCursor::~WorldActionCursor()
{
    ClearOwned();
}

// FUNCTION: LOCO 0x458bb0
// Real vtable slot 15 override. Unloads the widget's own descriptor and every embedded
// sub-icon's with the (0,-1,0) "clear" triple (own descriptor FIRST, then animMaybe0, the
// mode-feedback/couple icons animMaybe5/6/7, then the animArrayMaybe[4] variant row -- the
// original's order, not declaration order), chains the base's ClearOwned, then NULLs the
// thirteen owned pointers in exactly the ctor's store order. The SetDescriptor calls all
// dispatch through each object's vtable (`call [edx+0x18]`) -- outside a dtor MSVC has no
// dynamic type to fold, so no pSelf hop is needed here (contrast BuildToolButton's dtor).
void WorldActionCursor::ClearOwned()
{
    SetDescriptor(0, -1, 0);
    animMaybe0.SetDescriptor(0, -1, 0);
    animMaybe5.SetDescriptor(0, -1, 0);
    animMaybe6.SetDescriptor(0, -1, 0);
    animMaybe7.SetDescriptor(0, -1, 0);
    for (int i = 0; i < 4; i++) {
        animArrayMaybe[i].SetDescriptor(0, -1, 0);
    }
    WidgetBaseObj0x4784c8::ClearOwned();
    pActiveCandidateNodeMaybe = NULL;
    pCandidateDownMaybe = NULL;
    pCandidateUpMaybe = NULL;
    pAttachOrSpawnMenuItemMaybe = NULL;
    pDetachMenuItemMaybe = NULL;
    pCoupleChoiceAttachExistingMaybe = NULL;
    pCoupleChoiceNewTrainMaybe = NULL;
    pCoupleChoiceDetachMaybe = NULL;
    pCoupleChoiceAMaybe = NULL;
    pIconStateTargetCMaybe = NULL;
    pIconStateTargetBMaybe = NULL;
    pIconStateTargetAMaybe = NULL;
    pSelectedDecorObjMaybe = NULL;
}

// FUNCTION: LOCO 0x45b3a0 (Ghidra: WorldActionCursor::OnKeyDownMaybe)
// Vtable slot 16 override -- the decor-candidate widget's key handler. Chains the family
// default first; if that didn't consume the key: VK_UP/VK_DOWN (0x26/0x28) press the
// candidate page up/down buttons (consumed unconditionally; the visible+idle node, if any,
// is pushed to state 2 with its wSelIndexMaybe countdown armed to 6), every other key goes
// to the active candidate node's in-place label editor (HandleTextEditKey consumes every key
// once editable). A key none of those consumed falls through to the shared
// SelectedObjWidgetMaybe singleton's own handler. Any key that WAS consumed also commits the
// (possibly freshly edited) candidate label onto the live world object: mode 6 writes it to
// the selected train's lead car (carSlots[0]'s SetNameImpl, slot 0xd), mode 7 to the
// selected decor object directly plus a category-7 resort and a candidate-page refresh, all
// other modes to the selected decor object directly.
//
// Both dispatches are real `switch` statements, and the case ORDER is load-bearing both
// times: the bodies lie default-first, then descending (default / 0x28 / 0x26 and
// default / 7 / 6), which is what the original's physical layout shows; the value chains
// (`sub eax,0x26; je; sub eax,2; je` and `sub eax,ebp; je; dec eax; je`) are the compiler's
// own sorted-chain emission. Same layout-oracle lesson as 0x457080's inner switch.
//
// EFFECTIVE MATCH (v508, DIFF(6), compiled 337 B vs 339 B, asmscore total 12005 (align=12
// reg_pen=0 identity_miss=0 byte_diff=5), insns 112/112). Everything pairs
// instruction-for-instruction except ONE dead store the original keeps and this build
// eliminates: the SelectedObjWidgetMaybe fallback epilogue's `mov bl,al` (bHandled's
// register home) right before the pops -- the value returns in al straight from the call
// either way, so it costs the 2 missing bytes and the 3 shifted diff rows. The layout
// levers that DID matter (kept, all above): the two switch dispatches with default-first /
// descending case order; the label-commit block written as a POSITIVE `if (bHandled)`
// guard with the fallback as the trailing plain-statement tail (the negative
// `if (!bHandled) { fallback; return; }` early-exit form inlines the fallback block BEFORE
// the mode switch and scores DIFF(144)); duplicating the mode switch per exit for
// cross-jumping does NOT fire here (472 B, refuted). Probes refuted (all byte-identical):
// fallback as `bHandled = ...; return bHandled;` vs `if (!bHandled) { bHandled = ...; }
// return bHandled;` vs routing through a second `bool bSelHandled` local -- the dead-store
// elimination is cl's own coin-flip, same class as 0x40c3d0's `mov al,bl` (PARKED).
// UiIconListItem::HandleTextEditKey's return type is `bool` BECAUSE of this function: the
// plain un-normalized `mov bl,al` after its call pins it (see src/MenuNode.h).
// Retry only if the redundant-move-elimination coin-flip class ever cracks.
//
bool WorldActionCursor::OnKeyDownMaybe(unsigned int nKey)
{
    bool bHandled = WidgetBaseObj0x4784c8::OnKeyDownMaybe(nKey);
    if (!bHandled) {
        switch (nKey) {
        default:
            bHandled = pActiveCandidateNodeMaybe->HandleTextEditKey(nKey);
            break;
        case 0x28: // VK_DOWN
            bHandled = 1;
            if (pCandidateDownMaybe != NULL && pCandidateDownMaybe->bVisible != 0 &&
                pCandidateDownMaybe->wState == 1) {
                pCandidateDownMaybe->SetNodeState(2);
                pCandidateDownMaybe->wSelIndexMaybe = 6;
            }
            break;
        case 0x26: // VK_UP
            bHandled = 1;
            if (pCandidateUpMaybe != NULL && pCandidateUpMaybe->bVisible != 0 &&
                pCandidateUpMaybe->wState == 1) {
                pCandidateUpMaybe->SetNodeState(2);
                pCandidateUpMaybe->wSelIndexMaybe = 6;
            }
            break;
        }
    }
    if (bHandled) {
        switch (nModeMaybe) {
        default:
            ((CarNetObjVtblProbe *)pSelectedDecorObjMaybe)
                ->SetNameImpl(pActiveCandidateNodeMaybe->GetLabelText());
            break;
        case 7:
            ((CarNetObjVtblProbe *)pSelectedDecorObjMaybe)
                ->SetNameImpl(pActiveCandidateNodeMaybe->GetLabelText());
            DecorObjMgrMaybe_00485448.TickCategory7OnlyMaybe();
            this->RefreshDecorCategoryCandidatesMaybe(this->nCandidateBaseMaybe);
            break;
        case 6: {
            PeerTrainNodePartial *pTrain =
                (PeerTrainNodePartial *)pSelectedDecorObjMaybe[8].rect.top;
            ((CarNetObjVtblProbe *)pTrain->carSlots[0])
                ->SetNameImpl(pActiveCandidateNodeMaybe->GetLabelText());
            break;
        }
        }
        return bHandled;
    }
    if (!bHandled) {
        bHandled = SelectedObjWidgetMaybe_004852a0.OnKeyDownMaybe(nKey);
    }
    return bHandled;
}

#ifdef LOCO_PORT
// ─── PORT SCAFFOLDING (no original counterpart) ────────────────────────────────
// XC 10 and 11 of 13: g_worldActionCursor (DAT_004a9ef0), WorldActionCursor::WorldActionCursor
// (0x4589b0), and SelectedObjWidgetMaybe_004852a0 (DAT_004852a0),
// SelectedObjWidgetMaybe::SelectedObjWidgetMaybe (0x42cce0). Both ctors live in this TU;
// init_globals.cpp still calls them separately, in the XC table's order.
//
// The original constructs this global from the CRT's C++ dynamic-initializer table (.CRT$XC),
// which the port's zero-filled .bss mirror has no equivalent of. Declared in
// port/PortGlobalCtors.h, called from link/init_globals.cpp -- see either for the full story.
#include <new.h>
#include "PortGlobalCtors.h"

void Port_Construct_g_worldActionCursor(void) {
    new (&g_worldActionCursor) WorldActionCursor();
}

void Port_Construct_SelectedObjWidget(void) {
    new (&SelectedObjWidgetMaybe_004852a0) SelectedObjWidgetMaybe();
}
#endif // LOCO_PORT
