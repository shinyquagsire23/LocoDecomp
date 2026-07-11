// Phase 4: the DirectSound manager singleton -- device init, the fixed channel
// pool, and the channel-selection/steal dispatcher.
//
// Address-contiguous, confirmed TU boundaries on both sides (0x412bd0-0x41365a):
// preceded by the unrelated KeyedList_RemoveByKeyMaybe (ends 0x412bcc), followed
// immediately by Wav_ParseAndLoad at 0x413660 with no gap (the natural next TU --
// SoundBankEntry::EnsureLoaded/the WAV loader). 1 class (DSound), 13 real methods
// + ctor + an auto-generated scalar deleting destructor byproduct, plus 1 free
// enumeration callback. See docs/subsystems.md's DirectSound section.

#include "DSound.h"
#include "WorldBoardMaybe.h"
#include "UIResources.h"
#include "AppWindow.h"
#include "IniFile.h"       // IniFile, g_pIniFile

#include <string.h>

// Cross-TU dependency (owned by another, not-yet-tackled TU) -- declared with an arbitrary
// name since verify.py's byte comparison masks relocations; only the calling convention at
// each call site is real, matchable code. Same pattern as src/DSoundChannel.cpp.
extern "C" {  // TODO: idiom
    extern unsigned int g_dwGameTick;   // DAT_004a99b4
}

// The channel-pool array is built/torn down via a hand-rolled "array with
// 4-byte count header" idiom (docs/subsystems.md), NOT compiler new[]/delete[]
// machinery -- confirmed this session by directly disassembling both call
// sites: each explicitly pushes the per-element ctor/dtor FUNCTION POINTER as
// a plain data argument to a shared generic helper (FUN_004671e0 for
// construction, FUN_00467280 for destruction), which is how a hand-written
// (not compiler-synthesized) generic array utility looks, not an MSVC
// vector-construct/destruct iterator (those don't take the ctor/dtor as an
// explicit pushed argument in this shape). The per-element ctor really is
// DSoundChannel::DSoundChannel() (0x40ec30, now modeled + byte-matched in
// src/DSoundChannel.cpp -- its body duplicates Init()'s field-init logic,
// confirmed by the ex-LAB_0040ec30 stub) -- C++ doesn't allow taking a
// constructor's address directly, so the call site below still needs an
// opaque extern "C" stand-in symbol for it (verify.py masks relocations, so
// the stand-in's real identity doesn't affect the byte match). The
// destructor slot reuses DSoundChannel::Release()'s own address directly
// (0x40eca0) -- tried modeling a real `~DSoundChannel() { Release(); }` this
// session; it compiles to a distinct 5-byte `jmp Release` tail-call thunk at
// its OWN address, not literally Release()'s address, so there is no real
// separate destructor here: the original source passes `&DSoundChannel::
// Release` (or equivalent) directly as the generic helper's callback
// argument, confirming Teardown's already-documented double-release
// (once via a plain loop, once via the generic array-destruct helper) is
// real, not a modeling bug. See docs/PARKED.md.
typedef void (DSoundChannel::*DSoundChannelMethodMaybe)();
// ArrayConstructWithIteratorMaybe intentionally has real C++ linkage, NOT
// extern "C" -- confirmed via an isolated probe that /GX's automatic-unwind
// scaffolding (the fs:0 SEH-frame prologue seen in the real binary) is only
// emitted around a call the compiler considers capable of throwing a C++
// exception; a call to an extern "C" function is treated as non-throwing and
// suppresses the scaffolding entirely, even with an otherwise-identical
// class-typed guard local in scope. See docs/PARKED.md.
void *__stdcall ArrayConstructWithIteratorMaybe(void *pArray, unsigned int elemSize, unsigned int count, void *pCtorThunk, DSoundChannelMethodMaybe pDtorThunk);
extern "C" {  // TODO: idiom
    void *__stdcall ArrayDestructWithIteratorMaybe(void *pArray, unsigned int elemSize, unsigned int count, DSoundChannelMethodMaybe pDtorThunk);
    void DSoundChannel_ConstructThunkMaybe(DSoundChannel *pChannel);
}

extern "C" DSound *g_pDSoundManager;  // TODO: idiom

// RAII unwind guard confirmed via the FuncInfo table at 0x47a900 (maxState=1,
// nTryBlocks=0 -- automatic-unwind, not manual try/catch): frees the channel-pool
// header block if an exception unwinds through construction, before ownership
// transfers to pChannels. Unwind funclet body (0x4752a0): unconditional
// `operator delete([ebp+4])`, no null check (delete(NULL) is a safe no-op).
struct HeaderGuard {
    void *p;
    HeaderGuard(void *pp) : p(pp) {}
    ~HeaderGuard() { if (p) ::operator delete(p); }
};

// FUNCTION: LOCO 0x412bd0
DSound::DSound() {
    nChannelCount = 0;
    bPersistentMute = false;
    pChannels = NULL;
    pLastUsedTicks = NULL;
    pDirectSound = NULL;
    pPrimaryBuffer = NULL;
    memset(&deviceGuid, 0, sizeof(deviceGuid));
    memset(&caps, 0, sizeof(caps));
}

// FUNCTION: LOCO 0x412c20 (??_GDSound scalar dtor; ~DSound() itself inlines
// into this wrapper -- see the inline definition in DSound.h)
// PARKED (v80): the channel-pool alloc/construct/teardown mechanics (the
// "array with 4-byte count header" idiom) are not yet fully reconciled with
// the mystery per-element ctor stub at 0x40ec30 -- see the file-header note
// above and docs/PARKED.md. The rest of this function (device selection,
// GetCaps, SetCooperativeLevel, primary buffer + mix format, pool-size
// formula, per-channel ceiling defaults) is transcribed directly from the
// confirmed disasm.
// PARKED (v85, DIFF ~420/665 bytes, down from v84's ~492/648 -- see
// docs/PARKED.md for the full residual breakdown): two real transcription
// fixes this session, both confirmed by direct disasm comparison against
// 0x412c50-0x412ed8: (1) the WAVEFORMATEX `wfx` local's real declaration site
// is immediately before `pPrimaryBuffer->SetFormat(&wfx)`, NOT at the top of
// the function -- the original defers all 7 field-init stores until right
// before that call; our previous top-of-function placement caused /O2 to
// hoist them to function entry instead, which was ALSO the root cause of the
// SEH prologue's internal instruction order being wrong (too much unrelated
// stack-store traffic before the `mov fs:0,esp` install). (2) `bSelectBestDevice`
// is declared BEFORE the `DSound_Teardown` call, not after -- the original
// hoists its `mov bl,1` initializer above the call (safe: ebx is callee-saved
// under this convention), which only reproduces when the source itself
// places the declaration first. With both fixes the SEH prologue now matches
// byte-for-byte (`mov eax,fs:0; push -1; push <thunk>; push eax; mov
// eax,[g_pApp]; mov fs:0,esp`) -- item (c) from v84's pickup is CLOSED,
// it was source-order-steerable all along, not intrinsic. Remaining residual
// is the already-cataloged symmetric-register-swap ripple (Yoda #29/#30) and
// cmp/test zero-reuse tie-breaks (Yoda #6), plus `bSelectBestDevice`'s
// register WIDTH (real keeps it in `bl` throughout; ours uses a full 32-bit
// register regardless of declared type -- tried `char`, made it worse,
// spilled to a stack slot and broke the prologue reorder again, reverted).
// FUNCTION: LOCO 0x412c50
unsigned char DSound::DSound_InitDeviceAndChannelPool(int nDefaultChannelCount, HWND hwndOwner) {
    BOOL bSelectBestDevice = 1;
    DSound_Teardown(g_pApp->hwndOwner);

    if (g_pIniFile != NULL) {
        bSelectBestDevice = g_pIniFile->ReadInt("Sound", "SelectBestDevice", 1);
    }

    HRESULT hr;
    if (bSelectBestDevice) {
        DirectSoundEnumerateA(DSound_PickBestDeviceCallback, this);
        hr = DirectSoundCreate(&deviceGuid, &pDirectSound, NULL);
        if (hr != DS_OK) {
            return 0;
        }
    } else {
        hr = DirectSoundCreate(NULL, &pDirectSound, NULL);
        if (hr != DS_OK) {
            return 0;
        }
        memset(&caps, 0, sizeof(caps));
        caps.dwSize = sizeof(caps);
        pDirectSound->GetCaps(&caps);
    }

    hr = pDirectSound->SetCooperativeLevel(hwndOwner, DSSCL_PRIORITY);
    if (hr != DS_OK) {
        return 0;
    }

    // EFFECTIVE MATCH (v86): orig defers loading pDirectSound's vtable pointer
    // until immediately before `call [eax+0xc]`, after all the dsbdesc field
    // stores; our compile loads it right after dereferencing pDirectSound,
    // before the field stores. Same residual class as
    // DSound_PickBestDeviceCallback's GetCaps call below (v80) -- confirmed
    // a real recurring class this session, not a one-off; see
    // docs/PARKED.md's "deferred vtable-pointer load" entry. Tried an
    // explicit `hr =` assignment (kept, marginal real improvement) and an
    // extra HRESULT temp for the callback's GetCaps: neither moves the load.
    DSBUFFERDESC dsbdesc;
    memset(&dsbdesc, 0, sizeof(dsbdesc));
    dsbdesc.dwSize = sizeof(dsbdesc);
    dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
    hr = pDirectSound->CreateSoundBuffer(&dsbdesc, &pPrimaryBuffer, NULL);
    if (hr == DS_OK) {
        WAVEFORMATEX wfx;
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = 2;
        wfx.nSamplesPerSec = 22050;
        wfx.nAvgBytesPerSec = 88200;
        wfx.nBlockAlign = 4;
        wfx.wBitsPerSample = 16;
        wfx.cbSize = 0;
        pPrimaryBuffer->SetFormat(&wfx);
    }

    if (!(caps.dwFlags & DSCAPS_EMULDRIVER) && caps.dwFreeHwMixingStaticBuffers != 0) {
        int nPoolSize = caps.dwFreeHwMixingStaticBuffers - 1;
        if (nPoolSize < nDefaultChannelCount) {
            nPoolSize = nDefaultChannelCount;
        }
        nChannelCount = nPoolSize;
    } else {
        nChannelCount = nDefaultChannelCount;
    }

    int nCount = nChannelCount;
    unsigned int *pHeader = (unsigned int *)::operator new(nCount * sizeof(DSoundChannel) + 4);
    DSoundChannel *pNewChannels;
    {
        HeaderGuard guard(pHeader);
        if (pHeader != NULL) {
            pNewChannels = (DSoundChannel *)(pHeader + 1);
            *pHeader = nCount;
            ArrayConstructWithIteratorMaybe(pNewChannels, sizeof(DSoundChannel), nCount, (void *)DSoundChannel_ConstructThunkMaybe, &DSoundChannel::Release);
        } else {
            pNewChannels = NULL;
        }
        guard.p = 0;
    }
    pChannels = pNewChannels;
    pLastUsedTicks = (unsigned int *)::operator new(nChannelCount << 2);

    nCeilingHigh1Master = 100;
    nCeilingHigh2Master = 100;
    nCeilingHigh1Effective = 100;
    nCeilingHigh2Effective = 100;
    nCeilingLowMaster = 0x14;
    nCeilingMedMaster = 0x28;
    nCeilingLowEffective = 0x14;
    nCeilingMedEffective = 0x28;

    for (unsigned int i = 0; i < (unsigned int)nChannelCount; i++) {
        pLastUsedTicks[i] = 0;
        pChannels[i].Init();
        pChannels[i].SetCeilingsAndApply(nCeilingLowMaster, nCeilingMedMaster, nCeilingHigh1Master, nCeilingHigh2Master);
    }

    Unk0xb0 = 0;
    nListenerX = 0;
    nListenerY = 0;
    return 1;
}

// PARKED (v80, DIFF 106/200 bytes -- down from 118 after caching the
// pChannels-4 base pointer in a local): remainder is symmetric-register-swap
// tie-breaks in the trailing Release()/SetCooperativeLevel calls, plus one
// extra `add esp,0x10` where the original combines 2 consecutive __cdecl
// callee-cleanups into a single add and this compile doesn't -- not
// reproduced by any source reshuffle tried this session.
// FUNCTION: LOCO 0x412ee0
void DSound::DSound_Teardown(HWND hwndOwner) {
    if (pChannels != NULL) {
        for (unsigned int i = 0; i < (unsigned int)nChannelCount; i++) {
            pChannels[i].Release();
        }
        if (pChannels != NULL) {
            char *pBase = (char *)pChannels - 4;
            ArrayDestructWithIteratorMaybe(pChannels, sizeof(DSoundChannel), *(int *)pBase, &DSoundChannel::Release);
            ::operator delete(pBase);
        }
        pChannels = NULL;
    }
    if (pLastUsedTicks != NULL) {
        ::operator delete(pLastUsedTicks);
        pLastUsedTicks = NULL;
    }
    if (pPrimaryBuffer != NULL) {
        pPrimaryBuffer->Release();
        pPrimaryBuffer = NULL;
    }
    if (pDirectSound != NULL) {
        pDirectSound->SetCooperativeLevel(hwndOwner, DSSCL_NORMAL);
        pDirectSound->Release();
        pDirectSound = NULL;
    }
}

// FUNCTION: LOCO 0x413070
HRESULT DSound::CreateSoundBuffer(LPDSBUFFERDESC pDesc, LPLPDIRECTSOUNDBUFFER ppBuffer, IUnknown *pUnkOuter) {
    if (pDirectSound != NULL) {
        return pDirectSound->CreateSoundBuffer(pDesc, ppBuffer, pUnkOuter);
    }
    return -1;
}

// FUNCTION: LOCO 0x4130a0
void DSound::DSound_SetListenerPosition(int x, int y) {
    nListenerX = x;
    nListenerY = y;
    for (unsigned int i = 0; i < (unsigned int)nChannelCount; i++) {
        pChannels[i].SetBounds(x, y);
    }
}

// FUNCTION: LOCO 0x4130f0
void DSound::ReclaimFinishedChannels() {
    for (unsigned int i = 0; i < (unsigned int)nChannelCount; i++) {
        if (pChannels[i].IsReclaimable()) {
            pChannels[i].Release();
        }
    }
}

// FUNCTION: LOCO 0x413140
void DSound::ReleaseAllChannels() {
    for (unsigned int i = 0; i < (unsigned int)nChannelCount; i++) {
        pChannels[i].Release();
    }
}

// FUNCTION: LOCO 0x413180
void DSound::PlaySoundById(UINT nSoundId) {
    SoundBankEntry *pDesc = g_UIResources.SoundBank_LookupEntryById(nSoundId);
    AcquireChannelForSound(pDesc, NULL, g_worldBoard.dwHalfWidth, g_worldBoard.dwHalfHeight, 4, 0);
}

// FUNCTION: LOCO 0x4131c0
void DSound::PlaySoundByIdWithHandle(UINT nSoundId, DSoundChannel **ppHandle) {
    if (ppHandle != NULL && *ppHandle != NULL) {
        (*ppHandle)->Release();
    }
    SoundBankEntry *pDesc = g_UIResources.SoundBank_LookupEntryById(nSoundId);
    AcquireChannelForSound(pDesc, ppHandle, g_worldBoard.dwHalfWidth, g_worldBoard.dwHalfHeight, 4, 0);
}

// PARKED (v81, DIFF 706/811 bytes, down from v80's 739/831 -- semantics
// confirmed correct, still structurally far from matching): the largest,
// most complex function in the TU. v80's diagnosis was incomplete: the real
// issue was not merely `break` vs. goto-shaped rotation, but that the
// ORIGINAL reuses a SINGLE index variable (`nChosen`, register edi) across
// ALL FOUR search loops (exact-soundid-reclaim / idle / any-reclaimable /
// category-fallback) -- confirmed by raw objdump: edi is set to -1 once at
// function entry and never re-initialized between loops; each of loops 2-4
// opens with `if (nChosen != -1) break;` reusing the SAME variable, not
// fresh nIdle/nReclaimable/nBest locals. v81 rewrote the source this way
// (single `nChosen` reused for all 3 middle loops, `unsigned char
// nActiveCount` instead of `unsigned int` to match the byte-sized counter +
// `and eax,0xff` mask seen in the original) -- this shrank the diff by 33
// bytes but did NOT close it. NOTE: VC5 uses pre-standard `for`-scope (the
// loop variable leaks into the enclosing block) -- reusing the SAME loop
// counter name (`i`) across sibling loops in one function is a hard
// redefinition error (C2371); keep distinct counter names (i2/i3/i4/i5) even
// though the tracking variable (`nChosen`) is now shared. Remaining
// residual (v81): loop A's `nSoundId` local is kept in the ORIGINAL's a
// stack slot ([esp+0x14], reloaded every iteration) but OUR compile keeps it
// in a register (ebx) instead -- looks like a whole-function register-
// pressure/spill difference (Yoda lesson #13/#19's family), not obviously
// steerable by reordering statements near the declaration (tried moving the
// `nActiveCount=0` declaration before `nSoundId`: zero effect on diff).
// Untried: forcing more register pressure elsewhere in the function (extra
// locals) to see if it changes the spill decision -- next session should
// try that, or accept this as a TU-position-driven residual and move on to
// the item 3 SEH-wrapper work on `DSound_InitDeviceAndChannelPool` instead
// (higher ROI, better-understood residual). See docs/subsystems.md for the
// full algorithm writeup (unchanged by the residual -- the semantics are
// confirmed correct, only the codegen shape is off). Channel-selection/steal
// dispatcher: (1) scan for channels already playing
// this soundId, counting non-reclaimable ones and marking reclaimable ones as
// "candidate" (state=4); (2) if under the max-instance cap and the retrigger
// cooldown has elapsed: reuse an exact-soundId reclaimable channel if one
// exists (fast path -- reposition/recategorize/re-loop-flag/resume in place);
// else pick a channel to steal, preferring (in order) an idle channel, any
// reclaimable channel, or the lowest-category/oldest-last-used channel whose
// category is <= the new sound's category; Release() it, AcquireAndPlay() the
// new sound, and on success bind the caller's handle and arm the retrigger
// cooldown (nNextAllowedTick = now + nRetriggerDelay).
// FUNCTION: LOCO 0x413210
void DSound::AcquireChannelForSound(SoundBankEntry *pDesc, DSoundChannel **ppHandle, int x, int y, unsigned int category, char bLoop) {
    if (pChannels == NULL || pDesc == NULL) {
        return;
    }

    int nChosen = -1;
    int nSoundId = pDesc->nSoundId;
    unsigned char nActiveCount = 0;
    for (unsigned int i = 0; i < (unsigned int)nChannelCount; i++) {
        if (pChannels[i].nSoundId == nSoundId) {
            if (pChannels[i].IsReclaimable()) {
                pChannels[i].nState = 4;
            } else {
                nActiveCount++;
            }
        }
    }

    if (nActiveCount >= pDesc->nMaxInstances || (int)g_dwGameTick < pDesc->nNextAllowedTick) {
        return;
    }

    if (pDesc->nSoundId >= 0) {
        for (int i2 = 0; i2 < nChannelCount; i2++) {
            if (nChosen != -1) break;
            if (pChannels[i2].nSoundId == pDesc->nSoundId && pChannels[i2].IsReclaimable()) {
                pChannels[i2].nState = 4;
                nChosen = i2;
            }
        }
        if (nChosen != -1) {
            pChannels[nChosen].ClearExternalHandle();
            pChannels[nChosen].SetPosition(x, y);
            pChannels[nChosen].SetCategory(category);
            pChannels[nChosen].bLoop = bLoop;
            pChannels[nChosen].ResumeOrRestart();
            pChannels[nChosen].BindExternalHandle(ppHandle);
            return;
        }
    }

    for (int i3 = 0; i3 < nChannelCount; i3++) {
        if (nChosen != -1) break;
        if (pChannels[i3].IsIdle()) {
            nChosen = i3;
        }
    }

    if (nChosen == -1) {
        for (int i4 = 0; i4 < nChannelCount; i4++) {
            if (nChosen != -1) break;
            if (pChannels[i4].IsReclaimable()) {
                pChannels[i4].nState = 4;
                nChosen = i4;
            }
        }
    }

    if (nChosen == -1) {
        for (int i5 = 0; i5 < nChannelCount; i5++) {
            if ((unsigned int)pChannels[i5].nCategory <= category) {
                if (nChosen == -1 || pChannels[i5].nCategory < pChannels[nChosen].nCategory ||
                    pLastUsedTicks[i5] < pLastUsedTicks[nChosen]) {
                    nChosen = i5;
                }
            }
        }
        if (nChosen == -1) {
            if (ppHandle != NULL) {
                *ppHandle = NULL;
            }
            return;
        }
    }

    pChannels[nChosen].Release();
    pChannels[nChosen].AcquireAndPlay(pDirectSound, pDesc, x, y, category, bLoop);
    if (pChannels[nChosen].nState != 2) {
        pChannels[nChosen].Release();
        return;
    }
    pChannels[nChosen].BindExternalHandle(ppHandle);
    if (pDesc->nRetriggerDelay > 0) {
        pDesc->nNextAllowedTick = g_dwGameTick + pDesc->nRetriggerDelay;
    }
}

// FUNCTION: LOCO 0x413530
void DSound::DSound_SetPersistentMute(bool bMute) {
    bPersistentMute = bMute;
    if (bMute) {
        nCeilingHigh1Effective = 0;
        nCeilingMedEffective = 0;
        nCeilingLowEffective = 0;
        nCeilingHigh2Effective = 0;
    } else {
        nCeilingHigh1Effective = nCeilingHigh1Master;
        nCeilingMedEffective = nCeilingMedMaster;
        nCeilingLowEffective = nCeilingLowMaster;
        nCeilingHigh2Effective = nCeilingHigh2Master;
    }
    for (unsigned int i = 0; i < (unsigned int)nChannelCount; i++) {
        pChannels[i].SetCeilingsAndApply(nCeilingLowEffective, nCeilingMedEffective, nCeilingHigh1Effective, nCeilingHigh2Effective);
    }
}

// FUNCTION: LOCO 0x4135b0
void DSound::DSound_SetTemporaryDuck(bool bDuck) {
    if (bDuck) {
        nCeilingHigh1Effective = 0;
        nCeilingMedEffective = 0;
        nCeilingLowEffective = 0;
    } else if (!bPersistentMute) {
        nCeilingHigh1Effective = nCeilingHigh1Master;
        nCeilingMedEffective = nCeilingMedMaster;
        nCeilingLowEffective = nCeilingLowMaster;
    }
    for (unsigned int i = 0; i < (unsigned int)nChannelCount; i++) {
        pChannels[i].SetCeilingsAndApply(nCeilingLowEffective, nCeilingMedEffective, nCeilingHigh1Effective, nCeilingHigh2Effective);
    }
}

// FUNCTION: LOCO 0x413630
void DSound::DSound_ApplyIniVolumeDefaults(int nLow, int nMed, int nHigh1, int nHigh2) {
    nCeilingHigh2Master = nHigh2;
    nCeilingHigh1Master = nHigh1;
    nCeilingMedMaster = nMed;
    nCeilingLowMaster = nLow;
    DSound_SetPersistentMute(bPersistentMute);
}

// EFFECTIVE MATCH (v80, DIFF 17/183 bytes): the only residual is whether
// `caps.dwSize = sizeof(caps)` gets stored before or after loading the
// vtable pointer for the GetCaps call -- tried hoisting through an explicit
// `DSCAPS *pCaps` local, no change; a scheduling tie-break, not source-shape-
// steerable at this size. v86: re-tried with an explicit `HRESULT hrCaps =`
// temp, zero effect; confirmed as the same recurring "deferred vtable-
// pointer load" class as DSound_InitDeviceAndChannelPool's CreateSoundBuffer
// call above -- see docs/PARKED.md.
// FUNCTION: LOCO 0x412fb0
BOOL CALLBACK DSound::DSound_PickBestDeviceCallback(GUID *lpGuid, LPSTR lpcstrDescription, LPSTR lpcstrModule, LPVOID lpContext) {
    DSound *pManager = (DSound *)lpContext;
    if (lpGuid != NULL && pManager != NULL) {
        LPDIRECTSOUND pTempDS = NULL;
        DirectSoundCreate(lpGuid, &pTempDS, NULL);
        if (pTempDS != NULL) {
            DSCAPS caps;
            memset(&caps, 0, sizeof(caps));
            caps.dwSize = sizeof(caps);
            if (pTempDS->GetCaps(&caps) == DS_OK &&
                (((caps.dwFlags & DSCAPS_CERTIFIED) && !(pManager->caps.dwFlags & DSCAPS_CERTIFIED)) ||
                 caps.dwFlags > pManager->caps.dwFlags)) {
                pManager->caps = caps;
                pManager->deviceGuid = *lpGuid;
            }
            pTempDS->Release();
        }
    }
    return TRUE;
}

// FUNCTION: LOCO 0x45b7e0 (Ghidra: DSound::DSound_GetOrCreateManager)
// Lazily creates the DSound manager singleton: no-op (return 0) if it already exists or if
// the `new` fails; on a fresh manager, initializes the device + 16-channel pool against the
// app's owner HWND (deleting the manager and returning 0 if that fails), seeds the listener
// position from the world board's viewport center-pair fields, then applies the [Sound]
// VolumeLow/Med/High ceilings from lego.ini (defaults 75/75/78 when there is no ini file --
// High is deliberately passed twice, see DSound_ApplyIniVolumeDefaults's own nHigh1/nHigh2
// pair). Returns 1 only on the full success path. The SEH frame is /GX automatic-unwind
// scaffolding around the `new DSound` (same class as DSound_InitDeviceAndChannelPool's own
// frame), not a hand-written try/catch.
//
// EFFECTIVE MATCH (v508, DIFF(188), compiled 340 B vs 347 B, asmscore in the PARKED row).
// Content-complete and branch-exact: every gate, call, argument and store pairs
// instruction-for-instruction; the layout probes that DID matter are baked in above (the
// ini-present arm must be the fall-through with the 75/75/78 defaults arm as the trailing
// `else`, and Low/Med/High read in that order). The ENTIRE residual is ONE instance of the
// documented v375 zero-register-residency class: the original never materializes a zero
// register -- `test eax,eax`/`test ecx,ecx` at all four NULL gates, immediate
// `mov dword,0` for the EH-state-0 store AND the post-delete singleton clear, 347 B -- while
// this build hoists `xor esi,esi` in the prologue and spends it on all four compares plus
// both stores (7 bytes shorter), which also parks VolumeLow in esi where the original uses
// edi (one knock-on register rename). Same signature as the sibling parks at 0x423ab0,
// 0x40d770 and 0x434100. Probes refuted (all byte-identical): the `int nLow, nMed, nHigh`
// sibling declaration-order swap, and routing the new-expression through a named
// `DSound *pNewManager` local before the global store. Retry only if the v375 class cracks.
unsigned char __stdcall DSound_GetOrCreateManager() {
    if (g_pDSoundManager != NULL) {
        return 0;
    }
    g_pDSoundManager = new DSound;
    if (g_pDSoundManager == NULL) {
        return 0;
    }
    if (g_pDSoundManager->DSound_InitDeviceAndChannelPool(0x10, g_pApp->hwndOwner) == 0) {
        delete g_pDSoundManager;
        g_pDSoundManager = NULL;
        return 0;
    }
    g_pDSoundManager->DSound_SetListenerPosition(g_worldBoard.dwViewportWidth,
                                                 g_worldBoard.dwViewportHeightMaybe);
    int nLow, nMed, nHigh;
    if (g_pIniFile != NULL) {
        nLow = g_pIniFile->ReadInt("Sound", "VolumeLow", 0x4b);
        nMed = g_pIniFile->ReadInt("Sound", "VolumeMed", 0x4b);
        nHigh = g_pIniFile->ReadInt("Sound", "VolumeHigh", 0x4e);
    } else {
        nLow = 0x4b;
        nMed = 0x4b;
        nHigh = 0x4e;
    }
    g_pDSoundManager->DSound_ApplyIniVolumeDefaults(nLow, nMed, nHigh, nHigh);
    return 1;
}

// FUNCTION: LOCO 0x45bb20
// The subsystem-level shutdown: persist the three master volume ceilings back to lego.ini's
// [Sound] section, tear the device down, then delete the manager singleton. The volume write is
// skipped (but the teardown is not) when there is no ini file, which is what happens if the game
// was started before the config was created.
//
// sic: g_pDSoundManager is re-tested for NULL immediately after DSound_Teardown, which cannot
// have cleared it -- a redundant guard the original really does emit.
void DSound_SaveVolumesAndShutdown() {
    if (g_pDSoundManager != NULL) {
        if (g_pIniFile != NULL) {
            g_pIniFile->WriteInt("Sound", "VolumeLow", g_pDSoundManager->nCeilingLowMaster);
            g_pIniFile->WriteInt("Sound", "VolumeMed", g_pDSoundManager->nCeilingMedMaster);
            g_pIniFile->WriteInt("Sound", "VolumeHigh", g_pDSoundManager->nCeilingHigh1Master);
        }
        g_pDSoundManager->DSound_Teardown(g_pApp->hwndOwner);
        delete g_pDSoundManager;
        g_pDSoundManager = NULL;
    }
}
