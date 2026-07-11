// IniFile method bodies -- see IniFile.h for the class writeup. Every method is a thin
// GetPrivateProfile*/WritePrivateProfile* wrapper keyed off the iniPath field the ctor copies in,
// which is why the whole class fits in one small TU.
#include <windows.h>

#include <stdlib.h>
#include <string.h>

#include "IniFile.h"

// FUNCTION: LOCO 0x452ce0
// The vtable store at 0x452cee is the compiler's, not the source's; everything else is the
// inlined intrinsic strcpy (`repnz scasb` length scan then `rep movsd`/`rep movsb`) plus the
// one trailing field zero.
IniFile::IniFile(const char *pszIniPath)
{
    strcpy(iniPath, pszIniPath);
    Unk0x108 = 0;
}

// EFFECTIVE MATCH -- 30 B vs 32, DIFF(22). The original INLINES the dtor's single vtable store
// into this thunk where ours CALLS ??1IniFile, which is what an IN-CLASS dtor definition
// produces. **Measured, and it closes: `virtual ~IniFile() {}` in src/IniFile.h makes 0x452d30
// EXACT at 32 B and this file 6/6 at 249 B.** Rejected anyway, for the SAME reason the identical
// lever is rejected on ThumbnailBmp (see src/ThumbnailBmp.cpp): the in-class form makes every
// consumer TU inline the dtor instead of calling it, and that reshuffle costs
// src/TutorialWnd.cpp its `RestorePresenterBackdrop` (0x452b00) match, 249 B EXACT -> gone.
// Repo-wide that is 119121 B / 495 funcs out-of-line against 118897 B / 494 in-class, i.e. the
// in-class form loses a net 224 B. It also DELETES this TU's ??1 COMDAT entirely (nothing left
// needs it out of line), which is why 0x452d50's marker has to move with the lever, not stay.
// ** v449 re-measured this inside the full five-lever repo-wide sweep the v442-v445 rows all
// deferred to: +25 B here (0x452d30 EXACT at 32 B, and this file 7 funcs -> 6 as 0x452d50's
// marker loses its COMDAT) against the same fixed -249 B. The whole cluster's gain is +99 B
// against that 249, so NO subset tips positive and the hypothesis is CLOSED, not deferred.
// Do not re-probe. See docs/PARKED.md's v449 sweep section. **
//
// FUNCTION: LOCO 0x452d30 (??_GIniFile scalar deleting dtor -- compiler-generated around
// ~IniFile() below; no source of its own)

// FUNCTION: LOCO 0x452d50
// Bare vtable-dtor stub (mov [ecx],&vtbl 0x4784bc; ret) -- the .ini accessor owns no
// heap state. Confirmed as IniFile's own dtor via FUN_0041f5e0 (the "LoadEvents" script
// loader), which destroys its stack-local IniFile through this stub, and the IniFile
// ctor (0x452cee) storing the same vtable. Moved out of src/phase2_probe2.cpp
// 2026-07-22 (v322, was VtblStub0x452d50).
IniFile::~IniFile() {}

// FUNCTION: LOCO 0x452d60
int IniFile::ReadInt(const char *section, const char *key, int defaultValue)
{
    return GetPrivateProfileIntA(section, key, defaultValue, iniPath);
}

// FUNCTION: LOCO 0x452d80
void IniFile::ReadString(const char *section, const char *key, const char *defaultValue,
                         char *outBuf, int outSize)
{
    GetPrivateProfileStringA(section, key, defaultValue, outBuf, outSize, iniPath);
}

// FUNCTION: LOCO 0x452db0
void IniFile::WriteInt(const char *section, const char *key, int value)
{
    char szValue[100];

    _itoa(value, szValue, 10);
    WritePrivateProfileStringA(section, key, szValue, iniPath);
}

// FUNCTION: LOCO 0x452df0
void IniFile::WriteString(const char *section, const char *key, const char *value)
{
    WritePrivateProfileStringA(section, key, value, iniPath);
}
