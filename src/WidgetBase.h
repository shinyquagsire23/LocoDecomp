// Shared base hierarchy for the Widget family: RectFlagObj0x477820 (root) ->
// AnimDescRefObj0x477488 -> WidgetBaseObj0x4784c8 -> {WidgetTagObj0x478378,
// WidgetPickerObj0x477cc8, BuildToolButton, SelectedObjWidgetMaybe, ...}. This is the
// canonical home for the base classes (see docs/subsystems.md's widget-family table); each
// derived leaf gets its own file (e.g. WidgetPicker.h/.cpp).
//
// Only the members actually needed by an already-transcribed caller are given real bodies
// here -- this hierarchy has ~60 combined vtable slots across the family and most leaf
// classes' own overrides are still unread. Untranscribed virtuals are declared (so derived
// classes' own NEW slots land at the right vtable index) but left without a definition,
// same as any other not-yet-transcribed sibling function call.
#pragma once

#include <windows.h>

struct DSoundChannel;
struct SoundBankEntry;
class BigObj;
class CursorDesc;
class MenuNodeObj0x477568;

// Tiny polymorphic root: vtable, a manual-RTTI-style type tag (stomped by every derived
// ctor with its own literal), a screen-space RECT dirty-marked via MarkDirty, a
// validity flag, and two optional callback pointers (confirmed dead/unused -- no writer
// anywhere in .text, see docs/subsystems.md v66).
//
// ✅ LANDED v560 AT NO COST, and the -256 B pricing below is now STALE HISTORY -- read the
// correction before acting on any of it. Three of the four bodies (ctor 0x4369d0 46 B,
// MarkDirty 0x436ab0 41 B, TryInvokeCallbackA 0x436ae0 32 B) are in src/WidgetBase.cpp and all
// three are EXACT. Measured repo-wide: EXACT 173119 -> 173238 B, +119 B / +3 funcs, and the
// per-file table moved in exactly one place. **The 407 B toll did not happen.**
//   ⇒ AdvanceAnimFrameMaybe (0x405c40) had ALREADY lost its EXACT to something else in the ~20
//     sessions since v540, so the toll this block was withheld against no longer existed to pay.
//     The withholding decision was correct WHEN TAKEN and simply outlived its measurement.
//   ⇒ Process lesson, the same one docs/PARKED.md's v542 correction teaches from the other side:
//     a carried-forward toll is a measurement with an expiry date. Re-measure a withheld bundle
//     before spending a session on the lever that was supposed to unlock it -- and before
//     believing it is still withheld at all. Re-running it here cost one compile.
//   TryInvokeCallbackB (0x436b00, 32 B) is still NOT landed, for an unrelated reason: its root
//   declaration below is the wrong `void()` (see the model finding further down), and correcting
//   that entangles WidgetBaseObj0x4784c8's real slot-5 override at 0x454a60, which is currently
//   modeled as the ordinary member HitTestAndLocalizeMaybe. That is a model edit, not a pricing
//   one. Its body is byte-for-byte TryInvokeCallbackA's against +0x20 instead of +0x1c.
//
// ⛔ HISTORICAL (v540) -- the pricing that kept these four out, superseded by the block above:
// this class's four REMAINING bodies -- the ctor 0x4369d0 (46 B),
// MarkDirty 0x436ab0 (41 B), TryInvokeCallbackA 0x436ae0 (32 B) and TryInvokeCallbackB 0x436b00
// (32 B) -- were all transcribed and were all EXACT on the FIRST compile, 151 B in total. They
// are withheld anyway because ANY of them, landed in src/WidgetBase.cpp where the class's other
// four bodies already live, costs AnimDescRefObj0x477488::AdvanceAnimFrameMaybe (0x405c40) its
// whole 407 B EXACT. Net -256 B. Do NOT re-run without a new lever; the transcriptions
// themselves are correct and are recorded below so a later session only has to re-type them.
//   - ⚠ These four share the 0x405c40 toll with 0x454630 (75 B), 0x4545a0 (144 B) and 0x405a50
//     (96 B), and v540/v541 claimed that BUNDLING all seven turns the toll net positive (+59 B).
//     That was an ARITHMETIC ERROR, corrected v542: 0x4545a0 and 0x405a50 land PARTIAL, not
//     EXACT, so the bundle buys 226 B of new EXACT against -407 B = -181 B. It is net-NEGATIVE
//     and stays withheld. See the corrected v540 section in docs/PARKED.md before re-pricing.
//   - The dial is NOT a declaration-count parity like src/AppWindow.h's. Measured: the ctor
//     ALONE breaks it, MarkDirty ALONE breaks it, all four together break it identically, and
//     even the header-only model fix below (with no body added at all) breaks it. A spare
//     declaration added to this header changes NOTHING -- the DIFF is byte-identical with and
//     without it -- so there is no parity currency to spend here (contrast the ✅ block on
//     RepositionWithHotspot below, where there was).
//   - 0x405c40's residual is CONTENT-COMPLETE and intrinsic: 144/144 instructions, and the
//     whole diff is one eax<->edx base/index coin-flip on `pKindDesc->paFrameEntries +
//     nSubFrame` propagating through four later loads. Two source-shape probes (the
//     `&paFrameEntries[nSubFrame]` array form; caching pKindDesc in a local first) produce a
//     BYTE-IDENTICAL DIFF(314). Register coin-flip class, per docs/CODEGEN.md.
//
// ⭐ Two real model findings came out of that work and ARE kept, because they cost nothing to
// know and the bodies confirm them:
//   - TryInvokeCallbackB's slot signature is char(int, int), NOT the void() declared below.
//     0x436b00 is byte-for-byte 0x436ae0's shape against +0x20 instead of +0x1c, `ret 8` and
//     all. The declaration is left as-is ONLY because changing it is part of the withheld
//     header edit above; fix it in the same commit that ever lands these bodies.
//   - Both callback members are `void (__cdecl *)(int, int)`, not `void *`: both invokers do
//     `push arg2; push arg1; call eax; add esp, 8` and discard the result. Neither member has
//     a single consumer anywhere in src/, so retyping them is free of call-site churn.
class RectFlagObj0x477820 {
public:
    int nTypeTag;
    RECT rect;
    bool bValid;
    void *pCallbackA;
    void *pCallbackB;

    RectFlagObj0x477820();
    virtual ~RectFlagObj0x477820();
    virtual void MarkDirty();
    // slot 2 -- 0x436a10, the root base's own rect hit-test (body in WidgetBase.cpp; this IS
    // the function Phase 2 modeled as the "unrelated RectObj0x436a10"). Real signature
    // recovered 2026-07-16 from TestMenuCommand's call site (WidgetPicker.cpp): 2 explicit
    // args (screen x/y, per the call-site's own untyped param forwarding), byte-tested
    // return (hit/no-hit).
    virtual char Contains(int x, int y);
    // slot 3 -- 0x436a60, real (not placeholder) body: re-centers rect at (x,y) keeping
    // its width/height, dirty-marking (own slot 1) both the old and new position. Body in
    // WidgetBase.cpp, EXACT.
    //
    // ✅ AnimDescRefObj0x477488 OVERRIDES this at 0x405c00, and as of v486 it is DECLARED as doing
    // so. Both that override and WidgetBaseObj0x4784c8's further override at 0x454820 are landed
    // and EXACT (61 B + 98 B, both on the first compile, exactly as five sessions of parked rows
    // predicted), and the declaration ALSO fixed a real byte-invisible defect: the class-qualified
    // `AnimDescRefObj0x477488::RepositionWithHotspot(...)` at both sites in
    // PlacementCursorMaybe::SetTypeMaybe used to resolve to the INHERITED root member and emit a
    // call to 0x436a60 where the original calls 0x405c00. Nothing could see it -- verify.py masks
    // relocations, and no lint models overload resolution.
    //
    // ⚠⚠ HOW IT WENT FROM -852 B TO +302 B WITHOUT THE LEVER ITSELF CHANGING, because this is the
    // real lesson and it generalises. v445 priced this declaration at -85 B and v449's repo-wide
    // sweep at -852 B, the difference being a third victim nobody had priced:
    // WorldBoardMaybe::FindNearestObjOfCategoryMaybe (0x457ce0, 951 B). All of those measurements
    // were correct when taken. What changed is that 0x457ce0 answers to a PARITY on the combined
    // declaration count of more than one shared header -- v448 already knew it as a parity canary
    // for src/AppWindow.h -- and v486 happened to be holding a spare, freely-removable declaration
    // in src/Obj0x477798Family.h (`_v15`). Dropping that ONE declaration in the same commit as
    // adding this one keeps the parity, and every recorded victim then survives: 0x457ce0 holds,
    // 0x452b00 holds, and 0x458310 does not merely survive but GAINS (TilePlacedObj.cpp +143 B).
    // ⇒ A "net-negative header change" is only net-negative AT A GIVEN PARITY. Before believing a
    //   parked row that prices one, check whether some other header can absorb the parity change
    //   -- a true declaration you are free to write or not write is a currency, and this repo has
    //   several. See docs/CODEGEN.md's v486 note; the counterpart note is on `_v15` in
    //   src/Obj0x477798Family.h.
    // Declaration POSITION was tested in v445 (after the dtor vs. last member of the class) and
    // makes no difference -- it is the declaration's existence that moves the parity.
    virtual void RepositionWithHotspot(int x, int y);
    // slot 4 -- the ROOT's own body is confirmed dead (no writer/caller reaches it), but the
    // SLOT's real contract across the family is char(int x, int y): WidgetBaseObj0x4784c8's
    // override at 0x4549e0 (HitTestAndLocalizeSecondaryMaybe below) has exactly that shape, and
    // BuildToolButton::HitTestMaybe dispatches BOTH of its sub-regions through this slot with a
    // screen point, using the returned char as its own result. Signature filled in 2026-07-25.
    virtual char TryInvokeCallbackA(int x, int y);
    virtual void TryInvokeCallbackB(); // slot 5 -- confirmed dead

    // Ordinary (non-virtual) member, 0x436a40. Screen -> widget-local coordinate transform,
    // returning the localized point BY VALUE: {x - rect.left, y - rect.top}. Body in
    // WidgetBase.cpp. The 8-byte by-value return is why the original's signature reads
    // `int *f(int *, int, int)` in a decompiler -- that leading `int *` is MSVC's hidden
    // return-buffer pointer (returned in eax), not a real out-param.
    POINT ComputeLocalPos(int x, int y);
};

// Reusable positioned-descriptor+sound+anim component, embedded/reused across many
// unrelated subsystems (BuildToolButton, CarNetObj, EffectSpawner, DecorActorBase,
// PlacementCursorMaybe, and this family). Owns a TileKind descriptor ptr, a DSoundChannel*,
// anim-tick/cooldown/phase fields, and a default pos + short category string.
class AnimDescRefObj0x477488 : public RectFlagObj0x477820 {
public:
    bool bReady;
    int nSubFrame;
    int nBlitFlags;
    RECT rectViewport;
    BigObj *pKindDesc;
    SoundBankEntry *pSoundEntry; // +0x44 -- cached lookup result (SoundBank::
                                        //   SoundBank_LookupEntryById), released via
                                        //   SoundBankEntry::Release in the dtor
    DSoundChannel *pDSoundChannel;
    // +0x4c/+0x50 -- the hotspot-adjusted WORLD position, in pixels: RepositionWithHotspot
    // (slot 3, 0x405c00) sets them to pKindDesc->hotspotX/Y + the incoming (x,y) right after
    // recentering `rect`. This is the anchor point the world-tile graph is keyed on: every
    // consumer that asks "where is this object standing" reads THESE, not rect (e.g.
    // WalkerActor's whole path-stepping loop in src/DecorActor.cpp, and TilePlacedObj's own
    // neighbour lookups). Distinct from posX/posY (+0x74/+0x78), which are the ctor-supplied
    // DEFAULT position and are never updated afterwards.
    int hotspotPosX;
    int hotspotPosY;
    // +0x54 -- UNSIGNED (matches Ghidra's own `dword` typing, corrected 2026-07-26): the only
    // site that converts it to floating point, BuildToolButton's slide-progress ramp, emits
    // the `mov [t],v; mov [t+4],0; fild qword [t]` zero-extending sequence MSVC uses for
    // `unsigned int` -> double, not the plain `fild dword` an `int` would give.
    unsigned int nAnimValueCache;
    int nAnimCooldownUntil;
    unsigned int nSoundId;      // +0x5c -- change-detection cache of the last-set sound-bank id;
                                       // paired 1:1 with pSoundEntry
    int dwSoundResumeTick;      // +0x60 -- game-tick deadline (g_dwGameTick + jitter) after which
                                       // the channel is resumed/retriggered
    // +0x64/+0x68 -- real fields, but this class's own ctor never touches them; DecorActorBase's
    // ctor stores its spawn TileKind id into +0x64 and zeroes +0x68. Promoted off the Unk rung
    // 2026-07-25 by the second, independent consumer the earlier note was waiting for:
    // DecorObjMgrMaybe::ApplySeqRecordToActorsMaybe (0x435580, src/DecorActor.cpp) restores
    // +0x64 through SetDescriptor when a seq-record retarget invalidates the actor -- so the two
    // writers agree it is the actor's ORIGINAL/spawn descriptor id -- and treats a non-zero
    // +0x68 as "this actor is already claimed", arming it to g_dwGameTick + the record's own
    // delay after a reward fires. ⚠ That is still the ONLY consumer of +0x68 outside the ctor,
    // so the seq-reward reading of it is a hypothesis about a field on a SHARED base class:
    // a wider "busy until tick" role for the rest of the family would not contradict anything
    // seen so far.
    int nSpawnDescriptorIdMaybe;
    int dwSeqRewardUntilMaybe;
    int nAnimTickCounter;
    bool bAnimCoolingDownMaybe;
    int posX;
    int posY;
    // +0x7c .. +0x86 -- ELEVEN bytes, not twelve (the class still ends at 0x88 either way, so
    // sizeof could never tell them apart). Pinned 2026-07-25 by the one construct that renders a
    // member's exact width: TilePlacedObj's implicit copy constructor, inlined into
    // Obj0x477758Base::SetCopyAtMaybe (0x412140), copies this member as dword+dword+WORD+BYTE.
    // A [12] spelling gives dword+dword+dword there and breaks the match. Consistent with
    // SetCategoryIfPrintable, which copies at most 10 chars and terminates at +0x86.
    char szCategoryName[11];

    // Default args pinned by WorldActionCursor's array element thunk (0x458af0), which passes
    // (-1,-1,0,0) -- so those ARE the original's defaults. Adding them is a shared-header dial
    // touch: it costs WorldBoardMaybe's 0x457ce0 FindNearestObjOfCategoryMaybe its EXACT
    // (measured v511, CODEGEN #78) and must be landed BUNDLED with the 0x4589b0 ctor.
    AnimDescRefObj0x477488(int nResourceId = -1, short nSubFrameArg = -1, int nPosX = 0, int nPosY = 0);
    virtual ~AnimDescRefObj0x477488();
    // slot 3 (+0x0c) -- 0x405c00, this class's override of the root's 0x436a60. LANDED v486,
    // after five sessions of being held back; see the ⛔/✅ block on the root's own declaration
    // above for the whole history and for what changed. Recenters `rect` through the base, then
    // sets the hotspot-adjusted world anchor (+0x4c/+0x50) and moves any live sound channel to
    // the new position -- so the anchor and the audio follow the widget in one step, which is
    // why every "where is this object standing" consumer can read hotspotPosX/Y and be current.
    virtual void RepositionWithHotspot(int x, int y);
    // Loads/clears the descriptor for nResourceId and recomputes rect/subframe state.
    // vtable slot 6 -- overridden by WidgetBaseObj0x4784c8::SetDescriptor below.
    virtual unsigned char SetDescriptor(int nResourceId, int nSubFrameArg, char bForce);
    // vtable slot 7 (+0x1c) -- 0x405a20. Releases pDSoundChannel (if any, clearing
    // nSoundId) then dispatches vtable slot 14 (+0x38, still unidentified -- likely a subframe/
    // anim-state setter given its callers always pass a small direction/subframe-shaped value).
    // EXACT MATCH (v211, src/WidgetBase.cpp). (Modeled as an ordinary member before
    // 2026-07-22; Ghidra's vtable read at 0x477488+0x1c = 0x405a20 pins it as slot 7.)
    virtual void ReleaseChannelAndDispatch(unsigned int arg);
    // (0x405e20 is VIRTUAL slot 13 -- declared below with the rest of the vtable, no longer a
    // second non-virtual spelling of the same address. See the slot-13 comment there.)
    // NOTE: this class's ordinary member EnsureSoundPlayingMaybe (0x405ab0) is deliberately NOT
    // declared here -- see the per-consumer view in src/PeerTrainNode.cpp for why (adding it to
    // this shared header measurably rotated DPlaySessionMgr.cpp's codegen).
    // vtable slot 8 (+0x20) default body (0x405de0) -- reached polymorphically through this
    // slot by CarNetObj::SetStateArgMaybe (src/CarNetObj.h)
    // and DPlaySessionMgr.cpp's own widget-list GetItemImpl probe whenever a derived car/widget
    // doesn't override it. Applies a newly-computed animation-frame index: caches it
    // (nAnimValueCache), recomputes rectViewport's horizontal bounds from the descriptor's own
    // per-frame width (pKindDesc->nativeWidth), and optionally re-dirties via MarkDirty (slot 1).
    // Confirmed by its lone caller-in-waiting, AnimDescRefObj0x477488::AdvanceAnimFrame (0x405c40,
    // not yet transcribed): computes a new bounce frame value then forwards it here via the same
    // vtbl+0x20 virtual call, only when it differs from nAnimValueCache.
    virtual void SetAnimFrame(int nFrame, char bMarkDirty);
    // vtable slot 9 (+0x24) -- 0x4061b0. Sets bReady, dispatches MarkDirty (slot 1), then
    // resumes/pauses pDSoundChannel (if any) to match the new ready state. Called by
    // CarNetObjAnchorPartial::CompleteViewportExtendMaybe/CheckCarLeftViewportMaybe (src/PeerTrainNode.cpp) through the
    // real class hierarchy -- not overridden by CarNetObj (confirmed via its own vtable dump).
    // The argument is `bool` and NOT this family's usual `char`, pinned 2026-07-27 (v446) the
    // same way slot 16's return type was: the body's `bReady = bIsReady` is a bare `mov
    // [esi+0x24],bl` with no 0/1 normalization, and since bReady is itself a `bool` the source
    // can only be a straight bool-to-bool copy. With `char` the function is DIFF(38) at 54
    // bytes -- exactly the extra `test bl,bl; setne bl` (and cl says so out loud, C4800).
    virtual void SetReadyStateMaybe(bool bIsReady);
    // slot 10 (+0x28) -- 0x405c40, named 2026-07-25 (was `_v10`; a rename only, the
    // declaration count is unchanged -- see this header's own EnsureSoundPlayingMaybe note for
    // why that matters). Computes a new bounce/anim frame value and forwards it through slot 8
    // (SetAnimFrame) when it differs from nAnimValueCache. Called NON-virtually, as
    // `AnimDescRefObj0x477488::AdvanceAnimFrameMaybe()`, by DecorActorBase::TickIdleDecayMaybe
    // (src/DecorActor.cpp).
    virtual void AdvanceAnimFrameMaybe();
    // slot 11 (+0x2c) -- 0x405e60, named 2026-07-25 (was `_v11`; a rename + signature fill-in,
    // the declaration count is unchanged -- see this header's own EnsureSoundPlayingMaybe note
    // for why that matters). Blits this object's current anim frame clipped to the passed RECT.
    // Overridden by WidgetBaseObj0x4784c8 at 0x454900 (chains this, then propagates the dirty
    // rect to rectBMaybe/rectCMaybe), so every derived widget reached through this slot does
    // the composite blit; BuildToolButton::BlitAllRegionsMaybe fans it out by hand.
    // Takes the clip RECT BY VALUE (MSVC copies it with `sub esp,0x10` + four stores rather
    // than four individual pushes -- that copy is what pins it as one struct arg).
    virtual void BlitAnimFrameMaybe(RECT rect, char flag, unsigned int flags);
    // slot 12 (+0x30) -- 0x405fd0, named 2026-07-26 (was `_v12`; a rename + signature fill-in,
    // the declaration count is unchanged -- see this header's own EnsureSoundPlayingMaybe note
    // for why that matters). The SECOND, optional blit layer, painted right after
    // BlitAnimFrameMaybe by every caller and taking the identical (RECT, char, unsigned int)
    // argument list. Runs only when the current subframe's own bDoubleSpeedFlag is set: it
    // bumps the anim frame by +1 through slot 8, RestoreOverlapBlt's THAT frame over the same
    // clipped destination, then puts the frame back. `Unk0x16Maybe` on the subframe record
    // selects a horizontally-mirrored source window and ORs blit flag 0x20; the caller's
    // `flag == 1` ORs 0x40. Reads as the sprite's paired overlay/shadow frame -- hence Maybe.
    virtual void BlitOverlayFrameMaybe(RECT rect, char flag, unsigned int flags); // FUN_00405fd0
    // slot 13 (+0x34) -- 0x405e20, body in src/WidgetBase.cpp (EXACT). If pszName's first char
    // is alphanumeric (or NUL), copies up to 10 chars of it into szCategoryName (+0x7c); always
    // NUL-terminates at +0x86 regardless of the IsCharAlphaNumericA test.
    // ⚠ This header modelled ONE function as TWO until v577 -- a declared-only `_v13()`
    // placeholder for the slot plus a separate non-virtual `SetCategoryIfPrintable` for the same
    // 0x405e20 -- which is CLAUDE.md's "a vtable slot and a named method at the same address are
    // ONE function" hazard (v477's CarNetObj was the first catch, this the second). It stayed
    // invisible because every caller of the base spelling is a CONSTRUCTOR, and MSVC emits a
    // direct `call 0x405e20` for a virtual called from inside one, so both models produced
    // identical bytes. It was NOT invisible in the port: the never-defined `_v13` left a
    // gen_stubs stub in the vtable slot, and that stub is `void*(void)` -- it pops 0 where every
    // real call site pushes 4 -- so PeerTrainSlotQueueMaybe::SpawnOrAssignRandomTrain's slot-13
    // dispatch unbalanced the stack and crashed train creation from a depot (found v577 from
    // loco/stub_calls.log; same family as v576's slot-19 bug).
    virtual void SetCategoryIfPrintable(char *pszName); // FUN_00405e20
    // slot 14 (+0x38) -- still unidentified; likely a subframe/anim-state setter given every
    // known caller passes a small direction/subframe-shaped value (see
    // ReleaseChannelAndDispatch above). RETURNS a frame index, pinned 2026-07-27 by
    // AdvanceAnimFrameMaybe's bounce arm: that one takes the call's own EAX as the frame it then
    // publishes through slot 8, which only a non-void return can supply.
    // IDENTIFIED 2026-07-31 by reading 0x405a50, which also settles the "still unidentified"
    // note above: this slot SELECTS one of the descriptor's animation frame-sets by index and
    // returns the frame index that selection settled on. Out-of-range indices (negative, or
    // >= pKindDesc->nFrameSetCount) are ignored and the cached frame comes back unchanged --
    // which is why callers use the RETURN value rather than assuming the argument took.
    // The argument is SIGNED, pinned by the body: it range-checks with `jl` against 0 and `jge`
    // against nFrameSetCount, and a `>= 0` guard on an unsigned parameter could not survive into
    // the codegen at all. (This retype is byte-neutral -- MEASURED -- and stands on its own.)
    //
    // ✅ LANDED v563, IN src/WidgetBase.cpp, AND IT PAID +407 B -- the pricing below is STALE
    // HISTORY, kept only because the correction is the lesson. Measured on landing: EXACT
    // 173238 -> 173645 B, +407 B / +1 func; this file's TU went 24+7/31 1971 B -> 25+7/32 2378 B.
    // AdvanceAnimFrameMaybe (0x405c40) came BACK to EXACT and the new 0x405a50 arrived as PARTIAL.
    //   ⇒ The toll had not merely EXPIRED (v560's RectFlagObj0x477820 lesson) -- it had INVERTED:
    //     paying it refunded it. Two for two on "re-measure a withheld body before believing its
    //     price"; one compile is the whole cost of checking. docs/CODEGEN.md #186.
    //   ⇒ And withholding it was never free: a declared-only virtual is a generated STUB in the
    //     port, this slot is reached from SetDescriptor via ReleaseChannelAndDispatch, and
    //     SetDescriptor then tests the nSubFrame the stub never writes. So SetDescriptor returned
    //     0 for EVERY widget in the game and aborted the world load at
    //     BuildToolButton::InitMenuIconsMaybe's first guard. docs/CODEGEN.md #185.
    //
    // The superseded v479 pricing, for the record: merely ADDING it to that TU cost the sibling
    // AdvanceAnimFrameMaybe (0x405c40) its full 407-byte EXACT -- v479's "a new definition moves
    // the same parity bit a declaration does", MEASURED here three ways: as a plain member
    // definition, through a TU-local view struct, and with the signature change isolated (that
    // last one alone is free, 0x405c40 stays MATCH). The body itself reaches insns 39/40,
    // byte_diff 7 at the true 96-byte length -- the only residual is that the original saves edi
    // in the prologue while cl sinks the push past the bValid early exit. Compensating parity
    // currencies were tried and are all worse: declaring EnsureSoundPlayingMaybe here (and
    // retiring its view) buys back the 407 but costs WorldBoardMaybe's 0x457ce0 -951 and
    // RoadVehicleActor -504; spending src/Obj0x477798Family.h's `_v15` slot costs -504 and
    // leaves the -951. The verified body, for whoever cracks that parity class:
    //
    //     int AnimDescRefObj0x477488::DispatchAnimStateMaybe(int nFrameSetArg) {
    //         if (bValid != true) { return 0; }
    //         if (nFrameSetArg >= 0 && nFrameSetArg < pKindDesc->nFrameSetCount) {
    //             nSubFrame = nFrameSetArg;
    //             CursorAnimFrameEntry *pFrameEntry = &pKindDesc->paFrameEntries[nFrameSetArg];
    //             unsigned int nFrame = pFrameEntry->nStartFrame;  // read ONCE -- load-bearing
    //             nAnimTickCounter = 0;
    //             nAnimCooldownUntil = 0;
    //             nAnimValueCache = nFrame;
    //             SetAnimFrame(nFrame, 1);
    //             EnsureSoundPlayingMaybe(pFrameEntry->nSoundBankEntryId);  // via the 0x405ab0 view
    //         }
    //         return nAnimValueCache;
    //     }
    //
    // (That transcription is what landed, verbatim.) See docs/PARKED.md.
    virtual int DispatchAnimStateMaybe(int nFrameSetArg);
};

// Adds an effect-spawner ptr, a linked list of command-id "menu nodes", a last-hit-node
// cache, carousel scroll indices, two extra dirty-markable RECTs, and up/down menu-node
// candidates. Direct base of WidgetTagObj0x478378, WidgetPickerObj0x477cc8, and
// BuildToolButton.
class WidgetBaseObj0x4784c8 : public AnimDescRefObj0x477488 {
public:
    bool bActive;           // +0x88
    int Unk0x8c;                   // +0x8c
    // +0x90/+0x94/+0x98 -- the drag triple, RESOLVED 2026-07-25 from BuildToolButton's
    // grab/move pair (+0x90 was previously `bUnk0x90Maybe`, "a selection-lifetime flag
    // whose writer was not yet chased"; +0x94/+0x98 were inside the pad below).
    // BuildToolButton::HitTestMaybe (0x44a0c0) GRABS: on a slot-21 hit it stores
    // `cursor.resolvedPosA{X,Y} - rect.{left,top}` into +0x94/+0x98 and sets +0x90.
    // BuildToolButton::TickMaybe (0x4497a0) MOVES: while +0x90 is set it repositions to
    // `cursor.lastResolvedPos{X,Y} - +0x94/+0x98` every frame, so the triple is a
    // "dragging" flag plus the grab point's offset within the widget's own rect.
    // WorldActionCursor::SelectDecorObjAndDispatchModeMaybe's deselect path clears the flag
    // (only when previously set) -- i.e. cancels an in-flight drag.
    bool bDraggingMaybe;               // +0x90
    unsigned char pad0x91[3];          // +0x91 .. +0x93, alignment
    int nDragGrabOffsetXMaybe;         // +0x94
    int nDragGrabOffsetYMaybe;         // +0x98
    MenuNodeObj0x477568 *pLastHitNode; // +0x9c
    // +0xa0 -- the companion effect this widget spawned through the EffectSpawner singleton
    // (an AnimEffectObj0x477a90 in practice). Typed as the shared AnimDescRefObj0x477488
    // rather than AnimEffectObj0x477a90 itself so this header needs no new include: consumers
    // only ever dispatch slot 3 (RepositionWithHotspot, from the root) and slot 10
    // (AdvanceAnimFrameMaybe, from this base -- BuildToolButton's own tick), both of which
    // that base already declares, and AnimEffectObj0x477a90 derives from this same chain.
    AnimDescRefObj0x477488 *pEffectSpawner;
    int nCarouselScrollIndex; // +0xa4
    int nCarouselMaxIndex;    // +0xa8
    // +0xac -- a real byte flag, not padding (promoted from `pad0xac` 2026-07-26). The only
    // accesses anywhere in .text are in PlacementCursorMaybe::SelectCursorTypeAutoCurveMaybe,
    // on the g_BuildToolButton instance: it is cleared whenever the cursor is over the button's
    // slot-21 hit area OR outside the button entirely, and while it is clear a cursor inside
    // the button forces the default cursor type. No writer that SETS it has been found, so what
    // arms it is still open -- hence the Unk rung rather than a b-prefixed name.
    unsigned char Unk0xac;
    bool bSuppressRectBMaybe;             // +0xad
    RECT rectBMaybe;               // +0xb0
    RECT rectCMaybe;                // +0xc0
    MenuNodeObj0x477568 *pMenuListHead; // +0xd0
    // +0xd4: the menu node that triggered the widget's last tab/category switch (set
    // unconditionally by WidgetPickerObj0x477cc8::ActivateTab, read by its
    // HandleTabSwitchMenuNode to restore that node's highlight on a "reset to current"
    // command) -- RESOLVED 2026-07-16, was misclassified as padding.
    MenuNodeObj0x477568 *pLastActivatedNode; // +0xd4
    MenuNodeObj0x477568 *pBaseCandidateUp;   // +0xd8
    MenuNodeObj0x477568 *pBaseCandidateDown; // +0xdc

    WidgetBaseObj0x4784c8();
    // ⛔ 0x4545a0, 144 B. Transcribed and measured at DIFF(7) on the FIRST compile in v530, then
    // REVERTED -- the body is not the problem, its one dependency is. It needs
    // `DAT_004fd220.EffectSpawner_RemoveHandle(pEffectSpawner)`, and bringing `class
    // EffectSpawner` into src/WidgetBase.cpp costs `AnimDescRefObj0x477488::AdvanceAnimFrameMaybe`
    // (0x405c40) its whole 407-byte EXACT (MATCH -> DIFF(314) at 405 B). Net -263 B, so it stays
    // declared-only. The bisect is worth not repeating (all measured v530, control re-verified):
    //   - the DEFINITION alone is FREE; `#include "EffectSpawner.h"` alone pays the entire -407 B;
    //   - it is NOT a declaration count -- 1..5 extra free-function declarations are byte-neutral,
    //     a duplicate (guard-swallowed) #include is byte-neutral, and so are <stdio.h>/<math.h>/
    //     <time.h>, which are free only because they are already pulled in transitively;
    //   - it IS the arrival of a new FILE in the include set, and CONTENT IS IRRELEVANT: a
    //     completely EMPTY new header costs the full -407 B, and so does <assert.h> (genuinely
    //     new). Separately, any new TYPE declared inline in the .cpp does it too -- a class, a POD
    //     struct, even a bare `typedef int X;` -- as does one added member declaration on an
    //     already-included project header;
    //   - it is a THRESHOLD, not a parity bit -- TWO new empty headers cost exactly the same as
    //     one, so it cannot be paid back by adding a second trigger (the usual CODEGEN #95 lever).
    // A TU-local one-method view is NOT an out: it is also a new type, so it costs the identical
    // 407 B while additionally breaking the never-duplicate-a-struct rule. Full matrix: CODEGEN #98.
    // Retry when 0x405c40's own class cracks, or if src/WidgetBase.cpp ever needs `class
    // EffectSpawner` for something else -- the 407 B is a one-time toll, and once it is paid this
    // dtor rides in free.
    virtual ~WidgetBaseObj0x4784c8();
    // Real vtable slot 1 override (RectFlagObj0x477820::MarkDirty). Chains the base
    // implementation, then (if active) also dirty-marks rectCMaybe/rectBMaybe.
    virtual void MarkDirty();
    virtual unsigned char SetDescriptor(int nResourceId, int nSubFrameArg, char bForce);
    // Real vtable slot 3 override (0x454820, extern -- not yet transcribed): chains
    // AnimDescRefObj0x477488's own slot-3 body, then rebuilds rectCMaybe/rectBMaybe around the
    // new position and re-dirties. Called class-qualified by BuildToolButton::Reposition
    // WithHotspot.
    virtual void RepositionWithHotspot(int x, int y);
    // Tears down pMenuListHead/pEffectSpawner and releases the descriptor via
    // SetDescriptor(0,-1,0). Real vtable slot 15 in the full 21-slot layout; called
    // via qualified (non-virtual) dispatch at every known call site.
    //
    // ⛔ 0x454630, 75 B. BODY FULLY READ (2026-07-31, v537) and it is three statements:
    //     if (pMenuListHead != NULL) { delete pMenuListHead; pMenuListHead = NULL; }
    //     if (pEffectSpawner != NULL) {
    //         DAT_004fd220.EffectSpawner_RemoveHandle(pEffectSpawner); pEffectSpawner = NULL;
    //     }
    //     SetDescriptor(0, -1, 0);
    // NOT transcribed, because it needs the very same `DAT_004fd220.EffectSpawner_RemoveHandle`
    // dependency the destructor note above prices -- i.e. it is a SECOND member of that blocked
    // family, not an independent problem. The toll is one-time and shared, so the honest
    // arithmetic for paying it is now: -407 B (0x405c40's exact) against at most +75 B of new
    // EXACT here, plus 144 B (0x4545a0, DIFF(7)) and 96 B (0x405a50, byte_diff 7) that would
    // arrive as PARTIAL, not exact, since neither byte-matches. Still solidly net-negative, so
    // all three stay withheld together -- but if 0x405c40's class ever cracks, THREE bodies land
    // at once and two of them are already written out (here and on DispatchAnimStateMaybe).
    virtual void ClearOwned();
    // Real vtable slot 11 (+0x2c) override (0x454900), declared 2026-07-29 (v502) for
    // WorldActionCursor::RepositionSubIconsMaybe's class-qualified call to it; transcribed
    // v516 (EXACT). Chains AnimDescRefObj0x477488::BlitAnimFrameMaybe, then (if bActive)
    // intersects the passed rect against rectCMaybe (and rectBMaybe when !bSuppressRectBMaybe)
    // and darkens the overlap on the work surface (DDraw_DarkenRect).
    virtual void BlitAnimFrameMaybe(RECT rect, char flag, unsigned int flags);
    // slot 16 (+0x40) -- this class's own OnKeyDown (0x454ae0, src/WidgetBase.cpp). Signature
    // recovered from AppWndProc's own WM_CHAR/WM_KEYDOWN dispatch through the active-tab widget
    // (src/Main.cpp): one unsigned key code in, a byte "did you consume it" out. The return type
    // is `bool` and not this family's usual `char`, pinned 2026-07-26 by
    // WidgetPickerObj0x477cc8's override (0x4290a0): that one byte-matches EXACTLY only when its
    // own `bool bResult = WidgetBaseObj0x4784c8::OnKeyDownMaybe(nKey)` needs no 0/1
    // normalization, i.e. only when this base already returns a normalized bool -- and C++ ties
    // the two signatures together.
    virtual bool OnKeyDownMaybe(unsigned int nKey);
    // slots 17/18 -- the abstract per-node hit-test callback pair (base defaults are the
    // documented "17-20 abstract PureVirtualAbortMaybe placeholder" family,
    // docs/subsystems.md): HitTestAndLocalizeSecondaryMaybe dispatches each node through
    // slot 17, HitTestAndLocalizeMaybe through slot 18, passing the node pointer as an
    // explicit argument (confirmed via raw disasm: mov eax,[esi]; ...; mov ecx,esi; call
    // [eax+0x44/0x48] -- esi is `this`, edi (the node) is pushed as a plain stack arg).
    virtual char HitTestNodeSecondary(MenuNodeObj0x477568 *pNode, int x, int y); // slot 17 (+0x44)
    virtual char HitTestNode(MenuNodeObj0x477568 *pNode, int x, int y);          // slot 18 (+0x48)
    // slot 19 (+0x4c). ⭐ THE HONEST SIGNATURE IS KNOWN AND IS `char(MenuNodeObj0x477568 *, int,
    // int)` -- the same shape as slots 17/18 above, not the `void *()` this line spells. Pinned
    // v545 from the image itself: every implementation ends `ret 0xc` and answers in `al`, and
    // two of them are nothing BUT that -- 0x42d760 is `mov al,1; ret 0xc` (WorldActionCursor,
    // SelectedObjWidgetMaybe) and 0x44ef00 is `xor al,al; ret 0xc` (the shared false-stub, which
    // BuildToolButton carries at slot 18 AND slot 19). The call sites agree.
    //
    // ⭐ RETYPED v576, and the old price was STALE -- this is the CLAUDE.md "a priced-and-withheld
    // toll is a measurement with an EXPIRY DATE" corollary paying out for the second time. The
    // line spelled `void *_v19()` for 70 sessions because retyping it was priced at -1094 B (v506)
    // and then -1862 B (v545, retype + delete the two duplicate `WidgetSlot19VtblProbe` structs
    // and `WidgetTagObj0x478378View0x44ef10`), carrying a ⛔ "do NOT re-run either". Re-measured
    // from a clean v576 baseline, that SAME edit now costs -152 B / -1 func: the sole victim left
    // is PlacedObjRegistryMaybe::CompareEntriesMaybe (0x435c00), and v545's other four victims
    // (0x457ce0 -951, 0x405c40 -407, RoadVehicleActor -504, Obj0x477798Family +152) have all since
    // stopped responding to this dial. Idiom debt 10 -> 6 (both probe structs and both probe call
    // sites retired). See CODEGEN #138.
    //
    // ⚠ And the retype was not optional in the end: it fixes a HARD PORT CRASH. The vtable slot is
    // emitted from THIS declaration, so while it read `void *_v19()` the slot's symbol was a
    // zero-argument `?_v19@...@@UAEPAXXZ` that nothing defined -- the port linked a generated stub
    // that returns `ret 0` while every call site pushes 12 bytes of (node, x, y). Each dispatch
    // leaked 12 bytes of stack, so AdvanceAnimFrameMaybe's fixed-size epilogue returned into the
    // heap. It only became reachable in v576, when the CursorDesc ".but" fix finally gave
    // pMenuListHead some nodes to walk. See CODEGEN #213.
    virtual char TestAndToggleMenuNodeHoverMaybe(MenuNodeObj0x477568 *pNode, int x, int y);
    // slot 20 (+0x50) -- the third member of the same abstract per-node callback family: the
    // EXECUTE half to slots 17/18's TEST halves. WorldActionCursor::TickAndTutorialCheckMaybe
    // walks pMenuListHead dispatching every node through THIS slot on itself (raw disasm at
    // 0x459f7c: mov edx,[edi]; push esi; mov ecx,edi; call [edx+0x50]), and
    // WorldActionCursor overrides it at 0x45aa50.
    virtual char HandleMenuCommandMaybe(MenuNodeObj0x477568 *pNode);             // slot 20 (+0x50)
    // Real vtable slot 5 (0x454a60), overrides RectFlagObj0x477820::TryInvokeCallbackB with an
    // unrelated signature/behavior -- modeled as an ordinary member rather than a literal C++
    // override (the signature differs), same convention as SetAnimFrame above was before the
    // 2026-07-21 vtable modeling. Own Contains (slot 2) gates a walk of pMenuListHead
    // (skipping pLastHitNode), dispatching each node through THIS widget's own vtable slot 18
    // (+0x48) with the localized hit point from ComputeLocalPos.
    char HitTestAndLocalizeMaybe(int x, int y);
    // Real vtable slot 4 (0x4549e0), and DECLARED as the override of
    // RectFlagObj0x477820::TryInvokeCallbackA as of v549 -- it was an ordinary member under the
    // descriptive name `HitTestAndLocalizeSecondaryMaybe` until then, which is CLAUDE.md's
    // "a vtable slot and a named method at the same address are ONE function" hazard. Unlike
    // slot 5 below (whose root signature is still the wrong `void()`, and fixing THAT is part of
    // the withheld -256 B bundle at the top of this file), slot 4's root signature already reads
    // `char(int, int)` -- exactly this body's shape -- so the override needed no root edit and
    // no new declaration, only the rename plus `virtual`.
    //
    // The four call sites are all CLASS-QUALIFIED base calls out of derived overrides
    // (`WidgetBaseObj0x4784c8::TryInvokeCallbackA(x, y)`), which C++ resolves statically, so
    // `virtual` does not turn any of them into a vtable dispatch -- that is what makes this
    // model fix byte-free, and it is the same reasoning WorldActionCursor::TryHandleClickMaybe
    // documents in the OPPOSITE direction (its one call site is an unqualified direct call, so
    // the same edit there WOULD cost bytes and is correctly withheld).
    //
    // Structural twin of HitTestAndLocalizeMaybe -- identical body, dispatches each node through
    // slot 17 (+0x44) instead of slot 18. Which UI action invokes slot 4 vs slot 5 not yet
    // chased.
    virtual char TryInvokeCallbackA(int x, int y);
    // Ordinary (non-virtual) member, 0x4546d0, body in src/WidgetBase.cpp -- the menu-icon
    // factory every widget's own one-shot Init pass calls once per available TileKind
    // descriptor: allocates a MenuNodeObj0x477568 (or a text-labeled UiIconListItem when
    // nTextLen > 0), links it onto pMenuListHead, and returns the list's new tail. Declared
    // here (rather than on the TU-local derived views the three callers used to carry) as of
    // 2026-07-26 -- the move was measured byte-neutral across every WidgetBase.h consumer.
    MenuNodeObj0x477568 *GetOrCreateMenuIconItemMaybe(CursorDesc *pDesc, unsigned short wModeFlags,
                                                      unsigned int nTextLen);
};
