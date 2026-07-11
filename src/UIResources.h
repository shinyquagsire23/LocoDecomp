// UIResources (DAT_004855e8) -- the big shared UI-resources registry singleton. ONE
// canonical shared partial view: every consumer includes THIS header and extends it in place
// as more members are read, rather than each TU declaring its own divergent local struct --
// the old "N per-TU partial view" pattern is retired (see CLAUDE.md's "never duplicate a
// struct across TUs" rule, 2026-07-17). Consolidated 2026-07-17 from 4 pre-existing divergent
// copies (src/DSound.cpp's UIResourcesPartial, src/EditCardWnd.cpp's UIResourcesPartial,
// src/TutorialWnd.cpp's UIResourcesPartial, src/WidgetPicker.cpp's UIResourcesFxPartial) --
// their method sets are simply unioned here; no field offsets changed.
#pragma once

#include <windows.h>

#include "CursorDesc.h"
#include "DSoundChannel.h" // SoundBankEntry

struct UIResources {
    // +0x00 -- ⚠ THIS IS A VTABLE POINTER, not unmodeled padding. UIResources is POLYMORPHIC:
    // its ctor (0x445f70) stores `&PTR_FUN_00478270` here as its first act after chaining the
    // m_rfIndex ctor, and the dtor (0x445fe0) re-stamps the same address before calling
    // Shutdown -- the two writes that only a class with a vtable emits. Found v538 while reading
    // the ctor; NOT yet acted on, because fixing it properly means giving this struct a virtual
    // dtor and DELETING this pad (the vptr the compiler adds must occupy +0x00, not sit in front
    // of it), and every consumer of this header would have to be re-measured. Nothing currently
    // depends on the distinction -- no transcribed code dispatches through the slot -- so the
    // layout below is correct either way and the pad is only a mis-LABEL, not a mis-offset.
    // Landing 0x445f70/0x445fe0 requires resolving this first; 0x445fe0 additionally needs the
    // still-parked RFIndex dtor 0x45ca20.
    unsigned char pad0x0[4];
    // +0x04..+0x14 -- the five shared UI fonts, all created back-to-back by the resource-init
    // pass (0x446050) from one CreateFontA call each against the same typeface name, and all
    // DeleteObject'd back-to-back by Shutdown (0x446340) below. Named for the pixel height
    // each is created with; the weights are 800 / 700 / 700 / 700 / 900 respectively.
    HFONT m_hFont12;  // +0x04 -- height 12, weight 800
    HFONT m_hFont14;  // +0x08 -- height 14, weight 700
    HFONT m_hFont16;  // +0x0c -- height 16, weight 700
    HFONT m_hFont24;  // +0x10 -- height 24, weight 700
    HFONT m_hFont20;  // +0x14 -- height 20, weight 900
    // +0x18 -- the RF-archive index, EMBEDDED by value (16 bytes, exactly filling +0x18..+0x27)
    // rather than pointed to: DAT_00485600, which the rest of the codebase reaches as the
    // stand-alone `g_RFIndex`, is literally this member. Opened once by Init (0x446050) from
    // the [DIRECTORIES]/ResFile ini key, and default-constructed by 0x45ca10 out of UIResources'
    // own ctor (0x445f70) -- which is what that "partial zero-init" (offsets 0/4/0xc, skipping
    // 8) in src/UIResources.cpp actually is.
    RFIndex m_rfIndex;
    // +0x28 -- last played station-clock chime step (the 5-minute index 0..11), written by
    // TickStationClockChimeMaybe only on the steps where a chime actually fires (0, 3, 6, 9).
    int m_nLastClockChimeStepMaybe;
    // +0x2c -- per-kindId pointer-indirection table into m_apKindDescriptors below; read as
    // m_pKindSlotPtrsMaybe[kindId] by TileKind_GetOrLoadDescriptor (0x446ea0, offsets
    // confirmed by its own decompile), which recovers the aliased kind id by subtracting the
    // array base. SetKindSlotPtrMaybe (0x447290) writes &m_apKindDescriptors[b] into slot a,
    // i.e. it ALIASES kind id `a` onto kind id `b`'s descriptor slot. Sized 0x4001 (not
    // 0x4000) so m_apKindDescriptors lands exactly at its confirmed +0x10030 offset --
    // byte-match ground truth; the kindId validity check is `> 0x3fff`, so the extra tail
    // slot is never a valid kindId index.
    CursorDesc **m_pKindSlotPtrsMaybe[0x4001];
    // +0x10030 -- the interned per-kind descriptor table the pointer table above indexes
    // into. Retyped from `int[0x4000]` (v355) once TileKind_CreateDescriptor (0x446840) was
    // read: each slot holds a descriptor object of whichever CursorDesc-family class that
    // kind id's category selects, NULL = not yet created, and (CursorDesc *)-1 = creation
    // was attempted and the object failed to load. Resized 0x4000 -> 0x4001 in v356 from
    // ReleaseAllCachedResources' (0x4467e0) own sweep count, which is 0x4001 and lands the
    // next array exactly at its confirmed +0x20034 -- the same off-by-one tail slot
    // m_pKindSlotPtrsMaybe above has, and for the same reason (the kindId validity check is
    // `> 0x3fff`, so the extra tail slot is never a valid kindId index).
    CursorDesc *m_apKindDescriptors[0x4001];
    // +0x20034 -- the interned sound-bank entry table, one slot per WAV resource id in the
    // 0x5000..0x6060 band, so **the index is `soundId - 0x5000`** and the array is exactly
    // (0x6060 - 0x5000 + 1) = 0x1061 slots wide, ending precisely at m_nLocaleId below.
    // Same tri-state as m_apKindDescriptors: NULL = not yet loaded, (SoundBankEntry *)-1 =
    // load failed (see src/UIResources.cpp's two station-clock chime slots, which are just
    // named aliases for ids 0x53ab and 0x5399 -- 0x4a64c8 and 0x4a6480 are literally
    // &m_apSoundBankEntries[0x3ab] and [0x399]).
    //
    // ⚠ SoundBank_PreloadWavRangeMaybe (0x446cc0) reaches this array as
    // `[this + soundId*4 + 0xc034]` -- VC5 folded the `- 0x5000` index bias straight into
    // the addressing displacement (0x20034 - 0x5000*4 = 0xc034). That folded base is NOT a
    // second array at +0xc034: taken literally it would overlap m_pKindSlotPtrsMaybe and
    // m_apKindDescriptors, which is what makes the bias unmistakable. ReleaseAllCachedResources
    // sweeps the same storage with the bias already applied (base +0x20034, count 0x1061),
    // which is what pins both the true base and the true width.
    SoundBankEntry *m_apSoundBankEntries[0x1061];
    // +0x241b8 -- the UI language/locale id, switched on by every locale-aware string loader
    // in this subsystem (TileKind_GetOrLoadDescriptor 0x446ea0, its twin 0x4470b0,
    // SoundBank_PreloadWavRangeMaybe 0x446cc0, LoadLocaleString 0x447330) to pick a
    // per-language resource-id offset. Cases 1,2,4,5,6,7,8,9 are real; anything else means
    // "use the raw id".
    int m_nLocaleId;

    // 0x445f70 -- the registry's constructor, and the ONLY thing that establishes the IDENTITY
    // mapping in m_pKindSlotPtrsMaybe (`m_pKindSlotPtrsMaybe[i] = &m_apKindDescriptors[i]` for
    // i < 0x4000). That matters far more than it looks: TileKind_GetOrLoadDescriptor reaches
    // every descriptor THROUGH that table and returns NULL outright when the slot is NULL, so
    // without this ctor the whole descriptor registry answers NULL to everyone -- which is what
    // crashed SplashWnd::EnsureArtLoaded even after the descriptors themselves were being
    // constructed correctly. SetKindSlotPtrMaybe below is the only OTHER writer, and it only
    // ever installs seasonal ALIASES over an already-identity-filled table.
    // ⚠ PARTIAL, not byte-matched: the original also stores its vtable (0x478270) at +0x00,
    // which this class cannot reproduce until pad0x0 becomes a real vptr (see its note above).
    UIResources();
    // 0x447290 -- m_pKindSlotPtrsMaybe[a] = &m_kindBackingMaybe[b]; defined in
    // src/UIResources.cpp (moved out of src/phase2_probe4.cpp 2026-07-22).
    void SetKindSlotPtrMaybe(int a, int b);

    // 0x446ea0 -- large, not-yet-transcribed shared lazy-load method on this registry.
    CursorDesc *TileKind_GetOrLoadDescriptor(int kindId);
    // 0x4470b0 -- the twin of the above that skips the alias/redirect table (see the
    // m_pKindSlotPtrsMaybe note): every menu-populate loop that walks a whole TileKind id
    // RANGE uses this one, so it never resolves an aliased id twice.
    // `int`, not `unsigned` -- the byte-EXACT body at 0x4470b0 opens with `kindId < 0 ||
    // kindId >= 0x4000`, so the original's parameter is signed and the low guard is live.
    // The declaration said `unsigned` until the definition moved onto this class and the two
    // had to agree; the writer wins.
    CursorDesc *TileKind_GetOrLoadDescriptorNoAlias(int kindId);

    // 0x446050 -- brings the whole UI-resource layer up, once, from AppWindow::
    // InitSubsystemsAndWindows. Declared here rather than on a TU-local view because the
    // caller is in another TU: a view-only definition leaves THIS symbol -- the one
    // src/AppWindow.cpp actually calls -- undefined, which the byte-match cannot see (it
    // compares one COMDAT at a time and masks relocations) but a real link resolves to a
    // do-nothing stub. That stub returning 0 is what made InitSubsystemsAndWindows fail and
    // put up the fatal MessageBox; see tools/link_check.sh and link/gen_stubs.py.
    unsigned char Init();
    // 0x446840 -- the descriptor factory Init's range pass and both lazy loaders call.
    unsigned char TileKind_CreateDescriptor(int kindId, char *pszDefinition);
    // The shared inline range helper: Init instantiates it with the genuine 0x400..0x4000
    // range, the two lazy loaders with first == last.
    int TileKind_LoadDescriptorRange(int nFirstId, int nLastId);
    // 0x4463c0 -- picks m_nLocaleId from the installed language, which every LoadLocaleString
    // and every descriptor-loading string lookup then keys off. Called from src/Main.cpp, so
    // it has the same cross-TU problem Init did: a view-only definition left the symbol
    // Main.cpp calls undefined, and the resulting stub left m_nLocaleId at 0.
    void Locale_DetectLanguage();
    // 0x446340 -- Init's exact counterpart, called from AppWindow::SaveWindowAndCleanExit.
    unsigned char Shutdown();
    // 0x4467e0 -- the cache teardown Shutdown calls; here because Shutdown is.
    void ReleaseAllCachedResources();

    // NOTE (v356): ReleaseAllCachedResources (0x4467e0) and Shutdown (0x446340) are NOT
    // declared here either, for the same reason -- re-measured this session, and declaring
    // just those two here cost DPlaySessionMgr.cpp one EXACT (166 bytes). Every FIELD change
    // v356 made below (splitting pad0x0 into the five HFONTs, resizing m_apKindDescriptors,
    // and appending m_apSoundBankEntries + m_nLocaleId) was codegen-neutral in the same
    // measurement, which re-confirms v355's refinement: it is DECLARATIONS, not fields.

    // NOTE (v340): TickStationClockChimeMaybe (0x447400, transcribed) and
    // SoundBank_PreloadWavRangeMaybe (0x446cc0, untranscribed) are deliberately NOT
    // declared here -- ANY new method declaration in this shared header rotates
    // DPlaySessionMgr.cpp's /Og TU state and breaks SelectGridCellFromPointMaybe's
    // EXACT (bisected v340; same class as the WorldBoardMaybe.h v334 bisect). They live
    // on the TU-local UIResourcesView0x447400 in src/UIResources.cpp instead.
    void TickStationClockChimeMaybe(int nSeconds, int bFlagMaybe); // PRICE PROBE v577


    // 0x447330 -- locale-aware LoadStringA wrapper: remaps stringId through a per-language id
    // offset table (keyed by the locale id at this+0x241b8) when stringId falls in the
    // 100-500 "remappable" range, then LoadStringA's it; retries with the raw unmapped id if
    // the remapped lookup comes back empty. Not transcribed here (opaque extern), see
    // docs/subsystems.md's UIResources section.
    void LoadLocaleString(UINT stringId, LPSTR buf, int bufSize);

    // Transcribed in src/UIResources.cpp.
    SoundBankEntry *SoundBank_LookupEntryById(unsigned int nSoundId); // 0x4472b0
    unsigned char SoundBank_PreloadWavRange(unsigned int nFirstId, int nLastId); // 0x446cc0

    // Both of the below are genuine __thiscall members that never actually read `this` in
    // their own bodies (confirmed via raw disasm: PlaySoundAtScreenPos's own prologue only ever
    // touches [esp+...] stack args, never ecx) -- same class as Widget::
    // TestAndToggleMenuNodeHoverMaybe. Calling them as plain free functions leaves the
    // caller-side `mov ecx, 0x4855e8` this-load unexplained (a genuine byte-diff, confirmed
    // 2026-07-16 while transcribing HandleSavegameMenuNode) -- modeling them as members
    // reproduces it even though the callee itself never reads ecx.
    // Both are transcribed in src/UIResources.cpp, and both are PARKED there: the original
    // inlines SoundBank_LookupEntryById into them and /Ob1 cannot reproduce that without
    // deleting the helper's own COMDAT. See that file's park note.
    void PlayUiSound(unsigned nSoundId); // 0x447930

    // Plays/queues UI sound nSoundId at a screen position (x, y) in channel category nCategory.
    void PlaySoundAtScreenPos(unsigned nSoundId, int x, int y, unsigned nCategory); // 0x4479d0

    // Plays a one-shot sound loaded from a file path at a world position (x, y, flags). Same
    // this-in-ecx-but-never-read class as PlaySoundAtScreenPos above (Ghidra types it __stdcall since the
    // body ignores ecx). Returns void -- the raw disasm has no `mov eax` before its `ret 0x10`,
    // and both call sites discard the value (was mis-declared `unsigned int` until v474).
    void Sound_PlayOneShotAtPosition(char *pszPath, int x, int y, unsigned int nFlags); // 0x447a70

};
extern "C" extern UIResources g_UIResources; // DAT_004855e8
