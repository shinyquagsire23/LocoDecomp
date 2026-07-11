// EasterEggMgrAppView0x406ba0 -- the seasonal/easter-egg unlock manager singleton
// (DAT_004a99b0), which sits immediately below g_dwGameTick and is torn down by
// AppWindow::SaveWindowAndCleanExit.
//
// This header was split out of src/AppWindow.cpp in 2026-07-26 when a SECOND TU
// (src/LoadingScreen.cpp) needed one of these methods: the method has a real stack argument, so
// the usual `__fastcall` free-function escape hatch that models a `this`-only call does not
// reach it, and a per-TU copy of the struct would have been the fourth partial view of this one
// singleton -- exactly the duplicate-definition hazard CLAUDE.md's struct-discipline rule
// forbids. Only THIS view moved; src/UIResources.cpp's `EasterEggMgrMaybe` and src/Main.cpp's
// `EasterEggMgrWndProcView0x4618c0` are still independent TU-local views of the same object and
// should be folded onto this one when either of those TUs is next opened.
//
// Still a methods-only view: no field is modeled, so there is no layout to drift.
//
// 2026-07-29 (v494): the FULL field+method model of this singleton now exists as class
// `ScriptEventLoader` in src/ScriptEventLoader.cpp (the whole 0x41f4e0..0x4202b0 .obj
// transcribed: the class embeds g_dwGameTick at +0x4 and owns the ee.ini script lists, the
// world idle tick and the InsertSeq spawn pump). This view's two FUN_ declarations took the
// canonical names in the same pass (lint_alias). Fold direction flips here: THIS view (and
// Main.cpp's EasterEggMgrWndProcView0x4618c0, whose 0x4202b0 method is likewise renamed) is
// what should be folded onto ScriptEventLoader when a TU next needs the fields, not the other
// way round -- but doing so means moving the class into a header its consumers can include,
// which is a measured-parity change, not a free one.
#pragma once

// Spelled with methods rather than free `__fastcall` functions because the two script loaders
// below take a real stack argument beyond `this` (a second `__fastcall` parameter goes in edx,
// not on the stack) -- see src/AppWindow.cpp's own note on the escape hatch.
struct EasterEggMgrAppView0x406ba0 {
    // 0x41f5e0 -- opens "<install><base>.ini" and replays its [LoadEvents] 001.. script-event
    // list through ScriptEventLoader_ParseFixedRecordMaybe. Returns 1 unconditionally.
    char LoadEventScriptsMaybe(const char *pszIniBaseName);
    // 0x41f6e0 -- the same shape for the [TimeEvents] section, replayed through
    // ScriptEventLoader_ParseRandomizedRecordMaybe instead. Its one caller
    // (App_LoadWorldThreadProcMaybe, src/LoadingScreen.cpp) passes "ee", i.e. ee.ini -- the
    // seasonal-event table that ships in the RF archive.
    char LoadTimeEventScriptsMaybe(const char *pszIniBaseName);
    // 0x41f8e0 -- records a tile kind as "discovered". Resolves nKindId through
    // UIResources::TileKind_GetOrLoadDescriptor, bails if that descriptor's own +0x163 latch is
    // already set, otherwise bumps this manager's running discovery counter at +0x10 and writes
    // it back to lego.ini's [EasterEggs] section keyed by the counter, latching +0x163 only when
    // that write reports success. Its one caller is CursorDesc::GetOrLoadFrameBitmap
    // (src/CursorDesc.cpp), i.e. a kind counts as discovered the first time its bitmap is
    // actually realized.
    char RecordEasterEggUnlockMaybe(unsigned int nKindId);
    void ProcessInsertSeqSpawnsMaybe();                // 0x420000
    void DeleteAllEventRecordsMaybe();                 // 0x41f4e0
};
extern EasterEggMgrAppView0x406ba0 g_easterEggMgrMaybe; // DAT_004a99b0
