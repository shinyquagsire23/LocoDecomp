// Root-base (RectFlagObj0x477820) and AnimDescRefObj0x477488 ordinary-member bodies. Virtual
// slots and derived classes live in their own per-leaf TUs (WidgetPicker.cpp, MenuNode.h, ...);
// this file is for the 2 shared base classes' own non-virtual methods only.
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "WidgetBase.h"
#include "DSound.h"
#include "DSoundChannel.h"
#include "CursorDesc.h"
#include "LocoBitmap.h"       // RestoreOverlapBlt (BlitAnimFrameMaybe's own blit)
#include "Ddraw.h"            // g_pDDrawWorkSurface
#include "MenuNode.h"
#include "UIResources.h"    // g_UIResources.m_hFont14 (the label font)
#include "WorldBoardMaybe.h"
#include "AppWindow.h"        // g_pApp -- AdvanceAnimFrameMaybe's frame-rate throttle

extern unsigned int g_dwGameTick; // DAT_004a99b4
extern double DAT_00481170;       // last computed FPS sample (src/Main.cpp's own frame timer)

// FUNCTION: LOCO 0x4369d0
// The root base's ctor. Nulls both callbacks, tags the node type 1, empties the rect through
// SetRect (the four zero pushes plus `lea eax,[esi+8]` are the call, not four field stores),
// and arms bValid. Returns `this` in eax like every MSVC ctor.
//
// ⏱ MEASURED EXACT at v540 (commit b757018) and WITHHELD until now: this body -- or any one of
// its three siblings below, or even the header-only model fix -- costs
// AnimDescRefObj0x477488::AdvanceAnimFrameMaybe (0x405c40) its whole 407 B EXACT, for a net
// -256 B. It is landed anyway because the port needs it: left undefined it became a
// link/gen_stubs.py `ret`, so every widget in the family was constructed with a garbage rect,
// garbage bValid and two garbage callback pointers that TryInvokeCallbackA would then `call`.
// 0x405c40's own residual is content-complete and intrinsic (144/144 instructions, one eax<->edx
// base/index coin-flip); see src/WidgetBase.h and docs/PARKED.md's v540 section for the full
// pricing, and re-land the toll as a dial once transcription is complete.
RectFlagObj0x477820::RectFlagObj0x477820()
{
    pCallbackA = 0;
    pCallbackB = 0;
    nTypeTag = 1;
    SetRect(&rect, 0, 0, 0, 0);
    bValid = true;
}

// FUNCTION: LOCO 0x436ab0
// Slot 1. Hands this widget's own rect to the board's dirty-rect list, BY VALUE (the original's
// `sub esp,0x10` + four-dword copy is the argument, not a local). Same v540 toll as the ctor
// above -- one 407 B charge covers all of these, not one each.
void RectFlagObj0x477820::MarkDirty()
{
    g_worldBoard.MarkRectDirty(rect);
}

// FUNCTION: LOCO 0x436ae0
// Slot 4. Fire the primary per-node callback if one is installed, reporting whether it ran.
// The call is __cdecl (`push y; push x; call eax; add esp,8`) and its result is discarded, so
// the member's real type is `void (__cdecl *)(int, int)` rather than the `void *` still declared
// -- see src/WidgetBase.h, where retyping it is recorded as free-but-unlanded. Slot 5's
// TryInvokeCallbackB (0x436b00) is byte-for-byte this body against +0x20 instead of +0x1c, and
// is deliberately NOT landed here: its root declaration is still the wrong `void()`, and fixing
// that is a model edit tangled with WidgetBaseObj0x4784c8's slot-5 override.
char RectFlagObj0x477820::TryInvokeCallbackA(int x, int y)
{
    // Guard spelled `!= 0` with the CALL as the fallthrough arm and the zero return last: the
    // original's `test eax,eax / je` jumps AWAY to `xor al,al`, so the invoke path is the one
    // that falls through. The inverted spelling (early `return 0`) costs 23 bytes.
    if (pCallbackA != 0) {
        ((void(__cdecl *)(int, int))pCallbackA)(x, y);
        return 1;
    }
    return 0;
}

// FUNCTION: LOCO 0x436a00
// The root base's own virtual dtor: bare vptr store (0x477820) + bValid reset. Moved out
// of src/phase2_probe2.cpp 2026-07-22 (v322, was the probe-local VtblFlagStub0x436a00).
// Owner confirmed two ways: the stored vtable IS this class's own (raw disasm:
// `mov [ecx],0x477820; mov byte [ecx+0x18],0; ret`), and the derived dtors
// ~AnimDescRefObj0x477488 (0x405870) / ~MenuNodeObj0x477568 chain it as their base
// dtor. Defining the virtual dtor also makes cl auto-emit the matching scalar deleting
// destructor COMDAT below (same family as the 55-member `push mov call test je push call
// add mov pop ret` set tools/find_leaves.py surfaces -- see docs/subsystems.md §4).
RectFlagObj0x477820::~RectFlagObj0x477820() { bValid = false; }

// FUNCTION: LOCO 0x412600 (??_GRectFlagObj0x477820 scalar dtor)

// FUNCTION: LOCO 0x436a10
// Slot 2, the root base's own rect hit-test (ex-RectObj0x436a10::Contains in
// phase2_probe5.cpp -- moved out 2026-07-22, v322, once the +8/+0xc/+0x10/+0x14 field
// offsets were recognized as this class's own RECT behind the vtable ptr + nTypeTag).
// Referenced as a vtable slot by many derived widget classes (BigObj's +0x28 override,
// among others -- see docs/subsystems.md). AL-only return, no EAX-wide clear.
char RectFlagObj0x477820::Contains(int x, int y)
{
    if (x < rect.left || x >= rect.right) return 0;
    if (y < rect.top || y >= rect.bottom) return 0;
    return 1;
}

// FUNCTION: LOCO 0x436a40
// Screen -> widget-local transform. The point is returned BY VALUE; the original's apparent
// leading `int *out` parameter is MSVC's hidden 8-byte return-buffer pointer (first stack arg,
// also returned in eax -- hence `ret 0xc` for a __thiscall taking only two declared ints).
// See src/WidgetBase.h and docs/CODEGEN.md.
POINT RectFlagObj0x477820::ComputeLocalPos(int x, int y)
{
    POINT pt;
    pt.x = x - rect.left;
    pt.y = y - rect.top;
    return pt;
}

// FUNCTION: LOCO 0x436a60
// Slot 3, the root base's own reposition: move `rect` so its top-left lands on (x, y), keeping
// its width and height, and dirty-mark through slot 1 BOTH before and after the move -- the
// first call repaints what the widget is vacating, the second what it now covers.
void RectFlagObj0x477820::RepositionWithHotspot(int x, int y)
{
    MarkDirty();
    SetRect(&rect, x, y, x + (rect.right - rect.left), y + (rect.bottom - rect.top));
    MarkDirty();
}

// FUNCTION: LOCO 0x405c00
// Slot 3, this class's override of 0x436a60 above. Recenter `rect` through the base, then bring
// the two things that are anchored to the widget's position along with it: the hotspot-adjusted
// world anchor at +0x4c/+0x50, and the live sound channel if one is playing. That pairing is the
// point of the override -- it is why every consumer that asks "where is this object standing"
// reads hotspotPosX/Y rather than `rect`, and why a moving object's audio tracks it for free.
// Transcribed and verified EXACT in v445; landed in v486 once the declaration it needs stopped
// being net-negative (see the ⛔/✅ block in src/WidgetBase.h).
void AnimDescRefObj0x477488::RepositionWithHotspot(int x, int y)
{
    RectFlagObj0x477820::RepositionWithHotspot(x, y);
    hotspotPosX = pKindDesc->hotspotX + x;
    hotspotPosY = pKindDesc->hotspotY + y;
    if (pDSoundChannel != NULL) {
        pDSoundChannel->SetPosition(x, y);
    }
}

// FUNCTION: LOCO 0x405e20
// EXACT. First called from TilePlacedObj's own ctor (src/TilePlacedObj.cpp).
void AnimDescRefObj0x477488::SetCategoryIfPrintable(char *pszName)
{
    if (IsCharAlphaNumericA(*pszName) || *pszName == '\0') {
        strncpy(szCategoryName, pszName, 10);
    }
    szCategoryName[10] = 0;
}

// FUNCTION: LOCO 0x405790
// The class ctor. Zeroes the descriptor/sound/anim field block, arms bReady, seeds the
// default category string from the pooled "" literal (DAT_004851d0 -- an inlined strcpy),
// and only loads a descriptor when handed a positive resource id (nResourceId > 0); the
// embedded/no-arg users all pass -1 and skip it. The direct (devirtualized) SetDescriptor
// call matches the original's `CALL 0x00405900` -- inside the ctor the dynamic type is
// exactly this class. posX/posY (+0x74/+0x78) are the ctor-supplied DEFAULT position,
// stored before the compiler's vptr store by /Og scheduling.
AnimDescRefObj0x477488::AnimDescRefObj0x477488(int nResourceId, short nSubFrameArg,
                                               int nPosX, int nPosY) {
    posX = nPosX;
    posY = nPosY;
    nTypeTag = 2;
    pKindDesc = 0;
    pSoundEntry = 0;
    pDSoundChannel = 0;
    nSubFrame = 0;
    nAnimValueCache = 0;
    nSoundId = 0;
    dwSoundResumeTick = 0;
    nAnimTickCounter = 0;
    nAnimCooldownUntil = 0;
    bReady = true;
    strcpy(szCategoryName, "");
    if (nResourceId > 0) {
        SetDescriptor(nResourceId, nSubFrameArg, 0);
    }
    bAnimCoolingDownMaybe = false;
}

// FUNCTION: LOCO 0x405850 (??_GAnimDescRefObj0x477488 scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x405870
// The class's own destructor (vtable slot 0's non-deleting half). Drops the three references
// the object can hold, in the order the original does: the playing sound CHANNEL, the kind
// DESCRIPTOR, then the sound-bank ENTRY. The trailing ~RectFlagObj0x477820 call and the whole
// /GX unwind frame are compiler-emitted from the base subobject, not written here.
//
// The descriptor arm is the interesting one: the reference is given back ONLY when
// pKindDesc->bLoadOkFlag == 1, i.e. only when the descriptor actually finished loading -- and
// the MarkDirty() that precedes it repaints the vacated rect while the bitmap is still alive.
// pKindDesc is cleared either way, so a never-loaded descriptor is dropped without a
// ReleaseRef, which is what keeps a failed load from decrementing a refcount it never took.
// ReleaseRef() is CursorDesc's vtable slot 2 (BigObj : Obj0x4779e0 : CursorDesc), transcribed
// this session at 0x4257f0 -- calling it by name is what keeps this off the idiom lint's
// raw-slot-dispatch class.
//
// nSoundId is zeroed in BOTH the channel arm and the entry arm rather than once at the end;
// that duplication is the original's, and it is what pairs the id cache 1:1 with each of the
// two things that can invalidate it.
AnimDescRefObj0x477488::~AnimDescRefObj0x477488() {
    if (pDSoundChannel != NULL) {
        pDSoundChannel->Release();
        nSoundId = 0;
    }
    if (pKindDesc != NULL) {
        if (pKindDesc->bLoadOkFlag == 1) {
            MarkDirty();
            pKindDesc->ReleaseRef();
        }
        pKindDesc = NULL;
    }
    if (pSoundEntry != NULL) {
        nSoundId = 0;
        pSoundEntry->Release();
        pSoundEntry = 0;
    }
}

// TU-local view for 0x405ab0: EnsureSoundPlayingMaybe is REALLY an ordinary (non-virtual)
// member of AnimDescRefObj0x477488, but declaring it on the class in src/WidgetBase.h is
// measured net-negative (v349: it cost src/DPlaySessionMgr.cpp one exact match, 39 -> 38 /
// -166 B -- see the NOTE on the class and the per-consumer view in src/PeerTrainNode.cpp).
// The body therefore hangs off this view, same shape as the v506-v508 TU-local views; the
// view struct must sit ABOVE the // FUNCTION: marker (the lint_ghidra_sync site parser needs
// the marker directly above the definition). It also sits above 0x405900 SetDescriptor,
// whose body calls through it.
struct AnimDescRefSoundView0x405ab0 : AnimDescRefObj0x477488 {
    void EnsureSoundPlayingMaybe(unsigned int nSoundIdArg);  // 0x405ab0  // TODO: sync (TU-local view)
};

// FUNCTION: LOCO 0x405900
// Vtable slot 6 (the base's own body; WidgetBaseObj0x4784c8 overrides it at 0x454900).
// Loads/clears the descriptor for nResourceId and recomputes rect/subframe state. With an
// already-loaded descriptor of the SAME id and bForce clear, the whole reload is skipped and
// only the blit-flags/subframe tail runs. The reload: dirty-mark the old rect (own slot 1),
// ReleaseRef the old descriptor, fetch the new one through the UIResources factory
// (id <= 0 just leaves it NULL), realize its bitmap at the ctor-supplied default position,
// then size `rect` from the OLD origin + the descriptor's native dims and `rectViewport`
// from (0,0) + the same dims, and disarm nSubFrame. Every failure path (no descriptor, no
// realized bitmap, subframe still -1 after the dispatch) clears bValid and returns 0; the
// no-descriptor path also pokes EnsureSoundPlayingMaybe with the 0xffffffff sentinel. The
// tail re-caches the descriptor's dwRenderFlags into nBlitFlags and re-dispatches the
// subframe: the caller's nSubFrameArg when >= 0, else the descriptor's own
// wActiveFrameSetIndex (the movsx-read pin).
unsigned char AnimDescRefObj0x477488::SetDescriptor(int nResourceId, int nSubFrameArg,
                                                    char bForce) {
    nAnimCooldownUntil = 0;
    if (pKindDesc == 0 || bForce != 0 || pKindDesc->resourceId != nResourceId) {
        bValid = 1;
        if (pKindDesc != 0) {
            MarkDirty();
            pKindDesc->ReleaseRef();
            pKindDesc = 0;
        }
        if (nResourceId > 0) {
            pKindDesc = (BigObj *)g_UIResources.TileKind_GetOrLoadDescriptor(nResourceId);
        }
        if (pKindDesc == 0) {
            ((AnimDescRefSoundView0x405ab0 *)this)->EnsureSoundPlayingMaybe(0xffffffff); // idiom-exempt TU-local view call (the direct `call 0x405ab0` IS the original's byte shape; the member is view-only by the v349 measurement, see above)
            bValid = 0;
            return 0;
        }
        pKindDesc->GetOrLoadFrameBitmap(posX, posY);
        if (pKindDesc->pOwnedObjA == 0) {
            bValid = 0;
            return 0;
        }
        SetRect(&rect, rect.left, rect.top, rect.left + pKindDesc->nativeWidth,
                rect.top + pKindDesc->nativeHeight);
        SetRect(&rectViewport, 0, 0, pKindDesc->nativeWidth, pKindDesc->nativeHeight);
        nSubFrame = -1;
    }
    nBlitFlags = pKindDesc->dwRenderFlags;
    if (nSubFrameArg >= 0) {
        ReleaseChannelAndDispatch(nSubFrameArg);
    } else {
        ReleaseChannelAndDispatch((short)pKindDesc->wActiveFrameSetIndex);
    }
    if (nSubFrame == -1) {
        bValid = 0;
        return 0;
    }
    return 1;
}

// FUNCTION: LOCO 0x405a20
// EXACT. Releases pDSoundChannel (if any, clearing nSoundId) then dispatches
// vtable slot 14 (+0x38, still unidentified -- likely a subframe/direction setter given every
// known caller passes a small direction/subframe-shaped value, e.g. TrackTileObj's own
// ctor, src/TilePlacedObj.cpp).
void AnimDescRefObj0x477488::ReleaseChannelAndDispatch(unsigned int arg)
{
    if (pDSoundChannel != 0) {
        pDSoundChannel->Release();
        nSoundId = 0;
    }
    this->DispatchAnimStateMaybe(arg);
}

// FUNCTION: LOCO 0x405a50
// Vtable slot 14: SELECT one of the descriptor's animation frame-sets by index, and return the
// frame index that selection settled on. An out-of-range index (negative, or >= the descriptor's
// nFrameSetCount) is ignored and the cached frame comes back unchanged -- which is exactly why
// every caller uses the RETURN value instead of assuming the argument took. `nFrameSetArg` is
// SIGNED, pinned by the body's own `jl` against 0 / `jge` against nFrameSetCount.
//
// ⭐ LANDED v563 -- and the toll it was withheld against had already expired, exactly as the
// RectFlagObj0x477820 bundle's did in v560. From v479 to v562 this body sat verified but out of
// the tree because merely ADDING it cost the sibling AdvanceAnimFrameMaybe (0x405c40) its full
// 407-byte EXACT (measured three ways: plain member, TU-local view, signature-change-only).
// src/WidgetBase.h's own note records that 0x405c40 subsequently lost that EXACT to something
// else anyway, so the toll no longer existed to pay. Re-measuring cost one compile.
//
// It was also load-bearing for the PORT in a way nothing had noticed: a declared-only virtual is
// a generated stub, and this slot is reached from AnimDescRefObj0x477488::SetDescriptor via
// ReleaseChannelAndDispatch. The stub never wrote nSubFrame, so it stayed -1, so SetDescriptor
// took its `nSubFrame == -1` failure exit and returned 0 for EVERY widget in the game. That is
// what aborted the world load at BuildToolButton::InitMenuIconsMaybe's very first guard --
// PostMessage(WM_CLOSE, 5) and the "An error occurred while loading" box.
//
// `nFrame` is read ONCE into a local: re-reading pFrameEntry->nStartFrame at each of the two use
// sites changes the codegen.
int AnimDescRefObj0x477488::DispatchAnimStateMaybe(int nFrameSetArg)
{
    if (bValid != true) {
        return 0;
    }
    if (nFrameSetArg >= 0 && nFrameSetArg < pKindDesc->nFrameSetCount) {
        nSubFrame = nFrameSetArg;
        CursorAnimFrameEntry *pFrameEntry = &pKindDesc->paFrameEntries[nFrameSetArg];
        unsigned int nFrame = pFrameEntry->nStartFrame;
        nAnimTickCounter = 0;
        nAnimCooldownUntil = 0;
        nAnimValueCache = nFrame;
        SetAnimFrame(nFrame, 1);
        ((AnimDescRefSoundView0x405ab0 *)this)->EnsureSoundPlayingMaybe(pFrameEntry->nSoundBankEntryId); // idiom-exempt TU-local view call (same 0x405ab0 view as SetDescriptor above)
    }
    return nAnimValueCache;
}

// FUNCTION: LOCO 0x405ab0
// EFFECTIVE MATCH (v509, DIFF(10) -- DIFF(6) as of v512, when moving the view above the new
// 0x405900 SetDescriptor rotated the TU slightly; compiled 329 B = original's 329 B,
// asmscore 1110, insns 115/115, zero structural substitutions). Content-complete. The entire
// residual is TWO instances of the commutative-add accumulator coin-flip: in both dwSoundResumeTick arms
// the original folds the sum into the g_dwGameTick register (`mov eax,[tick]; add eax,edx;
// store eax`) while cl 11.00 folds it into the nDelay register (`add edx,eax; store edx`).
// Refuted probes: operand-order flips (both directions, both arms), parenthesization,
// decl-order swaps. One real lever found and KEPT: the temp split
// `nDelay = rand() % delay + 1; dwSoundResumeTick = g_dwGameTick + nDelay;` reproduces the
// original's `inc edx` where the fused expression emits `lea ecx,[edx+eax+1]`.
// Makes sound-bank id nSoundIdArg the one this object is playing. On an id change the old
// pDSoundChannel is released (and nSoundId cleared) and pSoundEntry is re-looked-up through
// the UIResources sound bank, rejecting an entry whose bLoaded byte isn't 1. The id is only
// republished into nSoundId when a valid entry exists (or the id is the 0xffffffff sentinel).
// With no live channel a fresh one is acquired (looping when the frame entry carries no
// retrigger delay) and, when there IS a delay, dwSoundResumeTick is armed to a randomized
// deadline -- `g_dwGameTick + rand() % delay + 1` for a positive delay, or the symmetric
// `delay + rand() % (2 - delay)` jitter for a negative one. With a live channel and an
// expired deadline the channel is simply retriggered. No-ops when g_pDSoundManager is NULL
// or the id is 0.
void AnimDescRefSoundView0x405ab0::EnsureSoundPlayingMaybe(unsigned int nSoundIdArg) // TODO: sync (TU-local view)
{
    int nDelay;
    CursorAnimFrameEntry *pFrameEntry;
    SoundBankEntry *pEntry;

    if (g_pDSoundManager != NULL && nSoundIdArg != 0) {
        if (nSoundIdArg != nSoundId) {
            if (pDSoundChannel != NULL) {
                pDSoundChannel->Release();
                nSoundId = 0;
            }
            dwSoundResumeTick = 0;
            pEntry = g_UIResources.SoundBank_LookupEntryById(nSoundIdArg);
            pSoundEntry = pEntry;
            if (pEntry != NULL && pEntry->bLoaded != 1) {
                pSoundEntry = NULL;
            }
        }
        pEntry = pSoundEntry;
        if (pEntry != NULL || nSoundIdArg == 0xffffffff) {
            nSoundId = nSoundIdArg;
        }
        if (pEntry != NULL) {
            pFrameEntry = pKindDesc->paFrameEntries + nSubFrame;
            if (pDSoundChannel != NULL) {
                if (pFrameEntry->nSoundRetriggerDelay > 0 &&
                    dwSoundResumeTick < (int)g_dwGameTick) {
                    pDSoundChannel->ResumeOrRestart();
                }
            } else {
                if (pFrameEntry->nSoundRetriggerDelay == 0) {
                    g_pDSoundManager->AcquireChannelForSound(pEntry, &pDSoundChannel,
                                                             rect.left, rect.top,
                                                             pFrameEntry->nSoundCategory, 1);
                    return;
                }
                g_pDSoundManager->AcquireChannelForSound(pEntry, &pDSoundChannel,
                                                         rect.left, rect.top,
                                                         pFrameEntry->nSoundCategory, 0);
                nDelay = pFrameEntry->nSoundRetriggerDelay;
                if (nDelay > 0) {
                    if (nDelay >= 1) {
                        nDelay = rand() % pFrameEntry->nSoundRetriggerDelay + 1;
                        dwSoundResumeTick = g_dwGameTick + nDelay;
                        return;
                    }
                    if (2 - nDelay != 0) {
                        nDelay = pFrameEntry->nSoundRetriggerDelay +
                                 rand() % (2 - pFrameEntry->nSoundRetriggerDelay);
                    }
                    dwSoundResumeTick = nDelay + g_dwGameTick;
                }
            }
        }
    }
}

// FUNCTION: LOCO 0x405c40
// Slot 10 -- the per-tick animation stepper, and the only writer of nAnimTickCounter. Steps the
// current subframe's frame window (nStartFrame -> nEndFrame, which may run in EITHER direction)
// at one frame per wFrameDivisor ticks, and on reaching the far end either arms a cooldown or
// "bounces" by handing nBounceSoundId to slot 14 and taking ITS return value as the new frame.
// The new frame is published through slot 8 (SetAnimFrame) only when it differs from the cached
// one, which is what keeps a settled animation from re-dirtying its rect every tick.
//
// Three things about the shape are worth recording:
//   - bDoubleSpeedFlag advances TWO ticks per call and snaps the resulting frame to an even
//     index (`& ~1`), rounding DOWN when counting up and UP when counting down -- i.e. the
//     sprite sheet's odd frames belong to the overlay layer (BlitOverlayFrameMaybe) and are
//     skipped by the base layer.
//   - all four arms compute the frame with a SHORT-width intermediate (the original's
//     `movsx eax,ax` pairs against the zero-extending `and edx,0xffff` of the unsigned-short
//     nEndFrame), so the `(short)` cast is load-bearing, not decoration.
//   - the four `if (frame past the end)` bodies are written out per arm in the source; VC5
//     cross-jumps the two shared blocks and leaves only the `test al,al; jne` per site, which
//     is exactly the original's layout.
// ⚠ REGRESSED, DELIBERATELY, in the session that made the smoke build boot: this was EXACT
// (407 bytes) as of 3a52a30 and is now DIFF(314). Nothing about this function changed -- the
// cause is the three method declarations added to src/UIResources.h (Init,
// TileKind_CreateDescriptor, TileKind_LoadDescriptorRange), which this TU includes for
// g_UIResources.m_hFont14. That header is the documented declaration-count dial (its own
// v340/v356 notes price it), and paying it is what let src/AppWindow.cpp's
// `g_UIResources.Init()` reach the real body instead of a do-nothing generated stub -- the
// stub whose 0 return produced the fatal startup MessageBox. Cost of the whole trade was
// -656 B / -2 funcs; this is 407 of it. Re-match by finding a form of the UIResources
// declarations that does not rotate this TU, NOT by reverting them. See docs/PARKED.md.
void AnimDescRefObj0x477488::AdvanceAnimFrameMaybe()
{
    if (bValid != true) {
        return;
    }
    CursorAnimFrameEntry *pEntry = pKindDesc->paFrameEntries + nSubFrame;
    // A one-frame window (or a frame already settled on the last one) is static -- but only
    // when this entry has no bounce target of its own.
    if (pEntry->nStartFrame == pEntry->nEndFrame && pEntry->nBounceSoundId < 0) {
        return;
    }
    if (nAnimValueCache == pEntry->nEndFrame && pEntry->nBounceSoundId < 0) {
        return;
    }
    if (bAnimCoolingDownMaybe == true) {
        // Frame-rate throttle: once the measured FPS drops below the configured vehicle
        // minimum, entries that carry a cooldown stop animating entirely.
        if ((double)g_pApp->minVehicleFps > DAT_00481170 && pEntry->nCooldownTicks > 0) {
            return;
        }
        if (nAnimCooldownUntil > (int)g_dwGameTick) {
            return;
        }
    }
    int nFrame;
    if (pEntry->bDoubleSpeedFlag == 0) {
        nAnimTickCounter++;
        if (pEntry->nStartFrame < pEntry->nEndFrame) {
            nFrame = (short)(nAnimTickCounter / pEntry->wFrameDivisor + pEntry->nStartFrame);
            if (nFrame > pEntry->nEndFrame) {
                if (bAnimCoolingDownMaybe == false) {
                    nFrame = pEntry->nEndFrame;
                    bAnimCoolingDownMaybe = true;
                    nAnimCooldownUntil = pEntry->nCooldownTicks + g_dwGameTick;
                } else {
                    nFrame = DispatchAnimStateMaybe(pEntry->nBounceSoundId);
                    bAnimCoolingDownMaybe = false;
                }
            }
        } else {
            nFrame = (short)(pEntry->nStartFrame - nAnimTickCounter / pEntry->wFrameDivisor);
            if (nFrame < pEntry->nEndFrame) {
                if (bAnimCoolingDownMaybe == false) {
                    nFrame = pEntry->nEndFrame;
                    bAnimCoolingDownMaybe = true;
                    nAnimCooldownUntil = pEntry->nCooldownTicks + g_dwGameTick;
                } else {
                    nFrame = DispatchAnimStateMaybe(pEntry->nBounceSoundId);
                    bAnimCoolingDownMaybe = false;
                }
            }
        }
    } else {
        nAnimTickCounter += 2;
        if (pEntry->nStartFrame < pEntry->nEndFrame) {
            nFrame = (short)(nAnimTickCounter / pEntry->wFrameDivisor + pEntry->nStartFrame) & ~1;
            if (nFrame > pEntry->nEndFrame) {
                if (bAnimCoolingDownMaybe == false) {
                    nFrame = pEntry->nEndFrame;
                    bAnimCoolingDownMaybe = true;
                    nAnimCooldownUntil = pEntry->nCooldownTicks + g_dwGameTick;
                } else {
                    nFrame = DispatchAnimStateMaybe(pEntry->nBounceSoundId);
                    bAnimCoolingDownMaybe = false;
                }
            }
        } else {
            nFrame = (short)(pEntry->nStartFrame - nAnimTickCounter / pEntry->wFrameDivisor + 1) & ~1;
            if (nFrame < pEntry->nEndFrame) {
                if (bAnimCoolingDownMaybe == false) {
                    nFrame = pEntry->nEndFrame;
                    bAnimCoolingDownMaybe = true;
                    nAnimCooldownUntil = pEntry->nCooldownTicks + g_dwGameTick;
                } else {
                    nFrame = DispatchAnimStateMaybe(pEntry->nBounceSoundId);
                    bAnimCoolingDownMaybe = false;
                }
            }
        }
    }
    if (nAnimValueCache != nFrame) {
        SetAnimFrame(nFrame, 1);
    }
}

// FUNCTION: LOCO 0x405de0
void AnimDescRefObj0x477488::SetAnimFrame(int nFrame, char bMarkDirty)
{
    if (bValid == true) {
        nAnimValueCache = nFrame;
        rectViewport.left = nFrame * pKindDesc->nativeWidth;
        rectViewport.right = (nFrame + 1) * pKindDesc->nativeWidth;
        if (bMarkDirty != '\0') {
            MarkDirty();
        }
    }
}

// FUNCTION: LOCO 0x405e60
// Slot 11 -- paint this object's current anim frame, clipped to the caller's dirty RECT.
//
// The destination is always the intersection of the object's own `rect` with that dirty rect;
// what varies is where the SOURCE rectangle is taken from, and that is selected per-subframe by
// the frame entry's own Unk0x16Maybe byte:
//   - the ordinary case measures the clip rect's inset from each of the object's own rect edges
//     and applies the same inset to rectViewport (which SetAnimFrame keeps pointing at the
//     current frame's horizontal slice of the sprite sheet);
//   - the Unk0x16Maybe == 1 case measures each HORIZONTAL edge against the OPPOSITE rectViewport
//     edge instead, which mirrors the source span, and ORs in blit flag 0x20. Because the two
//     mirrored X terms can come out in either order, the pair is normalized before SetRect --
//     the original spells that as two separate SetRect calls (VC5 tail-merges the shared `call`
//     but not the argument pushes, which is why both arms `jmp` to the same instruction).
// Blit flag 0x40 rides on the caller's `flag`, and the object's own persistent nBlitFlags is
// always ORed in.
//
// EFFECTIVE MATCH -- PARKED (asmscore --len 354: total 672, align **0**, reg_pen 6,
// identity_miss 6, byte_diff 12, insns 130/130, compiled 354 B = the original's exact code
// length). SIX instructions, one contiguous cluster, and every diff row is an `r`: computing
// `nRight`, the original loads rectClip.right before rectViewport.left and this compile loads
// them the other way round, which cascades into a 2-register (EBP/EDX) rename across the pair
// of hoisted loads that follow. The arithmetic order, the branch sense, both SetRect arms and
// the whole blit tail all pair instruction-for-instruction.
// **One lever IS baked in, do not undo:** `flags` must be mutated IN PLACE rather than copied
// into a `blitFlags` local. The original keeps the PARAMETER's own value in the long-lived EDI
// across all three ORs; introducing the local makes cl park nBlitFlags there instead and
// reshuffles the whole prologue (DIFF 27 -> 14, total 898 -> 672).
// **Measured and REJECTED -- do NOT re-run:** (1) spelling the first OR `nBlitFlags | flags`
// instead of `flags | nBlitFlags` is IDENTICAL -- cl canonicalizes the commutative operand
// order, so it cannot be used to steer which side lands in which register; (2) the
// swap-two-sibling-locals lever (declaring `nRight` ahead of `nLeft`) is much WORSE, not
// neutral -- DIFF(224) at 349 B -- which independently pins the original's declaration order.
void AnimDescRefObj0x477488::BlitAnimFrameMaybe(RECT rect, char flag, unsigned int flags)
{
    if (pKindDesc->pOwnedObjA != 0 && bReady != false) {
        RECT rectClip;
        if (IntersectRect(&rectClip, &this->rect, &rect)) {
            RECT rectSrc;
            flags |= nBlitFlags;
            if (pKindDesc->paFrameEntries[nSubFrame].Unk0x16Maybe == 1) {
                int nLeft = rectViewport.right - rectClip.left + this->rect.left;
                int nRight = this->rect.right + rectViewport.left - rectClip.right;
                flags |= 0x20;
                if (nLeft < nRight) {
                    SetRect(&rectSrc, nLeft, rectClip.top - this->rect.top, nRight,
                            rectViewport.bottom - this->rect.bottom + rectClip.bottom);
                } else {
                    SetRect(&rectSrc, nRight, rectClip.top - this->rect.top, nLeft,
                            rectViewport.bottom - this->rect.bottom + rectClip.bottom);
                }
            } else {
                SetRect(&rectSrc, rectViewport.left + rectClip.left - this->rect.left,
                        rectClip.top - this->rect.top,
                        rectViewport.right - this->rect.right + rectClip.right,
                        rectViewport.bottom - this->rect.bottom + rectClip.bottom);
            }
            if (flag == 1) {
                flags |= 0x40;
            }
            pKindDesc->pOwnedObjA->RestoreOverlapBlt(rectClip, g_pDDrawWorkSurface, rectSrc,
                                                     flags);
        }
    }
}

// FUNCTION: LOCO 0x405fd0
// Slot 12 -- the optional SECOND blit layer, painted right after BlitAnimFrameMaybe by every
// caller and taking the identical (RECT, char, unsigned int) argument list. Runs only for
// subframes whose bDoubleSpeedFlag is set. It bumps the anim frame by +1 through slot 8 (which
// re-points rectViewport at the NEXT frame's horizontal slice of the sprite sheet), blits THAT
// frame over the same clipped destination, then puts the frame index back.
//
// ⚠ Ghidra's decompilation of this function is WRONG and must not be used as the transcription
// source -- it invents `unaff_EBX`/`unaff_ESI` and reads both by-value RECT arguments 8 bytes
// low, which makes the two SetRect calls appear to target different locals. The layout below was
// re-derived by hand from the raw disasm and is pinned by an independent check: the `flag`
// parameter test at 0x406127 (`cmp BYTE PTR [esp+0x44],1`) resolves to entry_esp+0x14, which is
// exactly where `flag` sits. Both SetRect calls target the SAME `rectSrc` (entry_esp-0x10).
//
// sic: consequently the FIRST source-rect computation -- guard, mirror/plain split and all -- is
// DEAD. It is overwritten unconditionally by the second SetRect after the frame bump, and the
// second always uses the PLAIN formula, so the Unk0x16Maybe mirrored-source case never reaches
// the blit at all (flag 0x20 still does). This reads as a copy-paste leftover from
// BlitAnimFrameMaybe, whose first block it duplicates almost verbatim. Reproduced, not fixed;
// see docs/engine-bugs.md.
//
// ⚠ Two spellings differ from BlitAnimFrameMaybe's otherwise-identical block and are load-bearing
// (the operand order is visible in the emitted add/sub order): the subframe selector is tested
// `!= 0` here but `== 1` there, and the sums are grouped `clip - rect + viewport` here versus
// `viewport +/- clip +/- rect` there.
//
// EFFECTIVE MATCH -- PARKED (asmscore --len 466: total 1447, align **0**, reg_pen 13,
// identity_miss 13, byte_diff 17, insns 177/177, compiled 466 B = the original's exact code
// length). SAME residual class as its twin BlitAnimFrameMaybe above, in the SAME branch and on
// the SAME pair of expressions: every diff row is an `r` confined to the mirrored arm's
// nLeft/nRight computation, where the original and this compile schedule the operand loads into
// a different EDX/ECX/EDI assignment. Everything else pairs instruction-for-instruction --
// including the dead first SetRect, both 16-bit-wrapping SetAnimFrame calls, and the whole
// two-by-value-RECT blit tail -- which is what validates the hand-derived stack model above.
// Two functions landing on the identical cluster from independently written source is good
// evidence the class is intrinsic scheduling rather than a source-shape error; retry both
// together if it ever cracks. The `flags`-mutated-in-place lever is baked in here too.
void AnimDescRefObj0x477488::BlitOverlayFrameMaybe(RECT rect, char flag, unsigned int flags)
{
    if (bReady != false && pKindDesc->paFrameEntries[nSubFrame].bDoubleSpeedFlag != 0) {
        RECT rectClip;
        if (IntersectRect(&rectClip, &this->rect, &rect)) {
            RECT rectSrc;
            flags |= nBlitFlags;
            if (pKindDesc->paFrameEntries[nSubFrame].Unk0x16Maybe != 0) {
                int nLeft = rectViewport.right - rectClip.left + this->rect.left;
                int nRight = this->rect.right - rectClip.right + rectViewport.left;
                flags |= 0x20;
                if (nLeft < nRight) {
                    SetRect(&rectSrc, nLeft, rectClip.top - this->rect.top, nRight,
                            rectClip.bottom - this->rect.bottom + rectViewport.bottom);
                } else {
                    SetRect(&rectSrc, nRight, rectClip.top - this->rect.top, nLeft,
                            rectClip.bottom - this->rect.bottom + rectViewport.bottom);
                }
            } else {
                SetRect(&rectSrc, rectClip.left - this->rect.left + rectViewport.left,
                        rectClip.top - this->rect.top,
                        rectClip.right - this->rect.right + rectViewport.right,
                        rectClip.bottom - this->rect.bottom + rectViewport.bottom);
            }
            SetAnimFrame((unsigned short)(nAnimValueCache + 1), 0);
            SetRect(&rectSrc, rectClip.left - this->rect.left + rectViewport.left,
                    rectClip.top - this->rect.top,
                    rectClip.right - this->rect.right + rectViewport.right,
                    rectClip.bottom - this->rect.bottom + rectViewport.bottom);
            if (flag == 1) {
                flags |= 0x40;
            }
            pKindDesc->pOwnedObjA->RestoreOverlapBlt(rectClip, g_pDDrawWorkSurface, rectSrc,
                                                     flags);
            SetAnimFrame((unsigned short)(nAnimValueCache - 1), 0);
        }
    }
}

// FUNCTION: LOCO 0x4544e0
// The base's own ctor: default-constructs the AnimDescRefObj0x477488 half (its declared
// defaults ARE the -1,-1,0,0 the original pushes), then clears everything this class adds --
// the menu-node list head and the three node caches, the companion effect-spawner pointer,
// the carousel index pair -- empties the two satellite rects RepositionWithHotspot maintains,
// and drops the rectB suppression flag. Unk0xac (+0xac) is deliberately NOT touched here: the
// only byte store in the tail is to +0xad, so whatever arms Unk0xac stays elsewhere.
WidgetBaseObj0x4784c8::WidgetBaseObj0x4784c8()
{
    pMenuListHead = NULL;
    pLastHitNode = NULL;
    pBaseCandidateDown = NULL;
    pBaseCandidateUp = NULL;
    pEffectSpawner = NULL;
    nCarouselScrollIndex = 0;
    nCarouselMaxIndex = 0;
    SetRectEmpty(&rectBMaybe);
    SetRectEmpty(&rectCMaybe);
    bSuppressRectBMaybe = false;
    Unk0x8c = 0;
}

// The class's compiler-generated scalar-deleting-dtor thunk -- vtable 0x4784c8 slot 0. cl emits it
// from the declared-only `virtual ~WidgetBaseObj0x4784c8()` alongside this TU's vtable, so the
// COMDAT is already in WidgetBase.obj and the marker below is comment-only.
//
// Marked here even though the destructor BODY it calls (0x4545a0) is still unwritten: that dtor is
// priced-and-withheld (see docs/PARKED.md), and the two are independent COMDATs -- the thunk's own
// 30 bytes are byte-identical either way, since the call target is a relocation. What the thunk
// calls is the right function by name (`??1WidgetBaseObj0x4784c8`, which IS 0x4545a0); it is
// simply an unresolved external until that body lands, exactly like any other call to a
// not-yet-transcribed function.
//
// FUNCTION: LOCO 0x454580 (??_GWidgetBaseObj0x4784c8 scalar deleting dtor -- compiler-generated)

// FUNCTION: LOCO 0x454680
// Slot 6, this class's override of AnimDescRefObj0x477488::SetDescriptor. Chains the base, and on
// success eagerly REALIZES the newly bound descriptor's own bitmap -- widgets blit theirs every
// frame, so the lazy first-blit Convert() would otherwise land inside the draw loop. Kind id
// 0x2401 is exempt (it is the family's "no artwork" placeholder, whose pOwnedObjA is not a bitmap
// worth converting). The base's byte result is passed straight back out.
unsigned char WidgetBaseObj0x4784c8::SetDescriptor(int nResourceId, int nSubFrameArg, char bForce)
{
    unsigned char bBound = AnimDescRefObj0x477488::SetDescriptor(nResourceId, nSubFrameArg, bForce);
    if (bBound && nResourceId != 0x2401) {
        LocoBitmap *pBitmap = pKindDesc->pOwnedObjA;
        if (pBitmap->bConverted == 0) {
            pBitmap->Convert();
        }
    }
    return bBound;
}

// FUNCTION: LOCO 0x454820
// Slot 3, the family's third and last override of it (root 0x436a60 -> 0x405c00 -> here). Move
// the widget through the base, then rebuild the two SATELLITE rects this class adds -- rectCMaybe
// hanging 0x32 px right and directly below the widget, rectBMaybe 0x32 px right and 0x32 px down
// its side -- and dirty-mark, which through this class's own MarkDirty (0x454890, just below)
// repaints all three rects rather than just `rect`.
// ⚠ The leading base call must be spelled AnimDescRefObj0x477488::, not RectFlagObj0x477820::.
// Both compile and both byte-match, because verify.py masks relocations -- but only one of them
// emits a call to 0x405c00, which is what the original does. Until v486 the declaration that makes
// that spelling bind correctly did not exist, and this body was held back rather than shipped
// under a binding that would have been a silent lie in the source.
void WidgetBaseObj0x4784c8::RepositionWithHotspot(int x, int y)
{
    AnimDescRefObj0x477488::RepositionWithHotspot(x, y);
    SetRect(&rectCMaybe, rect.left + 0x32, rect.bottom, rect.right + 0x32, rect.bottom + 0x31);
    SetRect(&rectBMaybe, rect.right, rect.top + 0x32, rect.right + 0x32, rect.bottom);
    MarkDirty();
}

// FUNCTION: LOCO 0x454890
void WidgetBaseObj0x4784c8::MarkDirty()
{
    RectFlagObj0x477820::MarkDirty();
    if (bActive) {
        g_worldBoard.MarkRectDirty(rectCMaybe);
        g_worldBoard.MarkRectDirty(rectBMaybe);
    }
}

// Included here (mid-file) rather than at the top so the added declarations don't shift/rotate
// this already-heavily-matched TU's earlier functions (AdvanceAnimFrameMaybe/0x405c40 flipped
// EXACT->DIFF when this include sat at the top -- same end-of-file include pattern as
// src/DPlaySessionMgr.cpp).
#include "DDrawSurface.h"     // DDraw_DarkenRect (slot-11 BlitAnimFrameMaybe's overlap darken)

// FUNCTION: LOCO 0x454900
// Slot 11 (+0x2c), this class's own override of BlitAnimFrameMaybe: chains the embedded
// AnimDescRefObj0x477488 base's own body (the immediate sprite-frame blit), then, gated on
// bActive, intersects the passed rect against rectCMaybe -- and against rectBMaybe too when
// !bSuppressRectBMaybe -- and darkens each non-empty overlap in place on the work surface
// (DDraw_DarkenRect, 0x401540).
void WidgetBaseObj0x4784c8::BlitAnimFrameMaybe(RECT rect, char flag, unsigned int flags)
{
    RECT rectOverlap;

    AnimDescRefObj0x477488::BlitAnimFrameMaybe(rect, flag, flags);
    if (bActive != false) {
        if (IntersectRect(&rectOverlap, &rectCMaybe, &rect) != 0) {
            DDraw_DarkenRect(rectOverlap);
        }
        if (bSuppressRectBMaybe == false) {
            if (IntersectRect(&rectOverlap, &rectBMaybe, &rect) != 0) {
                DDraw_DarkenRect(rectOverlap);
            }
        }
    }
}

// FUNCTION: LOCO 0x4549e0
// EFFECTIVE MATCH -- structural twin of HitTestAndLocalizeMaybe below, same residual class.
// Two levers DID help close most of the gap from an initial DIFF(128)/139B candidate down to
// DIFF(110)/129B (target 123B): (1) the guard-clause branch-order lever (`if (Contains()==0)
// return 0;` instead of wrapping the body in `if (Contains()){...}`) -- the original keeps the
// FALSE case as fall-through and jumps forward into the body, matching the guard-clause form,
// not the wrapped form (CLAUDE.md's branch-order-is-a-real-lever family); (2) not caching `ly`
// (ComputeLocalPos's 2nd out-value) into a named local -- re-reading `local[1]` at its one use
// site frees a register `lx` alone doesn't need, matching the original's own asymmetric
// register/stack residency for the pair (lx stays in a register the whole loop, ly is spilled
// to a stack slot and reloaded each iteration in the ORIGINAL too -- Yoda lesson #13). What's
// left after both fixes: the original re-reads `this->vftableMaybe` (`mov eax,[esi]`) fresh
// before EACH of the 2 virtual calls (Contains, then the per-node dispatch inside the loop);
// our compile instead hoists/CSEs that read into one register (ebx) shared by both calls,
// since ComputeLocalPos is defined earlier in this same TU and provably doesn't touch `this`'s
// vtable -- the optimizer can prove the two reads are the same address and merges them. This
// steals the register slot the original reserves for `bAny` (bl), forcing bAny to a stack byte
// instead -- a real, but so far source-unsteerable, CSE/register-pressure interaction (several
// variants tried: bool vs char vs int bAny, register hint, inlined-condition vs separate bHit
// local -- all converge on the same shape). See docs/PARKED.md.
char WidgetBaseObj0x4784c8::TryInvokeCallbackA(int x, int y)
{
    char bAny = 0;
    if (Contains(x, y) == 0) {
        return 0;
    }
    POINT ptLocal = ComputeLocalPos(x, y);
    int lx = ptLocal.x;
    for (MenuNodeObj0x477568 *pNode = pMenuListHead; pNode != 0; pNode = pNode->pNext) {
        if ((pNode != pLastHitNode) &&
            ((this->HitTestNodeSecondary(pNode, lx, ptLocal.y) != 0) || (bAny != 0))) {
            bAny = 1;
        }
    }
    return bAny;
}

// FUNCTION: LOCO 0x454a60
// EFFECTIVE MATCH -- structural twin of HitTestAndLocalizeSecondaryMaybe above; same residual
// (the compiler CSEs the this->vftableMaybe read across the Contains() call and the loop's own
// per-node dispatch, stealing bAny's register -- see that function's autopsy comment for the
// full writeup). See docs/PARKED.md.
char WidgetBaseObj0x4784c8::HitTestAndLocalizeMaybe(int x, int y)
{
    char bAny = 0;
    if (Contains(x, y) == 0) {
        return 0;
    }
    POINT ptLocal = ComputeLocalPos(x, y);
    int lx = ptLocal.x;
    for (MenuNodeObj0x477568 *pNode = pMenuListHead; pNode != 0; pNode = pNode->pNext) {
        if ((pNode != pLastHitNode) &&
            ((this->HitTestNode(pNode, lx, ptLocal.y) != 0) || (bAny != 0))) {
            bAny = 1;
        }
    }
    return bAny;
}

extern unsigned char __fastcall CursorDesc_IsItemAvailableMaybe(CursorDesc *pDesc); // 0x4255f0, see
                                                                                                    // src/WorldActionCursor.cpp's own extern for the ready-check writeup
extern MenuNodeObj0x477568 *g_pMenuIconListTailMaybe; // DAT_00485270 -- private scratch tail
                                                                                    // cursor, see its own set_global plate comment

// FUNCTION: LOCO 0x4546d0
// 0x4546d0's real owner class is WidgetBaseObj0x4784c8, NOT TutorialWnd (its old Ghidra
// name/typing, refuted v357) -- ground-truthed via raw disasm: the field it reads/writes at
// `this`+0xd0 is exactly WidgetBaseObj0x4784c8::pMenuListHead, and its owning
// MenuNodeObj0x477568/UiIconListItem ctors ALREADY take a `WidgetBaseObj0x4784c8 *pOwner`
// (MenuNode.h). Every confirmed caller (BuildToolButton::InitMenuIconsMaybe,
// WidgetPickerObj0x477cc8::InitMenuIconsMaybe, WorldActionCursor::InitTrainCouplingMenuIconsMaybe)
// is a WidgetBaseObj0x4784c8 descendant. It lived on a TU-local derived view
// (WidgetBaseView0x4546d0) from v357 to 2026-07-26 purely to avoid rotating WidgetBase.h's many
// other TU consumers; promoting it to a real member of that class was then measured byte-neutral
// across the whole repo, so the view (and the sibling probe in WorldActionCursor.cpp) is gone.
// Builds (or appends) a menu-icon node onto this widget's own menu list. Early-returns the
// private tail cursor g_pMenuIconListTailMaybe unchanged if nDescAddr is 0 or the descriptor
// isn't ready yet (CursorDesc_IsItemAvailableMaybe). Allocates a plain MenuNodeObj0x477568 when
// nTextLen is 0, else a text-labeled UiIconListItem (font g_UIResources.m_hFont14). First node for this
// owner (pMenuListHead == 0) becomes both the owner's real list head AND the new tail cursor;
// every subsequent node is appended after the CURRENT tail cursor's own pNext, and the cursor
// advances by re-reading THROUGH the global (`g_pMenuIconListTailMaybe = g_pMenuIconListTailMaybe->pNext;`,
// not a cached local) -- matches the original's own redundant field reload in that branch.
// nTextLen deliberately unsigned + a `> 0` (not `!= 0`) comparison: the original's own
// TEST-then-JBE shape for this check only falls out of an explicit unsigned relational
// operator, not an equality test (same family as UiIconListItem::GetLabelTextLength's own
// "real return type is unsigned" lesson).
// EFFECTIVE MATCH -- PARKED (asmscore --len 0x146: total 30471, align=30 reg_pen=4
// identity_miss=4 byte_diff=31, insns 105/104; cc.sh DIFF(172), ours 334B vs orig 326B).
// Structure, both early-return guards, both allocation branches, and the append-vs-first-item
// split all confirmed byte-for-byte against the raw disasm (every branch target, every field
// read/write, and the shared final epilogue -- both early guards and both main branches jump
// into ONE common tail block, not per-branch returns). Residual is a single VC5 store-
// scheduling tie-break: the "first item" branch writes 2 fields from the same register
// (pMenuListHead, then g_pMenuIconListTailMaybe) -- the original computes the value once into
// edx/eax and reaches a SHARED 2-store tail block from all 3 sub-paths (UiIconListItem ctor,
// MenuNodeObj0x477568 ctor, and the allocation-failure fallthrough), while our compile
// duplicates the FIRST of the 2 stores into each of those 3 sub-paths and only shares the
// SECOND via jump. Tried reordering which field is written first (both orders tried: whichever
// write comes first in source gets duplicated, whichever comes second gets shared -- the
// compiler's own duplication threshold, not a source-order bug) and assigning through an
// intermediate local vs. reading back through the global directly (decompile's own literal
// shape, `PTR_00485270 = ctor(...); ...; this->pMenuListHead = PTR_00485270;` -- both variants
// converged on the SAME best score, 30471, achieved here). Same family as the project's other
// documented VC5 tail-duplication/cross-jump tie-breaks (0x4597e0's own "cross-jump/tail-merge
// class"). See docs/PARKED.md.
MenuNodeObj0x477568 *WidgetBaseObj0x4784c8::GetOrCreateMenuIconItemMaybe(CursorDesc *pDesc, unsigned short wModeFlags, unsigned int nTextLen)
{
    if (pDesc != NULL && CursorDesc_IsItemAvailableMaybe(pDesc)) {
        if (pMenuListHead == 0) {
            MenuNodeObj0x477568 *pNode;
            if (nTextLen > 0) {
                pNode = new UiIconListItem(nTextLen, this, pDesc, (int)g_UIResources.m_hFont14, wModeFlags);
            } else {
                pNode = new MenuNodeObj0x477568(this, pDesc, wModeFlags);
            }
            pMenuListHead = pNode;
            g_pMenuIconListTailMaybe = pNode;
        } else {
            MenuNodeObj0x477568 *pNode;
            if (nTextLen > 0) {
                pNode = new UiIconListItem(nTextLen, this, pDesc, (int)g_UIResources.m_hFont14, wModeFlags);
            } else {
                pNode = new MenuNodeObj0x477568(this, pDesc, wModeFlags);
            }
            g_pMenuIconListTailMaybe->pNext = pNode;
            g_pMenuIconListTailMaybe = g_pMenuIconListTailMaybe->pNext;
        }
    }
    return g_pMenuIconListTailMaybe;
}

// FUNCTION: LOCO 0x4061b0
// Slot 9. The "ready" flag is what gates a widget's own animation/interaction, so flipping it
// has to re-dirty the widget AND bring its looping sound into line: a widget that goes
// un-ready pauses its channel rather than releasing it, so the same sound resumes mid-phrase
// when it comes back. Only the pause/resume is conditional on there being a channel at all --
// the flag store and the MarkDirty happen either way.
void AnimDescRefObj0x477488::SetReadyStateMaybe(bool bIsReady) {
    bReady = bIsReady;
    MarkDirty();
    if (pDSoundChannel != NULL) {
        if (bIsReady) {
            pDSoundChannel->ResumeOrRestart();
        } else {
            pDSoundChannel->Pause();
        }
    }
}

// FUNCTION: LOCO 0x454ae0
// Slot 16, the family base's keyboard handler: ENTER and ESC "press" the widget's two standing
// candidate nodes (the OK and Cancel affordances every derived widget registers), driving each
// into node state 2 and stamping the 6 that marks a node as activated-by-key. Any other key is
// declined, which is what lets WidgetPickerObj0x477cc8's override (0x4290a0) chain this first
// and only then consider its own arrow/edit keys.
//
// A candidate that is absent or not in state 1 (idle) is silently skipped, but the key still
// counts as CONSUMED -- returning true here is about "this was one of my keys", not about
// anything having happened. The `bResult` + switch shape is the derived override's, and it is
// what produces the original's leading `xor al,al` with a single shared `mov al,1` tail.
bool WidgetBaseObj0x4784c8::OnKeyDownMaybe(unsigned int nKey) {
    bool bResult = false;
    switch (nKey) {
    case VK_RETURN:
        if (pBaseCandidateUp != 0 && pBaseCandidateUp->wState == 1) {
            pBaseCandidateUp->SetNodeState(2);
            pBaseCandidateUp->wSelIndexMaybe = 6;
        }
        bResult = 1;
        break;
    case VK_ESCAPE:
        if (pBaseCandidateDown != 0 && pBaseCandidateDown->wState == 1) {
            pBaseCandidateDown->SetNodeState(2);
            pBaseCandidateDown->wSelIndexMaybe = 6;
        }
        bResult = 1;
        break;
    }
    return bResult;
}
