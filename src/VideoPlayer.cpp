// VideoPlayer -- the AVI/MCI playback wrapper used by the boot/splash sequence. See
// src/VideoPlayer.h for the class writeup and docs/subsystems.md's "AVI/MCI video" section.
//
// The whole class is four functions: the ctor, OpenAndPlay and CloseWindow live here; the
// destructor is inline in the header (see the note on it there, and 0x454330). Playback itself is entirely MCIWnd's job -- this class only
// creates the child window, points it at a file, sizes its destination rect and asks the
// underlying MCI device to play with MCI_NOTIFY so the parent gets the 0x3b9 "video finished"
// post when the clip ends (SplashWnd::AppWndProc's own 0x3b9 case drives the boot sequence off
// exactly that notify).
#include <windows.h>
#include <mmsystem.h>
#include <vfw.h>
#include <string.h>

#include "VideoPlayer.h"
#include "WindowBase.h"

// The MCI AVI device's "play inside the given window" flag. Video for Windows spells this
// MCI_MCIAVI_PLAY_WINDOW in mciavi.h, which is not part of the VC5 SDK headers this project
// builds against, so the literal stands in for it here.
#define LOCO_MCI_MCIAVI_PLAY_WINDOW 0x01000000L

// FUNCTION: LOCO 0x4544a0
// Tear the MCIWnd child down: MCI_CLOSE releases the device, WM_CLOSE destroys the window.
// Kept out of line (and public) because SplashWnd calls it directly to stop a clip early, before
// it unsubclasses the HWND and deletes the player.
void VideoPlayer::CloseWindow() {
    if (hwndVideo != NULL) {
        SendMessageA(hwndVideo, MCI_CLOSE, 0, 0);
        SendMessageA(hwndVideo, WM_CLOSE, 0, 0);
        hwndVideo = NULL;
    }
}

// FUNCTION: LOCO 0x454250
// Probe the clip with CreateFileA before committing to it: MCIWnd is created with
// MCIWNDF_NOERRORDLG, so a missing file would otherwise fail silently and the boot sequence would
// stall waiting for a notify that never comes. On a miss we post the parent's own "video
// finished" message immediately and leave hwndVideo NULL, which is exactly what the sequence
// needs to move on. On a hit the 640x480 clip rect is centered in the parent's client area and
// handed to OpenAndPlay.
VideoPlayer::VideoPlayer(HINSTANCE hInstanceArg, HWND hwndParentArg, char *pszFilename) {
    hwndVideo = NULL;
    strcpy(szFilename, pszFilename);
    hwndParent = hwndParentArg;
    hInstance = hInstanceArg;

    HANDLE hFile = CreateFileA(szFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        PostMessageA(hwndParent, 0x3b9, 0, 0);
        return;
    }
    CloseHandle(hFile);

    RECT rcClient;
    GetClientRect(hwndParent, &rcClient);
    destRect.left = 0;
    destRect.right = 640;
    destRect.top = 0;
    destRect.bottom = 480;
    CenterRectInRect(&rcClient, &destRect);
    OpenAndPlay(szFilename, &destRect);
}

// FUNCTION: LOCO 0x454330 (??_GVideoPlayer scalar deleting dtor -- compiler-generated; the class's
// only virtual, and the only form of the destructor the linker kept. ~VideoPlayer() itself inlines
// into this wrapper, so no separate ??1 COMDAT is ever referenced -- see the inline definition in
// src/VideoPlayer.h.)

// FUNCTION: LOCO 0x454380 // EFFECTIVE MATCH -- 254 B vs 274, insns 97/96, DIFF(216)
// Structure, call order and stack layout all agree with 0x454380 instruction for instruction; the
// whole residual is one register-allocation decision and its ripple. The original spills dwNotify
// to its own stack slot (esp+0xc) and keeps NO dedicated zero register -- it makes zeros with a
// short-lived `xor eax,eax` for the three MCI_PLAY_PARMS stores and then uses literal `push 0` /
// `mov [esi+4],0` everywhere else. Ours keeps dwNotify in ebp and hoists one `xor edi,edi` for the
// whole body, so every literal zero becomes `push edi` and the null test becomes `cmp eax,edi`
// instead of `test eax,eax`. That is the documented zero-register-residency class (docs/CODEGEN.md)
// and it is exactly one instruction of difference. Probed and inert: LOWORD() vs `& 0xffff` (the
// mask is right -- the original does a 32-bit load then `and eax,0xffff`, LOWORD emits a 16-bit
// `mov bp,` load instead, so this spelling is kept), and swapping the dwNotify/mciPlay declaration
// order (byte-identical, total 140089 either way).
//
// Create the MCIWnd child over the destination rect, open the clip in it, and start playback.
// The window is styled WS_CHILD with every piece of MCIWnd furniture turned off (no toolbar, no
// context menu, no auto-resize, no error dialogs) because the splash screen wants a bare video
// surface it can subclass. MCI_NOTIFY is what makes the device post back to hwndParent when the
// clip ends -- and note it is the parent's handle that goes in dwCallback, LOWORD'd in the Win16
// style the surrounding code was ported from, not the MCIWnd child's.
void VideoPlayer::OpenAndPlay(char *pszFilename, RECT *pDestRect) {
    DWORD dwNotify = (DWORD)hwndParent & 0xffff;
    MCI_PLAY_PARMS mciPlay;

    mciPlay.dwCallback = 0;
    mciPlay.dwFrom = 0;
    mciPlay.dwTo = 0;

    if (hwndVideo != NULL) {
        SendMessageA(hwndVideo, MCI_CLOSE, 0, 0);
        SendMessageA(hwndVideo, WM_CLOSE, 0, 0);
        hwndVideo = NULL;
    }

    MCIWndRegisterClass();
    hwndVideo = CreateWindowExA(0, "MCIWndClass", "AVI",
                                WS_CHILD | MCIWNDF_NOERRORDLG | MCIWNDF_NOMENU |
                                    MCIWNDF_NOPLAYBAR | MCIWNDF_NOAUTOSIZEWINDOW,
                                pDestRect->left, pDestRect->top,
                                pDestRect->right - pDestRect->left,
                                pDestRect->bottom - pDestRect->top,
                                hwndParent, NULL, hInstance, NULL);
    if (hwndVideo != NULL) {
        SendMessageA(hwndVideo, MCIWNDM_OPENA, 0, (LPARAM)pszFilename);

        RECT rcDest;
        rcDest.right = pDestRect->right - pDestRect->left;
        rcDest.left = 0;
        rcDest.bottom = pDestRect->bottom - pDestRect->top;
        rcDest.top = 0;
        SendMessageA(hwndVideo, MCIWNDM_PUT_DEST, 0, (LPARAM)&rcDest);

        MCIDEVICEID mciId = SendMessageA(hwndVideo, MCIWNDM_GETDEVICEID, 0, 0);
        mciPlay.dwCallback = dwNotify;
        mciSendCommandA(mciId, MCI_PLAY, MCI_NOTIFY | LOCO_MCI_MCIAVI_PLAY_WINDOW,
                        (DWORD)&mciPlay);
    }
}
