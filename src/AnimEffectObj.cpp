// AnimEffectObj0x477a90's constructor (see src/AnimEffectObj.h for the class writeup).

#include <stdlib.h> // rand()
#include <ctype.h>  // toupper()

#include "AnimEffectObj.h"
#include "CursorDesc.h"      // BigObj (pKindDesc's type) + CursorAnimFrameEntry
#include "DSoundChannel.h"   // IsReclaimable, the last of TickMaybe's four expiry conditions
#include "EffectSpawner.h"   // DAT_004fd220 -- the real singleton (see the note below)
#include "RandRange.h"       // RAND_RANGE_MAYBE
#include "WorldBoardMaybe.h"

extern unsigned int g_dwGameTick; // DAT_004a99b4

// The TU-local BigObjTrackingSetsAnimPartial view that used to stand in for the effect-spawner
// singleton here was retired in v574, exactly as src/EffectSpawner.h's own migration note asks
// and by the same free mechanism v564 used for src/WorldBoardMaybe.cpp's view (CODEGEN #184).
// It was a LIVE defect, caught at runtime rather than by any lint: a view spelling mangles under
// the VIEW's class name, so the ctor's shadow spawn below resolved to a symbol defined nowhere
// and the port linked it against a `xor eax,eax; ret N` stub. Every animated world object's
// DROP SHADOW was silently swallowed -- 8 spawns lost in one boot-to-world run, visible only in
// loco/stub_calls.log. The dtor's RemoveHandle never fired either, since the stub handed back a
// NULL handle to test.

// The placement modes below all draw random coordinates from an inclusive [lo, hi] range via
// RAND_RANGE_MAYBE (src/RandRange.h) -- hoisted there in v367 once ScreenSaver::GetLayoutFileName
// turned up as a second, unrelated consumer of the identical guard shape.

// FUNCTION: LOCO 0x4234e0 (??_GAnimEffectObj0x477a90 scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x423500
// Hands the spawned companion effect back to the EffectSpawner singleton, then chains
// ~AnimDescRefObj0x477488. Out of line -- see the header's note on why the dtor is defined at
// all (a declared-but-undefined one breaks this TU's disasm-diff loop).
//
// ⚠ pEffectMaybe is deliberately NOT nulled after the handoff, unlike every other owning-pointer
// teardown in this codebase (compare ~MenuNodeObj0x477568, ~BigObj, ~Obj0x4779e0, which all
// clear the field they just released). That is the original's, not a transcription shortcut:
// the store is simply absent from the bytes, and adding it costs three instructions.
AnimEffectObj0x477a90::~AnimEffectObj0x477a90()
{
    if (pEffectMaybe != NULL) {
        DAT_004fd220.EffectSpawner_RemoveHandle(pEffectMaybe);
    }
}

// FUNCTION: LOCO 0x422ec0
// EFFECTIVE MATCH (asmscore.py --len 1510: total 381480, align=374 reg_pen=66
// identity_miss=66 byte_diff=220, insns 530/503): every block is structurally identical
// to the original -- the /GX EH prologue + AnimDescRefObj0x477488 base-ctor chain
// (byte-identical through the switch's jump-table dispatch), the pEffectMaybe/bUnk0x94/
// toupper init sequence, the frame-entry cooldown seed (nStartFrame==nEndFrame ->
// nCooldownTicks + g_dwGameTick), the categoryByte==8 ? bFootprintXSteps :
// rand()%3+1 variant byte, the jump-table switch with case blocks emitted in the
// original's C/R/P/S/U/D/W stream order (matched by ordering the source cases the same
// way), the per-arm duplicated class-qualified RepositionWithHotspot calls whose /Og
// suffix-merging reproduces the original's TWO shared call sites (0x423413 for
// C/R/U/D/W, and 0x42324d/0x423250 = S's shared NEG block falling into P's call +
// the SINGLE param_4==-1 nTargetXMaybe reroll -- matched by routing 'P' to 'S's tail
// with a `goto FixTargetX`, the one place the original shares that fixup), the
// divisor-zero-guarded inclusive-range random draw (RAND_RANGE_MAYBE below -- the
// guard + lo-fallback shape repeats identically at all 14 draw sites, incl. the
// self-cancelling +hsX-hsX / hsY-hsY pairs, so it was a shared macro/inline in the
// original), and the effect-spawner tail + epilogue.
//
// Residual is the documented VC5 /Og allocation/scheduling tie-break class: (1) the
// case-'R' y-half register cluster -- the original keeps (VH-nH) in EBP and nY in EBP
// with hsY in EDX (guard reassociated as VH-hsY-nH+1 from the VH register, lo-fallback
// as an explicit MOV EBP,EDX block, jnz-to-rand layout), this build shares nY/hsY in
// EBX (guard CSEs the compare's VH-nH, the hsY fallback folds away, per-arm je/jne
// polarity inverted) -- one root allocation choice cascading; (2) case-'P' entry
// desc-load placement (before vs after the param_5 test) and its else-arm's per-
// statement pKindDesc reload; (3) per-arm dwViewportWidth load scheduling (original:
// EAX=[VW] hoisted before the y-push pair); (4) case-'U' entry store/test/desc-preload
// order (its 'D' twin, identical source, matches byte-for-byte); (5) effect-tail
// EBX=[this+0x9c] load scheduling; (6) scattered LEA-vs-ADD and CDQ-placement
// tie-breaks. Variants tried (score path 1575540 -> 381480): single call per case with
// nX/nY locals (first draft, 1.58M), per-arm textual Reposition duplication (big win),
// macro ternary polarity `!= 0 ? rand : lo` vs `== 0 ? lo : rand` (flipped form is
// correct -- big win), fixup textually duplicated in 'P' and 'S' (partial /Og merge,
// worse) vs `goto FixTargetX` into 'S's tail (big win), fixup condition `< 0` vs
// `>= 0` (latter correct -- big win), per-arm nX local (worse), y-compare operand swap
// (worse), U/D compare operand swap (worse), (int) cast removal (neutral), U-only pD
// caching local (worse), R y-half Ghidra-polarity arms (worse), nY scope/declaration
// moves and per-arm y locals (neutral), spawn-arg locals (neutral). The leftovers look
// TU-context-bound (/Og global allocation), so this parks. PARKED.
AnimEffectObj0x477a90::AnimEffectObj0x477a90(int nResourceId, short nSubFrameArg,
                                             char chPlacementMode, int nTargetX, int nTargetY)
    : AnimDescRefHotspotPartial(nResourceId, nSubFrameArg, 0, 0)
{
    pEffectMaybe = 0;
    bUnk0x94Maybe = 1;
    chPlacementModeMaybe = (char)toupper(chPlacementMode);
    nTargetXMaybe = nTargetX;
    nTargetYMaybe = nTargetY;
    wUnk0x8aMaybe = nSubFrameArg;
    if (pKindDesc == 0) {
        return;
    }
    CursorAnimFrameEntry *pEntry = pKindDesc->paFrameEntries + nSubFrame;
    if (pEntry->nStartFrame == pEntry->nEndFrame) {
        nAnimCooldownUntil = pEntry->nCooldownTicks + g_dwGameTick;
    }
    char bUnk;
    if (pKindDesc->categoryByte == 8) {
        bUnk = pKindDesc->bFootprintXSteps;
    } else {
        bUnk = (char)(rand() % 3 + 1);
    }
    bUnk0x94Maybe = bUnk;
    int nY;
    switch (chPlacementModeMaybe) {
    case 'C': // viewport center, hotspot-adjusted
        AnimDescRefHotspotPartial::RepositionWithHotspot(
            g_worldBoard.dwViewportCenterXMaybe - pKindDesc->hotspotX,
            g_worldBoard.dwViewportCenterYMaybe - pKindDesc->hotspotY);
        break;
    case 'R': // random position fully inside the viewport
        if (pKindDesc->hotspotY <= (int)(g_worldBoard.dwViewportHeightMaybe - pKindDesc->nativeHeight)) {
            nY = RAND_RANGE_MAYBE(pKindDesc->hotspotY,
                                  g_worldBoard.dwViewportHeightMaybe - pKindDesc->nativeHeight);
        } else {
            nY = RAND_RANGE_MAYBE(g_worldBoard.dwViewportHeightMaybe - pKindDesc->nativeHeight,
                                  pKindDesc->hotspotY);
        }
        if (pKindDesc->hotspotX <= g_worldBoard.dwViewportWidth) {
            AnimDescRefHotspotPartial::RepositionWithHotspot(
                RAND_RANGE_MAYBE(pKindDesc->hotspotX, g_worldBoard.dwViewportWidth) - pKindDesc->hotspotX,
                nY - pKindDesc->hotspotY);
        } else {
            AnimDescRefHotspotPartial::RepositionWithHotspot(
                RAND_RANGE_MAYBE(g_worldBoard.dwViewportWidth, pKindDesc->hotspotX) - pKindDesc->hotspotX,
                nY - pKindDesc->hotspotY);
        }
        break;
    case 'P': // past the right edge (x = viewport width), random y
        if (nTargetY < 0) {
            if (pKindDesc->hotspotY <= (int)(g_worldBoard.dwViewportHeightMaybe - pKindDesc->nativeHeight)) {
                AnimDescRefHotspotPartial::RepositionWithHotspot(g_worldBoard.dwViewportWidth,
                    RAND_RANGE_MAYBE(pKindDesc->hotspotY,
                                     g_worldBoard.dwViewportHeightMaybe - pKindDesc->nativeHeight) - pKindDesc->hotspotY);
            } else {
                AnimDescRefHotspotPartial::RepositionWithHotspot(g_worldBoard.dwViewportWidth,
                    RAND_RANGE_MAYBE(g_worldBoard.dwViewportHeightMaybe - pKindDesc->nativeHeight,
                                     pKindDesc->hotspotY) - pKindDesc->hotspotY);
            }
        } else {
            AnimDescRefHotspotPartial::RepositionWithHotspot(g_worldBoard.dwViewportWidth,
                                                          pKindDesc->hotspotY + nTargetY);
        }
        goto FixTargetX;
    case 'S': // past the left edge (x = -nativeWidth), random y
        if (nTargetY < 0) {
            if (pKindDesc->hotspotY <= (int)(g_worldBoard.dwViewportHeightMaybe - pKindDesc->nativeHeight)) {
                AnimDescRefHotspotPartial::RepositionWithHotspot(-(int)pKindDesc->nativeWidth,
                    RAND_RANGE_MAYBE(pKindDesc->hotspotY,
                                     g_worldBoard.dwViewportHeightMaybe - pKindDesc->nativeHeight) - pKindDesc->hotspotY);
            } else {
                AnimDescRefHotspotPartial::RepositionWithHotspot(-(int)pKindDesc->nativeWidth,
                    RAND_RANGE_MAYBE(g_worldBoard.dwViewportHeightMaybe - pKindDesc->nativeHeight,
                                     pKindDesc->hotspotY) - pKindDesc->hotspotY);
            }
        } else {
            AnimDescRefHotspotPartial::RepositionWithHotspot(-(int)pKindDesc->nativeWidth,
                                                          nTargetY - pKindDesc->hotspotY);
        }
FixTargetX:
        if (nTargetX == -1) {
            if (g_worldBoard.dwViewportWidth >= 0) {
                nTargetXMaybe = RAND_RANGE_MAYBE(0, g_worldBoard.dwViewportWidth);
            } else {
                nTargetXMaybe = RAND_RANGE_MAYBE(g_worldBoard.dwViewportWidth, 0);
            }
        }
        break;
    case 'U': // random x across the top edge (y = viewport height)
        bUnk0x94Maybe = 1;
        if (nTargetX < 0) {
            if (-pKindDesc->hotspotX <= (int)(g_worldBoard.dwViewportWidth + pKindDesc->hotspotX)) {
                AnimDescRefHotspotPartial::RepositionWithHotspot(
                    RAND_RANGE_MAYBE(-pKindDesc->hotspotX,
                                     g_worldBoard.dwViewportWidth + pKindDesc->hotspotX),
                    g_worldBoard.dwViewportHeightMaybe);
            } else {
                AnimDescRefHotspotPartial::RepositionWithHotspot(
                    RAND_RANGE_MAYBE(g_worldBoard.dwViewportWidth + pKindDesc->hotspotX,
                                     -pKindDesc->hotspotX),
                    g_worldBoard.dwViewportHeightMaybe);
            }
        } else {
            AnimDescRefHotspotPartial::RepositionWithHotspot(nTargetX - pKindDesc->hotspotY,
                                                          nTargetY - pKindDesc->hotspotY);
        }
        break;
    case 'D': // 'U's twin with y = 0 (bottom edge)
        bUnk0x94Maybe = 1;
        if (nTargetX < 0) {
            if (-pKindDesc->hotspotX <= (int)(g_worldBoard.dwViewportWidth + pKindDesc->hotspotX)) {
                AnimDescRefHotspotPartial::RepositionWithHotspot(
                    RAND_RANGE_MAYBE(-pKindDesc->hotspotX,
                                     g_worldBoard.dwViewportWidth + pKindDesc->hotspotX),
                    0);
            } else {
                AnimDescRefHotspotPartial::RepositionWithHotspot(
                    RAND_RANGE_MAYBE(g_worldBoard.dwViewportWidth + pKindDesc->hotspotX,
                                     -pKindDesc->hotspotX),
                    0);
            }
        } else {
            AnimDescRefHotspotPartial::RepositionWithHotspot(nTargetX - pKindDesc->hotspotY,
                                                          nTargetY - pKindDesc->hotspotY);
        }
        break;
    case 'W': // explicit position, hotspot-adjusted
        AnimDescRefHotspotPartial::RepositionWithHotspot(nTargetX - pKindDesc->hotspotX,
                                                      nTargetY - pKindDesc->hotspotY);
        break;
    }
    if (pKindDesc->nShadowId > 0) {
        if (pKindDesc->nShadowOffsetY > 0) {
            nEffectOffsetXMaybe = pKindDesc->nShadowOffsetX;
            nEffectOffsetYMaybe = pKindDesc->nShadowOffsetY;
        } else {
            nEffectOffsetXMaybe = pKindDesc->nShadowOffsetX;
            nEffectOffsetYMaybe = rand() % 0x1f + 0x28;
        }
        pEffectMaybe = (AnimEffectObj0x477a90 *)DAT_004fd220.EffectSpawner_SpawnSimpleMaybe(
            pKindDesc->nShadowId, wUnk0x8aMaybe,
            rect.left + nEffectOffsetXMaybe, rect.top + nEffectOffsetYMaybe);
    }
}

// FUNCTION: LOCO 0x423560
// The per-frame step, and the whole of this class's behaviour after construction. Returns 1 for
// "I am finished" -- EffectSpawner's tick (0x423d70) walks both of its effect collections, calls
// this on every live entry and removes the ones that say yes.
//
// chPlacementModeMaybe picks the motion, and the four arms divide as:
//
//   'D'/'U'  drift VERTICALLY, with the current subframe's PARITY choosing the direction --
//            odd rises (rect.top - bUnk0x94Maybe, until rect.bottom leaves the top of the
//            viewport), even falls (rect.top + bUnk0x94Maybe, until rect.top passes
//            dwViewportHeightMaybe). Running out of viewport on either side is what expires it.
//            The parity test is what makes nSubFrame's SIGNEDNESS visible: the original spends a
//            full `cdq/xor/sub/and 1/xor/sub` on it, which is MSVC's signed `% 2` and not the
//            single `and eax,1` an unsigned value would have given.
//   'P'/'S'  a horizontal PAIR that hands off to each other. Each walks one bUnk0x94Maybe step
//            per tick ('P' left, 'S' right) until it reaches nTargetXMaybe, then flips
//            chPlacementModeMaybe to the OTHER letter, clears the target and steps the frame set
//            by one or two -- one when the current subframe IS the frame set, two when the
//            subframe's own bounce sentinel is spent and its end frame is already showing.
//            That is a sprite turning around at the end of a patrol. Running off its own side of
//            the viewport (past -nativeWidth going left, past dwViewportWidth going right)
//            expires it instead.
//   default  the stationary case: expire as soon as the frame set has played out
//            (nAnimValueCache has reached the subframe's nEndFrame), the bounce sentinel is
//            spent, the anim cooldown has passed, AND any sound channel is reclaimable. All four
//            must agree, which is how a one-shot effect outlives its own animation just long
//            enough for its sound to finish.
//
// Whatever it did, an effect that is NOT finished drags its companion effect along behind it
// (pEffectMaybe, the smoke/dust spawned by the constructor) at the fixed offset pair.
//
// ⚠ The `nSubFrame == 0` test really is written twice on the 'D'/'U' path -- once as the arm's
// own guard and once inside the even sub-arm, where it is unreachable. VC5 cross-jumps the two
// identical `{ AdvanceAnimFrameMaybe(); break; }` tails onto one block at 0x423788, which is the
// only reason the redundancy is visible at all. Reproduced rather than tidied.
//
// PARTIAL -- DIFF(639), 702 B compiled against a true COMDAT extent of 736 (0x423560..0x423840;
// Ghidra's `Body` span stops at 0x423811 and EXCLUDES the two trailing switch tables, so score
// this one with `--len 736`), insns 254/245. Content-complete: CALL PARITY is exact on three of
// the four targets -- RepositionWithHotspot 5/5, ReleaseChannelAndDispatch 4/4, IsReclaimable
// 1/1 -- and every block, constant, field and branch sense agrees. The whole residual is two
// stacked VC5 /Og classes, neither source-steerable:
//   (1) CROSS-JUMPING. This source spells AdvanceAnimFrameMaybe() at 14 sites; the original
//       emits 10, having tail-merged four identical `{ AdvanceAnimFrameMaybe(); break; }`
//       blocks onto shared copies (the two `nSubFrame == 0` arms both land on 0x423788). This
//       compile merges none of them, and that 4-site gap is essentially the whole instruction
//       excess. Same class as the v378 cross-jumped duplicate tail.
//   (2) ZERO-REGISTER + a spilled flag. The original materialises 0 in ebx once and compares
//       against it throughout (`cmp ecx,ebx`, `cmp [esi+0x14],ebx`), keeping bExpired in the
//       stack slot its `push ecx` prologue reserves; this compile keeps bExpired in bl and uses
//       `test reg,reg`. Two callee-saved registers plus a stack byte against three registers.
//       Same class already parked on 0x456150 and 0x462e90.
// One lever DID land and is baked in: bUnk0x94Maybe is UNSIGNED (see src/AnimEffectObj.h) --
// with a signed char the four step sites emit `movsx` where the original zero-extends.
char AnimEffectObj0x477a90::TickMaybe() {
    char bExpired = 0;
    switch (chPlacementModeMaybe) {
    case 'D':
    case 'U':
        if (nSubFrame == 0) {
            AdvanceAnimFrameMaybe();
            break;
        }
        if (nSubFrame % 2 != 0) {
            if (rect.bottom > 0) {
                RepositionWithHotspot(rect.left, rect.top - bUnk0x94Maybe);
                AdvanceAnimFrameMaybe();
                break;
            }
        } else {
            if (nSubFrame == 0) { // sic: unreachable, see the note above
                AdvanceAnimFrameMaybe();
                break;
            }
            if (rect.top < g_worldBoard.dwViewportHeightMaybe) {
                RepositionWithHotspot(rect.left, rect.top + bUnk0x94Maybe);
                AdvanceAnimFrameMaybe();
                break;
            }
        }
        bExpired = 1;
        AdvanceAnimFrameMaybe();
        break;

    case 'P':
        if (nTargetXMaybe != 0 && rect.right < nTargetXMaybe) {
            int nFrameSet = wUnk0x8aMaybe;
            if (nFrameSet < (int)pKindDesc->nFrameSetCount - 2) {
                if (nSubFrame == nFrameSet) {
                    ReleaseChannelAndDispatch(nFrameSet + 1);
                    AdvanceAnimFrameMaybe();
                    break;
                }
                CursorAnimFrameEntry *paEntries = pKindDesc->paFrameEntries;
                if (paEntries[nSubFrame].nBounceSoundId != -1 ||
                    nAnimValueCache != paEntries[nSubFrame].nEndFrame) {
                    AdvanceAnimFrameMaybe();
                    break;
                }
                ReleaseChannelAndDispatch(nFrameSet + 2);
            }
            chPlacementModeMaybe = 'S';
            nTargetXMaybe = 0;
            AdvanceAnimFrameMaybe();
            break;
        }
        if (hotspotPosX > -(int)pKindDesc->nativeWidth) {
            RepositionWithHotspot(rect.left - bUnk0x94Maybe, rect.top);
            AdvanceAnimFrameMaybe();
            break;
        }
        bExpired = 1;
        break;

    case 'S':
        if (nTargetXMaybe != 0 && nTargetXMaybe < rect.left) {
            if (wUnk0x8aMaybe > 1) {
                int nFrameSet = wUnk0x8aMaybe;
                if (nSubFrame == nFrameSet) {
                    ReleaseChannelAndDispatch(nFrameSet - 1);
                    AdvanceAnimFrameMaybe();
                    break;
                }
                CursorAnimFrameEntry *paEntries = pKindDesc->paFrameEntries;
                if (paEntries[nSubFrame].nBounceSoundId != -1 ||
                    nAnimValueCache != paEntries[nSubFrame].nEndFrame) {
                    AdvanceAnimFrameMaybe();
                    break;
                }
                ReleaseChannelAndDispatch(nFrameSet - 2);
            }
            chPlacementModeMaybe = 'P';
            nTargetXMaybe = 0;
            AdvanceAnimFrameMaybe();
            break;
        }
        if (hotspotPosX < g_worldBoard.dwViewportWidth) {
            RepositionWithHotspot(rect.left + bUnk0x94Maybe, rect.top);
            AdvanceAnimFrameMaybe();
            break;
        }
        bExpired = 1;
        break;

    default:
        {
            CursorAnimFrameEntry *paEntries = pKindDesc->paFrameEntries;
            if (nAnimValueCache < paEntries[nSubFrame].nEndFrame ||
                paEntries[nSubFrame].nBounceSoundId != -1 ||
                (int)g_dwGameTick <= nAnimCooldownUntil ||
                (pDSoundChannel != 0 && !pDSoundChannel->IsReclaimable())) {
                AdvanceAnimFrameMaybe();
                break;
            }
        }
        bExpired = 1;
        break;
    }

    if (bExpired == 0 && pEffectMaybe != 0) {
        pEffectMaybe->RepositionWithHotspot(rect.left + nEffectOffsetXMaybe,
                                           rect.top + nEffectOffsetYMaybe);
    }
    return bExpired;
}

// The slot 7/9/10 trio: propagate the call to the spawned companion effect first, then run
// this object's own inherited behaviour. See the declarations in src/AnimEffectObj.h.

// FUNCTION: LOCO 0x423840
void AnimEffectObj0x477a90::ReleaseChannelAndDispatch(unsigned int arg)
{
    if (pEffectMaybe != 0) {
        pEffectMaybe->ReleaseChannelAndDispatch(arg);
    }
    AnimDescRefObj0x477488::ReleaseChannelAndDispatch(arg);
}

// FUNCTION: LOCO 0x423870
void AnimEffectObj0x477a90::AdvanceAnimFrameMaybe()
{
    if (pEffectMaybe != 0) {
        pEffectMaybe->AdvanceAnimFrameMaybe();
    }
    AnimDescRefObj0x477488::AdvanceAnimFrameMaybe();
}

// FUNCTION: LOCO 0x423890
void AnimEffectObj0x477a90::SetReadyStateMaybe(bool bIsReady)
{
    if (pEffectMaybe != 0) {
        pEffectMaybe->SetReadyStateMaybe(bIsReady);
    }
    AnimDescRefObj0x477488::SetReadyStateMaybe(bIsReady);
}
