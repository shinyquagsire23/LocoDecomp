// ScreenSaver -- the attract-mode / Windows-screen-saver controller (canonical class in
// ScreenSaver.h). One contiguous .text run at 0x448040..0x44898f, bracketed by
// ThumbnailBmp_IsLoaded (0x448030) and the SoundBankEntry ctor (0x448990).
//
// SaveGame_ScanSavFiles (0x448390) is physically part of THIS run -- GetLayoutFileName is its
// only caller and reaches it through ecx, i.e. it is a `this`-ignoring member. It is currently
// transcribed (and EXACT) in src/EditCardWnd.cpp as a free __stdcall; moving it here would
// rotate that TU's /Og state, so it stays put and Ghidra just address-boxes it into the
// ScreenSaver namespace. See docs/subsystems.md.
#include <windows.h>
#include <ddraw.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ScreenSaver.h"
#include "AppWindow.h"            // g_pApp/AppWindow
#include "PostBag.h"              // PostBagCrdFileNode
#include "RandRange.h"            // RAND_RANGE_MAYBE
#include "DPlaySessionMgr.h"      // DPlaySessionMgr/g_pDPlaySessionMgr
#include "DSoundChannel.h"        // g_pInstallPathPrefix
#include "GameNet.h"              // ThreadWrapper/g_pGameNetThread
#include "GameNetMsgQueue.h"      // NetSettings/g_pNetSettings
#include "IniFile.h"              // IniFile/g_pIniFile
#include "PeerTrainNode.h"        // PeerTrainNodePartial
#include "PeerTrainSlotQueueMaybe.h" // g_PeerTrainSlotQueue
#include "SplashWnd.h"           // g_pSplashWnd

// Cross-TU callees (names kept in sync with Ghidra; see their owning TUs).
unsigned char __stdcall DSound_GetOrCreateManager(); // 0x45b7e0, src/DSound.cpp


// FUNCTION: LOCO 0x448040
ScreenSaver::ScreenSaver() {
    bScreenSaverMode = 0;
    hInstance = (HINSTANCE)0;
    nMouseMoveCount = 0;
    nTickCounter = 0x400;
    hwnd = (HWND)0;
    bDismissing = '\0';
    pfnGetPasswordStatus = (PFNGETPASSWORDSTATUS)0;
    pfnVerifyScreenSavePwd = (PFNVERIFYSCREENSAVEPWD)0;
    hPasswordCpl = (HMODULE)0;
    hbrBackground = CreateSolidBrush(0xa8c4d8);
}

// FUNCTION: LOCO 0x448080
ScreenSaver::~ScreenSaver() {
    DeleteObject(hbrBackground);
    hbrBackground = (HBRUSH)0;
    if (hwnd != (HWND)0) {
        DestroyWindow(hwnd);
        hwnd = (HWND)0;
    }
    if (hPasswordCpl != (HMODULE)0) {
        FreeLibrary(hPasswordCpl);
        hPasswordCpl = (HMODULE)0;
    }
}

// FUNCTION: LOCO 0x4480c0
// Startup hook, called from LocoWinMain right after ParseCommandLine; a zero return aborts
// startup with WPARAM 0 (which never happens -- this always returns 1).
//
// The mode test reads the singleton through its own global name rather than `this` (the
// original emits an absolute `mov eax,ds:0x4a9918`, not `[ecx+8]` -- ScreenSaver::Tick, one
// function later, does the opposite). Both spellings are in the original source.
char ScreenSaver::InitAndPlayIntroMusic() {
    CHAR szMusicPath[0x504];

    switch (g_screenSaver.bScreenSaverMode) {
    case 1:
        LoadPasswordProvider();
        if (g_pIniFile->ReadInt("ScreenSaver", "Sound", 0) == 0) {
            wsprintfA(szMusicPath, "%svideo\\music.wav", g_pInstallPathPrefix);
            PlaySoundA(szMusicPath, (HMODULE)0, SND_ASYNC | SND_LOOP);
        }
        break;
    }
    return 1;
}

// FUNCTION: LOCO 0x448120
// Per-frame tick (called unconditionally by the frame driver at 0x45c3c0). In screen-saver
// mode, every 0x800 frames each parked train re-picks one of its two selection endpoints --
// 3/7 of the time endpoint A, otherwise endpoint B -- so the attract-mode layout keeps
// shuffling its rolling stock.
void ScreenSaver::Tick() {
    int i;
    PeerTrainNodePartial **ppNode;

    if (bScreenSaverMode == 1 && ++nTickCounter >= 0x800) {
        nTickCounter = 0;
        // sic: three slots, not four -- every other sweep over g_PeerTrainSlotQueue.aSlots
        // (0x45c3c0's two SetSoundStateMaybe loops, ResetAllFields, ...) runs 0..3, so the
        // train in slot 3 never re-randomizes in screen-saver mode. See docs/engine-bugs.md.
        ppNode = g_PeerTrainSlotQueue.aSlots;
        for (i = 0; i < 3; i++) {
            if (*ppNode != 0) {
                switch (rand() / 0xfff) {
                case 0:
                case 1:
                case 2:
                    (*ppNode)->PeerTrainNode_UpdateSelectedCar((*ppNode)->wSelectedCarIdAMaybe);
                    break;
                case 3:
                case 4:
                case 5:
                case 6:
                default:
                    (*ppNode)->PeerTrainNode_UpdateSelectedCar((*ppNode)->wSelectedCarIdBMaybe);
                    break;
                }
            }
            ppNode++;
        }
    }
}

// FUNCTION: LOCO 0x4481b0
// Picks the .sav layout the screen saver loads and writes it into pszOut as a path relative to
// the install root. `[ScreenSaver] Layout` in lego.ini names it outright (defaulting to
// "ScrSaver\saver.sav"); `[ScreenSaver] Random` instead enumerates "<install>ScrSaver\*.sav"
// and draws one of those filenames uniformly, falling back to the configured Layout value when
// the directory is empty.
//
// ⚠ The nCount-1 sign test around the range draw is not defensive coding, it is the
// RAND_RANGE_MAYBE call-site idiom: the macro does not order its own arguments, so a call site
// whose bounds can invert branches and passes them swapped (see src/RandRange.h). Here the
// inverted arm is dead by construction -- nCount is a list length -- but the original emits it.
void ScreenSaver::GetLayoutFileName(char *pszOut)
{
    int nPick;
    char szLayout[0x80] = "";

    // The named local is load-bearing: it is what makes VC5 test the result with
    // `cmp eax,<zero-reg>` instead of `test eax,eax` (docs/CODEGEN.md's v360 lever).
    int bRandom = g_pIniFile->ReadInt("ScreenSaver", "Random", 0);
    if (bRandom != 0) {
        PostBagCrdFileNode *pHead = SaveGame_ScanSavFiles(1);
        g_pIniFile->ReadString("ScreenSaver", "Layout", "ScrSaver\\saver.sav", szLayout,
                               sizeof(szLayout));

        int nCount = 0;
        for (PostBagCrdFileNode *p = pHead; p != NULL; p = p->pNext) {
            nCount++;
        }

        srand(time(NULL));
        if (nCount - 1 >= 0) {
            nPick = RAND_RANGE_MAYBE(0, nCount - 1);
        } else {
            nPick = RAND_RANGE_MAYBE(nCount - 1, 0);
        }

        int i = 0;
        PostBagCrdFileNode *pNode = pHead;
        while (pNode != NULL) {
            if (i == nPick) {
                strcpy(szLayout, pNode->szPath);
            }
            PostBagCrdFileNode *pTemp = pNode;
            pNode = pNode->pNext;
            ::operator delete(pTemp);
            i++;
        }

        wsprintfA(pszOut, "ScrSaver\\%s", szLayout);
    } else {
        g_pIniFile->ReadString("ScreenSaver", "Layout", "ScrSaver\\saver.sav", szLayout,
                               sizeof(szLayout));
        strcpy(pszOut, szLayout);
    }
}

// FUNCTION: LOCO 0x448350
// EFFECTIVE MATCH -- insns 13/13, align 0, byte_diff 6, and the ONLY disagreement is which
// register holds the SECOND `g_pNetSettings` load: the original re-uses eax (the register its
// own first load went to), we take ecx (which the very next instruction then overwrites with
// g_pDPlaySessionMgr). Pure allocator coin-flip of the documented class -- the instruction
// sequence, scheduling and the tail `jmp` are all identical. See docs/PARKED.md.
//
// Drops the front end straight into the demo/attract session: skip the setup wizard, hand the
// splash window its state change, slow the net worker's tick, put the session manager in mode 1
// and the worker thread at THREAD_PRIORITY_BELOW_NORMAL, then bring the sound manager up.
void ScreenSaver::EnterDemoSession() {
    g_pNetSettings->bSkipSetupWizardMaybe = 1;
    g_pSplashWnd->StartGameNetThread();
    g_pNetSettings->nTickSleepMs = 0x32;
    g_pDPlaySessionMgr->SetMode(1);
    g_pGameNetThread->SetPriority(-1);
    DSound_GetOrCreateManager();
}

// Inlined into all four of FilterMessage's dismiss paths (no COMDAT of its own). Asks the
// password provider whether the user has a screen-saver password configured; a provider that
// never loaded (no password.cpl, or the registry did not name a status entry point) counts as
// "no password".
inline unsigned char ScreenSaver::IsPasswordSetMaybe() {
    if (pfnGetPasswordStatus == 0) {
        return 0;
    }
    // Spelled as a branch, not `return (... & 1) == 1`: the original leaves the true edge's
    // `al` holding the 1 that `and eax,1` already put there and emits NO instruction for
    // `return 1`, whereas returning the comparison makes VC5 normalize it the long way
    // (`dec al; neg al; sbb eax,eax; inc eax`).
    if ((pfnGetPasswordStatus(1) & 1) == 1) {
        return 1;
    }
    return 0;
}

// Inlined into all four of FilterMessage's dismiss paths (no COMDAT of its own). Tears the
// saver session down, optionally behind the system password prompt, and hands the caller back
// the message-specific result code it should return.
inline int ScreenSaver::DismissMaybe(int nResult) {
    if (bDismissing) {
        return nResult;
    }
    bDismissing = 1;
    if (!IsPasswordSetMaybe()) {
        bDismissing = 1;
        PostMessageA(g_pApp->hwndOwner, WM_CLOSE, 0, 0);
        g_screenSaver.nMouseMoveCount = 0;
        return nResult;
    }
    if (pfnVerifyScreenSavePwd(g_pApp->hwndOwner) == 0) {
        // Wrong password -- stay up and let the next event try again.
        bDismissing = 0;
        g_screenSaver.nMouseMoveCount = 0;
        return nResult;
    }
    PostMessageA(g_pApp->hwndOwner, WM_CLOSE, 0, 0);
    g_screenSaver.nMouseMoveCount = 0;
    return nResult;
}

// FUNCTION: LOCO 0x4484a0
// PARTIAL -- DIFF 841, len 1062 vs the original's 848. ⚠ The headline numbers are misleading:
// our CODE is 792 bytes to the original's 814 (we are 22 bytes SHORTER, and the four inlined
// dismiss blocks line up instruction-for-instruction). **The entire 214-byte gap is ONE switch
// jump-table shape decision.** The original emits a decision tree over {0x1c}, {0x20}, {0x100},
// {0x104-0x106}, {0x112} and then ONE 8-entry dword table for the dense 0x200..0x207 cluster
// (32 bytes at 0x4487d0, with 0x202/0x203/0x205/0x206 pointing at the shared `return 0`). Ours
// instead swallows WM_SYSCOMMAND into that cluster and tables the whole 0x112..0x207 range as a
// two-level BYTE-INDEX table: a 24-byte dword table plus **246 bytes** of byte indices. The
// top-level tree pivot moves with it (ours splits at 0x20, the original at 0x1c).
//
// Probed and refuted, do NOT retry: WM_ACTIVATEAPP guard polarity (`wParam == 0` + dismiss vs
// `wParam != 0` + `return 0`) is a no-op, VC5 normalizes it (identical score); an explicit
// `default: return 0;` inside the switch instead of a trailing `return 0` after it is also a
// no-op (identical score) -- unlike the INNER switch, where `case SC_SCREENSAVE: default:`
// sharing one label list IS load-bearing (see ScreenSaver::Tick's own bullet in
// docs/CODEGEN.md); and moving `case WM_SYSCOMMAND` to last makes it WORSE (841 -> 845).
// The case-body EMISSION order already matches the original exactly, so source order of the
// case groups is not the lever. Densifying the cluster with four extra `return 0` cases in the
// 0x202..0x206 gap (WM_LBUTTONUP/DBLCLK/WM_RBUTTONUP/DBLCLK) was also tried and is WORSE
// (841 -> 846, len 1070) -- the byte table just grows. So VC5's decision to absorb 0x112 is not
// reachable from the case list at all; the next axis to try is the SHAPE of the 0x112 arm
// itself (e.g. hoisting the inner switch into its own non-inline helper, which would leave the
// 0x112 arm a bare call and might drop it out of the cluster). See docs/PARKED.md.
//
// The screen-saver window's message filter, called from the app window procedure (0x461950)
// only while bScreenSaverMode is set. The return value is an ACTION CODE, not an LRESULT:
// 0 = not handled (the caller runs its own default handling), 1 = handled, return 0,
// 2 and 3 = handled, two distinct caller-side epilogues.
int ScreenSaver::FilterMessage(HWND hWnd, unsigned int uMsg, unsigned int wParam, int lParam) {
    switch (uMsg) {
    case WM_ACTIVATEAPP:
        // Losing activation to another app tears the saver down; gaining it does not.
        if (wParam == 0) {
            return DismissMaybe(2);
        }
        return 0;

    case WM_SETCURSOR:
        // The saver hides the cursor; force it visible again for the dismiss prompt.
        SetCursor(LoadCursorA((HINSTANCE)0, IDC_ARROW));
        while (ShowCursor(TRUE) < 0) {
        }
        return 1;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_SYSCHAR:
        return DismissMaybe(3);

    case WM_SYSCOMMAND:
        switch (wParam & 0xfff0) {
        case SC_CLOSE:
            OutputDebugStringA("sc_close intercepted");
            return 3;
        case SC_TASKLIST:
            OutputDebugStringA("sc_tasklist intercepted");
            return 3;
        case SC_SCREENSAVE:
        default:
            return 2;
        }

    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        return DismissMaybe(3);

    case WM_MOUSEMOVE:
        // A stray mouse jog must not kill the saver -- it takes three.
        if (bDismissing) {
            return 1;
        }
        if (++g_screenSaver.nMouseMoveCount > 2) {
            return DismissMaybe(1);
        }
        return 1;

    default:
        return 0;
    }
}

// FUNCTION: LOCO 0x4487f0
// Brings the Win95 screen-saver password provider up, once. Everything is guarded so the
// function is idempotent: the module is only loaded if it is not already held, and each entry
// point is only resolved if it is still null.
//
// The provider is ALWAYS "<system directory>\password.cpl" -- the
// HKLM\...\PwdProvider\SCRSAVE\ProviderPath value is read into a 0x504-byte buffer and then
// never looked at again, an original dead read (the shipped Win95 provider lives in the system
// directory anyway, so the two agree in practice). Only the sibling GetPasswordStatus value is
// really used: it names the export FilterMessage calls to ask "does this user have a
// screen-saver password?". The second entry point, VerifyScreenSavePwd, is hard-coded.
void ScreenSaver::LoadPasswordProvider()
{
    CHAR szProviderPath[0x504] = "";

    CHAR szCplPath[0x504] = "";

    CHAR szStatusExport[0x400] = "";

    if (hPasswordCpl == NULL) {
        GetSystemDirectoryA(szCplPath, sizeof(szCplPath));
        strcat(szCplPath, "\\");
        strcat(szCplPath, "password.cpl");

        g_pApp->ReadHklmValue("System\\CurrentControlSet\\control\\PwdProvider\\SCRSAVE",
                              (LPBYTE)szProviderPath, sizeof(szProviderPath), "ProviderPath");
        g_pApp->ReadHklmValue("System\\CurrentControlSet\\control\\PwdProvider\\SCRSAVE",
                              (LPBYTE)szStatusExport, sizeof(szStatusExport),
                              "GetPasswordStatus");

        UINT uPrevMode = SetErrorMode(SEM_NOOPENFILEERRORBOX);
        hPasswordCpl = LoadLibraryA(szCplPath);
        SetErrorMode(uPrevMode);
        if (hPasswordCpl == NULL) {
            return;
        }
    }

    if (pfnGetPasswordStatus == NULL) {
        pfnGetPasswordStatus =
            (PFNGETPASSWORDSTATUS)GetProcAddress(hPasswordCpl, szStatusExport);
    }
    if (pfnVerifyScreenSavePwd == NULL) {
        pfnVerifyScreenSavePwd =
            (PFNVERIFYSCREENSAVEPWD)GetProcAddress(hPasswordCpl, "VerifyScreenSavePwd");
    }
}

// FUNCTION: LOCO 0x448970
// Asks the app window to shut down (WM_USER+5; the owner's window procedure answers with
// Sleep(0x14) + the quit routine at 0x45e400). Used when the screen saver cannot bring its
// layout up.
void ScreenSaver::PostQuitRequest() {
    PostMessageA(g_pApp->hwndOwner, 0x405, 0, 0);
}

#ifdef LOCO_PORT
// ─── PORT SCAFFOLDING (no original counterpart) ────────────────────────────────
// XC 1 of 13: g_screenSaver (DAT_004a9910), ScreenSaver::ScreenSaver (0x448040).
//
// The original constructs this global from the CRT's C++ dynamic-initializer table (.CRT$XC),
// which the port's zero-filled .bss mirror has no equivalent of. Declared in
// port/PortGlobalCtors.h, called from link/init_globals.cpp -- see either for the full story.
#include <new.h>
#include "PortGlobalCtors.h"

void Port_Construct_g_screenSaver(void) {
    new (&g_screenSaver) ScreenSaver();
}
#endif // LOCO_PORT
