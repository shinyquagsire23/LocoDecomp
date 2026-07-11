// LocalPlayerIdentity's own TU -- see src/LocalPlayerIdentity.h for the class writeup.
//
// TU boundary: 0x452e10..0x4533cf, six functions, bounded below by src/IniFile.cpp's own
// 0x452df0 and above by RoadVehicleActor's ctor at 0x4533d0. Fully transcribed 2026-07-27.
#include <windows.h>
#include <string.h>

#include "EditCardWnd.h"
#include "IniFile.h"
#include "LocalPlayerIdentity.h"
#include "PostBag.h"

// DAT_004a99c8 -- declared file-locally exactly as src/AppWindow.cpp, src/LoadingScreen.cpp and
// src/DPlaySessionMgr.cpp already do (the shared home is src/DSoundChannel.h, which drags in
// <dsound.h> for a single char array).
extern char g_pInstallPathPrefix[];  // DAT_004a99c8

// EFFECTIVE MATCH -- 30 B vs 32, DIFF(22). The compiler-generated scalar-deleting-dtor thunk over
// ~LocalPlayerIdentity, which is vtable 0x4784c0's only slot and its only xref. The original's
// dtor is in-class (the thunk carries the vptr store itself, and no out-of-line `??1` exists in
// the image), and writing it that way in src/LocalPlayerIdentity.h DOES close this EXACT at 32 B
// -- but it then costs src/TutorialWnd.cpp's RestorePresenterBackdrop (0x452b00) its whole 249 B,
// a real 83-instruction reg-alloc reshuffle at identical length, for a net -217 B. Measured both
// ways this session; declared-only wins. Same lever and same victim as IniFile's dtor in v442.
//
// FUNCTION: LOCO 0x452fa0 (??_GLocalPlayerIdentity scalar deleting dtor -- compiler-generated)

// FUNCTION: LOCO 0x452e10
// Establishes the local player's identity at boot: three-step name fallback (lego.ini's
// [USER]Name, then the logged-in Windows account, then the literal "LEGO LOCO"), then the
// adopt-a-name path SetNameMaybe below spells out again.
//
// The adopt-a-name tail really is duplicated in the original, not shared: SetNameMaybe is a
// plain out-of-line member (SplashWnd::OnEnterCommitAndDispatch CALLs it at 0x4226d1), so
// nothing here could have inlined it -- under /Ob1 only an in-class/`inline` function expands,
// and making SetNameMaybe in-class would rewrite that SplashWnd call site.
LocalPlayerIdentity::LocalPlayerIdentity()
{
    char szName[13];
    DWORD dwSize;

    kindTag = 0x66;
    name[0] = '\0';
    sessionId = 0;
    clientId = 0;
    nextPostSeqId = 0;
    hasIdFlag = false;

    dwSize = 13;
    g_pIniFile->ReadString("USER", "Name", "", szName, 13);
    if (szName[0] == '\0') {
        GetUserNameA(szName, &dwSize);
        if (szName[0] == '\0') {
            strcpy(szName, "LEGO LOCO");
        }
    }

    if (strcmp(name, szName) != 0) {
        strcpy(name, szName);
        if (g_pPostBagFileCache != NULL) {
            g_pPostBagFileCache->SaveIndexFile();
        }
        if (!LoadProfile()) {
            int nNextId = g_pIniFile->ReadInt("CLIENT", "NextId", 0);
            if (nNextId > 999) {
                nNextId = 1;
            }
            g_pIniFile->WriteInt("CLIENT", "NextId", nNextId + 1);
            clientId = nNextId;
            hasIdFlag = true;
        } else {
            hasIdFlag = false;
        }
        // NULL during construction -- the singleton pointer is only published once this ctor
        // returns, so the two UI refreshes below are a rename-only path in practice.
        if (g_pLocalPlayerIdentity != NULL) {
            g_pEditCardWnd->RebuildLocalPlayerCard();
            g_pPostBagCache->PostBag_RecountCategoryOutFiles();
        }
    }
}

// FUNCTION: LOCO 0x452fc0
// Adopts pszName as the local player's name. Unchanged name => no-op; otherwise the postbag
// index is flushed under the OLD name, the profile is re-read (or minted) under the NEW one, and
// the two surfaces that render the name are rebuilt.
void LocalPlayerIdentity::SetNameMaybe(char *pszName)
{
    if (strcmp(name, pszName) != 0) {
        strcpy(name, pszName);
        if (g_pPostBagFileCache != NULL) {
            g_pPostBagFileCache->SaveIndexFile();
        }
        if (!LoadProfile()) {
            int nNextId = g_pIniFile->ReadInt("CLIENT", "NextId", 0);
            if (nNextId > 999) {
                nNextId = 1;
            }
            g_pIniFile->WriteInt("CLIENT", "NextId", nNextId + 1);
            clientId = nNextId;
            hasIdFlag = true;
        } else {
            hasIdFlag = false;
        }
        if (g_pLocalPlayerIdentity != NULL) {
            g_pEditCardWnd->RebuildLocalPlayerCard();
            g_pPostBagCache->PostBag_RecountCategoryOutFiles();
        }
    }
}

// FUNCTION: LOCO 0x4530c0
// Reads "<installPathPrefix><name>.usr" straight over the 0x120-byte record starting at kindTag
// -- the same opaque blob Profile_SavePlayerUserFile writes. The name is stashed on the stack and
// restored around the read because the record covers `name` too, and the file's copy is not
// trusted to be the one the caller just asked for.
bool LocalPlayerIdentity::LoadProfile()
{
    char szSavePath[1284];
    char szPath[1284];
    DWORD dwRead;
    DWORD dwWritten;
    char szName[13];

    strcpy(szName, name);
    kindTag = 0x66;
    name[0] = '\0';
    sessionId = 0;
    clientId = 0;
    nextPostSeqId = 0;
    strcpy(name, szName);

    wsprintfA(szPath, "%s%s.usr", g_pInstallPathPrefix, szName);
    HANDLE hFile = CreateFileA(szPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    if (!ReadFile(hFile, &kindTag, 0x120, &dwRead, NULL)) {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);

    // A tag that survived the read means the file really was one of ours; anything else is a
    // foreign/garbage .usr, so the record is re-cleared, given a fresh client id and written back.
    if (kindTag != 0x66) {
        kindTag = 0x66;
        name[0] = '\0';
        sessionId = 0;
        clientId = 0;
        nextPostSeqId = 0;
        strcpy(name, szName);

        int nNextId = g_pIniFile->ReadInt("CLIENT", "NextId", 0);
        if (nNextId > 999) {
            nNextId = 1;
        }
        g_pIniFile->WriteInt("CLIENT", "NextId", nNextId + 1);
        clientId = nNextId;

        wsprintfA(szSavePath, "%s%s.usr", g_pInstallPathPrefix, name);
        HANDLE hSave = CreateFileA(szSavePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                                   FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (hSave != INVALID_HANDLE_VALUE) {
            // The dead `test eax,eax` on WriteFile's result is real source -- same tail-merged
            // two-arm shape as Profile_SavePlayerUserFile below, see its autopsy.
            if (!WriteFile(hSave, &kindTag, 0x120, &dwWritten, NULL)) {
                CloseHandle(hSave);
                return true;
            }
            CloseHandle(hSave);
        }
    }
    return true;
}

// EFFECTIVE MATCH -- DIFF(3) at the exact original length. The three bytes are the ORDER of
// `push esi` (CloseHandle's own argument) against the dead `test eax,eax` on WriteFile's result:
// the original schedules the push first, this compile the test. Pure scheduling, the same class
// already parked on 0x406ba0 (docs/PARKED.md, v429).
//
// What the branch below IS load-bearing for: without SOME branch on WriteFile's result, cl drops
// the `test` entirely and the function comes out 2 bytes SHORT at DIFF(12)/DIFF(15). Four
// no-branch spellings were refuted (a discarded `BOOL`/`bool` local, an empty `if (...) {}`, and
// an empty `if (!...) {}` -- cl dead-codes the flag set in all four). Three BRANCHING spellings
// are byte-for-byte identical to each other and to what is written here (early-return on
// failure, `if (!ok) close; else close;`, and a `?:` of two CloseHandle calls), so the arms
// cross-jump into one block and only the dead test survives -- the tail-merge class from
// docs/CODEGEN.md. The early-return form is kept as the most plausible human source.

// FUNCTION: LOCO 0x4532a0
// Writes the whole identity record out as "<installPathPrefix><name>.usr". The payload is ONE
// opaque 0x120-byte blob starting at kindTag -- covering abRawProfileTail, which this build
// round-trips verbatim and never interprets -- so there is no per-field serialization here.
void __fastcall Profile_SavePlayerUserFile(LocalPlayerIdentity *pIdentity)
{
    char szPath[1284];
    DWORD dwWritten;

    wsprintfA(szPath, "%s%s.usr", g_pInstallPathPrefix, pIdentity->name);
    HANDLE hFile = CreateFileA(szPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                               FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        if (!WriteFile(hFile, &pIdentity->kindTag, 0x120, &dwWritten, NULL)) {
            CloseHandle(hFile);
            return;
        }
        CloseHandle(hFile);
    }
}

// EFFECTIVE MATCH -- DIFF(3), the identical scheduling residual; see 0x4532a0's autopsy above.
//
// FUNCTION: LOCO 0x453320
// Hands back the next "<clientId><seq>" postbag/card id string, bumps the counter (wrapping at
// 9999) and immediately re-saves the profile so the counter survives a crash. The re-save is
// the same blob write Profile_SavePlayerUserFile does, spelled out again rather than called --
// see the note below.
char *__fastcall AllocNextPostSeqIdString(LocalPlayerIdentity *pIdentity)
{
    char szPath[1284];
    DWORD dwWritten;

    wsprintfA(pIdentity->postSeqIdString, "%03d%04d", pIdentity->clientId,
              pIdentity->nextPostSeqId);
    pIdentity->nextPostSeqId++;
    if ((int)pIdentity->nextPostSeqId > 9999) {
        pIdentity->nextPostSeqId = 0;
    }

    wsprintfA(szPath, "%s%s.usr", g_pInstallPathPrefix, pIdentity->name);
    HANDLE hFile = CreateFileA(szPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                               FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        if (!WriteFile(hFile, &pIdentity->kindTag, 0x120, &dwWritten, NULL)) {
            CloseHandle(hFile);
            return pIdentity->postSeqIdString;
        }
        CloseHandle(hFile);
    }
    return pIdentity->postSeqIdString;
}
