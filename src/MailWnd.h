// ⚠ TU-WIDE CODEGEN DIAL (v399; RESOLVED v412): `MailWnd::RefreshClientClipRect` (0x42f8b0,
// 1332 B) is sensitive to the SIZE of this TU's whole included declaration set -- it compiled
// EXACT before v399, went to 1334 B / DIFF(150) when `TutorialWnd`'s declaration block grew
// from two placeholder methods to its real seven, and would not come back for any dummy count
// (8, 9, 10 all reproduce DIFF(150) exactly). v399 concluded from that threshold behaviour that
// the earlier EXACT was accidental. It was not: the function is EXACT again as of v412 with
// `TutorialWnd`'s real seven methods still in place, recovered by deleting NINE duplicate
// declarations from `src/PostBag.h`/`src/CarNetState.h` (five functions declared both as
// `PostBagCacheBundle` members and as free `__stdcall` externs, plus three whose parameter
// types disagreed between the two spellings). Do NOT trim TutorialWnd's method set -- and when
// this dial fires again, hunt for redundant declarations in the INCLUDED HEADERS before
// accepting a residual.
// MailWnd -- the post-box screen (class MAILWINDOWCLASS, RT_STRING id 501; ctor 0x42e900,
// sizeof 0x6e0 = 1760 bytes). A fullscreen WindowBase-derived window that shows ONE postcard at a
// time out of a PostBag category, with an 8-entry ResourceRef* icon-button row (paButtons,
// resource ids 0x3cf0..0x3cf9) hit-tested by HitTestButton into the command ids 2..9 that
// OnLButtonDown dispatches. See docs/subsystems.md's "MailWnd" entry.
//
// The two PostBag categories it pages between are 1 (outbox/sent) and 2 (inbox), selected by
// bViewingOutboxMaybe; every helper below re-derives its category from that flag rather than
// caching it.
//
// Field layout mirrors the Ghidra DB (built from MailWnd's own resource-ref init 0x42e980
// zero-init writes plus this TU's transcribed methods' accesses). Fields whose size is confirmed
// by a write but whose purpose is untraced keep Unk0xNN names; ranges nothing touches yet are
// plain pad0xNN blocks.
//
// ⚠ MEASURED DIAL (2026-07-25): the NUMBER of member-function declarations on this class moves
// src/Main.cpp's AppWndProc (0x4618c0). Bisected one declaration at a time with everything else
// held fixed: <=4 declarations -> AppWndProc compiles to 5440 B / DIFF(3830); >=5 -> 5464 B /
// DIFF(3873). It is the COUNT, not any particular name (swapping which five are present gives
// the same 5464). The field members, the `class`-vs-`struct` keyword, the probe struct's
// position and the virtual override are all individually inert. The correct model wins here --
// AppWndProc is a long way from matching and its length is only a soft metric -- but a future
// session grinding 0x4618c0 should know this dial exists before blaming its own source.
//
// ⚠ THE DIAL RUNS THE OTHER WAY TOO, and there it costs a real match (v422): this TU's own
// RefreshClientClipRect (0x42f8b0) byte-matches at 1332 B ONLY while every header it includes is
// left exactly as it was. Adding ANY declaration to src/AppWindow.h or src/DPlaySessionMgr.h --
// member or free function, either header independently, and regardless of how many are added --
// pushes it to 1334 B / DIFF(150), always identically. The autopsy is one register: at the
// rectFlagMaybe block the original zero-extends pDesc0x664->nativeHeight straight into edx
// (`xor edx,edx / mov dx,[ecx+0x14]`) and keeps eax for the `lea eax,[esi+0x66c]`, while the
// perturbed build has to go through eax and spend an extra `mov edx,eax`; the other 18
// identity_miss rows are that one choice cascading (397/396 insns, so nothing is missing). It is
// NOT fixable from this file's source -- v422 confirmed the operand spellings are already the
// original's -- so the match here is a knife edge, not evidence that 4 declarations is the
// correct model of AppWindow.
//
// ⚠ v428 SPENT IT, and 0x42f8b0 is now PARKED at 1334 B / DIFF(150). Two things were learned
// first. (1) The trigger set is wider than this note claimed: src/EditCardWnd.h is on the list
// too, and ANY declaration added there does it -- proved with a neutral `void ZZProbeUnrelated();`
// dummy, not inferred. Treat EVERY header this TU includes as part of the dial. (2) Six steering
// probes were run against this function's own source under the perturbed state and ALL are
// refuted -- do not re-run them: moving the added declaration's position; making it non-virtual;
// adding a SECOND declaration (parity: the DIFF is byte-identical at +1 and +2, confirming v423's
// saturation finding from the other side); swapping the `.bottom` operand order in both member-rect
// blocks (folded, zero effect); and hoisting `pDesc = pDesc0xNNN;` above its CopyRect (much worse,
// 34134 -> 88264).
//
// It was spent to buy EditCardWnd::OnRButtonDown (0x41ca80, 873 B EXACT), and spending it then
// dissolved two long-parked blockers that had both been waiting on this exact 1332 bytes: the
// PostBag virtual-dtor conversion (v426) and src/AppWindow.h's own MEASURED DIAL (v422/v423),
// which together bought AppWindow::SaveWindowAndCleanExit (831 B) and AppWindow_StartGame (641 B).
// Net +1013 B repo-wide. The knife-edge is still NOT understood -- if a future session cracks it,
// this function comes back for free on top. See docs/PARKED.md.
#pragma once

#include "WindowBase.h"

class CarNetState;
class ResourceRef;

class MailWnd : public WindowBase {
public:
    // +0xe8 -- input gate raised around the modal "open the attachment" flow (0x42f0e6/0x42f11e)
    // and cleared by BeginModalCapture (0x42f5e0); OnLButtonDown refuses everything while set.
    unsigned char bAttachmentModalBusyMaybe;
    // +0xe9 -- the destination path for the "open the attachment" flow, and the ONLY reason this
    // class is 1760 bytes. PromptForAttachmentSavePathMaybe (0x42eea0) fills it from its
    // GetSaveFileName dialog; OpenAttachmentMaybe then CopyFileA's the .att onto it. It doubles
    // as the read buffer for the .dat sidecar (ReadFile of exactly 0x504 bytes straight into it),
    // which is what pins the size: 0xe9 + 0x504 == 0x5ed, exactly where bTearingDownMaybe starts.
    // Was `pad0xe9[0x504]` until v383 -- the ReadFile/CopyFileA pair proved it is a real buffer.
    char szExtractPathMaybe[0x504];
    // +0x5ed -- second input gate, raised by the card-teardown helper (0x42e4c8) and cleared on
    // window (re-)entry (0x42ebee/0x42f6a6); also read by the WM_TIMER handler (0x42ff20).
    unsigned char bTearingDownMaybe;
    char pad0x5ee[0x5f0 - 0x5ee];
    // +0x5f0 -- button-press cooldown, set to 8 by a press and decremented once per WM_TIMER
    // tick (0x42fff0); a nonzero value swallows further presses of the same two buttons.
    int nInputCooldownMaybe;
    HICON hIcon;                    // +0x5f4 -- LoadIconA(hInstance, 0x65), see Create
    unsigned char bInputEnabled;    // +0x5f8 -- master enable; cleared when the screen hands off
    unsigned char bInit0x5f9;       // +0x5f9
    unsigned char bInit0x5fa;       // +0x5fa
    char pad0x5fb;
    // +0x5fc -- the SetTimer id (always 0x4d) for the 200 ms flag-animation tick, or 0 while no
    // timer is armed. BeginModalCapture arms it, EndActiveSession and the WM_SYSCOMMAND
    // screensaver path kill it, and OnTimerDefaultMaybe gates its whole body on it.
    int nTimerId;
    // +0x600 -- frame tick for the two-frame flag animation below; the WM_TIMER handler
    // increments it and flips bFlagFrameMaybe once it reaches 20. UNSIGNED is load-bearing --
    // it is what makes OnTimerDefaultMaybe's `>= 20` compile to the original's `cmp eax,0x14;
    // jb` rather than a signed `jl`.
    unsigned int nFlagAnimTickMaybe;
    // +0x604 -- raised by AppWndProc's WM_USER+1 command 8 immediately before it switches the app
    // to UI mode 5 (the mail screen); the only write anywhere in .text, so this reads as "the mail
    // screen was opened by a card arriving, not by the user". Reader not yet found.
    unsigned char bMailPendingMaybe;
    // +0x605 -- which of the flag sprite's two frames is showing (see DrawFlagFrameMaybe, which
    // offsets the source rect by one frame width when set).
    unsigned char bFlagFrameMaybe;
    // +0x606 -- 0 = viewing category 2 (inbox), 1 = viewing category 1 (outbox). Every PostBag
    // helper in this TU re-derives its category from this flag.
    unsigned char bViewingOutboxMaybe;
    // +0x607 -- per-card enable for the bin/flip/outbox buttons (commands 5, 6, 7); maintained by
    // the same helpers that repopulate pOpenCard.
    unsigned char bButtonsEnabledMaybe;
    CarNetState *pOpenCard;         // +0x608 -- the card currently displayed, or NULL
    CarNetState *pHeldCard;         // +0x60c -- the card picked up onto the cursor, or NULL
    // +0x610 -- which face of the card is drawn (passed straight to DrawCardThumbnail);
    // command 6 toggles it. Initialized to 1.
    unsigned char bCardSideMaybe;
    char pad0x611[3];
    RECT rectScreenMaybe;           // +0x614 -- screen-origin offset applied to the blit rects
    RECT rectCardMaybe;             // +0x624 -- the card thumbnail's own rect (also its hot area)
    // +0x634 -- the design-resolution layout box, reset to (0,0,800,600) and centered inside the
    // client clip bounds by RefreshClientClipRect (0x42f8b0); every other rect on the screen is
    // then laid out as a CopyRect of it plus a hardcoded OffsetRect.
    RECT rectLayoutBaseMaybe;
    CursorDesc *pBackdropDescMaybe; // +0x644 -- sizes rectScreenMaybe from its own native w/h
    LocoBitmap *pCardBackdropBmp;   // +0x648 -- backdrop restored under the card/attachment
    CursorDesc *pDesc0x64c;         // +0x64c -- sizes rect0x654
    LocoBitmap *p0x650;             // +0x650
    RECT rect0x654;                 // +0x654
    CursorDesc *pDesc0x664;         // +0x664 -- sizes rectFlagMaybe
    LocoBitmap *pFlagBmpMaybe;      // +0x668 -- two-frame flag sprite strip
    RECT rectFlagMaybe;             // +0x66c -- the flag's rect AND its hot area (the command
                                    //           path that toggles bFlagFrameMaybe by hand)
    RECT rect0x67c;                 // +0x67c -- hot area hit-tested to command id 9
    RECT rectAttachmentMaybe;       // +0x68c -- the attachment/stamp hot area, live only while
                                    //           pOpenCard->wAttachmentId != 0
    CursorDesc *pCardCursorDesc;    // +0x69c -- cursor pair swapped in while a card is held
    void *pCardCursorRect;          // +0x6a0
    ResourceRef *paButtons[8];      // +0x6a4 -- ids 0x3cf0,0x3cf1,0x3cf2,0x3cf3,0x3cac,0x3cf6,
                                    //           0x3cf5,0x3cf9 (note [5]/[6] are initialized out
                                    //           of order); HitTestButton maps [0..5] to command
                                    //           ids 2..7 and [6] to 8. [7] is never hit-tested.
    unsigned short aBadgeVariantsA[5]; // +0x6c4
    unsigned short aBadgeVariantsB[5]; // +0x6ce
    char pad0x6d8[8];

    MailWnd(HINSTANCE hInstance, UINT resourceId); // 0x42e900
    // 0x42ec10. Declaring it virtual also makes cl emit the matching scalar deleting destructor
    // (`??_GMailWnd`) that vtable slot 0 holds at 0x42e960 -- a free byproduct, not separately
    // transcribed (same precedent as AlbumCardWnd/EditCardWnd).
    virtual ~MailWnd();
    // 0x42edb0 -- hides (does not override) WindowBase::Create. `unsigned char` + an explicit
    // `!= 0` is what produces the trailing `test al,al; setne al`; a `bool` return widens the
    // same expression the long way (`neg al; sbb eax,eax; neg eax`) -- docs/CODEGEN.md.
    unsigned char Create(HWND hwndOwner);

    // 0x42fdf0 -- lazily realizes the screen backdrop's descriptor/bitmap pair (kind 0x3cf7),
    // guarded by bInit0x5fa.
    void EnsureBackdropRealizedMaybe();
    // 0x42fe30 -- lazily realizes the eight icon-button ResourceRefs plus the three
    // descriptor/bitmap pairs at +0x64c, +0x664 and +0x69c (kinds 0x3cf8, 0x3cfb, 0x3cfa),
    // guarded by bInit0x5f9. The inverse of the bInit0x5f9 block in ~MailWnd/EndActiveSession.
    void EnsureButtonResourcesRealizedMaybe();

    // 0x42d8a0 -- drops the held card out of the current category entirely (deletes its .crd and
    // its attachment .dat); used by the album hand-off.
    void DiscardHeldCardMaybe();
    // 0x42da10 -- moves the held card into category 1 (outbox), then reselects.
    void MoveHeldCardToOutboxMaybe();
    // 0x42db30 -- moves the held card into category 2 (inbox), then reselects.
    void MoveHeldCardToInboxMaybe();
    // 0x42dc50 -- deletes the currently open card's file from the current category.
    void DeleteOpenCardFileMaybe();
    // 0x42dd50 -- repopulates pOpenCard from the current category (advancing past the card that
    // was just consumed, if any).
    void SelectNextCardMaybe();
    // How many cards the currently-viewed category holds, refreshing bButtonsEnabledMaybe on the
    // way when that category is the outbox. Never emitted out of line -- it is inlined at all
    // nine of its call sites (the four card-move helpers, OnButtonMouseUp cases 6 and 7,
    // OnButtonMouseDown case 7, and OnMouseMove cases 5/6/7). Its two `mov eax,ebp` copies at
    // the `return n;` sites are what pin the count to EAX at the badge-table call sites;
    // spelling the block out inline instead puts it in EBP and misses by ~10 instructions.
    // Category 2 (inbox) goes through the cache; category 1 (outbox) is recounted every call.
    unsigned short GetViewedCategoryCardCountMaybe();

    // 0x42de70 / 0x42e150 -- per-button icon repaint in the released/pressed state.
    void OnButtonMouseUp(int nButtonId);
    void OnButtonMouseDown(int nButtonId);
    // 0x42e980 -- second-stage construction: zeroes the runtime state the ctor leaves alone and
    // allocates the eight icon-button ResourceRefs plus the two badge-variant tables.
    void InitResourceRefs();
    // 0x42e4e0 -- blits whichever of the flag sprite's two frames bFlagFrameMaybe selects.
    void DrawFlagFrameMaybe();
    // 0x42e5e0 -- restores the backdrop under the card (and, when bWithAttachment, under the
    // attachment area too).
    void RestoreCardBackdropMaybe(char bWithAttachment);
    // 0x42e760 -- full repaint of the card area: attachment, thumbnail and the flag button.
    void RedrawCardAreaMaybe();
    // 0x42eea0 -- the modal GetSaveFileName prompt that fills szExtractPathMaybe with where the
    // user wants the attachment written. Returns 0 = cancelled / overwrite declined / error
    // reported, 1 = go ahead, 2 = the chosen path names a directory that does not exist yet (the
    // caller creates it). `int` return, not a byte: OpenAttachmentMaybe compares the full EAX
    // against 0 and 2. pszScratchText is never read (the caller passes ""); see src/MailWnd.cpp.
    int PromptForAttachmentSavePathMaybe(char *pszScratchText);
    // 0x42f250 -- copies the open card's attachment .dat out; runs modally.
    void OpenAttachmentMaybe();
    // 0x4309b0 -- rewrites the open card's payload over its own .crd file in the viewed
    // category (delete + recreate, 0x398 bytes from wSignature on).
    void SaveOpenCardToFileMaybe();

    // 0x430090 -- maps a client-space point to one of the command ids 2..9 that OnLButtonDown
    // dispatches, or 0 for a miss. See src/MailWnd.cpp.
    unsigned int HitTestButton(LONG x, LONG y);

    // vtable slot 4 (0x42f6c0) -- tears the modal session down: drops the open card, releases
    // the bInit0x5f9 resources, kills the animation timer. See src/MailWnd.cpp.
    virtual void EndActiveSession();
    // vtable slot 8 (0x42f5e0) -- the entry point into the screen: hides the cursor, realizes
    // the button resources, lays out, and arms the animation timer. See src/MailWnd.cpp.
    virtual void BeginModalCapture();
    // vtable slot 0x20 (0x42e420) -- repaints the whole screen on (re-)activation and asks the
    // tutorial to take over. See src/MailWnd.cpp.
    virtual void OnActivate(int reservedMaybe);
    // vtable slot 0x1c (0x42f8b0) -- re-lays out every rect on the screen. See src/MailWnd.cpp.
    virtual void RefreshClientClipRect();
    // vtable slot 0x2c (0x42ee20) -- the fallback message handler: swallows the screensaver/
    // monitor-power WM_SYSCOMMANDs (killing the animation timer) and re-enables the window on
    // the private 0x5f5 message. See src/MailWnd.cpp.
    virtual LRESULT OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    // vtable slot 0x30 (0x42fff0) -- WM_TIMER: drives the two-frame flag animation and ticks the
    // button-press cooldown down. See src/MailWnd.cpp.
    virtual LRESULT OnTimerDefaultMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    // vtable slot 0x40 (0x4307c0) -- WM_RBUTTONDOWN: drops a held card back where it came from.
    // See src/MailWnd.cpp.
    virtual LRESULT OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    // vtable slot 0x54 (0x42f810) -- WM_KEYDOWN: Esc and Q leave the screen the same way the
    // exit button does. See src/MailWnd.cpp.
    virtual LRESULT OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    // vtable slot 0x60 (0x42ff20) -- WM_SETFOCUS: while the tutorial has taken over, bounce the
    // focus straight back to it. See src/MailWnd.cpp.
    virtual LRESULT OnSetFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    // vtable slot 0x80 (0x42ff80) -- WM_CLOSE: leave to app state 3 unless the app is already
    // gone or shutting down. See src/MailWnd.cpp.
    virtual LRESULT OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    // vtable slot 0x50 (0x430800) -- WM_MOUSEMOVE. See src/MailWnd.cpp.
    virtual LRESULT OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
    // vtable slot 0x38 (0x430190) -- WM_LBUTTONDOWN. See src/MailWnd.cpp.
    virtual LRESULT OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);
};

extern MailWnd *g_pMailWnd; // DAT_004fd37c

// Padded-vtable probe kept ONLY for the three TUs that dispatch MailWnd's slot 8
// (AlbumCardWnd/EditCardWnd/TutorialWnd, all through the g_pMailWnd GLOBAL pointer, where the
// documented global-devirtualization gotcha rules out a plain named-virtual spelling).
// Slot 8 is MailWnd's own BeginModalCapture override (0x42f5e0).
struct MailWndVtblProbe {
    virtual void *_v00();
    virtual void *_v04();
    virtual void Refresh(); // slot 8, role not yet identified beyond its call sites
};
