// ScreenSaver -- the attract-mode / Windows-screen-saver controller singleton (g_screenSaver,
// 0x4a9910, 0x80 bytes). LEGO LOCO ships as a dual-personality executable: launched with the
// `-s`/`/s`/`s` command-line flag (see AppWindow::ParseCommandLine, 0x406790) it runs as a
// genuine Win32 screen saver -- full-screen demo session, no setup wizard, dismissed by the
// first key press / mouse button / third mouse move, and (when the user has one configured)
// gated behind the system screen-saver password.
//
// The password gate is the canonical Win95 `password.cpl` dance, implemented in
// LoadPasswordProvider (0x4487f0): read `HKLM\System\CurrentControlSet\control\PwdProvider\...`
// for `ProviderPath` and `GetPasswordStatus`, LoadLibrary `<sysdir>\password.cpl`, then
// GetProcAddress the registry-named status entry point (+0x70) and `VerifyScreenSavePwd`
// (+0x74). FilterMessage (0x4484a0) calls the first with 1 to ask "is a password set?" and, if
// so, the second with the app window to prompt for it before letting the session close.
//
// ⚠ This object is a STATIC instance, not a heap allocation, so there is no `operator new(N)`
// sizeof oracle. 0x80 comes from the next known singleton (the NetSessionEventQueue instance at
// 0x4a9990) sitting exactly 0x80 bytes later; the last field the binary actually touches is
// hPasswordCpl at +0x78, so anything in [0x7c, 0x80] would fit the evidence.
//
// ⚠ bScreenSaverMode (+0x08) is the SAME storage 24 call sites across the binary read as a
// free global at 0x4a9918 -- an ordinary `g_screenSaver.bScreenSaverMode` member access from
// another TU compiles to an absolute address, which is exactly what the disassembly shows.
// It was modelled as a standalone `g_bCmdlineSFlagSet` int until v366 read the ctor and found
// it being zeroed through `this`. The same applies to hInstance (+0x0c, ex-`DAT_004a991c`) and
// nMouseMoveCount (+0x10, ex-`DAT_004a9920`).
#pragma once

#include <windows.h>

// The two password.cpl entry points LoadPasswordProvider resolves. Both are plain WINAPI, per
// FilterMessage's own call sites (`push 1; call eax` / `push hwnd; call [esi+0x74]`, neither
// followed by caller-side stack cleanup). The status one is NOT called by name -- its export
// name comes out of the registry's `GetPasswordStatus` value.
typedef BOOL (WINAPI *PFNGETPASSWORDSTATUS)(UINT uMode);
typedef BOOL (WINAPI *PFNVERIFYSCREENSAVEPWD)(HWND hwnd);

struct PostBagCrdFileNode; // src/PostBag.h

struct ScreenSaver {
    HWND hwnd;               // +0x00 -- the full-screen saver window (DestroyWindow'd by the dtor)
    char bDismissing;        // +0x04 -- a dismiss is already in flight; suppresses re-entry
    int bScreenSaverMode;    // +0x08 -- the `-s` command-line flag (see the header note above)
    HINSTANCE hInstance;     // +0x0c -- app HINSTANCE mirror, stored once by LocoWinMain
    int nMouseMoveCount;     // +0x10 -- WM_MOUSEMOVEs seen; the 3rd one dismisses
    int nTickCounter;        // +0x14 -- Tick's frame counter, seeded to 0x400 so the first
                             //   car re-randomize lands half a period in
    HBRUSH hbrBackground;    // +0x18 -- solid 0xa8c4d8 (BGR) saver-window background
    unsigned char Unk0x1c[0x54]; // +0x1c -- never read or written by any shipped code path
    PFNGETPASSWORDSTATUS pfnGetPasswordStatus;     // +0x70 -- entry point named by the registry
    PFNVERIFYSCREENSAVEPWD pfnVerifyScreenSavePwd; // +0x74 -- password.cpl `VerifyScreenSavePwd`
    HMODULE hPasswordCpl;    // +0x78 -- password.cpl module handle (FreeLibrary'd by the dtor)
    unsigned int Unk0x7c;    // +0x7c -- see the sizeof note above

    ScreenSaver();           // 0x448040
    ~ScreenSaver();          // 0x448080
    char InitAndPlayIntroMusic(); // 0x4480c0
    void Tick();                  // 0x448120
    void GetLayoutFileName(char *pszOut); // 0x4481b0

    // A `this`-ignoring member (the sole call site, GetLayoutFileName, still loads ecx = this),
    // physically inside this class's .text run but DEFINED in src/EditCardWnd.cpp, where it was
    // first matched next to its structural twin PostBag_ScanCategoryCrdFiles. It stays there --
    // moving the body would rotate that TU's /Og state. See src/PostBag.h for the node type.
    PostBagCrdFileNode *SaveGame_ScanSavFiles(char bScrSaver); // 0x448390

    void EnterDemoSession();      // 0x448350
    int FilterMessage(HWND hWnd, unsigned int uMsg, unsigned int wParam, int lParam); // 0x4484a0
    void LoadPasswordProvider();  // 0x4487f0
    void PostQuitRequest();       // 0x448970

    // Inlined-only helpers -- they have no COMDAT of their own in Loco.exe, they exist because
    // FilterMessage's four dismiss paths are byte-for-byte the same block with a different
    // return value. The `unsigned char` return on IsPasswordSetMaybe is LOAD-BEARING: it is
    // what reproduces the original's materialize-then-test (`xor al,al; test al,al; je`) at
    // each inline site rather than a folded branch. See docs/CODEGEN.md's byte-predicate lever.
    unsigned char IsPasswordSetMaybe();
    int DismissMaybe(int nResult);
};

extern ScreenSaver g_screenSaver; // DAT_004a9910
