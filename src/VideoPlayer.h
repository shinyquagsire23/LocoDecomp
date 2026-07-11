// VideoPlayer -- the tiny AVI/MCI playback wrapper the boot/splash sequence uses (vtable
// 0x4784c4, single slot; ctor 0x454250 with a 0x524-byte allocation, scalar-deleting dtor
// 0x454330). See docs/subsystems.md's "AVI/MCI video" section: the whole subsystem is three
// .avi files played by SplashWnd (IgSpin.avi, the ini-configured intro, legoSpin.avi).
//
// The object owns one MCIWnd child window (`hwndVideo`); SplashWnd subclasses that HWND to
// SplashVideoSubclassProc so any keypress/click can post its "skip video" message, and restores
// the saved proc before closing (see SplashWnd::EndActiveSession / SetState / ~SplashWnd).
#pragma once

#include <windows.h>

class VideoPlayer {
public:
    // 0x454250 -- stashes hInstance/hwndParent, copies pszFilename into szFilename, centers the
    // 640x480 destRect and opens the clip. src/VideoPlayer.cpp.
    VideoPlayer(HINSTANCE hInstanceArg, HWND hwndParentArg, char *pszFilename);

    HWND hwndVideo;        // +0x04 -- the MCIWnd child; NULL once CloseWindow has run
    RECT destRect;         // +0x08 -- centered 640x480 destination (0x425a50 centers it)
    HWND hwndParent;       // +0x18
    HINSTANCE hInstance;   // +0x1c
    char szFilename[0x504];// +0x20 -- inline copy of the path passed to the ctor

    // 0x454380 -- MCIWndRegisterClass + CreateWindowExA("MCIWndClass"), MCIWNDM_OPENA,
    // MCIWNDM_PUT_DEST, MCIWNDM_GETDEVICEID, mciSendCommandA(MCI_PLAY, MCI_NOTIFY|0x01000000).
    // Declared only.
    void OpenAndPlay(char *pszFilename, RECT *pDestRect);

    // 0x4544a0 -- tears the MCIWnd child down (MCIWNDM_CLOSE 0x804 then WM_CLOSE) and clears
    // hwndVideo. Separate from the destructor: SplashWnd calls it first, then deletes.
    // src/VideoPlayer.cpp.
    void CloseWindow();

    // Slot 0 -- the class's only virtual, and the reason this class has a vtable at all (every
    // known consumer reaches it as a plain `delete pVideoPlayer`). Defined IN-CLASS, i.e. inline:
    // that is what makes the body land inside the compiler-generated scalar deleting destructor
    // ??_GVideoPlayer at 0x454330 rather than in a ??1 COMDAT of its own, which is exactly what
    // the original does (same shape as ~DSound, see src/DSound.h). It repeats CloseWindow's body
    // instead of calling it because CloseWindow is genuinely out-of-line -- all five of its call
    // sites in SplashWnd's TU are real calls to 0x4544a0, so it is not an inline function and
    // VC5 will not fold it in here.
    virtual ~VideoPlayer() {
        if (hwndVideo != NULL) {
            SendMessageA(hwndVideo, MCI_CLOSE, 0, 0);
            SendMessageA(hwndVideo, WM_CLOSE, 0, 0);
            hwndVideo = NULL;
        }
    }
};
