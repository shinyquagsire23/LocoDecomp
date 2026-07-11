// EditCardWnd -- the card/decal editor screen (1856 bytes). See docs/subsystems.md's own
// entry ("Front-end screens") for the full behavioral map: hosts a real Win32 "EDIT" child
// showing the multiplayer roster or the local name, a 64-slot lazily-populated decal-thumbnail
// picker, a 1280x1024 lazily-composited help/certificate canvas, and the mouse-driven decal
// place/remove editor itself (OnLButtonDown et al, not yet transcribed).
//
// This TU covers InitFields (0x415a00) -- the ctor's own field-init helper,
// which touches essentially the whole object. Struct built field-by-field from ITS decompile
// (every access already goes through a named field -- see the marker comment in the .cpp);
// everything InitFieldsMaybe doesn't itself touch is left as explicit `pad0xNN` padding, real
// gaps for other not-yet-transcribed methods (BuildPlayerRosterList, RebuildLocalPlayerCard,
// the mouse handlers, ...) to fill in later.
#pragma once

#include <windows.h>
#include <ddraw.h>

#include "WindowBase.h"
#include "ResourceRef.h"
#include "CarNetState.h"
#include "CursorDesc.h"

struct LocoBitmap;

class EditCardWnd : public WindowBase {
public:
    EditCardWnd(HINSTANCE hInstance, UINT resourceId); // 0x415980 -- declared-only (Bootstrap)
    // 0x4169e0 -- NON-virtual, and it HIDES WindowBase's 11-argument virtual Create rather than
    // overriding it (same deliberate name-hiding several sibling screens do -- see the note on
    // WindowBase::Create in src/WindowBase.h). Chains the base version, then builds the child
    // EDIT. See src/EditCardWnd.cpp.
    unsigned char Create(HWND hwndOwner);

    HICON hIcon; // +0xe8 -- LoadIconA(hInstance, MAKEINTRESOURCE(0x65)) in Create, then handed
                 // straight to WindowBase::Create as its hIcon argument. Same offset and same
                 // role as AlbumCardWnd's own hIcon (see the partial view at the bottom of
                 // src/WindowBase.h); was field_0xe8 until Create (0x4169e0) was transcribed.
    int nEditMode;
    unsigned char field_0xf0;
    HWND hwndEdit;
    RECT rectDescriptionEdit;
    RECT rectDescriptionEditBorder;
    RECT rectDescriptionHitZone;
    RECT rectRosterPanel;
    RECT rectRosterBadge;
    ResourceRef *pRosterScrollUpBtn;
    ResourceRef *pRosterScrollDownBtn;
    RECT rectRosterList;
    RECT rectRosterTitle;
    unsigned int field_0x170; // roster scroll start index -- reset alongside field_0x174/
                               // field_0x180 by InitFields/OnLButtonDown; consumed by
                               // RedrawRosterList (0x419680) as the aRosterNames start offset
    unsigned int field_0x174; // roster "rows visible last pass" index -- RedrawRosterList
                               // reads it to detect a first-vs-continuation draw, then rewrites it
    unsigned int field_0x178; // last drawn roster row's own DrawTextA height -- real field, was
                               // folded into pad0x178[8] (idiom lint class D), split out 2026-07-18
                               // once RedrawRosterList's own write proved it real
    unsigned int field_0x17c; // roster rows counted this (first) pass -- same split as field_0x178
    bool bNoMoreRosterRows; // "no more rows" flag -- was mis-modeled as a 4-byte
                                 // field_0x180 (folded from pad0x178[0xc], idiom lint class D) by
                                 // an earlier session; RedrawRosterList's own writes
                                 // (`mov BYTE PTR [esi+0x180],0/1`) proved it's really a 1-byte
                                 // field, corrected 2026-07-18. Set by RedrawRosterList when
                                 // the roster scrollback runs out of rows to draw; read by
                                 // HandleRosterClick to gate the "page down" scroll.
    unsigned char pad0x181[3];
    CarNetState *pIdentityTextBuffer;
    bool bLocalCardBuilt;
    unsigned char field_0x189; // real field, not alignment pad -- RebuildLocalPlayerCard
                                // explicitly zeroes it (v137)
    UINT hDecalHitTestTimer;
    int nDecalHitTestInterval; // the 0x44-timer's own interval (ms) -- set (always to the
                                     // literal 200, mirroring SetTimer's own 3rd arg) by
                                     // ArmDecalHitTestMode (0x41a050); was folded into
                                     // pad0x190[4] (idiom lint's class D), split out 2026-07-18
                                     // once that function's own write proved a real field lives
                                     // here.
    bool bDecalTimerArmedMaybe;
    int nClickSoundCooldown; // was named nRosterRefreshCountdownMaybe -- renamed 2026-07-18
                                   // once SelectDecalSubkind/AdjustIdentityColorChannel/
                                   // OnLButtonDown's decal-hit-test branch all showed the
                                   // SAME field gating an unrelated one-shot UI click sound (not
                                   // roster-specific at all): reset to 0 on mode entry/exit, set to
                                   // 10 whenever a sound plays, decremented by a not-yet-transcribed
                                   // WM_TIMER tick elsewhere. Two independent non-roster consumers
                                   // agreeing on the same pattern is the CLAUDE.md-documented bar
                                   // for a confident field rename.
    UINT hRosterRefreshTimer;
    RECT rectIdentityPreview;
    RECT rectDecalPickerGrid;
    ResourceRef *pBtnResMaybe_3cbe;
    ResourceRef *pBtnResMaybe_3cc2;
    RECT rectMainCanvas;
    RECT rectHelpCanvas;
    LocoBitmap *pHelpBitmapCache;
    void *pPreviewIconRealized; // GetOrLoadFrameBitmap(0,0)'s own return value
    CursorDesc *pPreviewIconDesc; // real runtime type is BigObj (see .cpp) -- typed to
                                             // its CursorDesc base since only the inherited,
                                             // un-overridden GetOrLoadFrameBitmap/
                                             // ReleaseRef slots are ever used here
    ResourceRef *paColorSwatchBtn[10];
    RECT rectColorSwatchRow;
    unsigned char aColorRGBTriple[30]; // 10 groups of 3 (parsed from post\Edit\colour.dat)
    unsigned int field_0x24c; // timer id for the 0x4d (RGB-slider auto-repeat) timer
    int field_0x250; // active RGB channel index (0/1/2) for the 0x4d timer's auto-repeat --
                      // real field, was folded into pad0x250[8] (idiom lint class D), split out
                      // 2026-07-18 once AdjustIdentityColorChannel's own write proved it real
    char field_0x254; // auto-repeat increment direction (nonzero=increase), same split as above
    unsigned char pad0x255[3];
    RECT rectColorChannelBar0;
    RECT rectColorChannelBar1;
    RECT rectColorChannelBar2;
    RECT rectUnk0x288;
    int byIdentityColor0;
    int byIdentityColor1;
    int byIdentityColor2;
    ResourceRef *pRedWheelBtn;
    ResourceRef *pYellowWheelBtn;
    ResourceRef *pBlueWheelBtn;
    unsigned char field_0x2b0;
    unsigned char byDecalKindPending;
    unsigned char byDecalSubkindPending;
    unsigned char byDecalSubkindCommitted;
    unsigned char field_0x2b4;
    unsigned char field_0x2b5;
    int nDecalPickerScrollA;
    int nDecalPickerScrollB;
    bool bNeedsCleanup;
    ResourceRef *pExitBtn;
    ResourceRef *pPostBtn;
    ResourceRef *pMailBtn;
    RECT rectSendButton;
    ResourceRef *pDecalHitTestToggleBtn;
    ResourceRef *pAlbumBtn;
    ResourceRef *pDeleteBtn;
    ResourceRef *pNetworkRosterBtn;
    ResourceRef *pDecalScrollBackBtn;
    ResourceRef *pDecalScrollForwardBtn;
    RECT rectDecalKindRow;
    ResourceRef *pDecalKindBtn1;
    ResourceRef *pDecalKindBtn2;
    ResourceRef *pDecalKindBtn3;
    ResourceRef *pDecalKindBtn4;
    ResourceRef *pDecalKindBtn5;
    ResourceRef *pDecalKindBtn6;
    RECT rectDecalCategoryGrid;
    ResourceRef *paDecalCategoryBtn[16];
    unsigned char aRandomDecalPickIndex[12];
    ResourceRef *pRandomizeBtnRes;
    HBRUSH hBackgroundBrush;
    int nSelectedDecalSlot;
    unsigned char field_0x388;
    unsigned char pad0x389[3];
    // +0x38c -- 16 candidate decal-picker row hit-rects, scanned by OnLButtonDown's
    // scrollbar-click branch (index range [0, nDecalPickerScrollA-nDecalPickerScrollB]).
    // Was folded into a single pad0x389[0x103] block (a still-unmodeled RECT array read through
    // a pad-named field is exactly the idiom lint's class D) -- split out 2026-07-18 once
    // OnLButtonDown's own array-of-RECT walk (`pRVar14 = (RECT*)&field_0x38c; ...
    // pRVar14++;`) proved a real field lives here; the 16-entry count exactly fills the gap to
    // paDecalThumbCache (+0x48c) with no slack, matching this file's other 16-slot decal
    // arrays (paDecalCategoryBtn).
    RECT aDecalPickerRowRect[16];
    LocoBitmap *paDecalThumbCache[64];
    int nDecalPickerRowOffset;
    IDirectDrawSurface *pDecalPickerSurfaceA;
    unsigned char field_0x594;
    IDirectDrawSurface *pDecalPickerSurfaceB;
    unsigned char field_0x59c;
    unsigned char bPickerVsRosterGate;
    char aRosterNames[26][13]; // +0x59e, built by BuildPlayerRosterList
    int nSelectedRosterIndex;
    unsigned int nRosterCount;
    int aRosterLabelStringId[17];
    WNDPROC pOrigEditWndProc;

    void InitFields();
    virtual ~EditCardWnd(); // 0x4166b0 (Ghidra: EditCardWnd_DtorMaybe)

    // 0x417180 -- override of WindowBase::RefreshClientClipRect (vtable slot 0x1c;
    // class-wide default is WindowBase's own body). See src/EditCardWnd.cpp for the full
    // session writeup.
    virtual void RefreshClientClipRect();

    // vtable slot 0x20 override -- the WindowBase-wide "window just became active, redraw
    // yourself" hook (base default is the shared NoOpVirtualMaybe body; ground-truthed against
    // this class's own vtable at 0x477930: slot 0x20 = 0x418210). Full-client redraw: restores
    // the help-canvas backdrop over the whole clip bounds, then repaints every editor element
    // (identity preview, roster list, all 10 color-swatch buttons, channel bars, picker-button
    // highlights) and either re-enters the decal picker or rebuilds it from scratch depending
    // on nDecalPickerScrollB. See src/EditCardWnd.cpp.
    virtual void OnActivate(int reservedMaybe); // 0x418210

    // 0x4180a0 (Ghidra: TeardownBuiltUi) -- see src/EditCardWnd.cpp. Tears down the ~23
    // ResourceRef button handles + the preview icon descriptor built by the mouse-editor's
    // own "start editing" path, resets pPreviewIconRealized, clears bNeedsCleanup.
    // Called from EditCardWnd::Create, this dtor, and EndActiveSession.
    void TeardownBuiltUi();

    // 0x416f70 (Ghidra: EndEdit) -- override of WindowBase::EndActiveSession (vtable slot 4,
    // base default 0x425990). Renamed EndActiveSession in src 2026-07-21 when the slot was
    // modeled as a real virtual (one C++ name must serve the base default and every override;
    // AlbumCardWnd's own slot-4 name won) -- sync parked via // TODO: sync at the definition
    // until Ghidra is renamed. See src/EditCardWnd.cpp. The "cancel/close edit"
    // counterpart to BeginEdit (0x416b80): ends
    // the modal mouse capture, tears down the built decal-editor UI, kills both edit-session
    // timers, releases the 2 decal-picker DirectDraw surfaces, and destroys every cached
    // decal-thumbnail bitmap. Gated on WindowBase::bModalCaptureActive (the modal-capture-active flag).
    virtual void EndActiveSession();

    // 0x417f20 (Ghidra: BuildEditUiResources) -- see src/EditCardWnd.cpp. The "build"
    // counterpart to TeardownBuiltUi (same bNeedsCleanup gate, opposite state):
    // realizes the preview icon descriptor and loads every button/decal-category/color-swatch
    // ResourceRef. Called from BeginEdit (0x416b80).
    void BuildEditUiResources();

    // 0x416460 -- see src/EditCardWnd.cpp. Lazily builds the 1280x1024 "help/certificate
    // canvas" (pHelpBitmapCache) from 4 resource-keyed TileKind icon frames, sibling to
    // SplashWnd::BuildDrawTargetCompositeMaybe.
    void BuildPreviewCanvasAMaybe();

    // 0x41a0e0 (Ghidra: RebuildLocalPlayerCard) -- see src/EditCardWnd.cpp. Rebuilds the local
    // player's own CarNetState identity-card object; called from Config_InitClientIdentity
    // whenever the display name is recomputed, and from BeginEdit
    // (0x416b80) when no clone-source card was passed in.
    void RebuildLocalPlayerCard();

    // 0x416e00 -- see src/EditCardWnd.cpp. Builds the multiplayer roster label list: network
    // mode walks DPlaySessionMgr's provider-slot array (skipping the local player's own slot),
    // else falls back to a single localized default label; then unconditionally appends the
    // "easter.usr" cached name table, capped at 26 total entries.
    void BuildPlayerRosterList();

    // FUNCTION: LOCO 0x416b80 -- the "open/begin editing" entry point and the exact mirror image
    // of EndActiveSession above: begins a modal edit session, either on pCloneSource (an existing
    // card, e.g. from AlbumCardWnd's own grid, whose OWNERSHIP it takes) or on a freshly rebuilt
    // local-player card (RebuildLocalPlayerCard) when pCloneSource is NULL. Gated on the same
    // bModalCaptureActive flag as EndActiveSession, with the opposite polarity, so the pair cannot
    // re-enter each other. EXACT match -- see src/EditCardWnd.cpp.
    void BeginEdit(CarNetState *pCloneSource);

    // vtable slot 0x38 override -- the WindowBase-wide WM_LBUTTONDOWN convention slot. The
    // editor's main mouse-click dispatcher: ~30 PtInRect hit-tests over per-field/sub-object
    // rects on the card/decal editor UI (button icons, decal picker grid/scrollbar, roster
    // list, name edit box, the DDraw "dissolve" commit-and-exit transition). See
    // src/EditCardWnd.cpp for the full behavioral summary; own multi-session arc, first-draft
    // transcribed 2026-07-18, not yet byte-matched. Return type corrected int -> LRESULT
    // 2026-07-22 when WindowBase's slot-0x38 virtual was modeled (a true override needs the
    // exact same signature; int/LRESULT are codegen-identical on x86 anyway).
    virtual LRESULT OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x41ac10

    // vtable slot 0x40 override -- the WindowBase-wide WM_RBUTTONDOWN convention slot (ground-
    // truthed against this class's own vtable at 0x477930: slot 0x38 = 0x41ac10 = OnLButtonDown,
    // slot 0x3c = 0x41aa40, slot 0x40 = 0x41ca80, slot 0x50 = 0x41ce50 = OnMouseMove). Ghidra
    // called it EditCardWnd_HandleLButtonUpMaybe -- that was WRONG, same class of slot-mislabel
    // as OnMouseMove's own. Right-click is the editor's universal CANCEL: it drops modes 6/9
    // (decal hit-test) and 4 (name edit) back to browsing mode 1, and otherwise acts as the
    // DECREMENT twin of OnLButtonDown's colour steppers -- the three RGB channel bars/wheels
    // dispatch to the same AdjustIdentityColorChannel with the increase flag CLEARED. See
    // src/EditCardWnd.cpp.
    virtual LRESULT OnLButtonUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam);   // 0x41aa40 -- slot 0x3c, not yet transcribed

    // vtable slot 0x44 override -- WM_RBUTTONUP, the release half of the right-button drag
    // OnRButtonDown starts. Its body is OnLButtonUp's mode-5 arm and nothing else: right-drag is
    // the DECREMENT twin of the left-button colour steppers, so releasing either button ends the
    // same RGB-wheel drag the same way -- back to browsing mode 1, kill the auto-repeat timer,
    // redraw all three wheel buttons unpressed and commit. It keeps OnLButtonUp's `!= 8 && != 10`
    // inert-mode guard even though the only surviving arm then tests `== 5`, which makes the pair
    // vestigial here; that redundancy is the original's (the two bodies are plainly copy-paste)
    // and reproducing it is what makes the three sequential compares come out.
    virtual LRESULT OnRButtonUp(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x41aae0
    // vtable slot 0x48 override -- the WindowBase-wide WM_LBUTTONDBLCLK convention slot (ground-
    // truthed against this class's own vtable at 0x477930: slot 0x44 = 0x41aae0, slot 0x48 =
    // 0x41ab70, slot 0x4c = the shared DefWindowProcStub 0x422ea0, slot 0x50 = 0x41ce50 =
    // OnMouseMove). Ghidra called it EditCardWnd_HandleMouseMoveMaybe and its plate hypothesized
    // WM_MOUSEMOVE -- both WRONG, and doubly so: 0x41ce50 already holds slot 0x50, and the plate
    // also read the PtInRect sense backwards ("moving OFF the roster-build button rect"). Same
    // class of slot-mislabel as OnMouseMove's and OnLButtonUp's own. Only roster-build mode 7
    // reacts: double-clicking INSIDE rectRosterPanel commits the pick and drops back to browsing
    // mode 1, repainting the identity preview. Modes 8 and 10 are inert (the same pair OnLButtonUp
    // and OnLButtonDown treat as inert); everything else falls through to DefWindowProcA.
    virtual LRESULT OnLButtonDblClk(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x41ab70
    // vtable slot 0x2c override -- the WindowBase-wide catch-all slot (OnUnhandledMessageMaybe,
    // ground-truthed against this class's own vtable at 0x477930: slot 0x2c = 0x419a60, between
    // slot 0x28's inherited RouteMessage and slot 0x30 = 0x41a8a0 = OnTimerDefaultMaybe).
    // WM_CTLCOLOREDIT for the child EDIT (green-on-background brush), WM_SYSCOMMAND/
    // SC_SCREENSAVE (re-show the window stack), and the app-private 0x5f5/0x5f6 pair
    // OnLButtonDown posts. See src/EditCardWnd.cpp.
    virtual LRESULT OnUnhandledMessageMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x419a60

    // vtable slot 0x30 override -- the WindowBase-wide WM_TIMER convention slot (ground-
    // truthed against this class's own vtable at 0x477930: slot 0x30 = 0x41a8a0, between
    // slot 0x2c = 0x419a60 = OnUnhandledMessageMaybe and slot 0x38 = 0x41ac10 = OnLButtonDown). Handles the
    // three timer ids this window arms: 0x44 (decal hit-test polling), 0x4d (RGB-slider
    // auto-repeat), 0x53 (click-sound cooldown).
    virtual LRESULT OnTimerDefaultMaybe(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x41a8a0
    virtual LRESULT OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x41ca80

    // vtable slot 0x60 override -- the WindowBase-wide WM_SETFOCUS convention slot (WindowBase's
    // own default there is the shared DefWindowProcStub). While nEditMode is 10 (the "inactive"
    // mode a tutorial forces this window into), refuses the focus and bounces it straight back to
    // the tutorial window, then raises that window to the top; otherwise falls through to the
    // default. src/AlbumCardWnd.cpp's slot-0x60 override (0x405620) is the same handler against
    // its own bInputBlocked gate, as is MailWnd's (0x42fe80).
    virtual LRESULT OnSetFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x41cdf0

    // vtable slot 0x50 override -- the WindowBase-wide WM_MOUSEMOVE convention slot. Pure
    // cursor feedback: hand cursor over anything clickable, point cursor elsewhere, the dragged
    // decal's own thumbnail while a decal is being dragged across the card. See
    // src/EditCardWnd.cpp. (Ghidra called it EditCardWnd_HandleSetCursorMaybe; the vtable at
    // 0x477930 puts 0x41ce50 at slot 0x50 = WM_MOUSEMOVE, not slot 0x70 = WM_SETCURSOR.)
    virtual LRESULT OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x41ce50

    // vtable slot 0x54 override -- the WindowBase-wide WM_KEYDOWN convention slot
    // (ground-truthed against this class's own vtable at 0x477930: slot 0x54 = 0x417040).
    // RETURN/ESCAPE both act as clicking the exit button (flash + EndActiveSession + screen
    // state 3); everything else goes to DefWindowProcA. Transcribed v510 -- see
    // src/EditCardWnd.cpp.
    virtual LRESULT OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x417040

    // vtable slot 0x80 override (WM_CLOSE), ground-truthed against this class's own vtable at
    // 0x477930 (slot 0x80 = 0x419a10). While the app is alive and not already tearing down,
    // closing the card editor ends the session and bounces the front end to screen state 3
    // rather than closing anything; only once shutdown is underway does WindowBase::OnClose run.
    // ⚠ ICF-FOLDED with AlbumCardWnd's own slot 0x80 (its vtable 0x4773f0 carries the same
    // address at 0x477470). The surviving COMDAT sits at 0x419a10, inside THIS TU's
    // 0x417xxx-0x41cxxx block and nowhere near AlbumCardWnd's 0x401xxx-0x405xxx one, so it is
    // this file's copy the linker kept -- src/AlbumCardWnd.cpp will need the identical body
    // when its slot 0x80 is transcribed. Same relationship as ApplSetupWnd's slot 0x2c note.
    virtual LRESULT OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam); // 0x419a10

    // 0x41a050 -- arms the decal-editor's own mouse-move hit-test polling mode (nEditMode=9):
    // (re)starts the WM_TIMER(0x44) pump if not already running, deselects any picked decal slot,
    // requests a mode-transition redraw with the "settled target"/eraser cursor pair (a 3rd,
    // previously undocumented vtable-slot-0xc argument pairing -- see OnLButtonDown's own
    // write-up), and commits a full-window redraw. Called by OnLButtonDown when a decal
    // hotspot is clicked with no modifier held.
    void ArmDecalHitTestMode(); // 0x41a050

    // 0x4189a0 -- redraws the identity-preview strip: restores the button-row background over
    // rectIdentityPreview (offset up by 0xb px, per the original's own dest/src rect derivation) via
    // pHelpBitmapCache, then (if an identity card is loaded) redraws its thumbnail through
    // PostBagCacheBundle::DrawCardThumbnail into rectIdentityPreview, badge-clipped to rectRosterBadge.
    // Called after every mode transition back to "browsing" (nEditMode=1) throughout
    // OnLButtonDown.
    void RedrawIdentityPreview(); // 0x4189a0

    // 0x419680 -- redraws the multiplayer roster scrollback list into rectRosterList (only while
    // nEditMode==7): draws a locale-string title (id 100) + sunken edge box, then walks
    // aRosterNames starting at the scroll index (field_0x170), drawing each name (highlighted via
    // nSelectedRosterIndex) until running out of vertical space or hitting an empty name --
    // sets field_0x180 ("no more rows") when the list is exhausted either way. Was a genuinely
    // under-`this`-typed __fastcall(void*) prototype before this session (same family as
    // RedrawIdentityPreview/ArmDecalHitTestMode's own mislabeling) -- retyped +
    // transcribed 2026-07-18, content-complete first draft, not yet byte-matched.
    void RedrawRosterList(); // 0x419680

    // 0x41a360 -- clicked a decal-picker thumbnail slot. If a local card is already built: exits
    // decal-hit-test mode 9 if active (un-highlights the eraser icon, restores the point cursor via
    // the inherited RequestModeTransitionFromSource), enters "decal armed" mode 2, selects the
    // slot, and schedules a custom drag cursor sized/centered to the decal's own thumbnail bitmap
    // (vtable slot 0x10 = the inherited WindowBase::ScheduleModeTransition). If no local card
    // is built yet: instead pokes a "random decal preview" index into pIdentityTextBuffer->byStampSlotB
    // and redraws the identity preview strip. Content-complete first draft, not yet byte-matched.
    void SelectDecalSlot(unsigned int nSlotIndex); // 0x41a360

    // 0x41a460 -- decal-KIND hit-test cycler: hit-tests the 6 decal-category-kind swatch rects
    // (pDecalKindBtn1..3c99) at the cursor, sets byDecalKindPending to 1-6 on a hit. If
    // the kind actually changed, resets the picker scroll/selection state, releases every cached
    // decal thumbnail, exits decal-hit-test mode 9 back to mode 1 (or just resets the selection if
    // already in mode 1), and re-arms the point cursor + picker scroll + roster-vs-picker redraw
    // (0x418e20). Called from OnLButtonDown when clicking the decal-kind swatch strip.
    // EFFECTIVE MATCH (asmscore byte_diff 27/494) -- residual is a pure edi/ebp register-swap on
    // the cached cursor x/y (Yoda #29/#30 family), not source-steerable.
    void CycleDecalKindHighlight(POINT pt); // 0x41a460
    // 0x41a650 -- multiplayer-roster click handler: page-up/page-down button hits scroll
    // field_0x170 by field_0x17c rows and redraw; a hit inside rectRosterList itself walks
    // aRosterNames from the scroll offset to find the clicked row, copies its name into a local,
    // and (if non-empty) selects it as nSelectedRosterIndex, writes an out-of-range flag into
    // pIdentityTextBuffer->nameA[0x14], copies the name into pIdentityTextBuffer->nameA,
    // switches to mode 6, and redraws the identity preview. EXACT MATCH.
    void HandleRosterClick(LONG param_1, int param_2); // 0x41a650
    // 0x41a210 -- commits a new decal subkind/category selection: stores param_1 into
    // byDecalSubkindPending, resets picker scroll/selection state, releases every cached
    // decal thumbnail, re-loads the category's clipart bitmap via
    // PostBagCacheBundle::ClipartBitmapCache_GetOrLoad, steps the picker scroll, then either just
    // redraws (if the subkind didn't really change and param_2==0) or runs the full roster/picker
    // redraw (0x418e20) otherwise. The end-of-function comparison against the PRE-assignment
    // subkind value decompiles as bogus `unaff_EDI`/`unaff_BL` reads (a stack-spill-slot
    // mistracking artifact, not a hidden parameter -- confirmed via raw disasm: the old value is
    // spilled to [esp+0x10] at function entry and reloaded from the same slot at the comparison,
    // Ghidra just lost the slot's identity across the intervening push/pop depth changes). EXACT
    // MATCH (needed `unsigned int byOldSubkind`, not `unsigned char` -- the spill slot widens the
    // byte to a full dword).
    void SelectDecalSubkind(unsigned char param_1, unsigned char param_2); // 0x41a210
    // 0x418340 -- color-swatch preset click: hit-tests the 10 paColorSwatchBtn rects, and on
    // a hit copies that swatch's own aColorRGBTriple[i*3+0..2] preset into the 3
    // byIdentityColorNMaybe sliders, redraws the channel bars (RedrawColorChannelBars),
    // propagates the new RGB into the loaded identity card, and redraws the preview. EFFECTIVE
    // MATCH (asmscore byte_diff 34/269) -- residual is register-allocation noise on the i*3
    // addressing into aColorRGBTriple; caching `i*3` into a local made it WORSE (tried,
    // reverted), so this is treated as intrinsic (Yoda #29/#30 family).
    void ApplyColorSwatchPreset(POINT pt); // 0x418340
    // 0x418450 -- RGB channel +/- stepper (param_1=channel 0/1/2, param_2=nonzero to increase):
    // arms the 0x4d auto-repeat timer, steps the selected channel's byIdentityColorNMaybe by +/-6
    // (playing a throttled click sound via nClickSoundCooldown), clamps every channel to
    // [0,255], redraws the channel bars + propagates into the identity card + redraws the preview.
    // Content-complete, DIFF 82/803 (asmscore) -- needed a real `switch(param_1)` (not
    // if/else-if: tried both, switch scores dramatically better) but the original still shares ONE
    // hoisted `edi=0xff`/`edi=1` register across all 3 case bodies' comparisons (a decrement-chain
    // dispatch shape, not a jump table -- no intervening code clobbers the register between
    // cases), which a lexically-scoped `switch` in this source doesn't reproduce. See
    // docs/PARKED.md.
    void AdjustIdentityColorChannel(int param_1, char param_2, int param_3, int param_4); // 0x418450
    // 0x418780 -- redraws the 3 RGB channel progress bars (rectColorChannelBar0/0x268/0x278, each filled
    // bottom-up proportional to its byIdentityColorNMaybe value out of 255) into the work surface;
    // param_1 nonzero also un-highlights (releases capture on) all 3 slider buttons. EXACT MATCH --
    // needed the bar-height computed BEFORE the `fillRect = rectUnk0x2NN;` whole-struct copy (not
    // field-by-field, and not with the height calc after), matching the original's own read of all
    // 4 source RECT fields via one cached base pointer even though only 3 survive into the dest.
    void RedrawColorChannelBars(char param_1); // 0x418780

    // 0x4198b0 -- redraws the 6 decal-kind swatch buttons (pDecalKindBtn1..3c99): all disabled
    // (frame 2) when field_0x2b0==0 (no card loaded yet), else all normal (frame 0) with the
    // currently-pending kind (byDecalKindPending, 1-6) highlighted (frame 1). Was a genuinely
    // under-`this`-typed __fastcall(int) prototype, same family as this TU's other mislabeled
    // callees -- retyped + transcribed 2026-07-18. EXACT MATCH -- needed the enabled/disabled
    // branches as `if (field_0x2b0 != 0) {enabled...} else {disabled...}` (enabled branch first),
    // not an early-return guard.
    void RedrawDecalKindButtons(); // 0x4198b0

    // 0x419560 -- full decal-picker "chrome" redraw: un-highlights the 6 fixed picker buttons
    // (pExitBtn/3c8e/3c90/3cac/3cc2/3cc3), conditionally highlights the network-roster
    // button (pNetworkRosterBtn, only relevant when g_pDPlaySessionMgr->connectionMode==2) by
    // whether the loaded card has a network attachment (pIdentityTextBuffer->wAttachmentId), highlights
    // the decal-hit-test toggle (pDecalHitTestToggleBtn) when in mode 9, calls
    // RedrawDecalKindButtons, then syncs all 16 decal-category buttons' highlight state
    // against byDecalSubkindPending (offset by 0x10 when bPickerVsRosterGate is set) or
    // forces them all highlighted when no local card is built yet. Same
    // under-`this`-typed-callee family as RedrawDecalKindButtons -- retyped + transcribed
    // 2026-07-18. EFFECTIVE MATCH (asmscore byte_diff 27/278) -- the boolean args needed explicit
    // if/else branches (literal push 0/1) rather than a computed `cond ? 1 : 0` expression
    // (setcc-style codegen); remaining residual is an ebx/edi register swap on the category-button
    // loop pointer/index (Yoda #29/#30 family).
    void SyncDecalPickerButtonHighlights(); // 0x419560

    // 0x418e20 -- the decal-picker page-flip wipe animation: swaps the double-buffered
    // pDecalPickerSurfaceA/BMaybe pair (nDecalPickerRowOffset toggles which is "new"),
    // full-redraws the new page onto the now-inactive surface via RedrawDecalThumbnailGrid,
    // then plays a multi-step horizontal wipe (new content sliding in one edge, old content
    // sliding out the other -- only once field_0x594/field_0x59c show the old surface was ever
    // actually populated, else just a one-shot RestoreOverlapBlt preps it with no slide-out) with
    // an eased Sleep(2/1/0) per step, each step also nudging the point-cursor via the inherited
    // RequestModeTransitionFromSource. bForward mirrors StepDecalPickerScroll's own
    // direction arg at every call site (checked all 4). param_2 is genuinely dead (per the
    // caller-side-cleanup tell, see CLAUDE.md) -- never read anywhere in the body.
    void AnimateDecalPickerPageWipe(char bForward, unsigned char param_2); // 0x418e20

    // 0x418a90 -- the decal-picker THUMBNAIL GRID redraw (called by SelectDecalSubkind,
    // FUN_00418e20, and others with a target/force-redraw pair of args). When pTargetSurface is
    // NULL, defaults to the shared work surface and forces bFullRedraw off. bFullRedraw
    // toggles between two modes: 0 = incremental (restores just the help-canvas edge strip above
    // the grid from pHelpBitmapCache, coordinates relative to the window), 1 = full (solid-fills
    // the whole target with the format-appropriate magenta colorkey via DDBLT_COLORFILL,
    // coordinates relative to 0,0). Either way, un/highlights the randomize button's own preview
    // thumbnail, then blits every cached decal thumbnail in [nDecalPickerScrollB,
    // nDecalPickerScrollA] right-to-left across the row -- vertical anchor picked by height
    // tier (short thumbnails top-align with a margin bump scaled by height; tall ones >=0x54px
    // bottom-align, cropping the source to the bottom 0x54 rows) -- caching each row's own screen
    // rect into aDecalPickerRowRect for OnLButtonDown's later hit-testing. The
    // incremental mode also refreshes the swatch-preset enable/disable buttons
    // (pDecalScrollBackBtn/3c93) against whether an identity card is actually loaded. Transcribed
    // 2026-07-18 (own session, not byte-matched yet -- see docs pickup).
    void RedrawDecalThumbnailGrid(IDirectDrawSurface *pTargetSurface, char bFullRedraw); // 0x418a90

    // Pages the decal picker one row (bForward: false=up/prev, true=down/next); returns
    // nonzero if the page actually moved (false at either scroll extreme). Transcribed in
    // src/EditCardWnd.cpp -- see the marker comment there for the walk itself.
    bool StepDecalPickerScroll(char bForward); // 0x419260

    // 0x419b10 -- "Import Decal Image" file picker: shows a centered GetOpenFileNameA dialog
    // (EditCardWnd_CenterFileDialogHookProcMaybe, not yet transcribed), validates the chosen
    // file (must open, be non-empty, and be under ~1000K, re-showing the dialog on any
    // failure), deletes any previously-attached file first, imports the new one via
    // PostBag_ImportAttachmentFile, and — for a .wav attachment — plays it back once via
    // UIResources::Sound_PlayOneShotAtPosition. Always ends back in "browsing" mode
    // (nEditMode=1), win or lose. See src/EditCardWnd.cpp for the full write-up.
    void ImportDecalImageMaybe(); // 0x419b10
};

extern EditCardWnd *g_pEditCardWnd; // DAT_004fd380
