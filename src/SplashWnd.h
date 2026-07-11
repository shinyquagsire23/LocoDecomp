// SplashWnd -- the front-end/boot window singleton (*g_pSplashWnd, 0x4fd378). A real
// WindowBase-derived Win32 window (vtable 0x4779f8, installed by its own ctor at 0x420321);
// sizeof 0x224 (548), pinned by the Ghidra struct of the same name. See docs/subsystems.md's
// "SplashWnd and the boot video sequence" section.
//
// Promoted from the old padded `SplashWndPartial` view (which lived in SplashWndMaybe.h and
// modeled only pDrawTargetMaybe/pApplSetupWnd behind a 0x1ec-byte pad) once
// SplashWnd::OnLButtonDown was transcribed and needed the real WindowBase base plus the
// button-rect block. The WindowBase base is now a REAL base class, not a pad: three separate
// consumers reach WindowBase members through this object (OnLButtonDown's own hwndSelf /
// pPointCursorRect / pPointCursorDesc / vtable-slot-0xc+0x10 dispatches, and
// PopupWndBase.cpp's bModalCaptureActive + hwndSelf reads, which previously had to smuggle in
// a file-local `extern WindowBase *g_pSplashWnd;` -- a lint-class-I desync hazard, now gone).
//
// Vtable slot map (dumped from 0x4779f8, cross-checked against WindowBase.h's slot table by six
// independent inherited-default matches -- 0xc/0x10/0x14/0x28 plus OnMouseActivate@0x5c,
// OnSize@0x68, OnPaint@0x6c, OnSetCursor@0x70, OnEraseBkgnd@0x78, OnDestroy@0x7c -- and by
// Ghidra's own independently-derived `SplashWnd::OnKeyDown` name sitting at slot 0x54):
//   0x00 0x4203a0 scalar-deleting dtor   0x04 0x420860 EndActiveSession
//   0x08 0x4206b0 BeginModalCapture      0x1c 0x421200 RefreshClientClipRect
//   0x20 0x421be0 OnActivate             0x24 0x421eb0 OnIdlePump
//   0x2c 0x420ec0 OnUnhandledMessage     0x38 0x422930 OnLButtonDown  <-- transcribed
//   0x40 0x4323c0 OnRButtonDown          0x50 0x422d80 OnMouseMove
//   0x54 0x420bb0 OnKeyDown              0x80 0x422610 OnClose
//   0x90 0x420e90 OnActivateApp
// NOTE: Ghidra named 0x422930 `SplashWnd::OnLButtonUp` until this session -- that was WRONG (the
// pointer lives at 0x477a30, i.e. slot 0x38 off the 0x4779f8 base, the WindowBase-wide
// WM_LBUTTONDOWN convention slot; WM_LBUTTONUP is slot 0x3c and SplashWnd leaves it at the
// DefWindowProcStub default 0x422ea0). Corrected DB-side, so src and Ghidra now agree.
#pragma once

#include <windows.h>

#include "VideoPlayer.h"
#include "WindowBase.h"  // the real base class; also supplies LocoBitmap/CursorDesc

class ApplSetupWnd;  // src/ApplSetupWnd.h -- the application-setup page (+0x220)
class NetSetupWnd;   // src/NetSetupWnd.h -- the connection-setup page (+0x21c)

class SplashWnd : public WindowBase {
public:
    // Boot-sequence state machine; 0 = "not started yet" (OnLButtonDown kicks it to 7 and
    // swallows the click). Driven by SetState (0x4208f0).
    int state;                          // +0xe8
    int subState;                       // +0xec
    int Unk0xf0;                        // +0xf0
    // Gates the whole click dispatch -- clicks are ignored until the boot sequence has finished
    // building the front-end and armed input.
    unsigned char bReadyForInputMaybe;  // +0xf4
    unsigned char pad0xf5[3];           // +0xf5
    HICON hIcon;                        // +0xf8

    // --- the front-end's eight screen rects, one contiguous block +0xfc..+0x17c ---
    // "Play alone" -- sets g_pNetSettings->bSkipSetupWizardMaybe and drops the session to
    // DPlaySessionMgr mode 3 (disconnected).
    RECT rectPlayAlone;                 // +0xfc
    // "Connect online" -- the inverse of rectPlayAlone (clears bSkipSetupWizardMaybe, session
    // mode 0). Only live while g_pNetSettings->Unk0x10Maybe != 0, i.e. a provider list exists.
    RECT rectConnectOnline;             // +0x10c
    // The two halves of a remembered-choice radio pair, driving
    // g_pNetSettings->bUseSecondaryRememberedChoice (on / off respectively). Both are dead
    // while bSkipSetupWizardMaybe is set.
    RECT rectRememberChoiceOn;          // +0x11c
    RECT rectRememberChoiceOff;         // +0x12c
    // The two labelled command buttons. Each owns a normal/pressed CursorDesc+LocoBitmap pair
    // further down; a click draws the pressed art, plays sound 0x5015, sleeps 150 ms, then acts.
    RECT rectEnterLabel;                // +0x13c
    RECT rectEscLabel;                  // +0x14c
    // The player-name EDIT child's own screen rect. Create (0x4204d0) reads all four members to
    // build hwndChild's CreateWindowExA position/extent (x=.left, y=.top, w=.right-.left,
    // h=.bottom-.top), which is what pins the name.
    RECT rectNameField;                 // +0x15c
    // The composited backdrop's own origin -- OffsetRect'd onto a label rect to turn it into the
    // matching source rect inside pDrawTargetMaybe.
    RECT rectBackground;                // +0x16c

    // Laid out by RefreshClientClipRect in the same pass as every rect above -- a 680x680 square
    // at design coords (300,172)-(980,852), backdrop-relative like the others. NOTHING in the
    // whole binary reads it back (checked by sweeping the TU's disasm for `+0x17c`), so its role
    // is unrecovered; kept as a real RECT because its layout site proves the type.
    RECT rectUnk0x17c;                  // +0x17c
    // Latch for the whole +0x190..+0x1f0 art block below: EnsureArtLoaded sets it after
    // realizing all 12 descriptor/bitmap pairs plus pDrawTargetMaybe, ReleaseArt clears it
    // after freeing them. Both are idempotent because of it.
    unsigned char bArtLoaded;               // +0x18c
    unsigned char pad0x18d[0x190 - 0x18d];  // +0x18d

    // ---- the front-end's 12 (CursorDesc*, LocoBitmap*) art pairs, +0x190..+0x1ec ----
    // All realized in one go by EnsureArtLoaded (0x421500) from consecutive TileKind resource
    // ids 0x403..0x40f (0x40d is unused); the CursorDesc supplies the source extent (its
    // nativeWidth/nativeHeight, +0x14/+0x16) and the LocoBitmap is the art actually blitted.
    // Semantics recovered from RedrawSettingRectsMaybe (0x422010), which picks exactly one of
    // each unchecked/checked pair per setting rect.
    CursorDesc *pPlayAloneDescUnchecked;        // +0x190 -- resource 0x407
    LocoBitmap *pPlayAloneBitmapUnchecked;      // +0x194
    CursorDesc *pPlayAloneDescChecked;          // +0x198 -- resource 0x408
    LocoBitmap *pPlayAloneBitmapChecked;        // +0x19c
    CursorDesc *pConnectOnlineDescUnchecked;    // +0x1a0 -- resource 0x409
    LocoBitmap *pConnectOnlineBitmapUnchecked;  // +0x1a4
    CursorDesc *pConnectOnlineDescChecked;      // +0x1a8 -- resource 0x40a
    LocoBitmap *pConnectOnlineBitmapChecked;    // +0x1ac

    // Normal/pressed art for the two labelled buttons. Only the PRESSED pair is read by
    // OnLButtonDown -- the normal pair is the resting state some other painter draws.
    CursorDesc *pEnterDescNormal;       // +0x1b0 -- resource 0x403
    LocoBitmap *pEnterBitmapNormal;     // +0x1b4
    CursorDesc *pEnterDescPressed;      // +0x1b8 -- resource 0x404
    LocoBitmap *pEnterBitmapPressed;    // +0x1bc
    CursorDesc *pEscDescNormal;         // +0x1c0 -- resource 0x405
    LocoBitmap *pEscBitmapNormal;       // +0x1c4
    CursorDesc *pEscDescPressed;        // +0x1c8 -- resource 0x406
    LocoBitmap *pEscBitmapPressed;      // +0x1cc

    // The remembered-choice radio pair's own two-state art (rectRememberChoiceOn/...Off).
    CursorDesc *pRememberOnDescUnchecked;       // +0x1d0 -- resource 0x40b
    LocoBitmap *pRememberOnBitmapUnchecked;     // +0x1d4
    CursorDesc *pRememberOnDescChecked;         // +0x1d8 -- resource 0x40c
    LocoBitmap *pRememberOnBitmapChecked;       // +0x1dc
    CursorDesc *pRememberOffDescUnchecked;      // +0x1e0 -- resource 0x40e
    LocoBitmap *pRememberOffBitmapUnchecked;    // +0x1e4
    CursorDesc *pRememberOffDescChecked;        // +0x1e8 -- resource 0x40f
    LocoBitmap *pRememberOffBitmapChecked;      // +0x1ec

    // Lazily-built 1280x1024 composited backdrop bitmap -- see BuildDrawTargetCompositeMaybe
    // and docs/subsystems.md's "SplashWnd and the boot video sequence" section. NULL until
    // built, which is why every restore blit below is guarded.
    LocoBitmap *pDrawTargetMaybe;       // +0x1f0

    unsigned char pad0x1f4[0x204 - 0x1f4];  // +0x1f4

    HBRUSH hbrSolid;                    // +0x204
    HBRUSH hbrHatch;                    // +0x208
    HWND hwndChild;                     // +0x20c
    VideoPlayer *pVideoPlayer;          // +0x210
    // Raw SetWindowLongA(GWL_WNDPROC) return values, fed straight back to SetWindowLongA /
    // CallWindowProcA -- LONG, not WNDPROC, is what the code actually stores and passes.
    LONG oldChildWndProc;               // +0x214
    LONG oldVideoWndProc;               // +0x218
    NetSetupWnd *pNetSetupWnd;          // +0x21c
    ApplSetupWnd *pApplSetupWnd;        // +0x220

    SplashWnd(HINSTANCE hInstanceArg, UINT resourceIdArg);  // 0x4202f0 -- src/SplashWnd.cpp
    virtual ~SplashWnd();                             // 0x4203a0-family scalar-deleting dtor

    // Non-virtual Create(HWND) hiding WindowBase's own 11-arg virtual Create -- same
    // shadowing convention AlbumCardWnd/EditCardWnd use (see WindowBase.h slot 0x18).
    unsigned char Create(HWND hwndOwner);             // 0x4204d0 -- declared only

    // 0x4208f0 -- drives the boot-sequence state machine (see docs/subsystems.md's "SplashWnd
    // and the boot video sequence"). See src/SplashWnd.cpp.
    void SetState(int stateArg);

    // vtable slot 8 override (0x4206b0; Ghidra: SplashWnd::OnCreateComplete) -- the post-Create
    // init pass. Chains WindowBase::BeginModalCapture partway through, so it carries the base's
    // own name, the same one-name-per-slot convention EndActiveSession uses.
    // See src/SplashWnd.cpp.
    virtual void BeginModalCapture();

    // 0x421500 -- realizes all 12 art pairs above (TileKind resources 0x403..0x40f) plus
    // pDrawTargetMaybe, once, latched by bArtLoaded. Not yet transcribed -- declared only.
    void EnsureArtLoaded();             // Ghidra: FUN_00421500

    // 0x4216f0 -- composites 5 TileKind icons (0x413/0x444/0x445/0x446/0x443) onto a freshly-
    // allocated 1280x1024 pDrawTargetMaybe, each at its own fixed screen offset. Called only
    // from SplashWnd::EnsureArtLoaded. See src/SplashWnd.cpp.
    void BuildDrawTargetCompositeMaybe();

    // vtable slot 0x54 override (0x420bb0) -- WM_KEYDOWN. See src/SplashWnd.cpp.
    virtual LRESULT OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // vtable slot 0x20 override (0x421be0) -- the "screen just became active, draw yourself" hook
    // (same slot AlbumCardWnd::OnActivate overrides; NOT WM_PAINT, which is slot 0x6c and stays at
    // WindowBase's default). Declared non-virtual, matching AlbumCardWnd's own convention for this
    // slot. See src/SplashWnd.cpp.
    void OnActivate(int reserved);

    // vtable slot 0x1c override (0x421200) -- chains WindowBase::RefreshClientClipRect, then
    // re-derives every screen rect above from the composited backdrop's current centering.
    // See src/SplashWnd.cpp.
    virtual void RefreshClientClipRect();

    // vtable slot 0x24 override (0x421eb0) -- the splash window's idle pump: shows/focuses
    // hwndChild once state 7 is reached, and kicks off the first boot video. See src/SplashWnd.cpp.
    virtual void OnIdlePump();

    // 0x421ae0 -- the inverse of EnsureArtLoaded: ReleaseRef()s all 12 descriptors, deletes
    // pDrawTargetMaybe, clears bArtLoaded. See src/SplashWnd.cpp.
    void ReleaseArt();                  // Ghidra: FUN_00421ae0

    // 0x422440 -- restores the composited backdrop under pRect, then stamps one frame of the
    // given art over it at the descriptor's own natural extent. See src/SplashWnd.cpp.
    void DrawArtOverBackdrop(RECT *pRect, int nFrameIndex, CursorDesc *pDesc, LocoBitmap *pArt);

    // 0x422570 -- the backdrop-restore half of DrawArtOverBackdrop on its own (used to erase a
    // setting rect before repainting it). See src/SplashWnd.cpp.
    void RestoreBackdropUnderRect(RECT *pRect);

    // 0x422820 -- brings the GameNet background subsystem up (the thread-state singleton plus
    // its worker thread), once per process. Called from OnCreateComplete and, for attract mode,
    // from ScreenSaver::EnterDemoSession. See src/SplashWnd.cpp.
    void StartGameNetThread();

    // 0x422660 -- the Enter button's own commit-and-dispatch action. Not yet transcribed.
    void OnEnterCommitAndDispatch();

    // 0x422010 -- repaints the four setting rects (play-alone / connect-online / the
    // remembered-choice pair) after OnLButtonDown flips one of the two g_pNetSettings flags
    // they display. Not yet transcribed -- declared only.
    void RedrawSettingRectsMaybe();     // Ghidra: FUN_00422010

    // vtable slot 4 override (0x420860) -- tears the boot session down: chains
    // WindowBase::EndActiveSession, disarms input, closes any playing video, parks the state
    // machine at 1 and drops the art, then hands focus back to the app window.
    // See src/SplashWnd.cpp.
    virtual void EndActiveSession();

    // vtable slot 0x38 override -- the WindowBase-wide WM_LBUTTONDOWN convention slot. See
    // src/SplashWnd.cpp.
    // vtable slot 0x2c -- the catch-all handler for every message SplashWnd's own per-message
    // slots don't claim; drives the two-step boot-video sequence off the MCI "finished" notify.
    // See src/SplashWnd.cpp.
    virtual LRESULT OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x420ec0

    virtual LRESULT OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x422930

    // vtable slot 0x50 override (0x422d80) -- WM_MOUSEMOVE. Swaps the cursor to the "anipoint"
    // (hover) art while the pointer is over any region that is actually CLICKABLE right now, and
    // back to the plain point cursor otherwise. See src/SplashWnd.cpp.
    virtual LRESULT OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    // slot 0x40 (WM_RBUTTONDOWN) -- 0x4323c0, declared-only: the surviving COMDAT lives in
    // src/MapWnd.cpp (MapWnd::OnRButtonDown), and this class, ApplSetupWnd and NetSetupWnd all
    // carry that same address in their own slot 0x40 (see the slot map at the top of this file).
    // Added v545 so the emitted vtable stops holding WindowBase's default here.
    virtual LRESULT OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // vtable slot 0x80 override (0x422610) -- WM_CLOSE. See src/SplashWnd.cpp.
    virtual LRESULT OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);

    // vtable slot 0x90 override (0x420e90; Ghidra: RedrawParentThunk) -- WM_ACTIVATEAPP.
    // See src/SplashWnd.cpp.
    virtual LRESULT OnActivateApp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
};

extern SplashWnd *g_pSplashWnd;  // 0x4fd378

// The two window procs SplashWnd installs with SetWindowLongA(hwnd, GWL_WNDPROC, ...). Both are
// free __stdcall functions with no `this`; they recover the singleton from the TU-local
// s_pSplashWnd cache instead (see src/SplashWnd.cpp). Declared here only so the install sites
// can name them.
LRESULT CALLBACK SplashChildSubclassProc(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x420b20
LRESULT CALLBACK SplashVideoSubclassProc(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x4207c0
