// NetSetupWnd -- the multiplayer connection-setup wizard page, one of the two setup windows
// SplashWnd owns (SplashWnd::pNetSetupWnd @ +0x21c; the other is ApplSetupWnd @ +0x220, see
// src/ApplSetupWnd.h). A real WindowBase-derived Win32 window: constructed by
// SplashWnd::Create (0x4204d0) with `push 0x1e4; call operator new`
// immediately followed by the ctor 0x440f20 -- so sizeof is 484 (0x1e4) on the allocation site's
// own authority, and the Ghidra Structure of the same name agrees field-for-field.
//
// Only the members src/ actually reads are named so far; extend in place (never fork a
// per-TU partial view). The two provider-availability flags at the tail are the ones
// SplashWnd::OnEnterCommitAndDispatch consults to decide whether a remembered protocol choice
// can still be honoured.
#pragma once

#include <windows.h>

#include "CursorDesc.h"
#include "ResourceRef.h"
#include "WindowBase.h"

class NetSetupWnd : public WindowBase {
public:
    // The whole +0xe8..+0x1ac block was re-derived 2026-07-27/28 by disassembling the entire
    // 0x440a50..0x442620 run and grepping EVERY access to the span. What used to sit here --
    // four RECTs at +0xfc/+0x10c/+0x11c/+0x12c plus a loose `nUnk0x13cMaybe` -- was inherited
    // wholesale from the Ghidra struct and is FICTION: not one instruction in the run touches
    // +0xfc..+0x12f, and OnFirstActivateMaybe (0x441870) passes `&this->szLabelText` to
    // UIResources::LoadLocaleString with cch 0x40, which fixes a 64-byte character buffer at
    // +0xf0 straight through the old rect block. The two rects that ARE real start at +0x130
    // (copied field-by-field out of another rect at 0x4422db) and +0x14c (built as {0, 0, w, h}
    // from the icon descriptor's own +0x14/+0x16 WORDs at 0x4413d7).
    //
    // PROVENANCE of the fiction, so it does not come back: the six-rect block is SplashWnd's,
    // not this class's. It was read off 0x422930, which docs/subsystems.md still attributes to
    // `NetSetupWnd::OnLButtonUp` -- but 0x422930 was re-identified as `SplashWnd::OnLButtonDown`
    // (vtable 0x4779f8 slot 0x38) on 2026-07-25, and its rects are SplashWnd's own
    // rectEnterLabel/rectEscLabel/rectPlayAlone/... at SplashWnd's offsets. Two classes'
    // layouts had been fused. Same failure mode as the v435 `rectEnterMaybe` correction one
    // field further down: a rect the Ghidra struct asserts and no code ever reads does not exist.
    unsigned char bUnk0xe8Maybe;         // +0xe8 -- cleared by InitFields, set 1 at 0x4421df,
                                              //   read at 0x4423d9
    unsigned char pad0xe9[0xec - 0xe9];  // +0xe9
    UINT hRedrawTimer;                   // +0xec -- SetTimer(hwndSelf, 0x50, 50, NULL)
    char szLabelText[0x40];              // +0xf0 -- LoadLocaleString(0x79, ..., 0x40) target
    RECT rectLabelMaybe;                 // +0x130 -- the label's own laid-out rect
    // +0x140 -- which leg of the label's crawl around the inside of rectTextAreaMaybe is running:
    // 0 = moving left, 1 = right, 2 = up, 3 = down. OnTimerDefaultMaybe steps the label one pixel
    // per 50 ms tick along it and turns the corner (1 -> 2 -> 0 -> 3 -> 1) when it runs off the far
    // edge; LayoutAndDrawLabel reuses it to pick which edge a freshly measured label is flushed to.
    int nUnk0x140Maybe;                  // +0x140
    HICON hIcon;                         // +0x144 -- LoadIconA(hInstance, 0x65), Create
    unsigned char bUnk0x148Maybe;        // +0x148 -- written 0 (0x441870) / 1 (0x441b20) and
                                              //   branched on at 0x441c80 and 0x441f80
    unsigned char pad0x149[0x14c - 0x149];  // +0x149

    RECT rectIconMaybe;                  // +0x14c -- built {0, 0, pIconDescMaybe->w, ->h}
    // +0x15c is a real RECT, not the loose int an earlier pass assumed: CreateInputBox reads
    // all four sub-fields as the child EDIT's (x, y, w = right-left, h = bottom-top).
    RECT rectInputBox;                   // +0x15c

    // The two layout anchors RefreshClientClipRect (0x441360) derives everything else from, both
    // recovered from that function: a fixed 800x600 "page" centered on the window's clip bounds,
    // and the tray button's own rect hung 0x18/0x24 in from the page's top-left corner. Every
    // other widget rect on the page is then positioned relative to one of these two.
    RECT rectPageMaybe;                     // +0x16c -- {0, 0, 800, 600} centered in rectClipBounds
    RECT rectTrayMaybe;                     // +0x17c -- copied verbatim into pTrayBtn->rect

    // Two more real RECTs, both recovered from LayoutAndDrawLabel/OnLButtonDown. What used to be
    // a loose `nUnk0x190Maybe` "right-edge/extent" here is really rectTextAreaMaybe.top: 0x4421d9
    // takes `lea ebp, [esi+0x18c]` and reads all four sub-fields as a coherent group (a whole-RECT
    // struct assignment into a local at 0x4421e8 and into rectLabelMaybe at 0x4422db), and the
    // four-way OffsetRect switch at the tail consumes one edge of it per case.
    RECT rectTextAreaMaybe;                 // +0x18c -- the label's layout bounds; DrawTextA gets
                                                 //   a copy of it and CenterRectInRect centers the
                                                 //   measured label inside it. Also copied verbatim
                                                 //   into pMatrixBtn->rect by RefreshClientClipRect
                                                 //   -- the scrolling text lives inside the "MATX"
                                                 //   panel graphic.
    // Easter-egg click region, exactly ApplSetupWnd::rectEasterEggSoundMaybe's shape (+0x20c
    // there): hitting it plays one of several random sound ids (`rand()/0x1fff + 0x500f`) via
    // PlaySoundAtScreenPos and has no other effect.
    RECT rectEasterEggSoundMaybe;           // +0x19c
    unsigned char bResourcesLoadedMaybe;    // +0x1ac
    unsigned char pad0x1ad[0x1b0 - 0x1ad];  // +0x1ad

    // The page's seven ResourceRef sub-objects, constructed by InitFields in this field order,
    // realized together by OnFirstActivateMaybe, and destroyed by the dtor in the same order.
    // The names are certain, not hypotheses: each id resolves through the RT_STRING table to an
    // RF-archive art pair listed in loco/rfh.txt (the id list is deliberately non-contiguous --
    // 0x41b..0x41e do not exist).
    ResourceRef *pGoBtn;      // +0x1b0 -- id 0x419 -> startup\net_Go
    ResourceRef *pExitBtn;    // +0x1b4 -- id 0x41a -> startup\net_Exit
    ResourceRef *pIpxBtn;     // +0x1b8 -- id 0x417 -> startup\net_ipx
    ResourceRef *pTcpBtn;     // +0x1bc -- id 0x418 -> startup\net_tcp
    ResourceRef *pInputBox;   // +0x1c0 -- id 0x41f -> startup\netInput
    ResourceRef *pMatrixBtn;  // +0x1c4 -- id 0x420 -> startup\net_MATX
    ResourceRef *pTrayBtn;    // +0x1c8 -- id 0x421 -> startup\net_Tray

    // An EIGHTH button resource (id 0x439) that is NOT wrapped in a ResourceRef -- the descriptor
    // and its realized frame bitmap are cached raw, side by side, by OnFirstActivateMaybe.
    CursorDesc *pIconDescMaybe;   // +0x1cc
    LocoBitmap *pIconRealizedMaybe;  // +0x1d0
    HBRUSH hBackgroundBrush;      // +0x1d4 -- CreateSolidBrush(0xa8c4d8), DeleteObject'd by ~
    // The child EDIT control the page hosts for the typed-in address, plus the window proc it
    // displaces when it subclasses it. Created lazily by CreateInputBox (which uses the HWND
    // itself as its own "already built" flag), subclassed to EditSubclassProc, and matched
    // against WM_CTLCOLOREDIT's lParam in OnUnhandledMessageMaybe to paint it in the page's own
    // colours. NOT the plain int index an earlier pass assumed.
    HWND hwndInputBox;            // +0x1d8
    WNDPROC pOrigInputBoxProc;    // +0x1dc

    // Set once at startup by RefreshProviderAvailability (0x4412f0, called from
    // SplashWnd::OnCreateComplete), which walks g_pNetSettings->Unk0x10Maybe -- the linked list
    // of DirectPlay providers actually detected on this machine -- and marks which of the two
    // protocol ids the remembered-choice fields can name are still present.
    unsigned char bProviderId4AvailableMaybe;  // +0x1e0
    unsigned char bProviderId2AvailableMaybe;  // +0x1e1
    unsigned char pad0x1e2[0x1e4 - 0x1e2];     // +0x1e2

    // 0x440f20 -- chains WindowBase's ctor with the page's own resource id 0x1f6. Declared only.
    NetSetupWnd(HINSTANCE hInstanceArg, UINT resourceIdArg);

    // 0x441190 -- releases the eighth (raw) icon descriptor and the seven ResourceRefs' realized
    // handles when they are still loaded, deletes all seven ResourceRefs, drops the shared boot
    // sound entry (id 0x5015), deletes the background brush, then chains WindowBase's dtor.
    // The compiler's own `??_GNetSetupWnd` scalar-deleting thunk is a free byproduct.
    virtual ~NetSetupWnd();

    // 0x4412f0 -- the page's own non-virtual Create(HWND), hiding WindowBase's 11-arg virtual
    // Create the same way ApplSetupWnd::Create (0x408f00) does: sizes itself to the desktop
    // client rect, loads the window icon (resource id 0x65) into hIcon, and chains
    // WindowBase::Create with style 0x41000000. Called by SplashWnd::Create right after the ctor.
    unsigned char Create(HWND hwndOwnerParam);

    // 0x440fa0 -- seeds the background brush and the 7 ResourceRef sub-objects (resource ids
    // 0x417-0x421) the page paints, then publishes itself into g_pNetSetupWnd. Called by the
    // ctor.
    void InitFields();

    // 0x4419c0 -- (re)computes the two bProviderId*AvailableMaybe flags above from the detected
    // provider list.
    void RefreshProviderAvailability();

    // 0x441720 -- lazily builds the child EDIT that takes the typed-in address, sized from
    // rectInputBox, using hwndInputBox itself as the "already built" flag. Sets the 24-point UI
    // font, limits input to 0x40 chars, seeds it from the remembered address, and subclasses it
    // to EditSubclassProc below (stashing the displaced proc in pOrigInputBoxProc).
    void CreateInputBox();

    // 0x441b40 -- repaints the protocol-button row for the currently remembered choice and shows
    // or hides the child EDIT with it. See src/NetSetupWnd.cpp.
    void DrawStatusTextMaybe();

    // 0x4421d0 -- measures szLabelText into rectLabelMaybe (DT_CALCRECT), centers it in
    // rectTextAreaMaybe and then flushes it to the edge nUnk0x140Maybe names. See
    // src/NetSetupWnd.cpp.
    void LayoutAndDrawLabel();

    // ==== vtable overrides, ground-truthed against NetSetupWnd_Vtbl @ 0x4781d0 ====
    virtual void EndActiveSession();     // slot 0x04 -- 0x441a00
    virtual void BeginModalCapture();    // slot 0x08 -- 0x441870
    // slot 0x1c -- 0x441360, the page's own layout pass: derives every widget rect on the page
    // from rectPageMaybe and rectTrayMaybe. See src/NetSetupWnd.cpp.
    virtual void RefreshClientClipRect();
    virtual void OnActivate(int reservedMaybe);  // slot 0x20 -- 0x441a90
    // slot 0x2c -- 0x442150, the catch-all: WM_SYSCOMMAND minimize/restore and WM_CTLCOLOREDIT
    // for the child EDIT; everything else falls through to DefWindowProcA.
    virtual LRESULT OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    // slot 0x30 -- 0x4423d0, the 50 ms label-crawl pump. See src/NetSetupWnd.cpp.
    virtual LRESULT OnTimerDefaultMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    // slot 0x38 -- 0x441c80, the page's whole hit-test. See src/NetSetupWnd.cpp.
    virtual LRESULT OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    // slot 0x50 -- 0x442090, hover cursor feedback over the four live buttons.
    virtual LRESULT OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    // slot 0x54 -- 0x441f80, the Enter/Escape keyboard half of OnLButtonDown.
    virtual LRESULT OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    virtual LRESULT OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);  // 0x80/0x441f20
    // slot 0x40 (WM_RBUTTONDOWN) -- 0x4323c0, whose surviving COMDAT lives in an entirely
    // different TU (src/MapWnd.cpp, as MapWnd::OnRButtonDown): ApplSetupWnd, SplashWnd and this
    // class all carry that one address in their own slot 0x40. Declared here anyway as of v545,
    // because "not a NetSetupWnd member" (what this note used to say, and the reason the line was
    // withheld) is a statement about where the BODY lives, not about the vtable -- and the vtable
    // is what a declaration controls. Without the line we emit WindowBase's own slot 0x40 into
    // NetSetupWnd's table, which is the wrong table; tools/vtable_audit.py flags it. Declared-only,
    // same treatment as AlbumCardWnd's two ICF-folded slots.
    virtual LRESULT OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
};

// 0x4417e0 -- the subclass proc CreateInputBox installs on the child EDIT. Claims WM_SETCURSOR
// and puts the system I-beam back, forwards Enter/Escape keydowns to the page's own window so the wizard
// (not the edit control) gets to act on them, and chains everything else to pOrigInputBoxProc.
LRESULT CALLBACK NetSetupWnd_EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// DAT_00485260 -- published by InitFields (the only writer) and read back by FUN_004417e0, the
// same "the singleton window registers itself from its own field-init helper" shape the other
// WindowBase pages use. Never cleared, not even by the dtor.
extern NetSetupWnd *g_pNetSetupWnd;  // DAT_00485260
