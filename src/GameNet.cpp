// Phase 4: the GameNet worker-thread inbound-message TU (contiguous .text cluster
// 0x438e40-0x43d096, sitting just below the DPlaySessionMgr.cpp TU at 0x43d0a0). This is
// the receive/dispatch side of multiplayer: GameNetThread_TickLoop pumps DirectPlay, and
// GameNet_DispatchMessage fans each inbound packet out to a per-opcode handler that mutates
// the GameNetThreadState peer/train lists and posts local-queue notifications
// (g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode) for the UI/game thread.
//
// The handlers are __thiscall methods on the manager (GameNetThreadState, the object
// threaded as `this` through GameNetThread_TickLoop -> GameNet_DispatchMessage). Ghidra boxes
// them in a plain `GameNet` namespace with an explicit void* this; they are members here so the
// thiscall convention (extra args on the stack, not edx) reproduces exactly.
//
// Transcribed so far: the rehome-filter walk and the two peer-train list drains.

#include <windows.h>           // HANDLE, CloseHandle (player-record file-transfer handles)
#include <shellapi.h>          // ShellExecuteA (DPlay_UiConnectHandler's URL-launch path)
#include <ddraw.h>             // IDirectDrawSurface (PostBag.h clipart-cache surfaces)
#include <stdlib.h>            // rand()
#include "GameNet.h"
#include "GameNetMsgQueue.h"   // NetMsgQueueNode
#include "LockableMaybe.h"     // g_pGameNetMsgQueueLock's real class (Lock/Unlock, 0x449410/0x449420)
#include "DPlaySessionMgr.h"   // g_pDPlaySessionMgr, GameNetQueuedNodeMaybe, EnqueueOrProcessLocalNode
#include "PeerTrainNode.h"     // PeerTrainNodePartial
#include "NetSessionEventQueue.h" // g_NetSessionEventQueue + the Pair16 edge-placement quartet
#include "CarNetObj.h"         // CarNetObj (RemovePeerTrainsForPlayer apply-net-state)
#include "GNetManager.h"       // GNetManager, g_pNetManager (teardown DPlay transport + free)
#include "PostBag.h"           // PostBagCacheBundle, g_pPostBagCache (BeginFileTransfer path build)
#include "LocalPlayerIdentity.h"  // g_pLocalPlayerIdentity, Profile_SavePlayerUserFile
#include "AppWindow.h"           // g_pApp
#include "IniFile.h"       // IniFile, g_pIniFile (DPlay_UiConnectHandler's Protocol/Name lookup)
#include "CarNetState.h"   // CarNetState (DPlay_UiConnectHandler's fresh local-player card)
#include "UIResources.h"   // g_UIResources (DPlay_UiConnectHandler's default description string)
#include "SplashWnd.h"     // g_pSplashWnd->pApplSetupWnd (AttemptJoinOrHostSession's session name)
#include "ApplSetupWnd.h"  // pSelectedNodeTextMaybe / pSelectedNodeTextSecondaryMaybe


#define CLSCTX_INPROC_SERVER 1


// Safe-mode flag (/s cmdline arg; see docs/subsystems.md's command-line-parser entry) -- also
// gates whether GameNet_ConnectOrJoinSession ever attempts a real DirectPlay connect at all
// (==1: drain the pending-train list immediately without connecting) or proceeds to try
// (!=1: normal play, actually establish/join a session). Not yet promoted past its Ghidra
// auto-name (raw DAT_ symbol kept here so lint_ghidra_sync sees it as an in-sync alias).
extern "C" int DAT_004a9918;  // TODO: idiom

// FUNCTION: LOCO 0x438e40
// Walk a just-received player card and ask its owner for every piece of clip-art we can't
// resolve locally: the up-to-128 decal slots (deduplicated into a temporary list, since one
// card usually stamps the same sticker many times) plus the two corner "postmark" badges.
// A piece counts as present if the RF archive has it OR a loose file exists; otherwise a 6-byte
// opcode-0x3ed request goes to the card's owner and the pending-receive count goes up, so
// GameNet_DispatchMessage's 0x3ee handler can tear the connection down once the last one lands.
// Gated entirely on connectionMode == 1 (a real connected session).
//
// ⚠ The `bIdx`/`bDesc` locals in each of the three request blocks are LOAD-BEARING, not noise:
// they are what makes VC5 read the two selector bytes BEFORE the `operator new` call (parking
// one in ebx and spilling the other), which is exactly the shape the original has. Writing the
// stores as direct `pMsg->bIndexByte = pHead->bIndexByte;` reads the fields AFTER the call
// instead and costs 2 instructions per block (177207 vs 0 on asmscore) -- verified by removing
// them from blocks 2/3 alone. The give-away is that the original also loads `bl` from
// pState->byStampSlotA before `push 6; call new` in block 2, where nothing else would force it.
// This reads exactly like the argument evaluation of a small `inline` "send one request" helper
// the original had; the whole-block "check + request" helper is REFUTED (that shape would have
// to spill/reload the index byte across BuildClipartFilePath, and the original re-loads it fresh
// from pState instead).
void GameNetThreadState::NetResource_RequestMissingAppearances(CarNetState *pState) {
    char szPath[0x504];

    MissingClipartNode *pHead = 0;
    if (g_pDPlaySessionMgr->connectionMode == 1) {
        for (unsigned int i = 0; i < 128; i++) {
            if (pState->decalSlots[i].placementSeq == 0)
                break;
            unsigned char bDup = 0;
            for (MissingClipartNode *pScan = pHead; pScan != 0; pScan = pScan->pNext) {
                if (pState->decalSlots[i].packedKind == pScan->bDescByte &&
                    pState->decalSlots[i].placementSeq == pScan->bIndexByte) {
                    bDup = 1;
                    break;
                }
            }
            if (!bDup) {
                MissingClipartNode *pNew = new MissingClipartNode;
                pNew->bDescByte = pState->decalSlots[i].packedKind;
                pNew->bIndexByte = pState->decalSlots[i].placementSeq;
                pNew->pNext = pHead;
                pHead = pNew;
            }
        }

        while (pHead != 0) {
            g_pPostBagCache->PostBag_BuildClipartFilePath(pHead->bDescByte, pHead->bIndexByte, szPath);
            unsigned char bFound = 0;
            if (g_RFIndex.IsOpen()) {
                int nRfSize;
                void *pRfBuf = g_RFIndex.LoadResource(
                    (const unsigned char *)(szPath + strlen(g_pInstallPathPrefix)), &nRfSize);
                if (pRfBuf != 0) {
                    bFound = 1;
                    ::operator delete(pRfBuf);
                }
            }
            if (!bFound) {
                if (GetFileAttributesA(szPath) != 0xffffffff)
                    bFound = 1;
                if (!bFound) {
                    unsigned char bIdx = pHead->bIndexByte;
                    unsigned char bDesc = pHead->bDescByte;
                    ClipartRequestWireMsg *pMsg = new ClipartRequestWireMsg;
                    pMsg->wOpcode = 0x3ed;
                    pMsg->bIndexByte = bIdx;
                    pMsg->bDescByte = bDesc;
                    g_pNetManager->DPlay_SendMessage(dpidCurrentPlayer, pMsg, 6, 1);
                    delete pMsg;
                    nPendingFileReceiveCount++;
                }
            }
            MissingClipartNode *pTemp = pHead;
            pHead = pHead->pNext;
            delete pTemp;
        }

        if (pState->byStampSlotA != 0) {
            unsigned char byPacked = g_pPostBagCache->PostBag_PackDecalKind(0x1e, pState->byStampVariantA);
            g_pPostBagCache->PostBag_BuildClipartFilePath(byPacked, pState->byStampSlotA, szPath);
            unsigned char bFound = 0;
            if (g_RFIndex.IsOpen()) {
                int nRfSize;
                void *pRfBuf = g_RFIndex.LoadResource(
                    (const unsigned char *)(szPath + strlen(g_pInstallPathPrefix)), &nRfSize);
                if (pRfBuf != 0) {
                    bFound = 1;
                    ::operator delete(pRfBuf);
                }
            }
            if (!bFound) {
                if (GetFileAttributesA(szPath) != 0xffffffff)
                    bFound = 1;
                if (!bFound) {
                    unsigned char bIdx = pState->byStampSlotA;
                    ClipartRequestWireMsg *pMsg = new ClipartRequestWireMsg;
                    pMsg->wOpcode = 0x3ed;
                    pMsg->bIndexByte = bIdx;
                    pMsg->bDescByte = byPacked;
                    g_pNetManager->DPlay_SendMessage(dpidCurrentPlayer, pMsg, 6, 1);
                    delete pMsg;
                    nPendingFileReceiveCount++;
                }
            }
        }

        if (pState->byStampSlotB != 0) {
            unsigned char byPacked = g_pPostBagCache->PostBag_PackDecalKind(0x1f, 1);
            g_pPostBagCache->PostBag_BuildClipartFilePath(byPacked, pState->byStampSlotB, szPath);
            unsigned char bFound = 0;
            if (g_RFIndex.IsOpen()) {
                int nRfSize;
                void *pRfBuf = g_RFIndex.LoadResource(
                    (const unsigned char *)(szPath + strlen(g_pInstallPathPrefix)), &nRfSize);
                if (pRfBuf != 0) {
                    bFound = 1;
                    ::operator delete(pRfBuf);
                }
            }
            if (!bFound) {
                if (GetFileAttributesA(szPath) != 0xffffffff)
                    bFound = 1;
                if (!bFound) {
                    unsigned char bIdx = pState->byStampSlotB;
                    ClipartRequestWireMsg *pMsg = new ClipartRequestWireMsg;
                    pMsg->wOpcode = 0x3ed;
                    pMsg->bIndexByte = bIdx;
                    pMsg->bDescByte = byPacked;
                    g_pNetManager->DPlay_SendMessage(dpidCurrentPlayer, pMsg, 6, 1);
                    delete pMsg;
                    nPendingFileReceiveCount++;
                }
            }
        }
    }
}

// FUNCTION: LOCO 0x4391a0
// Tear down the old DirectPlay net-manager singleton (if any) and construct a fresh one: destroy
// and free g_pNetManager, briefly yield (Sleep(1)) to let the worker thread notice, then allocate
// a new 0x160c-byte GNetManager, DPlayLobby_Init it with our context arg, and re-seed its parent
// hwnd from the worker context. /GX new-alloc-protection scaffolding wraps the construction.
void GameNetThreadState::GameNetThread_ResetNetManager() {
    if (g_pNetManager != 0) {
        delete g_pNetManager;
        g_pNetManager = 0;
        Sleep(1);
    }
    GNetManager *pMgr = new GNetManager(this->hInstance, 0);
    g_pNetManager = pMgr;
    pMgr->pfnErrorTextHookMaybe = 0;
    pMgr->bShowErrorMessageBoxMaybe = 0;
    g_pNetManager->hWndParent = this->hwndOwner;
}

// FUNCTION: LOCO 0x43a6d0
// Walk the rehomed-train list (manager +0x1c) and, for every node owned by provider slot nSlot,
// unlink it. If nSlot is the local player's own selected slot, re-stamp its secondary owner byte
// and move it onto the active-train list (+0x14); otherwise release it outright (virtual
// scalar-deleting dtor). After each removal, re-read the walk pointer from the just-patched link.
void GameNetThreadState::GameNet_RemoveOrRehomeNode(unsigned int nSlot) {
    unsigned int nSelfSlot = g_pDPlaySessionMgr->selectedProviderIndex;
    PeerTrainNodePartial *pNode = pTrainListRehomed;
    PeerTrainNodePartial *pPrev = 0;
    while (pNode != 0) {
        if (pNode->bOwnerByteA == nSlot) {
            if (pPrev != 0) {
                pPrev->pNext = pNode->pNext;
                if (nSlot == nSelfSlot) {
                    pNode->bOwnerByteB = (unsigned char)nSelfSlot;
                    pNode->pNext = pTrainListActive;
                    pTrainListActive = pNode;
                    pNode = (PeerTrainNodePartial *)pPrev->pNext;
                } else {
                    if (pNode != 0)
                        delete (GameNetQueuedNodeMaybe *)pNode;
                    pNode = (PeerTrainNodePartial *)pPrev->pNext;
                }
            } else {
                pTrainListRehomed = (PeerTrainNodePartial *)pNode->pNext;
                if (nSlot == nSelfSlot) {
                    pNode->bOwnerByteB = (unsigned char)nSelfSlot;
                    pNode->pNext = pTrainListActive;
                    pTrainListActive = pNode;
                    pNode = pTrainListRehomed;
                } else {
                    if (pNode != 0)
                        delete (GameNetQueuedNodeMaybe *)pNode;
                    pNode = pTrainListRehomed;
                }
            }
        } else {
            pPrev = pNode;
            pNode = (PeerTrainNodePartial *)pNode->pNext;
        }
    }
}

// FUNCTION: LOCO 0x43b240
// Inbound msg 0x3f2 ("train state sync", send side: GameNet_SendTrainStateSync). Spawn a
// fresh PeerTrainNode from the wire header, stamp heading/train id/owner/selected-car/reversed
// flag, then for each of the wire's bCarCount records: alloc a car slot (kind/sub-kind from
// the record) and, if the record carries detail (bHasDetailFlagMaybe), unpack it into a scratch
// CarNetState and apply it to the next detailed car slot. A detailed record whose
// CarNetState::wAttachmentId id is nonzero AND whose nameA matches our own currently-selected
// provider triggers an outbound appearance-request (opcode 0x3fb) to whichever provider slot's
// name matches nameB, tracked via a new inbound-transfer record; on failure (no match found)
// wAttachmentId is cleared instead. After the record loop, name car slot 0 from the wire's fixed
// trailing name span (records[3]'s reserved space, reused when fewer than 4 records are sent).
// If the sync's owner is our own selected provider, drop any existing active-list entry with the
// same train id (it's being superseded by this fresh one). Finally send a state-ack (opcode
// 0x3f3) back to every peer and post the new train as a type-0x11 local-queue notify.
//
// EFFECTIVE MATCH (asmscore byte_diff 56 at true len 1162, 337/338 insns). Real fixes banked:
// hoisting the record-loop counter `i` above the dwReversed store (source-order lever);
// re-reading the AllocNextAttId-echo dword from `pReq[3]` at each use instead of caching it in a
// local (Yoda #19 aliasing family -- the original re-loads it twice); `PostBag_AllocNextAttId`
// reclassified to a `PostBagCacheBundle::` member (this-in-ecx-but-never-read class, same as
// PostBag_BuildAttFilePath/BuildClipartFilePath); the active-list unlink loop needed a plain
// `while` (not `if`+`do-while`) with `pPrev != 0` (not `== 0`) as the fall-through arm; the two
// `(unsigned char)g_pDPlaySessionMgr->selectedProviderIndex` narrowing stores each needed an
// intermediate `unsigned int` local declared immediately before the store (the byte-spill-width
// lesson) to keep the original's full dword load instead of a byte load. Residual: (a) a handful
// of `this`-register-role swaps in the roster-provider-slot scan setup, the well-documented
// intrinsic symmetric-register-swap class (Yoda #29/#30); (b) the appearance-request success/
// failure branch's fall-through polarity -- tried both `if(pFound==0)`/`if(pFound!=0)` source
// orders, the natural one scores lower (387 vs 533 total DIFF bytes) but neither reproduces the
// original's own choice, matching the documented "genuinely not source-steerable" guard-clause
// counter-example (CLAUDE.md, PostBag_SaveCardToCategoryMaybe); (c) a redundant `pCur != 0` recheck
// immediately before the scalar-deleting-dtor call, provably dead in both binaries (pCur was just
// matched in the loop) -- kept by the original, folded away by our compiler, the documented
// intrinsic fold-vs-keep optimizer-strength class. Not re-probed further per the triage budget.
void GameNetThreadState::GameNet_HandleTrainStateSync(TrainSyncWireMsg *pWire, int nSenderId) {
    CarNetState state;
    PeerTrainNodePartial *pTrain = new PeerTrainNodePartial(pWire->nKindId, 2, 1, 1);
    pTrain->wHeading = pWire->wHeading;
    pTrain->wTrainId = pWire->wTrainId;
    pTrain->bOwnerByteA = pWire->bOwnerByteA;
    pTrain->PeerTrainNode_UpdateSelectedCar(pWire->wSelectedCar);
    int i = 0;
    pTrain->dwReversed = pWire->dwReversed;

    if (0 < pWire->bCarCount) {
        TrainSyncCarRecord *pRec = pWire->records;
        int j = 1;
        do {
            pTrain->PeerTrainNode_AllocCarSlot(pRec->nCarTypeId, pRec->nCarCategory, 1);
            if (pRec->bHasDetail != 0) {
                state.wSignature = pRec->wSignature;
                state.ownerClientId = pRec->ownerClientId;
                state.nPostSeqId = pRec->nPostSeqId;
                memcpy(state.nameA, pRec->nameA, sizeof(state.nameA));
                memcpy(state.nameB, pRec->nameB, sizeof(state.nameB));
                state.wAttachmentId = pRec->wAttachmentId;
                state.bAttachmentSoundPlayedMaybe = pRec->bAttachmentSoundPlayedMaybe;
                state.byIdentityColorR = pRec->byIdentityColorR;
                state.byIdentityColorG = pRec->byIdentityColorG;
                state.byIdentityColorB = pRec->byIdentityColorB;
                memcpy(state.szDescription, pRec->szDescription, sizeof(state.szDescription));
                state.byStampSlotB = pRec->byStampSlotB;
                state.byStampSlotA = pRec->byStampSlotA;
                state.byStampVariantA = pRec->byStampVariantA;
                memcpy(state.decalSlots, pRec->decalSlotsRaw, sizeof(state.decalSlots));
                state.Unk0x398 = pRec->UnkTrailingMaybe;

                if (state.wAttachmentId != 0) {
                    if (strcmp(state.nameA, g_pDPlaySessionMgr->GetSelectedProvider()->sAddressOrName) == 0) {
                        unsigned short *pReq = (unsigned short *)operator new(8);
                        pReq[0] = 0x3fb;
                        pReq[2] = state.wAttachmentId;
                        pReq[3] = g_pPostBagCache->PostBag_AllocNextAttId();
                        DPlaySessionMgrProviderSlot *pFound = 0;
                        for (int k = 0; k < g_pDPlaySessionMgr->field_0x8; k++) {
                            if (g_pDPlaySessionMgr->aProviderSlots[k].providerId != 0 &&
                                strcmp(state.nameB, g_pDPlaySessionMgr->aProviderSlots[k].sAddressOrName) == 0) {
                                pFound = &g_pDPlaySessionMgr->aProviderSlots[k];
                                break;
                            }
                        }
                        if (pFound == 0) {
                            state.wAttachmentId = 0;
                        } else {
                            g_pNetManager->DPlay_SendMessage(pFound->providerId, pReq, 8, 1);
                            state.wAttachmentId = pReq[3];
                            pTrain->bAckCounterA = pTrain->bAckCounterA + 1;
                            FileTransferNode *pXfer = new FileTransferNode;
                            pXfer->dwPeerId = pFound->providerId;
                            pXfer->hFile = 0;
                            pXfer->wAttId = pReq[3];
                            pXfer->wXferId = pReq[3];
                            pXfer->bBlockStage = 0;
                            pXfer->blockCount = 0;
                            pXfer->pNext = 0;
                            pXfer->bOwnerByteA = pTrain->bOwnerByteA;
                            pXfer->wTrainId = pTrain->wTrainId;
                            pXfer->pNext = pInboundTransfers;
                            pInboundTransfers = pXfer;
                        }
                        operator delete(pReq);
                    }
                }
                ((CarNetObj *)pTrain->carSlots[j])->CarNetObj_ApplyNetState(&state);
                j++;
            }
            i++;
            pRec++;
        } while (i < pWire->bCarCount);
    }

    ((CarNetObjVtblProbe *)pTrain->carSlots[0])->SetNameImpl((const char *)&pWire->records[3]);

    if ((unsigned int)pTrain->bOwnerByteA == (unsigned int)g_pDPlaySessionMgr->selectedProviderIndex) {
        pTrain->nDiscardFlag = 0;
        PeerTrainNodePartial *pPrev = 0;
        PeerTrainNodePartial *pCur = pTrainListActive;
        while (pCur != 0) {
            if (pCur->wTrainId == pTrain->wTrainId) {
                if (pPrev != 0)
                    pPrev->pNext = pCur->pNext;
                else
                    pTrainListActive = (PeerTrainNodePartial *)pCur->pNext;
                ((PeerTrainNodeVtblProbe *)pCur)->ScalarDeletingDtor(1);
                break;
            }
            pPrev = pCur;
            pCur = (PeerTrainNodePartial *)pCur->pNext;
        }
    }

    TrainStateAckWireMsg *pAck = (TrainStateAckWireMsg *)operator new(sizeof(TrainStateAckWireMsg));
    pAck->wOpcode = 0x3f3;
    pAck->wTrainId = pTrain->wTrainId;
    pAck->bOwnerByteA = pTrain->bOwnerByteA;
    unsigned int nSelfIndex = g_pDPlaySessionMgr->selectedProviderIndex;
    pAck->bOwnerByteB = (unsigned char)nSelfIndex;
    pAck->wHeading = pTrain->wHeading;
    g_pNetManager->DPlay_SendMessage(0, pAck, sizeof(TrainStateAckWireMsg), 1);
    operator delete(pAck);

    unsigned int nSelfIndex2 = g_pDPlaySessionMgr->selectedProviderIndex;
    pTrain->bOwnerByteB = (unsigned char)nSelfIndex2;
    pTrain->bAckCounterB = 0;
    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 0x11;
    pNode->pPayload = pTrain;
    pTrain->bHasDetailFlagMaybe = 0;
    g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);
}

// FUNCTION: LOCO 0x43b6d0
// Apply an inbound train-state ack: find the active-list train matching the message's train id and
// source owner, stamp it with the acking peer's id (+0x8c) and secondary owner byte, then always
// post a type-0x12 notification echoing the message's id/owner/count to the game thread.
void GameNetThreadState::GameNet_HandleTrainStateAck(TrainStateWireMsg *pMsg, int nAckPlayerId) {
    PeerTrainNodePartial *pTrain = pTrainListActive;
    while (pTrain != 0) {
        if (pTrain->wTrainId == pMsg->wTrainId &&
            pTrain->bOwnerByteA == pMsg->bOwnerByteA) {
            pTrain->dwAckPlayerId = nAckPlayerId;
            pTrain->bOwnerByteB = pMsg->bOwnerByteB;
            break;
        }
        pTrain = (PeerTrainNodePartial *)pTrain->pNext;
    }
    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->pPayload = 0;
    pNode->type = 0x12;
    pNode->eventTrainId = (unsigned short)pMsg->wTrainId;
    pNode->bEventOwnerA = pMsg->bOwnerByteA;
    pNode->bEventOwnerB = pMsg->bOwnerByteB;
    pNode->payloadLen = (unsigned short)pMsg->wCount;
    g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);
}

// FUNCTION: LOCO 0x43a4b0
// Handle a "rotate self train 180deg" inbound message: post a type-0x17 notification carrying the
// message as payload, then -- only if the message targets the local player's own provider slot --
// find the matching active train, unlink it, flip its heading a half turn (0<->180, 90<->270),
// re-stamp its secondary owner byte with the local slot, and post it as a type-0x11 notification.
// EFFECTIVE MATCH: /O2 peels the find-first search loop (a 2-exit mid-exit loop: match-test at top,
// null-check at bottom) -- a duplicated match test + the no-match return tail-merged into the shared
// epilogue instead of kept inline. Confirmed intrinsic: while / for(;;) / do-while+goto all compile
// byte-identically to the peeled form (same class as GameNet_DrainPendingTrainQueue 0x43e010).
void GameNetThreadState::GameNet_HandleSelfStateRotate(TrainRotateWireMsg *pMsg, int nReliable) {
    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 0x17;
    pNode->pPayload = pMsg;
    pNode->bReliable = nReliable;
    g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);

    if ((unsigned int)pMsg->bOwner == (unsigned int)g_pDPlaySessionMgr->selectedProviderIndex) {
        PeerTrainNodePartial *pTrain = pTrainListActive;
        if (pTrain != 0) {
            while ((unsigned int)pTrain->wTrainId != (unsigned int)pMsg->nTrainId) {
                pTrain = (PeerTrainNodePartial *)pTrain->pNext;
                if (pTrain == 0)
                    return;
            }
            pTrainListActive = (PeerTrainNodePartial *)pTrain->pNext;
            unsigned short wHeading = pTrain->wHeading;
            pTrain->pNext = 0;
            switch (wHeading) {
            case 0:     pTrain->wHeading = 0xb4;  break;
            case 0x5a:  pTrain->wHeading = 0x10e; break;
            case 0xb4:  pTrain->wHeading = 0;     break;
            case 0x10e: pTrain->wHeading = 0x5a;  break;
            }
            unsigned int nSelfSlot = g_pDPlaySessionMgr->selectedProviderIndex;
            pTrain->bOwnerByteB = (unsigned char)nSelfSlot;
            NetMsgQueueNode *pNodeB = new NetMsgQueueNode();
            pNodeB->type = 0x11;
            pNodeB->pPayload = pTrain;
            pTrain->bHasDetailFlagMaybe = 0;
            g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNodeB);
        }
    }
}

// FUNCTION: LOCO 0x43a5c0
// Handle a player leaving the session: resolve their DPID to a provider slot, reset our
// self-player DPID if it was theirs, rehome/remove their trains (RemoveOrRehomeNode +
// RemovePeerTrainsForPlayer), then walk the per-player inbound-file record list unlinking every
// record owned by the leaving player -- closing its transfer handle, posting a type-0x18 notify,
// and freeing the node (restarting the walk from the head after each removal). Finally post a
// type-0xb "player left" notification carrying the player's DPID.
void GameNetThreadState::GameNet_HandlePlayerLeft(int nPlayerId) {
    FileTransferNode *pPrev = 0;
    unsigned int nSlot = g_pDPlaySessionMgr->ResolveIdToSlot(nPlayerId);
    if (nPlayerId == dpidCurrentPlayer) {
        dpidCurrentPlayer = 1;
    }
    GameNet_RemoveOrRehomeNode(nSlot);
    GameNet_RemovePeerTrainsForPlayer(nPlayerId);

    FileTransferNode *pRec = pInboundTransfers;
    while (pRec != 0) {
        if (pRec->dwPeerId == nPlayerId) {
            if (pPrev != 0) {
                pPrev->pNext = pRec->pNext;
            } else {
                pInboundTransfers = pRec->pNext;
            }
            HANDLE hFile = pRec->hFile;
            pRec->pNext = 0;
            if (hFile != 0) {
                CloseHandle(hFile);
                pRec->hFile = 0;
            }
            NetMsgQueueNode *pNode = new NetMsgQueueNode();
            pNode->type = 0x18;
            pNode->pNext = 0;
            pNode->bReliable = (unsigned int)pRec->wTrainId;
            pNode->bEventOwnerA = pRec->bOwnerByteA;
            g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);
            delete pRec;
            pPrev = 0;
            pRec = pInboundTransfers;
        } else {
            pPrev = pRec;
            pRec = pRec->pNext;
        }
    }
    NetMsgQueueNode *pNodeLeft = new NetMsgQueueNode();
    pNodeLeft->type = 0xb;
    pNodeLeft->pPayload = 0;
    pNodeLeft->destPlayerId = nPlayerId;
    g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNodeLeft);
}

// FUNCTION: LOCO 0x43b770
// Remove every active train belonging to a departing/target player (host-only, connectionMode==2):
// resolve the player id to a provider slot (or use our own selected slot when nPlayerId==0), then
// walk the active-train list. For each train owned by that slot (secondary owner when nPlayerId!=0,
// primary owner when nPlayerId==0) or acked by that player, unlink it, flip its heading a half turn,
// re-stamp its owner byte, clear the net-state of each of its cars, and post a type-0x11 notify --
// restarting the walk from the head after each removal.
void GameNetThreadState::GameNet_RemovePeerTrainsForPlayer(int nPlayerId) {
    if (g_pDPlaySessionMgr->connectionMode == 2) {
        int nSlot;
        if (nPlayerId != 0) {
            nSlot = g_pDPlaySessionMgr->ResolveIdToSlot(nPlayerId);
        } else {
            nSlot = g_pDPlaySessionMgr->selectedProviderIndex;
        }
        if (nSlot >= 0) {
            PeerTrainNodePartial *pTrain = pTrainListActive;
            while (pTrain != 0) {
                if ((pTrain->bOwnerByteB == nSlot && nPlayerId != 0) ||
                    (pTrain->bOwnerByteA == nSlot && nPlayerId == 0) ||
                    (pTrain->dwAckPlayerId == nPlayerId)) {
                    pTrainListActive = (PeerTrainNodePartial *)pTrain->pNext;
                    unsigned short wHeading = pTrain->wHeading;
                    pTrain->pNext = 0;
                    switch (wHeading) {
                    case 0:     pTrain->wHeading = 0xb4;  break;
                    case 0x5a:  pTrain->wHeading = 0x10e; break;
                    case 0xb4:  pTrain->wHeading = 0;     break;
                    case 0x10e: pTrain->wHeading = 0x5a;  break;
                    }
                    unsigned int nSelfSlot = g_pDPlaySessionMgr->selectedProviderIndex;
                    pTrain->bOwnerByteB = (unsigned char)nSelfSlot;
                    for (int i = 1; i <= pTrain->wCarSlotCount; i++) {
                        ((CarNetObj *)pTrain->carSlots[i])->CarNetObj_ApplyNetState(NULL);
                    }
                    NetMsgQueueNode *pNode = new NetMsgQueueNode();
                    pNode->type = 0x11;
                    pNode->pPayload = pTrain;
                    pTrain->bHasDetailFlagMaybe = 0;
                    g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);
                    pTrain = pTrainListActive;
                } else {
                    pTrain = (PeerTrainNodePartial *)pTrain->pNext;
                }
            }
        }
    }
}

// FUNCTION: LOCO 0x43b8c0
// The target-slot-empty half of the board-edge handoff -- TrainNet_TryBoardEdgeHandoffMaybe
// (0x43c160) picks between this and GameNet_SendTrainStateSync. "Empty" there means the
// neighbouring provider slot has no providerId; what this function then asks is whether that slot
// nevertheless holds a stored board LAYOUT, and the two answers are opposite outcomes:
//   * pLayoutData != NULL -- there IS a board over there, just no peer currently driving it. The
//     train really leaves: stamp its new owner slot, move it onto pTrainListRehomed, keep its
//     local heading as-is, broadcast the 0x3f3 train-state ack, feed that same message straight
//     back through our own inbound handler (so the local side processes the handoff it just
//     announced -- the peer that would normally reply does not exist), and re-seat the train one
//     tile inside the destination board's matching edge.
//   * pLayoutData == NULL -- nowhere to go. REVERSE the train's network heading in place
//     (0<->0xb4, 0x5a<->0x10e) and append it to pTrainListAwaitingAck, then drain that list as
//     notifications. Note this reverses wHeading (+0x74, the network hand-off heading) whereas the
//     departure path above sets wLocalHeading (+0x76, the track-following one) -- the two branches
//     deliberately touch different fields.
void GameNetThreadState::TrainNet_HandleEmptySlotHandoffMaybe(PeerTrainNodePartial *pTrain,
                                                              int nHeading, int nSlot) {
    DPlaySessionMgrProviderSlot *pSlot = g_pDPlaySessionMgr->ProviderSlotAt(nSlot);
    if (pSlot->pLayoutData == 0) { // sic: ProviderSlotAt returns NULL for nSlot<0, deref'd anyway
        // A SEPARATE temp, not a reassignment of nHeading: the original seeds ecx from the
        // argument (`mov ecx,eax`) and lets the switch overwrite only the copy, so the default
        // case falls through to a second store of the SAME value rather than skipping it.
        int nReversed = nHeading;
        pTrain->wHeading = (unsigned short)nHeading;
        switch (nHeading) {
        case 0:     nReversed = 0xb4;  break;
        case 0x5a:  nReversed = 0x10e; break;
        case 0xb4:  nReversed = 0;     break;
        case 0x10e: nReversed = 0x5a;  break;
        }
        pTrain->wHeading = (unsigned short)nReversed;
        if (pTrainListAwaitingAck != 0) {
            PeerTrainNodePartial *pTail = pTrainListAwaitingAck;
            while (pTail->pNext != 0)
                pTail = (PeerTrainNodePartial *)pTail->pNext;
            pTrain->pNext = 0;
            pTail->pNext = pTrain;
        } else {
            pTrain->pNext = 0;
            pTrainListAwaitingAck = pTrain;
        }
        GameNet_DrainBlockedTrainListAsNotify();
        return;
    }

    pTrain->bOwnerByteB = (unsigned char)nSlot;
    pTrain->pNext = pTrainListRehomed;
    pTrainListRehomed = pTrain;
    // An identity map, unlike the reversal above -- the train keeps travelling the same way, it
    // just does so on the neighbouring board. Still written as a switch (the original emits the
    // same four-way compare tree), so an out-of-range heading leaves wLocalHeading untouched.
    switch (nHeading) {
    case 0:     pTrain->wLocalHeading = 0;     break;
    case 0x5a:  pTrain->wLocalHeading = 0x5a;  break;
    case 0xb4:  pTrain->wLocalHeading = 0xb4;  break;
    case 0x10e: pTrain->wLocalHeading = 0x10e; break;
    }

    TrainStateAckWireMsg *pAck = (TrainStateAckWireMsg *)operator new(sizeof(TrainStateAckWireMsg));
    pAck->wOpcode = 0x3f3;
    pAck->wTrainId = pTrain->wTrainId;
    pAck->bOwnerByteA = pTrain->bOwnerByteA;
    pAck->bOwnerByteB = pTrain->bOwnerByteB;
    pAck->wHeading = (unsigned short)nHeading;
    g_pNetManager->DPlay_SendMessage(0, pAck, sizeof(TrainStateAckWireMsg), 1);
    GameNet_HandleTrainStateAck((TrainStateWireMsg *)pAck, g_pDPlaySessionMgr->searchProviderId);
    operator delete(pAck);

    // Same shape as DPlaySessionMgr::HandleQueuedTrainPlacement's own already-byte-matched
    // switch (0x43e370): a memory `tmp` for the by-value return and a register-resident `coord`
    // accumulator, with every step staying in Pair16's own `short` fields. Spelling the result as
    // two loose `short x/y` instead promotes each `+ 1` to int and gets `lea ecx,[eax+1]` off the
    // still-live return dword, where the original re-loads 16-bit halves and does `inc cx`.
    Pair16 coord;
    Pair16 tmp;
    coord.lo = 0;
    coord.hi = 0;
    switch (pTrain->wLocalHeading) {
    case 0:
        tmp = g_NetSessionEventQueue.ComputeBottomEdgePlacement();
        coord = tmp;
        coord.lo += 1;
        coord.hi -= 1;
        break;
    case 0x5a:
        tmp = g_NetSessionEventQueue.ComputeLeftEdgePlacement();
        coord = tmp;
        coord.hi += 1;
        coord.lo += 1;
        break;
    case 0xb4:
        tmp = g_NetSessionEventQueue.ComputeTopEdgePlacement();
        coord = tmp;
        coord.lo += 1;
        coord.hi += 1;
        break;
    case 0x10e:
        tmp = g_NetSessionEventQueue.ComputeRightEdgePlacement();
        coord = tmp;
        coord.hi += 1;
        coord.lo -= 1;
        break;
    }
    pTrain->wPosX = coord.lo;
    pTrain->wPosY = coord.hi;
    pTrain->wCheckpointPosX = -1;
    pTrain->wCheckpointPosY = -1;
    pTrain->bStallStepCounter = 0;
}

// FUNCTION: LOCO 0x43cbe0
// Drain the active peer-train list (manager +0x14): for each node, post a type-0xf notification
// carrying the node as payload, clear its detail flag, unlink it, and null its chain link.
void GameNetThreadState::GameNet_DrainPeerListAsNotify() {
    while (pTrainListActive != 0) {
        NetMsgQueueNode *pNode = new NetMsgQueueNode();
        pNode->type = 0xf;
        pNode->pPayload = pTrainListActive;
        pTrainListActive->bHasDetailFlagMaybe = 0;
        pTrainListActive = (PeerTrainNodePartial *)pTrainListActive->pNext;
        ((PeerTrainNodePartial *)pNode->pPayload)->pNext = 0;
        g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);
    }
}

// FUNCTION: LOCO 0x43cc40
// Drain the awaiting-ack peer-train list (manager +0x18): post a type-0x11 notification per node,
// clear its detail flag, stamp its secondary owner byte with the local selected-provider index,
// null its chain link + detail flag again, and unlink it.
void GameNetThreadState::GameNet_DrainBlockedTrainListAsNotify() {
    while (pTrainListAwaitingAck != 0) {
        NetMsgQueueNode *pNode = new NetMsgQueueNode();
        pNode->type = 0x11;
        pNode->pPayload = pTrainListAwaitingAck;
        pTrainListAwaitingAck->bHasDetailFlagMaybe = 0;
        PeerTrainNodePartial *pTrain = pTrainListAwaitingAck;
        unsigned int nSelIndex = g_pDPlaySessionMgr->selectedProviderIndex;
        pTrain->bOwnerByteB = (unsigned char)nSelIndex;
        pTrain->pNext = 0;
        pTrain->bHasDetailFlagMaybe = 0;
        pTrainListAwaitingAck = (PeerTrainNodePartial *)pTrainListAwaitingAck->pNext;
        g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);
    }
}

// FUNCTION: LOCO 0x43ac10
// Tear down the DirectPlay transport and flush all three peer-train lists. If a net manager exists:
// when we're in a joined host/peer session (connectionMode==2), send a bare 0x3fd "leaving" opcode and
// give it 10ms to go out; then teardown + delete the manager and null the global. Finally, when the
// session is host/local (connectionMode 2 or 0), drain the active/awaiting-ack/rehomed train lists,
// deleting every node via its virtual scalar-deleting dtor. Always returns 1.
char GameNetThreadState::GameNet_TeardownAndFlushQueues() {
    if (g_pNetManager != 0) {
        if (g_pNetManager->bSessionJoined && g_pDPlaySessionMgr->connectionMode == 2) {
            // 4-byte control message; only the low 2-byte opcode is written -- the high word (wPad)
            // is sent uninitialized (holds leftover stack, i.e. `this`). sic: word store, not dword.
            struct { unsigned short wOpcode; unsigned short wPad; } msg;
            msg.wOpcode = 0x3fd;
            g_pNetManager->DPlay_SendMessage(0, &msg, 4, 1);
            Sleep(10);
        }
        g_pNetManager->DPlay_TeardownConnection();
        delete g_pNetManager;
        g_pNetManager = 0;
    }
    if (g_pDPlaySessionMgr->connectionMode == 2 || g_pDPlaySessionMgr->connectionMode == 0) {
        PeerTrainNodePartial *p;
        while ((p = pTrainListActive) != 0) {
            pTrainListActive = (PeerTrainNodePartial *)p->pNext;
            delete (GameNetQueuedNodeMaybe *)p;
        }
        while ((p = pTrainListAwaitingAck) != 0) {
            pTrainListAwaitingAck = (PeerTrainNodePartial *)p->pNext;
            delete (GameNetQueuedNodeMaybe *)p;
        }
        while ((p = pTrainListRehomed) != 0) {
            pTrainListRehomed = (PeerTrainNodePartial *)p->pNext;
            delete (GameNetQueuedNodeMaybe *)p;
        }
    }
    return 1;
}

#pragma inline_depth(0)
// FUNCTION: LOCO 0x438bc0 (GameNetThreadState::GameNetThreadState)
// The real constructor (Ghidra: GameNetThread_InitState) -- seeds the two context fields from
// the owning SplashWnd's WindowBase::hInstance/hwndOwner, NULLs the list heads/counters, then
// (unless the screensaver/attract mode is active) rebuilds the net transport underneath itself
// (GameNetThread_ResetNetManager), snapshots DPlay_ProbeAvailableProviders' list into
// g_pNetSettings->pDetectedProviderList as FRESH 8-byte nodes (reversed -- each copy is
// cons-pushed), and fills g_pNetSettings->bComPortAvailableMaybe[0..3] from ProbeComPort on
// the one-character digits '0'..'3'. pOutboundTransfers/pInboundTransfers are NULLed LAST,
// after the probe block (the original's store order).
//
// EFFECTIVE MATCH: insns 77/77, compiled 211 B vs 210 B, DIFF(53). Instruction-for-instruction
// identical (same schedule, same store order everywhere); the entire residual is ONE
// register-allocation rotation inside the COM-port probe loop: the original allocates the
// digit char->al, the buffer-address arg->ecx, and the per-iteration g_pNetSettings
// reload->edx (`mov al,bl; lea ecx,[esp+0x10]; add al,0x30 (04 30 short form); ... mov
// [edx+esi+0x14],al`); this build rotates them one slot over (cl/edx/ecx, `add cl,0x30` has no
// short form -> the +1 B, and the byte store's SIB index/base swap). Everything upstream --
// the cons-loop temp in edx, the `a1`-form head store, the xor/store schedule -- matches
// exactly. Probes refuted (no byte change): int-i-first decl order (the original's xor order
// proves cDigit declared FIRST), named digit local, direct store without the bPortExists
// local (WORSE: moves inc-bl before the call and compensates the store to +0x13), unsigned
// counter/index, function-scope buffer (KEPT -- the original's frame is 0xc for a 2-byte
// string, so the buffer is a function-scope 12; loop-scope compiles identical).
// The `#pragma inline_depth(0)`/`(8)` bracket is LOAD-BEARING: without it VC5 inlines
// ProbeComPort at this site (the original makes a real `call` to the out-of-line COMDAT at
// 0x45ee60 -- which this TU now emits and byte-matches, 93 B). inline_depth is the only lever
// found that suppresses a single call site's /Ob1 expansion; auto_inline(off), argument
// spelling, and body placement AFTER the ctor in the TU all failed (VC5 inlines with
// whole-TU visibility).
GameNetThreadState::GameNetThreadState(void *hInstanceParam, HWND hwndOwnerParam) {
    char szPortDigit[12];
    dpidCurrentPlayer = 0;
    hInstance = hInstanceParam;
    hwndOwner = hwndOwnerParam;
    bShutdownRequestedMaybe = 0;
    pTrainListActive = 0;
    pTrainListAwaitingAck = 0;
    pTrainListRehomed = 0;
    nTickCounter = 0;
    nTrainAdvanceInterval = 0x14;
    bSkipConnectMaybe = 0;
    nPendingFileReceiveCount = 0;
    g_pNetManager = 0;
    if (DAT_004a9918 != 1) {
        GameNetThread_ResetNetManager();
        DPlayProviderNode *pNode = g_pNetManager->DPlay_ProbeAvailableProviders();
        DPlayProviderNode *pCopiedList = 0;
        for (; pNode != 0; pNode = pNode->pNext) {
            DPlayProviderNode *pCopy = new DPlayProviderNode;
            pCopy->nProviderType = pNode->nProviderType;
            pCopy->pNext = pCopiedList;
            pCopiedList = pCopy;
        }
        g_pNetSettings->pDetectedProviderList = pCopiedList;
        char cDigit = 0;
        int i = 0;
        do {
            szPortDigit[0] = cDigit + '0';
            szPortDigit[1] = 0;
            bool bPortExists = g_pNetManager->ProbeComPort(szPortDigit);
            cDigit++;
            g_pNetSettings->bComPortAvailableMaybe[i] = bPortExists;
            i++;
        } while (cDigit < 4);
    }
    pOutboundTransfers = 0;
    pInboundTransfers = 0;
}
#pragma inline_depth(8)

// FUNCTION: LOCO 0x438ca0 (??_GGameNetThreadState scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x438cc0
// Net-subsystem teardown (the class's virtual dtor, vtable 0x4781c4): stop the worker thread
// if it is still running, destroy the DirectPlay transport (g_pNetManager), drain the three
// peer-train lists (+0x14/+0x18/+0x1c) through each node's scalar-deleting dtor, then the
// global send queue under the queue lock (type-0xe payloads are polymorphic objects freed via
// their virtual dtor, everything else is a raw operator delete), and finally the two
// file-transfer lists (+0x28/+0x2c), CloseHandle-ing each node's open file handle first.
//
// EFFECTIVE MATCH: insns 136/137, compiled 367 B vs 371 B, DIFF(113). Instruction-for-
// instruction identical everywhere except the send-queue drain, whose residual is ONE coupled
// register-allocation cascade:
//   * Loop bottom: the original emits `mov eax,edi; cmp edi,ebx; mov [head],eax (a3); jne`
//     (pNode cached in eax, the head store through the a3 short form); this build emits
//     `cmp edi,ebx; mov eax,edi; mov [head],edi; jne`. Same three statements
//     (`operator delete(pNode); pNode = pNext; g_pNetMsgSendQueueHead = pNode;`), scheduler
//     coin-flip. Chained `g = pNode = pNext` compiles byte-identical (refuted).
//   * Because the original's head store goes through eax, eax == head is INVARIANT at its loop
//     top, so the payload==0/null-check path skips the head reload entirely (`je` straight to
//     the `mov [eax+8],ebx` store) and the post-delete read of the head CANNOT be CSE'd (its
//     eax has two merging definitions) -- the original keeps TWO `a1` reloads, this build one
//     (the missing 137th insn, +4 B net with the a3-vs-`89 3d` byte).
//   * The type-0xe virtual delete dispatches `mov edx,[ecx]; call [edx]` there vs
//     `mov eax,[ecx]; call [eax]` here -- the same coin-flip, downstream of the above.
// Levers that DID matter (kept): the type test spelled `!= 0xe` with operator delete as the
// fall-through arm (reproduces the original's je-to-virtual-delete layout); the payload delete
// spelled `delete (NetMsgPayloadObjMaybe *)pNode->pPayload` (reading through the node keeps
// delete's inner null check UNFOLDED -- the pPayload-local spelling folds it); and spelling
// the payload-null store as a fresh `pNode = g_pNetMsgSendQueueHead;` read BEFORE the store
// (makes the store base reload land in eax, `a1` form -- store-first ordering flips it to
// `8b 0d` ecx). Probes refuted (byte-identical): hoisting pPayload/pNext decls out of the
// loop, a named NetMsgPayloadObjMaybe local, chained vs two-statement loop bottom.
GameNetThreadState::~GameNetThreadState() {
    if (g_pGameNetThread != 0 && g_pGameNetThread->IsRunning()) {
        g_pGameNetThreadState->StopThreadAndWait();
    }
    if (g_pNetManager != 0) {
        delete g_pNetManager;
        g_pNetManager = 0;
    }
    PeerTrainNodePartial *p;
    while ((p = pTrainListActive) != 0) {
        pTrainListActive = (PeerTrainNodePartial *)p->pNext;
        delete (GameNetQueuedNodeMaybe *)p;
    }
    while ((p = pTrainListAwaitingAck) != 0) {
        pTrainListAwaitingAck = (PeerTrainNodePartial *)p->pNext;
        delete (GameNetQueuedNodeMaybe *)p;
    }
    while ((p = pTrainListRehomed) != 0) {
        pTrainListRehomed = (PeerTrainNodePartial *)p->pNext;
        delete (GameNetQueuedNodeMaybe *)p;
    }
    g_pGameNetMsgQueueLock->Lock();
    NetMsgQueueNode *pNext;
    void *pPayload;
    NetMsgQueueNode *pNode = g_pNetMsgSendQueueHead;
    while (pNode != 0) {
        pPayload = pNode->pPayload;
        pNext = pNode->pNext;
        if (pPayload != 0) {
            if (pNode->type != 0xe) {
                operator delete(pPayload);
            } else {
                delete (NetMsgPayloadObjMaybe *)pNode->pPayload;
            }
            pNode = g_pNetMsgSendQueueHead;
            g_pNetMsgSendQueueHead->pPayload = 0;
        }
        operator delete(pNode);
        pNode = pNext;
        g_pNetMsgSendQueueHead = pNode;
    }
    g_pGameNetMsgQueueLock->Unlock();
    FileTransferNode *pXfer = pOutboundTransfers;
    while (pXfer != 0) {
        if (pXfer->hFile != 0) {
            CloseHandle(pXfer->hFile);
            pOutboundTransfers->hFile = 0;
        }
        pXfer = pOutboundTransfers;
        pOutboundTransfers = pXfer->pNext;
        operator delete(pXfer);
        pXfer = pOutboundTransfers;
    }
    pXfer = pInboundTransfers;
    while (pXfer != 0) {
        if (pXfer->hFile != 0) {
            CloseHandle(pXfer->hFile);
            pInboundTransfers->hFile = 0;
        }
        pXfer = pInboundTransfers;
        pInboundTransfers = pXfer->pNext;
        operator delete(pXfer);
        pXfer = pInboundTransfers;
    }
}

// FUNCTION: LOCO 0x4394e0
// Shut down the GameNet worker thread: if it's still running, enqueue a type-8 shutdown command
// (GameNet_ProcessLocalCommand case 8 sets the stop flag + tears the session down), then spin-wait
// -- Sleep(10) per pass -- until the thread reports it has exited. Called from the clean-exit path.
void GameNetThreadState::StopThreadAndWait() {
    if (g_pGameNetThread->IsRunning()) {
        NetMsgQueueNode *pNode = new NetMsgQueueNode();
        pNode->type = 8;
        EnqueueOrFreeNode(pNode);
    }
    while (g_pGameNetThread->IsRunning()) {
        Sleep(10);
    }
}

// FUNCTION: LOCO 0x439d00
// Handle a peer's request for one of our clipart-attachment files (opcode 0x11): allocate an
// outbound FileTransferNode stamped with the requester's DPID + the requested file ids, build
// the local "PostBag<cat>\<id>.att" path, and open it GENERIC_READ/OPEN_ALWAYS. On success, append
// the node to the tail of the outbound-transfer list (manager +0x28); on open failure, discard it.
// The 0x504-byte path buffer is a plain `= ""` aggregate initializer.
// EFFECTIVE MATCH: structure is byte-identical (82/82 insns, same CFG) but /O2 picks a different
// callee-saved 3-cycle for {this,pHead,zeroConst} = {ebx,ebp,edi} vs {ebp,edi,ebx}. The one visible
// downstream tell -- the two byte-field zero stores emit an immediate `mov byte[..],0` in the
// original vs `mov byte[..],bl` here -- is a pure consequence of that: the original's zero lands in
// edi, whose low byte isn't 8-bit-addressable, forcing the immediate. Confirmed intrinsic: identical
// score in isolation (not TU-position-sensitive) and unmoved by local declaration order. Yoda #29/#30.
void GameNetThreadState::GameNet_BeginFileTransfer(FileRequestWireMsg *pMsg, unsigned int nRequesterId) {
    char szPath[0x504] = "";

    FileTransferNode *pHead = pOutboundTransfers;
    FileTransferNode *pNode = new FileTransferNode;
    pNode->dwPeerId = nRequesterId;
    pNode->hFile = 0;
    unsigned short wId = pMsg->wAttId;
    pNode->wAttId = wId;
    pNode->wXferId = pMsg->wXferId;
    pNode->bBlockStage = 0;
    pNode->blockCount = 0;
    pNode->bCooldownTicks = 0;
    pNode->pNext = 0;
    g_pPostBagCache->PostBag_BuildAttFilePath(wId, 4, szPath);
    pNode->hFile = CreateFileA(szPath, 0x80000000, 1, 0, 4, 0x8000000, 0);
    if (pNode->hFile == INVALID_HANDLE_VALUE) {
        delete pNode;
        return;
    }
    if (pOutboundTransfers == 0) {
        pOutboundTransfers = pNode;
    } else {
        for (FileTransferNode *p = pHead->pNext; p != 0; p = p->pNext)
            pHead = p;
        pHead->pNext = pNode;
    }
}

// FUNCTION: LOCO 0x43a140
// Handle an inbound file-transfer data block (opcode 0x12): a peer is streaming us a clipart file.
// Match the block to its inbound transfer node (pInboundTransfers, by wXferId); for the
// FIRST block create the local file GENERIC_WRITE/CREATE_NEW and write it; for INTERIM blocks bump
// and validate the running sequence count then write; for the FINAL block validate the count, write,
// close, and (for a .dat) rebuild the path. On any sequencing error or I/O failure, log the mismatch
// ("Attachment First/Interim/FINAL Block out of sequence"), post a type-0x18 abort notify to the game
// thread, unlink the node from the inbound list, and free it. Uses the usual dead-init path buffer.
// PARTIAL (transcribed, not yet byte-matched -- DIFF ~647). Structure now matches: v259 landed the
// compare-chain SWITCH dispatch (sub eax,0/je FIRST; dec/je INTERIM; fall to FINAL default -- the
// prior if/else form emitted the wrong cmp/and shape, score 877k->544k) and removed the FINAL-scan
// do-while peel (rewritten as a guard-wrapped while-at-top). The three notify+unlink+delete tail
// copies are modelled at the correct sites (copy1 = LAB_0043a466 for FINAL-mismatch/INTERIM-write-
// fail; copy2 inline for FIRST create-fail/out-of-seq, no hFile=0; copy3 = LAB_0043a457 for FINAL-
// count-match/FINAL-create-fail/FIRST-write-fail/INTERIM-mismatch). The residual is now essentially
// intrinsic (Yoda #29/#30 + #15): (a) a whole-function register-role cascade -- the original pins the
// 0-constant in edi and evicts pWire to scratch ecx (reloaded from its home slot after each call) so
// it can HOIST the CloseHandle IAT into callee-saved ebp for the FINAL block's 3 call sites; mine
// keeps pWire cached in edi and calls CloseHandle direct (a 5-live-values-for-4-callee-saved-regs
// tie-break -- local-decl reorder confirmed inert); (b) the three tail copies' physical block
// ordering (trace-driven layout). A dedicated register-allocation session could still close it.
void GameNetThreadState::GameNet_HandleFileTransferBlock(FileBlockWireMsg *pWire) {
    FileTransferNode *pPrev = 0;
    char szPath[0x504] = "";
    DWORD dwWritten;
    NetMsgQueueNode *pMsg;
    FileTransferNode *pNode;

    dwWritten = 0;

    // Dispatch on block type. The original compiles this as a decrement compare-chain switch
    // (sub eax,0/je FIRST; dec eax/je INTERIM; fall to FINAL default) -- a plain switch matches;
    // the if/else form emitted the wrong cmp/and dispatch shape.
    switch (pWire->bBlockType) {
    case 1:
        // INTERIM block: bump + validate sequence, write.
        pNode = pInboundTransfers;
        if (pNode == 0)
            return;
        while (pNode->wXferId != pWire->wXferId) {
            pPrev = pNode;
            pNode = pNode->pNext;
            if (pNode == 0)
                return;
        }
        pNode->blockCount = pNode->blockCount + 1;
        if (pNode->blockCount != pWire->wBlockSeq) {
            OutputDebugStringA("Attachment Interim Block out of sequence, attachment discard");
            CloseHandle(pNode->hFile);
            pNode->hFile = 0;
            pMsg = new NetMsgQueueNode();
            goto LAB_0043a457;
        }
        if (WriteFile(pNode->hFile, pWire->data, pWire->nDataLen, &dwWritten, 0) != 0)
            return;
        CloseHandle(pNode->hFile);
        goto LAB_0043a466;

    default:
        // FINAL block: locate node (closing any open handle), validate count, write + close.
        pNode = pInboundTransfers;
        if (pNode != 0) {
            while (pNode->wXferId != pWire->wXferId) {
                pPrev = pNode;
                pNode = pNode->pNext;
                if (pNode == 0)
                    goto final_after;
            }
            // matched: close any open handle before rewriting.
            if (pNode->hFile != 0) {
                CloseHandle(pNode->hFile);
                pNode->hFile = 0;
            }
        }
final_after:
        if (pNode == 0)
            return;
        pNode->blockCount = pNode->blockCount + 1;
        if (pNode->blockCount == pWire->wBlockSeq) {
            g_pPostBagCache->PostBag_BuildDatFilePath((unsigned int)pNode->wXferId, 5, szPath);
            pNode->hFile = CreateFileA(szPath, 0x40000000, 1, 0, 1, 0x8000000, 0);
            if (pNode->hFile != INVALID_HANDLE_VALUE) {
                WriteFile(pNode->hFile, pWire->data, pWire->nDataLen, &dwWritten, 0);
                CloseHandle(pNode->hFile);
            }
LAB_0043a43c:
            pNode->hFile = 0;
            pMsg = new NetMsgQueueNode();
LAB_0043a457:
            pMsg->type = 0x18;
            pMsg->pNext = 0;
            pMsg->bReliable = (unsigned int)pNode->wTrainId;
            pMsg->bEventOwnerA = pNode->bOwnerByteA;
            g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pMsg);
            if (pPrev == 0)
                pInboundTransfers = pNode->pNext;
            else
                pPrev->pNext = pNode->pNext;
            goto LAB_0043a491;
        }
        OutputDebugStringA("Attachment FINAL Block out of sequence, attachment discarded");
        CloseHandle(pNode->hFile);
LAB_0043a466:
        pNode->hFile = 0;
        pMsg = new NetMsgQueueNode();
        pMsg->type = 0x18;
        pMsg->pNext = 0;
        pMsg->bReliable = (unsigned int)pNode->wTrainId;
        pMsg->bEventOwnerA = pNode->bOwnerByteA;
        g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pMsg);
        if (pPrev != 0) {
            pPrev->pNext = pNode->pNext;
            goto LAB_0043a491;
        }
        pInboundTransfers = pNode->pNext;
        goto LAB_0043a491;

    case 0:
        // FIRST block: open the file, write block 0.
        if (pInboundTransfers == 0)
            return;
        pNode = pInboundTransfers;
        while (pNode->wXferId != pWire->wXferId) {
            pPrev = pNode;
            pNode = pNode->pNext;
            if (pNode == 0)
                return;
        }
        if (pNode->blockCount == 0) {
            g_pPostBagCache->PostBag_BuildAttFilePath((unsigned int)pNode->wXferId, 5, szPath);
            pNode->hFile = CreateFileA(szPath, 0x40000000, 1, 0, 1, 0x8000000, 0);
            if (pNode->hFile != INVALID_HANDLE_VALUE) {
                if (WriteFile(pNode->hFile, pWire->data, pWire->nDataLen, &dwWritten, 0) != 0)
                    return;
                CloseHandle(pNode->hFile);
                goto LAB_0043a43c;
            }
        } else {
            OutputDebugStringA("Attachment First Block out of sequence, attachment discarded");
        }
        pMsg = new NetMsgQueueNode();
        pMsg->type = 0x18;
        pMsg->pNext = 0;
        pMsg->bReliable = (unsigned int)pNode->wTrainId;
        pMsg->bEventOwnerA = pNode->bOwnerByteA;
        g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pMsg);
        if (pPrev != 0) {
            pPrev->pNext = pNode->pNext;
            goto LAB_0043a491;
        }
        pInboundTransfers = pNode->pNext;
        goto LAB_0043a491;
    }
LAB_0043a491:
    delete pNode;
}

// FUNCTION: LOCO 0x43ad00
// Handle a local "move my train one board slot in heading D" command: the payload is the train
// node, +0x10 the heading (0/90/180/270deg). If we're not the host (connectionMode != 2), or the
// train is flagged "release outright" (+0x4 == 1), just delete it and clear the payload. Otherwise
// map the heading to the neighbouring provider slot (up/down a grid row, or +/-1 column) from our
// own selected slot; if that slot is owned by a peer (providerId != 0), try to hand the train off
// (GameNet_SendTrainStateSync) and, on failure, park it on the awaiting-ack list tail and pump
// the blocked-train drain; if the slot is empty, defer to TrainNet_HandleEmptySlotHandoffMaybe.
// EFFECTIVE MATCH: identical CFG (same branches, same 4-arg SendTrainStateSync call incl. the
// trailing literal 0, same tail-walk + list append) -- the only difference is a whole-function
// register-residency tie-break. The original SPILLS `this` to a reused parameter stack slot
// (push ecx / mov [esp+0xc],ecx, reloaded at each call) and keeps the manager pointer in
// callee-saved edi; my allocation keeps `this` in callee-saved ebx and the manager in scratch ecx
// (equivalent, in fact 8 insns shorter). Confirmed intrinsic: the original leaves ebp entirely
// free yet still chooses the stack spill, so it's not register pressure; reordering the mgr-cache
// declaration only worsened it. The residency swap cascades a register-byte difference through
// every this/mgr reference (hence the large raw byte_diff) plus a lone `ja` vs `jne` (unsigned
// !=0 form). Yoda #29/#30.
void GameNetThreadState::TrainNet_HandleMoveRequest(NetMsgQueueNode *pNode) {
    DPlaySessionMgr *mgr = g_pDPlaySessionMgr;
    PeerTrainNodePartial *pTrain = (PeerTrainNodePartial *)pNode->pPayload;
    int nHeading = pNode->nMoveHeading;
    if (mgr->connectionMode != 2) {
        if (pTrain != 0) {
            delete (GameNetQueuedNodeMaybe *)pTrain;
            pNode->pPayload = 0;
            return;
        }
        pNode->pPayload = 0;
        return;
    }
    if (pTrain->nDiscardFlag == 1) {
        delete (GameNetQueuedNodeMaybe *)pTrain;
        pNode->pPayload = 0;
        return;
    }

    int nSlot = mgr->selectedProviderIndex;
    switch (nHeading) {
    case 0x5a:  nSlot = nSlot + 1; break;
    case 0:     nSlot = nSlot - mgr->nProviderSlotsPerRow; break;
    case 0xb4:  nSlot = mgr->nProviderSlotsPerRow + nSlot; break;
    case 0x10e: nSlot = nSlot - 1; break;
    default:    nSlot = 0; break;
    }

    DPlaySessionMgrProviderSlot *pSlot = mgr->ProviderSlotAt(nSlot);
    if (pSlot->providerId == 0) {  // sic: ProviderSlotAt returns NULL for nSlot<0, deref'd anyway
        TrainNet_HandleEmptySlotHandoffMaybe(pTrain, nHeading, nSlot);
        return;
    }
    if (GameNet_SendTrainStateSync(pSlot->providerId, pTrain, nHeading, 0) == 0) {
        if (pTrainListAwaitingAck != 0) {
            PeerTrainNodePartial *pTail = pTrainListAwaitingAck;
            while (pTail->pNext != 0)
                pTail = (PeerTrainNodePartial *)pTail->pNext;
            pTrain->pNext = 0;
            pTail->pNext = pTrain;
            GameNet_DrainBlockedTrainListAsNotify();
            return;
        }
        pTrain->pNext = 0;
        pTrainListAwaitingAck = pTrain;
        GameNet_DrainBlockedTrainListAsNotify();
    }
}

// Teardown-side counterpart of the shared array construct/destruct helper (0x467280 -- same
// routine already modeled in src/DSound.cpp as ArrayDestructWithIteratorMaybe for the
// DSoundChannel pool; redeclared here with a fresh typedef for this TU's own element type,
// masked-reloc call so the two TUs' differing C++ signatures don't need to agree). Only the
// TEARDOWN side needs an explicit hand-written call: `new TrainSyncWireMsgSend` reproduces the
// ALLOC+construct side exactly (its implicit ctor -- "construct my one non-trivial array member,
// nothing else" -- is small enough that /O2 inlines it straight into the caller, showing the
// exact SEH-scaffolding + direct array-construct-helper-call shape the original has, confirmed
// via an isolated probe); but a plain `delete pMsg` does NOT inline its own implicit dtor the
// same way (stays a real out-of-line thiscall to a tiny wrapper) -- so the teardown side is
// modeled the DSound.cpp way instead (explicit helper call + operator delete), matching the
// original's own two-call shape. C++ has no `&T::~T()` syntax, so the dtor argument is a
// hand-written free-function stand-in (never defined -- this project only compiles+diffs
// COMDATs, never links).
extern "C" {  // TODO: idiom
    void *__stdcall ArrayDestructWithIteratorMaybe(void *pArray, unsigned int elemSize, unsigned int count, void *pDtorThunk);
    void TrainSyncCarRecord_DestructThunkMaybe(TrainSyncCarRecordSend *pRecord);
}

// TrainSyncCarRecordSend's own implicit ctor/dtor pair, both entirely compiler-generated from the
// CarNetState the struct embeds by value (src/GameNet.h) -- 15 bytes of construct-the-member and
// 8 of destruct-it, with no source line anywhere. They exist out of line because the record array
// is built and torn down through the vector ctor/dtor iterators the two 0x3f2 senders below use.
//
// FUNCTION: LOCO 0x43b220 (??0TrainSyncCarRecordSend -- compiler-generated)
// FUNCTION: LOCO 0x43b230 (??1TrainSyncCarRecordSend -- compiler-generated)

// FUNCTION: LOCO 0x43ae20
// Outbound msg 0x3f2 ("train state sync", receive side: GameNet_HandleTrainStateSync). Special
// case: if the target provider slot IS our own currently-selected slot (a self hand-off --
// TrainNet_HandleMoveRequest's own way of moving a train into a board slot WE now occupy), skip
// the wire entirely: flip the heading 180deg (computed twice in a row, a net no-op since the
// flip is its own inverse -- sic) and re-park the train on the awaiting-ack list. Otherwise,
// build the wire message: alloc a fixed TrainSyncWireMsgSend (placement-constructing its 3
// embedded CarNetState detail records via the shared array-construct helper), stamp the
// header from the train's own fields, snapshot car slot 0 (the locomotive)'s type id + category
// name into the header, then for each additional car slot (1..N) fill a detail record with the
// car's type id + category and, if the car's CarNetState is live, bulk-copy the whole
// object onto the wire and mark bHasDetailFlagMaybe. Send via DPlay_SendMessage; CORRECTED
// 2026-07-20 (DPlay_SendMessage's return polarity was previously misdocumented backwards --
// see its own plate comment): on send FAILURE (return 0), free the message, flip the heading
// (once) and re-park + drain the awaiting-ack list as type-0x11 notifies (a retry path). On
// send SUCCESS (return nonzero), free the message and either park the train on the active list
// (nDiscardFlag==0) or release it outright (nDiscardFlag!=0, virtual scalar-deleting dtor). Always
// returns 1.
//
// PARTIAL MATCH (asmscore total 383072 at true len 1015, align=378 reg_pen=44 byte_diff=232,
// insns 325/318). Real fixes banked this session: `new TrainSyncWireMsgSend` (a single combined
// object whose only non-trivial content is the `records[3]` array member) reproduces the /GX
// alloc-protection SEH scaffolding AND a direct call to the shared array-construct-iterator
// helper INLINE in the caller (confirmed via an isolated probe -- the implicit ctor, "construct
// my one array member, nothing else", is small enough that /O2 inlines it away) -- explicit
// `operator new`+hand-written helper call does NOT reproduce this. The teardown side does NOT
// mirror this (a plain `delete pMsg` does not inline its own implicit dtor the same way -- stays
// a real out-of-line thiscall wrapper), so teardown keeps the DSound.cpp-style explicit
// `ArrayDestructWithIteratorMaybe` call instead (declared `__stdcall`, `ret 0x10` confirmed via
// raw disasm of 0x467280 -- the default extern-block cdecl left a spurious `add esp,0x10` at
// every call site). Per-car-record loop needed direct `pMsg->records[idx].field` subscripting at
// every site (not a cached record pointer -- the "single element touched at several offsets"
// lever, CLAUDE.md) and the has-detail/no-detail branch written `if (pState != 0) {...} else
// {...}` (has-detail first) to match the original's fall-through order; the detail payload copy
// needed `memcpy(&rec->state, pState, sizeof(CarNetState))`, not a struct assignment (which
// register-promotes to field-by-field copies instead of the original's `rep movsd`). Car slot
// 0's type id and category-name reads are NOT cached in a local (re-read `pTrain->carSlots[0]`
// at each use -- Yoda #19 aliasing). Residual: (a) the self-hand-off branch's double-flip and
// the send-success branch's single flip each show a handful of `this`-register-role differences
// (the symmetric-register-swap class, Yoda #29/#30, extended by the repeated-flip-logic shape);
// (b) the send-success branch's original has a genuinely redundant self-store of the heading
// field immediately before its own flip computation (read-then-store-back-unchanged) that survives
// in the original but gets dead-store-eliminated by our compiler -- tried reproducing it with an
// explicit `pTrain->wHeading = wHead;` before the flip (already present in source) with no
// effect, likely the same fold-vs-keep optimizer-strength class as the documented redundant-
// recheck lessons; (c) a handful of remaining `this`/list-pointer register swaps in the
// send-failure branch's active-list-prepend tail. Not re-probed further per the triage budget --
// this is among the largest/most structurally novel functions transcribed this session (SEH
// framing, 5 major branches, an embedded-object array member, redundant duplicate heading-flip
// logic, and a name-string copy all in one function).
char GameNetThreadState::GameNet_SendTrainStateSync(unsigned int nProviderId, PeerTrainNodePartial *pTrain, int nHeading, int nFlag) {
    unsigned int nSelfSlot = g_pDPlaySessionMgr->selectedProviderIndex;
    unsigned int nTargetSlot = g_pDPlaySessionMgr->ResolveIdToSlot(nProviderId);
    if (nSelfSlot == nTargetSlot) {
        switch (nHeading) {
        case 0x5a:  nHeading = 0x10e; break;
        case 0:     nHeading = 0xb4; break;
        case 0xb4:  nHeading = 0; break;
        case 0x10e: nHeading = 0x5a; break;
        }
        pTrain->wHeading = (unsigned short)nHeading;
        // sic: flipped again immediately (its own inverse -- net no-op, matches original)
        switch (nHeading) {
        case 0x5a:  nHeading = 0x10e; break;
        case 0:     nHeading = 0xb4; break;
        case 0xb4:  nHeading = 0; break;
        case 0x10e: nHeading = 0x5a; break;
        }
        pTrain->wHeading = (unsigned short)nHeading;

        if (pTrainListAwaitingAck == 0) {
            pTrain->pNext = 0;
            pTrainListAwaitingAck = pTrain;
        } else {
            PeerTrainNodePartial *pTail = pTrainListAwaitingAck;
            while (pTail->pNext != 0)
                pTail = (PeerTrainNodePartial *)pTail->pNext;
            pTrain->pNext = 0;
            pTail->pNext = pTrain;
        }
        GameNet_DrainBlockedTrainListAsNotify();
    } else {
        TrainSyncWireMsgSend *pMsg = new TrainSyncWireMsgSend;

        pMsg->wOpcode = 0x3f2;
        pMsg->wPad = 0;
        pMsg->wHeading = (unsigned short)nHeading;
        pMsg->wTrainId = pTrain->wTrainId;
        pMsg->bOwnerByteA = pTrain->bOwnerByteA;
        pMsg->wSelectedCar = pTrain->wSelectedCarId;
        pMsg->dwReversed = pTrain->dwReversed;

        pMsg->nKindId = CarNetObj_GetCarTypeId((CarNetObj *)pTrain->carSlots[0]);
        pMsg->bCarCount = 0;
        strcpy(pMsg->szName, ((CarNetObj *)pTrain->carSlots[0])->szCategoryName);

        if (pTrain->wCarSlotCount != 0) {
            for (int i = 1; i <= (int)pTrain->wCarSlotCount; i++) {
                CarNetObj *pCar = (CarNetObj *)pTrain->carSlots[i];
                if (pCar != 0) {
                    pMsg->records[pMsg->bCarCount].nCarTypeId = CarNetObj_GetCarTypeId(pCar);
                    pMsg->records[pMsg->bCarCount].nCarCategory = pCar->nCarCategory;
                    CarNetState *pState = CarNetObj_GetAppliedState(pCar);
                    if (pState != 0) {
                        pMsg->records[pMsg->bCarCount].bHasDetail = 1;
                        memcpy(&pMsg->records[pMsg->bCarCount].state, pState, sizeof(CarNetState));
                    } else {
                        pMsg->records[pMsg->bCarCount].bHasDetail = 0;
                    }
                    pMsg->bCarCount++;
                }
            }
        }

        int nSendResult = g_pNetManager->DPlay_SendMessage(nProviderId, pMsg, sizeof(TrainSyncWireMsgSend), 1);
        if (nSendResult == 0) {
            if (pMsg != 0) {
                ArrayDestructWithIteratorMaybe(pMsg->records, sizeof(TrainSyncCarRecordSend), 3, (void *)TrainSyncCarRecord_DestructThunkMaybe);
                operator delete(pMsg);
            }

            unsigned short wHead = pTrain->wHeading;
            pTrain->wHeading = wHead;
            switch (wHead) {
            case 0x5a:  wHead = 0x10e; break;
            case 0:     wHead = 0xb4; break;
            case 0xb4:  wHead = 0; break;
            case 0x10e: wHead = 0x5a; break;
            }
            pTrain->wHeading = wHead;

            if (pTrainListAwaitingAck == 0) {
                pTrain->pNext = 0;
                pTrainListAwaitingAck = pTrain;
            } else {
                PeerTrainNodePartial *pTail = pTrainListAwaitingAck;
                while (pTail->pNext != 0)
                    pTail = (PeerTrainNodePartial *)pTail->pNext;
                pTrain->pNext = 0;
                pTail->pNext = pTrain;
            }

            while (pTrainListAwaitingAck != 0) {
                NetMsgQueueNode *pNode = new NetMsgQueueNode();
                pNode->type = 0x11;
                pNode->pPayload = pTrainListAwaitingAck;
                pTrainListAwaitingAck->bHasDetailFlagMaybe = 0;
                pTrainListAwaitingAck->bOwnerByteB = (unsigned char)g_pDPlaySessionMgr->selectedProviderIndex;
                pTrainListAwaitingAck->pNext = 0;
                pTrainListAwaitingAck->bHasDetailFlagMaybe = 0;  // sic: redundant re-clear
                // sic: pNext was just zeroed above, so this always dequeues to NULL --
                // the drain only ever processes the list head once per call, regardless of
                // how many trains are actually queued (see docs/subsystems.md).
                pTrainListAwaitingAck = (PeerTrainNodePartial *)pTrainListAwaitingAck->pNext;
                g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);
            }
        } else {
            if (pMsg != 0) {
                ArrayDestructWithIteratorMaybe(pMsg->records, sizeof(TrainSyncCarRecordSend), 3, (void *)TrainSyncCarRecord_DestructThunkMaybe);
                operator delete(pMsg);
            }

            pTrain->bOwnerByteB = (unsigned char)g_pDPlaySessionMgr->ResolveIdToSlot(nProviderId);
            pTrain->bAckCounterB = 0;
            if (pTrain->nDiscardFlag == 0) {
                pTrain->pNext = pTrainListActive;
                pTrainListActive = pTrain;
            } else {
                pTrain->bHasDetailFlagMaybe = 1;
                delete (GameNetQueuedNodeMaybe *)pTrain;
            }
        }
    }
    return 1;
}

// The net-subsystem shutdown gate. The `unsigned char` return type is LOAD-BEARING, not
// cosmetic -- it is what reproduces the original's sete-materialized branch; see the autopsy
// on EnqueueOrFreeNode below. Kept TU-local rather than hoisted into src/GameNetMsgQueue.h
// beside the `extern int g_nScreenState` it wraps: that header is shared, and v340/v355/v356 all
// measured that adding DECLARATIONS to a shared header rotates other TUs' codegen. Hoisting
// these predicates into one shared header is a worthwhile but separately-measured pass.
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// FUNCTION: LOCO 0x4393d0
// Append a send-queue node to g_pNetMsgSendQueueHead, or dispose of it inline. When the net
// subsystem is shutting down (g_nScreenState==10, a non-8 opcode) or the queue is in discard mode
// (this->bShutdownRequestedMaybe), the node is freed immediately: type 0xe/0x10 payloads are C++
// objects freed via their virtual dtor, other payloads are raw operator delete'd. Otherwise the
// node is linked at the tail under the queue lock, unless the queue is already >=6 deep and the
// node is an unreliable type-6 tick (dropped to bound the backlog).
//
// EXACT MATCH as of v359. Two levers, found two sessions apart, were needed.
//
// ⭐ This was the ORIGINAL park of the "sete-materialization class" and v356 CRACKED it. The
// prior diagnosis here -- that the original spills the compare flag via `setcc` because it
// reuses esi between the cmp and the branch, and that the shape is unsteerable -- was WRONG on
// both counts, and its 8-byte-shorter prologue was cascading every downstream jump displacement
// (which is what made cc.sh's raw DIFF read ~192 for what is really a 3-byte disagreement).
// The real cause is a **byte-returning inline predicate**: `IsNetShuttingDownMaybe()` above
// returns `unsigned char`, which forces the comparison through AL and makes the `if` a
// `test al,al` -- exactly the original's `xor/cmp/sete/test`. Writing `g_nScreenState == 10`
// inline gives a plain `cmp; je` and cannot reproduce it. Same lever fixed the sibling sites in
// src/WorldBoardMaybe.cpp and src/WorldActionCursor.cpp; see docs/CODEGEN.md.
//
// ⭐ The last 3 bytes fell in v359: **WALK THE TAIL WITH ONE CURSOR, NOT TWO.** The residual
// was one scheduling tie in the tail-walk loop -- the original emits `inc edx` BEFORE the
// `mov eax,[reg+0x18]` pNext load and reads that load through ecx (pTail); ours emitted the load
// first and read it through eax. Both come from the same root cause: a TWO-cursor walk
// (`pCur = pTail->pNext` with `pTail = pCur` at the top) lets MSVC see pTail == pCur and reuse
// the register the value is ALREADY in (eax), leaving nothing to schedule into the load's
// shadow. The one-cursor form
//     count = 1;
//     while (pTail->pNext != 0) { pTail = pTail->pNext; count++; }
// makes the load genuinely depend on the just-written pTail, which pins its base to ecx and
// pushes `inc edx` up into the dependency stall. Byte-identical result, and the natural way to
// write a tail walk anyway. Probed WITHOUT effect (both before and, for the first, after the
// SP3 switch): reordering `count++` above `pTail = pCur`, a `for`-loop form, and
// `pCur = pCur->pNext` vs `pTail->pNext` -- all of which keep two cursors, which is why none of
// them could work. A `do/while` form is strictly worse (25013). See docs/CODEGEN.md.
void GameNetThreadState::EnqueueOrFreeNode(NetMsgQueueNode *pNode) {
    void *pPayload;
    NetMsgQueueNode *pTail;
    int count;
    int type;

    if (IsNetShuttingDownMaybe() && (type = pNode->type) != 8) {
        pPayload = pNode->pPayload;
        if (pPayload == 0) goto free_node;
        switch (type) {
        case 0xe:
        case 0x10:
            delete (NetMsgPayloadObjMaybe *)pNode->pPayload;
            break;
        default:
            operator delete(pPayload);
            break;
        }
    } else {
        if (this->bShutdownRequestedMaybe != 0) {
            if (pNode->pPayload != 0) {
                operator delete(pNode->pPayload);
                operator delete(pNode);
                return;
            }
            goto free_node;
        }
        pNode->pNext = 0;
        g_pGameNetMsgQueueLock->Lock();
        pTail = g_pNetMsgSendQueueHead;
        if (pTail != 0) {
            count = 1;
            while (pTail->pNext != 0) {
                pTail = pTail->pNext;
                count++;
            }
            if (count <= 5 || pNode->type != 6 || pNode->bReliable != 0) {
                pTail->pNext = pNode;
                g_pGameNetMsgQueueLock->Unlock();
                return;
            }
            g_pGameNetMsgQueueLock->Unlock();
            pPayload = pNode->pPayload;
            if (pPayload == 0) goto free_node;
            operator delete(pPayload);
        } else {
            g_pNetMsgSendQueueHead = pNode;
            g_pGameNetMsgQueueLock->Unlock();
            return;
        }
    }
    pNode->pPayload = 0;
free_node:
    operator delete(pNode);
}

// FUNCTION: LOCO 0x439240
// The GameNet worker thread's main tick loop (an infinite do{}while(true) driven by
// Sleep(g_pNetSettings->nTickSleepMs) at the bottom, NOT a Windows message pump).
// Each tick: (1) if a net manager exists and we've joined a session, pump inbound packets via
// GameNet_DispatchMessage; (2) drain the local-command queue node-by-node under the queue lock,
// running GameNet_ProcessLocalCommand on each -- but if a dispatch went reentrant
// (bShutdownRequestedMaybe), instead flush+dispose the whole send queue (freeing type-0xe/0x10 payload
// objects via their virtual dtor, others via operator delete) and EXIT the thread; (3) otherwise
// bump the tick counter and, every nTrainAdvanceInterval ticks, advance local trains (no
// sleep that tick); on the other ticks pump any pending outbound file transfer, age the pending-
// train hand-off timers (decrementing each active train's wHeading; on expiry tear down the
// session and drain the peer list), then Sleep.
//
// EFFECTIVE MATCH (structure 100% correct; asmscore byte_diff 22 at true len 394, insns 125/129).
// All register ROLES match the original -- ebx=0-constant, edi=this, esi=scratch-node, and /O2
// correctly HOISTS the Sleep IAT slot into callee-saved ebp (call ebp) exactly as the original.
// The entire residual is three REDUNDANT pointer null-checks that a dominating branch already
// proved non-null: (a) the inner-drain loop's `if (pNode == 0)` recheck after `delete pNode`;
// (b) the pending-train `while (pTrain != 0)` rotation guard, redundant with the enclosing
// `if (pTrain != 0 && ...)` guard; (c) the dispose-case `delete (NetMsgPayloadObjMaybe*)pPayload`'s
// own null-check, redundant with the enclosing `if (pPayload != 0)`. The original binary KEEPS all
// three (`cmp reg,ebx; je`); our cl 11.00 FOLDS them via redundant-branch elimination. Confirmed
// unsteerable: `delete pNode` vs `operator delete(pNode)`, split vs `||` tail, and `for` vs `while`
// rotation ALL fold identically. The type-value register (edx vs eax) and the `mov al,[edi+0xd]`
// materialization of bDispatchBusy are downstream artifacts of fold (a). Same redundant-check-
// elimination-strength difference class as the peel lessons, inverted (fold removes vs peel adds).
void GameNetThread_TickLoop(GameNetThreadState *param_1) {
    do {
        if (g_pNetManager != 0 && g_pNetManager->bSessionJoined != 0) {
            param_1->GameNet_DispatchMessage();
        }
        while (true) {
            NetMsgQueueNode *pNode = 0;
            g_pGameNetMsgQueueLock->Lock();
            if (g_pNetMsgSendQueueHead != 0) {
                pNode = g_pNetMsgSendQueueHead;
                g_pNetMsgSendQueueHead = pNode->pNext;
            }
            g_pGameNetMsgQueueLock->Unlock();
            if (pNode == 0) break;
            param_1->GameNet_ProcessLocalCommand(pNode);
            delete pNode;
            if (pNode == 0 || param_1->bShutdownRequestedMaybe != 0) break;
        }
        if (param_1->bShutdownRequestedMaybe != 0) {
            g_pGameNetMsgQueueLock->Lock();
            while (g_pNetMsgSendQueueHead != 0) {
                void *pPayload = g_pNetMsgSendQueueHead->pPayload;
                NetMsgQueueNode *pNext = g_pNetMsgSendQueueHead->pNext;
                if (pPayload != 0) {
                    switch (g_pNetMsgSendQueueHead->type) {
                    case 0xe:
                    case 0x10:
                        delete (NetMsgPayloadObjMaybe *)pPayload;
                        g_pNetMsgSendQueueHead->pPayload = 0;
                        break;
                    default:
                        operator delete(pPayload);
                        g_pNetMsgSendQueueHead->pPayload = 0;
                        break;
                    }
                }
                delete g_pNetMsgSendQueueHead;
                g_pNetMsgSendQueueHead = pNext;
            }
            g_pGameNetMsgQueueLock->Unlock();
            return;
        }
        param_1->nTickCounter = param_1->nTickCounter + 1;
        if (param_1->nTickCounter % param_1->nTrainAdvanceInterval == 0) {
            param_1->nTickCounter = 0;
            param_1->TrainNet_AdvanceLocalTrainSteps();
        } else {
            if (g_pNetManager != 0 && g_pNetManager->bSessionJoined != 0 &&
                param_1->pOutboundTransfers != 0) {
                param_1->NetFile_PumpPendingTransferSend();
            }
            PeerTrainNodePartial *pTrain = param_1->pTrainListActive;
            if (pTrain != 0 && g_pDPlaySessionMgr->connectionMode == 1 &&
                g_pNetManager != 0 && g_pNetManager->bSessionJoined != 0) {
                while (pTrain != 0) {
                    --pTrain->wHeading;
                    if (pTrain->wHeading == 0) {
                        param_1->GameNet_TeardownAndFlushQueues();
                        param_1->GameNet_DrainPeerListAsNotify();
                    }
                    pTrain = (PeerTrainNodePartial *)pTrain->pNext;
                }
            }
            Sleep(g_pNetSettings->nTickSleepMs);
        }
    } while (true);
}

// FUNCTION: LOCO 0x439df0
// Pump the outbound file-transfer list (manager +0x28), rate-limited to at most ONE block sent per
// call: each node cools down bCooldownTicks ticks between service; when ready, dispatch on
// bBlockStage (0=not started, 1=streaming, 2=EOF-draining the local .att copy into a final .dat
// payload). FIRST/INTERIM blocks stream straight from the already-open .att handle (0x7fec scratch
// buffer, 0x7fdc-byte reads) and return immediately after a successful send. The FINAL/.dat block
// re-opens "PostBag<cat>\<id>.dat" (PostBag_BuildDatFilePath) with a smaller 0x400-byte buffer,
// always closes + deletes the source .att file afterward, and only continues the loop when the
// finishing node was the list HEAD (a non-head finish returns immediately). On any ReadFile failure
// the node is unlinked and freed and the scan continues onto the next node in the SAME call.
// sic: a CreateFileA failure on a NON-head node frees the failed node and reassigns the walk cursor
// to the PREVIOUS node, then falls through UNCONDITIONALLY into the ReadFile call below using that
// previous node's (possibly already-closed) handle and a NULL+0xd garbage buffer pointer (the
// just-freed packet buffer was zeroed first) -- an original engine bug, not a head-case-only path.
// PARTIAL (transcribed, not yet byte-matched -- asmscore --len 837 byte_diff 440, insns 270/276).
// Structure now matches closely (dispatch order, all three case bodies, the CreateFileA-failure
// head case routed through the shared unlink_head tail like the FIRST/INTERIM failure paths).
// Residual: (a) the FINAL-success head-continue path is a self-contained original block (its own
// store-head+delete+reread+explicit-pPrev-zero) that does NOT share code with the CreateFileA-
// failure head case's tail, unlike here where both funnel through one shared unlink_head -- a
// block-layout/cross-jump difference (Yoda #15/#18), not obviously source-steerable; forcing a
// second, textually-duplicated copy did not reproduce it. (b) an extra `xor edx,edx` zero-extend
// before the first `PostBag_BuildDatFilePath` argument's 16-bit load that the original lacks
// (original leaves the upper word of that register as leftover garbage) -- tried caching
// wAttId in an explicit local first (BeginFileTransfer's own idiom) with no effect.
void GameNetThreadState::NetFile_PumpPendingTransferSend() {
    char szPath[0x504] = "";

    FileTransferNode *pPrev = 0;
    FileTransferNode *pNode = pOutboundTransfers;
    DWORD dwRead = 0;
    FileBlockWireMsg *pPkt;

    while (pNode != 0) {
        if (pNode->bCooldownTicks > 0) {
            pNode->bCooldownTicks--;
            pPrev = pNode;
            pNode = pNode->pNext;
            continue;
        }
        pNode->bCooldownTicks = 0x14;

        switch (pNode->bBlockStage) {
        default: {
            // FINAL block: re-open the built .dat file and stream it in one 0x400-byte chunk.
            g_pPostBagCache->PostBag_BuildDatFilePath(pNode->wAttId, 4, szPath);
            pPkt = (FileBlockWireMsg *)operator new(0x410);
            pNode->hFile = CreateFileA(szPath, 0x80000000, 1, 0, 4, 0x8000000, 0);
            if (pNode->hFile == INVALID_HANDLE_VALUE) {
                pNode->hFile = 0;
                operator delete(pPkt);
                pPkt = 0;
                if (pPrev == 0)
                    goto unlink_head;
                pPrev->pNext = pNode->pNext;
                operator delete(pNode);
                pNode = pPrev;
                // sic: falls through into the ReadFile below using pPrev's (stale) handle and a
                // NULL+0xd garbage buffer pointer -- see function-level "sic" note above.
            }
            if (ReadFile(pNode->hFile, pPkt->data, 0x400, &dwRead, 0)) {
                pPkt->bBlockType = 2;
                pPkt->wXferId = pNode->wXferId;
                pNode->blockCount++;
                pPkt->wBlockSeq = pNode->blockCount;
                pPkt->wOpcode = 0x3fc;
                pPkt->nDataLen = dwRead;
                g_pNetManager->DPlay_SendMessage(pNode->dwPeerId, pPkt, dwRead + 0x10, 1);
                operator delete(pPkt);
            }
            CloseHandle(pNode->hFile);
            pNode->hFile = 0;
            g_pPostBagCache->PostBag_DeleteAttachmentFiles(4, pNode->wAttId);
            if (pPrev != 0) {
                pPrev->pNext = pNode->pNext;
                operator delete(pNode);
                return;
            }
            pOutboundTransfers = pNode->pNext;
            operator delete(pNode);
            pNode = pOutboundTransfers;
            pPrev = 0;
            continue;
        }
        case 1: {
            // INTERIM block.
            pPkt = (FileBlockWireMsg *)operator new(0x7fec);
            if (!ReadFile(pNode->hFile, pPkt->data, 0x7fdc, &dwRead, 0)) {
                CloseHandle(pNode->hFile);
                pNode->hFile = 0;
                operator delete(pPkt);
                if (pPrev == 0)
                    goto unlink_head;
                goto unlink_nonhead;
            }
            if (dwRead > 0) {
                pPkt->bBlockType = 1;
                pPkt->wXferId = pNode->wXferId;
                pNode->blockCount++;
                pPkt->wBlockSeq = pNode->blockCount;
                pPkt->wOpcode = 0x3fc;
                pPkt->nDataLen = dwRead;
                g_pNetManager->DPlay_SendMessage(pNode->dwPeerId, pPkt, dwRead + 0x10, 1);
                operator delete(pPkt);
                return;
            }
            // EOF -- transition to the FINAL/.dat phase, keep this node in the list.
            pNode->bBlockStage = 2;
            CloseHandle(pNode->hFile);
            pNode->hFile = 0;
            operator delete(pPkt);
            pPrev = pNode;
            pNode = pNode->pNext;
            continue;
        }
        case 0: {
            // FIRST block.
            pPkt = (FileBlockWireMsg *)operator new(0x7fec);
            if (ReadFile(pNode->hFile, pPkt->data, 0x7fdc, &dwRead, 0)) {
                pPkt->bBlockType = 0;
                pPkt->wXferId = pNode->wXferId;
                pPkt->wBlockSeq = 0;
                pPkt->wOpcode = 0x3fc;
                pPkt->nDataLen = dwRead;
                g_pNetManager->DPlay_SendMessage(pNode->dwPeerId, pPkt, dwRead + 0x10, 1);
                operator delete(pPkt);
                pNode->bBlockStage = 1;
                return;
            }
            CloseHandle(pNode->hFile);
            pNode->hFile = 0;
            operator delete(pPkt);
            if (pPrev == 0)
                goto unlink_head;
            goto unlink_nonhead;
        }
        }

    unlink_head:
        pOutboundTransfers = pNode->pNext;
        operator delete(pNode);
        pNode = pOutboundTransfers;
        continue;

    unlink_nonhead:
        pPrev->pNext = pNode->pNext;
        operator delete(pNode);
        pNode = pPrev->pNext;
    }
}

// FUNCTION: LOCO 0x4396c0
// The gameplay opcode dispatcher: while a session is active and dispatch isn't already running,
// drain every queued inbound DirectPlay message (DPlay_ReceiveAndDispatch) and fan each one out
// by its 16-bit opcode. Opcode ranges: <0x15 = small control opcodes (0xa = teardown-and-rehome
// every active train as type-0xf notifies if we're a joined peer; 0x14 = player-left); <0x3e9 =
// handshake opcodes (1000 = "who are you" -> reset every active train's heading to a disconnect
// sentinel and reply with our (now this-player's) session id and 4 opaque app-identity dwords;
// 0x3c = redirect a pending local command's target player to the local player); >=0x3ea = the
// dense 20-opcode (0x3ea..0x3fd) gameplay jump table (opcode - 0x3ea = table index). After each
// message, the wire payload is freed unless its ownership was transferred into a queued
// NetMsgQueueNode (opcodes 0x3f6/0x3f7/0x3f9 -- cases 0xc/0xd/0xf), then the receive header
// itself is freed.
// PARTIAL (transcribed, not yet byte-matched).
void GameNetThreadState::GameNet_DispatchMessage() {
    if (bShutdownRequestedMaybe != 0 || g_pNetManager == 0)
        return;

    NetMsgQueueNode *pNode;
    HANDLE hFile;
    DWORD dwWritten;
    char *pName;
    int nSenderId;
    char szPath[0x500];
    LayoutNameWireMsg *pMsgLayoutMaybe;

    while (g_pNetManager != 0) {
        DPlayRecvMsg *lpMem = g_pNetManager->DPlay_ReceiveAndDispatch();
        if (lpMem == 0)
            break;
        unsigned short *pWire = lpMem->pPacket;
        int wOpcode = *pWire;

        // Cascading if/else-if, not nested range guards -- matches the original's literal
        // cmp/jg/je chain (Yoda #2): each level tests > upper, == the next-highest sentinel,
        // then falls through to the last == check with no further alternative. The small-
        // opcode branch is written FIRST (as the <=0x14 arm) so it lands as the compiler's
        // fall-through, matching the original's layout (small handling immediately follows
        // the cmp/jg; the big dispatch is jumped to) -- branch-order-as-fall-through lever.
        if (wOpcode <= 0x14) {
            if (wOpcode != 0x14) {
                if (wOpcode == 10) {
                    GameNet_TeardownAndFlushQueues();
                    pNode = new NetMsgQueueNode();
                    pNode->type = 5;
                    g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);
                    if (g_pDPlaySessionMgr->connectionMode == 1) {
                        while (pTrainListActive != 0) {
                            NetMsgQueueNode *pNode2 = new NetMsgQueueNode();
                            pNode2->type = 0xf;
                            pNode2->pPayload = pTrainListActive;
                            pTrainListActive->bHasDetailFlagMaybe = 0;
                            pTrainListActive = (PeerTrainNodePartial *)pTrainListActive->pNext;
                            ((PeerTrainNodePartial *)pNode2->pPayload)->pNext = 0;
                            g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode2);
                        }
                    }
                }
            } else {
                GameNet_HandlePlayerLeft(((PlayerLeftWireMsg *)pWire)->nPlayerId);
            }
        } else if (wOpcode <= 0x3e8) {
            if (wOpcode != 0x3e8) {
                if (wOpcode == 0x3c) {
                    pNode = new NetMsgQueueNode();
                    pNode->type = 0xc;
                    pNode->destPlayerId = dpidCurrentPlayer;
                    dpidCurrentPlayer = g_pNetManager->dpidLocalPlayer;
                    g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);
                }
            } else {
                dpidCurrentPlayer = lpMem->fromPlayerId;
                for (PeerTrainNodePartial *p = pTrainListActive; p != 0; p = (PeerTrainNodePartial *)p->pNext)
                    p->wHeading = 32000;
                WelcomeReplyMsg *pReply = (WelcomeReplyMsg *)operator new(0x18);
                pReply->wOpcode = 0x3e9;
                pReply->sessionId = g_pLocalPlayerIdentity->sessionId;
                pReply->dwFileVersionMajor = g_pApp->dwFileVersionMajor;
                pReply->dwFileVersionMinor = g_pApp->dwFileVersionMinor;
                pReply->dwFileVersionBuild = g_pApp->dwFileVersionBuild;
                pReply->dwFileVersionRevision = g_pApp->dwFileVersionRevision;
                g_pNetManager->DPlay_SendMessage(dpidCurrentPlayer, pReply, 0x18, 1);
                operator delete(pReply);
            }
        } else {
            // Case bodies below are ordered to match the ORIGINAL jump table's memory
            // layout (0xe,0x13,7,6,0,1,2,8,9,0xa,0xb,0xc,0xf,0xd,0x10,0x11,0x12,4), not
            // numeric case-value order -- a jump-table switch lays out bodies in source
            // DECLARATION order (CLAUDE.md "switch case bodies... source declaration order").
                switch (wOpcode - 0x3ea) {
                case 0xe: {
                    SlotByteWireMsg *pMsg = (SlotByteWireMsg *)pWire;
                    nSenderId = lpMem->fromPlayerId;
                    pNode = new NetMsgQueueNode();
                    pNode->type = 0x1a;
                    pNode->payloadLen = 0;
                    pNode->pPayload = (void *)(unsigned int)pMsg->bSlot;
                    pNode->destPlayerId = nSenderId;
                    g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);
                    GameNet_RemoveOrRehomeNode(pMsg->bSlot);
                    break;
                }
                case 0x13:
                    GameNet_HandlePlayerLeft(lpMem->fromPlayerId);
                    break;
                case 7: {
                    RosterSnapshotWireMsg *pMsg = (RosterSnapshotWireMsg *)pWire;
                    dpidCurrentPlayer = lpMem->fromPlayerId;
                    pNode = new NetMsgQueueNode();
                    pNode->type = 9;
                    pNode->pPayload = operator new(sizeof(DPlaySessionMgrProviderSlot) * 9);
                    pNode->bReliable = pMsg->nReliable;
                    pNode->bEventGridCols = pMsg->bGridCols;
                    pNode->bEventGridRows = pMsg->bGridRows;
                    for (int i = 0; i < 9; i++)
                        ((DPlaySessionMgrProviderSlot *)pNode->pPayload)[i].GameNet_UnpackRosterRecord(&pMsg->records[i]);
                    goto case6_7_enqueue;
                }
                case 6: {
                    if (g_pNetManager->bIsHost) {
                        pMsgLayoutMaybe = (LayoutNameWireMsg *)pWire;
                        pNode = new NetMsgQueueNode();
                        pNode->type = 4;
                        pName = (char *)operator new(0xd);
                        pNode->pPayload = pName;
                        strcpy(pName, pMsgLayoutMaybe->szName);
                        pNode->destPlayerId = lpMem->fromPlayerId;
                        pNode->bReliable = pMsgLayoutMaybe->nReliable;
                        pNode->bEventOwnerA = pMsgLayoutMaybe->bOwnerA;
                        pNode->bEventOwnerB = pMsgLayoutMaybe->bOwnerB;
                    case6_7_enqueue:
                        // Own physical tail, SHARED ONLY between case 6 and case 7 (adjacent in
                        // jump-table layout order) -- NOT the same physical block as the far-away
                        // enqueue_tail: used by resolve_and_enqueue (cases 0xa/0xb/0xf). The
                        // original's case 6 body ends with a bare `jmp` straight into case 7's own
                        // inline call (raw disasm: 0x43998b `jmp 0x439a03`, landing on case 7's own
                        // `mov ecx,[g_pDPlaySessionMgr]; call EnqueueOrProcessLocalNodeMaybe`) -- a DIFFERENT
                        // physical instance of the same call than the one resolve_and_enqueue/
                        // enqueue_tail shares among 0xa/0xb/0xf. A single shared `enqueue_tail:` label
                        // for all 5 cases (6/7/0xa/0xb/0xf) merges cases 6+7 into that far block too,
                        // adding a spurious extra g_pDPlaySessionMgr reload relative to the original.
                        g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);
                    }
                    break;
                }
                case 0: {
                    SessionWelcomeWireMsg *pMsg = (SessionWelcomeWireMsg *)pWire;
                    unsigned char bStateFlag = pMsg->bStateFlag;
                    unsigned int sessionId = pMsg->sessionId;
                    for (PeerTrainNodePartial *p = pTrainListActive; p != 0; p = (PeerTrainNodePartial *)p->pNext)
                        p->wHeading = 32000;
                    if (bStateFlag != 0)
                        bSessionStateFlagMaybe = 1;
                    g_pLocalPlayerIdentity->sessionId = sessionId;
                    Profile_SavePlayerUserFile(g_pLocalPlayerIdentity);
                    GameNet_BroadcastPlayerRoster();
                    break;
                }
                case 1:
                    DPlay_UiConnectHandler((UiConnectWireMsg *)pWire);
                    break;
                case 2:
                    GameNet_ReceiveRosterSnapshot((PlayerRosterWireMsg *)pWire);
                    break;
                case 8:
                    GameNet_HandleTrainStateSync((TrainSyncWireMsg *)pWire, lpMem->fromPlayerId);
                    break;
                case 9:
                    GameNet_HandleTrainStateAck((TrainStateWireMsg *)pWire, lpMem->fromPlayerId);
                    break;
                case 0xa:
                    pNode = new NetMsgQueueNode();
                    pNode->type = 0x13;
                    pNode->pPayload = 0;
                    nSenderId = lpMem->fromPlayerId;
                    goto resolve_and_enqueue;
                case 0xb:
                    pNode = new NetMsgQueueNode();
                    pNode->type = 0x14;
                    pNode->pPayload = 0;
                    nSenderId = lpMem->fromPlayerId;
                    goto resolve_and_enqueue;
                case 0xc:
                    // Own tail (NOT the shared enqueue_tail case-6/7 block below) -- the
                    // original's case 0xc jumps straight to post_switch (0x439c53), no
                    // register reload, distinct from cases 6/7's shared call site.
                    pNode = new NetMsgQueueNode();
                    pNode->type = 0x15;
                    pNode->pPayload = pWire;
                    g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);
                    break;
                case 0xf: {
                    int nSlot = g_pDPlaySessionMgr->ResolveIdToSlot(lpMem->fromPlayerId);
                    if (nSlot >= 0) {
                        pNode = new NetMsgQueueNode();
                        pNode->type = 0x16;
                        pNode->pPayload = pWire;
                        nSenderId = lpMem->fromPlayerId;
                        goto resolve_and_enqueue;
                    }
                    break;
                }
                case 0xd: {
                    int nSlot = g_pDPlaySessionMgr->ResolveIdToSlot(lpMem->fromPlayerId);
                    GameNet_HandleSelfStateRotate((TrainRotateWireMsg *)pWire, nSlot);
                    break;
                }
                case 0x10:
                    g_pDPlaySessionMgr->LayoutNet_ReplyWithStoredLayout(lpMem->fromPlayerId);
                    break;
                case 0x11:
                    GameNet_BeginFileTransfer((FileRequestWireMsg *)pWire, lpMem->fromPlayerId);
                    break;
                case 0x12:
                    GameNet_HandleFileTransferBlock((FileBlockWireMsg *)pWire);
                    break;
                case 4: {
                    ClipartFileWireMsg *pMsg = (ClipartFileWireMsg *)pWire;
                    if (pMsg != 0) {
                        g_pPostBagCache->PostBag_BuildClipartFilePath(pMsg->bDescByte, pMsg->bIndexByte, szPath);
                        hFile = CreateFileA(szPath, 0x40000000, 0, 0, 1, 0x80, 0);
                        if (hFile != INVALID_HANDLE_VALUE) {
                            WriteFile(hFile, pMsg->data, pMsg->nDataLen, &dwWritten, 0);
                            CloseHandle(hFile);
                            nPendingFileReceiveCount--;
                        }
                        if (nPendingFileReceiveCount == 0)
                            g_pNetManager->DPlay_TeardownConnection();
                    }
                    break;
                }
                case 3:
                case 5:
                    break;
                }
        }
        goto post_switch;

    resolve_and_enqueue:
        pNode->bReliable = g_pDPlaySessionMgr->ResolveIdToSlot(nSenderId);
        g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);

    post_switch:
        wOpcode = *pWire;
        if (wOpcode < 0x3f6 || (wOpcode > 0x3f7 && wOpcode != 0x3f9))
            HeapFree(GetProcessHeap(), 0, lpMem->pPacket);
        HeapFree(GetProcessHeap(), 0, lpMem);
    }
}

// FUNCTION: LOCO 0x439550
void GameNetThreadState::GameNet_ProcessLocalCommand(NetMsgQueueNode *pNode) {
    // Case bodies below are ordered to match the ORIGINAL's real memory layout
    // (0,1,2,3,6,0xe,0x10,8-falls-into-5,0x19), NOT numeric case-value order -- confirmed via
    // raw disasm (0x43962d/0x43963a/0x439647/0x439657) that case 8/5's body sits AFTER cases
    // 0xe and 0x10, not right after case 6 as Ghidra's own decompile text prints it.
    switch (pNode->type) {
    case 0:
        if (g_pNetManager == 0)
            GameNetThread_ResetNetManager();
        g_pNetManager->DPlay_TeardownConnection();
        g_pNetManager->InitBigFields(pNode->pPayload != 0, pNode->bReliable, 0, 1);
        pNode->pPayload = 0;
        return;
    case 1:
        DPlay_PrepareInternetConnection(pNode);
        return;
    case 2:
        DPlay_BuildOtherSessionsList(pNode);
        return;
    case 3:
        AttemptJoinOrHostSession();
        return;
    case 6: {
        int bReliable = 1;
        if (g_pNetManager != 0 && g_pNetManager->bSessionJoined != 0) {
            if (pNode->bReliable == 0)
                bReliable = 0;
            g_pNetManager->DPlay_SendMessage(pNode->destPlayerId, pNode->pPayload,
                                              pNode->payloadLen, bReliable);
        }
        if (pNode->pPayload != 0) {
            operator delete(pNode->pPayload);
            pNode->pPayload = 0;
            return;
        }
        break;
    }
    case 0xe:
        GameNet_ConnectOrJoinSession(pNode);
        return;
    case 0x10:
        TrainNet_HandleMoveRequest(pNode);
        return;
    case 8:
        bShutdownRequestedMaybe = 1;
    case 5:
        GameNet_TeardownAndFlushQueues();
        return;
    case 0x19:
        GameNet_RemoveOrRehomeNode(g_pDPlaySessionMgr->selectedProviderIndex);
        GameNet_RemovePeerTrainsForPlayer(0);
    }
}

// FUNCTION: LOCO 0x45e700
void GNetManager::InitBigFields(unsigned char bA, int nB, unsigned char bC, unsigned char bD) {
    bIsHost = bA;
    nMaxPlayers = nB;
    bJoinAttempted = bC;
    bAllowHostMigration = bD;
}

// FUNCTION: LOCO 0x43a760
// Local command opcode 1 ("prepare internet/direct connection", GameNet_ProcessLocalCommand
// case 1): build the DirectPlay connect-address string + protocol id to hand to
// DPlay_InitConnection (whose 3rd arg is the .ini-configured Port, default 31415/0x7ab7,
// sic -- the same string is read regardless of which branch below actually uses it). If
// hosting, use the SECONDARY remembered protocol choice (itoa'ing its own per-protocol custom
// value when ==3, else leaving the address buffer at its initial near-empty state); otherwise,
// if connectionMode==1 ("network game active"), read Protocol/Address straight from the .ini
// (default "loco.legomedia.com"); otherwise use the PRIMARY remembered protocol choice,
// copying its remembered address string (1/2, two separate per-protocol buffers) or itoa'ing
// its own custom value (3). pNode is a real but unread parameter (sic, matches the callee's
// ret 0x4 cleanup at every call site).
void GameNetThreadState::DPlay_PrepareInternetConnection(NetMsgQueueNode *pNode) {
    char szAddr[1024] = "";

    int nPort = g_pIniFile->ReadInt("Configuration", "Port", 0x7ab7);
    int nProtocol;
    char *pSrc;

    if (g_pNetManager->bIsHost != 0) {
        if (g_pNetSettings->rememberedProtocolSecondary == 3)
            _itoa(g_pNetSettings->nRememberedCustomValueSecondary, szAddr, 10);
        nProtocol = g_pNetSettings->rememberedProtocolSecondary;
        goto sendConnect;
    }
    if (g_pDPlaySessionMgr->connectionMode != 1) {
        nProtocol = g_pNetSettings->rememberedProtocolPrimary;
        switch (nProtocol) {
        case 1:
            pSrc = g_pNetSettings->szRememberedAddrPrimary;
copyAddr:
            strcpy(szAddr, pSrc);
            break;
        case 2:
            pSrc = g_pNetSettings->szRememberedAddrPrimaryAlt;
            goto copyAddr;
        case 3:
            _itoa(g_pNetSettings->nRememberedCustomValuePrimary, szAddr, 10);
            nProtocol = g_pNetSettings->rememberedProtocolPrimary;
            goto sendConnect;
        }
        nProtocol = g_pNetSettings->rememberedProtocolPrimary;
    } else {
        nProtocol = g_pIniFile->ReadInt("Configuration", "Protocol", 2);
        g_pIniFile->ReadString("Configuration", "Address", "loco.legomedia.com", szAddr,
                                     sizeof(szAddr));
    }
sendConnect:
    g_pNetManager->DPlay_InitConnection(nProtocol, szAddr, nPort);
}

// LEGO Loco's own registered DirectPlay guidApplication, pinned via the VA->file-offset raw-byte
// technique against the 4 dwords at 0x479158 (see GNetManager.h).
const GUID g_guidLocoApp = {0xf9cd2546, 0x577f, 0x11d2, {0x94, 0x26, 0x00, 0xa0, 0x24, 0x4b, 0xda, 0x7a}};

// DirectPlay service-provider + address-element-type GUIDs used by DPlay_InitConnection, all
// pinned via the VA->file-offset raw-byte-read technique (see GNetManager.h for per-constant
// provenance notes).
const GUID g_guidDPSPModem  = {0x44eaa760, 0xcb68, 0x11cf, {0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
const GUID g_guidDPSPTcpIp  = {0x36e95ee0, 0x8577, 0x11cf, {0x96, 0x0c, 0x00, 0x80, 0xc7, 0x53, 0x4e, 0x82}};
const GUID g_guidDPSPSerial = {0x0f1d6860, 0x88d9, 0x11cf, {0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
const GUID g_guidDPSPIpx    = {0x685bc400, 0x9d2c, 0x11cf, {0xa9, 0xcd, 0x00, 0xaa, 0x00, 0x68, 0x86, 0xe3}};
const GUID g_guidDPAIDServiceProvider = {0x07d916c0, 0xe0af, 0x11cf, {0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
const GUID g_guidDPAIDPhoneMaybe           = {0xf6dcc200, 0xa2fe, 0x11d0, {0x9c, 0x4f, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
const GUID g_guidDPAIDModemNameMaybe       = {0x78ec89a0, 0xe0af, 0x11cf, {0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
const GUID g_guidDPAIDINetMaybe            = {0xc4a54da0, 0xe0af, 0x11cf, {0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
const GUID g_guidDPAIDInetPortMaybe        = {0xe4524541, 0x8ea5, 0x11d1, {0x8a, 0x96, 0x00, 0x60, 0x97, 0xb0, 0x14, 0x11}};
const GUID g_guidDPAIDComSettingsMaybe     = {0xf2f0ce00, 0xe0af, 0x11cf, {0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e}};
const GUID g_iidDirectPlay4  = {0x0ab1c531, 0x4745, 0x11d1, {0xa7, 0xa1, 0x00, 0x00, 0xf8, 0x03, 0xab, 0xfc}};
const GUID g_clsidDirectPlay = {0xd1eb6d20, 0x8923, 0x11d0, {0x9d, 0x97, 0x00, 0xa0, 0xc9, 0x0a, 0x43, 0xcb}};

// FUNCTION: LOCO 0x45f2b0 (?DPlay_EnumSessionsCallback@@YGHPAUDPSessionDesc2Partial@@PAIIPAX@Z)
// EXACT MATCH (211/211 bytes). DPENUMSESSIONSCALLBACK2 body for DPlay_FindSession's own
// EnumSessions call (dwFlags bit 0 = DPESC_TIMEDOUT). Non-timeout calls (a session was found)
// operator-new/build+prepend a FoundSessionNode onto g_pNetManager->pListHead1: the
// name buffer is sized off a real strlen() (not the fixed 0x100 DPlay_BuildOtherSessionsList's
// own copy uses), and the session GUID is copied into a fresh GlobalAlloc'd/locked 16-byte block
// (matches DPlay_FindSession's own GlobalHandle/GlobalUnlock/GlobalFree teardown of that same
// field). Timeout calls (lpdwTimeOut deref never read -- only dwFlags matters) return TRUE (keep
// enumerating) only while nProtocol == 1 AND no session has been found yet, else FALSE
// (stop) -- nProtocol is set elsewhere (not yet identified; likely DPlay_InitConnection or
// DPlay_JoinOrHostSession's own EnumSessions-vs-single-session-search mode selector).
// Needed the branch-order lever (Yoda "if/else fall-through polarity" family, CLAUDE.md): the
// ORIGINAL's fall-through (no jump) path is the TIMEOUT check, with the build-node case as the
// forward jump target -- a first attempt writing `if (!timeout) {build} else if(...) return 0;`
// (matching Ghidra's own decompile order) put build-node on the fall-through instead and scored
// byte_diff 89/38 wrong-side insns; swapping to `if (timeout) {...} else {build}` (timeout as the
// primary condition) matched byte-for-byte on the next attempt.
int __stdcall DPlay_EnumSessionsCallback(DPSessionDesc2Partial *lpDPSessionDesc,
                                               unsigned int *lpdwTimeOut, unsigned int dwFlags,
                                               void *lpContext) {
    if ((dwFlags & 1) != 0) {
        if (g_pNetManager->nProtocol != 1 || g_pNetManager->pListHead1 != 0) {
            return 0;
        }
    } else {
        FoundSessionNode *pNode = (FoundSessionNode *)operator new(sizeof(FoundSessionNode));
        char *pszName = (char *)operator new(strlen(lpDPSessionDesc->lpszSessionNameA) + 0x18);
        pNode->pszName = pszName;
        strcpy(pszName, lpDPSessionDesc->lpszSessionNameA);

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, 0x10);
        GUID *pGuidCopy = (GUID *)GlobalLock(hMem);
        *pGuidCopy = lpDPSessionDesc->guidInstance;
        pNode->pSessionDescMem = pGuidCopy;

        pNode->pNext = g_pNetManager->pListHead1;
        g_pNetManager->pListHead1 = pNode;
    }
    return 1;
}

// The real Win32 DPNAME (ANSI variant), built inline on the stack by DPlay_JoinOrHostSession
// right before its own CreatePlayer call -- dwSize 0x10 (sizeof this), dwFlags 0,
// lpszShortNameA left at the shared empty-scratch string, lpszLongNameA the real player name.
struct DPNamePartial {
    unsigned int dwSize;      // +0x0
    unsigned int dwFlags;     // +0x4
    char *lpszShortNameA;     // +0x8
    char *lpszLongNameA;      // +0xc
};

// A throwaway, per-TU padded-vtable probe for IDirectPlay4 (this project never includes
// <dplay.h> -- pDirectPlay4 is a raw void* everywhere, DirectPlay is called via manual
// vtable dispatch). Slots 0-12 are IUnknown + the 10 DirectPlay methods preceding EnumSessions
// (QueryInterface/AddRef/Release/AddPlayerToGroup/Close/CreateGroup/CreatePlayer/
// DeletePlayerFromGroup/DestroyGroup/EnableNewPlayers/EnumGroupPlayers/EnumGroups/EnumPlayers) --
// CORRECTED: CreatePlayer is call-site shape-matched (6 args: lpidPlayer/lpPlayerName/hEvent/
// lpData/dwDataSize/dwFlags) to real vtbl+0x18 (slot 6), one slot LATER than a naive per-header
// name order would suggest -- see docs/subsystems.md's own "call-site shape-matched" note.
// EnumSessions (vtbl+0x34, slot 13) and SetSessionDesc (vtbl+0x38, slot 14) are the only other
// two slots given real signatures, matching the established "padded-dummy-virtuals probe
// struct" idiom (src/NetSessionEventQueue.cpp) -- EXTENDED here to a real COM interface: COM
// vtables are STDMETHODCALLTYPE (__stdcall, `this` pushed as an ordinary first stack arg,
// callee cleanup), not the plain-C++-class default __thiscall (`this` in ECX)
// NetSessionEventQueue.cpp's own probes use.
// CORRECTED (DPlay_InitConnection, 2026-07-20): slots 0/2 (IUnknown QueryInterface/Release) and
// slot 18 (vtbl+0x48, a GetCaps-shaped 3-arg method: called twice, once with a NULL out-buffer to
// size a scratch block and once with it filled -- exact DirectPlay method name unconfirmed) and
// slot 38 (vtbl+0x98, IDirectPlay4::InitializeConnection) are also given real signatures, since
// DPlay_InitConnection is the first consumer to need the temp-interface teardown and the final
// connect step.
struct IDirectPlay4VtblProbe {
    virtual long __stdcall QueryInterfaceImpl(const GUID *riid, void **ppvObj);
    virtual void __stdcall Unused01Impl();
    virtual unsigned long __stdcall ReleaseImpl();
    virtual void __stdcall Unused03Impl();
    // vtbl+0x10, slot 4 -- IDirectPlay4::Close(). DPlay_TeardownConnection's only caller.
    virtual unsigned int __stdcall CloseImpl();
    virtual void __stdcall Unused05Impl();
    virtual unsigned int __stdcall CreatePlayerImpl(int *lpidPlayer, DPNamePartial *lpPlayerName,
                                                      void *hEvent, void *lpData,
                                                      unsigned int dwDataSize,
                                                      unsigned int dwFlags);
    virtual void __stdcall Unused07Impl();
    virtual void __stdcall Unused08Impl(); virtual void __stdcall Unused09Impl();
    virtual void __stdcall Unused10Impl(); virtual void __stdcall Unused11Impl();
    virtual void __stdcall Unused12Impl();
    virtual unsigned int __stdcall EnumSessionsImpl(DPSessionDesc2Partial *lpsd,
                                                      unsigned int dwTimeout,
                                                      void *lpEnumSessionsCallback2,
                                                      void *lpContext, unsigned int dwFlags);
    virtual unsigned int __stdcall SetSessionDescImpl(void *lpSessDesc, unsigned int dwFlags);
    virtual void __stdcall Unused15Impl(); virtual void __stdcall Unused16Impl();
    virtual void __stdcall Unused17Impl();
    // vtbl+0x48, slot 18 -- GetCaps-shaped: called (this,0,0,&scratch) to size a modem-caps
    // blob, then (this,0,pFilledBlob,&scratch) to fill it. Real DirectPlay method name
    // unconfirmed; modeled by observed call shape only.
    virtual unsigned int __stdcall GetConnectionCapsMaybeImpl(unsigned int dwFlags, void *pArg2,
                                                                 void *pOutBuf);
    virtual void __stdcall Unused19Impl(); virtual void __stdcall Unused20Impl();
    virtual void __stdcall Unused21Impl(); virtual void __stdcall Unused22Impl();
    virtual void __stdcall Unused23Impl();
    // vtbl+0x60, slot 24 -- IDirectPlay4::Open(LPDPSESSIONDESC2 lpsd, DWORD dwFlags).
    virtual unsigned int __stdcall OpenImpl(DPSessionDesc2Partial *lpsd, unsigned int dwFlags);
    // vtbl+0x64, slot 25 -- IDirectPlay4::Receive(LPDPID lpidFrom, LPDPID lpidTo, DWORD dwFlags,
    // LPVOID lpData, LPDWORD lpdwDataSize). Confirmed via DPlay_ReceiveAndDispatch's own raw
    // disasm (2026-07-20): args pushed this,lpidFrom,lpidTo,dwFlags,lpData,lpdwDataSize.
    virtual unsigned int __stdcall ReceiveImpl(int *lpidFrom, int *lpidTo, unsigned int dwFlags,
                                                 void *lpData, unsigned int *lpdwDataSize);
    // vtbl+0x68, slot 26 -- IDirectPlay4::Send(DPID idFrom, DPID idTo, DWORD dwFlags,
    // LPVOID lpData, DWORD dwDataSize). Confirmed via DPlay_SendMessage's own raw disasm
    // (2026-07-20): args pushed this,idFrom,idTo,dwFlags,lpData,dwDataSize.
    virtual unsigned int __stdcall SendImpl(int idFrom, int idTo, unsigned int dwFlags,
                                              void *lpData, unsigned int dwDataSize);
    virtual void __stdcall Unused27Impl(); virtual void __stdcall Unused28Impl();
    virtual void __stdcall Unused29Impl(); virtual void __stdcall Unused30Impl();
    virtual void __stdcall Unused31Impl(); virtual void __stdcall Unused32Impl();
    virtual void __stdcall Unused33Impl(); virtual void __stdcall Unused34Impl();
    virtual void __stdcall Unused35Impl(); virtual void __stdcall Unused36Impl();
    virtual void __stdcall Unused37Impl();
    // vtbl+0x98, slot 38 -- IDirectPlay4::InitializeConnection(LPVOID lpConnection, DWORD dwFlags).
    virtual unsigned int __stdcall InitializeConnectionImpl(void *lpConnection,
                                                               unsigned int dwFlags);
    virtual void __stdcall Unused39Impl(); virtual void __stdcall Unused40Impl();
    virtual void __stdcall Unused41Impl(); virtual void __stdcall Unused42Impl();
    virtual void __stdcall Unused43Impl(); virtual void __stdcall Unused44Impl();
    virtual void __stdcall Unused45Impl(); virtual void __stdcall Unused46Impl();
    virtual void __stdcall Unused47Impl(); virtual void __stdcall Unused48Impl();
    // vtbl+0xc4, slot 49 -- IDirectPlay4::SendEx(DPID idFrom, DPID idTo, DWORD dwFlags,
    // LPVOID lpData, DWORD dwDataSize, DWORD dwPriority, DWORD dwTimeout, LPVOID lpContext,
    // LPDWORD lpdwMsgID). Confirmed via DPlay_SendMessage's own raw disasm (2026-07-20): all
    // 4 trailing args (dwPriority/dwTimeout/lpContext/lpdwMsgID) are always 0 at this call site.
    virtual unsigned int __stdcall SendExImpl(int idFrom, int idTo, unsigned int dwFlags,
                                                void *lpData, unsigned int dwDataSize,
                                                unsigned int dwPriority, unsigned int dwTimeout,
                                                void *lpContext, unsigned int *lpdwMsgID);
    // vtbl+0xc8, slot 50 -- IDirectPlay4::GetMessageQueue(DPID idFrom, DPID idTo, DWORD dwFlags,
    // LPDWORD lpdwNumMsgs, LPDWORD lpdwNumBytes). Confirmed via DPlay_SendMessage's own raw
    // disasm (2026-07-20): called (0, 0, DPGETMSGQUEUE_SEND=1, &dwNumMsgs, 0) to check the
    // outbound send queue depth.
    virtual unsigned int __stdcall GetMessageQueueImpl(int idFrom, int idTo, unsigned int dwFlags,
                                                          unsigned int *lpdwNumMsgs,
                                                          unsigned int *lpdwNumBytes);
    // vtbl+0xcc, slot 51 -- IDirectPlay4::CancelMessage(DWORD dwMsgID, DWORD dwFlags).
    // DPlay_TeardownConnection calls it (0, 0) = cancel every queued message.
    virtual unsigned int __stdcall CancelMessageImpl(unsigned int dwMsgID, unsigned int dwFlags);

    long QueryInterface(const GUID *riid, void **ppvObj) {
        return QueryInterfaceImpl(riid, ppvObj);
    }
    unsigned long Release() { return ReleaseImpl(); }
    unsigned int Close() { return CloseImpl(); }
    unsigned int CancelMessage(unsigned int dwMsgID, unsigned int dwFlags) {
        return CancelMessageImpl(dwMsgID, dwFlags);
    }
    unsigned int CreatePlayer(int *lpidPlayer, DPNamePartial *lpPlayerName, void *hEvent,
                               void *lpData, unsigned int dwDataSize, unsigned int dwFlags) {
        return CreatePlayerImpl(lpidPlayer, lpPlayerName, hEvent, lpData, dwDataSize, dwFlags);
    }
    unsigned int EnumSessions(DPSessionDesc2Partial *lpsd, unsigned int dwTimeout,
                               void *lpEnumSessionsCallback2, void *lpContext,
                               unsigned int dwFlags) {
        return EnumSessionsImpl(lpsd, dwTimeout, lpEnumSessionsCallback2, lpContext, dwFlags);
    }
    unsigned int SetSessionDesc(void *lpSessDesc, unsigned int dwFlags) {
        return SetSessionDescImpl(lpSessDesc, dwFlags);
    }
    unsigned int GetConnectionCapsMaybe(unsigned int dwFlags, void *pArg2, void *pOutBuf) {
        return GetConnectionCapsMaybeImpl(dwFlags, pArg2, pOutBuf);
    }
    unsigned int Open(DPSessionDesc2Partial *lpsd, unsigned int dwFlags) {
        return OpenImpl(lpsd, dwFlags);
    }
    unsigned int Receive(int *lpidFrom, int *lpidTo, unsigned int dwFlags, void *lpData,
                          unsigned int *lpdwDataSize) {
        return ReceiveImpl(lpidFrom, lpidTo, dwFlags, lpData, lpdwDataSize);
    }
    unsigned int InitializeConnection(void *lpConnection, unsigned int dwFlags) {
        return InitializeConnectionImpl(lpConnection, dwFlags);
    }
    unsigned int Send(int idFrom, int idTo, unsigned int dwFlags, void *lpData,
                       unsigned int dwDataSize) {
        return SendImpl(idFrom, idTo, dwFlags, lpData, dwDataSize);
    }
    unsigned int SendEx(int idFrom, int idTo, unsigned int dwFlags, void *lpData,
                         unsigned int dwDataSize, unsigned int dwPriority, unsigned int dwTimeout,
                         void *lpContext, unsigned int *lpdwMsgID) {
        return SendExImpl(idFrom, idTo, dwFlags, lpData, dwDataSize, dwPriority, dwTimeout,
                           lpContext, lpdwMsgID);
    }
    unsigned int GetMessageQueue(int idFrom, int idTo, unsigned int dwFlags,
                                  unsigned int *lpdwNumMsgs, unsigned int *lpdwNumBytes) {
        return GetMessageQueueImpl(idFrom, idTo, dwFlags, lpdwNumMsgs, lpdwNumBytes);
    }
};

// A throwaway padded-vtable probe for the SECONDARY DirectPlay object DPlay_InitConnection uses
// (GNetManager::pDPlaySecondary, +0x15e0 -- IDirectPlayLobby3-shaped, created elsewhere by
// the real ctor/DPlayLobby_Init). Only 2 real slots needed so far: EnumAddress (vtbl+0x14, the
// modem phone-number-extraction callback's driver) and CreateCompoundAddress (vtbl+0x38, builds
// the serialized address blob passed to IDirectPlay4::InitializeConnection).
struct IDPlaySecondaryVtblProbe {
    virtual long __stdcall QueryInterfaceImpl(const GUID *riid, void **ppvObj);
    virtual void __stdcall Unused01Impl();
    // vtbl+0x0 / +0x8, slots 0 and 2 -- IUnknown::QueryInterface / Release. The ctor uses both
    // to turn DirectPlayLobbyCreateA's object into an IDirectPlayLobby3A; the dtor uses Release.
    virtual unsigned long __stdcall ReleaseImpl();
    virtual void __stdcall Unused03Impl();
    virtual void __stdcall Unused04Impl();
    // vtbl+0x14, slot 5 -- call shape (this, lpCallback, lpAddress, dwAddressSize, lpContext);
    // exact DirectPlay method name unconfirmed.
    virtual unsigned int __stdcall EnumAddressImpl(void *pCallback, void *lpAddress,
                                                           unsigned int dwAddressSize,
                                                           void *lpContext);
    virtual void __stdcall Unused06Impl(); virtual void __stdcall Unused07Impl();
    virtual void __stdcall Unused08Impl(); virtual void __stdcall Unused09Impl();
    virtual void __stdcall Unused10Impl(); virtual void __stdcall Unused11Impl();
    virtual void __stdcall Unused12Impl(); virtual void __stdcall Unused13Impl();
    // vtbl+0x38, slot 14 -- IDirectPlayLobby3::CreateCompoundAddress(lpElements, dwElementCount,
    // lpAddress, lpdwAddressSize).
    virtual unsigned int __stdcall CreateCompoundAddressImpl(
        DPCompoundAddressElement *lpElements, unsigned int dwElementCount, void *lpAddress,
        unsigned int *lpdwAddressSize);

    long QueryInterface(const GUID *riid, void **ppvObj) {
        return QueryInterfaceImpl(riid, ppvObj);
    }
    unsigned long Release() { return ReleaseImpl(); }
    unsigned int EnumAddress(void *pCallback, void *lpAddress, unsigned int dwAddressSize,
                                   void *lpContext) {
        return EnumAddressImpl(pCallback, lpAddress, dwAddressSize, lpContext);
    }
    unsigned int CreateCompoundAddress(DPCompoundAddressElement *lpElements,
                                        unsigned int dwElementCount, void *lpAddress,
                                        unsigned int *lpdwAddressSize) {
        return CreateCompoundAddressImpl(lpElements, dwElementCount, lpAddress, lpdwAddressSize);
    }
};

extern int __stdcall DPlay_SelectConnectionDlgProc(HWND hDlg, unsigned int uMsg,
                                                      unsigned int wParam, unsigned int lParam);

// FUNCTION: LOCO 0x45f390
// See GNetManager.h for the full behavior writeup. PARTIAL transcription -- the Modem branch
// (case 1) is the rarest path (never exercised by LEGO Loco's own UI flows, which only ever pass
// nProtocol==2/TCP-IP) and its exact vtbl+0x48/EnumAddress argument semantics are call-site
// shape-matched rather than confirmed against a real DirectPlay SDK header (this project never
// includes <dplay.h>).
unsigned int GNetManager::DPlay_InitConnection(int nProtocolArg, char *pszSessionName, int nPort) {
    if (pDirectPlay4 != 0) {
        DPlay_TeardownConnection();
    }
    sConnectParam[0] = 0;
    if (nProtocolArg == 0) {
        nProtocol = 0;
        sConnectParam[0] = 0;
        nUnk0x934Maybe = 0;
        if (DialogBoxParamA(0, (LPCSTR)0x7d0a, hWndParent,
                             (DLGPROC)DPlay_SelectConnectionDlgProc, 0) == 0) {
            return 0;
        }
    } else {
        nProtocol = nProtocolArg;
        if (pszSessionName != 0) {
            strcpy(sConnectParam, pszSessionName);
        }
        if (nPort != 0) {
            wPortOrExt = (unsigned short)nPort;
        } else {
            wPortOrExt = 0;
        }
    }

    GUID guidSp;
    switch (nProtocol) {
    case 1: guidSp = g_guidDPSPModem; break;
    case 2: guidSp = g_guidDPSPTcpIp; break;
    case 3: guidSp = g_guidDPSPSerial; break;
    case 4: guidSp = g_guidDPSPIpx; break;
    default: return 0;
    }

    hrLastResult = DirectPlayCreate(&guidSp, &pTempDPlayIface, 0);
    if (hrLastResult != 0) {
        char szErrBuf[212];
        DPlay_FormatHresultString(szErrBuf, hrLastResult);
        char szMsgBuf[300];
        wsprintfA(szMsgBuf, "Direct Play Create failed (SetNetwork)...\r\r %s", szErrBuf);
        DPlay_ReportNetworkError(0, szMsgBuf);
        return 0;
    }

    hrLastResult = ((IDirectPlay4VtblProbe *)pTempDPlayIface)
                             ->QueryInterface(&g_iidDirectPlay4, &pDirectPlay4);
    if (hrLastResult != 0) {
        char szErrBuf[212];
        DPlay_FormatHresultString(szErrBuf, hrLastResult);
        char szMsgBuf[300];
        wsprintfA(szMsgBuf, "Direct Play Query Interface failed (SetNetwork)...\r\r %s", szErrBuf);
        DPlay_ReportNetworkError(0, szMsgBuf);
        return 0;
    }

    ((IDirectPlay4VtblProbe *)pTempDPlayIface)->Release();
    pTempDPlayIface = 0;

    DPCompoundAddressElement elems[3];
    unsigned int nElements;

    switch (nProtocol) {
    case 1: {
        elems[0].guidDataType = g_guidDPAIDServiceProvider;
        elems[0].dwDataSize = sizeof(GUID);
        elems[0].lpData = (void *)&g_guidDPSPModem;
        unsigned int dwCapsSize = 0;
        ((IDirectPlay4VtblProbe *)pDirectPlay4)->GetConnectionCapsMaybe(0, 0, &dwCapsSize);
        HGLOBAL hCaps = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, dwCapsSize);
        void *pCaps = GlobalLock(hCaps);
        hrLastResult =
            ((IDirectPlay4VtblProbe *)pDirectPlay4)->GetConnectionCapsMaybe(0, pCaps, &dwCapsSize);
        if (hrLastResult != 0) {
            if (pCaps != 0) {
                GlobalUnlock(GlobalHandle(pCaps));
                GlobalFree(GlobalHandle(pCaps));
            }
            char szErrBuf[212];
            DPlay_FormatHresultString(szErrBuf, hrLastResult);
            char szMsgBuf[300];
            wsprintfA(szMsgBuf, "Modem selection failed (not available)...\r\r %s", szErrBuf);
            DPlay_ReportNetworkError(0, szMsgBuf);
            return 0;
        }

        sPhoneNumber[0] = 0;
        hrLastResult = ((IDPlaySecondaryVtblProbe *)pDPlaySecondary)
                                 ->EnumAddress((void *)DPlay_EnumAddressCallback, pCaps, dwCapsSize, 0);
        if (hrLastResult != 0) {
            if (pCaps != 0) {
                GlobalUnlock(GlobalHandle(pCaps));
                GlobalFree(GlobalHandle(pCaps));
            }
            char szErrBuf[212];
            DPlay_FormatHresultString(szErrBuf, hrLastResult);
            char szMsgBuf[300];
            wsprintfA(szMsgBuf, "Modem selection failed (not extractable)...\r\r %s", szErrBuf);
            DPlay_ReportNetworkError(0, szMsgBuf);
            return 0;
        }
        if (pCaps != 0) {
            GlobalUnlock(GlobalHandle(pCaps));
            GlobalFree(GlobalHandle(pCaps));
        }

        elems[1].guidDataType = g_guidDPAIDPhoneMaybe;
        elems[1].dwDataSize = strlen(sPhoneNumber) + 1;
        elems[1].lpData = sPhoneNumber;
        elems[2].guidDataType = g_guidDPAIDModemNameMaybe;
        elems[2].dwDataSize = strlen(sConnectParam) + 1;
        elems[2].lpData = sConnectParam;
        nElements = 3;
        break;
    }
    case 2:
        elems[0].guidDataType = g_guidDPAIDServiceProvider;
        elems[0].dwDataSize = sizeof(GUID);
        elems[0].lpData = (void *)&g_guidDPSPTcpIp;
        elems[1].guidDataType = g_guidDPAIDINetMaybe;
        elems[1].dwDataSize = strlen(sConnectParam) + 1;
        elems[1].lpData = sConnectParam;
        nElements = 2;
        if (wPortOrExt != 0) {
            elems[2].guidDataType = g_guidDPAIDInetPortMaybe;
            elems[2].dwDataSize = sizeof(unsigned short);
            elems[2].lpData = &wPortOrExt;
            nElements = 3;
        }
        break;
    case 3:
        elems[0].guidDataType = g_guidDPAIDServiceProvider;
        elems[0].dwDataSize = sizeof(GUID);
        elems[0].lpData = (void *)&g_guidDPSPSerial;
        nElements = 1;
        if (sConnectParam[0] != 0) {
            serialSettings.nPortNum = 0;
            serialSettings.nBaudRate = 0;
            serialSettings.nUnk8Maybe = 0;
            serialSettings.nUnk0xcMaybe = 0;
            serialSettings.nByteSizeMaybe = 0;
            unsigned int nPort = atoi(sConnectParam);
            serialSettings.nPortNum = nPort;
            if (nPort < 1 || nPort > 4) {
                serialSettings.nPortNum = 0;
            }
            serialSettings.nBaudRate = 0x9600;
            serialSettings.nUnk8Maybe = 0;
            serialSettings.nUnk0xcMaybe = 0;
            serialSettings.nByteSizeMaybe = 2;
            elems[1].guidDataType = g_guidDPAIDComSettingsMaybe;
            elems[1].dwDataSize = sizeof(serialSettings);
            elems[1].lpData = &serialSettings;
            nElements = 2;
        }
        break;
    case 4:
        elems[0].guidDataType = g_guidDPAIDServiceProvider;
        elems[0].dwDataSize = sizeof(GUID);
        elems[0].lpData = (void *)&g_guidDPSPIpx;
        nElements = 1;
        break;
    default:
        return 0;
    }

    char addrBuf[0x1000];
    unsigned int dwAddrSize = sizeof(addrBuf);
    hrLastResult = ((IDPlaySecondaryVtblProbe *)pDPlaySecondary)
                             ->CreateCompoundAddress(elems, nElements, addrBuf, &dwAddrSize);
    if (hrLastResult != 0) {
        char szErrBuf[212];
        DPlay_FormatHresultString(szErrBuf, hrLastResult);
        char szMsgBuf[300];
        wsprintfA(szMsgBuf, "Failed to create Direct Play address\r\rDirect Play code: %s", szErrBuf);
        DPlay_ReportNetworkError(0, szMsgBuf);
        goto teardown_fail;
    }

    ((IDirectPlay4VtblProbe *)pDirectPlay4)->Release();
    pDirectPlay4 = 0;

    hrLastResult = CoCreateInstance(g_clsidDirectPlay, 0, CLSCTX_INPROC_SERVER,
                                          g_iidDirectPlay4, &pDirectPlay4);
    if (hrLastResult != 0) {
        GUID guidZero = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
        hrLastResult = DirectPlayCreate(&guidZero, &pTempDPlayIface, 0);
        if (hrLastResult != 0) {
            char szErrBuf[212];
            DPlay_FormatHresultString(szErrBuf, hrLastResult);
            char szMsgBuf[300];
            wsprintfA(szMsgBuf, "Direct Play final Create failed (SetNetwork)...\r\r %s", szErrBuf);
            DPlay_ReportNetworkError(0, szMsgBuf);
            // sic: unlike every other error exit in this function, the original does NOT
            // call DPlay_TeardownConnection() here -- confirmed via raw disasm (0x45fa8b-
            // 0x45fad0, no call to 0x45fc30) -- likely an oversight, since pDirectPlay4
            // may already be partially set from the CoCreateInstance fallback path.
            return 0;
        }

        hrLastResult = ((IDirectPlay4VtblProbe *)pTempDPlayIface)
                                 ->QueryInterface(&g_iidDirectPlay4, &pDirectPlay4);
        if (hrLastResult != 0) {
            char szErrBuf[212];
            DPlay_FormatHresultString(szErrBuf, hrLastResult);
            char szMsgBuf[300];
            wsprintfA(szMsgBuf, "Direct Play final Query Interface failed (SetNetwork)...\r\r %s",
                      szErrBuf);
            DPlay_ReportNetworkError(0, szMsgBuf);
            // sic: same as above -- no DPlay_TeardownConnection() call in the original here.
            return 0;
        }

        ((IDirectPlay4VtblProbe *)pTempDPlayIface)->Release();
        pTempDPlayIface = 0;
    }

    hrLastResult = ((IDirectPlay4VtblProbe *)pDirectPlay4)->InitializeConnection(addrBuf, 0);
    if (hrLastResult != 0) {
        char szErrBuf[212];
        DPlay_FormatHresultString(szErrBuf, hrLastResult);
        char szMsgBuf[300];
        wsprintfA(szMsgBuf, "Direct Play Initialise Connection FAILED\r\rDirect Play code: %s",
                  szErrBuf);
        DPlay_ReportNetworkError(0, szMsgBuf);
        goto teardown_fail;
    }
    return 1;

teardown_fail:
    DPlay_TeardownConnection();
    return 0;
}

// FUNCTION: LOCO 0x45f090
// pszPasswordFilter truncate-copy uses the compiler's own strcpy intrinsic (repnz scasb +
// rep movsd/movsb shape) both for the "fits" case and, after a temporary null-poke at index
// 0x80, for the truncate case. sic: the truncate case's strcpy therefore copies the poked null
// TOO (0x81 bytes total, including the terminator), overflowing sPassword (a 0x80-byte
// field) by 1 byte into the following (currently unmodeled) padding whenever the caller passes
// a password >= 0x80 chars -- unreachable today since every known caller passes NULL, but a
// real bug in the original for a sufficiently long password argument.
// EFFECTIVE MATCH (asmscore byte_diff 97 at true len 531, insns 178/177 -- every branch and
// call site structurally aligned, calling convention/vtable dispatch confirmed correct). Two
// intrinsic residual classes, both triage-budgeted without a source-level fix: (a) the
// truncate-case's saved byte (src[0x80], restored after the copy) lands in ECX here instead of
// the original's EBX -- ECX collides with `rep movs`' own count register inside the strcpy
// intrinsic, forcing a spill/reload pair around the call; tried hoisting the local's declaration
// to function-top (Yoda's SEH-placement-order lever), zero effect -- likely an intrinsic
// short-lived-value register tie-break (Yoda #29/#30 family), not source-steerable. This single
// register choice cascades into a broad register-swap ripple through most of the rest of the
// function (visible as dozens of `r`-marked reg-only diffs in `asmscore.py --dump`), inflating
// `align`/`reg_pen` far more than the 2 genuinely extra spill instructions alone would suggest.
// (b) the original keeps 3 SEPARATE physical epilogues (initial-failure tail, loop-exit-success
// tail, bJoinAttempted-early-exit tail) while this compiles the initial-failure and loop-exit-
// success tails (both a bare `return pListHead1;`) into ONE shared tail -- tried both a
// nested-if-wrapping-the-loop shape and an early-return-then-unconditional-loop shape (logically
// identical, differ only in whether the loop is lexically inside an `if`), byte-identical output
// either way; a self-contained block-layout/cross-jump choice (Yoda #15/#18), not source-
// steerable. Confirmed genuinely correct calling convention (COM STDMETHODCALLTYPE = __stdcall
// with `this` as an explicit first arg, NOT the plain-C++-class __thiscall default) via a first
// compile attempt that dropped the `this` push entirely -- see IDirectPlay4VtblProbe below.
FoundSessionNode *GNetManager::DPlay_FindSession(char *pszPasswordFilter) {
    sPassword[0] = 0;
    if (pszPasswordFilter != 0) {
        if (strlen(pszPasswordFilter) < 0x80) {
            strcpy(sPassword, pszPasswordFilter);
        } else {
            char cSaved = pszPasswordFilter[0x80];
            pszPasswordFilter[0x80] = 0;
            strcpy(sPassword, pszPasswordFilter);
            pszPasswordFilter[0x80] = cSaved;
        }
    }

    if (!bJoinAttempted) {
        while (pListHead1 != 0) {
            FoundSessionNode *pNext = pListHead1->pNext;
            if (pListHead1->pszName != 0) {
                operator delete(pListHead1->pszName);
            }
            if (pListHead1->pSessionDescMem != 0) {
                HGLOBAL hMem = GlobalHandle(pListHead1->pSessionDescMem);
                GlobalUnlock(hMem);
                hMem = GlobalHandle(pListHead1->pSessionDescMem);
                GlobalFree(hMem);
                pListHead1->pSessionDescMem = 0;
            }
            operator delete(pListHead1);
            pListHead1 = pNext;
        }
    }

    if (pDirectPlay4 == 0 && (char)DPlay_InitConnection(0, 0, 0) == 0) {
        return 0;
    }

    memset(&sessionDesc, 0, sizeof(sessionDesc));
    sessionDesc.dwSize = sizeof(sessionDesc);
    sessionDesc.guidApplication = g_guidLocoApp;
    if (sPassword[0] != 0) {
        sessionDesc.lpszPasswordA = sPassword;
    }

    hrLastResult = ((IDirectPlay4VtblProbe *)pDirectPlay4)->EnumSessions(
        &sessionDesc, 0, (void *)&DPlay_EnumSessionsCallback,
        hWndParent, 0x81);
    if (hrLastResult != 0x8877015e) {
        return pListHead1;
    }
    do {
        if (bJoinAttempted) {
            return 0;
        }
        if (pfnIdlePumpCallback != 0) {
            pfnIdlePumpCallback();
        }
        Sleep(1);
        hrLastResult = ((IDirectPlay4VtblProbe *)pDirectPlay4)->EnumSessions(
            &sessionDesc, 0, (void *)&DPlay_EnumSessionsCallback,
            hWndParent, 0x81);
    } while (hrLastResult == 0x8877015e);
    return pListHead1;
}

// FUNCTION: LOCO 0x45e730
// Session name copy has NO length cap (unconditional strcpy) -- unlike the player-name and
// password copies just below it, which share DPlay_FindSession's own truncate-copy idiom
// (strcpy under strlen<0x80, else a temporary null-poke at index 0x80, same 1-byte-overflow
// "sic" applies for a >=0x80-char argument -- see that function's own note).
// bIsHost written as the PRIMARY `if` condition (host branch first) to match the raw
// disasm's own fall-through order -- Ghidra's decompile prints the join branch first, but the
// actual compiled fall-through (no jump) is the host branch (branch-order lever, see CLAUDE.md).
// Likewise the host branch's own HostNewSessionMaybe check is written condition-inverted
// (`== 0` primary, CreatePlayer success as the `else`) to put the bJoinAttempted-retry check
// on the fall-through path, matching the original's own physical layout (fail-path code
// precedes the CreatePlayer success block in .text, reached via a forward jump on success).
// PARTIAL MATCH (asmscore byte_diff 201 at true len 882, insns 262/288 -- every branch
// polarity/target and the CreatePlayer/SetSessionDesc calling convention confirmed correct;
// structural gap is one class: the function's FIVE "DPlay_TeardownConnection(); return 0;"
// early-exit sites are each their OWN separate physical `call teardown; xor bl,bl; jmp`
// block in the original (byte-identical to each other, yet NOT tail-merged), while this
// compile's own /O2 cross-jump pass merges 4 of them into shared jumps to one another --
// same "goto-tail sharing is LOCAL/trace-driven, not globally forced by a repeated identical
// statement sequence" class already documented on this file's own DPlay_FindSession (3 vs 1
// physical epilogues) and in CLAUDE.md's cross-jump-geography lessons. Tried: per-branch
// distinct `goto` labels (no effect, the merge operates on compiled bytes not source labels);
// bypassing the shared `done:` epilogue entirely via bare `return 0;` at each site (WORSE --
// forces a full duplicated pop/pop/pop/ret epilogue per site, 309 insns vs the original's
// 288). The current `goto done;` shape is the best of the three tried and matches the
// original's INTENT (all paths funnel through one shared `test bl,bl` epilogue gate); the
// remaining over-merged prefixes are parked as an intrinsic optimizer difference, not a
// source-level bug.
char GNetManager::DPlay_JoinOrHostSession(char *pszPlayerName, char *pszSessionName,
                                           char *pszPassword) {
    sSessionName[0] = 0;
    if (pszSessionName != 0) {
        strcpy(sSessionName, pszSessionName);
    }

    sPlayerName[0] = 0;
    if (pszPlayerName != 0) {
        if (strlen(pszPlayerName) < 0x80) {
            strcpy(sPlayerName, pszPlayerName);
        } else {
            char cSaved = pszPlayerName[0x80];
            pszPlayerName[0x80] = 0;
            strcpy(sPlayerName, pszPlayerName);
            pszPlayerName[0x80] = cSaved;
        }
    }

    sPassword[0] = 0;
    if (pszPassword != 0) {
        if (strlen(pszPassword) < 0x80) {
            strcpy(sPassword, pszPassword);
        } else {
            char cSaved = pszPassword[0x80];
            pszPassword[0x80] = 0;
            strcpy(sPassword, pszPassword);
            pszPassword[0x80] = cSaved;
        }
    }

    DPNamePartial dpname;
    char bSuccess;
    bool bJoined;

    if (bIsHost) {
        if (pDirectPlay4 == 0 && (char)DPlay_InitConnection(0, 0, 0) == 0) {
            DPlay_TeardownConnection();
            bSuccess = 0;
            goto done;
        }
        if ((char)DPlay_HostNewSession() == 0) {
            if (bJoinAttempted != 0 && hrLastResult == 0x8877015e) {
                bSuccess = 0;
                goto done;
            }
        } else {
            if (pDirectPlay4 == 0) {
                bJoined = false;
            } else {
                dpname.dwSize = sizeof(dpname);
                dpname.dwFlags = 0;
                dpname.lpszShortNameA = "";
                dpname.lpszLongNameA = sPlayerName;
                hrLastResult = ((IDirectPlay4VtblProbe *)pDirectPlay4)->CreatePlayer(
                    &dpidLocalPlayer, &dpname, 0, 0, 0, 0);
                if (hrLastResult == 0) {
                    bJoined = true;
                } else {
                    char szErrBuf[200];
                    char szMsgBuf[300];
                    g_pNetManager->DPlay_FormatHresultString(szErrBuf, hrLastResult);
                    wsprintfA(szMsgBuf, "Failed to join member %s to session\r\rDirect Play error code %s",
                              sPlayerName, szErrBuf);
                    DPlay_ReportNetworkError(0, szMsgBuf);
                    bJoined = false;
                }
            }
            if (!bJoined) {
                DPlay_TeardownConnection();
                bSuccess = 0;
                goto done;
            }
            bSuccess = 1;
            goto done;
        }
    } else {
        if (pDirectPlay4 == 0 && (char)DPlay_InitConnection(0, 0, 0) == 0) {
            DPlay_TeardownConnection();
            bSuccess = 0;
            goto done;
        }
        if ((char)DPlay_JoinExistingSession() == 0) {
            DPlay_TeardownConnection();
            bSuccess = 0;
            goto done;
        }
        if (pDirectPlay4 == 0) {
            bJoined = false;
        } else {
            dpname.dwSize = sizeof(dpname);
            dpname.dwFlags = 0;
            dpname.lpszShortNameA = "";
            dpname.lpszLongNameA = sPlayerName;
            hrLastResult = ((IDirectPlay4VtblProbe *)pDirectPlay4)->CreatePlayer(
                &dpidLocalPlayer, &dpname, 0, 0, 0, 0);
            if (hrLastResult == 0) {
                bJoined = true;
            } else {
                char szErrBuf[200];
                char szMsgBuf[300];
                g_pNetManager->DPlay_FormatHresultString(szErrBuf, hrLastResult);
                wsprintfA(szMsgBuf, "Failed to join member %s to session\r\rDirect Play error code %s",
                          sPlayerName, szErrBuf);
                DPlay_ReportNetworkError(0, szMsgBuf);
                bJoined = false;
            }
        }
        if (bJoined) {
            bSuccess = 1;
            goto done;
        }
    }
    DPlay_TeardownConnection();
    bSuccess = 0;
done:
    if (bSuccess != 0 && pDirectPlay4 != 0) {
        memset(&sessionDescUpdate, 0, sizeof(sessionDescUpdate));
        sessionDescUpdate.dwSize = sizeof(sessionDescUpdate);
        ((IDirectPlay4VtblProbe *)pDirectPlay4)->SetSessionDesc(&sessionDescUpdate, 0);
    }
    return bSuccess;
}

// FUNCTION: LOCO 0x45ee60 (?ProbeComPort@GNetManager@@QAE_NPBD@Z out-of-line copy)
// GNetManager::ProbeComPort's out-of-class `inline` definition, placed HERE deliberately:
// GameNetThreadState's ctor (0x438bc0, above) makes its COM-port probe calls BEFORE this point
// in the TU, so those sites compile to a real `call` of the out-of-line COMDAT copy exactly as
// the original does (0x438c67 -> 0x45ee60), while DPlay_ProbeAvailableProviders (below) sees the
// body and expands it four times. Declared-only in src/GNetManager.h for the same reason.
//
// Two source facts are load-bearing for DPlay_ProbeAvailableProviders' byte match:
// (a) `char szPortName[5] = "COMn"` is an ARRAY INITIALIZER, not `strcpy(szPortName, "COMn")` --
// VC5's /Oi strcpy intrinsic is the generic runtime `repne scasb` + `rep movsd` shape even for a
// constant literal, whereas an initializer copies the 5-byte template out of .data as one dword +
// one byte, which is exactly what the original does (933 -> 906 B and 327 -> 283 instructions
// from this one change). The 'n' is a placeholder overwritten by the digit; the template lives
// at 0x481214 and the four one-character digit literals "1".."4" at 0x481204/08/0c/10.
// (b) the result must go through an explicit `bPortExists` local assigned BEFORE the CloseHandle
// call, not `return true` after it -- the original keeps the value live across that call
// (`mov byte ptr [esp+0x17],1` ... `mov al,[esp+0x13]`), which a trailing `return true` compiles
// to a post-call `mov al,1` instead (DIFF 565 -> 39).
inline bool GNetManager::ProbeComPort(const char *pszPortDigit) {
    char szPortName[5] = "COMn";
    szPortName[3] = *pszPortDigit;
    bool bPortExists;
    HANDLE hPort = CreateFileA(szPortName, GENERIC_READ | GENERIC_WRITE, 0, 0,
                               OPEN_EXISTING, 0, 0);
    if (hPort == INVALID_HANDLE_VALUE) {
        bPortExists = false;
    } else {
        bPortExists = true;
        CloseHandle(hPort);
    }
    return bPortExists;
}

// FUNCTION: LOCO 0x45eab0
// Builds GNetManager::pProviderList -- the list of DirectPlay service providers this machine
// actually has -- by physically probing for each one, then returns its head. Runs its expensive
// probe at most once (a non-null head returns immediately) and refuses to run at all once a real
// connection is live, since every probe creates and immediately releases a throwaway
// IDirectPlay4.
//
// The four provider types were pinned by decoding the raw GUID bytes passed to each
// DirectPlayCreate (see GNetManager.h's g_guidDPSP* set): 1=Modem (delegated to
// DPlay_ProbeModem, no GUID probe of its own), 4=IPX, 2=TCP/IP, 3=Serial. The IPX/TCP/IP
// probes are pure "can DirectPlay even instantiate this provider?" tests; the Serial probe is
// gated behind a real COM1..COM4 CreateFileA scan first, because DirectPlayCreate on the serial
// provider succeeds on machines with no serial hardware at all. This is what resolves the
// long-open "is NetSettingsMaybe's 1-4 protocol domain IPX or TCP/IP?" question --
// rememberedProtocolPrimary/SecondaryMaybe's 4-valued domain is exactly this enum.
//
// Each arm's `if (hrLastResult != 0) { <release temp> } else { <release both, add node> }` is
// written failure-first so the FAILURE arm lands on the fall-through, matching the original's
// own `je <success>` layout (branch-order lever, see CLAUDE.md). The Serial arm's failure case
// is an early `return` rather than an `else`, matching the original's own separate epilogue.
// Both are load-bearing: flipping the IPX arm to success-first costs 39 -> 86 byte_diff.
//
// EFFECTIVE MATCH (asmscore byte_diff 39 at the exact true len 933, insns 286/286, align 12 --
// EVERY instruction pairs up and the compiled length is byte-exact; the only `-`/`+` rows in the
// whole dump are the position of one `push edi`). The entire residual is a single 2-way callee-
// saved register coin flip: the original allocates `this`->edi and the CSE'd `&pTempDPlayIface`
// ->esi, this compile allocates them the other way round (`&pDirectPlay4`->ebp and the hoisted
// CreateFileA import thunk->ebx agree in both). Six probes, none moved it off 39: consolidating
// the four node locals into one reused `pNode`; hoisting `GUID guidSp` to function scope;
// swapping ProbeComPort's own `bPortExists`/`hPort` declaration order (the documented
// swap-two-sibling-locals lever -- there is no same-type sibling pair in THIS function to swap,
// which is why that lever has nothing to grip); IPX-arm success-first polarity (worse, 86);
// spelling the four probe calls `g_pNetManager->ProbeComPort(...)` as the this-ignoring-thiscall
// family does (byte-identical -- the dead global load is eliminated, so that call spelling is
// unobservable here either way). Two source facts that DID matter are baked in above.
DPlayProviderNode *GNetManager::DPlay_ProbeAvailableProviders() {
    if (pProviderList != 0) {
        return pProviderList;
    }
    if (pDirectPlay4 != 0) {
        return 0;
    }

    if (DPlay_ProbeModem()) {
        DPlayProviderNode *pModemNode = new DPlayProviderNode;
        pModemNode->nProviderType = 1;
        pModemNode->pNext = pProviderList;
        pProviderList = pModemNode;
    }

    GUID guidSp = g_guidDPSPIpx;
    hrLastResult = DirectPlayCreate(&guidSp, &pTempDPlayIface, 0);
    if (hrLastResult == 0) {
        hrLastResult = ((IDirectPlay4VtblProbe *)pTempDPlayIface)
                                 ->QueryInterface(&g_iidDirectPlay4, &pDirectPlay4);
        if (hrLastResult != 0) {
            ((IDirectPlay4VtblProbe *)pTempDPlayIface)->Release();
            pTempDPlayIface = 0;
        } else {
            ((IDirectPlay4VtblProbe *)pTempDPlayIface)->Release();
            pTempDPlayIface = 0;
            ((IDirectPlay4VtblProbe *)pDirectPlay4)->Release();
            pDirectPlay4 = 0;
            DPlayProviderNode *pIpxNode = new DPlayProviderNode;
            pIpxNode->nProviderType = 4;
            pIpxNode->pNext = pProviderList;
            pProviderList = pIpxNode;
        }
    }

    guidSp = g_guidDPSPTcpIp;
    hrLastResult = DirectPlayCreate(&guidSp, &pTempDPlayIface, 0);
    if (hrLastResult == 0) {
        hrLastResult = ((IDirectPlay4VtblProbe *)pTempDPlayIface)
                                 ->QueryInterface(&g_iidDirectPlay4, &pDirectPlay4);
        if (hrLastResult != 0) {
            ((IDirectPlay4VtblProbe *)pTempDPlayIface)->Release();
            pTempDPlayIface = 0;
        } else {
            ((IDirectPlay4VtblProbe *)pTempDPlayIface)->Release();
            pTempDPlayIface = 0;
            ((IDirectPlay4VtblProbe *)pDirectPlay4)->Release();
            pDirectPlay4 = 0;
            DPlayProviderNode *pTcpNode = new DPlayProviderNode;
            pTcpNode->nProviderType = 2;
            pTcpNode->pNext = pProviderList;
            pProviderList = pTcpNode;
        }
    }

    if (ProbeComPort("1") || ProbeComPort("2") || ProbeComPort("3") || ProbeComPort("4")) {
        guidSp = g_guidDPSPSerial;
        hrLastResult = DirectPlayCreate(&guidSp, &pTempDPlayIface, 0);
        if (hrLastResult == 0) {
            hrLastResult = ((IDirectPlay4VtblProbe *)pTempDPlayIface)
                                     ->QueryInterface(&g_iidDirectPlay4, &pDirectPlay4);
            if (hrLastResult != 0) {
                ((IDirectPlay4VtblProbe *)pTempDPlayIface)->Release();
                pTempDPlayIface = 0;
                return pProviderList;
            }
            ((IDirectPlay4VtblProbe *)pTempDPlayIface)->Release();
            pTempDPlayIface = 0;
            ((IDirectPlay4VtblProbe *)pDirectPlay4)->Release();
            pDirectPlay4 = 0;
            DPlayProviderNode *pSerialNode = new DPlayProviderNode;
            pSerialNode->nProviderType = 3;
            pSerialNode->pNext = pProviderList;
            pProviderList = pSerialNode;
        }
    }
    return pProviderList;
}

// FUNCTION: LOCO 0x45eec0
// The Modem arm of DPlay_ProbeAvailableProviders' probe, and the one arm that needs more than
// "can DirectPlay instantiate this provider?": a machine can have the modem service provider
// installed with no modem attached, so after the usual DirectPlayCreate + QueryInterface +
// release-the-temp bring-up it pulls the provider's connection-caps blob
// (IDirectPlay4::GetConnectionCaps into a GlobalAlloc/GlobalLock'd buffer sized by a first
// zero-length call) and runs it through pDPlaySecondary->EnumAddress with the standalone
// DPAID-phone-number callback at 0x45fbd0. That callback's only job is to copy the phone-number
// address element into this->sPhoneNumber -- so "is there a real modem?" is answered by
// `sPhoneNumber[0] != 0` after the enumeration, which is exactly what this returns.
// sPhoneNumber[0] is cleared immediately before the enumeration so a stale number from an
// earlier probe cannot answer for a modem that has since gone away.
//
// Unlike DPlay_InitConnection's own copy of the same caps dance, every failure here is SILENT
// (no DPlay_FormatHresultString/DPlay_ReportNetworkError) -- this is a background capability
// probe, not a user-initiated connect. Six separate `return false` exits, each with its own
// physical epilogue in the original; the caps buffer's GlobalHandle/GlobalUnlock/GlobalFree
// teardown is likewise written out three times rather than shared.
// sic: dwCapsSize is deliberately NOT zero-initialized (the original has no such store) -- it is
// an out-parameter of the sizing call, and this function trusts it. (DPlay_InitConnection's own
// copy of the dance DOES zero it.)
// EXACT. The final `return sPhoneNumber[0] != 0 ? true : false;` must be spelled as a TERNARY:
// both `return sPhoneNumber[0] != 0;` and the implicit `return sPhoneNumber[0];` make VC5 emit an
// extra `xor eax,eax` full-width bool normalization ahead of the `setne` (and push the loaded byte
// out of al into cl), where the original just does `test al,al / setne al` on the value it already
// has in eax -- DIFF 19 vs EXACT, and 451 vs 449 compiled bytes. Same bool-materialization
// spelling lever as WidgetPickerObj0x477cc8::OnKeyDownMaybe's VK_BACK arm (v433).
bool GNetManager::DPlay_ProbeModem() {
    if (pDirectPlay4 != 0) {
        return false;
    }

    GUID guidSp = g_guidDPSPModem;
    hrLastResult = DirectPlayCreate(&guidSp, &pTempDPlayIface, 0);
    if (hrLastResult != 0) {
        return false;
    }
    hrLastResult = ((IDirectPlay4VtblProbe *)pTempDPlayIface)
                             ->QueryInterface(&g_iidDirectPlay4, &pDirectPlay4);
    if (hrLastResult != 0) {
        ((IDirectPlay4VtblProbe *)pTempDPlayIface)->Release();
        pTempDPlayIface = 0;
        return false;
    }
    ((IDirectPlay4VtblProbe *)pTempDPlayIface)->Release();
    pTempDPlayIface = 0;

    unsigned int dwCapsSize;
    ((IDirectPlay4VtblProbe *)pDirectPlay4)->GetConnectionCapsMaybe(0, 0, &dwCapsSize);
    HGLOBAL hCaps = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, dwCapsSize);
    void *pCaps = GlobalLock(hCaps);
    hrLastResult =
        ((IDirectPlay4VtblProbe *)pDirectPlay4)->GetConnectionCapsMaybe(0, pCaps, &dwCapsSize);
    if (hrLastResult != 0) {
        if (pCaps != 0) {
            GlobalUnlock(GlobalHandle(pCaps));
            GlobalFree(GlobalHandle(pCaps));
        }
        ((IDirectPlay4VtblProbe *)pDirectPlay4)->Release();
        pDirectPlay4 = 0;
        return false;
    }

    sPhoneNumber[0] = 0;
    hrLastResult = ((IDPlaySecondaryVtblProbe *)pDPlaySecondary)
                             ->EnumAddress((void *)&DPlay_EnumAddressCallback, pCaps,
                                           dwCapsSize, 0);
    if (hrLastResult != 0) {
        if (pCaps != 0) {
            GlobalUnlock(GlobalHandle(pCaps));
            GlobalFree(GlobalHandle(pCaps));
        }
        ((IDirectPlay4VtblProbe *)pDirectPlay4)->Release();
        pDirectPlay4 = 0;
        return false;
    }

    if (pCaps != 0) {
        GlobalUnlock(GlobalHandle(pCaps));
        GlobalFree(GlobalHandle(pCaps));
    }
    ((IDirectPlay4VtblProbe *)pDirectPlay4)->Release();
    pDirectPlay4 = 0;
    return sPhoneNumber[0] != 0 ? true : false;
}

// FUNCTION: LOCO 0x45fbd0
// The DPENUMADDRESSCALLBACK DPlay_ProbeModem feeds the modem provider's connection-caps blob
// to (pDPlaySecondary->EnumAddress, vtbl+0x14). It ignores every address element except the
// DPAID phone-number one, whose string it strcpy's into g_pNetManager->sPhoneNumber -- and
// that copy landing is exactly how DPlay_ProbeModem answers "is there a real modem here?".
// Returning 0 STOPS the enumeration (the element was found); 1 continues.
// Reaches the manager through the g_pNetManager GLOBAL rather than its own lpContext parameter,
// which it ignores entirely along with dwDataSize -- so a probe on a second manager instance
// would write the wrong object. Also `sic:` an unbounded strcpy into the fixed sPhoneNumber
// field, with only a "not empty" length check.
// The GUID compare is a 16-byte `repz cmpsb` (memcmp intrinsic), not a field-by-field test.
int __stdcall DPlay_EnumAddressCallback(const GUID *pGuidType, unsigned int dwDataSize,
                                        const void *lpData, void *lpContext) {
    if (memcmp(pGuidType, &g_guidDPAIDPhoneMaybe, sizeof(GUID)) == 0) {
        if (lstrlenA((LPCSTR)lpData) != 0) {
            strcpy(g_pNetManager->sPhoneNumber, (const char *)lpData);
            return 0;
        }
    }
    return 1;
}

// FUNCTION: LOCO 0x45fc30
// Full connection teardown, called from every DPlay_JoinOrHostSession failure exit and from
// GameNet_ProcessLocalCommand's connection-reset opcode. Four stages: (1) release the
// GlobalLock'd guidInstance buffer of the session we were joining, unless bSkipGlobalFree says
// someone else still owns it; (2) if a live IDirectPlay4 exists, CancelMessage(0,0) (drop every
// queued message) then Close() then Release(); (3) drain BOTH found-session lists; (4) reset the
// session identity/protocol scratch to its empty state -- note it clears only the first BYTE of
// each string field, the usual "empty C string" idiom, not the whole buffer.
// The two list drains are deliberately asymmetric: pListHead1's nodes own HGLOBAL session-desc
// memory and pListHead2Maybe's do not, matching the two lists' different producers.
void GNetManager::DPlay_TeardownConnection() {
    if (pGlobalLockedBuf != 0 && !bSkipGlobalFree) {
        HGLOBAL hMem = GlobalHandle(pGlobalLockedBuf);
        GlobalUnlock(hMem);
        hMem = GlobalHandle(pGlobalLockedBuf);
        GlobalFree(hMem);
    }
    bSkipGlobalFree = false;
    pGlobalLockedBuf = 0;

    if (pDirectPlay4 != 0) {
        ((IDirectPlay4VtblProbe *)pDirectPlay4)->CancelMessage(0, 0);
        ((IDirectPlay4VtblProbe *)pDirectPlay4)->Close();
        ((IDirectPlay4VtblProbe *)pDirectPlay4)->Release();
        pDirectPlay4 = 0;
    }

    while (pListHead1 != 0) {
        FoundSessionNode *pNext = pListHead1->pNext;
        if (pListHead1->pszName != 0) {
            operator delete(pListHead1->pszName);
        }
        if (pListHead1->pSessionDescMem != 0) {
            HGLOBAL hMem = GlobalHandle(pListHead1->pSessionDescMem);
            GlobalUnlock(hMem);
            hMem = GlobalHandle(pListHead1->pSessionDescMem);
            GlobalFree(hMem);
            pListHead1->pSessionDescMem = 0;
        }
        operator delete(pListHead1);
        pListHead1 = pNext;
    }

    while (pListHead2Maybe != 0) {
        FoundSessionNode *pNext = pListHead2Maybe->pNext;
        if (pListHead2Maybe->pszName != 0) {
            operator delete(pListHead2Maybe->pszName);
        }
        operator delete(pListHead2Maybe);
        pListHead2Maybe = pNext;
    }

    sSessionName[0] = 0;
    sPlayerName[0] = 0;
    nProtocol = 0;
    sConnectParam[0] = 0;
    nMaxPlayers = 0;
    nUnk0x934Maybe = 0;
    bSessionJoined = 0;
}

// FUNCTION: LOCO 0x460ea0
// The shared DirectPlay error sink: composes "<resource string nPrefixResId>\r\r<pszMessage>"
// (either half optional), records it in this->sLastErrorText, and then EITHER hands it to the
// pfnErrorTextHookMaybe callback (if one is installed) OR shows it in a MB_TOPMOST message box
// titled with resource string 0x7d06 -- or does neither, silently, when no hook is installed and
// bShowErrorMessageBoxMaybe is clear. Returns MessageBoxA's result in the box case, else 0.
// Both scratch buffers are `char[0x200] = ""` ARRAY INITIALIZERS: VC5 emits one byte store from
// the pooled empty literal plus a `rep stosd` zero fill of the remaining 511 bytes, and hoists
// the literal's byte into dl to share it between the two. Declaration order matters -- the
// CAPTION buffer is declared first (it lands at the higher address and is initialized first).
int GNetManager::DPlay_ReportNetworkError(unsigned int nPrefixResId, char *pszMessage) {
    char szCaption[0x200] = "";
    char szText[0x200] = "";

    if (nPrefixResId != 0) {
        LoadStringA(hInstance, nPrefixResId, szText, sizeof(szText));
    }
    if (pszMessage != 0) {
        if (nPrefixResId != 0) {
            strcat(szText, "\r\r");
        }
        strcat(szText, pszMessage);
    }
    LoadStringA(hInstance, 0x7d06, szCaption, sizeof(szCaption));
    strcpy(sLastErrorText, szText);

    if (pfnErrorTextHookMaybe != 0) {
        pfnErrorTextHookMaybe(szText);
        return 0;
    }
    if (bShowErrorMessageBoxMaybe) {
        return MessageBoxA(hWndParent, szText, szCaption, MB_TOPMOST);
    }
    return 0;
}

// FUNCTION: LOCO 0x461020
// The "Select Connection" dialog's DLGPROC (resource 0x7d0a), shown by DPlay_InitConnection when
// it is called with nProtocol == 0. Four mutually-exclusive radio buttons whose control ids map
// to the provider tag stored in g_pNetManager->nProtocol -- and the mapping is NOT in id order:
//   0x7d32 -> 4 (IPX, and the one pre-checked on WM_INITDIALOG)
//   0x7d33 -> 2 (TCP/IP)
//   0x7d34 -> 1 (Modem)
//   0x7d35 -> 3 (Serial)
// The last arm is deliberately not symmetric with the first three: a checked 0x7d35 sets
// nProtocol, but the EndDialog(hDlg, 1)/return 1 that follows runs whether or not ANY button was
// checked -- so dismissing the dialog with nothing selected still reports success and leaves
// nProtocol at whatever it already held.
// Reaches the manager through the g_pNetManager global rather than the dialog's lParam context.
// sic: the WM_INITDIALOG arm falls through to `return 0` (FALSE) rather than returning TRUE, so
// Windows keeps its own default input focus instead of honouring the dialog's first tab stop.
// BOTH dispatches must be `switch`, not `if`/`else if` -- that is what took this from DIFF(294)
// to EXACT. The tell is in the original's compare CHAINS: `sub eax,0x110 / je / dec eax / jne`
// for uMsg and `and eax,0xffff / dec eax / je / dec eax / jne` for the command id. An if/else-if
// ladder re-compares from scratch around each arm (`cmp eax,0x110` … body … `cmp eax,0x111`, and
// a 16-bit `cmp ax,1` with no zero-extension) and lays the first arm's body INLINE; the switch
// emits the subtract-and-decrement chain up front and moves every case body out of line, which is
// also why the original's WM_INITDIALOG block sits physically LAST. Two cases is well under any
// jump-table threshold, so the shape is pure dispatch-lowering, not table-vs-chain.
int __stdcall DPlay_SelectConnectionDlgProc(HWND hDlg, unsigned int uMsg, unsigned int wParam,
                                            unsigned int lParam) {
    switch (uMsg) {
    case WM_INITDIALOG:
        SendDlgItemMessageA(hDlg, 0x7d32, BM_SETCHECK, 1, 0);
        SendDlgItemMessageA(hDlg, 0x7d33, BM_SETCHECK, 0, 0);
        SendDlgItemMessageA(hDlg, 0x7d34, BM_SETCHECK, 0, 0);
        SendDlgItemMessageA(hDlg, 0x7d35, BM_SETCHECK, 0, 0);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            if (SendDlgItemMessageA(hDlg, 0x7d32, BM_GETCHECK, 0, 0) == 1) {
                g_pNetManager->nProtocol = 4;
                EndDialog(hDlg, 1);
                return 1;
            }
            if (SendDlgItemMessageA(hDlg, 0x7d33, BM_GETCHECK, 0, 0) == 1) {
                g_pNetManager->nProtocol = 2;
                EndDialog(hDlg, 1);
                return 1;
            }
            if (SendDlgItemMessageA(hDlg, 0x7d34, BM_GETCHECK, 0, 0) == 1) {
                g_pNetManager->nProtocol = 1;
                EndDialog(hDlg, 1);
                return 1;
            }
            if (SendDlgItemMessageA(hDlg, 0x7d35, BM_GETCHECK, 0, 0) == 1) {
                g_pNetManager->nProtocol = 3;
            }
            EndDialog(hDlg, 1);
            return 1;

        case IDCANCEL:
            EndDialog(hDlg, 0);
            return 1;
        }
        break;
    }
    return 0;
}

// FUNCTION: LOCO 0x45e490
// The whole ctor body is the CreateLobby call -- every field init and the DirectPlayLobby
// bring-up live there, not here.
GNetManager::GNetManager(void *hInstance, HWND hWndParent) {
    CreateLobby(hInstance, hWndParent);
}

// FUNCTION: LOCO 0x45e5a0
// Non-virtual dtor, called directly by `delete` (GameNet_TeardownAndFlushQueues). Tears the
// connection down, releases the lobby object, frees the cached reliable-message payload, and
// then drains all FOUR lists this object owns. Only pListHead1's nodes carry HGLOBAL
// session-desc memory, so only that drain does the GlobalHandle/GlobalUnlock/GlobalFree dance --
// pListHead0Maybe's and pListHead2Maybe's nodes give up just pszName and the node, and
// pProviderList's 8-byte nodes have nothing to free but themselves.
GNetManager::~GNetManager() {
    DPlay_TeardownConnection();

    if (pDPlaySecondary != 0) {
        ((IDPlaySecondaryVtblProbe *)pDPlaySecondary)->Release();
        pDPlaySecondary = 0;
    }
    if (pDataCacheMaybe != 0) {
        operator delete(pDataCacheMaybe);
        pDataCacheMaybe = 0;
        cbDataCacheMaybe = 0;
    }

    while (pListHead0Maybe != 0) {
        FoundSessionNode *pNext = pListHead0Maybe->pNext;
        if (pListHead0Maybe->pszName != 0) {
            operator delete(pListHead0Maybe->pszName);
        }
        operator delete(pListHead0Maybe);
        pListHead0Maybe = pNext;
    }

    while (pListHead1 != 0) {
        FoundSessionNode *pNext = pListHead1->pNext;
        if (pListHead1->pszName != 0) {
            operator delete(pListHead1->pszName);
        }
        if (pListHead1->pSessionDescMem != 0) {
            HGLOBAL hMem = GlobalHandle(pListHead1->pSessionDescMem);
            GlobalUnlock(hMem);
            hMem = GlobalHandle(pListHead1->pSessionDescMem);
            GlobalFree(hMem);
            pListHead1->pSessionDescMem = 0;
        }
        operator delete(pListHead1);
        pListHead1 = pNext;
    }

    while (pListHead2Maybe != 0) {
        FoundSessionNode *pNext = pListHead2Maybe->pNext;
        if (pListHead2Maybe->pszName != 0) {
            operator delete(pListHead2Maybe->pszName);
        }
        operator delete(pListHead2Maybe);
        pListHead2Maybe = pNext;
    }

    while (pProviderList != 0) {
        DPlayProviderNode *pNext = pProviderList->pNext;
        operator delete(pProviderList);
        pProviderList = pNext;
    }
}

// FUNCTION: LOCO 0x45e4b0
// The real body of the ctor (0x45e490 is a one-line forwarder). Two halves: zero/default every
// scalar field, then bring up the DirectPlay LOBBY object -- DirectPlayLobbyCreateA followed by
// QueryInterface(IID_IDirectPlayLobby3A) into pDPlaySecondary and an immediate Release of the
// transient object, the same create-QI-release shape as the provider probes. Long labelled
// "DirectSound init" by an early doc pass; that was wrong (DirectSound comes up in
// DSound_InitDeviceAndChannelPool, 0x412c50).
// sic: BOTH HRESULTs are stored into hrLastResult and then ignored -- a failed lobby create
// leaves pDPlaySecondary null and is only noticed much later, when DPlay_InitConnection
// dereferences it. Note also that sConnectParam is NOT cleared here (only
// DPlay_TeardownConnection does that), so it starts life as uninitialized heap.
// Non-obvious defaults: bAllowHostMigration and bShowErrorMessageBoxMaybe start TRUE (so errors
// DO pop message boxes out of the box) and nSendThrottleQueueDepth starts at 10.
void GNetManager::CreateLobby(void *hInstance, HWND hWndParent) {
    bShowErrorMessageBoxMaybe = true;
    bAllowHostMigration = true;
    pfnIdlePumpCallback = 0;
    pfnErrorTextHookMaybe = 0;
    sLastErrorText[0] = 0;
    hrLastResult = 0;
    bSessionJoined = false;
    bIsHost = false;
    bJoinAttempted = false;
    nMaxPlayers = 0;
    bUnk0x3Maybe = 0;
    bUnk0x4Maybe = 0;
    pDirectPlay4 = 0;
    sSessionName[0] = 0;
    pGlobalLockedBuf = 0;
    bSkipGlobalFree = false;
    sPlayerName[0] = 0;
    dpidLocalPlayer = 0;
    nProtocol = 0;
    this->hInstance = (HINSTANCE)hInstance;
    this->hWndParent = hWndParent;
    cbDataCacheMaybe = 0;
    pDataCacheMaybe = 0;
    pListHead0Maybe = 0;
    pListHead1 = 0;
    pListHead2Maybe = 0;
    pProviderList = 0;
    nSendThrottleQueueDepth = 10;
    pTempLobbyIface = 0;
    pDPlaySecondary = 0;

    hrLastResult = DirectPlayLobbyCreateA(0, &pTempLobbyIface, 0, 0, 0);
    hrLastResult = ((IDPlaySecondaryVtblProbe *)pTempLobbyIface)
                             ->QueryInterface(&g_iidDirectPlayLobby3A, &pDPlaySecondary);
    ((IDPlaySecondaryVtblProbe *)pTempLobbyIface)->Release();
    pTempLobbyIface = 0;
}

// FUNCTION: LOCO 0x43a8b0
// Local command opcode 2 ("scan for other sessions", GameNet_ProcessLocalCommand case 2):
// re-enumerate DirectPlay sessions (GNetManager::DPlay_FindSession, only when pDirectPlay4 is
// live) and walk the found-session list building a NEW list of every session whose name does NOT
// strcmp-match the .ini-configured default session name ("Configuration"/"Name", default "LEGO
// International Train Server") -- i.e. filters OUT the well-known default session, keeping only
// "other" custom sessions found on the LAN/net. Each surviving entry gets its own new_alloc'd
// FoundSessionNode node plus a new_alloc'd 0x100-byte name copy; the new list's head is
// posted as the payload of a type-2 local-queue notify. sic: a new node's middle
// pSessionDescMem field (real content on DPlay_FindSession's OWN list) is left uninitialized
// here -- only pNext and pszName are zeroed before pszName is (maybe) filled.
// EFFECTIVE MATCH (asmscore byte_diff 24 at true len 321, insns 110/111 -- structure, all branch
// polarities, and every register role match). Two small intrinsic residuals: (a) the
// strcmp-match ("filter out") case's own `pFound = pFound->pNext` advance is a DUPLICATE physical
// copy in the original (own `mov ebx,[ebx]; jmp <loop top>`) vs. a single TAIL-MERGED copy shared
// with the no-match case's own advance here -- tried an explicit `goto`-labelled duplicate copy,
// no effect (a self-contained block-layout/cross-jump choice, Yoda #15/#18, not source-steerable);
// (b) the IniFile::ReadString call's `this` (g_pIniFile) load lands a few bytes earlier here
// than in the original (which computes the output-buffer address into ecx first, then reuses ecx
// for `this` only after that address is pushed) -- tried an explicit `IniFile *pIni` local,
// no effect. Both are pure scheduling/layout tie-breaks; the `bHasDirectPlayMaybe` bool local +
// its inverted `if (!bHasDirectPlayMaybe)` polarity (matching FALSE as the fall-through arm) DID
// reproduce the full boolean-materialization prologue that DPlay_UiConnectHandler's own attempt at
// the same pDirectPlay4 idiom could not -- worth retrying that polarity lever there.
void GameNetThreadState::DPlay_BuildOtherSessionsList(NetMsgQueueNode *pNode) {
    bool bHasDirectPlayMaybe = g_pNetManager->pDirectPlay4 != 0;
    FoundSessionNode *pFound;
    if (!bHasDirectPlayMaybe)
        pFound = 0;
    else
        pFound = g_pNetManager->DPlay_FindSession(0);

    char szDefaultName[0x100];
    g_pIniFile->ReadString("Configuration", "Name", "LEGO International Train Server",
                                 szDefaultName, sizeof(szDefaultName));

    FoundSessionNode *pHead = 0;
    FoundSessionNode *pTail = 0;
    while (pFound != 0) {
        if (strcmp(pFound->pszName, szDefaultName) == 0) {
            pFound = pFound->pNext;
            continue;
        }
        FoundSessionNode *pNew = (FoundSessionNode *)operator new(sizeof(FoundSessionNode));
        pNew->pNext = 0;
        pNew->pszName = 0;
        if (pTail != 0) {
            pTail->pNext = pNew;
        } else {
            pHead = pNew;
        }
        if (pFound->pszName != 0) {
            pNew->pszName = (char *)operator new(0x100);
            strcpy(pNew->pszName, pFound->pszName);
        }
        pTail = pNew;
        pFound = pFound->pNext;
    }

    NetMsgQueueNode *pOut = new NetMsgQueueNode();
    pOut->type = 2;
    pOut->pPayload = pHead;
    g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pOut);
}

// FUNCTION: LOCO 0x43aa00
// EFFECTIVE MATCH (512 B vs 514, insns 144/144, total 12340, align=10). The instruction
// SEQUENCE is identical operand-shape for operand-shape; the whole residual is one register
// coin-flip. The original allocates ESI for g_pNetManager across the `||` guard and ECX for
// g_pLocalPlayerIdentity in the host branch; cl gives us EDX and EAX for those, a 3-cycle
// permutation (eax->edx->ecx) through the wsprintf block. The 2 missing bytes are NOT missing
// body: `mov eax,[mem32]` is the 5-byte A1 accumulator encoding while `mov ecx,[mem32]` is the
// 6-byte 8B 0D form, and that load happens TWICE in the host path (0x4e, 0x75). Same family as
// the `test al,al` (original) vs `cmp al,bl` (ours) selection at 0x30 -- both 2 bytes, pure
// tie-break. Probed and REFUTED, one compile each: local for pApplSetupWnd; hoisting
// `bTransportLive`'s declaration to the top; `unsigned char`/`char`/`int` for it (int loses the
// setne entirely); and splitting the `||` guard back into two ifs (that one is strongly
// NEGATIVE now -- 132499 -- so the `||` is load-bearing, not cosmetic).
// Local command opcode 3 ("join or host the session", GameNet_ProcessLocalCommand case 3).
// Three ways to arrive at a session name: HOSTING builds "<remembered secondary node
// text>.<local player name>" (the format literal is "%s.%s" -- a DOT, read out of the image at
// 0x47eae8, NOT the space Ghidra's own `s__s__s_0047eae8` label implies, per CLAUDE.md's
// string-literal rule); JOINING with connectionMode != 1 uses the primary remembered node text
// verbatim; JOINING with connectionMode == 1 (a real "network game" session) reads
// [Configuration]Name from the .ini (default "LEGO International Train Server") and retries the
// join up to twice more, sleeping 1s then 2s between attempts. On success re-posts local command
// 3 to the queue (tagged with our own DPID) and returns true; on any failure -- including the
// two early bail-outs -- posts local command 5 (teardown) and returns false, EXCEPT that the two
// early bail-outs (dispatch already busy, or no manager at all) return false without posting
// anything.
bool GameNetThreadState::AttemptJoinOrHostSession() {
    char szSessionName[256];
    char szIniSessionName[1024];

    if (bShutdownRequestedMaybe != 0 || g_pNetManager == 0)
        return false;

    // Both bail-outs below post the SAME local command 5 (teardown) and are written out
    // separately here on purpose: the original's compiler tail-merged them into one block
    // entered by a `push 0x1c; jmp`, which is only reproducible from two source copies -- a
    // single shared block reached by `goto` compiles 3 bytes shorter (see the autopsy below).
    bool bTransportLive = g_pNetManager->pDirectPlay4 != 0;
    if (!bTransportLive) {
        NetMsgQueueNode *pNoTransport = new NetMsgQueueNode();
        pNoTransport->type = 5;
        pNoTransport->pPayload = 0;
        g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNoTransport);
        return false;
    }

    dpidCurrentPlayer = 0;
    if (g_pNetManager->bIsHost != 0) {
        wsprintfA(szSessionName, "%s.%s",
                  g_pSplashWnd->pApplSetupWnd->pSelectedNodeTextSecondaryMaybe,
                  g_pLocalPlayerIdentity->name);
        g_pNetManager->DPlay_JoinOrHostSession(g_pLocalPlayerIdentity->name, szSessionName, 0);
    } else if (g_pDPlaySessionMgr->connectionMode != 1) {
        g_pNetManager->DPlay_JoinOrHostSession(g_pLocalPlayerIdentity->name,
                                               g_pSplashWnd->pApplSetupWnd->pSelectedNodeTextMaybe,
                                               0);
    } else {
        g_pIniFile->ReadString("Configuration", "Name", "LEGO International Train Server",
                               szIniSessionName, sizeof(szIniSessionName));
        g_pNetManager->DPlay_JoinOrHostSession(g_pLocalPlayerIdentity->name, szIniSessionName, 0);
        if (g_pNetManager->bSessionJoined == 0) {
            Sleep(1000);
            g_pNetManager->DPlay_JoinOrHostSession(g_pLocalPlayerIdentity->name, szIniSessionName,
                                                   0);
        }
        if (g_pNetManager->bSessionJoined == 0) {
            Sleep(2000);
            g_pNetManager->DPlay_JoinOrHostSession(g_pLocalPlayerIdentity->name, szIniSessionName,
                                                   0);
        }
    }

    if (g_pNetManager->bSessionJoined == 0) {
        NetMsgQueueNode *pFailed = new NetMsgQueueNode();
        pFailed->type = 5;
        pFailed->pPayload = 0;
        g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pFailed);
        return false;
    }

    NetMsgQueueNode *pJoined = new NetMsgQueueNode();
    pJoined->type = 3;
    pJoined->pPayload = 0;
    pJoined->destPlayerId = g_pNetManager->dpidLocalPlayer;
    g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pJoined);
    return true;
}

// FUNCTION: LOCO 0x43c860
// Inbound msg 0x3eb ("connect UI request"), reached only when pWire->szData is non-empty.
// bConnect&1==1: DirectPlay connect/(re)join path -- tear down any existing connection, briefly
// yield, reinit the manager, read the configured Protocol id + session name from the .ini, and
// attempt to join/host (pWire->szData doubles as the requested provider-address string). On
// failure, retry once more aggressively (InitBigFields's 2nd arg flips 0->1, a longer Sleep)
// before giving up. bConnect&1==0: treat pWire->szData as a URL suffix, wsprintfA it into
// "http:\\%s" (sic: two literal backslashes, not "http://" -- a real original typo, reproduced
// faithfully), validate every character is alnum or one of ",-.:;@\\_" (plus an always-false dead
// check against the multichar literal '//' == 0x2f2f -- sic, a single scanned char can never equal
// a 2-byte constant), and ShellExecuteA "open" it if the whole string passes. Either way, falls
// through to a shared teardown/reset tail: tear down the connection, reset the current-player id,
// post a type-0x1c notify, build a fresh local-player CarNetState card (default description
// string id 0xdf, "no owner" byte 0xff, own player name + "LEGO LOCO" as the two card names), save
// it to category 1, release it (no null check -- sic, a latent crash if the alloc failed), then
// drain the active-train list as type-0xf notifies.
// EFFECTIVE MATCH (v269): the URL char-class validation branch tree byte-matches exactly once
// each if/else pair's polarity is picked so the SMALLER/flatter sub-tree is the fall-through
// side and the sub-tree that recurses into another comparison is the out-of-line jump target
// (both the outer c<=0x3b/c>0x3b split and the inner c<=0x5c/c>0x5c split needed this). The
// remaining residual is a consistent 3-way register rotation (edx/ecx/eax) spanning nearly the
// whole function, rooted at the g_pNetManager->pDirectPlay4 boolean materialization
// (xor/test/setne/test) -- matches the documented intrinsic prologue-materialization /
// symmetric-register-swap class (Yoda #29/#30, GameNetMsgQueue::EnqueueOrFreeNode 0x4393d0)
// and was not further reproducible via source form; not re-probed beyond the triage budget.
void GameNetThreadState::DPlay_UiConnectHandler(UiConnectWireMsg *pWire) {
    if (strlen(pWire->szData) != 0) {
        if ((pWire->bConnect & 1) == 1) {
            g_pNetManager->DPlay_TeardownConnection();
            Sleep(10);
            g_pNetManager->InitBigFields(0, 0, 0, 0);
            int nProtocol = g_pIniFile->ReadInt("Configuration", "Protocol", 2);
            g_pNetManager->DPlay_InitConnection(nProtocol, pWire->szData, 0);
            bool bHasDirectPlayMaybe = g_pNetManager->pDirectPlay4 != 0;
            if (bHasDirectPlayMaybe) {
                char szSessionName[1204];
                g_pIniFile->ReadString("Configuration", "Name", "LEGO International Train Server",
                                             szSessionName, sizeof(szSessionName));
                g_pNetManager->DPlay_JoinOrHostSession(g_pLocalPlayerIdentity->name, szSessionName, 0);
                if (!g_pNetManager->bSessionJoined) {
                    g_pNetManager->DPlay_TeardownConnection();
                    Sleep(1000);
                    g_pNetManager->InitBigFields(0, 1, 0, 0);
                    g_pNetManager->DPlay_InitConnection(nProtocol, pWire->szData, 0);
                    g_pNetManager->DPlay_JoinOrHostSession(g_pLocalPlayerIdentity->name, szSessionName, 0);
                }
                if (g_pNetManager->bSessionJoined)
                    return;
            }
        } else {
            this->bSkipConnectMaybe = true;
            char szUrl[512];
            wsprintfA(szUrl, "http:\\\\%s", pWire->szData);
            char bAborted = 0;
            char *p = szUrl;
            while (*p != '\0') {
                if (!IsCharAlphaNumericA(*p)) {
                    int c = *p;
                    if (c <= 0x3b) {
                        if (c < 0x3a) {
                            if (c < 0x2c || c > 0x2e)
                                bAborted = 1;
                        }
                    } else {
                        if (c <= 0x5c) {
                            if (c != 0x5c && c != 0x40)
                                bAborted = 1;
                        } else {
                            if (c != 0x5f && c != 0x2f2f)  // sic: '//' (0x2f2f) never matches a scanned char
                                bAborted = 1;
                        }
                    }
                    if (bAborted)
                        goto teardown;
                }
                p++;
            }
            if (!bAborted)
                ShellExecuteA(0, "open", szUrl, 0, 0, 0);
        }
    }

teardown:
    g_pNetManager->DPlay_TeardownConnection();
    CarNetState *pCard = 0;
    this->dpidCurrentPlayer = 0;
    NetMsgQueueNode *pNode = new NetMsgQueueNode();
    pNode->type = 0x1c;
    pNode->pNext = 0;
    pNode->pPayload = 0;
    g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode);

    pCard = new CarNetState();
    g_UIResources.LoadLocaleString(0xdf, pCard->szDescription, sizeof(pCard->szDescription));
    pCard->byIdentityColorG = 0xff;
    strcpy(pCard->nameA, g_pLocalPlayerIdentity->name);
    strcpy(pCard->nameB, "LEGO LOCO");
    g_pPostBagCache->PostBag_SaveCardToCategory(pCard, 1, 0);
    if (pCard != 0)
        delete pCard;

    while (pTrainListActive != 0) {
        NetMsgQueueNode *pNodeB = new NetMsgQueueNode();
        pNodeB->type = 0xf;
        pNodeB->pPayload = pTrainListActive;
        pTrainListActive->bHasDetailFlagMaybe = 0;
        pTrainListActive = (PeerTrainNodePartial *)pTrainListActive->pNext;
        ((PeerTrainNodePartial *)pNodeB->pPayload)->pNext = 0;
        g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNodeB);
    }
}

// FUNCTION: LOCO 0x43ce10
// Inbound msg 0x3ec ("peer's player-roster/train-state snapshot", send side:
// GameNet_BroadcastPlayerRoster). First mark every currently active train's heading as the
// disconnect sentinel (32000). If the snapshot carries any car records: spawn a fresh
// PeerTrainNode (random car-kind id) named "LEGO LOCO", unpack each wire record via
// CarNetState_CreateFromWireRecord, stamp the local player's own name onto it (sic: used
// before its own null-check -- an alloc failure would crash here), apply it onto the new train's
// next car slot, request any missing appearance resources, then free the temporary
// CarNetState. Post the new train as a type-0xf local-queue notify -- NOT linked onto
// pTrainListActive here, that's the type-0xf handler's own job. Then, unconditionally, pop
// exactly one entry off the head of the active-train list (if any) and post it too as a type-0xf
// notify -- one pending hand-off per call, matching the iterative join/roster-sync design
// (docs/subsystems.md). Finally: broadcast the roster again if the active list is still non-empty
// (fall-through branch in the original -- the polarity matters, see below); else tear down the
// connection unless a file receive is still pending, then re-check the (now UNCHANGED, but
// no-longer-provably-so past the opaque teardown call) empty list once more before a drain loop
// that's unreachable in practice.
//
// EFFECTIVE MATCH (asmscore total 154042, align=150 reg_pen=33 byte_diff=142, insns 198/199 at
// true len 647). The one real structural fix: the tail if/else's fall-through side must be
// `if (list != 0) broadcast(); else {teardown...}` -- NOT the more natural-reading `if (list==0)
// {teardown...} else broadcast()` (semantically identical, opposite fall-through) -- this alone
// dropped the score 200050->154042 and closed every `-`/`+` structural gap in the whole function
// (same branch-fall-through-polarity family as PostBag_ScanCategoryCrdFilesMaybe/AlbumCardWnd
// DrawOrEraseCardSlot, CLAUDE.md). What's left after that fix is a single pervasive residual: `this`
// lands in esi throughout my compile vs edi in the original (plus the matching push-order/spill
// shuffle this drags along) -- the well-documented intrinsic symmetric-register-swap class (Yoda
// #29/#30), confirmed dozens of times elsewhere in this file; not source-steerable. A couple of
// minor loop-internal scheduling diffs (the do-while's first-iteration reload skip; the AllocCarSlot
// arg pushes interleaved differently with the strcpy tail-copy) are the same flavor, not chased
// further per the triage budget.
void GameNetThreadState::GameNet_ReceiveRosterSnapshot(PlayerRosterWireMsg *pWire) {
    for (PeerTrainNodePartial *p = pTrainListActive; p != 0; p = (PeerTrainNodePartial *)p->pNext)
        p->wHeading = 32000;

    if (pWire->nCarCount != 0) {
        PeerTrainNodePartial *pNew =
            new PeerTrainNodePartial(((int)rand() % 3) * 2 + 0x1804, 1, 1, 1);
        ((CarNetObjVtblProbe *)pNew->carSlots[0])->SetNameImpl("LEGO LOCO");
        pNew->wHeading = 0;
        pNew->wLocalHeading = 0;
        pNew->wPosX = 0;
        pNew->wPosY = 0;
        pNew->bStallStepCounter = 0;
        pNew->wCheckpointPosX = 0;
        pNew->wCheckpointPosY = 0;

        CarNetObj **ppCar = (CarNetObj **)&pNew->carSlots[1];
        if (0 < pWire->nCarCount) {
            int i = 0;
            do {
                CarNetState *pState =
                    CarNetState_CreateFromWireRecord(&pWire->records[i]);
                strcpy(pState->nameA, g_pLocalPlayerIdentity->name);  // sic: no null-check yet
                pState->wAttachmentId = 0;
                pNew->PeerTrainNode_AllocCarSlot(0x1871, 4, 1);
                (*ppCar)->CarNetObj_ApplyNetState(pState);
                NetResource_RequestMissingAppearances(pState);
                delete pState;
                i++;
                ppCar++;
            } while (i < pWire->nCarCount);
        }

        NetMsgQueueNode *pOut = new NetMsgQueueNode();
        pOut->type = 0xf;
        pOut->pPayload = pNew;
        pNew->bHasDetailFlagMaybe = 0;
        g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pOut);
    }

    PeerTrainNodePartial *pTrain = pTrainListActive;
    if (pTrain != 0) {
        pTrainListActive = (PeerTrainNodePartial *)pTrain->pNext;
        pTrain->pNext = 0;
        NetMsgQueueNode *pOut = new NetMsgQueueNode();
        pOut->type = 0xf;
        pOut->pPayload = pTrain;
        pTrain->bHasDetailFlagMaybe = 0;
        g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pOut);
    }

    if (pTrainListActive != 0) {
        GameNet_BroadcastPlayerRoster();
    } else {
        if (nPendingFileReceiveCount == 0)
            g_pNetManager->DPlay_TeardownConnection();
        while (pTrainListActive != 0) {
            NetMsgQueueNode *pOut = new NetMsgQueueNode();
            pOut->type = 0xf;
            pOut->pPayload = pTrainListActive;
            pTrainListActive->bHasDetailFlagMaybe = 0;
            pTrainListActive = (PeerTrainNodePartial *)pTrainListActive->pNext;
            ((PeerTrainNodePartial *)pOut->pPayload)->pNext = 0;
            g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pOut);
        }
    }
}

// FUNCTION: LOCO 0x43ccc0
// Outbound msg 0x3ec ("our player-roster/train-state snapshot"; receive side:
// GameNet_ReceiveRosterSnapshot). Walks pTrainListActive's HEAD train's car slots 1..N
// (slot 0 -- the locomotive -- is skipped), counting how many carry a live CarNetState
// (CarNetObj_GetAppliedState non-NULL). Allocs a PlayerRosterWireMsg sized for exactly that many
// RosterCarStateBlockMaybe records, stamps the header (opcode/session id/car count, written
// twice -- see the struct's own "sic" note), then re-walks the same slots: for each with a live
// state, clears its wAttachmentId id, snapshots it via CarNetStateAlt_CreateFromState, raw-copies the
// 0x390-byte snapshot onto the next wire record slot, frees the snapshot, and clears the car's
// applied net-state. Sends the finished message and frees it.
//
// EFFECTIVE MATCH (asmscore total 202252, align=200 reg_pen=18 byte_diff=152, insns 96/104 at
// true len 327). Two intrinsic, unsteerable residual classes tried and confirmed unmovable
// (~30 min triage): (1) a full 3-way register-role rotation (original keeps pTrain/i/nCount in
// edi/esi/ebp; every recompile picks a different permutation) -- same family as the documented
// symmetric-register-swap class (Yoda #29/#30), just extended to 3 roles instead of 2. (2) the
// second loop's record-slot address: the original recomputes `nCount*0x390` via a fresh
// multiply-decomposition (3 lea + 1 shl) EVERY iteration, while every source form tried here
// (direct `pMsg->records[nCount]` subscript, an explicit bumped output pointer) gets
// strength-reduced by /O2 into a plain `+= 0x390` accumulator instead -- the same
// induction-variable-elimination class as the already-parked `ApplyProviderSnapshot`
// parallel-array residual (CLAUDE.md), just on the write side instead of the read side. The
// direct-subscript form scored best overall (202252 vs 204472 for the pointer-bump variant) so
// it's the one kept, even though its raw byte_diff wasn't the smaller of the two. A dead
// `cmp word ptr [esp+N],3` with no consuming branch also appears in the original right before
// the header writes (spilled first-pass car count vs the literal 3) -- never reproduced by any
// variant, likely a redundant-check-kept-by-original fold residual (CLAUDE.md's fold-vs-keep
// family); not chased further.
void GameNetThreadState::GameNet_BroadcastPlayerRoster() {
    PeerTrainNodePartial *pTrain = pTrainListActive;

    unsigned short i = 1;
    unsigned int nCount = 0;
    if (pTrain->wCarSlotCount != 0) {
        do {
            if (CarNetObj_GetAppliedState(pTrain->carSlots[i]) != 0)
                nCount++;
            i++;
        } while (i <= pTrain->wCarSlotCount);
    }

    unsigned int nSize = nCount * sizeof(RosterCarStateBlockMaybe) + 0x14;  // sic: see struct note
    PlayerRosterWireMsg *pMsg = (PlayerRosterWireMsg *)operator new(nSize);
    pMsg->wOpcode = 0x3ec;
    pMsg->nCarCount = nCount;
    pMsg->sessionId = g_pLocalPlayerIdentity->sessionId;
    pMsg->dwCarCountDup = nCount;

    nCount = 0;
    i = 1;
    if (pTrain->wCarSlotCount != 0) {
        do {
            CarNetState *pState = CarNetObj_GetAppliedState(pTrain->carSlots[i]);
            if (pState != 0) {
                pState = CarNetObj_GetAppliedState(pTrain->carSlots[i]);  // sic: re-read, not cached
                pState->wAttachmentId = 0;
                CarNetStateAlt *pAlt = CarNetStateAlt_CreateFromState(pState);
                memcpy(&pMsg->records[nCount], pAlt, sizeof(RosterCarStateBlockMaybe));
                nCount++;
                if (pAlt != 0)
                    delete pAlt;
                ((CarNetObj *)pTrain->carSlots[i])->CarNetObj_ApplyNetState(NULL);
            }
            i++;
        } while (i <= pTrain->wCarSlotCount);
    }

    g_pNetManager->DPlay_SendMessage(dpidCurrentPlayer, pMsg, nSize, 1);
    operator delete(pMsg);
}

// The board-edge hand-off check, called once per train per tick by
// TrainNet_AdvanceLocalTrainSteps. The four edges are tested in a fixed order -- bottom, top,
// left, right -- and each maps to the heading the train would leave through (0xb4/0/0x10e/0x5a
// = 180/0/270/90 degrees) plus the neighbouring provider slot in the 3x3 board grid
// (+nProviderSlotsPerRow / -nProviderSlotsPerRow / -1 / +1 off the train's own owner slot).
//
// When that neighbour has room, the train is unlinked from whichever list it was walking and
// either synced to the peer owning the target slot or, when the slot is unowned, handed to
// TrainNet_HandleEmptySlotHandoffMaybe; either way the return is 1 so the caller stops ticking
// this train. When the neighbour is full the train is bounced instead: wLocalHeading is
// reflected to the OPPOSITE heading and the stall-detection checkpoint is reset, and the return
// is still 1. Only a train that is not on any edge at all returns 0.
//
// EFFECTIVE MATCH -- insns 255/257, total 70133 (align 70, reg_pen 1, byte_diff 23) against 685
// real bytes. Every structural fact is pinned: all four edge tests, both branch polarities, the
// bottom-edge-only bDirty guard, the reflected headings and the shared tail all land
// instruction-for-instruction. The entire residual is ONE peephole coin-flip repeated three
// times. ProviderSlotAt's inlined `i >= 0` guard compiles to a bare `js` here, where the
// original re-materializes the comparison as `cmp <idx>, <zero-reg>; jl` -- and the original
// itself is INCONSISTENT about it: the top edge, whose index is computed with `sub`, uses the
// same bare `js` we emit and matches exactly, while the three computed with `add`/`inc`/`dec`
// (bottom/right/left) take the long form. Same compiler, same helper, same shape -- cl simply
// declines the flag-reuse peephole after those three opcodes and takes it after `sub`. The
// 2-instruction deficit is exactly those three `cmp`s; the one reg_pen is the operand-order
// coin-flip on the commutative `add` that follows from it (cl parks bOwnerByteB in EDX and the
// row width in EAX, the original the other way round).
//
// Probed and INERT, one compile each: the `if (i >= 0) return &...; return 0;` spelling of
// ProviderSlotAt (byte-identical); swapping the commutative add's operands (byte-identical --
// cl normalizes); `nSlot = ...; nSlot += ...;` (byte-identical); block-scoping nSlot/pSlot per
// branch instead of one function-scope pair (byte-identical). Probed and WORSE: inverting
// ProviderSlotAt to `i < 0 ? 0 : &aProviderSlots[i]` (DIFF 260 -> 282). Note the left/right
// edges have no operand order to flip at all -- `dec eax`/`inc eax` are identical on both sides
// and only the following test differs, which is what rules out any source-level lever.
//
// // sic: two real asymmetries in the original. The bottom edge is the ONLY one that also
// requires the target slot's bDirty to be set before syncing -- the other three test
// providerId alone -- and every branch dereferences ProviderSlotAt's result without a null
// check, so a negative computed slot index (top or left edge on an owner slot in row/column 0)
// reads through a NULL pointer. Reproduced as-is.
//
// FUNCTION: LOCO 0x43c160
char GameNetThreadState::TrainNet_TryBoardEdgeHandoffMaybe(PeerTrainNodePartial *pPrev,
                                                           PeerTrainNodePartial *pTrain,
                                                           int x, int y, int cols, int rows)
{
    int nSlot;
    DPlaySessionMgrProviderSlot *pSlot;

    if (y >= rows - 1) {
        if (g_pDPlaySessionMgr->HasProviderSlotRoomInHeading(0xb4, pTrain->bOwnerByteB)) {
            if (pPrev != NULL) {
                pPrev->pNext = pTrain->pNext;
            } else {
                pTrainListRehomed = (PeerTrainNodePartial *)pTrain->pNext;
            }
            pTrain->pNext = NULL;

            nSlot = pTrain->bOwnerByteB + g_pDPlaySessionMgr->nProviderSlotsPerRow;
            pSlot = g_pDPlaySessionMgr->ProviderSlotAt(nSlot);
            if (pSlot->providerId > 0 && pSlot->bDirty != 0) {
                GameNet_SendTrainStateSync(pSlot->providerId, pTrain, 0xb4, 0);
            } else {
                TrainNet_HandleEmptySlotHandoffMaybe(pTrain, 0xb4, nSlot);
            }
            return 1;
        }
        pTrain->wLocalHeading = 0;
    } else if (y <= 0) {
        if (g_pDPlaySessionMgr->HasProviderSlotRoomInHeading(0, pTrain->bOwnerByteB)) {
            if (pPrev != NULL) {
                pPrev->pNext = pTrain->pNext;
            } else {
                pTrainListRehomed = (PeerTrainNodePartial *)pTrain->pNext;
            }
            pTrain->pNext = NULL;

            nSlot = pTrain->bOwnerByteB - g_pDPlaySessionMgr->nProviderSlotsPerRow;
            pSlot = g_pDPlaySessionMgr->ProviderSlotAt(nSlot);
            if (pSlot->providerId <= 0) {
                TrainNet_HandleEmptySlotHandoffMaybe(pTrain, 0, nSlot);
            } else {
                GameNet_SendTrainStateSync(pSlot->providerId, pTrain, 0, 0);
            }
            return 1;
        }
        pTrain->wLocalHeading = 0xb4;
    } else if (x <= 0) {
        if (g_pDPlaySessionMgr->HasProviderSlotRoomInHeading(0x10e, pTrain->bOwnerByteB)) {
            if (pPrev != NULL) {
                pPrev->pNext = pTrain->pNext;
            } else {
                pTrainListRehomed = (PeerTrainNodePartial *)pTrain->pNext;
            }
            pTrain->pNext = NULL;

            nSlot = pTrain->bOwnerByteB - 1;
            pSlot = g_pDPlaySessionMgr->ProviderSlotAt(nSlot);
            if (pSlot->providerId <= 0) {
                TrainNet_HandleEmptySlotHandoffMaybe(pTrain, 0x10e, nSlot);
            } else {
                GameNet_SendTrainStateSync(pSlot->providerId, pTrain, 0x10e, 0);
            }
            return 1;
        }
        pTrain->wLocalHeading = 0x5a;
    } else if (x >= cols - 1) {
        if (g_pDPlaySessionMgr->HasProviderSlotRoomInHeading(0x5a, pTrain->bOwnerByteB)) {
            if (pPrev != NULL) {
                pPrev->pNext = pTrain->pNext;
            } else {
                pTrainListRehomed = (PeerTrainNodePartial *)pTrain->pNext;
            }
            pTrain->pNext = NULL;

            nSlot = pTrain->bOwnerByteB + 1;
            pSlot = g_pDPlaySessionMgr->ProviderSlotAt(nSlot);
            if (pSlot->providerId <= 0) {
                TrainNet_HandleEmptySlotHandoffMaybe(pTrain, 0x5a, nSlot);
            } else {
                GameNet_SendTrainStateSync(pSlot->providerId, pTrain, 0x5a, 0);
            }
            return 1;
        }
        pTrain->wLocalHeading = 0x10e;
    } else {
        return 0;
    }

    pTrain->bStallStepCounter = 0;
    pTrain->wCheckpointPosX = -1;
    pTrain->wCheckpointPosY = -1;
    return 1;
}

// FUNCTION: LOCO 0x43c410
// Local command opcode 0xe ("connect or join session", GameNet_ProcessLocalCommand case 0xe):
// the full session bootstrap. pNode's payload is always a PeerTrainNodePartial* (the sole
// producer, DPlaySessionMgr::GameNet_PostConnectOrJoinForNode, stashes one and sets its
// bHasDetailFlagMaybe). Only proceeds while DPlaySessionMgr::connectionMode==1 ("network game
// active, still connecting"); in any other mode this just releases the train and clears
// pNode->pPayload (nothing to connect).
//
// Stamps the train's own heading to a long countdown sentinel (32000, sic) and, unless it's
// flagged for outright release (nDiscardFlag==1, which just releases it immediately instead),
// appends it onto this->pTrainListActive -- reused here as a "pending connect" list, the
// same field GameNetThread_TickLoop/etc. otherwise drain as notify type 0xf. If the list already
// had entries, appends to the tail; if safe mode is off (DAT_004a9918 != 1) returns without
// draining (someone else's earlier call already owns the real connect attempt) -- only in safe
// mode does THIS call drain the whole list as type-0xf local notifies and return. If the list was
// empty, this call becomes the list's head AND (unless safe mode or bSkipConnectMaybe) the one
// responsible for the real connect attempt: create g_pNetManager if needed; if not already
// joined, teardown+InitBigFields+PrepareInternetConnection, then (if a DirectPlay4 interface came
// up) read the ini-configured session name and DPlay_JoinOrHostSession, retrying once after a 1s
// Sleep+re-teardown on failure. On total failure, post a type-0x1c "connect failed" local notify
// and register a placeholder solo/offline player card (own name + "LEGO LOCO", one decal,
// category 1) via PostBag_SaveCardToCategory, then drain the pending list as notify-0xf either way.
void GameNetThreadState::GameNet_ConnectOrJoinSession(NetMsgQueueNode *pNode) {
    PeerTrainNodePartial *pTrain = (PeerTrainNodePartial *)pNode->pPayload;

    if (g_pDPlaySessionMgr->connectionMode != 1) {
        if (pTrain != 0)
            ((PeerTrainNodeVtblProbe *)pTrain)->ScalarDeletingDtor(1);
        pNode->pPayload = 0;
        return;
    }

    pTrain->wHeading = 32000;
    if (pTrain->nDiscardFlag == 1) {
        if (pTrain != 0)
            ((PeerTrainNodeVtblProbe *)pTrain)->ScalarDeletingDtor(1);
        pNode->pPayload = 0;
        return;
    }

    if (this->pTrainListActive != 0) {
        PeerTrainNodePartial *pTail = this->pTrainListActive;
        while (pTail->pNext != 0)
            pTail = (PeerTrainNodePartial *)pTail->pNext;
        pTrain->pNext = 0;
        pTail->pNext = pTrain;

        if (DAT_004a9918 != 1)
            return;
        if (this->pTrainListActive == 0)
            return;
        do {
            NetMsgQueueNode *pOut = new NetMsgQueueNode();
            pOut->type = 0xf;
            pOut->pPayload = this->pTrainListActive;
            this->pTrainListActive->bHasDetailFlagMaybe = 0;
            this->pTrainListActive = (PeerTrainNodePartial *)this->pTrainListActive->pNext;
            ((PeerTrainNodePartial *)pOut->pPayload)->pNext = 0;
            g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pOut);
        } while (this->pTrainListActive != 0);
        return;
    }

    pTrain->pNext = 0;
    this->pTrainListActive = pTrain;

    if (DAT_004a9918 == 1 || this->bSkipConnectMaybe != 0) {
        if (pTrain != 0) {
            do {
                NetMsgQueueNode *pOut = new NetMsgQueueNode();
                pOut->type = 0xf;
                pOut->pPayload = this->pTrainListActive;
                this->pTrainListActive->bHasDetailFlagMaybe = 0;
                this->pTrainListActive = (PeerTrainNodePartial *)this->pTrainListActive->pNext;
                ((PeerTrainNodePartial *)pOut->pPayload)->pNext = 0;
                g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pOut);
            } while (this->pTrainListActive != 0);
        }
        return;
    }

    if (g_pNetManager == 0) {
        g_pNetManager = new GNetManager(this->hInstance, 0);
        g_pNetManager->pfnErrorTextHookMaybe = 0;
        g_pNetManager->bShowErrorMessageBoxMaybe = false;
        g_pNetManager->hWndParent = this->hwndOwner;
    }

    if (g_pNetManager->bSessionJoined != 0)
        goto broadcast_roster;

    g_pNetManager->DPlay_TeardownConnection();
    g_pNetManager->InitBigFields(0, 1, 0, 0);
    this->DPlay_PrepareInternetConnection(0);

    char szSessionName[0x200];
    bool bHasDirectPlay;
    bHasDirectPlay = g_pNetManager->pDirectPlay4 != 0;
    if (bHasDirectPlay) {
        g_pIniFile->ReadString("Configuration", "Name", "LEGO International Train Server",
                                     szSessionName, sizeof(szSessionName));
        g_pNetManager->DPlay_JoinOrHostSession(g_pLocalPlayerIdentity->name, szSessionName, 0);
        if (!g_pNetManager->bSessionJoined) {
            g_pNetManager->DPlay_TeardownConnection();
            Sleep(1000);
            g_pNetManager->InitBigFields(0, 1, 0, 0);
            this->DPlay_PrepareInternetConnection(0);
            g_pNetManager->DPlay_JoinOrHostSession(g_pLocalPlayerIdentity->name, szSessionName, 0);
        }
    }

    if (g_pNetManager->bSessionJoined != 0)
        return;

    NetMsgQueueNode *pFailNode;
    pFailNode = new NetMsgQueueNode();
    pFailNode->type = 0x1c;
    pFailNode->pNext = 0;
    pFailNode->pPayload = 0;
    g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pFailNode);

    CarNetState *pCard;
    pCard = new CarNetState();
    g_UIResources.LoadLocaleString(0xdf, pCard->szDescription, sizeof(pCard->szDescription));
    pCard->AddDecal(5, 1, 5, 0x94, 99, 0x48, 0x48);
    pCard->byIdentityColorG = 0xff;
    strcpy(pCard->nameA, g_pLocalPlayerIdentity->name);
    strcpy(pCard->nameB, "LEGO LOCO");
    g_pPostBagCache->PostBag_SaveCardToCategory(pCard, 1, 0);
    if (pCard != 0)
        delete pCard;

    if (this->pTrainListActive != 0) {
        do {
            NetMsgQueueNode *pOut = new NetMsgQueueNode();
            pOut->type = 0xf;
            pOut->pPayload = this->pTrainListActive;
            this->pTrainListActive->bHasDetailFlagMaybe = 0;
            this->pTrainListActive = (PeerTrainNodePartial *)this->pTrainListActive->pNext;
            ((PeerTrainNodePartial *)pOut->pPayload)->pNext = 0;
            g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pOut);
        } while (this->pTrainListActive != 0);
    }
    return;

broadcast_roster:
    this->GameNet_BroadcastPlayerRoster();
}

// FUNCTION: LOCO 0x4611b0 (?DPlay_JoinSessionDlgProc@@YGHPAXIIJ@Z)
// DLGPROC for the join-session-browser dialog (resource 0x7d0b, listbox control 0x7d20),
// launched by DPlay_JoinExistingSession when pszSessionName is empty. Real 4-param DLGPROC
// (`ret 0x10`) -- Ghidra's own original 3-explicit-param inference silently dropped the unused
// lParam, same "dead-but-real parameter" tell documented elsewhere in this file.
// WM_INITDIALOG (0x110): drains+frees the stale pGlobalLockedBuf GUID copy (unless
// bSkipGlobalFree says to keep it) and the whole pListHead1 found-session list (same
// GlobalHandle/GlobalUnlock/GlobalFree + operator-delete idiom as DPlay_FindSession's own drain
// loop -- duplicated inline here rather than shared, matching the original's own 2 separate
// physical copies), then re-enumerates (DPlay_FindSession(0), retrying on DPERR_CONNECTING) and
// fills the listbox (LB_ADDSTRING the name, LB_SETITEMDATA the returned index to the
// pSessionDescMem pointer), selecting the first entry and focusing the listbox.
// WM_COMMAND (0x111): HIWORD(wParam)==2 (a listbox notification, fired on double-click) reads
// the current selection and stashes its LB_GETITEMDATA'd pointer into pGlobalLockedBuf (an
// unselected listbox is simply ignored -- shares its own "return 1" tail with the ID_REFRESH
// retry-exhausted path below, a real cross-jump in the original, reproduced here via `goto`).
// Otherwise dispatches on LOWORD(wParam) (control id): IDOK (1) does the same
// LB_GETITEMDATA/LB_GETTEXT selection read but EndDialog(0)s immediately when nothing is
// selected; IDCANCEL (2) clears pGlobalLockedBuf and EndDialog(0)s unconditionally;
// ID_REFRESH (0x7d01) redrains+re-enumerates+refills the listbox exactly like WM_INITDIALOG,
// EndDialog-free either way. A successful IDOK/notification pick falls into the shared tail
// (bSkipGlobalFree = true; EndDialog(hDlg, 1); return 1) -- the SAME physical tail the
// no-selection listbox-notification case and ID_REFRESH's retry-exhausted case jump directly
// into (skipping the EndDialog call), a genuine shared-return-CONST cross-jump in the original
// (Yoda's "independent `return CONST` sites don't auto-share; a shared `goto` does" family).
// EFFECTIVE MATCH (asmscore byte_diff 427 at true len 1104, insns 343/340 -- every branch
// polarity, message constant (LB_* listbox IDs, not combobox -- 0x180-0x19a fall in the
// standard LB_* range), and field write confirmed correct against the decompile). Needed 3
// separate levers to get this close: (1) the outer WM_INITDIALOG/WM_COMMAND dispatch and the
// inner id==1/id==2/id==0x7d01 dispatch both wanted a real `switch` (decrement-chain codegen,
// Yoda #11) over an if-ladder -- an if-chain compiled independent `cmp`s instead of the
// original's `dec ecx;je` chain; (2) the HIWORD(wParam)==2 vs LOWORD-dispatch branch needed the
// LOWORD switch as the primary/fall-through and HIWORD==2 as the `else` (matching the original's
// `cmp ecx,2;je <far>` -- jump-away-when-2 shape); (3) each of the 2 duplicated drain-loop+
// retry-loop regions (WM_INITDIALOG's own, and ID_REFRESH's) needed `GNetManager *pNet =
// g_pNetManager;` SCOPED locally around just that region -- without it, the compiler reloaded
// the g_pNetManager global fresh via a different scratch register at nearly every field access
// instead of caching it in one callee-saved register for the loop's duration; scoping the local
// tightly (not function-wide) avoided pinning a register across the whole function. Residual is
// the id==1/notification==2 cases' identical "LB_GETCOUNT then conditionally LB_GETCURSEL"
// prologue getting physically interleaved/duplicated in one extra copy (an intrinsic tail-
// scheduling choice, confirmed source-order-INDEPENDENT -- swapping the case 1/0x7d01 source
// order was a no-op, consistent with the already-documented compare-chain layout lesson) plus
// the usual `cmp reg,ebx`-vs-`test reg,reg` zero-check register tie-breaks (Yoda #29/#30
// family). Triage-budgeted after 3 confirmed structural fixes; not re-litigated further.
int __stdcall DPlay_JoinSessionDlgProc(HWND hDlg, unsigned int uMsg, unsigned int wParam,
                                             long lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        if (g_pNetManager->pGlobalLockedBuf != 0 && !g_pNetManager->bSkipGlobalFree) {
            HGLOBAL hMem = GlobalHandle(g_pNetManager->pGlobalLockedBuf);
            GlobalUnlock(hMem);
            hMem = GlobalHandle(g_pNetManager->pGlobalLockedBuf);
            GlobalFree(hMem);
        }
        g_pNetManager->bSkipGlobalFree = false;
        g_pNetManager->pGlobalLockedBuf = 0;

        FoundSessionNode *pFound;
        {
            GNetManager *pNet = g_pNetManager;
            while (pNet->pListHead1 != 0) {
                FoundSessionNode *pNext = pNet->pListHead1->pNext;
                if (pNet->pListHead1->pszName != 0) {
                    operator delete(pNet->pListHead1->pszName);
                }
                if (pNet->pListHead1->pSessionDescMem != 0) {
                    HGLOBAL hMem = GlobalHandle(pNet->pListHead1->pSessionDescMem);
                    GlobalUnlock(hMem);
                    hMem = GlobalHandle(pNet->pListHead1->pSessionDescMem);
                    GlobalFree(hMem);
                    pNet->pListHead1->pSessionDescMem = 0;
                }
                operator delete(pNet->pListHead1);
                pNet->pListHead1 = pNext;
            }

            pFound = pNet->DPlay_FindSession(0);
            if (pFound == 0) {
                while (pNet->hrLastResult == 0x8877015e) {
                    Sleep(1);
                    pFound = pNet->DPlay_FindSession(0);
                    if (pFound != 0) {
                        break;
                    }
                }
            }
        }
        if (pFound != 0) {
            do {
                WPARAM nIndex = SendDlgItemMessageA(hDlg, 0x7d20, LB_ADDSTRING, 0,
                                                     (LPARAM)pFound->pszName);
                SendDlgItemMessageA(hDlg, 0x7d20, LB_SETITEMDATA, nIndex,
                                     (LPARAM)pFound->pSessionDescMem);
                pFound = pFound->pNext;
            } while (pFound != 0);
        }
        SendDlgItemMessageA(hDlg, 0x7d20, LB_SETCURSEL, 0, 0);
        SetFocus(GetDlgItem(hDlg, 0x7d20));
        return 0;
    }
    case WM_COMMAND:
        break;
    default:
        return 0;
    }

    if (HIWORD(wParam) != 2) {
        switch (wParam & 0xffff) {
        case 2:
            g_pNetManager->pGlobalLockedBuf = 0;
            g_pNetManager->bSkipGlobalFree = false;
            EndDialog(hDlg, 0);
            return 1;

        case 1: {
            LRESULT nCount = SendDlgItemMessageA(hDlg, 0x7d20, LB_GETCOUNT, 0, 0);
            WPARAM nSel;
            if (nCount == 0) {
                nSel = 0xffffffff;
            } else {
                nSel = SendDlgItemMessageA(hDlg, 0x7d20, LB_GETCURSEL, 0, 0);
            }
            if (nSel == 0xffffffff) {
                g_pNetManager->sSessionName[0] = 0;
                g_pNetManager->pGlobalLockedBuf = 0;
                g_pNetManager->bSkipGlobalFree = false;
                EndDialog(hDlg, 0);
                return 1;
            }
            void *pData = (void *)SendDlgItemMessageA(hDlg, 0x7d20, LB_GETITEMDATA, nSel,
                                                       (LPARAM)g_pNetManager->sSessionName);
            g_pNetManager->pGlobalLockedBuf = pData;
            SendDlgItemMessageA(hDlg, 0x7d20, LB_GETTEXT, nSel,
                                 (LPARAM)g_pNetManager->sSessionName);
            break;
        }

        case 0x7d01: {
            FoundSessionNode *pFound;
            {
                GNetManager *pNet = g_pNetManager;
                while (pNet->pListHead1 != 0) {
                    FoundSessionNode *pNext = pNet->pListHead1->pNext;
                    if (pNet->pListHead1->pszName != 0) {
                        operator delete(pNet->pListHead1->pszName);
                    }
                    if (pNet->pListHead1->pSessionDescMem != 0) {
                        HGLOBAL hMem = GlobalHandle(pNet->pListHead1->pSessionDescMem);
                        GlobalUnlock(hMem);
                        hMem = GlobalHandle(pNet->pListHead1->pSessionDescMem);
                        GlobalFree(hMem);
                        pNet->pListHead1->pSessionDescMem = 0;
                    }
                    operator delete(pNet->pListHead1);
                    pNet->pListHead1 = pNext;
                }

                SendDlgItemMessageA(hDlg, 0x7d20, LB_RESETCONTENT, 0, 0);
                pFound = pNet->DPlay_FindSession(0);
                if (pFound == 0) {
                    while (pNet->hrLastResult == 0x8877015e) {
                        Sleep(1);
                        pFound = pNet->DPlay_FindSession(0);
                        if (pFound != 0) {
                            break;
                        }
                    }
                    if (pFound == 0) {
                        goto return_true;
                    }
                }
            }
            do {
                WPARAM nIndex = SendDlgItemMessageA(hDlg, 0x7d20, LB_ADDSTRING, 0,
                                                     (LPARAM)pFound->pszName);
                SendDlgItemMessageA(hDlg, 0x7d20, LB_SETITEMDATA, nIndex,
                                     (LPARAM)pFound->pSessionDescMem);
                pFound = pFound->pNext;
            } while (pFound != 0);
            return 1;
        }

        default:
            return 0;
        }
    } else {
        LRESULT nCount = SendDlgItemMessageA(hDlg, 0x7d20, LB_GETCOUNT, 0, 0);
        WPARAM nSel;
        if (nCount == 0) {
            nSel = 0xffffffff;
        } else {
            nSel = SendDlgItemMessageA(hDlg, 0x7d20, LB_GETCURSEL, 0, 0);
        }
        if (nSel == 0xffffffff) {
            goto return_true;
        }
        SendDlgItemMessageA(hDlg, 0x7d20, LB_GETTEXT, nSel, (LPARAM)g_pNetManager->sSessionName);
        void *pData = (void *)SendDlgItemMessageA(hDlg, 0x7d20, LB_GETITEMDATA, nSel, 0);
        g_pNetManager->pGlobalLockedBuf = pData;
    }

    g_pNetManager->bSkipGlobalFree = true;
    EndDialog(hDlg, 1);
return_true:
    return 1;
}

// FUNCTION: LOCO 0x460620 (?DPlay_JoinSessionEnumCallback@@YGHPAUDPSessionDesc2Partial@@PAIIPAX@Z)
// DPENUMSESSIONSCALLBACK2 for DPlay_JoinExistingSession's own single-target EnumSessions
// call (vtbl+0x34) -- unlike DPlay_EnumSessionsCallback's own list-building version, this
// one looks for ONE specific session (exact strcmp match against g_pNetManager->sSessionName,
// the real pszSessionName DPlay_JoinOrHostSession copied in) and stashes just its guidInstance
// into g_pNetManager->pGlobalLockedBuf (GlobalAlloc/GlobalLock'd 16 bytes -- same idiom as
// DPlay_EnumSessionsCallback's own GUID copy), returning FALSE (stop enumerating) the
// moment a match is found (even when the alloc/lock itself fails), TRUE (keep enumerating)
// otherwise. Timeout calls (dwFlags & DPESC_TIMEDOUT) share nProtocol's gating with
// DPlay_EnumSessionsCallback, but simpler -- no found-list to also check here, since this
// callback never builds one. Branch order mirrors DPlay_EnumSessionsCallback's own proven
// shape (timeout check as the primary `if`/fall-through, match-and-copy as the `else`).
int __stdcall DPlay_JoinSessionEnumCallback(DPSessionDesc2Partial *lpDPSessionDesc,
                                                  unsigned int *lpdwTimeOut, unsigned int dwFlags,
                                                  void *lpContext) {
    if ((dwFlags & 1) != 0) {
        return g_pNetManager->nProtocol == 1;
    } else {
        if (strcmp(lpDPSessionDesc->lpszSessionNameA, g_pNetManager->sSessionName) == 0) {
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, 0x10);
            GUID *pGuidCopy = (GUID *)GlobalLock(hMem);
            if (pGuidCopy != 0) {
                *pGuidCopy = lpDPSessionDesc->guidInstance;
                g_pNetManager->pGlobalLockedBuf = pGuidCopy;
            }
            return 0;
        }
    }
    return 1;
}

// FUNCTION: LOCO 0x45fd80
// DPlay_JoinOrHostSession's host-branch callee: builds a fresh session descriptor (dwFlags =
// DPSESSION_MIGRATEHOST|DPSESSION_KEEPALIVE|DPSESSION_DIRECTPLAYPROTOCOL (0xa040), + 4 when
// bAllowHostMigration is set) and calls IDirectPlay4's Open method (vtbl+0x60) synchronously, retrying on
// DPERR_CONNECTING (0x8877015e) via Sleep(1) until either it resolves or bJoinAttempted is
// set. On success sets bSessionJoined and returns 1. DPERR_NOSESSIONS's own sentinel
// (0x88770118) short-circuits to a masked 0x88770100. Any other failure formats an HRESULT
// string and reports it via DPlay_ReportNetworkError -- szMsgBuf's char-array aggregate
// initializer ("Failed to Open new session - ", zero-padded to 300 bytes by the language,
// exactly matching the original's own literal-dword-copy + zero-fill-remainder shape) then
// gets the formatted HRESULT text appended: the original computes strlen(szErrBuf) and
// strlen(szMsgBuf) separately and memcpy's exactly the source length onto the destination's end
// (no null-terminator byte copied -- the destination's own zero-filled tail already terminates
// it), NOT a real strcat() call (which this toolchain apparently never inlines the way it does
// strlen/strcpy/memcpy/memset -- a real strcat() call site compiles to a genuine external call).
// bAllowHostMigration is read into a local BEFORE the memset (matching the original's own early read,
// Yoda's "declare-before-call" scheduling family) -- must stay `unsigned char`, not `bool`
// (a `bool` local forces an extra setne/bool-normalization sequence the original lacks; the raw
// byte feeds the neg/sbb 0-or-4 mask idiom directly). The final hr==0 (success) vs hr!=0 (error)
// split needed the real `if (hr != 0) {error...} else {success}` polarity (error primary, success
// as the trailing fall-through) -- a bare `if (hr==0) goto success;` compiled the OPPOSITE
// layout regardless of goto placement (this function's own success block is short enough that
// /O2 always prefers it as the physical fall-through unless the source spells out the inverted
// condition explicitly).
// EFFECTIVE MATCH (asmscore byte_diff 129 at true len 419, insns 132/134 -- branch polarity,
// calling convention, and the memcpy/strlen error-string shape all confirmed correct). Residual
// is a register-allocation-class difference: the original uses 4 callee-saved registers
// (ebx=this, plus ebp/esi/edi) across the whole function including the memcpy-intrinsic's own
// internal length/pointer bookkeeping, while this compile only needs 3 (no ebp) -- the extra
// register lets the original keep more values live across the strlen/memcpy/DPlay_* calls
// without reloading; no source variant tried (explicit pointer locals, separate length locals)
// induced the same allocation. Triage-budgeted; same family as the already-documented
// symmetric-register-swap/allocator tie-break residuals elsewhere in this file.
unsigned int GNetManager::DPlay_HostNewSession() {
    if (pDirectPlay4 == 0) {
        return 0;
    }
    unsigned char bUnk = bAllowHostMigration;

    memset(&sessionDesc, 0, sizeof(sessionDesc));
    sessionDesc.dwSize = sizeof(sessionDesc);
    sessionDesc.dwFlags = (bUnk ? 4 : 0) + 0xa040;
    sessionDesc.guidApplication = g_guidLocoApp;
    sessionDesc.dwMaxPlayers = nMaxPlayers;
    sessionDesc.lpszSessionNameA = sSessionName;
    if (sPassword[0] != 0) {
        sessionDesc.lpszPasswordA = sPassword;
    }

    hrLastResult = ((IDirectPlay4VtblProbe *)pDirectPlay4)->Open(&sessionDesc, 0x82);
    while (hrLastResult == 0x8877015e && !bJoinAttempted) {
        Sleep(1);
        hrLastResult = ((IDirectPlay4VtblProbe *)pDirectPlay4)->Open(&sessionDesc, 0x82);
    }

    if (hrLastResult != 0) {
        if (hrLastResult == 0x88770118) {
            return 0x88770100;
        }
        char szErrBuf[212];
        char szMsgBuf[300] = "Failed to Open new session - ";
        DPlay_FormatHresultString(szErrBuf, hrLastResult);
        memcpy(szMsgBuf + strlen(szMsgBuf), szErrBuf, strlen(szErrBuf));  // idiom-exempt: strlen()-sized, not a magic number (matches the original's own manual strlen+copy expansion)
        return DPlay_ReportNetworkError(0, szMsgBuf) & 0xffffff00;
    }
    bSessionJoined = true;
    return 1;
}

// FUNCTION: LOCO 0x45ff30
// The DirectPlay HRESULT-to-name table every error path in this TU runs its result through
// before showing it. 50 named codes plus a printf fallback; see GNetManager.h for why this is a
// `this`-ignoring __thiscall.
//
// The codes are spelled as raw hex with the SDK name in a comment rather than as DPERR_*
// macros, for the reason recorded at DPlay_InitConnection above: this project never includes
// <dplay.h>, and pulling it in here just for 50 constants would re-roll this TU's whole /Og
// state. The values ARE the SDK's.
//
// sic: six of the names carry a stray TRAILING SPACE in the shipped image
// ("DPERR_CANTADDPLAYER ", "DPERR_EXCEPTION ", "DPERR_SENDTOOBIG ", "DPERR_CANTCREATEPROCESS ",
// "DPERR_APPNOTSTARTED ", "DPERR_UNKNOWNAPPLICATION "), and the fallback reads "Unknown Error
// Code - %d" with a DASH. Both are read out of the image's own string bytes -- Ghidra's auto
// labels render the dash as an underscore and drop nothing about the spaces, which is exactly
// the trap CLAUDE.md warns about.
//
// `default:` sits in the MIDDLE of the case list, before DP_OK / DPERR_NOTLOGGEDIN /
// DPERR_NOTLOBBIED. That is not a transcription quirk: the image's string pool holds "Unknown
// Error Code - %d" BETWEEN "DPERR_UNKNOWNAPPLICATION " and "DP_OK", so those three cases really
// were appended after the default clause was already written. Codegen-neutral (the compare tree
// is built from the sorted values either way), kept because it is what the source said.
void GNetManager::DPlay_FormatHresultString(char *pszOutBuf, int hresult) {
    switch (hresult) {
    case (int)0x8000000a: // DPERR_PENDING
        strcpy(pszOutBuf, "DPERR_PENDING");
        break;
    case (int)0x80004001: // DPERR_UNSUPPORTED
        strcpy(pszOutBuf, "DPERR_UNSUPPORTED");
        break;
    case (int)0x80004002: // DPERR_NOINTERFACE
        strcpy(pszOutBuf, "DPERR_NOINTERFACE");
        break;
    case (int)0x80004005: // DPERR_GENERIC
        strcpy(pszOutBuf, "DPERR_GENERIC");
        break;
    case (int)0x80040110: // CLASS_E_NOAGGREGATION
        strcpy(pszOutBuf, "CLASS_E_NOAGGREGATION");
        break;
    case (int)0x8007000e: // DPERR_NOMEMORY
        strcpy(pszOutBuf, "DPERR_NOMEMORY");
        break;
    case (int)0x80070057: // DPERR_INVALIDPARAM
        strcpy(pszOutBuf, "DPERR_INVALIDPARAM");
        break;
    case (int)0x88770005: // DPERR_ALREADYINITIALIZED
        strcpy(pszOutBuf, "DPERR_ALREADYINITIALIZED");
        break;
    case (int)0x8877000a: // DPERR_ACCESSDENIED
        strcpy(pszOutBuf, "DPERR_ACCESSDENIED");
        break;
    case (int)0x88770014: // DPERR_ACTIVEPLAYERS
        strcpy(pszOutBuf, "DPERR_ACTIVEPLAYERS");
        break;
    case (int)0x8877001e: // DPERR_BUFFERTOOSMALL
        strcpy(pszOutBuf, "DPERR_BUFFERTOOSMALL");
        break;
    case (int)0x88770028: // DPERR_CANTADDPLAYER 
        strcpy(pszOutBuf, "DPERR_CANTADDPLAYER ");
        break;
    case (int)0x88770032: // DPERR_CANTCREATEGROUP
        strcpy(pszOutBuf, "DPERR_CANTCREATEGROUP");
        break;
    case (int)0x8877003c: // DPERR_CANTCREATEPLAYER
        strcpy(pszOutBuf, "DPERR_CANTCREATEPLAYER");
        break;
    case (int)0x88770046: // DPERR_CANTCREATESESSION
        strcpy(pszOutBuf, "DPERR_CANTCREATESESSION");
        break;
    case (int)0x88770050: // DPERR_CAPSNOTAVAILABLEYET
        strcpy(pszOutBuf, "DPERR_CAPSNOTAVAILABLEYET");
        break;
    case (int)0x8877005a: // DPERR_EXCEPTION 
        strcpy(pszOutBuf, "DPERR_EXCEPTION ");
        break;
    case (int)0x88770078: // DPERR_INVALIDFLAGS
        strcpy(pszOutBuf, "DPERR_INVALIDFLAGS");
        break;
    case (int)0x88770082: // DPERR_INVALIDOBJECT
        strcpy(pszOutBuf, "DPERR_INVALIDOBJECT");
        break;
    case (int)0x88770096: // DPERR_INVALIDPLAYER
        strcpy(pszOutBuf, "DPERR_INVALIDPLAYER");
        break;
    case (int)0x8877009b: // DPERR_INVALIDGROUP
        strcpy(pszOutBuf, "DPERR_INVALIDGROUP");
        break;
    case (int)0x887700a0: // DPERR_NOCAPS
        strcpy(pszOutBuf, "DPERR_NOCAPS");
        break;
    case (int)0x887700aa: // DPERR_NOCONNECTION
        strcpy(pszOutBuf, "DPERR_NOCONNECTION");
        break;
    case (int)0x887700be: // DPERR_NOMESSAGES
        strcpy(pszOutBuf, "DPERR_NOMESSAGES");
        break;
    case (int)0x887700c8: // DPERR_NONAMESERVERFOUND
        strcpy(pszOutBuf, "DPERR_NONAMESERVERFOUND");
        break;
    case (int)0x887700d2: // DPERR_NOPLAYERS
        strcpy(pszOutBuf, "DPERR_NOPLAYERS");
        break;
    case (int)0x887700dc: // DPERR_NOSESSIONS
        strcpy(pszOutBuf, "DPERR_NOSESSIONS");
        break;
    case (int)0x887700e6: // DPERR_SENDTOOBIG 
        strcpy(pszOutBuf, "DPERR_SENDTOOBIG ");
        break;
    case (int)0x887700f0: // DPERR_TIMEOUT
        strcpy(pszOutBuf, "DPERR_TIMEOUT");
        break;
    case (int)0x887700fa: // DPERR_UNAVAILABLE
        strcpy(pszOutBuf, "DPERR_UNAVAILABLE");
        break;
    case (int)0x8877010e: // DPERR_BUSY
        strcpy(pszOutBuf, "DPERR_BUSY");
        break;
    case (int)0x88770118: // DPERR_USERCANCEL
        strcpy(pszOutBuf, "DPERR_USERCANCEL");
        break;
    case (int)0x8877012c: // DPERR_PLAYERLOST
        strcpy(pszOutBuf, "DPERR_PLAYERLOST");
        break;
    case (int)0x88770136: // DPERR_SESSIONLOST
        strcpy(pszOutBuf, "DPERR_SESSIONLOST");
        break;
    case (int)0x88770140: // DPERR_UNINITIALIZED
        strcpy(pszOutBuf, "DPERR_UNINITIALIZED");
        break;
    case (int)0x8877014a: // DPERR_NONEWPLAYERS
        strcpy(pszOutBuf, "DPERR_NONEWPLAYERS");
        break;
    case (int)0x88770168: // DPERR_CONNECTIONLOST
        strcpy(pszOutBuf, "DPERR_CONNECTIONLOST");
        break;
    case (int)0x88770172: // DPERR_UNKNOWNMESSAGE
        strcpy(pszOutBuf, "DPERR_UNKNOWNMESSAGE");
        break;
    case (int)0x8877017c: // DPERR_CANCELFAILED
        strcpy(pszOutBuf, "DPERR_CANCELFAILED");
        break;
    case (int)0x88770186: // DPERR_INVALIDPRIORITY
        strcpy(pszOutBuf, "DPERR_INVALIDPRIORITY");
        break;
    case (int)0x88770190: // DPERR_NOTHANDLED
        strcpy(pszOutBuf, "DPERR_NOTHANDLED");
        break;
    case (int)0x8877019a: // DPERR_CANCELLED
        strcpy(pszOutBuf, "DPERR_CANCELLED");
        break;
    case (int)0x887701a4: // DPERR_ABORTED
        strcpy(pszOutBuf, "DPERR_ABORTED");
        break;
    case (int)0x887703e8: // DPERR_BUFFERTOOLARGE
        strcpy(pszOutBuf, "DPERR_BUFFERTOOLARGE");
        break;
    case (int)0x887703f2: // DPERR_CANTCREATEPROCESS 
        strcpy(pszOutBuf, "DPERR_CANTCREATEPROCESS ");
        break;
    case (int)0x887703fc: // DPERR_APPNOTSTARTED 
        strcpy(pszOutBuf, "DPERR_APPNOTSTARTED ");
        break;
    case (int)0x88770406: // DPERR_INVALIDINTERFACE
        strcpy(pszOutBuf, "DPERR_INVALIDINTERFACE");
        break;
    case (int)0x8877041a: // DPERR_UNKNOWNAPPLICATION 
        strcpy(pszOutBuf, "DPERR_UNKNOWNAPPLICATION ");
        break;
    default:
        wsprintfA(pszOutBuf, "Unknown Error Code - %d", hresult);
        break;
    case (int)0x00000000: // DP_OK
        strcpy(pszOutBuf, "DP_OK");
        break;
    case (int)0x88770816: // DPERR_NOTLOGGEDIN
        strcpy(pszOutBuf, "DPERR_NOTLOGGEDIN");
        break;
    case (int)0x8877042e: // DPERR_NOTLOBBIED
        strcpy(pszOutBuf, "DPERR_NOTLOBBIED");
        break;
    }
}

// FUNCTION: LOCO 0x460360
// DPlay_JoinOrHostSession's join-branch callee. Named session (real strlen(sSessionName)
// check, NOT a first-byte peek -- the original computes a full strlen, matching the
// DPlay_JoinOrHostSession truncate-copy family's own manual strlen shape) -> re-use an
// already-found guidInstance (pGlobalLockedBuf, torn down first unless
// bJoinAttempted/bSkipGlobalFree say to keep it) or a fresh single-target
// EnumSessions (DPlay_JoinSessionEnumCallback); empty session name -> the session-browser
// dialog (resource 0x7d0b, DLGPROC DPlay_JoinSessionDlgProc) -- written as the non-empty-name case's `else`
// (not its own primary `if`) to match the original's own fall-through order: the named-session
// path is the physical fall-through, the dialog path a forward jump, confirmed via
// asmscore --dump before this branch-order fix (score 443225 -> 249576 from this single swap).
// Either way, once guidInstance is known, calls IDirectPlay4's Open method (vtbl+0x60)
// synchronously, retrying on DPERR_CONNECTING the same way DPlay_HostNewSession does. Both
// retry loops bail early to the SAME shared error tail the instant bJoinAttempted becomes
// true mid-wait (uResult left holding the still-DPERR_CONNECTING hrLastResult, masked to
// the same 0x88770100 sentinel the explicit DPERR_NOSESSIONS check below also returns) --
// modeled as a shared `report_error:` label, mirroring DPlay_JoinOrHostSession's own proven
// `goto done;` shared-tail idiom; the final hr==0/!=0 split needed the same real
// `if (hr != 0) {error} else {success}` polarity fix as DPlay_HostNewSession (see its own
// plate comment).
// EFFECTIVE MATCH (asmscore byte_diff 146 at true len 699, insns 222/233 -- branch polarity
// (including the empty-name/named-session swap above), calling convention, and the
// memcpy/strlen error-string shape (see DPlay_HostNewSession's own note on why this isn't a
// real strcat() call) all confirmed correct). Residual: this function's shared `report_error:`
// tail compiles to THREE separate physical epilogue copies in this candidate (one per distinct
// "value of uResult" at each of its 3 `goto` sites -- the 2 retry-loop bailouts reload
// hrLastResult directly since uResult is provably equal to it there, while the normal
// error-format path's copy uses DPlay_ReportNetworkError's already-live return value) rather
// than one shared block -- same "goto-tail sharing is LOCAL/trace-driven, not globally forced"
// class already documented on this file's own DPlay_FindSession (3-vs-1 physical epilogues) and
// DPlay_JoinOrHostSession (5-vs-1); not re-litigated here, same triage-budgeted intrinsic class.
unsigned int GNetManager::DPlay_JoinExistingSession() {
    unsigned int uResult;

    memset(&sessionDesc, 0, sizeof(sessionDesc));
    sessionDesc.dwSize = sizeof(sessionDesc);
    sessionDesc.guidApplication = g_guidLocoApp;
    if (sPassword[0] != 0) {
        sessionDesc.lpszPasswordA = sPassword;
    }

    if (strlen(sSessionName) != 0) {
        if (pDirectPlay4 == 0) {
            return 0;
        }
        if (!bJoinAttempted) {
            if (pGlobalLockedBuf != 0 && !bSkipGlobalFree) {
                HGLOBAL hMem = GlobalHandle(pGlobalLockedBuf);
                GlobalUnlock(hMem);
                hMem = GlobalHandle(pGlobalLockedBuf);
                GlobalFree(hMem);
            }
            bSkipGlobalFree = false;
            pGlobalLockedBuf = 0;
        }
        if (pGlobalLockedBuf == 0) {
            hrLastResult = ((IDirectPlay4VtblProbe *)pDirectPlay4)->EnumSessions(
                &sessionDesc, 0, (void *)&DPlay_JoinSessionEnumCallback, 0, 0x81);
            while (hrLastResult == 0x8877015e) {
                if (bJoinAttempted) {
                    uResult = hrLastResult;
                    goto report_error;
                }
                if (pfnIdlePumpCallback != 0) {
                    pfnIdlePumpCallback();
                }
                Sleep(1);
                hrLastResult = ((IDirectPlay4VtblProbe *)pDirectPlay4)->EnumSessions(
                    &sessionDesc, 0, (void *)&DPlay_JoinSessionEnumCallback, 0, 0x81);
            }
            if (pGlobalLockedBuf == 0) {
                return 0;
            }
        }
    } else {
        if (DialogBoxParamA(0, MAKEINTRESOURCEA(0x7d0b), hWndParent,
                             (DLGPROC)DPlay_JoinSessionDlgProc, 0) == 0) {
            return 0;
        }
    }

    sessionDesc.guidInstance = *(GUID *)pGlobalLockedBuf;
    hrLastResult = ((IDirectPlay4VtblProbe *)pDirectPlay4)->Open(&sessionDesc, 0x81);
    while (hrLastResult == 0x8877015e) {
        if (bJoinAttempted) {
            uResult = hrLastResult;
            goto report_error;
        }
        Sleep(1);
        hrLastResult = ((IDirectPlay4VtblProbe *)pDirectPlay4)->Open(&sessionDesc, 0x81);
    }

    if (hrLastResult != 0) {
        if (hrLastResult == 0x88770118) {
            return 0x88770100;
        }
        char szErrBuf[212];
        char szMsgBuf[300] = "Failed to join session\r\rDirect Play error: ";
        DPlay_FormatHresultString(szErrBuf, hrLastResult);
        memcpy(szMsgBuf + strlen(szMsgBuf), szErrBuf, strlen(szErrBuf));  // idiom-exempt: strlen()-sized, not a magic number (matches the original's own manual strlen+copy expansion)
        uResult = DPlay_ReportNetworkError(0x7d05, szMsgBuf);
        goto report_error;
    }
    bSessionJoined = true;
    return 1;
report_error:
    return uResult & 0xffffff00;
}

// Raw DPSYS system message shapes DPlay_ReceiveAndDispatch reads from lpMem (lpidFrom==0 --
// exact DPSYS_* ids unconfirmed for the opcodes marked "Maybe", this project never includes
// <dplay.h>). All share a leading dword opcode; field offsets pinned via raw disasm.

// Opcode 3 (DPSYS_CREATEPLAYERORGROUP -- the one id here that matches the real Win32 DirectPlay
// enum value): the new player's DPID + name.
struct DPSysCreatePlayerMsg {
    unsigned int type;        // +0x0
    char pad0x4[8 - 4];       // +0x4
    int dpid;                  // +0x8
    char pad0xc[0x24 - 0xc];  // +0xc
    char *pszName;              // +0x24
};

// Opcode 5 (DestroyPlayerOrGroupMaybe): just the leaving player's/group's DPID.
struct DPSysDestroyPlayerMsg {
    unsigned int type;   // +0x0
    char pad0x4[4];        // +0x4
    int dpid;                // +0x8
};

// Opcode 0x103 (ProviderAddrOrJoinNotifyMaybe): DPID + an address/name string, same shape as
// DPSysCreatePlayerMsg but with the name pointer at a different offset.
struct DPSysAddrMsg {
    unsigned int type;      // +0x0
    char pad0x4[4];           // +0x4
    int dpid;                   // +0x8
    char pad0xc[0x18 - 0xc];   // +0xc
    char *pszName;                // +0x18
};

// Opcode 0x104 (SessionNameChangedMaybe): just the new session name string.
struct DPSysSessionNameMsg {
    unsigned int type;        // +0x0
    char pad0x4[0x34 - 4];      // +0x4
    char *pszNewName;              // +0x34
};

// Tag-300 app-envelope opcode 0x32 ("reliable message" cache update): header + an alloc-size/
// copy-length pair (both observed as independently re-read 16-bit fields) + inline payload
// bytes starting at +0xa. unk0x4 is never read by this function.
struct AckPayloadWireMsg {
    unsigned short type;          // +0x0
    unsigned short tag;            // +0x2
    unsigned int unk0x4;             // +0x4
    unsigned short cbAllocSize;        // +0x6
    unsigned short cbCopyLen;            // +0x8
    char data[1];                          // +0xa
};

// Common 4-byte type/tag header -- reused both to peek at an inbound message's opcode/envelope
// tag and to build every output notify node that carries no payload beyond it (opcodes
// 0x31/0x101, the opcode-0x32 "ack processed" reply, and the version-check-failed reply).
struct AppMsgHeader {
    unsigned short type;  // +0x0
    unsigned short tag;     // +0x2
};

// 8-byte notify node: header + a DPID (opcode 5's leave notify).
struct DpidNotifyNode {
    unsigned short type;  // +0x0
    unsigned short tag;     // +0x2
    int dpid;                 // +0x4
};

// 0x88-byte notify node: header + DPID + a truncate-copied name (opcodes 3 and 0x103; SAME
// 0x80-char truncate idiom as DPlay_FindSession's password copy -- a name >=0x80 chars
// overflows by 1 byte into the following padding, unreachable today since these come from a
// peer's own bounded player-name/session-address string, but a real bug in the original).
struct NamedPlayerNotifyNode {
    unsigned short type;   // +0x0
    unsigned short tag;      // +0x2
    int dpid;                  // +0x4
    char szName[0x80];           // +0x8
};

// 0x404-byte notify node: header + the full new session name (opcode 0x104), NOT truncated
// (0x400-byte buffer -- comfortably larger than sSessionName's own 200-byte field, which gets
// updated from the SAME source string with no cap at all, matching sSessionName's other
// established no-cap copies).
struct SessionNameNotifyNode {
    unsigned short type;   // +0x0
    unsigned short tag;      // +0x2
    char szName[0x400];        // +0x4
};

// FUNCTION: LOCO 0x4606d0
// Receives one DirectPlay message (growing the scratch buffer on DPERR_BUFFERTOOSMALL) and
// translates it into Loco's own small heap-allocated notify-node shapes, wrapped in a
// DPlayRecvMsg the caller (GameNet_DispatchMessage) frees via HeapFree. Two message families:
// raw DPSYS_* system messages (Receive's own lpidFrom out-param comes back 0) get translated
// into ad hoc notify nodes below; Loco's own tag-300 app envelope messages (real lpidFrom) are
// either the special opcode-0x32 reliable-message-ack (cache the payload, reply with a small
// ack-processed node) or handed to the caller unmodified (ownership of the raw receive buffer
// transfers to the returned wrapper, NOT freed here).
//
// lpidTo's out-param storage is &dpidLocalPlayer itself (confirmed via raw disasm) -- every
// receive OVERWRITES this->dpidLocalPlayer with whatever player the message was addressed to,
// not just our own id. Since nearly all traffic targets us directly this is usually a no-op,
// but the field is not purely "our own player id" the way its name suggests -- sic, not modeled
// differently since no known consumer relies on it staying stable across a receive.
//
// Opcode 0x101 (HostMigrationMaybe) carries NO payload dword despite a prior session's
// scoping guess -- raw disasm confirms it only flips bIsHost=1 and clears nMaxPlayers/
// bJoinAttempted/bAllowHostMigration (all from the SAME already-zero register used for the
// notify node's own alloc flags, a plain register-reuse coincidence, not a payload copy).
// Opcode 0x104 (SessionNameChangedMaybe) does TWO copies, not one: the new name into the
// notify node's own payload AND into this->sSessionName itself, keeping our cached session
// name in sync with the peer that changed it.
//
// PARTIAL (transcribed 2026-07-20, byte-hunting pass same day dropped asmscore total
// 1289134->793556, DIFF(1283)->DIFF(1098) -- large multi-opcode dispatcher, see
// docs/subsystems.md). Two real fixes landed:
// (1) the top-level `if (*pDpidFrom == 0)` branch polarity was backwards -- raw disasm
//     shows the switch (pDpidFrom==0 case) is the OUT-OF-LINE/jumped-to block while the
//     pDpidFrom!=0 body (tag-check/ack/generic) is the fall-through, i.e. source must read
//     `if (*pDpidFrom != 0) {...} else { switch(...){...} }`, not the other way around.
// (2) cases 3 and 0x103 (NamedPlayerNotifyNode) zero `pNode->szName[0] = 0;`
//     UNCONDITIONALLY before the length check/strcpy -- confirmed via raw disasm
//     (`mov BYTE PTR [edx+8],0` executed on both the <0x80 and >=0x80 length paths).
// Remaining residual (asmscore.py --len 1648: total 733520, align=728/byte_diff=460, insns
// 605/603) is PURELY block-order: original emits [switch][switch-tail][ack-check+inline ack
// body][far hrLastResult!=0 error handler][generic-message handler][switch's own
// pBuiltNode!=0 wrapper-return], this candidate emits the SAME set of blocks with just the
// ack-check and hrLastResult-handler SWAPPED: [switch][switch-tail][hrLastResult
// handler][ack-check][generic-message][wrapper-return] -- confirmed via a full linear
// capstone disasm of the compiled COMDAT (not just asmscore.py --dump's realigned view);
// every block's own internal content is byte-identical, only the physical ORDER of these
// two blocks differs. PARKED 2026-07-20 (v293) after 3 restructurings this session, none
// net-positive -- see docs/PARKED.md for the full writeup: (1) nesting the whole
// pDpidFrom/switch/ack/generic body inside `if (hrLastResult == 0) {...} else {handler}`
// DID move the handler block, but as a side effect tail-merged the unrelated top-of-function
// `pDirectPlay4==0` early return into a shared block, regressing a previously byte-EXACT
// region -- rejected. (2) a `goto recv_error;` to an end-of-loop label: byte-IDENTICAL to
// the structured-if baseline, zero effect. (3) an `if/else-if/else` 3-way chain (same
// textual position as the structured-if baseline, no relocation): also byte-identical, zero
// effect. This is the Yoda "block layout is trace-driven and mostly NOT source-steerable"
// class (#15) at a scale (6+ cold blocks, the most of any parked residual in this codebase)
// that makes hand-theorizing MSVC's cold-block worklist order impractical without a
// decompiler-driven trace -- do not re-grind without a genuinely new lever (see
// docs/PARKED.md's retry idea: reordering the switch's own case-label order, untested).
DPlayRecvMsg *GNetManager::DPlay_ReceiveAndDispatch() {
    if (pDirectPlay4 == 0) {
        return 0;
    }

    char *szMsgBuf = (char *)HeapAlloc(GetProcessHeap(), 0, 0x7ff);
    char *szErrBuf = (char *)HeapAlloc(GetProcessHeap(), 0, 0x400);
    int *pDpidTo = &dpidLocalPlayer;

    for (;;) {
        unsigned int dwDataSize = 0x7ff;
        int *pDpidFrom = (int *)HeapAlloc(GetProcessHeap(), 0, sizeof(int));
        void *lpMem = HeapAlloc(GetProcessHeap(), 0, dwDataSize);
        hrLastResult = ((IDirectPlay4VtblProbe *)pDirectPlay4)
                           ->Receive(pDpidFrom, pDpidTo, 1, lpMem, &dwDataSize);
        while (hrLastResult == 0x8877001e) {  // DPERR_BUFFERTOOSMALL
            HeapFree(GetProcessHeap(), 0, lpMem);
            dwDataSize += 0x7ff;
            lpMem = HeapAlloc(GetProcessHeap(), 0, dwDataSize);
            hrLastResult = ((IDirectPlay4VtblProbe *)pDirectPlay4)
                               ->Receive(pDpidFrom, pDpidTo, 1, lpMem, &dwDataSize);
        }

        if (hrLastResult != 0) {
            HeapFree(GetProcessHeap(), 0, lpMem);
            HeapFree(GetProcessHeap(), 0, pDpidFrom);
            if (hrLastResult == 0x887700be) {  // DPERR_NOMESSAGES -- nothing more to receive
                HeapFree(GetProcessHeap(), 0, szMsgBuf);
                HeapFree(GetProcessHeap(), 0, szErrBuf);
                return 0;
            }
            DPlay_FormatHresultString(szErrBuf, hrLastResult);
            wsprintfA(szMsgBuf, "Network Receive failed (GetNetMessage)\r\rDirect Play code: %s",
                      szErrBuf);
            DPlay_ReportNetworkError(0, szMsgBuf);
            HeapFree(GetProcessHeap(), 0, szMsgBuf);
            HeapFree(GetProcessHeap(), 0, szErrBuf);
            return 0;
        }

        if (*pDpidFrom != 0) {
            if (((AppMsgHeader *)lpMem)->tag != 300) {
                DPlay_ReportNetworkError(0, "Version check failed on Network Message");
                if (pDpidFrom != 0 && pDirectPlay4 != 0) {
                    AppMsgHeader reply;
                    reply.type = 0x1e;
                    reply.tag = 300;
                    DPlay_SendMessage(*pDpidFrom, &reply, sizeof(reply), 1);
                }
                HeapFree(GetProcessHeap(), 0, lpMem);
                HeapFree(GetProcessHeap(), 0, pDpidFrom);
                continue;
            }
        } else {
            // Raw DPSYS system message (no genuine sender).
            void *pBuiltNode = 0;
            switch (*(unsigned int *)lpMem) {
            case 3: {
                DPSysCreatePlayerMsg *pMsg = (DPSysCreatePlayerMsg *)lpMem;
                NamedPlayerNotifyNode *pNode = (NamedPlayerNotifyNode *)HeapAlloc(
                    GetProcessHeap(), 0, sizeof(NamedPlayerNotifyNode));
                pNode->type = 0x46;
                pNode->tag = 300;
                pNode->dpid = pMsg->dpid;
                pNode->szName[0] = 0;
                if (strlen(pMsg->pszName) < 0x80) {
                    strcpy(pNode->szName, pMsg->pszName);
                } else {
                    char cSaved = pMsg->pszName[0x80];
                    pMsg->pszName[0x80] = 0;
                    strcpy(pNode->szName, pMsg->pszName);
                    pMsg->pszName[0x80] = cSaved;
                }
                pBuiltNode = pNode;
                break;
            }
            case 5: {
                DPSysDestroyPlayerMsg *pMsg = (DPSysDestroyPlayerMsg *)lpMem;
                DpidNotifyNode *pNode =
                    (DpidNotifyNode *)HeapAlloc(GetProcessHeap(), 0, sizeof(DpidNotifyNode));
                pNode->type = 0x14;
                pNode->tag = 300;
                pNode->dpid = pMsg->dpid;
                pBuiltNode = pNode;
                break;
            }
            case 0x31: {
                DPlay_TeardownConnection();
                AppMsgHeader *pNode =
                    (AppMsgHeader *)HeapAlloc(GetProcessHeap(), 0, sizeof(AppMsgHeader));
                pNode->type = 0xa;
                pNode->tag = 300;
                pBuiltNode = pNode;
                break;
            }
            case 0x101: {
                bIsHost = 1;
                nMaxPlayers = 0;
                bJoinAttempted = 0;
                bAllowHostMigration = 0;
                AppMsgHeader *pNode =
                    (AppMsgHeader *)HeapAlloc(GetProcessHeap(), 0, sizeof(AppMsgHeader));
                pNode->type = 0x3c;
                pNode->tag = 300;
                pBuiltNode = pNode;
                break;
            }
            case 0x103: {
                DPSysAddrMsg *pMsg = (DPSysAddrMsg *)lpMem;
                NamedPlayerNotifyNode *pNode = (NamedPlayerNotifyNode *)HeapAlloc(
                    GetProcessHeap(), 0, sizeof(NamedPlayerNotifyNode));
                pNode->type = 0x50;
                pNode->tag = 300;
                pNode->dpid = pMsg->dpid;
                pNode->szName[0] = 0;
                if (strlen(pMsg->pszName) < 0x80) {
                    strcpy(pNode->szName, pMsg->pszName);
                } else {
                    char cSaved = pMsg->pszName[0x80];
                    pMsg->pszName[0x80] = 0;
                    strcpy(pNode->szName, pMsg->pszName);
                    pMsg->pszName[0x80] = cSaved;
                }
                pBuiltNode = pNode;
                break;
            }
            case 0x104: {
                DPSysSessionNameMsg *pMsg = (DPSysSessionNameMsg *)lpMem;
                if (strcmp(sSessionName, pMsg->pszNewName) != 0) {
                    SessionNameNotifyNode *pNode = (SessionNameNotifyNode *)HeapAlloc(
                        GetProcessHeap(), 0, sizeof(SessionNameNotifyNode));
                    pNode->type = 0x5a;
                    pNode->tag = 300;
                    strcpy(pNode->szName, pMsg->pszNewName);
                    strcpy(sSessionName, pMsg->pszNewName);
                    pBuiltNode = pNode;
                }
                break;
            }
            }

            if (pBuiltNode != 0) {
                DPlayRecvMsg *pWrapper =
                    (DPlayRecvMsg *)HeapAlloc(GetProcessHeap(), 0, sizeof(DPlayRecvMsg));
                pWrapper->fromPlayerId = 0;
                pWrapper->pPacket = (unsigned short *)pBuiltNode;
                if (lpMem != 0) {
                    HeapFree(GetProcessHeap(), 0, lpMem);
                }
                HeapFree(GetProcessHeap(), 0, pDpidFrom);
                HeapFree(GetProcessHeap(), 0, szMsgBuf);
                HeapFree(GetProcessHeap(), 0, szErrBuf);
                return pWrapper;
            }
            if (lpMem != 0) {
                HeapFree(GetProcessHeap(), 0, lpMem);
            }
            HeapFree(GetProcessHeap(), 0, pDpidFrom);
            continue;
        }

        // Reached only when *pDpidFrom != 0 && tag == 300 (else-branch above always
        // continues; the tag != 300 guard above always continues too).
        if (((AppMsgHeader *)lpMem)->type == 0x32) {  // reliable-message ack: cache payload
            AckPayloadWireMsg *pAck = (AckPayloadWireMsg *)lpMem;
            if (pDataCacheMaybe != 0) {
                operator delete(pDataCacheMaybe);
                cbDataCacheMaybe = 0;
            }
            pDataCacheMaybe = operator new(pAck->cbAllocSize);
            memcpy(pDataCacheMaybe, pAck->data, pAck->cbCopyLen);  // idiom-exempt: cbCopyLen is a wire-provided runtime length, not a magic number
            cbDataCacheMaybe = pAck->cbAllocSize;

            AppMsgHeader *pAckNode =
                (AppMsgHeader *)HeapAlloc(GetProcessHeap(), 0, sizeof(AppMsgHeader));
            pAckNode->type = 0x28;
            pAckNode->tag = 300;

            DPlayRecvMsg *pWrapper =
                (DPlayRecvMsg *)HeapAlloc(GetProcessHeap(), 0, sizeof(DPlayRecvMsg));
            pWrapper->fromPlayerId = 0;
            pWrapper->pPacket = (unsigned short *)pAckNode;
            HeapFree(GetProcessHeap(), 0, lpMem);
            HeapFree(GetProcessHeap(), 0, pDpidFrom);
            HeapFree(GetProcessHeap(), 0, szMsgBuf);
            HeapFree(GetProcessHeap(), 0, szErrBuf);
            return pWrapper;
        }

        // Generic app message: hand the raw receive buffer to the caller as-is (ownership
        // transfers -- NOT freed here).
        DPlayRecvMsg *pWrapper =
            (DPlayRecvMsg *)HeapAlloc(GetProcessHeap(), 0, sizeof(DPlayRecvMsg));
        pWrapper->fromPlayerId = *pDpidFrom;
        pWrapper->pPacket = (unsigned short *)lpMem;
        HeapFree(GetProcessHeap(), 0, pDpidFrom);
        HeapFree(GetProcessHeap(), 0, szMsgBuf);
        HeapFree(GetProcessHeap(), 0, szErrBuf);
        return pWrapper;
    }
}

// FUNCTION: LOCO 0x460d40
// Stamps the app envelope tag (300) into the caller's buffer, then sends it via DirectPlay.
// When the send is unreliable (bit 0 of dwFlags clear) and nSendThrottleQueueDepth is nonzero,
// first checks the outbound send-queue depth (IDirectPlay4::GetMessageQueue, DPGETMSGQUEUE_SEND)
// and refuses to send (reporting "Message Throttled by WigNet") if it exceeds the threshold.
// sessionDescUpdate.dwFlags (this+0x15e8, an already-modeled field -- see GNetManager.h) gates
// Send vs SendEx; confirmed via a whole-binary xref sweep this is the ONLY reference to that
// field anywhere (no writer exists), so dwFlags is provably always 0 post-construction and the
// SendEx path is DEAD CODE in the shipped binary -- modeled faithfully anyway (// sic), see
// docs/engine-bugs.md. On a real failure HRESULT (negative, excluding E_PENDING/0x8000000a,
// which is treated as a benign in-flight result) formats and reports the error and returns 0;
// otherwise (including the not-yet-connected early-out and the throttle rejection) returns 0 on
// every OTHER failure path too, and 1 on success. CORRECTED 2026-07-20: a prior session's own
// GNetManager.h decl comment and the 0x43ae20 plate comment both had this return-value polarity
// BACKWARDS ("returns 0 on success") -- ground-truthed via raw disasm: the success return reads
// a stack local set to the constant 1 in the prologue and never touched again, while EVERY
// failure path (not connected, throttled, real error HRESULT) explicitly `xor eax,eax`s to 0.
//
// PARTIAL (asmscore.py --len 330: total 165257, align=162 reg_pen=28 byte_diff=157, insns
// 105/109). Two real structural fixes landed: (1) `dwNumMsgs`/`nResult` must be declared at
// the very TOP of the function, before the `pDirectPlay4==0` early-return -- raw disasm shows
// both stack slots ([esp+0x10]/[esp+0x14]) are written unconditionally in the true prologue,
// even on the early-failure path that never reads them. (2) the Send-vs-SendEx `if` must test
// `sessionDescUpdate.dwFlags != 0` (SendEx first) not `== 0` (Send first) -- the original's
// physical block order is SendEx-inline/Send-at-jump-target, the opposite of a naive top-down
// reading of the source condition. Residual is mostly the intrinsic register-role-swap class
// (Yoda #7/#29/#30 -- `r` marks throughout the dump, this/dpidLocalPlayer/etc. picked
// different scratch registers) plus TWO stack-slot residuals not yet explained: even after
// hoisting, my compile still constant-folds `nResult`'s `mov eax,1` at the return (the
// original loads it from the stack slot instead) and drops the defensive `dwNumMsgs=0` init
// entirely (proven dead since the only read is right after the address-taking call) -- neither
// reproduces via declaration-order swaps (tried both). Likely intrinsic register-pressure tie-
// breaks from the two heavy 9-arg/6-arg vtable calls; not re-probed further this session.
int GNetManager::DPlay_SendMessage(int dpid, void *buf, int len, int dwFlags) {
    unsigned int dwNumMsgs = 0;
    int nResult = 1;
    if (pDirectPlay4 == 0) {
        return 0;
    }

    ((AppMsgHeader *)buf)->tag = 300;

    if (((dwFlags & 1) == 0) && (nSendThrottleQueueDepth != 0)) {
        ((IDirectPlay4VtblProbe *)pDirectPlay4)->GetMessageQueue(0, 0, 1, &dwNumMsgs, 0);
        if (dwNumMsgs > nSendThrottleQueueDepth) {
            hrLastResult = 0x8877010e;
            DPlay_ReportNetworkError(0, "Message Throttled by WigNet");
            return 0;
        }
    }

    int hr;
    if ((sessionDescUpdate.dwFlags & 0x10000) != 0) {
        // sic: dwFlags is provably always 0 (no writer anywhere in the binary) -- this branch
        // is dead code, kept faithful to the original's shape.
        hr = ((IDirectPlay4VtblProbe *)pDirectPlay4)
                 ->SendEx(dpidLocalPlayer, dpid, dwFlags | 0x600, buf, len, 0, 0, 0, 0);
    } else {
        hr = ((IDirectPlay4VtblProbe *)pDirectPlay4)
                 ->Send(dpidLocalPlayer, dpid, dwFlags, buf, len);
    }
    hrLastResult = hr;

    if ((hr < 0) && (hr != (int)0x8000000a)) {  // E_PENDING is not a real failure
        char szErrBuf[212];
        DPlay_FormatHresultString(szErrBuf, hr);
        DPlay_ReportNetworkError(0x7d03, szErrBuf);
        return 0;
    }

    return nResult;
}

// Wire payload for the per-tick LOCAL train-step broadcast built by TrainNet_AdvanceLocalTrainSteps
// (also opcode 0x3f6): the same small header + one packed RosterTickRecord as RosterTickWireMsg,
// plus one trailing byte that is never written. The 0x12-byte heap copy posted to the local queue
// moves 18 bytes (4 dwords + 1 word in the original), so the copied struct is genuinely 0x12 --
// one past the record's last byte; the type-0x15 consumer (HandleQueuedPlacementEvent case 0x15)
// reads it as a plain RosterTickWireMsg and never touches the tail. Byte-packed so the record
// sits unaligned at +0x9, same as the sibling.
#pragma pack(push, 1)
struct TrainStepWireMsgMaybe {
    unsigned short wOpcode;        // +0x0  -- 0x3f6
    unsigned char pad0x2[2];       // +0x2  -- left unset
    unsigned char bProviderIndex;  // +0x4  -- the train's bOwnerByteB slot key
    unsigned char pad0x5;          // +0x5  -- left unset
    unsigned short wCount;         // +0x6  -- always 1 (a single record per tick)
    unsigned char bConst0;         // +0x8  -- 0 here (RosterTickWireMsg's sibling producer writes 1)
    RosterTickRecord record;       // +0x9  -- the one per-tick position record
    unsigned char pad0x11;         // +0x11 -- trailing byte, never written (ships heap garbage) // sic
};
#pragma pack(pop)

// FUNCTION: LOCO 0x43bb00
// Per-tick LOCAL train movement stepper, driven by GameNetThread_TickLoop every
// nTrainAdvanceInterval ticks (headings 0/0x5a/0xb4/0x10e = 0/90/180/270deg). Walks the
// pTrainListRehomed chain; for each train whose owner provider slot has layout data but no
// provider (providerId == 0), first gives the board-edge hand-off helper a chance to move the
// train to a neighboring board (a successful hand-off ends the whole tick), then follows the
// track grid (pLayoutData bytes == 5 mark track) one tile in the train's wLocalHeading, turning
// at junctions and reversing off dead ends. A train that cannot advance in any direction gets a
// random new heading and resets its stall checkpoint. A train that moved stores its new tile,
// bumps bStallStepCounter, and posts the new position twice: as a type-0x15 local-queue notify
// (an 0x12-byte heap copy of the wire message, consumed by HandleQueuedPlacementEvent case 0x15)
// and as an unreliable opcode-0x3f6 send-queue broadcast. When the randomized stall threshold
// (rand()/0x1999+3) fires, the current tile is compared against the last checkpoint; no real
// progress means another random reroute. Trains whose slot check fails are unlinked in place,
// get their NETWORK wHeading reflected, are appended to the pTrainListAwaitingAck tail, and that
// list is drained as type-0x11 notifies -- with the same one-node-per-call dequeue quirk as
// GameNet_SendTrainStateSync's retry path (pNext zeroed before the advance read; // sic).
//
// EFFECTIVE MATCH (cc.sh DIFF 641/1636, down from 1484 at v359). The 4-insn count gap was the
// "dead signed-index guard" on `&aProviderSlots[bOwnerByteB]` (the original keeps
// `jl -> (slot=0)` + an `xor eax,eax` block + a `jmp`). v360 established that guard is NOT
// intrinsic: it is the body of the implicitly inline DPlaySessionMgr::ProviderSlotAt accessor
// (see DPlaySessionMgr.h), and routing this site through it restores both the guard and the
// materialized slot base -- the fused `[eax+edx*4+0x55c]` pLayoutData load was downstream of the
// same missing guard, exactly as predicted here. Remaining residual is register tie-breaks only,
// all inside the heading
// switch's 4 case bodies: (a) a symmetric eax/ecx role swap between the grid pointer and the
// stride accumulator (original: stride in ecx, grid chain in eax; ours: swapped -- Yoda #29/#30);
// (b) the spill-victim choice in cases 0x5a/0x10e (original spills idxUp to [esp+0x18] and uses
// edx for the wLayoutRows movsx compares; ours keeps idxUp in edx and spills the second idx to
// [esp+0x1c], using esi for those compares). Probed without movement: nCols/nY/nX declaration
// orders (3 variants -- the final one fixed cols=esi/y=edi), pGrid hoisted vs per-case (per-case
// required for the original's per-case `mov eax,[esp+0x10]; mov eax,[eax+0x44]` reloads),
// idx-first vs pGrid-first per case, an explicit switch-pivot local (byte-identical output).
// Everything outside the switch (prologue, handoff unlink/reflect/append, the type-0x11 drain,
// stall/reroute blocks, wire-message build + HeapAlloc copy + both enqueues) is byte-aligned
// modulo the register names riding the same case-body allocation.
void GameNetThreadState::TrainNet_AdvanceLocalTrainSteps() {
    PeerTrainNodePartial *pPrev = 0;
    PeerTrainNodePartial *pTrain = this->pTrainListRehomed;
    while (pTrain != 0) {
        DPlaySessionMgrProviderSlot *pSlot =
            g_pDPlaySessionMgr->ProviderSlotAt(pTrain->bOwnerByteB);
        if (pSlot->pLayoutData != 0 && pSlot->providerId == 0) {
            int nY = pTrain->wPosY;
            int nCols = pSlot->wLayoutCols;
            int nX = pTrain->wPosX;
            if (this->TrainNet_TryBoardEdgeHandoffMaybe(pPrev, pTrain, nX, nY, nCols,
                                                        pSlot->wLayoutRows) != 0) {
                return;
            }
            switch (pTrain->wLocalHeading) {
            case 0: {
                int idx = nCols * (nY - 1) + nX;
                char *pGrid = (char *)pSlot->pLayoutData;
                if (pGrid[idx] == 5) {
                    nY--;
                } else if (pGrid[idx + 1] == 5 && nX < pSlot->wLayoutCols) {
                    nY--;
                    nX++;
                } else if (pGrid[idx - 1] == 5 && nX > 0) {
                    nX--;
                    nY--;
                } else {
                    idx = nCols * nY + nX;
                    if (pGrid[idx - 1] == 5 && nX > 0) {
                        nX--;
                    } else if (pGrid[idx + 1] == 5 && nX < pSlot->wLayoutCols) {
                        nX++;
                        pTrain->wLocalHeading = 0x5a;
                    }
                }
                break;
            }
            case 0x5a: {
                int idx = nCols * nY + nX;
                char *pGrid = (char *)pSlot->pLayoutData;
                if (pGrid[idx + 1] == 5) {
                    nX++;
                } else {
                    int idxUp = nCols * (nY - 1) + nX;
                    if (pGrid[idxUp + 1] == 5 && nY > 0) {
                        nX++;
                        nY--;
                    } else {
                        idx = nCols * (nY + 1) + nX;
                        if (pGrid[idx + 1] == 5 && nY < pSlot->wLayoutRows) {
                            nX++;
                            nY++;
                        } else if (pGrid[idxUp] == 5 && nY > 0) {
                            nY--;
                        } else if (pGrid[idx] == 5 && nY < pSlot->wLayoutRows) {
                            nY++;
                            pTrain->wLocalHeading = 0xb4;
                        }
                    }
                }
                break;
            }
            case 0xb4: {
                int idx = nCols * (nY + 1) + nX;
                char *pGrid = (char *)pSlot->pLayoutData;
                if (pGrid[idx] == 5) {
                    nY++;
                } else if (pGrid[idx + 1] == 5 && nX < pSlot->wLayoutCols) {
                    nY++;
                    nX++;
                } else if (pGrid[idx - 1] == 5 && nX > 0) {
                    nX--;
                    nY++;
                } else {
                    idx = nCols * nY + nX;
                    if (pGrid[idx - 1] == 5 && nX > 0) {
                        nX--;
                    } else if (pGrid[idx + 1] == 5 && nX < pSlot->wLayoutCols) {
                        nX++;
                        pTrain->wLocalHeading = 0x10e;
                    }
                }
                break;
            }
            case 0x10e: {
                int idx = nCols * nY + nX;
                char *pGrid = (char *)pSlot->pLayoutData;
                if (pGrid[idx - 1] == 5) {
                    nX--;
                } else {
                    int idxUp = nCols * (nY - 1) + nX;
                    if (pGrid[idxUp - 1] == 5 && nY > 0) {
                        nX--;
                        nY--;
                    } else {
                        idx = nCols * (nY + 1) + nX;
                        if (pGrid[idx - 1] == 5 && nY < pSlot->wLayoutRows) {
                            nX--;
                            nY++;
                        } else if (pGrid[idxUp] == 5 && nY > 0) {
                            nY--;
                        } else if (pGrid[idx] == 5 && nY < pSlot->wLayoutRows) {
                            nY++;
                            pTrain->wLocalHeading = 0;
                        }
                    }
                }
                break;
            }
            }
            if (nX == pTrain->wPosX && nY == pTrain->wPosY) {
                // No track exit in any direction: pick a random new heading, reset the checkpoint.
                switch (rand() / 0x1fff) {
                case 0:  pTrain->wLocalHeading = 0x5a; break;
                case 1:  pTrain->wLocalHeading = 0x10e; break;
                case 2:  pTrain->wLocalHeading = 0; break;
                default: pTrain->wLocalHeading = 0xb4; break;
                }
                pTrain->wCheckpointPosX = -1;
                pTrain->wCheckpointPosY = -1;
                pTrain->bStallStepCounter = 0;
            } else {
                pTrain->wPosX = (short)nX;
                pTrain->wPosY = (short)nY;
                pTrain->bStallStepCounter = pTrain->bStallStepCounter + 1;
                if (pTrain->bStallStepCounter > rand() / 0x1999 + 3) {
                    if (pTrain->wPosX == pTrain->wCheckpointPosX &&
                        pTrain->wPosY == pTrain->wCheckpointPosY) {
                        // No real progress since the last checkpoint: random reroute.
                        switch (rand() / 0x1fff) {
                        case 0:  pTrain->wLocalHeading = 0x5a; break;
                        case 1:  pTrain->wLocalHeading = 0x10e; break;
                        case 2:  pTrain->wLocalHeading = 0; break;
                        default: pTrain->wLocalHeading = 0xb4; break;
                        }
                        pTrain->bStallStepCounter = 0;
                        pTrain->wCheckpointPosX = -1;
                        pTrain->wCheckpointPosY = -1;
                    }
                    pTrain->bStallStepCounter = 0;
                    // sic: the original copies the wPosX/wPosY pair onto the checkpoint pair as
                    // ONE dword (`mov ecx,[ebx+0x7e]; mov [ebx+0x84],ecx`) -- a genuine dword
                    // alias of the two short pairs, but modeling it as a union here regressed
                    // four sibling functions in this TU (struct-layout desync; reverted).
                    pTrain->wCheckpointPosX = pTrain->wPosX;
                    pTrain->wCheckpointPosY = pTrain->wPosY;
                }
                TrainStepWireMsgMaybe *pMsg =
                    (TrainStepWireMsgMaybe *)::operator new(0x2000);
                RosterTickRecord rec;
                rec.wPosX = (unsigned short)nX;
                rec.wPosY = (unsigned short)nY;
                pMsg->wOpcode = 0x3f6;
                pMsg->bProviderIndex = pTrain->bOwnerByteB;
                pMsg->bConst0 = 0;
                pMsg->wCount = 1;
                rec.wTrainId = pTrain->wTrainId;
                rec.bOwnerA = pTrain->bOwnerByteA;
                rec.bSlotKey = pTrain->bOwnerByteB;
                pMsg->record = rec;
                TrainStepWireMsgMaybe *pCopy = (TrainStepWireMsgMaybe *)
                    HeapAlloc(GetProcessHeap(), 0, sizeof(TrainStepWireMsgMaybe));
                *pCopy = *pMsg;
                NetMsgQueueNode *pNotify = new NetMsgQueueNode();
                pNotify->type = 0x15;
                pNotify->pPayload = pCopy;
                g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNotify);
                NetMsgQueueNode *pSend = new NetMsgQueueNode();
                pSend->type = 6;
                pSend->payloadLen = sizeof(TrainStepWireMsgMaybe);
                pSend->pPayload = pMsg;
                pSend->destPlayerId = 0;
                pSend->bReliable = 0;
                g_pGameNetThreadState->EnqueueOrFreeNode(pSend);
            }
            pPrev = pTrain;
            pTrain = (PeerTrainNodePartial *)pTrain->pNext;
        } else {
            // No layout data (or a live provider) on the owner slot: hand the train off.
            PeerTrainNodePartial *pNode = pTrain;
            if (pPrev != 0) {
                pPrev->pNext = pNode->pNext;
            } else {
                this->pTrainListRehomed = (PeerTrainNodePartial *)pNode->pNext;
            }
            pTrain = (PeerTrainNodePartial *)pNode->pNext;
            pNode->pNext = 0;

            int wHead = pNode->wHeading;
            pNode->wHeading = wHead;  // sic: redundant re-store (the original emits it)
            switch (wHead) {
            case 0:     wHead = 0xb4; break;
            case 0x5a:  wHead = 0x10e; break;
            case 0xb4:  wHead = 0; break;
            case 0x10e: wHead = 0x5a; break;
            }
            pNode->wHeading = wHead;

            if (this->pTrainListAwaitingAck != 0) {
                PeerTrainNodePartial *pTail = this->pTrainListAwaitingAck;
                while (pTail->pNext != 0)
                    pTail = (PeerTrainNodePartial *)pTail->pNext;
                pNode->pNext = 0;
                pTail->pNext = pNode;
            } else {
                pNode->pNext = 0;
                this->pTrainListAwaitingAck = pNode;
            }

            while (this->pTrainListAwaitingAck != 0) {
                NetMsgQueueNode *pNode2 = new NetMsgQueueNode();
                pNode2->type = 0x11;
                pNode2->pPayload = this->pTrainListAwaitingAck;
                this->pTrainListAwaitingAck->bHasDetailFlagMaybe = 0;
                PeerTrainNodePartial *pHead = this->pTrainListAwaitingAck;
                unsigned int nSelfSlot = g_pDPlaySessionMgr->selectedProviderIndex;
                pHead->bOwnerByteB = (unsigned char)nSelfSlot;
                pHead->pNext = 0;
                pHead->bHasDetailFlagMaybe = 0;  // sic: redundant re-clear
                // sic: pNext was just zeroed above, so this always dequeues to NULL -- the drain
                // only ever processes the list head once per call, regardless of how many trains
                // are actually queued (see docs/subsystems.md).
                this->pTrainListAwaitingAck =
                    (PeerTrainNodePartial *)this->pTrainListAwaitingAck->pNext;
                g_pDPlaySessionMgr->GameNetMsgQueue_EnqueueOrProcessLocalNode(pNode2);
            }
        }
    }
}
