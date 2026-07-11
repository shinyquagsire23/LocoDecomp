// LocalPlayerIdentity -- the local player's own identity singleton (PTR_004aa4a8),
// 0x124 (292) bytes total; ctor Config_InitClientIdentity (0x452e10), see docs/subsystems.md's
// `LocalPlayerIdentity` entry for the full derivation. Mirrors the canonical Ghidra
// Structure field-for-field.
#pragma once

class LocalPlayerIdentity {
public:
    unsigned short kindTag; // +0x04, const 0x66
    char name[13];          // +0x06, NUL-terminated, from lego.ini's [USER]Name, else GetUserNameA,
                            //        else the literal "LEGO LOCO" (the ctor's three-step fallback)
    char pad0x13;                // +0x13
    unsigned int sessionId; // +0x14, network-assigned session/game id (GameNet_DispatchMessage msg 0x3ea/0x3e9)
    unsigned int clientId;  // +0x18, the DirectPlay client id ([CLIENT] NextId ini value, capped at 999)
    unsigned int nextPostSeqId;  // +0x1c, auto-incrementing 0-9999 postbag/card id counter (wraps to 0)
    char postSeqIdString[16];    // +0x20, "%03d%04d" (clientId, nextPostSeqId) -- NO space
                                 // between the two fields; read out of the image at 0x47f0a8, not out of
                                 // Ghidra's own `s__03d_04d_0047f0a8` label, whose underscores are its
                                 // mangling of the `%` signs (CLAUDE.md's string-literal rule).
                                 // Produced by AllocNextPostSeqIdString/0x453320.
    unsigned char abRawProfileTail[240]; // +0x30, raw round-tripped .usr profile bytes, no typed field semantics
    bool hasIdFlag;         // +0x120, true once clientId was freshly allocated this run

    // 0x452e10 -- what `new LocalPlayerIdentity` in AppWindow's bootstrap (0x406ba0)
    // dispatches to. Defined in src/LocalPlayerIdentity.cpp.
    LocalPlayerIdentity();

    // DECLARED-ONLY, and that is a measured decision, not an oversight. The original's dtor
    // really is in-class (no out-of-line `??1LocalPlayerIdentity` exists anywhere in the image,
    // and the `??_G` thunk at 0x452fa0 carries the vptr store itself rather than a `call ??1`),
    // so writing `virtual ~LocalPlayerIdentity() {}` here DOES close 0x452fa0 EXACT (+32 B) --
    // but it costs src/TutorialWnd.cpp its RestorePresenterBackdrop (0x452b00), -249 B, for a
    // net -217 B. Same lever, same victim function and same verdict as IniFile's own dtor in
    // v442; see docs/PARKED.md's `??_G` in-class-dtor section.
    // ** v449 re-measured this inside the full five-lever repo-wide sweep those rows deferred to:
    // +32 B here against the same fixed -249 B, which all five levers pay INDEPENDENTLY and
    // IDENTICALLY (a saturating flip, not additive, not a parity cycle). Cluster gain +99 B vs
    // 249, so no subset tips positive -- CLOSED, not deferred. Do not re-probe. **
    virtual ~LocalPlayerIdentity();

    // 0x452fc0 -- adopts a new player name: no-op if unchanged, otherwise flushes the postbag
    // index, re-reads (or freshly mints) the .usr profile under the new name, and refreshes the
    // two UI surfaces that show it.
    void SetNameMaybe(char *pszName);

    // 0x4530c0 (Ghidra: Config_LoadPlayerIdentityFileMaybe) -- reads
    // "<installPathPrefix><name>.usr" over the 0x120-byte record starting at kindTag. Returns
    // false when the file cannot be opened or read at all; true once the record is in hand,
    // including the "the file held a foreign/garbage tag" repair path, which re-clears the
    // record, allocates a fresh [CLIENT]NextId and writes the file back out.
    bool LoadProfile();
};

extern LocalPlayerIdentity *g_pLocalPlayerIdentity; // DAT_004aa4a8

// Round-trip the local player's identity to its .usr profile file.
// Both live in src/LocalPlayerIdentity.cpp. They are genuine `this`-in-ecx members in the
// original, but every call site spells them as free __fastcall functions and the generated code
// is identical either way, so the free form is kept (it is what three consumer TUs already use).
void __fastcall Profile_SavePlayerUserFile(LocalPlayerIdentity *pIdentity);  // 0x4532a0
// Next "<clientId><seq>" postbag/card id string; bumps the counter and re-saves the profile.
char *__fastcall AllocNextPostSeqIdString(LocalPlayerIdentity *pIdentity);   // 0x453320
