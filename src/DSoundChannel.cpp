// Phase 4, first real TU: the DirectSound per-voice "channel" object.
//
// Address-contiguous, confirmed TU boundaries on both sides (0x40ec70-0x40f090):
// preceded by the unrelated CarNetObj/CarKindDesc train-car-descriptor cluster
// (last function ends at 0x40eb60), followed immediately by CreditsWnd::CreditsWnd
// at 0x40f1c0 with no gap. One class, 14 methods, all resolved to certain names --
// see docs/subsystems.md's DirectSound section for the full writeup.
//
// DSoundChannel wraps one physical DirectSound secondary buffer slot out of the
// fixed-size pool built by DSound::DSound_InitDeviceAndChannelPool (0x412c50, a
// DIFFERENT TU -- the DSound manager -- not tackled this session). The manager
// selects/steals channels and calls these methods; this TU only needs the manager's
// interface pointer and a minimal SoundBankEntry forward shape, not their full
// implementations.

#include <math.h>

#include "DSound.h"
#include "DSoundChannel.h"

// LocoBitmap_GetDSoundErrorString lives in src/FrameDriver.cpp (0x45c2e0, EXACT v498) --
// declared extern "C" to sidestep C++ mangling since the call's target is a relocation masked
// out by tools/verify.py's byte comparison anyway; only the calling convention at the call
// site is real, matchable code.
// Real C++ linkage, NOT extern "C": src/FrameDriver.cpp DEFINES this as
// ?LocoBitmap_GetDSoundErrorString@@YAPADH@Z, so an extern "C" spelling here emitted a call to
// _LocoBitmap_GetDSoundErrorString -- a symbol nothing defines. Byte-invisible (relocations are
// masked by tools/verify.py); only the calling convention at the call site is real code.
char *LocoBitmap_GetDSoundErrorString(int hresult); // 0x45c2e0, src/FrameDriver.cpp

// FUNCTION: LOCO 0x40ec30
DSoundChannel::DSoundChannel() {
    pExternalHandle = 0;
    bLoop = false;
    nCategory = 2;
    nVolumePercent = 100;
    nState = 1;
    pBuffer = 0;
    nBoundX = 0;
    nBoundY = 0;
    nPosX = 0;
    nPosY = 0;
    nSoundId = 0;
}

// FUNCTION: LOCO 0x40ec70
void DSoundChannel::Init() {
    nCategory = 2;
    pExternalHandle = 0;
    bLoop = false;
    nVolumePercent = 100;
    nState = 1;
    pBuffer = 0;
    nBoundX = 0;
    nBoundY = 0;
    nPosX = 0;
    nPosY = 0;
    nSoundId = 0;
}

// FUNCTION: LOCO 0x40eca0
void DSoundChannel::Release() {
    if (pBuffer != 0) {
        pBuffer->Stop();
        pBuffer->Release();
        pBuffer = 0;
    }
    if (pExternalHandle != 0) {
        *pExternalHandle = 0;
        pExternalHandle = 0;
    }
    nState = 1;
    nSoundId = 0;
}

// FUNCTION: LOCO 0x40ecf0
void DSoundChannel::ClearExternalHandle() {
    if (pExternalHandle != 0) {
        *pExternalHandle = 0;
        pExternalHandle = 0;
    }
}

// FUNCTION: LOCO 0x40ed10
void DSoundChannel::BindExternalHandle(DSoundChannel **externalHandle) {
    pExternalHandle = externalHandle;
    if (externalHandle != 0) {
        *externalHandle = this;
    }
}

// PARKED (v79, DIFF 155/228 bytes): predominantly the symmetric-register-swap
// class (ecx/edx/esi/edi/ebx/ebp picked oppositely at several equivalent spots,
// see docs/PARKED.md's recurring-class note) -- structurally identical call
// sequence (DuplicateSoundBuffer, retry-via-ReclaimFinishedChannels, field
// stores, Play). One real residual: the bLoop store normalizes via `setne`
// in the original (xor ecx,ecx; test al,al; setne cl) vs a direct byte store
// here -- tried `bLoop = (loop != 0)` (should force the same setne shape);
// no change observed, likely masked by the upstream register-swap drift
// shifting the comparison window. Not re-chased further this session.
// FUNCTION: LOCO 0x40ed20
void DSoundChannel::AcquireAndPlay(LPDIRECTSOUND pDS, SoundBankEntry *pDesc, int x, int y, int category, char loop) {
    if (pBuffer != 0) {
        OutputDebugStringA("Should never get here....\n");
    }
    if (pDesc->pMasterBuffer == 0) {
        pDesc->EnsureLoaded();
    }
    HRESULT hr = pDS->DuplicateSoundBuffer(pDesc->pMasterBuffer, &pBuffer);
    if (hr == DSERR_ALLOCATED) {
        g_pDSoundManager->ReclaimFinishedChannels();
        hr = pDS->DuplicateSoundBuffer(pDesc->pMasterBuffer, &pBuffer);
    }
    if (hr < 0) {
        LocoBitmap_GetDSoundErrorString(hr);
        return;
    }
    nCategory = category;
    SetVolume(nVolumePercent);
    SetPosition(x, y);
    bLoop = (loop != 0);
    nSoundId = pDesc->nSoundId;
    if (!loop) {
        pBuffer->Play(0, 0, 0);
        nState = 2;
        return;
    }
    pBuffer->Play(0, 0, DSBPLAY_LOOPING);
    nState = 2;
}

// FUNCTION: LOCO 0x40ee00
void DSoundChannel::Pause() {
    if (pBuffer != 0) {
        pBuffer->Stop();
    }
    nState = 3;
}

// PARKED (v79, DIFF 89/106 bytes): original compares `nState==2` directly
// (cmp [this+0x10],2) at the branch; this compiles the same source condition
// through a shared bl flag (xor bl,bl / mov bl,1 / test bl,bl / je) both with
// the `||`+comma-operator form and an equivalent goto rewrite (tried both,
// identical byte count) -- reads as this toolchain's fixed lowering for the
// "state==2 THEN conditionally check hardware status" shape, not a statement-
// order issue. See docs/PARKED.md.
// FUNCTION: LOCO 0x40ee20
void DSoundChannel::ResumeOrRestart() {
    if (pBuffer == 0) {
        return;
    }
    if (nState == 2) {
        DWORD status;
        pBuffer->GetStatus(&status);
        if ((status & 1) != 0) goto done;
    }
    if (nState != 3) {
        pBuffer->SetCurrentPosition(0);
    }
    if (!bLoop) {
        pBuffer->Play(0, 0, 0);
        nState = 2;
        return;
    }
    pBuffer->Play(0, 0, DSBPLAY_LOOPING);
done:
    nState = 2;
}

// MATCHED v5 (src/phase2_probe.cpp) -- unparked from v1: fix was unsigned char return type +
// if/return-1/return-0 shape, not the field load.
// FUNCTION: LOCO 0x40eea0
unsigned char DSoundChannel::IsIdle() {
    if (nState == 1) return 1;
    return 0;
}

// FUNCTION: LOCO 0x40eeb0
// Two source facts get the original's jump table, both found in v361 (parked v79-v361 as an
// unsteerable "jump-table-vs-ifchain threshold call the compiler makes on its own"):
//  1. `case 3` needs its EXPLICIT `result = 0;`. With a bare `break` it is semantically
//     identical to the default, so MSVC folds the two and sees only THREE case labels --
//     below its table threshold -- and emits an if-chain. The redundant store survives /O2
//     as the original's own second `xor bl,bl` (0x40eec4) with its own epilogue, distinct
//     from the prologue's initializer at 0x40eeb5. DIFF 45/55 -> 14.
//  2. Case bodies land in .text in SOURCE DECLARATION order (this codebase's documented VC5
//     lesson, cf. AlbumCardWnd::DrawButtonIcon), so they are declared 3, 2, then 1/4 to match
//     the original's block order -- NOT case-value order. That closed the rest.
// The COMDAT is 76 bytes = 60 code + a 16-byte 4-entry jump table; entries [0] and [3]
// (states 1 and 4) share one target, which is why only 3 distinct bodies exist.
unsigned char DSoundChannel::IsReclaimable() {
    unsigned char result = 0;
    DWORD status;
    switch (nState) {
    case 3:
        result = 0;
        break;
    case 2:
        if (pBuffer == 0) break;
        pBuffer->GetStatus(&status);
        if ((status & 1) == 0) result = 1;
        break;
    case 1:
    case 4:
        result = 1;
        break;
    }
    return result;
}

// MATCHED (src/phase2_probe2.cpp) -- CORRECTED 2026-07-14: genuine DSoundChannel method (the
// old "generic multi-class XY setter, 5+ unrelated callers" doc claim was wrong -- exactly one
// caller, DSound::DSound_SetListenerPosition). Updates the listener/viewport bounds used for
// the pan calculation below, then re-applies SetPosition with the unchanged current position
// to recompute pan against the new bounds.
// FUNCTION: LOCO 0x40ef00
void DSoundChannel::SetBounds(int x, int y) {
    nBoundX = x;
    nBoundY = y;
    SetPosition(nPosX, nPosY);
}

// PARKED (v79, DIFF 139/267 bytes): pan formula rigorously re-derived from raw
// x87 disasm (fldln2+fyl2x twice + fdivp = log(ratio)/log(2.0), i.e. true
// log2, matching Ghidra's own pseudocode "log2(...)" calls once decoded) and
// cross-checked constant-for-constant (0.0/1.0/10000.0/2.0 at 0x477628/30/38/40)
// -- algorithm and constants confirmed correct. Byte residual is clamp/bounds
// register-swap plus scheduling of the two intermediate multiplies (*10.0 then
// *100.0, kept as separate statements per the traced two distinct fmul sites,
// not constant-folded to *1000.0). Not re-chased further this session.
// FUNCTION: LOCO 0x40ef20
void DSoundChannel::SetPosition(int x, int y) {
    if (x < 0) {
        x = 0;
    }
    if (nBoundX != 0 && x > nBoundX) {
        x = nBoundX - 1;
    }
    if (y < 0) {
        y = 0;
    }
    if (nBoundY != 0 && y > nBoundY) {
        y = nBoundY - 1;
    }
    nPosX = x;
    nPosY = y;

    double pan = 0.0;
    if (nBoundX != 0 && nBoundY != 0) {
        double offset = ((double)x - (double)(nBoundX >> 1)) / (double)(nBoundX >> 1);
        if (offset != 0.0) {
            double mag = fabs(offset);
            if (mag == 1.0) {
                pan = 10000.0;
            } else {
                pan = log(1.0 / (1.0 - mag)) / log(2.0);
                pan = pan * 10.0;
                pan = pan * 100.0;
            }
            if (offset < 0.0) {
                pan = -pan;
            }
        }
    }
    if (pBuffer != 0) {
        pBuffer->SetPan((long)pan);
    }
}

// FUNCTION: LOCO 0x40f040
void DSoundChannel::SetCeilingsAndApply(int low, int med, int high1, int high2) {
    nCeilingLow = low;
    nCeilingMed = med;
    nCeilingHigh1 = high1;
    nCeilingHigh2 = high2;
    SetVolume(nVolumePercent);
}

// FUNCTION: LOCO 0x40f070
void DSoundChannel::SetCategory(int category) {
    nCategory = category;
    SetVolume(nVolumePercent);
}

// PARKED (v79, DIFF 221/300 bytes): volume-to-dB curve rigorously re-derived
// from raw x87 disasm (fldl2e+frndint+f2xm1+fscale = classic inlined exp(),
// matching Ghidra's pseudocode once the intrinsic is decoded) and cross-checked
// constant-for-constant (0.01/2.55/255.0/0.03611937255272424 at 0x477658/68/70/78,
// -10000.0/0.0 boundary clamps at 0x477660/28) -- algorithm and constants
// confirmed correct, the per-case 64-bit (uint)->double fild idiom now
// reproduces per-case as in the original. Residual is the same symmetric-
// register-swap class as AcquireAndPlay/ResumeOrRestart. Not re-chased further
// this session.
// FUNCTION: LOCO 0x40f090
void DSoundChannel::SetVolume(int percent) {
    nVolumePercent = percent;
    double atten;
    switch (nCategory) {
    case 1: atten = (double)(unsigned int)(nCeilingLow * percent); break;
    case 2: atten = (double)(unsigned int)(nCeilingMed * percent); break;
    case 3: atten = (double)(unsigned int)(nCeilingHigh1 * percent); break;
    case 4: atten = (double)(unsigned int)(nCeilingHigh2 * percent); break;
    default: atten = (double)(unsigned int)(nCeilingHigh1 * percent); break;
    }
    atten = atten * 0.01;
    double db;
    if (atten <= 0.0) {
        db = -10000.0;
    } else if (atten >= 100.0) {
        db = 0.0;
    } else {
        // exp() inlines via the same x87 idiom as log() below; matches the
        // traced disasm (fldl2e/f2xm1/fscale) exactly -- see docs/subsystems.md.
        db = 1.0 - exp((255.0 - atten * 2.55) * 0.03611937255272424);
    }
    if (pBuffer != 0) {
        pBuffer->SetVolume((long)db);
    }
}
