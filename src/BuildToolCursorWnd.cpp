// BuildToolCursorWnd's 4-slot icon-overlay drawing cluster. See src/BuildToolCursorWnd.h and
// docs/subsystems.md's BuildToolCursorWnd entry for the full field derivation and the
// 2026-07-17 class-attribution correction (was misattributed to TutorialWnd by address
// clustering alone).

#include "BuildToolCursorWnd.h"
#include "UIResources.h"
#include "AppWindow.h"
#include "WorldBoardMaybe.h"
#include "EffectSpawner.h"   // DAT_004fd220 -- the dirty-rect ghost handles below are its effects
#include "DSound.h"

#include <string.h>

extern void CenterRectInRect(RECT *outer, RECT *rect); // 0x425a50
extern unsigned int g_dwScreenWidth;  // DAT_004851d8
extern unsigned int g_dwScreenHeight; // DAT_00485214
extern unsigned char g_bBoardScrollFlag; // DAT_00485210 -- board is scrollable/windowed
extern int g_nScreenState;              // DAT_004851f4 -- app screen-state selector, see src/GameNetMsgQueue.h
// Byte-returning predicate (the original materializes the `== 10` via cmp/setz al and reuses
// that AL for bModeChangeNotifyPending's zero store) -- same idiom as the sibling TUs' own
// file-local copies (CreditsWnd/AppWindow/NetSetupWnd).
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// The "opaque dirty-rect collection wrapper" this TU used to carry was the EffectSpawner
// singleton all along -- its RemoveHandle/SpawnEffect were 0x423d20/0x423c50 under local
// names. Retired 2026-07-31 when 0x423d20's real body landed in src/EffectSpawner.cpp: from
// that moment the local spelling was a live lint_alias defect (a call emitted against a symbol
// defined nowhere), not the harmless placeholder it had been while the callee was untranscribed.
// The two call sites below now go through the real class.


// FUNCTION: LOCO 0x436b20
// Runs the PopupWndBase base ctor, then zeroes the tool-overlay state: no icon, no loaded cursor
// resource set, tool mode 0 and inactive, no dirty-rect handle. The two -1s at the end are
// sentinels, not zeroes -- nAnimState -1 means "no animation set selected" and
// nCurrentFrameIndex 0xffffffff is the value AdvanceSlotDAnimation returns immediately on.
//
// The two -1 stores are SCHEDULED last regardless of where they sit in the source, but their
// source POSITION still decides where cl materializes the shared `or reg,0xffffffff` constant
// and, with it, whether the vptr store lands before or after it. Writing them last (the obvious
// spelling) puts the vptr store first and costs DIFF(55) at 24/24 instructions; hoisting them
// to the very top costs DIFF(53); only this mid-body position reproduces the original's
// `or ecx,-1` early / vptr store late pairing.
BuildToolCursorWnd::BuildToolCursorWnd(HINSTANCE hInstance, UINT resourceId)
    : PopupWndBase(hInstance, resourceId)
{
    hIcon = NULL;
    bCursorResLoaded = 0;
    bCursorResLoadedCompanion = 0;
    nAnimState = -1;
    nCurrentFrameIndex = 0xffffffff;
    nAnimTickCount = 0;
    toolMode = 0;
    bIconDrawReadyFlag = 0;
    bToolActive = 0;
    pDirtyRectHandle = NULL;
}

// FUNCTION: LOCO 0x436b90 (??_GBuildToolCursorWnd scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x436bb0
// Releases the realized cursor-descriptor set if one is loaded (same B, C, A, D order as
// Create's own release block), then runs the PopupWndBase base dtor. The SEH wrapper and the
// own-vtable restore are compiler-generated dtor machinery under /GX, nothing source-visible.
BuildToolCursorWnd::~BuildToolCursorWnd()
{
    if (bCursorResLoaded != 0) {
        pDescB->ReleaseRef();
        pDescB = NULL;
        pDescC->ReleaseRef();
        pDescC = NULL;
        pDescA->ReleaseRef();
        pDescA = NULL;
        pDescD->ReleaseRef();
        pDescD = NULL;
        bCursorResLoaded = 0;
    }
}

// FUNCTION: LOCO 0x436c50
// Realizes the window: loads the app icon (resource 0x65), (re)loads the 14-mode cursor bitmap
// set, then sizes itself to slot A's descriptor natural size and centers that rect in the full
// screen before handing off to the PopupWndBase base Create with style 0x86000000
// (WS_POPUP|WS_DISABLED|WS_CLIPSIBLINGS). The release block in the middle is NOT dead: the
// LoadCursorBitmapSet call above it is what may have latched bCursorResLoaded, so this drops the
// previous generation of descriptors after the new sizes have already been read out of slot A --
// which is also why the read of pDescA->nativeWidth/nativeHeight has to precede it (sic).
bool BuildToolCursorWnd::Create(HWND hwndOwner)
{
    hIcon = LoadIconA((HINSTANCE)hInstance, MAKEINTRESOURCE(0x65));
    LoadCursorBitmapSet();

    RECT rectTool;
    rectTool.left = 0;
    rectTool.right = pDescA->nativeWidth;
    rectTool.top = 0;
    rectTool.bottom = pDescA->nativeHeight;

    RECT rectScreen;
    SetRectEmpty(&rectScreen);
    rectScreen.right = g_dwScreenWidth;
    rectScreen.bottom = g_dwScreenHeight;

    if (bCursorResLoaded != 0) {
        pDescB->ReleaseRef();
        pDescB = NULL;
        pDescC->ReleaseRef();
        pDescC = NULL;
        pDescA->ReleaseRef();
        pDescA = NULL;
        pDescD->ReleaseRef();
        pDescD = NULL;
        bCursorResLoaded = 0;
    }
    CenterRectInRect(&rectScreen, &rectTool);
    if (PopupWndBase::Create(0, hwndOwner, rectTool.left, rectTool.top,
                             rectTool.right - rectTool.left, rectTool.bottom - rectTool.top,
                             NULL, hIcon, 0, 0x86000000, 0, 0)) {
        return 1;
    }
    return 0;
}

// FUNCTION: LOCO 0x436d60 // EFFECTIVE MATCH -- 99/99 instructions, and every instruction from
// the CopyRect onward (spawn / union / intersect / MarkRectDirty / UpdateDirtyTiles, ~2/3 of the
// body) is byte-identical bar one lea's register choice. The entire residual (asmscore total
// 82826, byte_diff 56) lives in the ~40-byte coordinate block: our compile hoists
// `mov edi,[esi+0xdc]` ABOVE the g_bBoardScrollFlag branch as a partial-redundancy elimination --
// both arms read Unk0xdc -- and having the value already in a register then makes it fold the
// scroll offset with `add`/`sub` on edi, where the original reloads per arm and folds
// scroll + 0x32 into one `lea edi,[eax+ecx+0x32]`. Three probes, none of which moved it:
// reassociating `+0x32` to last (identical 185), inverting the g_bBoardScrollFlag guard so the
// simple arm comes first (WORSE, 190), and the two branch-polarity fixes that ARE kept below
// (215010 -> 82826 total, which is what closed the structural half). See docs/PARKED.md.
//
// Repaints the mouse-tracked ghost/preview sprite. Inert unless a tool is active, and skipped
// outright for the four modes that have no ghost sprite (3, 4, 8, 10). The sprite is a spawned
// world effect (id 0x2c0d) rather than a blit: the first call spawns it, every later call
// snapshots its old rect into rectPrevGhost, detaches it and moves it to the new spot, then
// dirties the union of old and new (clipped to the board viewport) in one flush.
//
// The +0x32 in both coordinates is the sprite's own hotspot bias. When the board is scrollable
// (g_bBoardScrollFlag), the cached screen-space position has to be rebased through the main
// window's own origin and the board scroll offset; unscrollable full-screen boards skip both.
void BuildToolCursorWnd::RedrawGhostCursor()
{
    if (bToolActive != 0 && toolMode != 3 && toolMode != 4 && toolMode != 8 && toolMode != 10) {
        RECT rectUnion;
        RECT rectApp;
        RECT rectFinal;

        GetWindowRect(g_pApp->hwndOwner, &rectApp);

        int x;
        int y;
        if (g_bBoardScrollFlag != 0) {
            x = Unk0xdc - rectApp.left + 0x32 + g_worldBoard.dwScrollX;
            y = Unk0xe0 - rectApp.top + 0x32 + g_worldBoard.dwScrollY;
        } else {
            x = Unk0xdc + 0x32;
            y = Unk0xe0 + 0x32;
        }

        if (pDirtyRectHandle != NULL) {
            CopyRect(&rectPrevGhost, &pDirtyRectHandle->rect);
            pDirtyRectHandle->DetachMaybe();
            pDirtyRectHandle->MoveTo(x, y);
        } else {
            pDirtyRectHandle = (WorldDirtyRectNodeMaybe *)
                DAT_004fd220.EffectSpawner_SpawnSimpleMaybe(0x2c0d, 0, x, y);
        }
        UnionRect(&rectUnion, &pDirtyRectHandle->rect, &rectPrevGhost);
        IntersectRect(&rectFinal, &rectUnion, &g_worldBoard.rcViewport);
        g_worldBoard.MarkRectDirty(rectFinal);
        g_worldBoard.UpdateDirtyTiles(0);
    }
}

// FUNCTION: LOCO 0x436ec0
// Activates tool `mode`: runs the base Show, reloads the cursor bitmap set for the new mode,
// refreshes the client rect through vtable slot 0x18, brings the overlay up and focused, starts
// the 200 ms ghost-cursor redraw timer, latches param2/bToolActive and paints one ghost frame.
// The tail is the acquire half of OnExit's own release block: it pins sound-bank entry 0x50f8
// persistent (once) and then plays it as the tool-activation cue.
void BuildToolCursorWnd::ShowTool(int mode, int param2)
{
    PopupWndBase::Show();
    toolMode = mode;
    LoadCursorBitmapSet();
    RefreshClientRect();

    HWND hWnd = hwndSelf;
    ShowWindow(hWnd, SW_SHOWNORMAL);
    SetFocus(hWnd);
    nRedrawTimerId = SetTimer(hWnd, 1, 200, NULL);
    toolParam2Maybe = param2;
    bToolActive = 1;
    RedrawGhostCursor();
    if (g_pDSoundManager != NULL && bCursorResLoadedCompanion == 0) {
        SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(0x50f8);
        if (pEntry->EnsureLoaded() != 0) {
            pEntry->bPersistent = 1;
            bCursorResLoadedCompanion = 1;
        }
    }
    g_UIResources.PlayUiSound(0x50f8);
}

// PopupWndBase's own vtable slot 0xc (SetCursorDesc, 0x414340 -- confirmed CONSISTENT
// across all 3 derived singletons, see docs/subsystems.md) is called generically below (the
// original's raw `call [ecx+0xc]` shape) -- now an ordinary unqualified virtual call, since
// SetCursorDesc is a real virtual on PopupWndBase (vtable modeled through slot 0x20 as of
// 2026-07-21). Slot 4 (OnExit, used below) is the OPPOSITE case: genuinely overridden per
// class (BuildToolCursorWnd's own is transcribed below, named+this-typed v196).
// PopupWndBaseVtblProbe itself still lives in src/PopupWndBase.h for
// PopupWndBase_RouteMessage's pWnd dispatch.

// FUNCTION: LOCO 0x436f70 // EXACT MATCH (2026-07-18, v199) -- vtable slot 4 override (see the note above): calls the base
// PopupWndBase::OnExit, releases the 4 owned CursorDesc icon descriptors (B, C, A, D --
// NOT alphabetical, matches the raw disasm's own call order) via their real virtual
// ReleaseRef (vtable slot 2), KillTimer's the redraw timer, resets the animation-state
// fields, and -- if a world-board dirty-rect handle is outstanding -- unions its embedded RECT
// (at +8 of the handle) with this window's own on-screen rect (Unk0xdc/Unk0xe0, a coordinate
// pair whose exact purpose beyond this dirty-rect role isn't pinned down), intersects against
// the world board's viewport bounds, removes the handle from its owning collection (opaque
// FUN_00423d20/DAT_004fd220 -- a vtable-driven collection wrapper, not modeled beyond this one
// call site), and flushes via WorldBoardMaybe::MarkRectDirty/UpdateDirtyTiles. Finally
// releases a DSound bank entry (id 0x50f8) if its own "loaded companion" flag is set. First-try
// compile had a calling-convention bug (DIFF 81/376): FUN_00423d20 is a real __thiscall method
// (this in ECX, one explicit stack arg per raw disasm), not a 2-arg free function -- modeling it
// as an opaque no-body member of a minimal WorldDirtyRectCollectionMaybe wrapper (see this file's
// own top-of-file declaration) fixed the this-in-ecx/single-push shape and closed the match
// outright.
void BuildToolCursorWnd::OnExit() {

    PopupWndBase::OnExit();
    if (bCursorResLoaded != 0) {
        pDescB->ReleaseRef();
        pDescB = NULL;
        pDescC->ReleaseRef();
        pDescC = NULL;
        pDescA->ReleaseRef();
        pDescA = NULL;
        pDescD->ReleaseRef();
        pDescD = NULL;
        bCursorResLoaded = 0;
    }
    KillTimer(hwndSelf, nRedrawTimerId);
    bIconDrawReadyFlag = 0;
    nCurrentFrameIndex = 0xffffffff;
    nAnimState = -1;
    bToolActive = 0;
    if (pDirtyRectHandle != NULL) {
        int width = nClientWidth;
        int yTop = Unk0xe0;
        int xLeft = Unk0xdc;

        RECT rectSelf;
        SetRect(&rectSelf, xLeft, yTop, width + xLeft, yTop + width);
        RECT rectUnion;
        UnionRect(&rectUnion, &pDirtyRectHandle->rect, &rectSelf);
        RECT rectFinal;
        IntersectRect(&rectFinal, &rectUnion, &g_worldBoard.rcViewport);
        DAT_004fd220.EffectSpawner_RemoveHandle(pDirtyRectHandle);
        pDirtyRectHandle = NULL;
        g_worldBoard.MarkRectDirty(rectFinal);
        g_worldBoard.UpdateDirtyTiles(0);
    }
    if (g_pDSoundManager != NULL && bCursorResLoadedCompanion != 0) {
        SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(0x50f8);
        pEntry->bPersistent = 0;
        pEntry->Release();
        bCursorResLoadedCompanion = 0;
    }
}

// FUNCTION: LOCO 0x437670
// ⭐ The case blocks below are deliberately NOT in ascending order, and that ordering is
// load-bearing, not cosmetic. cl 11.00 emits a switch's case BODIES into .text in SOURCE order
// (the jump table itself is of course value-indexed), and it cross-jumps/tail-merges ADJACENT
// bodies only -- so the original binary's physical block layout reads back the original source's
// case order directly. Transcribed ascending (0, 1/3/6, 2/4/7/8, 5, 9, 10, 11, 12, 13) this
// compiled to DIFF(436) at 186 vs 169 instructions -- 17 instructions the original had merged
// away and ours had not. Re-ordering the blocks to the layout the original's own .text shows
// (10, 9, 11, 12, 13, 5, 0, 1/3/6, 2/4/7/8), with no other change at all, made it EXACT: the
// pairs 10+12 and 9+13 become push-and-jump stubs into their neighbours' shared tails, and case 0
// becomes a stub into 2/4/7/8's. See docs/CODEGEN.md.
//
// Realizes the four icon-slot descriptors for the CURRENT toolMode and latches bCursorResLoaded,
// so it is a no-op on a second call. Slots A (0x2c0a), C (0x2c04) and D (0x2c01) are the same for
// every mode; only slot B -- the per-tool cursor bitmap -- is mode-dependent, which is what the
// 14-way switch picks. Three shapes of slot-B case: the animated ones (1/3/6 and 5) derive B's
// frame loop from the descriptor's own nButtonFrameCount; the plain ones (0, 2/4/7/8) realize a
// frame through the virtual GetOrLoadFrameBitmap and pin a single-frame loop; the 9-0xd group
// takes the ALREADY-loaded pShadowBitmap off the descriptor instead of realizing a frame at all.
// toolMode > 13 falls through the switch entirely, leaving slot B (and the frame loop) untouched.
void BuildToolCursorWnd::LoadCursorBitmapSet()
{
    if (bCursorResLoaded != 0) {
        return;
    }
    pDescC = g_UIResources.TileKind_GetOrLoadDescriptor(0x2c04);
    pBitmapC = pDescC->GetOrLoadFrameBitmap(0, 0);

    switch (toolMode) {
    case 10:
        pDescB = g_UIResources.TileKind_GetOrLoadDescriptor(0x2c05);
        pBitmapB = pDescB->pShadowBitmap;
        nFrameIndexBLoopStart = 0;
        nFrameIndexBLoopEnd = 0;
        nFrameIndexBStopValue = 1;
        break;
    case 9:
        pDescB = g_UIResources.TileKind_GetOrLoadDescriptor(0x2c0c);
        pBitmapB = pDescB->pShadowBitmap;
        nFrameIndexBLoopStart = 0;
        nFrameIndexBLoopEnd = 0;
        nFrameIndexBStopValue = 1;
        break;
    case 11:
        pDescB = g_UIResources.TileKind_GetOrLoadDescriptor(0x3865);
        pBitmapB = pDescB->pShadowBitmap;
        nFrameIndexBLoopStart = 0;
        nFrameIndexBLoopEnd = 0;
        nFrameIndexBStopValue = 1;
        break;
    case 12:
        pDescB = g_UIResources.TileKind_GetOrLoadDescriptor(0x3866);
        pBitmapB = pDescB->pShadowBitmap;
        nFrameIndexBLoopStart = 0;
        nFrameIndexBLoopEnd = 0;
        nFrameIndexBStopValue = 1;
        break;
    case 13:
        pDescB = g_UIResources.TileKind_GetOrLoadDescriptor(0x3867);
        pBitmapB = pDescB->pShadowBitmap;
        nFrameIndexBLoopStart = 0;
        nFrameIndexBLoopEnd = 0;
        nFrameIndexBStopValue = 1;
        break;
    case 5:
        pDescB = g_UIResources.TileKind_GetOrLoadDescriptor(0x2c02);
        pBitmapB = pDescB->GetOrLoadFrameBitmap(0, 0);
        nFrameIndexBLoopStart = 0;
        nFrameIndexBLoopEnd = pDescB->nButtonFrameCount - 3;
        nFrameIndexBStopValue = pDescB->nButtonFrameCount - 2;
        break;
    case 0:
        pDescB = g_UIResources.TileKind_GetOrLoadDescriptor(0x387b);
        pBitmapB = pDescB->GetOrLoadFrameBitmap(0, 0);
        nFrameIndexBLoopStart = 0;
        nFrameIndexBLoopEnd = 0;
        nFrameIndexBStopValue = 1;
        break;
    case 1:
    case 3:
    case 6:
        pDescB = g_UIResources.TileKind_GetOrLoadDescriptor(0x2c03);
        pBitmapB = pDescB->GetOrLoadFrameBitmap(0, 0);
        nFrameIndexBLoopStart = 0;
        nFrameIndexBLoopEnd = pDescB->nButtonFrameCount - 3;
        nFrameIndexBStopValue = pDescB->nButtonFrameCount - 2;
        break;
    case 2:
    case 4:
    case 7:
    case 8:
        pDescB = g_UIResources.TileKind_GetOrLoadDescriptor(0x2c05);
        pBitmapB = pDescB->GetOrLoadFrameBitmap(0, 0);
        nFrameIndexBLoopStart = 0;
        nFrameIndexBLoopEnd = 0;
        nFrameIndexBStopValue = 1;
        break;
    }

    nFrameIndexB = nFrameIndexBLoopStart;
    nFrameIndexC = 0;
    pDescA = g_UIResources.TileKind_GetOrLoadDescriptor(0x2c0a);
    pBitmapA = pDescA->GetOrLoadFrameBitmap(0, 0);
    pDescD = g_UIResources.TileKind_GetOrLoadDescriptor(0x2c01);
    pBitmapD = pDescD->GetOrLoadFrameBitmap(0, 0);
    bCursorResLoaded = 1;
    if (pDescB == NULL || pBitmapB == NULL || pDescC == NULL || pBitmapC == NULL ||
        pDescA == NULL || pBitmapA == NULL || pDescD == NULL || pBitmapD == NULL) {
        OutputDebugStringA("Error loading bmps");
    }
}

// FUNCTION: LOCO 0x437900 // EFFECTIVE MATCH -- residual is a pure register-allocation
// scheduling difference (candidate needs an extra callee-saved register (ebp) to hold pBitmap
// across the function body vs the original's single stack reload right before the call;
// confirmed intrinsic via 3 independent source-shape probes -- 1-local vs 2-local RECT
// construction, an explicit `RECT destRect = *pRect;` local, and CursorDesc* vs void* for
// the dead pDescUnused param -- all three compiled to the byte-identical 2466-byte .obj).
// See docs/PARKED.md.
void BuildToolCursorWnd::DrawIconFrame(RECT *pRect, int nFrameIndex, CursorDesc *pDescUnused, LocoBitmap *pBitmap)
{
    if (bIconDrawReadyFlag != 0) {
        RECT localRect;
        localRect.left = 0;
        localRect.top = 0;
        localRect.right = pRect->right - pRect->left;
        localRect.bottom = pRect->bottom - pRect->top;
        if (nFrameIndex != 0) {
            OffsetRect(&localRect, nFrameIndex * localRect.right, 0);
        }
        RECT srcRect;
        srcRect.top = localRect.top;
        srcRect.left = localRect.left;
        srcRect.right = localRect.right;
        srcRect.bottom = localRect.bottom;
        unsigned char ok = pBitmap->RestoreOverlapBlt(*pRect, pOffscreenSurface, srcRect, 1);
        if (ok == 0) {
            OutputDebugStringA("Error drawing mb bitmap");
        }
    }
}

// FUNCTION: LOCO 0x437cf0
void BuildToolCursorWnd::DrawToolLabel()
{
    if (toolMode != 0 && toolMode == 8) {
        char szText[512];
        g_UIResources.LoadLocaleString(0x6c, szText, sizeof(szText));
        HDC hdc = AcquireOffscreenSurfaceDC(hwndSelf);
        COLORREF oldColor = SetTextColor(hdc, 0xff5c00);
        int oldBkMode = SetBkMode(hdc, TRANSPARENT);
        HGDIOBJ oldFont = SelectObject(hdc, g_UIResources.m_hFont16);
        RECT rect;
        CopyRect(&rect, &rectD);
        int cchText = strlen(szText);
        DrawTextA(hdc, szText, cchText, &rect, DT_CENTER | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        CenterRectInRect(&rectD, &rect);
        DrawTextA(hdc, szText, cchText, &rect, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        OffsetRect(&rect, 2, 0);
        DrawTextA(hdc, szText, cchText, &rect, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        OffsetRect(&rect, 0, 2);
        DrawTextA(hdc, szText, cchText, &rect, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        OffsetRect(&rect, -2, 0);
        DrawTextA(hdc, szText, cchText, &rect, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        OffsetRect(&rect, 1, -1);
        SetTextColor(hdc, 0xffffff);
        DrawTextA(hdc, szText, cchText, &rect, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(hdc, oldFont);
        SetTextColor(hdc, oldColor);
        SetBkMode(hdc, oldBkMode);
        CommitScreenUpdate(hwndSelf, hdc, 0);
    }
}

// FUNCTION: LOCO 0x437ea0 // EFFECTIVE MATCH -- content-complete first compile; asmscore
// total 99142 (align=98 is masked jump-table noise), byte_diff 52, insns 72/76. The WHOLE
// residual is the same /Og cross-jump-granularity class parked on NotifyToolModeChanged
// (0x438890) in this very file: the original keeps each switch arm's OWN
// `g_pApp`/`[eax+8]`/`push 0x401`/`push hwnd` sequence and cross-jumps ONLY the bare
// `call [PostMessageA]` (case 8's `jmp` lands on the list arm's call), while this compile
// suffix-merges everything from the `g_pApp` load onward, so the case-8 arm loses its 4
// instruction load/push group (72 vs 76 insns) and the list arm's `mov edx,[g_pApp]`
// hoist-before-push scheduling goes with it. Three source-shape probes, all byte-identical
// .obj output (same refutation as 0x438890's own three): per-case `HWND hwnd` local in the
// case-8 arm only, in the case-list arm only, and in both. See docs/PARKED.md.
//
// vtable slot 0x7c override of PopupWndBase::OnClose (WM_CLOSE) -- the class vtable dword at
// 0x4781ac (0x478130+0x7c) is this address, and the fall-through call target at its tail is the
// base body 0x414b80. While the app is alive and not tearing down (g_pApp set, g_nScreenState
// != 10) the message box SWALLOWS the close: it runs NotifyToolModeChanged's own commit tail
// (clear bModeChangeNotifyPending, PlayUiSound(0x5015), the toolMode-keyed PostMessageA switch
// with case 8's body first in .text -- hence declared first below, per the "switch case bodies
// laid out in source declaration order" rule -- then the vtable+4 OnExit call and
// bIconDrawReadyFlag = 0) and returns 0. Only during shutdown does the base OnClose (destroy
// the window) run. The SETZ-materialized IsNetShuttingDownMaybe result doubles as the zero
// stored into bModeChangeNotifyPending on the swallow path (AL is provably 0 past the JNZ).
LRESULT BuildToolCursorWnd::OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_pApp == NULL || IsNetShuttingDownMaybe() != 0) {
        return PopupWndBase::OnClose(hwndMsg, msg, wParam, lParam);
    }
    bModeChangeNotifyPending = 0;
    g_UIResources.PlayUiSound(0x5015);

    unsigned int mode = toolMode;
    switch (mode) {
    case 8:
        PostMessageA(g_pApp->hwndOwner, 0x401, 8, (unsigned char)bModeChangeNotifyPending);
        break;
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
    case 9: case 10: case 11: case 12: case 13:
        if (bModeChangeNotifyPending != 0) {
            PostMessageA(g_pApp->hwndOwner, 0x401, mode, toolParam2Maybe);
        }
        break;
    default:
        goto skipVtbl;
    }
    this->OnExit();
skipVtbl:
    bIconDrawReadyFlag = 0;
    return 0;
}

// FUNCTION: LOCO 0x4379c0 // EFFECTIVE MATCH -- same intrinsic register-allocation/scheduling
// residual class as DrawIconFrame above (229/234 insns matching structurally; remaining
// diff is register-only reshuffles plus a couple of allocator tie-breaks: (1) the original
// loads each slot's pBitmapX/nFrameIndexX unconditionally right after the bIconDrawReadyFlag re-check,
// before the `if` even branches -- confirmed via raw disasm; hoisting these into locals
// declared at the same point (pBitmapB/nFrameB/etc below) improved the match (score
// 458943->458315, insns 223->229/234) but the compiler still doesn't always materialize the
// load at that exact point, since C++ local-declaration order doesn't force scheduling; (2) a
// `test reg,reg` vs `cmp reg,zeroreg` tie-break on the nFrameIndex!=0 checks, where our
// compile opportunistically reuses an already-zeroed register from the adjacent RECT.left/top
// zero-fill instead of emitting a fresh test -- same class as the documented tri-state/
// zero-comparison register-reuse residuals elsewhere in this project). See docs/PARKED.md.
void BuildToolCursorWnd::OnDrawContent(PAINTSTRUCT *pPs)
{
    if (bCursorResLoaded == 0) {
        return;
    }
    if (bIconDrawReadyFlag == 0) {
        bIconDrawReadyFlag = 1;
    }

    LocoBitmap *pBmpA = pBitmapA;
    if (bIconDrawReadyFlag != 0) {
        RECT localRect;
        localRect.right = rectWindow.right - rectWindow.left;
        localRect.bottom = rectWindow.bottom - rectWindow.top;
        localRect.left = 0;
        localRect.top = 0;
        RECT destRect;
        destRect.left = rectWindow.left;
        destRect.top = rectWindow.top;
        destRect.right = rectWindow.right;
        destRect.bottom = rectWindow.bottom;
        unsigned char ok = pBmpA->RestoreOverlapBlt(destRect, pOffscreenSurface, localRect, 1);
        if (ok == 0) {
            OutputDebugStringA("Error drawing mb bitmap");
        }
    }

    LocoBitmap *pBmpB = pBitmapB;
    unsigned int nFrameB = nFrameIndexB;
    if (bIconDrawReadyFlag != 0) {
        RECT localRect;
        localRect.left = 0;
        localRect.top = 0;
        localRect.right = rectIconSlotB.right - rectIconSlotB.left;
        localRect.bottom = rectIconSlotB.bottom - rectIconSlotB.top;
        if (nFrameB != 0) {
            OffsetRect(&localRect, nFrameB * localRect.right, 0);
        }
        RECT destRect;
        destRect.left = rectIconSlotB.left;
        destRect.top = rectIconSlotB.top;
        destRect.right = rectIconSlotB.right;
        destRect.bottom = rectIconSlotB.bottom;
        RECT srcRect;
        srcRect.top = localRect.top;
        srcRect.left = localRect.left;
        srcRect.right = localRect.right;
        srcRect.bottom = localRect.bottom;
        unsigned char ok = pBmpB->RestoreOverlapBlt(destRect, pOffscreenSurface, srcRect, 1);
        if (ok == 0) {
            OutputDebugStringA("Error drawing mb bitmap");
        }
    }

    LocoBitmap *pBmpC = pBitmapC;
    unsigned int nFrameC = nFrameIndexC;
    if (bIconDrawReadyFlag != 0) {
        RECT localRect;
        localRect.left = 0;
        localRect.top = 0;
        localRect.right = rectIconSlotC.right - rectIconSlotC.left;
        localRect.bottom = rectIconSlotC.bottom - rectIconSlotC.top;
        if (nFrameC != 0) {
            OffsetRect(&localRect, nFrameC * localRect.right, 0);
        }
        RECT destRect;
        destRect.left = rectIconSlotC.left;
        destRect.top = rectIconSlotC.top;
        destRect.right = rectIconSlotC.right;
        destRect.bottom = rectIconSlotC.bottom;
        RECT srcRect;
        srcRect.top = localRect.top;
        srcRect.left = localRect.left;
        srcRect.right = localRect.right;
        srcRect.bottom = localRect.bottom;
        unsigned char ok = pBmpC->RestoreOverlapBlt(destRect, pOffscreenSurface, srcRect, 1);
        if (ok == 0) {
            OutputDebugStringA("Error drawing mb bitmap");
        }
    }

    StartSlotDAnimation(4);
    this->SetCursorDesc(cursorNormal.nMaskSurfaceKey, cursorNormal.pDesc, 0, 1);

    unsigned int nFrameD = nCurrentFrameIndex;
    LocoBitmap *pBmpD = pBitmapD;
    if (bIconDrawReadyFlag != 0) {
        RECT localRect;
        localRect.left = 0;
        localRect.top = 0;
        localRect.right = rectD.right - rectD.left;
        localRect.bottom = rectD.bottom - rectD.top;
        if (nFrameD != 0) {
            OffsetRect(&localRect, nFrameD * localRect.right, 0);
        }
        RECT destRect;
        destRect.left = rectD.left;
        destRect.top = rectD.top;
        destRect.right = rectD.right;
        destRect.bottom = rectD.bottom;
        RECT srcRect;
        srcRect.top = localRect.top;
        srcRect.left = localRect.left;
        srcRect.right = localRect.right;
        srcRect.bottom = localRect.bottom;
        unsigned char ok = pBmpD->RestoreOverlapBlt(destRect, pOffscreenSurface, srcRect, 1);
        if (ok == 0) {
            OutputDebugStringA("Error drawing mb bitmap");
        }
    }

    DrawToolLabel();
}

// FUNCTION: LOCO 0x438280 // EFFECTIVE MATCH -- 205/211 insns structurally matching
// (asmscore total 501421, byte_diff 381/760). Content-complete and fully transcribed (all 5
// states, the shared draw tail via goto, the CursorAnimFrameEntry table reads); residual is
// register-allocation/scheduling noise (the same class documented for DrawIconFrame/
// DrawAllIconSlots above) plus a couple of prologue/comparison-shape deltas not resolved this
// session. See docs/PARKED.md.
void BuildToolCursorWnd::StartSlotDAnimation(unsigned int nState)
{
    if (nAnimState == (unsigned short)nState) {
        return;
    }

    CursorDesc *pDesc;
    LocoBitmap *pBitmap;
    unsigned int nFrame;
    unsigned short nFrame16;

    switch ((unsigned short)nState) {
    case 0:
        pDesc = pDescD;
        nAnimState = 0;
        nAnimSubPhase = (short)pDesc->wActiveFrameSetIndex;
        nFrame = pDesc->paFrameEntries[(short)pDesc->wActiveFrameSetIndex].nStartFrame;
        pBitmap = pBitmapD;
        nCurrentFrameIndex = nFrame;
        goto drawTail;
    case 1:
        pDesc = pDescD;
        nAnimState = 1;
        nAnimSubPhase = 2;
        nFrame16 = pDesc->paFrameEntries[2].nStartFrame;
        break;
    case 2:
        nAnimSubPhase = 4;
        pDesc = pDescD;
        nAnimState = 2;
        nFrame16 = pDesc->paFrameEntries[4].nStartFrame;
        break;
    case 3:
        nAnimState = 3;
        nAnimSubPhase = 0;
        pBitmap = pBitmapD;
        nFrame = pDescD->paFrameEntries[0].nStartFrame;
        nCurrentFrameIndex = nFrame;
        if (bIconDrawReadyFlag != 0) {
            RECT localRect;
            localRect.right = rectD.right - rectD.left;
            localRect.left = 0;
            localRect.bottom = rectD.bottom - rectD.top;
            localRect.top = 0;
            if (nFrame != 0) {
                OffsetRect(&localRect, nFrame * localRect.right, 0);
            }
            RECT destRect;
            destRect.left = rectD.left;
            destRect.top = rectD.top;
            destRect.right = rectD.right;
            destRect.bottom = rectD.bottom;
            RECT srcRect;
            srcRect.top = localRect.top;
            srcRect.left = localRect.left;
            srcRect.right = localRect.right;
            srcRect.bottom = localRect.bottom;
            unsigned char ok = pBitmap->RestoreOverlapBlt(destRect, pOffscreenSurface, srcRect, 1);
            if (ok == 0) {
                OutputDebugStringA("Error drawing mb bitmap");
            }
        }
        DrawToolLabel();
        CommitScreenUpdate(hwndSelf, 0, 0);
        return;
    case 4:
        nAnimState = 4;
        pBitmap = pBitmapD;
        nAnimSubPhase = 5;
        nFrame = pDescD->paFrameEntries[5].nStartFrame;
        nCurrentFrameIndex = nFrame;
        if (bIconDrawReadyFlag != 0) {
            RECT localRect;
            localRect.right = rectD.right - rectD.left;
            localRect.left = 0;
            localRect.bottom = rectD.bottom - rectD.top;
            localRect.top = 0;
            if (nFrame != 0) {
                OffsetRect(&localRect, nFrame * localRect.right, 0);
            }
            RECT destRect;
            destRect.left = rectD.left;
            destRect.top = rectD.top;
            destRect.right = rectD.right;
            destRect.bottom = rectD.bottom;
            RECT srcRect;
            srcRect.top = localRect.top;
            srcRect.left = localRect.left;
            srcRect.right = localRect.right;
            srcRect.bottom = localRect.bottom;
            unsigned char ok = pBitmap->RestoreOverlapBlt(destRect, pOffscreenSurface, srcRect, 1);
            if (ok == 0) {
                OutputDebugStringA("Error drawing mb bitmap");
            }
        }
        DrawToolLabel();
        CommitScreenUpdate(hwndSelf, 0, 0);
        return;
    default:
        return;
    }

    nFrame = nFrame16;
    pBitmap = pBitmapD;
    nCurrentFrameIndex = nFrame;
drawTail:
    DrawIconFrame(&rectD, nFrame, pDesc, pBitmap);
    DrawToolLabel();
    CommitScreenUpdate(hwndSelf, 0, 0);
}

// FUNCTION: LOCO 0x438590 // EFFECTIVE MATCH -- 214/226 insns structurally matching (asmscore
// total 214784, byte_diff 184/752). Content-complete: all 5 states (0-2 shared-tail table
// reads, state 3's counting loop, state 4's 2-phase sub-machine with its own direct-rectD
// blit shape distinct from StartSlotDAnimation's copy-local shape). Residual is the same register-
// allocation/scheduling class documented elsewhere in this cluster. See docs/PARKED.md.
void BuildToolCursorWnd::AdvanceSlotDAnimation()
{
    unsigned int nFrame = nCurrentFrameIndex;
    if (nFrame == 0xffffffff) {
        return;
    }

    CursorDesc *pDesc;
    LocoBitmap *pBitmap;
    unsigned int nFrameIndex;

    switch (nAnimState) {
    case 0:
        pDesc = pDescD;
        nAnimTickCount = nAnimTickCount + 1;
        nFrameIndex = pDesc->paFrameEntries[(short)pDesc->wActiveFrameSetIndex].nStartFrame;
        break;
    case 1:
        pDesc = pDescD;
        nFrameIndex = pDesc->paFrameEntries[2].nStartFrame;
        break;
    case 2:
        pDesc = pDescD;
        nFrameIndex = pDesc->paFrameEntries[4].nStartFrame;
        break;
    case 3:
        pDesc = pDescD;
        if (pDesc->paFrameEntries[0].nEndFrame - 1 <= (int)nFrame) {
            StartSlotDAnimation(0);
            return;
        }
        nFrameIndex = nFrame + 1;
        break;
    case 4:
        pDesc = pDescD;
        if (nAnimSubPhase == 5) {
            if (nFrame == pDesc->paFrameEntries[5].nEndFrame) {
                nAnimSubPhase = 6;
                nFrameIndex = pDesc->paFrameEntries[6].nStartFrame;
                break;
            }
            pBitmap = pBitmapD;
            nFrame = nFrame - 1;
            nCurrentFrameIndex = nFrame;
            if (bIconDrawReadyFlag != 0) {
                RECT localRect;
                localRect.right = rectD.right - rectD.left;
                localRect.left = 0;
                localRect.bottom = rectD.bottom - rectD.top;
                localRect.top = 0;
                if (nFrame != 0) {
                    OffsetRect(&localRect, nFrame * localRect.right, 0);
                }
                RECT srcRect;
                srcRect.top = localRect.top;
                srcRect.left = localRect.left;
                srcRect.right = localRect.right;
                srcRect.bottom = localRect.bottom;
                unsigned char ok = pBitmap->RestoreOverlapBlt(rectD, pOffscreenSurface, srcRect, 1);
                if (ok == 0) {
                    OutputDebugStringA("Error drawing mb bitmap");
                }
            }
        } else {
            if (nFrame == (unsigned int)(pDesc->paFrameEntries[6].nEndFrame - 1)) {
                StartSlotDAnimation(0);
                return;
            }
            pBitmap = pBitmapD;
            nFrame = nFrame + 1;
            nCurrentFrameIndex = nFrame;
            if (bIconDrawReadyFlag != 0) {
                RECT localRect;
                localRect.right = rectD.right - rectD.left;
                localRect.left = 0;
                localRect.bottom = rectD.bottom - rectD.top;
                localRect.top = 0;
                if (nFrame != 0) {
                    OffsetRect(&localRect, nFrame * localRect.right, 0);
                }
                RECT srcRect;
                srcRect.top = localRect.top;
                srcRect.left = localRect.left;
                srcRect.right = localRect.right;
                srcRect.bottom = localRect.bottom;
                unsigned char ok = pBitmap->RestoreOverlapBlt(rectD, pOffscreenSurface, srcRect, 1);
                if (ok == 0) {
                    OutputDebugStringA("Error drawing mb bitmap");
                }
            }
        }
        DrawToolLabel();
        CommitScreenUpdate(hwndSelf, 0, 0);
        return;
    default:
        return;
    }

    nCurrentFrameIndex = nFrameIndex;
    DrawIconFrame(&rectD, nFrameIndex, pDesc, pBitmapD);
    DrawToolLabel();
    CommitScreenUpdate(hwndSelf, 0, 0);
}

// FUNCTION: LOCO 0x438890 // EFFECTIVE MATCH -- asmscore total 114717, byte_diff 57, insns 51/54
// ⚠ scored with `--len 0xb0` (= 0x438940 - 0x438890, the NEXT function's start). This function
// ends in a jump table, so both obvious length sources give the CODE extent (137 B) instead of
// the COMDAT extent and truncate the window -- CLAUDE.md's third `--len` trap. The older
// "total 148396, byte_diff 66/150" note recorded here was one of those truncated readings AND
// predates the v544 signature fix below; it is not comparable and has been replaced.
//
// v544: this is the class's real vtable slot-0x60 (WM_KILLFOCUS) override -- see the
// declaration's own note in src/BuildToolCursorWnd.h for how tools/vtable_audit.py found it and
// why the image's `ret 0x10` settles the signature. The previous zero-argument `void
// NotifyToolModeChanged()` model could not emit the original's `xor eax,eax; pop esi; ret 0x10`
// epilogue at all, so the fix is worth real bytes at the tail, not just correctness on paper.
//
// Content-complete (case 8 vs the shared case-list body, the byte-remap+jump-table dispatch
// reproduced by declaring case 8 first per the "switch case bodies laid out in source
// declaration order" rule). Residual is UNCHANGED by the signature fix and still the same
// class: the compiler CSEs/hoists the g_pApp->hwndOwner load across both mutually-exclusive
// case bodies (the original re-loads `[0x4aa4a0]` independently in each arm) even when written
// as two fully independent PostMessageA call statements (tried: shared goto tail, per-case
// locals, inlined expressions -- all 3 produced byte-identical output, ruling out simple
// source-shape levers). See docs/PARKED.md.
LRESULT BuildToolCursorWnd::OnKillFocus(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    bModeChangeNotifyPending = 0;
    g_UIResources.PlayUiSound(0x5015);

    // Local deliberately NOT named `wParam` any more: since v544 this function's real signature
    // is the four-argument WM_KILLFOCUS handler, so that name is taken by the message's own
    // wParam -- which this body never reads. (VC5 diagnoses the collision as C2082 rather than
    // silently shadowing, unlike the member-vs-local case CLAUDE.md warns about.)
    unsigned int nToolMode = toolMode;
    switch (nToolMode) {
    case 8:
        PostMessageA(g_pApp->hwndOwner, 0x401, 8, (unsigned char)bModeChangeNotifyPending);
        break;
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
    case 9: case 10: case 11: case 12: case 13:
        if (bModeChangeNotifyPending != 0) {
            PostMessageA(g_pApp->hwndOwner, 0x401, nToolMode, toolParam2Maybe);
        }
        break;
    default:
        goto skipVtbl;
    }
    this->OnExit();
skipVtbl:
    bIconDrawReadyFlag = 0;
    return 0;
}

// FUNCTION: LOCO 0x438940 // EFFECTIVE MATCH -- 109/116 insns structurally matching (asmscore
// total 230339, byte_diff 179/388). This is the WM_TIMER handler driving the whole 4-slot
// animation cluster -- content-complete (hwndSelf gate, the 0x27-tick idle-state restart,
// AdvanceSlotDAnimation's own per-tick call, and slot B's independent frame-counter advance+blit).
// Residual matches the DrawIconFrame/DrawAllIconSlots family: the compiler needs an extra
// callee-saved register (ebp) to hold a value across the RestoreOverlapBlt call that the
// original reloads from a struct field instead. See docs/PARKED.md.
LRESULT BuildToolCursorWnd::OnTimerDefault(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (bIconDrawReadyFlag == 0) {
        return 0;
    }
    if (hWnd == hwndSelf) {
        if (nAnimState == 0 && nAnimTickCount > 0x27) {
            nAnimTickCount = 0;
            StartSlotDAnimation(3);
        }
        AdvanceSlotDAnimation();
        if (nFrameIndexB != nFrameIndexBStopValue) {
            if (nFrameIndexBLoopStart != nFrameIndexBLoopEnd) {
                unsigned int nFrame = nFrameIndexB + 1;
                nFrameIndexB = nFrame;
                if ((int)nFrameIndexBLoopEnd < (int)nFrame) {
                    nFrameIndexB = nFrameIndexBLoopStart;
                }
                LocoBitmap *pBitmap = pBitmapB;
                if (bIconDrawReadyFlag != 0) {
                    RECT localRect;
                    localRect.right = rectIconSlotB.right - rectIconSlotB.left;
                    localRect.bottom = rectIconSlotB.bottom - rectIconSlotB.top;
                    localRect.left = 0;
                    localRect.top = 0;
                    if (nFrameIndexB != 0) {
                        OffsetRect(&localRect, nFrameIndexB * localRect.right, 0);
                    }
                    RECT destRect;
                    destRect.left = rectIconSlotB.left;
                    destRect.top = rectIconSlotB.top;
                    destRect.right = rectIconSlotB.right;
                    destRect.bottom = rectIconSlotB.bottom;
                    RECT srcRect;
                    srcRect.top = localRect.top;
                    srcRect.left = localRect.left;
                    srcRect.right = localRect.right;
                    srcRect.bottom = localRect.bottom;
                    unsigned char ok = pBitmap->RestoreOverlapBlt(destRect, pOffscreenSurface, srcRect, 1);
                    if (ok == 0) {
                        OutputDebugStringA("Error drawing mb bitmap");
                    }
                }
                CommitScreenUpdate(hwndSelf, 0, 0);
            }
        }
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x4370f0
// The shared "commit the tool-mode change" tail -- see the header. Written out a second time as
// the epilogue of NotifyToolModeChanged (0x438890) and of the "no" arm of OnLButtonDown/OnKeyDown,
// which is how the original reads: this out-of-line copy exists because the confirm ("yes") arm
// calls it, while the other three sites expand it.
void BuildToolCursorWnd::PostToolModeChangeMaybe()
{
    unsigned int wParam = toolMode;
    switch (wParam) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
    case 9: case 10: case 11: case 12: case 13:
        if (bModeChangeNotifyPending != 0) {
            PostMessageA(g_pApp->hwndOwner, 0x401, wParam, toolParam2Maybe);
        }
        break;
    case 8:
        PostMessageA(g_pApp->hwndOwner, 0x401, 8, (unsigned char)bModeChangeNotifyPending);
        break;
    default:
        goto skipVtbl;
    }
    this->OnExit();
skipVtbl:
    bIconDrawReadyFlag = 0;
}

// vtable slot 0x3c -- WM_RBUTTONDOWN is simply WM_LBUTTONDOWN here, forwarded THROUGH the
// vtable exactly as an unqualified call to a virtual member compiles; do not "optimize" it to a
// class-qualified call. UNMARKED: this body ICF-folds onto 0x451520, whose marker lives on
// TutorialWnd::OnRButtonDown (src/TutorialWnd.cpp). The PopupWndBase hierarchy's copies fold to a
// DIFFERENT address than the other family's purely because the slot displacement differs.
LRESULT BuildToolCursorWnd::OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return OnLButtonDown(hwndMsg, msg, wParam, lParam);
}

// vtable slot 0x90 -- WM_WINDOWPOSCHANGING, this window's own last slot. A bare `return 1`, so it
// ICF-folds onto 0x426ac0, whose marker lives on WindowBase::OnEraseBkgnd (src/WindowBase.cpp) --
// UNMARKED here for that reason. The two have nothing to do with each other semantically; a
// one-instruction body is simply small enough that any two of them are the same function.
LRESULT BuildToolCursorWnd::OnWindowPosChanging(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return 1;
}

// FUNCTION: LOCO 0x437f90
// The mouse half of the yes/no confirmation -- see the header for the shape.
LRESULT BuildToolCursorWnd::OnLButtonDown(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (bIconDrawReadyFlag == 0) {
        return 0;
    }

    POINT pt;
    pt.x = (unsigned int)lParam & 0xffff;
    pt.y = (unsigned int)lParam >> 16;

    RECT *pRect = &rectIconSlotB;
    if (PtInRect(pRect, pt)) {
        nFrameIndexB = nFrameIndexBStopValue;
        DrawIconFrame(pRect, nFrameIndexBStopValue, pDescB, pBitmapB);
        CommitScreenUpdate(hwndSelf, 0, 0);
        Sleep(150);
        nFrameIndexB = nFrameIndexBLoopStart;
        DrawIconFrame(pRect, nFrameIndexBLoopStart, pDescB, pBitmapB);
        CommitScreenUpdate(hwndSelf, 0, 0);
        bModeChangeNotifyPending = 1;
        g_UIResources.PlayUiSound(0x5015);
        PostToolModeChangeMaybe();
        return 0;
    }

    pRect = &rectIconSlotC;
    if (!PtInRect(pRect, pt)) {
        return 0;
    }

    LocoBitmap *pBitmap = pBitmapC;
    nFrameIndexC = 1;
    if (bIconDrawReadyFlag != 0) {
        RECT localRect;
        localRect.right = pRect->right - pRect->left;
        localRect.bottom = pRect->bottom - pRect->top;
        localRect.left = 0;
        localRect.top = 0;
        OffsetRect(&localRect, localRect.right, 0);
        RECT srcRect;
        srcRect.top = localRect.top;
        srcRect.left = localRect.left;
        srcRect.right = localRect.right;
        srcRect.bottom = localRect.bottom;
        unsigned char ok = pBitmap->RestoreOverlapBlt(*pRect, pOffscreenSurface, srcRect, 1);
        if (ok == 0) {
            OutputDebugStringA("Error drawing mb bitmap");
        }
    }
    CommitScreenUpdate(hwndSelf, 0, 0);
    Sleep(150);
    nFrameIndexC = 0;
    if (bIconDrawReadyFlag != 0) {
        RECT localRect;
        localRect.right = pRect->right - pRect->left;
        localRect.bottom = pRect->bottom - pRect->top;
        localRect.top = 0;
        localRect.left = 0;
        RECT srcRect;
        srcRect.top = localRect.top;
        srcRect.left = localRect.left;
        srcRect.right = localRect.right;
        srcRect.bottom = localRect.bottom;
        unsigned char ok = pBitmap->RestoreOverlapBlt(*pRect, pOffscreenSurface, srcRect, 1);
        if (ok == 0) {
            OutputDebugStringA("Error drawing mb bitmap");
        }
    }
    CommitScreenUpdate(hwndSelf, 0, 0);
    bModeChangeNotifyPending = 0;
    g_UIResources.PlayUiSound(0x5015);

    unsigned int mode = toolMode;
    switch (mode) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
    case 9: case 10: case 11: case 12: case 13:
        if (bModeChangeNotifyPending != 0) {
            PostMessageA(g_pApp->hwndOwner, 0x401, mode, toolParam2Maybe);
        }
        break;
    case 8:
        PostMessageA(g_pApp->hwndOwner, 0x401, 8, (unsigned char)bModeChangeNotifyPending);
        break;
    default:
        goto skipVtbl;
    }
    this->OnExit();
skipVtbl:
    bIconDrawReadyFlag = 0;
    return 0;
}

// FUNCTION: LOCO 0x437180
// The keyboard half of the yes/no confirmation -- see the header. Enter/Y/y confirm, Esc/N/n
// cancel; each arm is the same code as OnLButtonDown's corresponding rect-hit arm. Unlike
// OnLButtonDown, the first slot-C blit here is NOT re-guarded on bIconDrawReadyFlag in the
// original: nothing between the entry check and that blit takes the address of anything inside
// `this`, so cl can prove the flag still holds and folds the re-test away. The guard is still
// written in both places -- it is DrawIconFrame's own gate, and cl decides where it survives.
LRESULT BuildToolCursorWnd::OnKeyDown(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (bIconDrawReadyFlag == 0) {
        return 0;
    }

    switch (wParam) {
    case VK_RETURN: case 'Y': case 'y':
        nFrameIndexB = nFrameIndexBStopValue;
        DrawIconFrame(&rectIconSlotB, nFrameIndexBStopValue, pDescB, pBitmapB);
        CommitScreenUpdate(hwndSelf, 0, 0);
        Sleep(150);
        nFrameIndexB = nFrameIndexBLoopStart;
        DrawIconFrame(&rectIconSlotB, nFrameIndexBLoopStart, pDescB, pBitmapB);
        CommitScreenUpdate(hwndSelf, 0, 0);
        bModeChangeNotifyPending = 1;
        g_UIResources.PlayUiSound(0x5015);
        PostToolModeChangeMaybe();
        break;

    case VK_ESCAPE: case 'N': case 'n': {
        RECT *pRect = &rectIconSlotC;
        LocoBitmap *pBitmap = pBitmapC;
        nFrameIndexC = 1;
        if (bIconDrawReadyFlag != 0) {
            RECT localRect;
            localRect.right = pRect->right - pRect->left;
            localRect.bottom = pRect->bottom - pRect->top;
            localRect.left = 0;
            localRect.top = 0;
            OffsetRect(&localRect, localRect.right, 0);
            RECT srcRect;
            srcRect.top = localRect.top;
            srcRect.left = localRect.left;
            srcRect.right = localRect.right;
            srcRect.bottom = localRect.bottom;
            unsigned char ok = pBitmap->RestoreOverlapBlt(*pRect, pOffscreenSurface, srcRect, 1);
            if (ok == 0) {
                OutputDebugStringA("Error drawing mb bitmap");
            }
        }
        CommitScreenUpdate(hwndSelf, 0, 0);
        Sleep(150);
        nFrameIndexC = 0;
        if (bIconDrawReadyFlag != 0) {
            RECT localRect;
            localRect.right = pRect->right - pRect->left;
            localRect.bottom = pRect->bottom - pRect->top;
            localRect.top = 0;
            localRect.left = 0;
            RECT srcRect;
            srcRect.top = localRect.top;
            srcRect.left = localRect.left;
            srcRect.right = localRect.right;
            srcRect.bottom = localRect.bottom;
            unsigned char ok = pBitmap->RestoreOverlapBlt(*pRect, pOffscreenSurface, srcRect, 1);
            if (ok == 0) {
                OutputDebugStringA("Error drawing mb bitmap");
            }
        }
        CommitScreenUpdate(hwndSelf, 0, 0);
        bModeChangeNotifyPending = 0;
        g_UIResources.PlayUiSound(0x5015);

        unsigned int mode = toolMode;
        switch (mode) {
        case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
        case 9: case 10: case 11: case 12: case 13:
            if (bModeChangeNotifyPending != 0) {
                PostMessageA(g_pApp->hwndOwner, 0x401, mode, toolParam2Maybe);
            }
            break;
        case 8:
            PostMessageA(g_pApp->hwndOwner, 0x401, 8, (unsigned char)bModeChangeNotifyPending);
            break;
        default:
            goto skipVtbl;
        }
        this->OnExit();
    skipVtbl:
        bIconDrawReadyFlag = 0;
        break;
    }
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x4374f0
// Vtable slot 0x18 -- see src/BuildToolCursorWnd.h for the layout summary. The window itself is
// sized to pDescA's native size and centered against the whole screen; SWP_HIDEWINDOW means the
// layout pass never shows the box, only Show does.
void BuildToolCursorWnd::RefreshClientRect()
{
    if (bCursorResLoaded != 0) {
        PopupWndBase::RefreshClientRect();

        rectIconSlotB.left = 0xa4;
        rectIconSlotB.top = 0x21;
        rectIconSlotB.bottom = pBitmapB->height + 0x21;
        // Unsigned division: pBitmapB->width is `unsigned` exactly so this emits the original's
        // `xor edx,edx; div edi` rather than a signed `idiv` -- see src/LocoBitmap.h.
        rectIconSlotB.right = pBitmapB->width / pDescB->nTotalFrameCount + 0xa4;
        rectIconSlotC.left = 0xa4;
        rectIconSlotC.top = 0x92;
        rectIconSlotC.right = pDescC->wShadowFrameWidth + 0xa4;
        rectIconSlotC.bottom = pDescC->wShadowBitmapHeight + 0x92;
        rectD.left = 10;
        rectD.top = 0x14;
        rectD.right = pDescD->nativeWidth + 10;
        rectD.bottom = pDescD->nativeHeight + 0x14;
        UnionRect(&rectIconSlotUnion, &rectIconSlotC, &rectIconSlotB);

        // X pair then Y pair, not left/top/right/bottom: the two spellings differ by exactly one
        // scheduling swap (`xor edx,edx` vs. the `top = 0` store) and only this one is EXACT.
        RECT rectWnd;
        rectWnd.left = 0;
        rectWnd.right = pDescA->nativeWidth;
        rectWnd.top = 0;
        rectWnd.bottom = pDescA->nativeHeight;

        RECT rectScreen;
        SetRectEmpty(&rectScreen);
        rectScreen.right = g_dwScreenWidth;
        rectScreen.bottom = g_dwScreenHeight;
        CenterRectInRect(&rectScreen, &rectWnd);
        SetWindowPos(hwndSelf, HWND_TOPMOST, rectWnd.left, rectWnd.top,
                     rectWnd.right - rectWnd.left, rectWnd.bottom - rectWnd.top,
                     SWP_HIDEWINDOW | SWP_NOACTIVATE);
    }
}

// FUNCTION: LOCO 0x438ad0
// Vtable slot 0x4c -- hover feedback for the yes/no confirmation. See src/BuildToolCursorWnd.h.
LRESULT BuildToolCursorWnd::OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    POINT pt;
    pt.x = LOWORD(lParam);
    pt.y = HIWORD(lParam);

    if (bIconDrawReadyFlag == 0) {
        return 0;
    }

    if (PtInRect(&rectIconSlotB, pt)) {
        this->SetCursorDesc(cursorHover.nMaskSurfaceKey, cursorHover.pDesc, 0, 1);
        StartSlotDAnimation(1);
    } else if (PtInRect(&rectIconSlotC, pt)) {
        this->SetCursorDesc(cursorHover.nMaskSurfaceKey, cursorHover.pDesc, 0, 1);
        StartSlotDAnimation(2);
    } else {
        this->SetCursorDesc(cursorNormal.nMaskSurfaceKey, cursorNormal.pDesc, 0, 1);
        // Only drop back to idle once the pointer has left the two buttons' bounding union --
        // being merely in the GAP between them leaves the current hover state alone.
        if (!PtInRect(&rectIconSlotUnion, pt) && (nAnimState == 1 || nAnimState == 2)) {
            StartSlotDAnimation(0);
        }
    }
    return PopupWndBase::OnMouseMove(hwndMsg, msg, wParam, lParam);
}
