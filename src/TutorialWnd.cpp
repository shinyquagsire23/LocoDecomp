#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream.h>
#include <strstrea.h>

#include "TutorialWnd.h"
#include "LocoBitmap.h"
#include "AlbumCardWnd.h"
#include "EditCardWnd.h"
#include "DSound.h"
#include "WorldBoardMaybe.h"
#include "PlacementCursorMaybe.h"
#include "UIResources.h"
#include "MailWnd.h"
#include "NetSessionEventQueue.h"
#include "DPlaySessionMgr.h"       // g_pDPlaySessionMgr (connectionMode)
#include "ScreenSaver.h"           // g_screenSaver.bScreenSaverMode
#include "IniFile.h"               // g_pIniFile -- the [TUTORIAL] "already shown" key
#include "LocalPlayerIdentity.h"   // g_pLocalPlayerIdentity->name -- that key's name

// The shared text-formatting scratch buffer (see src/WidgetPicker.cpp's own extern) -- used
// below only as a 1-byte template prefix for 3 local buffers.

// DrawEllipsis's own font -- a DIFFERENT global from g_UIResources.m_hFont16 above, and read at exactly
// one site in the whole image (this TU's), so nothing else pins down what it is.

// The shared DirectDraw work/back surface (DAT_004fd3c4) -- see src/PopupWndBase.cpp's own
// extern. RestorePresenterBackdrop blits the presenter box out of it.
extern IDirectDrawSurface *g_pDDrawWorkSurface;

// App screen-state selector (see docs/subsystems.md) -- Launch stamps this from
// nGlobalStateMirror, then hardcodes it to 8 (the tutorial-view app state) once the view is
// actually launching.
extern int g_nScreenState;
// Byte flag consulted once at Launch's own entry -- role not yet traced past this one
// read site.
// Board-scroll flag (0x485210): read by NotifyOrLaunch (latched into
// bBoardScrollFlagAtNotify) and by Launch's own scrollbar/window-style fixup. Same global in
// both places -- see TutorialWnd.h's note on bBoardScrollFlagAtNotify.
extern unsigned char g_bBoardScrollFlag;      // DAT_00485210
// Global screen half-width/half-height, used to anchor the popup's own PopupWndBase::Move
// call (screen center minus a fixed offset).
extern int g_dwScreenHalfWidth; // DAT_004851f8
extern int g_dwScreenHalfHeight; // DAT_004851fc

// 0x408130 -- the app's screen/UI-mode transition switch. See src/AlbumCardWnd.cpp.
extern void AppWindow_SetScreenState(int newState);
// 0x463670 -- the app's window-visibility pass. See src/Main.cpp.
extern unsigned char __stdcall FUN_00463670_LotsOfShowWindow(void);

// g_pMailWnd: declared in src/MailWnd.h -- only used here to hand its own
// WindowBase::SetCaptureMode a base pointer.
// g_pAlbumCardWnd: declared in src/AlbumCardWnd.h.

// FUNCTION: LOCO 0x44f490
// The bootstrap singleton's constructor: base subobject, then the field-init helper. The
// vtable store between the two is the compiler's own.
TutorialWnd::TutorialWnd(HINSTANCE hInstance, UINT resourceIdArg)
    : PopupWndBase(hInstance, resourceIdArg)
{
    ResourceRefCategoryTable_InitMaybe();
}

// FUNCTION: LOCO 0x44f4f0 (??_GTutorialWnd scalar deleting dtor -- compiler-generated,
// emitted by the destructor definition below)
// FUNCTION: LOCO 0x44f510
// The scalar-deleting-dtor thunk the vtable's slot 0 actually points at (0x44f4f0) is
// compiler-generated from this declaration, not hand-written.
TutorialWnd::~TutorialWnd()
{
    ReleaseIconResources();
}

// FUNCTION: LOCO 0x451440
void TutorialWnd::ReleaseIconResources() {
    if (pErrObj3 != NULL) {
        delete pErrObj3;
        pErrObj3 = NULL;
    }
    if (pErrObj1 != NULL) {
        delete pErrObj1;
        pErrObj1 = NULL;
    }
    if (pErrObj2 != NULL) {
        delete pErrObj2;
        pErrObj2 = NULL;
    }
    if (pErrObj4 != NULL) {
        delete pErrObj4;
        pErrObj4 = NULL;
    }
    if (pErrObj5 != NULL) {
        delete pErrObj5;
        pErrObj5 = NULL;
    }
    if (pErrObj9 != NULL) {
        delete pErrObj9;
        pErrObj9 = NULL;
    }
    if (pErrObj6 != NULL) {
        delete pErrObj6;
        pErrObj6 = NULL;
    }
    if (pErrObj7 != NULL) {
        delete pErrObj7;
        pErrObj7 = NULL;
    }
    if (pErrObj8 != NULL) {
        delete pErrObj8;
        pErrObj8 = NULL;
    }
    bIconResourcesLoadedFlag = 0;
}

// FUNCTION: LOCO 0x450ca0
// Sizes the popup to a fixed 0x240 x 0x170 centred on the desktop's client rect, then nudged
// 0x30 up-and-left, and hands that to PopupWndBase::Create along with icon 0x65.
unsigned char TutorialWnd::Create(HWND hwndOwner) {
    hIcon = LoadIconA((HINSTANCE)hInstance, MAKEINTRESOURCE(0x65));

    RECT rectDesktop;
    GetClientRect(GetDesktopWindow(), &rectDesktop);

    RECT rectWnd;
    rectWnd.left = 0;
    rectWnd.right = 0x240;
    rectWnd.top = 0;
    rectWnd.bottom = 0x170;
    CenterRectInRect(&rectDesktop, &rectWnd);
    // sic: offsets the DESKTOP rect, whose value is dead from here on -- the nudge never
    // reaches the window rect actually handed to Create below. Reproduced as written.
    OffsetRect(&rectDesktop, -0x30, -0x30);

    if (PopupWndBase::Create(0, hwndOwner, rectWnd.left, rectWnd.top,
                             rectWnd.right - rectWnd.left, rectWnd.bottom - rectWnd.top,
                             NULL, hIcon, 0, 0x86000000, 0, 1)) {
        return 1;
    }
    return 0;
}

// FUNCTION: LOCO 0x451180
// The constructor's field-init helper (see the declaration in TutorialWnd.h). The odd
// 3,1,2,4,5,9,6,7,8 allocation order is the original's, and ReleaseIconResources destroys them
// in exactly the same order. Note nTextLineHeight is written twice -- 1 up front, then -1 with
// the rest of the sentinel block; reproduced as written.
void TutorialWnd::ResourceRefCategoryTable_InitMaybe() {
    hIcon = NULL;
    pDSoundChannel = NULL;
    bIconResourcesLoadedFlag = 0;
    bPresenterActiveFlag = 0;
    bErrObj1Loaded = 1;
    bErrObj2Loaded = 1;
    bErrObj3Loaded = 1;
    bErrObjsLoaded = 0;
    nTextLineHeight = 1;
    nScrollTimerId = 0;

    pErrObj3 = new ResourceRef(0x3d01);
    pErrObj1 = new ResourceRef(0x3cff);
    pErrObj2 = new ResourceRef(0x3d00);
    pErrObj4 = new ResourceRef(0x3cfd);
    pErrObj5 = new ResourceRef(0);
    pErrObj9 = new ResourceRef(0x3cfe);
    pErrObj6 = new ResourceRef(0);
    pErrObj7 = new ResourceRef(0);
    pErrObj8 = new ResourceRef(0);

    for (int i = 0; i < 200; i++) {
        categoryRecords[i].bUsed = 0;
    }

    nSelectedItemIndex = -1;
    dwUnk0x305c = -1;
    dwUnk0x3060 = -1;
    nListScrollOffset = -1;
    nPrevItemIndex = -1;
    nNextItemIndex = -1;
    nPrevScanCursor = -1;
    nNextScanCursor = -1;
    nAnimTickCounter = -1;
    nTextLineHeight = -1;
    bCategoryFileLoaded = 0;
    nPresenterFrameIndex = 0;
    nGlobalStateMirror = 3;

    memset(categoryRecords, 0, sizeof(categoryRecords));

    lastNotifyCode = 0;
    lastNotifySubcode = 0;
}

// EFFECTIVE MATCH (asmscore.py --len 0x170: insns 112/112, align 60, byte_diff 38,
// total 61358; cc.sh DIFF(121) at len 369 vs 367). Content-complete. Three real source levers
// were found and applied, taking it 168870 -> 61358 in three compiles:
//   (1) declaration ORDER -- `char szPath[0x105] = "";` must precede `ifstream fileStream;`
//       (the original inits the path buffer before running the stream ctor), and `bLoaded`
//       must precede the ifstream too;
//   (2) the explicit `bool bHaveArchive = g_RFIndex.pFile != NULL;` intermediate, the same
//       lever CreditsWnd::BuildResourcePath needed -- it supplies the original's
//       `xor r,r; cmp; setne rl; test rl,rl` materialization that a plain `if` does not;
//   (3) NO redundant `bLoaded = 0;` inside the archive branch. Ghidra prints one there, but
//       it is provably dead (bLoaded is still 0 from its initializer) and writing it costs a
//       real extra instruction -- 113/112 with it, 112/112 without.
// The rest is the documented non-steerable class: cl picks EDX/EAX where the original picks
// ECX/EDX for the bHaveArchive materialization, and that choice then cascades through the
// LoadResource argument setup and the `delete pRfStream` virtual-base adjustor
// (`lea ecx,[eax+esi]` vs `add ecx,esi`); the inline `repnz scasb` strlen block is also
// scheduled a few instructions earlier. See docs/CODEGEN.md's setcc-widening bullet (4).
// The iostream library's own vbase-destructor helper for `ifstream`, instantiated into every TU
// that declares one on the stack -- nine of ours do, and the linker folded them to one copy. It is
// claimed here because this TU's ResourceRefCategoryTable_LoadCategoryFile below is the documented
// reference site for the whole RF-archive-first / loose-ifstream-fallback idiom.
//
// FUNCTION: LOCO 0x40e8b0 (??_Difstream vbase dtor helper -- library COMDAT, no source line)

// FUNCTION: LOCO 0x44fb10
// Loads the tutorial category table from "<install>tutorial\\tutorial.dat", RF archive first
// (LoadResource + an istrstream over the returned buffer), falling back to a loose-file
// ifstream when the archive isn't open, the resource isn't in it, or the archived copy fails
// to parse. Both paths hand the stream to ParseCategoryRecordsMaybe and return its verdict.
// Same shape as CreditsWnd::BuildResourcePath, which is the template for this idiom.
char TutorialWnd::ResourceRefCategoryTable_LoadCategoryFile() {
    char szPath[0x105] = "";
    char bLoaded = 0;
    ifstream fileStream;

    sprintf(szPath, "%s%s%s", g_pInstallPathPrefix, "tutorial\\", "tutorial.dat");

    bool bHaveArchive = g_RFIndex.pFile != NULL;
    if (bHaveArchive) {
        int nSize;
        void *pRfBuf = g_RFIndex.LoadResource(
            (const unsigned char *)(szPath + strlen(g_pInstallPathPrefix)), &nSize);
        if (pRfBuf != NULL) {
            istrstream *pRfStream = new istrstream((char *)pRfBuf, nSize);
            if (pRfStream != NULL) {
                bLoaded = ResourceRefCategoryTable_ParseCategoryRecordsMaybe(pRfStream);
                delete pRfStream;
            }
            _free(pRfBuf);
        }
    }

    if (bLoaded == 0) {
        fileStream.open(szPath, ios::nocreate);
        bLoaded = ResourceRefCategoryTable_ParseCategoryRecordsMaybe(&fileStream);
        fileStream.close();
    }
    return bLoaded;
}

// FUNCTION: LOCO 0x44fc80
// Parses the whitespace/token-delimited tutorial.dat stream into categoryRecords. One record per
// loop iteration, keyed by an index taken FROM the stream (not sequential), so the file may
// declare its 200 slots in any order and leave gaps.
//
// Record grammar, as read token by token (`\` and `,` are literal):
//     <index> <anything ending in \> [\<iconId>] [@<fieldD>] [#] <iconResId> <...\>
//     <fieldB> <...\> <descStringId> <...\> [ \l,t,r,b\ \l,t,r,b\ ]
// The bracketed leading sigils are optional and each defaults when absent (iconId 0xaf,
// fieldD 0x50f8, the `#` flag 0). The three "<...\>" slots are separator tokens that are only
// validated, never stored -- each must END in a backslash or the whole parse fails (return 0),
// which is the format's only real syntax check. The trailing pair of `\l,t,r,b\` rects is
// optional: when the token after the separator doesn't start with `\`, both rects are emptied
// instead. The table ends at a "-9" sentinel token (or at EOF).
//
// Each rect token arrives wrapped in backslashes (`\10,20,30,40\`); the original overwrites both
// with spaces in a scratch copy and then strtok's on "," -- so the leading/trailing space are
// what stop atoi and strtok from choking on the delimiters, and the scratch copy is why the
// stream token itself survives for the next comparison.
char TutorialWnd::ResourceRefCategoryTable_ParseCategoryRecordsMaybe(istream *pStream)
{
    char szToken[264];
    char szRectA[264];
    char szRectB[264];

    *pStream >> szToken;
    while (_stricmp(szToken, "-9") != 0 && !pStream->eof()) {
        int nIndex = atoi(szToken);

        *pStream >> szToken;
        if (szToken[strlen(szToken) - 1] != '\\') {
            return 0;
        }

        *pStream >> szToken;
        if (szToken[0] == '\\') {
            categoryRecords[nIndex].iconId = atoi(szToken + 1);
            *pStream >> szToken;
        } else {
            categoryRecords[nIndex].iconId = 0xaf;
        }

        if (szToken[0] == '@') {
            categoryRecords[nIndex].dwNarrationSoundId = atoi(szToken + 1);
            *pStream >> szToken;
        } else {
            categoryRecords[nIndex].dwNarrationSoundId = 0x50f8;
        }

        if (szToken[0] == '#') {
            categoryRecords[nIndex].bHashFlagMaybe = 1;
            *pStream >> szToken;
        } else {
            categoryRecords[nIndex].bHashFlagMaybe = 0;
        }

        categoryRecords[nIndex].dwIconResourceId = atoi(szToken);

        *pStream >> szToken;
        if (szToken[strlen(szToken) - 1] != '\\') {
            return 0;
        }

        *pStream >> szToken;
        categoryRecords[nIndex].dwTitleStringId = atoi(szToken);

        *pStream >> szToken;
        if (szToken[strlen(szToken) - 1] != '\\') {
            return 0;
        }

        *pStream >> szToken;
        categoryRecords[nIndex].dwDescriptionStringId = atoi(szToken);

        *pStream >> szToken;
        if (szToken[strlen(szToken) - 1] != '\\') {
            return 0;
        }

        categoryRecords[nIndex].bUsed = 1;

        *pStream >> szToken;
        // Negated deliberately: the original lays the two-call SetRectEmpty arm out INLINE
        // (fall-through) and the long rect-parsing arm out-of-line, which is the shape cl gives
        // the `!=` spelling. Writing it as `if (szToken[0] == '\\')` is otherwise identical --
        // same 311 instructions -- but swaps the two blocks and costs 106 align.
        if (szToken[0] != '\\') {
            SetRectEmpty(&categoryRecords[nIndex].rectA);
            SetRectEmpty(&categoryRecords[nIndex].rectB);
        } else {
            strcpy(szRectA, szToken);
            szRectA[0] = ' ';
            szRectA[strlen(szRectA) - 1] = ' ';
            categoryRecords[nIndex].rectA.left = atoi(strtok(szRectA, ","));
            categoryRecords[nIndex].rectA.top = atoi(strtok(NULL, ","));
            categoryRecords[nIndex].rectA.right = atoi(strtok(NULL, ","));
            categoryRecords[nIndex].rectA.bottom = atoi(strtok(NULL, ","));

            *pStream >> szToken;
            strcpy(szRectB, szToken);
            szRectB[0] = ' ';
            szRectB[strlen(szRectB) - 1] = ' ';
            categoryRecords[nIndex].rectB.left = atoi(strtok(szRectB, ","));
            categoryRecords[nIndex].rectB.top = atoi(strtok(NULL, ","));
            categoryRecords[nIndex].rectB.right = atoi(strtok(NULL, ","));
            categoryRecords[nIndex].rectB.bottom = atoi(strtok(NULL, ","));

            *pStream >> szToken;
        }
    }
    return 1;
}

// Applies a SetWindowPos-shaped scrollbar/window-style fixup -- role beyond the name not yet
// traced. Not yet transcribed.
void __cdecl AppWindow_ApplyDisplayModeMaybe(char bParam);  // 0x407d20, defined in src/AppWindow.cpp

// EFFECTIVE MATCH (asmscore.py --len 0x1c0: insns 112/112, align 0, byte_diff 12, total 452):
// content-complete and structurally identical -- every instruction pairs up and the block
// layout is byte-for-byte aligned. The whole residual is ONE intrinsic register tie-break
// appearing twice: after each arm's SetCaptureMode call the original reloads the singleton
// pointer into EAX (the 5-byte A1 short form), while cl picks EDX in the case-3 arm and ECX
// in the case-1 arm (6 bytes each) -- hence len 435 vs 433. Note the case-2 arm, written
// identically, DOES pick EAX, which is what makes this a coin-flip rather than a source
// lever. Probed and REJECTED (v396): rewriting the switch as an if/else-if chain, which
// costs far more (DIFF 117, align 308) because it relocates the default arm out of the
// fall-through position the original's `dec eax; je` ladder puts it in -- the switch is the
// right shape, confirmed. Do not re-grind; see docs/PARKED.md.
// FUNCTION: LOCO 0x450ae0
// vtable slot 4 -- TutorialWnd's override of PopupWndBase::OnExit. Un-realizes the five icon
// resources Launch realized, kills the scroll timer, drops the narration channel, then hands
// input capture back to whichever window launched the view (lastNotifySubcode), or -- for the
// standalone/world-view case -- repaints the board and re-stamps the app screen state.
void TutorialWnd::OnExit() {
    PopupWndBase::OnExit();
    bPresenterActiveFlag = 0;
    SetModalCapture(1);

    if (bIconResourcesLoadedFlag != 0) {
        pErrObj1->ReleaseRealized();
        pErrObj2->ReleaseRealized();
        pErrObj3->ReleaseRealized();
        pErrObj4->ReleaseRealized();
        if (pErrObj5->resourceId != 0) {
            pErrObj5->ReleaseRealized();
        }
        pErrObj9->ReleaseRealized();
        bIconResourcesLoadedFlag = 0;
    }

    KillTimer(hwndSelf, nScrollTimerId);

    // sic: pDSoundChannel is released here and then AGAIN below without being nulled in
    // between -- an original double-release. See docs/engine-bugs.md.
    if (pDSoundChannel != NULL) {
        pDSoundChannel->Release();
    }
    if (g_pDSoundManager != NULL && pDSoundChannel != NULL) {
        int nSoundId = pDSoundChannel->nSoundId;
        if (nSoundId != 0) {
            SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(nSoundId);
            pEntry->Release();
        }
        pDSoundChannel->Release();
    }

    // Latched from g_bBoardScrollFlag by NotifyOrLaunch; Launch's entry-side counterpart gates
    // on the same global. See the field's declaration in TutorialWnd.h.
    if (bBoardScrollFlagAtNotify == 1) {
        AppWindow_ApplyDisplayModeMaybe(0);
    }

    switch (lastNotifySubcode) {
    case 1:
        ((WindowBase *)g_pMailWnd)->SetCaptureMode(0);
        g_pMailWnd->bTearingDownMaybe = 0;
        break;
    case 2:
        g_pAlbumCardWnd->SetCaptureMode(0);
        g_pAlbumCardWnd->bInputBlocked = false;
        break;
    case 3:
        g_pEditCardWnd->SetCaptureMode(0);
        g_pEditCardWnd->nEditMode = 1;
        break;
    default:
        g_nScreenState = 4;
        g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
        g_worldBoard.UpdateDirtyTiles(0);
        if (lastNotifySubcode == 7 && lastNotifyCode == 0x2407) {
            g_NetSessionEventQueue.RebuildBoardFromPlacedObjectsMaybe();
        }
        g_nScreenState = 8;
        break;
    }
}

// EFFECTIVE MATCH (asmscore.py --len 0x32c: total 197994, insns 228/236, byte_diff 184):
// fully transcribed and content-complete -- caught and fixed a genuine Ghidra decompiler
// misattribution along the way (the common post-4-condition-check rect-snap block, which
// Ghidra printed using hallucinated `unaff_ESI`/`unaff_EBP` values; raw disasm confirmed it's
// really just a second reuse of the same `local_20`-shaped scratch RECT already used earlier
// in the function -- see docs/PARKED.md). Remaining residual is dominated by plain
// register-only swaps (Yoda #29/#30's intrinsic tie-break family) plus a handful of
// alternate-addressing-mode substitutions in the same family (lea-vs-mov+sub, materialized
// pointer vs. direct offset load) that resisted every source-shape probe tried -- see
// docs/PARKED.md for the retry log.
// FUNCTION: LOCO 0x450520
void TutorialWnd::SelectCategory(int nIndex) {
    if (bIconResourcesLoadedFlag != 0) {
        CursorDesc *pDesc = pErrObj4->pCursorDesc;
        nPresenterFrameIndex = pDesc->paFrameEntries[(short)pDesc->wActiveFrameSetIndex].nStartFrame;
    }
    nSelectedItemIndex = nIndex;

    if (pErrObj5->resourceId != 0) {
        pErrObj5->ReleaseRealized();
    }
    pErrObj5->resourceId = categoryRecords[nSelectedItemIndex].dwIconResourceId;
    pErrObj5->Load();

    RECT rect1;
    RECT rect2;
    ResourceRef *pObj5 = pErrObj5;
    if ((pObj5->pCursorDesc == NULL) || (pObj5->nRealizedHandle == 0)) {
        CopyRect(&rect2, &pErrObj6->rect);
        rect2.bottom = (rect2.top + 0x96) - ((rect2.top + 0x96) - rect2.top) % nTextLineHeight - 1;
        pErrObj6->rect = rect2;
    } else {
        SetRect(&rect1, 0, 0, pObj5->pCursorDesc->nativeWidth, pObj5->pCursorDesc->nativeHeight);
        OffsetRect(&rect1, 0xe8, 0);
        OffsetRect(&rect1, 0x96, 0xb2);
        CursorDesc *pDesc = pErrObj5->pCursorDesc;
        OffsetRect(&rect1, -(pDesc->nativeWidth >> 1), -(pDesc->nativeHeight >> 1));
        pObj5 = pErrObj5;
        pObj5->rect = rect1;

        CopyRect(&rect2, &pErrObj6->rect);
        rect2.bottom = (rect1.top - 10) - ((rect1.top - 10) - rect2.top) % nTextLineHeight - 1;
        pErrObj6->rect = rect2;
    }

    int idx = nSelectedItemIndex;
    if ((0x3c < categoryRecords[idx].rectA.right - categoryRecords[idx].rectA.left) &&
        (0x14 < categoryRecords[idx].rectA.bottom - categoryRecords[idx].rectA.top) &&
        (0x3c < categoryRecords[idx].rectB.right - categoryRecords[idx].rectB.left) &&
        (0x14 < categoryRecords[idx].rectB.bottom - categoryRecords[idx].rectB.top)) {
        CopyRect(&rect1, &categoryRecords[idx].rectA);
        rect1.bottom = rect1.bottom - (rect1.bottom - rect1.top) % nTextLineHeight - 1;
        pErrObj6->rect = rect1;

        CopyRect(&rect1, &categoryRecords[idx].rectB);
        pErrObj5->rect = rect1;
    }

    RefreshListAndNavState();

    if (nPrevItemIndex == -1) {
        bErrObj1Loaded = 0;
    } else {
        bErrObj1Loaded = 1;
    }
    if (nNextItemIndex == -1) {
        bErrObj2Loaded = 0;
    } else {
        bErrObj2Loaded = 1;
    }
    if (bErrObj2Loaded != 0) {
        bErrObjsLoaded = 1;
        return;
    }
    bErrObjsLoaded = 0;
}

// FUNCTION: LOCO 0x450850
int TutorialWnd::DrawDescriptionChunks(int nChunkCount, HDC *pHdc) {
    if (bCategoryFileLoaded == 0) {
        return -1;
    }

    COLORREF color = SetTextColor(*pHdc, 0xa0c0d1);
    int mode = SetBkMode(*pHdc, TRANSPARENT);
    HGDIOBJ hOldFont = SelectObject(*pHdc, g_UIResources.m_hFont16);

    char szLine1[0x200] = "";

    char szLine2[0x200] = "";

    char szText[0x200] = "";

    g_UIResources.LoadLocaleString(categoryRecords[nSelectedItemIndex].dwDescriptionStringId, szText, sizeof(szText));

    int nOffset = 0;
    int nChunk = 0;
    while (nChunk < nChunkCount) {
        if (nChunk >= 200) break;

        RECT rect;
        CopyRect(&rect, &pErrObj6->rect);

        strcpy(szLine1, &szText[nOffset]);
        strcpy(szLine2, &szText[nOffset]);
        int nLen = strlen(szLine1);
        DrawTextA(*pHdc, szLine1, nLen, &rect, DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS | DT_MODIFYSTRING);

        unsigned int i = 0;
        if (szLine1[0] == szLine2[0]) {
            do {
                i++;
                if ((int)i > (int)strlen(szLine1)) {
                    SelectObject(*pHdc, hOldFont);
                    SetBkMode(*pHdc, mode);
                    SetTextColor(*pHdc, color);
                    return -1;
                }
            } while (szLine1[i] == szLine2[i]);
        }

        char c = szLine1[i];
        for (; c != ' ' && i < 0x200;) {
            i--;
            c = szLine1[i];
        }

        nChunk++;
        nOffset = nOffset + 1 + i;
    }

    SelectObject(*pHdc, hOldFont);
    SetBkMode(*pHdc, mode);
    SetTextColor(*pHdc, color);
    return nOffset;
}

// FUNCTION: LOCO 0x4500a0
void TutorialWnd::RefreshListAndNavState() {
    if (nListScrollOffset > 0) {
        nPrevItemIndex = nSelectedItemIndex;
        nPrevScanCursor = nListScrollOffset - 1;
    } else {
        nPrevScanCursor = -1;
        nPrevItemIndex = nSelectedItemIndex - 1;
        while (!categoryRecords[nPrevItemIndex].bUsed) {
            nPrevItemIndex--;
            if (nPrevItemIndex <= 0) {
                nPrevItemIndex = -1;
                break;
            }
        }
    }

    HDC hDC = AcquireOffscreenSurfaceDC(hwndSelf);

    int nCount = 0;
    if (bCategoryFileLoaded == 0) {
        nCount = 1;
    } else {
        int nResult = DrawDescriptionChunks(0, &hDC);
        while (-1 < nResult) {
            if (nCount >= 200) break;
            nCount++;
            nResult = DrawDescriptionChunks(nCount, &hDC);
        }
    }

    CommitScreenUpdate(hwndSelf, hDC, 1);

    int nSelected = nSelectedItemIndex;
    if (nListScrollOffset < nCount - 1) {
        nNextItemIndex = nSelected;
        nNextScanCursor = nListScrollOffset + 1;
    } else {
        nNextScanCursor = -1;
        nNextItemIndex = nSelected + 1;
        while (!categoryRecords[nNextItemIndex].bUsed) {
            nNextItemIndex++;
            if (nNextItemIndex >= 200) {
                nNextItemIndex = -1;
                break;
            }
        }
    }

    if ((nPrevItemIndex != -1) && (lastNotifySubcode != 0) && (nPrevItemIndex != nSelected) &&
        (categoryRecords[nPrevItemIndex].bHashFlagMaybe == 1)) {
        nPrevItemIndex = -1;
    }
    if ((nNextItemIndex != -1) && (lastNotifySubcode != 0) && (nNextItemIndex != nSelected) &&
        (categoryRecords[nSelected].bHashFlagMaybe == 1)) {
        nNextItemIndex = -1;
    }
}

// FUNCTION: LOCO 0x450240
void TutorialWnd::Launch(int nIndex) {
    g_nScreenState = nGlobalStateMirror;
    if (g_bBoardScrollFlag == 1) {
        AppWindow_ApplyDisplayModeMaybe(1);
    }

    PlacementCursorMaybe_004854c8.SetCursorCapture(0, 0, 0);
    g_worldBoard.MarkRectDirty(g_worldBoard.rcViewport);
    g_worldBoard.UpdateDirtyTiles(0);
    g_nScreenState = 8;

    if (bCategoryFileLoaded == 0) {
        ResourceRefCategoryTable_LoadCategoryFile();
        bCategoryFileLoaded = 1;
    }

    nTextLineHeight = MeasureTextLineHeight();
    SelectCategory(nIndex);

    if (g_pDSoundManager != NULL) {
        g_pDSoundManager->DSound_SetTemporaryDuck(1);
    }

    if (bIconResourcesLoadedFlag != 1) {
        pErrObj1->Load();
        pErrObj2->Load();
        pErrObj3->Load();
        pErrObj4->Load();
        pErrObj9->Load();
        bIconResourcesLoadedFlag = 1;
    }

    this->RefreshClientRect();

    HWND hWnd = (HWND)hwndSelf;
    nListScrollOffset = 0;
    nScrollTimerId = SetTimer(hWnd, 0x54, 10, NULL);
    bPresenterActiveFlag = 0;
    nAnimTickCounter = 0;

    Move(g_dwScreenHalfWidth - 0x120, g_dwScreenHalfHeight - 0xb8);

    if (nPrevItemIndex == -1) {
        bErrObj1Loaded = 0;
    } else {
        bErrObj1Loaded = 1;
    }
    if (nNextItemIndex == -1) {
        bErrObj2Loaded = 0;
    } else {
        bErrObj2Loaded = 1;
    }
    if (bErrObj2Loaded != 0) {
        bErrObjsLoaded = 1;
    } else {
        bErrObjsLoaded = 0;
    }

    CursorDesc *pDesc = pErrObj4->pCursorDesc;
    nPresenterFrameIndex = pDesc->paFrameEntries[(short)pDesc->wActiveFrameSetIndex].nStartFrame;

    PopupWndBase::Show();

    switch (lastNotifySubcode) {
    case 1:
        ((WindowBase *)g_pMailWnd)->SetCaptureMode(1);
        break;
    case 2:
        g_pAlbumCardWnd->SetCaptureMode(1);
        break;
    case 3:
        g_pEditCardWnd->SetCaptureMode(1);
        break;
    }
}

// FUNCTION: LOCO 0x450d60
// vtable slot 0x18 override of PopupWndBase::RefreshClientRect. Runs the base's own client-rect
// refresh first, then re-centers the fixed 0x240 x 0x170 popup on the desktop and lays out all
// nine ResourceRef slots' rects.
//
// Every slot but pErrObj4 is positioned relative to a shared (0xe8, 0) content-pane origin, with
// a per-slot second OffsetRect on top; pErrObj4 is the 0xe8 x 0x130 left panel and offsets by
// (0, 0) instead -- a genuine no-op call the original really makes, kept verbatim. Slots
// 1/2/3/9 take their size from their realized CursorDesc's native size; 4/6/7/8 use literal
// sizes; 5 (the current item's icon) centers itself on (0x96, 0xb2) by backing off half its own
// size, and collapses to an empty rect when its resource isn't realized.
//
// The whole body is gated on bIconResourcesLoadedFlag -- before Launch has loaded the icon
// slots, every pCursorDesc deref below would fault.
void TutorialWnd::RefreshClientRect()
{
    RECT rectDesktop;
    RECT rectWindow;
    RECT rectIcon;
    RECT rect;

    if (!bIconResourcesLoadedFlag) {
        return;
    }

    PopupWndBase::RefreshClientRect();

    GetClientRect(GetDesktopWindow(), &rectDesktop);
    SetRect(&rectWindow, 0, 0, 0x240, 0x170);
    CenterRectInRect(&rectDesktop, &rectWindow);
    // 0x90 = SWP_NOACTIVATE | SWP_HIDEWINDOW: the HWND is only an input sink for this
    // DirectDraw-composited overlay, so the layout pass leaves it hidden and PopupWndBase::Show
    // is what actually reveals it.
    SetWindowPos(hwndSelf, (HWND)0, rectWindow.left, rectWindow.top,
                 rectWindow.right - rectWindow.left, rectWindow.bottom - rectWindow.top,
                 SWP_NOACTIVATE | SWP_HIDEWINDOW);

    SetRect(&rect, 0, 0, pErrObj1->pCursorDesc->nativeWidth, pErrObj1->pCursorDesc->nativeHeight);
    OffsetRect(&rect, 0xe8, 0);
    OffsetRect(&rect, 0x2a, 0xf4);
    pErrObj1->rect = rect;

    SetRect(&rect, 0, 0, pErrObj2->pCursorDesc->nativeWidth, pErrObj2->pCursorDesc->nativeHeight);
    OffsetRect(&rect, 0xe8, 0);
    OffsetRect(&rect, 0x57, 0xf4);
    pErrObj2->rect = rect;

    SetRect(&rect, 0, 0, pErrObj3->pCursorDesc->nativeWidth, pErrObj3->pCursorDesc->nativeHeight);
    OffsetRect(&rect, 0xe8, 0);
    OffsetRect(&rect, 0xde, 0xf4);
    pErrObj3->rect = rect;

    SetRect(&rect, 0, 0, 0xe8, 0x130);
    OffsetRect(&rect, 0, 0);
    pErrObj4->rect = rect;

    SetRect(&rect, 0, 0, pErrObj9->pCursorDesc->nativeWidth, pErrObj9->pCursorDesc->nativeHeight);
    OffsetRect(&rect, 0xe8, 0);
    pErrObj9->rect = rect;

    // EFFECTIVE MATCH -- 1051/1054 bytes, insns 351/352 (asmscore total 6998); the ONLY
    // disagreement in the whole function is right here: the original re-loads
    // pErrObj5->pCursorDesc after the guard (`mov eax,[eax+0x14]` at 0x450fc6) instead of
    // reusing the copy the guard already loaded, so it keeps pErrObj5 in EAX and burns ECX for
    // both guard temps, where cl CSEs the load for us and permutes to ECX/EAX/EDX. From the
    // SetRect onward the two are byte-identical. Probed and REFUTED (v398): negating the
    // condition and swapping the arms is fully inert (cl canonicalizes the short-circuit
    // chain); hoisting the descriptor into a then-block local is inert; and writing the two
    // empty arms out as a duplicated `else if` pair does NOT cross-jump the way a switch's
    // common tail does (1059 B, DIFF 449 -- strictly worse). Intrinsic CSE/regalloc coin flip.
    if (pErrObj5->pCursorDesc != (CursorDesc *)0 && pErrObj5->nRealizedHandle != 0) {
        SetRect(&rectIcon, 0, 0, pErrObj5->pCursorDesc->nativeWidth,
                pErrObj5->pCursorDesc->nativeHeight);
        OffsetRect(&rectIcon, 0xe8, 0);
        OffsetRect(&rectIcon, 0x96, 0xb2);
        OffsetRect(&rectIcon, -(pErrObj5->pCursorDesc->nativeWidth >> 1),
                   -(pErrObj5->pCursorDesc->nativeHeight >> 1));
        pErrObj5->rect = rectIcon;
    } else {
        SetRectEmpty(&rectIcon);
        pErrObj5->rect = rectIcon;
    }

    SetRect(&rect, 0, 0, 0xd9, 0x96);
    OffsetRect(&rect, 0xe8, 0);
    OffsetRect(&rect, 0x2a, 0x23);
    pErrObj6->rect = rect;

    SetRect(&rect, 0, 0, 0xd9, 0x14);
    OffsetRect(&rect, 0xe8, 0);
    OffsetRect(&rect, 0x2a, 0xa);
    pErrObj7->rect = rect;

    SetRect(&rect, 0, 0, 0xd9, 0x14);
    OffsetRect(&rect, 0xe8, 0);
    OffsetRect(&rect, 0x2a, 0x23);
    OffsetRect(&rect, 0, 0x69);
    pErrObj8->rect = rect;
}

// FUNCTION: LOCO 0x451880
// vtable slot 0x2c -- the WM_TIMER passthrough. Three guards, all of which must hold before the
// presenter tick runs: the presenter must actually be playing, the message must be for this
// window (not one forwarded via the owner, the way PopupWndBase_RouteMessage's WM_MOUSEMOVE arm
// can be), and the timer must be this window's own 0x54 scroll timer -- NOT PopupWndBase::Show's
// 0x43 cursor-animation timer, which RouteMessage handles itself and never routes here.
LRESULT TutorialWnd::OnTimerDefault(HWND hwndMsg, UINT /*msg*/, WPARAM wParam, LPARAM /*lParam*/)
{
    if (bPresenterActiveFlag && hwndMsg == hwndSelf && wParam == 0x54) {
        AdvancePresenterFrame();
    }
    return 0;
}

// FUNCTION: LOCO 0x4518b0
// vtable slot 0x28 -- RouteMessage's default arm. The only message this class does anything with
// here is WM_SYSCOMMAND/SC_SCREENSAVE (the wParam & 0xfff0 mask is the documented Win32 idiom --
// the low 4 bits carry the OS's own accelerator/mnemonic state): the screensaver kicking in
// tears the tutorial view down and returns the app to the screen that launched it. Handled or
// not, the message still goes on to DefWindowProcA -- so the screensaver is NOT suppressed, only
// reacted to.
LRESULT TutorialWnd::OnUnhandledMessage(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_SYSCOMMAND && (wParam & 0xfff0) == SC_SCREENSAVE) {
        OnExit();
        // The original's `dec eax; je` ladder over lastNotifySubcode -- a switch, not an
        // if/else-if chain (the v396 OnExit lesson: the ladder IS the switch shape).
        //
        // The two calls are REPEATED IN EVERY ARM rather than hoisted below the switch with a
        // local `int nNewState`. Both spellings give 43/43 instructions, but the hoisted local
        // costs 9 bytes and misses: it materializes the value into a register per arm and does
        // one `push esi`, whereas the original pushes the constant inside each arm
        // (`push 5`/`push 6`/`push 7`/`push eax`) and CROSS-JUMPS the identical three-
        // instruction tail (`call 0x408130; add esp,4; call 0x463670`) into one shared copy at
        // 0x4518fa. Writing the duplication out is what lets VC5 tail-merge it back; the
        // hoisted-local form gives it nothing to merge. EXACT with this shape.
        switch (lastNotifySubcode) {
        case 1:
            AppWindow_SetScreenState(5);
            FUN_00463670_LotsOfShowWindow();
            break;
        case 2:
            AppWindow_SetScreenState(6);
            FUN_00463670_LotsOfShowWindow();
            break;
        case 3:
            AppWindow_SetScreenState(7);
            FUN_00463670_LotsOfShowWindow();
            break;
        default:
            AppWindow_SetScreenState(nGlobalStateMirror);
            FUN_00463670_LotsOfShowWindow();
            break;
        }
    }
    return DefWindowProcA(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x451520
// vtable slot 0x3c -- WM_RBUTTONDOWN is simply WM_LBUTTONDOWN here. The forward goes THROUGH the
// vtable in the original (`mov eax,[ecx]; call [eax+0x34]`), which is exactly what an unqualified
// call to a virtual member compiles to; do not "optimize" it to TutorialWnd::OnLButtonDown.
LRESULT TutorialWnd::OnRButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return OnLButtonDown(hwndMsg, msg, wParam, lParam);
}

// vtable slot 0x50 -- WM_KEYDOWN. A bare `return 0`: this window swallows key-downs instead of
// passing them to DefWindowProcStub (the note in src/TutorialWnd.h). UNMARKED -- one instruction
// is small enough to ICF-fold onto 0x426950, whose marker is on WindowBase::OnMouseActivate.
LRESULT TutorialWnd::OnKeyDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return 0;
}

// g_pApp reached without pulling AppWindow.h into the whole TU, the same shape five sibling TUs
// carry alongside their own copy of the IsNetShuttingDownMaybe byte-returning predicate.
class AppWindow;
extern AppWindow *g_pApp; // DAT_004aa4a0
inline unsigned char IsNetShuttingDownMaybe() { return g_nScreenState == 10; }

// vtable slot 0x7c (WM_CLOSE) -- byte-for-byte CreditsWnd::OnClose, and ICF-folded onto it at
// 0x40f760, where the marker lives (src/CreditsWnd.cpp). While the app is alive and not already
// tearing down the window swallows its own close; only once shutdown is underway does the base
// PopupWndBase::OnClose actually destroy it. UNMARKED -- one address, one marker.
LRESULT TutorialWnd::OnClose(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_pApp != NULL && !IsNetShuttingDownMaybe()) {
        return 0;
    }
    return PopupWndBase::OnClose(hwndMsg, msg, wParam, lParam);
}

// FUNCTION: LOCO 0x4517b0
// vtable slot 0x4c -- WM_MOUSEMOVE. Delegates to the base handler for the actual software-cursor
// redraw (base-qualified: the original's `mov ecx,esi; call 0x414a80` is a DIRECT call, not a
// slot dispatch), then decides which of the popup's two preloaded cursors should be showing.
//
// sic: the four switch arms FALL THROUGH. A pointer over nav button 1 whose own enabled byte is
// clear does not fall back to the resting cursor -- it goes on to test button 2's enabled byte,
// then button 3's, then the shared bErrObjsLoaded, and shows the hover cursor if any of those
// happens to be set. Reproduced verbatim; see docs/engine-bugs.md.
LRESULT TutorialWnd::OnMouseMove(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!bPresenterActiveFlag) {
        return 0;
    }

    PopupWndBase::OnMouseMove(hwndMsg, msg, wParam, lParam);

    switch (HitTestControl((unsigned int)lParam & 0xffff, (unsigned int)lParam >> 16)) {
    case 1:
        if (bErrObj1Loaded == 1) {
            SetCursorDesc(cursorHover.nMaskSurfaceKey, cursorHover.pDesc, 0, 1);
            return 0;
        }
        // sic: falls through
    case 2:
        if (bErrObj2Loaded == 1) {
            SetCursorDesc(cursorHover.nMaskSurfaceKey, cursorHover.pDesc, 0, 1);
            return 0;
        }
        // sic: falls through
    case 3:
        if (bErrObj3Loaded == 1) {
            SetCursorDesc(cursorHover.nMaskSurfaceKey, cursorHover.pDesc, 0, 1);
            return 0;
        }
        // sic: falls through
    case 4:
        if (bErrObjsLoaded == 1) {
            SetCursorDesc(cursorHover.nMaskSurfaceKey, cursorHover.pDesc, 0, 1);
            return 0;
        }
        break;
    }

    SetCursorDesc(cursorNormal.nMaskSurfaceKey, cursorNormal.pDesc, 0, 1);
    return 0;
}

// FUNCTION: LOCO 0x451e90
// Maps a client-space point to a control code. The probe order is fixed and does NOT follow the
// pErrObj slot numbering -- pErrObj5 is tested before pErrObj4, and the codes they return (7 and
// 6) don't line up with the slot numbers either. Codes 1/2/3 additionally require the button's
// own enabled byte; the rest are unconditional. 0 = nothing hit.
//
// The eight PtInRect calls share one register-cached copy of the import thunk in the original
// (`mov ebp, ds:0x47734c` up front, then `call ebp` eight times) -- that hoist is cl's own doing,
// not a source construct.
int TutorialWnd::HitTestControl(LONG x, LONG y)
{
    // The two assignments are in y-then-x order deliberately, and it is load-bearing: it
    // decides which parameter the allocator parks in EBX and which in EDI (the original loads
    // x into EBX from the LOWER stack slot first). `pt.x = x; pt.y = y;` gives a perfectly
    // aligned 126/126 that still misses by 18 bytes -- the two registers are simply swapped
    // throughout. This is NOT contradicted by PopupWndBase::RedrawSoftwareCursor's note that
    // reordering ITS rect-field assignments is inert: that RECT has its address taken and is
    // really materialized on the stack, so cl reschedules the stores freely. This POINT is
    // never materialized -- it is passed by value straight from registers -- so the assignment
    // order IS the register assignment.
    POINT pt;
    pt.y = y;
    pt.x = x;

    if (bErrObj1Loaded && PtInRect(&pErrObj1->rect, pt)) {
        return 1;
    }
    if (bErrObj2Loaded && PtInRect(&pErrObj2->rect, pt)) {
        return 2;
    }
    if (PtInRect(&pErrObj3->rect, pt)) {
        return 3;
    }
    if (PtInRect(&pErrObj5->rect, pt)) {
        return 7;
    }
    if (PtInRect(&pErrObj4->rect, pt)) {
        return 6;
    }
    if (PtInRect(&pErrObj6->rect, pt)) {
        return 4;
    }
    if (PtInRect(&pErrObj7->rect, pt)) {
        return 5;
    }
    return PtInRect(&pErrObj9->rect, pt) ? 8 : 0;
}

// FUNCTION: LOCO 0x451540
// vtable slot 0x34 -- WM_LBUTTONDOWN. While the presenter is idle the click is handed straight
// back to the base default. Otherwise HitTestControl decides: codes 1/2/3 are the three nav
// buttons and all three run the same press-flash (draw pressed, commit, Sleep(150), draw
// resting) before their real action; code 7 is the current item's own icon, which doubles as a
// close button for exactly two icon resources; codes 4/5/6/8 do nothing.
//
// The close path -- OnExit followed by a transition back to whichever screen launched the view
// -- is written out THREE times (once for code 3, once per arm of code 7) rather than factored
// into a helper. That is what the original does: there is no call, and cl cross-jumps the three
// copies' 5/6/7 arms into one shared block each while leaving their nGlobalStateMirror default
// arms separate (they land in different registers). Same lever as OnUnhandledMessage above.
LRESULT TutorialWnd::OnLButtonDown(HWND hwndMsg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!bPresenterActiveFlag) {
        return PopupWndBase::OnUnhandledMessage(hwndMsg, msg, wParam, lParam);
    }

    switch (HitTestControl((unsigned int)lParam & 0xffff, (unsigned int)lParam >> 16)) {
    case 1:
        RedrawControlPressed(1);
        CommitScreenUpdate(hwndSelf, NULL, 0);
        Sleep(0x96);
        RedrawControl(1);
        GoToPrevPage();
        CommitScreenUpdate(hwndSelf, NULL, 0);
        if (g_pDSoundManager != NULL) {
            g_UIResources.PlayUiSound(0x5015);
        }
        break;

    case 2:
        RedrawControlPressed(2);
        CommitScreenUpdate(hwndSelf, NULL, 0);
        Sleep(0x96);
        RedrawControl(2);
        if (g_pDSoundManager != NULL) {
            g_UIResources.PlayUiSound(0x5015);
        }
        GoToNextPage();
        CommitScreenUpdate(hwndSelf, NULL, 0);
        break;

    case 3:
        RedrawControlPressed(3);
        CommitScreenUpdate(hwndSelf, NULL, 0);
        Sleep(0x96);
        RedrawControl(3);
        if (g_pDSoundManager != NULL) {
            g_UIResources.PlayUiSound(0x5015);
        }
        CommitScreenUpdate(hwndSelf, NULL, 0);
        OnExit();
        switch (lastNotifySubcode) {
        case 1:
            AppWindow_SetScreenState(5);
            break;
        case 2:
            AppWindow_SetScreenState(6);
            break;
        case 3:
            AppWindow_SetScreenState(7);
            break;
        default:
            AppWindow_SetScreenState(nGlobalStateMirror);
            break;
        }
        break;

    case 7:
        switch (pErrObj5->pCursorDesc->resourceId) {
        case 0x3d01:
            OnExit();
            switch (lastNotifySubcode) {
            case 1:
                AppWindow_SetScreenState(5);
                break;
            case 2:
                AppWindow_SetScreenState(6);
                break;
            case 3:
                AppWindow_SetScreenState(7);
                break;
            default:
                AppWindow_SetScreenState(nGlobalStateMirror);
                break;
            }
            break;
        case 0x3d06:
            OnExit();
            switch (lastNotifySubcode) {
            case 1:
                AppWindow_SetScreenState(5);
                break;
            case 2:
                AppWindow_SetScreenState(6);
                break;
            case 3:
                AppWindow_SetScreenState(7);
                break;
            default:
                AppWindow_SetScreenState(nGlobalStateMirror);
                break;
            }
            break;
        }
        break;

    // The original's jump table is NINE entries wide (`cmp eax,8; ja` + 9 dwords). With cases
    // 1/2/3/7 alone cl emits a SEVEN-entry table and `cmp eax,6` -- a 1-byte code miss plus 8
    // bytes of missing table. So the source's highest case label is 9, even though
    // HitTestControl tops out at 8.
    //
    // ⭐ And the body spelling decides whether the label survives: `case 8: case 9: break;` is
    // FOLDED AWAY (its target is already the switch's end label, which is also the default
    // target, so cl prunes the trailing table entries and the table stays seven wide -- the
    // recompile was byte-for-byte unchanged). Writing `return 0;` instead keeps both labels and
    // widens the table to nine, and the two blocks then cross-jump into the same shared
    // epilogue anyway -- identical machine code, different table. When a jump table is too
    // NARROW, look at how the do-nothing arms are spelled before concluding the case labels are
    // missing.
    //
    // Codes 4/5/6 stay indistinguishable from `default` either way (their slots point at the
    // shared `return 0` regardless), so whether the author listed them too is unrecoverable.
    case 8:
        return 0;
    case 9:
        return 0;
    }

    return 0;
}

// FUNCTION: LOCO 0x451920
// The prev-page nav action (hit code 1). Two distinct cases, keyed off whether the previous
// "page" lives inside the CURRENT item or in the previous one:
//   * nPrevItemIndex == nSelectedItemIndex -- still on the same item, so just wind the scroll
//     offset back to nPrevScanCursor (which RefreshListAndNavState set to offset-1) and
//     re-select in place.
//   * otherwise -- move to the previous item, then measure how many word-wrapped chunks its
//     description needs (same probe loop as RefreshListAndNavState: call DrawDescriptionChunks
//     with 1, 2, 3, ... until it returns negative) and land on that item's LAST page.
// Either way the narration channel is swapped to the newly selected record's sound and the nav
// buttons are returned to their resting state.
//
// The 9-call nav-button reset + timer restart is written out THREE times, and the narration
// swap TWICE, because that is what the original really contains: 27 calls to RedrawControl are
// present in the binary at 0x451920..0x451c5f, not 9 with shared tails. Per CODEGEN #18i, cl
// only cross-jumps a common tail when the source itself duplicates it, so factoring either
// block into a helper here would change the codegen rather than preserve it.
void TutorialWnd::GoToPrevPage()
{
    int nIndex = nPrevItemIndex;
    if (nIndex == -1) {
        return;
    }

    if (nIndex == nSelectedItemIndex) {
        nListScrollOffset = nPrevScanCursor;
        SelectCategory(nSelectedItemIndex);

        if (g_pDSoundManager != NULL) {
            UINT nSoundId = categoryRecords[nSelectedItemIndex].dwNarrationSoundId;
            if (pDSoundChannel != NULL) {
                if (pDSoundChannel->nSoundId != 0) {
                    SoundBankEntry *pOld =
                        g_UIResources.SoundBank_LookupEntryById(pDSoundChannel->nSoundId);
                    pOld->Release();
                }
                pDSoundChannel->Release();
            }
            if (nSoundId != 0) {
                SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(nSoundId);
                pEntry->EnsureLoaded();
                g_pDSoundManager->PlaySoundByIdWithHandle(nSoundId, &pDSoundChannel);
            }
        }

        if (bPresenterActiveFlag) {
            RedrawControl(6);
            RedrawControl(8);
            RedrawControl(1);
            RedrawControl(2);
            RedrawControl(3);
            RedrawControl(7);
            RedrawControl(4);
            RedrawControl(9);
            RedrawControl(5);
            KillTimer(hwndSelf, nScrollTimerId);
            nScrollTimerId = SetTimer(hwndSelf, 0x54, 10, NULL);
        }
    } else {
        SelectCategory(nIndex);

        HDC hDC = AcquireOffscreenSurfaceDC(hwndSelf);

        int nCount = 0;
        if (bCategoryFileLoaded == 0) {
            nCount = 1;
        } else {
            int nResult = DrawDescriptionChunks(0, &hDC);
            while (-1 < nResult) {
                if (nCount >= 200) break;
                nCount++;
                nResult = DrawDescriptionChunks(nCount, &hDC);
            }
        }

        CommitScreenUpdate(hwndSelf, hDC, 1);

        if (nCount > 1) {
            nListScrollOffset = nCount - 1;
            SelectCategory(nSelectedItemIndex);
            if (bPresenterActiveFlag) {
                RedrawControl(6);
                RedrawControl(8);
                RedrawControl(1);
                RedrawControl(2);
                RedrawControl(3);
                RedrawControl(7);
                RedrawControl(4);
                RedrawControl(9);
                RedrawControl(5);
                KillTimer(hwndSelf, nScrollTimerId);
                nScrollTimerId = SetTimer(hwndSelf, 0x54, 10, NULL);
            }
        }

        if (g_pDSoundManager != NULL) {
            UINT nSoundId = categoryRecords[nSelectedItemIndex].dwNarrationSoundId;
            if (pDSoundChannel != NULL) {
                if (pDSoundChannel->nSoundId != 0) {
                    SoundBankEntry *pOld =
                        g_UIResources.SoundBank_LookupEntryById(pDSoundChannel->nSoundId);
                    pOld->Release();
                }
                pDSoundChannel->Release();
            }
            if (nSoundId != 0) {
                SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(nSoundId);
                pEntry->EnsureLoaded();
                g_pDSoundManager->PlaySoundByIdWithHandle(nSoundId, &pDSoundChannel);
            }
        }

        if (bPresenterActiveFlag) {
            RedrawControl(6);
            RedrawControl(8);
            RedrawControl(1);
            RedrawControl(2);
            RedrawControl(3);
            RedrawControl(7);
            RedrawControl(4);
            RedrawControl(9);
            RedrawControl(5);
            KillTimer(hwndSelf, nScrollTimerId);
            nScrollTimerId = SetTimer(hwndSelf, 0x54, 10, NULL);
        }
    }
}

// FUNCTION: LOCO 0x451c60
// The next-page nav action (hit code 2) -- GoToPrevPage's mirror, and simpler, because moving
// FORWARD always lands on a page 0 and so never needs that function's chunk-count probe:
//   * nNextItemIndex == nSelectedItemIndex -- still on the same item, so wind the scroll offset
//     forward to nNextScanCursor (RefreshListAndNavState set it to offset+1);
//   * otherwise -- move to the next item and reset the scroll offset to its first page.
// Both arms then run the same narration swap and nav-button reset, written out twice for the
// same reason as GoToPrevPage's three copies (see its comment).
void TutorialWnd::GoToNextPage()
{
    int nIndex = nNextItemIndex;
    if (nIndex == -1) {
        return;
    }

    if (nIndex == nSelectedItemIndex) {
        nListScrollOffset = nNextScanCursor;
        SelectCategory(nSelectedItemIndex);

        if (g_pDSoundManager != NULL) {
            UINT nSoundId = categoryRecords[nSelectedItemIndex].dwNarrationSoundId;
            if (pDSoundChannel != NULL) {
                if (pDSoundChannel->nSoundId != 0) {
                    SoundBankEntry *pOld =
                        g_UIResources.SoundBank_LookupEntryById(pDSoundChannel->nSoundId);
                    pOld->Release();
                }
                pDSoundChannel->Release();
            }
            if (nSoundId != 0) {
                SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(nSoundId);
                pEntry->EnsureLoaded();
                g_pDSoundManager->PlaySoundByIdWithHandle(nSoundId, &pDSoundChannel);
            }
        }

        if (bPresenterActiveFlag) {
            RedrawControl(6);
            RedrawControl(8);
            RedrawControl(1);
            RedrawControl(2);
            RedrawControl(3);
            RedrawControl(7);
            RedrawControl(4);
            RedrawControl(9);
            RedrawControl(5);
            KillTimer(hwndSelf, nScrollTimerId);
            nScrollTimerId = SetTimer(hwndSelf, 0x54, 10, NULL);
        }
    } else {
        nListScrollOffset = 0;
        SelectCategory(nIndex);

        if (g_pDSoundManager != NULL) {
            UINT nSoundId = categoryRecords[nSelectedItemIndex].dwNarrationSoundId;
            if (pDSoundChannel != NULL) {
                if (pDSoundChannel->nSoundId != 0) {
                    SoundBankEntry *pOld =
                        g_UIResources.SoundBank_LookupEntryById(pDSoundChannel->nSoundId);
                    pOld->Release();
                }
                pDSoundChannel->Release();
            }
            if (nSoundId != 0) {
                SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(nSoundId);
                pEntry->EnsureLoaded();
                g_pDSoundManager->PlaySoundByIdWithHandle(nSoundId, &pDSoundChannel);
            }
        }

        if (bPresenterActiveFlag) {
            RedrawControl(6);
            RedrawControl(8);
            RedrawControl(1);
            RedrawControl(2);
            RedrawControl(3);
            RedrawControl(7);
            RedrawControl(4);
            RedrawControl(9);
            RedrawControl(5);
            KillTimer(hwndSelf, nScrollTimerId);
            nScrollTimerId = SetTimer(hwndSelf, 0x54, 10, NULL);
        }
    }
}

// FUNCTION: LOCO 0x450450
// The presenter animation tick, driven off the 0x54 scroll timer by OnTimerDefault (10 ms).
// Runs on EVERY other tick -- nAnimTickCounter free-runs 0..99 and only even values do work, so
// the presenter advances at 50 Hz while the timer fires at 100 Hz.
//
// The frame index it drives is nPresenterFrameIndex, read back by the case-6 arm of the
// nav-button redraw helper. Which frame comes next depends on whether the narration channel is
// still playing: while it is, walk pErrObj4's current animation set (paFrameEntries indexed by
// wActiveFrameSetIndex) one frame at a time and wrap at nEndFrame back to nStartFrame; once the
// channel has gone reclaimable, snap to the fixed "idle" set's first frame (entry [1]) instead
// -- and if it is already sitting there, do nothing at all, which is what keeps a finished
// narration from repainting the presenter 50 times a second.
void TutorialWnd::AdvancePresenterFrame()
{
    if (pDSoundChannel == NULL) {
        return;
    }

    // `char` + an explicit `== 1` test below, not `bool`: the original ends on `cmp bl, 1`,
    // where a bool gives `test bl, bl`. That one instruction was this function's entire
    // residual (67/67 insns either way, DIFF 19 -> MATCH).
    char bRedraw = 0;

    nAnimTickCounter++;
    if (nAnimTickCounter == 100) {
        nAnimTickCounter = 0;
    }

    if (nAnimTickCounter % 2 == 0) {
        if (!pDSoundChannel->IsReclaimable()) {
            CursorDesc *pDesc = pErrObj4->pCursorDesc;
            CursorAnimFrameEntry *pEntry =
                &pDesc->paFrameEntries[(short)pDesc->wActiveFrameSetIndex];
            if ((int)nPresenterFrameIndex < pEntry->nEndFrame) {
                nPresenterFrameIndex++;
            } else {
                nPresenterFrameIndex = pEntry->nStartFrame;
            }
            RedrawControl(6);
            bRedraw = 1;
        } else {
            // The redraw pair below is deliberately a second copy of the one above rather than
            // being hoisted past the `if`: the original has ONE copy reached by a jump from
            // here, which per CODEGEN #18i is cl cross-jumping a tail the SOURCE duplicated.
            // Hoisting it out would have to run on the already-idle path too.
            unsigned int nIdleFrame = pErrObj4->pCursorDesc->paFrameEntries[1].nStartFrame;
            if (nPresenterFrameIndex != nIdleFrame) {
                nPresenterFrameIndex = nIdleFrame;
                RedrawControl(6);
                bRedraw = 1;
            }
        }
    }

    if (bRedraw == 1) {
        CommitScreenUpdate(hwndSelf, 0, 0);
    }
}

// FUNCTION: LOCO 0x451fb0
// Repaints one control. `nControl` is a HitTestControl code, not a pErrObj slot number: 1/2/3
// are the three nav buttons, 7 the current item's icon, 8 the close button, and 4/5/6/9 the
// four composite areas that own their own draw helper. Both press-flash sites and all five
// page-nav sites drive this; only the presenter tick uses case 6.
//
// The three nav buttons draw frame 0 when their own enabled byte is set and frame 2 (the
// greyed-out variant) when it is not -- except button 3, whose disabled state draws NOTHING at
// all rather than a greyed frame. That asymmetry is the original's.
//
// Case order below is the original's `.text` block order (9 first, then 1..8), which per
// CODEGEN is the SOURCE case order for a jump-table switch. The bare `case 0:` with no
// `default:` label is what keeps the table anchored at 0 -- the original dispatches on a plain
// `cmp eax,9; ja; jmp [eax*4+tbl]` with a 10-entry table whose slot 0 points at the shared
// epilogue, so case 0 is a live label even though it emits no code.
void TutorialWnd::RedrawControl(int nControl)
{
    switch (nControl) {
    case 0:
        break;
    case 9:
        if (bErrObjsLoaded == 1) {
            HDC hDC = AcquireOffscreenSurfaceDC(hwndSelf);
            DrawEllipsis(&hDC);
            CommitScreenUpdate(hwndSelf, hDC, 1);
        }
        break;
    case 1:
        if (bErrObj1Loaded == 1) {
            pErrObj1->DrawFrame(0, pOffscreenSurface);
        } else {
            pErrObj1->DrawFrame(2, pOffscreenSurface);
        }
        break;
    case 2:
        if (bErrObj2Loaded == 1) {
            pErrObj2->DrawFrame(0, pOffscreenSurface);
        } else {
            pErrObj2->DrawFrame(2, pOffscreenSurface);
        }
        break;
    case 3:
        if (bErrObj3Loaded == 1) {
            pErrObj3->DrawFrame(0, pOffscreenSurface);
        }
        break;
    case 4: {
        // sic: no CommitScreenUpdate, unlike cases 5 and 9 right below.
        HDC hDC = AcquireOffscreenSurfaceDC(hwndSelf);
        DrawDescriptionPage(&hDC);
        break;
    }
    case 5: {
        HDC hDC = AcquireOffscreenSurfaceDC(hwndSelf);
        DrawItemTitle(&hDC);
        CommitScreenUpdate(hwndSelf, hDC, 1);
        break;
    }
    case 6:
        DrawPresenterFrame(nPresenterFrameIndex);
        break;
    case 7:
        if (pErrObj5->pCursorDesc != NULL && pErrObj5->nRealizedHandle != 0) {
            pErrObj5->DrawFrame(0, pOffscreenSurface);
            CursorAnimFrameEntry *pEntry = pErrObj5->pCursorDesc->paFrameEntries;
            if (pEntry != NULL && pEntry->bDoubleSpeedFlag == 1) {
                pErrObj5->DrawFrame(1, pOffscreenSurface);
            }
        }
        break;
    case 8:
        pErrObj9->DrawFrame(0, pOffscreenSurface);
        break;
    }
}

// FUNCTION: LOCO 0x4527b0
// The PRESSED half of the press-flash pair with RedrawControl above: OnLButtonDown calls this,
// sleeps ~150 ms, then calls RedrawControl with the same code to put the control back. Same
// control-code space, same jump table shape (10 entries anchored at 0, block order 9/1/2/3/
// 6/7/8), and the same 9 arm verbatim.
//
// Only the three nav buttons actually have a distinct pressed look -- they draw frame 1 instead
// of frame 0, and unlike RedrawControl there is no greyed-out `else` arm at all, since a
// disabled button never gets pressed in the first place. Codes 6/7/8 just redraw normally, and
// 4/5 are no-ops here (they are composite areas with no pressed state), which is why they join
// case 0 in the codegen-free group.
void TutorialWnd::RedrawControlPressed(int nControl)
{
    switch (nControl) {
    case 0:
    case 4:
    case 5:
        break;
    case 9:
        if (bErrObjsLoaded == 1) {
            HDC hDC = AcquireOffscreenSurfaceDC(hwndSelf);
            DrawEllipsis(&hDC);
            CommitScreenUpdate(hwndSelf, hDC, 1);
        }
        break;
    case 1:
        if (bErrObj1Loaded == 1) {
            pErrObj1->DrawFrame(1, pOffscreenSurface);
        }
        break;
    case 2:
        if (bErrObj2Loaded == 1) {
            pErrObj2->DrawFrame(1, pOffscreenSurface);
        }
        break;
    case 3:
        if (bErrObj3Loaded == 1) {
            pErrObj3->DrawFrame(1, pOffscreenSurface);
        }
        break;
    case 6:
        pErrObj4->DrawFrame(0, pOffscreenSurface);
        break;
    case 7:
        if (pErrObj5->pCursorDesc != NULL && pErrObj5->nRealizedHandle != 0) {
            pErrObj5->DrawFrame(0, pOffscreenSurface);
        }
        break;
    case 8:
        pErrObj9->DrawFrame(0, pOffscreenSurface);
        break;
    }
}

// FUNCTION: LOCO 0x4526b0
// RedrawControl/RedrawControlPressed's case-9 delegate: draws a literal "..." into pErrObj8's
// own rect -- the "there is more description text than fits" indicator, right-aligned and
// word-broken like the description text itself. Takes the caller's already-acquired offscreen
// DC by pointer, as all three of the case-4/5/9 delegates do.
void TutorialWnd::DrawEllipsis(HDC *pHdc)
{
    COLORREF oldColor = SetTextColor(*pHdc, 0xff5c00);
    int oldBkMode = SetBkMode(*pHdc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(*pHdc, g_UIResources.m_hFont12);

    char szText[0x200] = "";
    strcpy(szText, "...");

    RECT rect;
    CopyRect(&rect, &pErrObj8->rect);
    int nLen = strlen(szText);
    DrawTextA(*pHdc, szText, nLen, &rect, DT_RIGHT | DT_WORDBREAK | DT_NOPREFIX);

    SelectObject(*pHdc, oldFont);
    SetBkMode(*pHdc, oldBkMode);
    SetTextColor(*pHdc, oldColor);
}

// FUNCTION: LOCO 0x452b00
// Re-captures the 0xe8 x 0x130 presenter box from the SHARED work surface into this window's
// own offscreen surface, so the next presenter frame composites over a clean backdrop rather
// than over the previous frame. Sole caller is DrawPresenterFrame below.
//
// The shared work surface stays Locked between calls (g_worldBoard.bSurfaceLockGuard), and a
// Blt needs it unlocked -- hence the drop-the-lock/blit/retake-the-lock sandwich, the same one
// LocoBitmap::RestoreOverlapBlt and PopupWndBase use. The retake is conditional on the lock
// having been held on ENTRY, so a caller that already unlocked it stays unlocked.
// ⚠ REGRESSED, DELIBERATELY -- the twin of the note on
// AnimDescRefObj0x477488::AdvanceAnimFrameMaybe in src/WidgetBase.cpp. EXACT (249 bytes) as of
// 3a52a30, now DIFF(32), and for the same reason: the three method declarations added to
// src/UIResources.h so that UIResources::Init resolves to its real body instead of a generated
// stub. This function is unchanged. See docs/PARKED.md.
void TutorialWnd::RestorePresenterBackdrop()
{
    RECT rectDest;
    SetRect(&rectDest, 0, 0, 0xe8, 0x130);
    RECT rectWindow;
    GetWindowRect(hwndSelf, &rectWindow);
    rectWindow.right = rectWindow.left + rectDest.right - rectDest.left;
    rectWindow.bottom = rectWindow.top + rectDest.bottom - rectDest.top;

    // ⚠ THIS FUNCTION IS EXACT AS WRITTEN, AND THE LOCAL DECLARATION ORDER ABOVE IS LOAD-BEARING
    // -- do not "tidy" it, and in particular do not move `RECT rectWindow;` up next to
    // `RECT rectDest;`. It is EXACT only in the repo's CURRENT header configuration.
    //
    // This is the shared victim of the whole v442-v445 in-class-dtor cluster: five different
    // header levers each knock it MATCH -> DIFF(32) at unchanged length, independently and
    // identically. v449 ran the repo-wide sweep those five rows deferred to and it lost 1008 B;
    // see docs/PARKED.md's `v449 -- the in-class-dtor / shared-victim cluster sweep`.
    //
    // Recorded there and repeated here because it is the one real lever anyone found: IN THE
    // LEVER-ON CONFIGURATION, swapping the two sibling RECT locals to `RECT rectWindow; RECT
    // rectDest;` takes the residual DIFF(32) -> DIFF(4), at which point align/reg_pen/
    // identity_miss are all 0 and insns are 83/83 -- schedule, registers and both RECTs' stack
    // slots agree exactly, and the whole remainder is 4 displacement bytes encoding one binary
    // choice (the original loads the rectDest fields into eax and seeds each accumulator from
    // the rectWindow field; cl does the reverse). ⚠ But that same swap scores DIFF(30) HERE,
    // with the levers off -- i.e. the declaration order is compensating for TU-level codegen
    // state rather than recovering a truer source order, which is why the checked-in order
    // stays. Expression-shape probes REFUTED in the lever-on config (do not re-run): two `int`
    // temporaries with the stores sunk (32); this guard read interleaved between the two
    // assignments (38); swapped addends (30); the parenthesized `left + (right - left)` width
    // form (4, folded to the same code as the plain form); `right - left + left` association
    // (30); accumulators seeded from rectWindow then `+=` (29); `.bottom` assigned before
    // `.right` (8).
    unsigned char bWasLockGuarded = g_worldBoard.bSurfaceLockGuard;
    if (g_worldBoard.bSurfaceLockGuard != 0 && g_pDDrawWorkSurface->Unlock(NULL) == 0) {
        g_worldBoard.bSurfaceLockGuard = 0;
    }

    if (pOffscreenSurface->Blt(&rectDest, g_pDDrawWorkSurface, &rectWindow, 0x1000000, NULL) != 0) {
        OutputDebugStringA("Error drawing tw bitmap");
    }

    if (bWasLockGuarded != 0 && g_worldBoard.bSurfaceLockGuard == 0) {
        memset(g_worldBoard.aSurfaceDescScratch, 0, sizeof(g_worldBoard.aSurfaceDescScratch));
        g_worldBoard.aSurfaceDescScratch[0] = 0x7c; // sic: see DDSurfaceDescPadded0x7c comment
        if (g_pDDrawWorkSurface->Lock(NULL, (LPDDSURFACEDESC)g_worldBoard.aSurfaceDescScratch,
                                            0, NULL) == 0) {
            g_worldBoard.bSurfaceLockGuard = 1;
        }
    }
}

// FUNCTION: LOCO 0x452c00
// RedrawControl's case-6 delegate: paints presenter frame nFrameIndex of pErrObj4's realized
// bitmap strip into the offscreen surface, over a freshly restored backdrop. Frame selection is
// a horizontal offset into the strip by nFrameIndex * the resource's own native width, which is
// why frame 0 needs no OffsetRect at all.
//
// Gated on bPresenterActiveFlag, so the whole thing is a no-op until OnDrawContent has started
// the narration -- that gate is what stops the 50 Hz animation tick from painting anything
// before the view is really up.
// EFFECTIVE MATCH (byte_diff 34/203, insns 68/68). Pure zero-register residency: cl parks 0 in
// ebx up front (`xor ebx,ebx`) and spends it on all six zero uses -- the flag test, the two
// srcRect zero stores, the nFrameIndex test and the two `push 0` arguments -- while the
// original materializes each one as an immediate (`test al,al`, `mov [..],0`, `push 0`). Per
// CODEGEN that is a residency tie-break with no source lever; probed and REFUTED here anyway:
// aggregate-initializing srcRect vs. four field stores (byte-identical) and an early-return
// guard vs. a wrapping `if` (byte-identical). Everything else in the body agrees exactly.
void TutorialWnd::DrawPresenterFrame(unsigned int nFrameIndex)
{
    if (!bPresenterActiveFlag) {
        return;
    }

    RestorePresenterBackdrop();

    RECT srcRect = {0, 0, pErrObj4->rect.right - pErrObj4->rect.left,
                          pErrObj4->rect.bottom - pErrObj4->rect.top};
    if (nFrameIndex != 0) {
        OffsetRect(&srcRect, pErrObj4->pCursorDesc->nativeWidth * nFrameIndex, 0);
    }

    if (!((LocoBitmap *)pErrObj4->nRealizedHandle)
             ->RestoreOverlapBlt(pErrObj4->rect, pOffscreenSurface, srcRect, 0)) {
        OutputDebugStringA("Error drawing tw bitmap");
    }
}

// FUNCTION: LOCO 0x452570
// RedrawControl's case-5 delegate: draws the currently selected item's TITLE -- locale string
// categoryRecords[nSelectedItemIndex].dwTitleStringId -- centered and word-broken into
// pErrObj7's rect. Bails out (after restoring the DC) when the category file never loaded or
// the selected record has no title string, which is why the three restore calls appear twice in
// the original rather than once below a merged tail.
void TutorialWnd::DrawItemTitle(HDC *pHdc)
{
    COLORREF oldColor = SetTextColor(*pHdc, 0x461eff);
    int oldBkMode = SetBkMode(*pHdc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(*pHdc, g_UIResources.m_hFont16);

    char szText[0x200] = "";

    // EFFECTIVE MATCH (insns 110/110, byte_diff 108/314; 8 bytes long, all of it wider jump
    // encodings). The ONLY disagreement is block placement: the original emits this bail-out
    // restore block IMMEDIATELY after the guard and jumps FORWARD over it into the draw body
    // (`jne body`, fall through to the bail), while cl here sinks it past the body and jumps
    // forward to IT instead. Probed and REFUTED, all strictly worse: an explicit if/else with a
    // restore in each arm (331 B), the positive `&&` guard with the body as the then-arm and its
    // own `return` (331 B), and a nested `if (loaded) { if (id == 0) bail; ... }` (365 B).
    // The two restore copies themselves are correct -- per CODEGEN #18i cl never cross-jumps a
    // tail the source did not duplicate, and the original's two copies differ in register state
    // (the bail's SelectObject still has oldFont live in edx; the body's reloads it).
    if (bCategoryFileLoaded == 0 || categoryRecords[nSelectedItemIndex].dwTitleStringId == 0) {
        SelectObject(*pHdc, oldFont);
        SetBkMode(*pHdc, oldBkMode);
        SetTextColor(*pHdc, oldColor);
        return;
    }

    g_UIResources.LoadLocaleString(categoryRecords[nSelectedItemIndex].dwTitleStringId, szText,
                                   sizeof(szText));

    RECT rect;
    CopyRect(&rect, &pErrObj7->rect);
    int nLen = strlen(szText);
    DrawTextA(*pHdc, szText, nLen, &rect,
              DT_CENTER | DT_WORDBREAK | DT_NOCLIP | DT_NOPREFIX);

    SelectObject(*pHdc, oldFont);
    SetBkMode(*pHdc, oldBkMode);
    SetTextColor(*pHdc, oldColor);
}

// FUNCTION: LOCO 0x4528e0
// vtable slot 0x1c -- TutorialWnd's override of PopupWndBase::OnDrawContent. Ignores its
// PAINTSTRUCT entirely (the original never reads the argument, but still `ret 4`) and instead
// does the view's whole first-paint bring-up:
//   1. one-shot: latch bPresenterActiveFlag, park the mouse at screen centre, and swap the
//      narration channel over to the selected item's own narration sound;
//   2. capture the full 0x240 x 0x170 window backdrop out of the shared work surface, using
//      the same drop-the-lock/blit/retake-the-lock sandwich as RestorePresenterBackdrop;
//   3. repaint all nine controls and restart the 100 Hz presenter timer;
//   4. present, and re-assert the resting cursor.
// Step 3 runs only once the presenter really is active, which is why bPresenterActiveFlag is
// tested a second time here rather than being folded into the one-shot above.
void TutorialWnd::OnDrawContent(PAINTSTRUCT *pPs)
{
    if (bIconResourcesLoadedFlag != 0) {
        if (bPresenterActiveFlag == 0) {
            bPresenterActiveFlag = 1;
            SetCursorPos(g_dwScreenHalfWidth, g_dwScreenHalfHeight);
            if (g_pDSoundManager != NULL) {
                UINT nSoundId = categoryRecords[nSelectedItemIndex].dwNarrationSoundId;
                if (pDSoundChannel != NULL) {
                    if (pDSoundChannel->nSoundId != 0) {
                        SoundBankEntry *pEntry =
                            g_UIResources.SoundBank_LookupEntryById(pDSoundChannel->nSoundId);
                        pEntry->Release();
                    }
                    pDSoundChannel->Release();
                }
                if (nSoundId != 0) {
                    SoundBankEntry *pEntry = g_UIResources.SoundBank_LookupEntryById(nSoundId);
                    pEntry->EnsureLoaded();
                    g_pDSoundManager->PlaySoundByIdWithHandle(nSoundId, &pDSoundChannel);
                }
            }
        }

        if (bCategoryFileLoaded != 0) {
            RECT rectWindow;
            GetWindowRect(hwndSelf, &rectWindow);
            RECT rectDest = {0, 0, 0x240, 0x170};

            unsigned char bWasLockGuarded = g_worldBoard.bSurfaceLockGuard;
            if (g_worldBoard.bSurfaceLockGuard != 0 && g_pDDrawWorkSurface->Unlock(NULL) == 0) {
                g_worldBoard.bSurfaceLockGuard = 0;
            }

            pOffscreenSurface->Blt(&rectDest, g_pDDrawWorkSurface, &rectWindow, 0x1000000, NULL);

            if (bWasLockGuarded != 0 && g_worldBoard.bSurfaceLockGuard == 0) {
                memset(g_worldBoard.aSurfaceDescScratch, 0,
                       sizeof(g_worldBoard.aSurfaceDescScratch));
                g_worldBoard.aSurfaceDescScratch[0] = 0x7c; // sic: see DDSurfaceDescPadded0x7c
                if (g_pDDrawWorkSurface->Lock(NULL,
                        (LPDDSURFACEDESC)g_worldBoard.aSurfaceDescScratch, 0, NULL) == 0) {
                    g_worldBoard.bSurfaceLockGuard = 1;
                }
            }

            if (bPresenterActiveFlag != 0) {
                RedrawControl(6);
                RedrawControl(8);
                RedrawControl(1);
                RedrawControl(2);
                RedrawControl(3);
                RedrawControl(7);
                RedrawControl(4);
                RedrawControl(9);
                RedrawControl(5);
                KillTimer(hwndSelf, nScrollTimerId);
                nScrollTimerId = SetTimer(hwndSelf, 0x54, 10, NULL);
            }

            CommitScreenUpdate(hwndSelf, 0, 0);
            SetCursorDesc(cursorNormal.nMaskSurfaceKey, cursorNormal.pDesc, 0, 1);
        }
    }
}

// FUNCTION: LOCO 0x452230
// RedrawControl's case-4 delegate: draws the ONE page of the selected item's description text
// that is currently scrolled into view, into pErrObj6's rect.
//
// The page is carved out of the full description string by two DrawDescriptionChunks probes
// rather than by any stored offset: chunk count nListScrollOffset gives the character index
// where the current page STARTS, and nListScrollOffset + 1 gives where it ends (-1 meaning "the
// rest of the string"). The copy loop below then keeps exactly the [nSkip, nEnd) window.
//
// Between assembling the text and drawing it the function presents once, re-stamps the text
// panel's background from pErrObj9's bitmap, repaints the item icon, and re-acquires the
// offscreen DC through *pHdc -- which is why the DC is passed by pointer: the handle the caller
// gave us is dead by the time this returns, and the replacement is written back through it.
//
// EFFECTIVE MATCH (insns 264/292 -- the candidate is 28 SHORT; byte_diff 259/823). Every
// instruction of the real body agrees; the whole residual is that cl CROSS-JUMPS the three
// bail/tail restore-and-present copies below into one shared block, while the original emits
// each one in full with its own epilogue. This is the MIRROR of CODEGEN #18i and it lands on
// that lesson's documented hard limit: the original's copies are not byte-identical to each
// other (tail 1 stages oldFont through ecx, tail 2 through eax), so its non-merge is a
// phase-ordering artifact of the original build rather than something a source shape selects.
// Probed and REFUTED: factoring the sequence into a private inline member helper called at all
// four sites -- cl inlines it fully and then cross-jumps the copies anyway, byte-for-byte the
// same 772-byte output.
void TutorialWnd::DrawDescriptionPage(HDC *pHdc)
{
    COLORREF oldColor = SetTextColor(*pHdc, 0xff5c00);
    int oldBkMode = SetBkMode(*pHdc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(*pHdc, g_UIResources.m_hFont16);

    char szPage[0x200] = "";

    char szDescription[0x200] = "";

    if (bCategoryFileLoaded == 0) {
        SelectObject(*pHdc, oldFont);
        SetBkMode(*pHdc, oldBkMode);
        SetTextColor(*pHdc, oldColor);
        CommitScreenUpdate(hwndSelf, *pHdc, 1);
        return;
    }

    if (nSelectedItemIndex >= 0 &&
        categoryRecords[nSelectedItemIndex].dwDescriptionStringId != 0) {
        g_UIResources.LoadLocaleString(categoryRecords[nSelectedItemIndex].dwDescriptionStringId,
                                       szDescription, sizeof(szDescription));
        if (szDescription[0] != '\0') {
            int nSkip = DrawDescriptionChunks(nListScrollOffset, pHdc);
            int nEnd = DrawDescriptionChunks(nListScrollOffset + 1, pHdc);
            if (nEnd == -1) {
                nEnd = strlen(szDescription);
            }

            char *pDst = szPage;
            for (int i = 0; i < nEnd; i++) {
                if (i >= nSkip) {
                    *pDst++ = szDescription[i];
                }
            }
            // sic: terminates at nEnd, not at (pDst - szPage). Harmless only because the tail
            // between the two indices is still whatever the zero-init left there.
            szPage[nEnd] = '\0';

            int nLen = strlen(szPage);
            RECT rect;
            CopyRect(&rect, &pErrObj6->rect);

            SelectObject(*pHdc, oldFont);
            SetBkMode(*pHdc, oldBkMode);
            SetTextColor(*pHdc, oldColor);
            CommitScreenUpdate(hwndSelf, *pHdc, 1);

            RECT rectPanel;
            SetRect(&rectPanel, 0, 0, 0xd9, 0x96);
            OffsetRect(&rectPanel, 0x2a, 0x23);
            ((LocoBitmap *)pErrObj9->nRealizedHandle)
                ->RestoreOverlapBlt(pErrObj6->rect, pOffscreenSurface, rectPanel, 0);
            RedrawControl(7);

            *pHdc = AcquireOffscreenSurfaceDC(hwndSelf);
            oldColor = SetTextColor(*pHdc, 0xff5c00);
            oldBkMode = SetBkMode(*pHdc, TRANSPARENT);
            oldFont = SelectObject(*pHdc, g_UIResources.m_hFont16);

            OffsetRect(&rect, 1, -1);
            DrawTextA(*pHdc, szPage, nLen, &rect,
                      DT_WORDBREAK | DT_NOCLIP | DT_NOPREFIX | DT_END_ELLIPSIS | DT_MODIFYSTRING);

            SelectObject(*pHdc, oldFont);
            SetBkMode(*pHdc, oldBkMode);
            SetTextColor(*pHdc, oldColor);
            CommitScreenUpdate(hwndSelf, *pHdc, 1);
            return;
        }
    }

    SelectObject(*pHdc, oldFont);
    SetBkMode(*pHdc, oldBkMode);
    SetTextColor(*pHdc, oldColor);
    CommitScreenUpdate(hwndSelf, *pHdc, 1);
}

// TU-local byte-returning predicate over the accumulated [TUTORIAL] ini value: the original
// MATERIALIZES the strstr result into a byte register (`test eax,eax; setne al; test al,al`)
// before branching on it, which is what an `unsigned char`-returning inline predicate does and
// a plain `if (strstr(...) != NULL)` does not. See docs/CODEGEN.md's
// sete-materialized-predicate lesson.
static inline unsigned char IsNotifyTokenRecordedMaybe(const char *pszValue,
                                                       const char *pszToken) {
    return strstr(pszValue, pszToken) != NULL;
}

// EFFECTIVE MATCH (asmscore.py --len 494: insns 138/143, align 98, byte_diff 38, total 98038):
// content-complete and structurally identical -- every block pairs up, the frame is the exact
// 0x414 the original uses, and both scratch-buffer pairs land in the original's own slots. Two
// stacked intrinsic classes, both already documented as unsteerable, account for the whole
// residual:
//   (1) the strstr predicate widens the wrong way -- the original is `test eax,eax; setne al`,
//       cl gives `neg eax; sbb eax,eax; neg eax` (docs/CODEGEN.md's sete-materialized-predicate
//       caveat (4)). The predicate lever still EARNS ITS KEEP: it supplies the `test al,al` pair
//       the plain `if (strstr(...) != NULL)` lacks, which is worth ~80 align on its own.
//   (2) cl cross-jumps this arm's `return 0` into the early-exit guard's epilogue (one `jmp`
//       replacing 7 instructions) where the original keeps two separate copies -- and it can
//       only keep them because ITS two copies differ, interleaving `xor al,al` between the pops
//       at 0x44f73f but not at 0x44f642. That is scheduling, with no source spelling behind it.
//       CODEGEN #18i's documented hard limit.
// Probed and REJECTED (v401), all byte-identical or worse: an `unsigned char` predicate taking
// the strstr RESULT as a pointer parameter (byte-identical, total 98038); hoisting the whole
// if/else chain inside the first buffer scope so the predicate feeds the branch directly with no
// intervening local (WORSE, total 98056 -- and it does not restore the `setne` either). Do not
// re-grind; see docs/PARKED.md.
// FUNCTION: LOCO 0x44f560
// The generic "the player just did <code>/<subCode>" entry point, called from UI handlers all
// over the binary. Records the pair, then decides whether it warrants opening the tutorial
// view: never in a network game or under the screensaver, and never twice for the same pair --
// the [TUTORIAL] ini key for this player name accumulates the "(sub)"/"(sub,code)" tokens of
// every pair already shown, and a strstr against that value is the already-shown test.
//
// The `code != 0` tests in both arms are what makes the API dual-purpose: callers passing
// code 0 ALWAYS launch (and never touch the ini), while callers passing a real code launch at
// most once per pair and record the pair as they do. Returns 1 when the view was launched.
unsigned char TutorialWnd::NotifyOrLaunch(int code, unsigned int subCode) {
    lastNotifySubcode = code;
    lastNotifyCode = subCode;

    if (g_pDPlaySessionMgr->connectionMode == 2 || g_screenSaver.bScreenSaverMode == 1) {
        return 0;
    }

    nSelectedItemIndex = MapNotifyToItemIndex();
    bBoardScrollFlagAtNotify = g_bBoardScrollFlag;

    unsigned char bAlreadyShown;
    {
        char szValue[0x400];

        g_pIniFile->ReadString("TUTORIAL", g_pLocalPlayerIdentity->name, "", szValue,
                               sizeof(szValue));

        char szToken[20] = "";
        FormatNotifyToken(szToken);
        bAlreadyShown = IsNotifyTokenRecordedMaybe(szValue, szToken);
    }

    if (bAlreadyShown || nSelectedItemIndex == -1) {
        if (code != 0) {
            return 0;
        }
    } else if (code != 0) {
        char szValue[0x400];

        g_pIniFile->ReadString("TUTORIAL", g_pLocalPlayerIdentity->name, "", szValue,
                               sizeof(szValue));

        char szToken[20] = "";
        FormatNotifyToken(szToken);
        strcat(szValue, szToken);
        g_pIniFile->WriteString("TUTORIAL", g_pLocalPlayerIdentity->name, szValue);
    }

    nGlobalStateMirror = g_nScreenState;
    AppWindow_SetScreenState(8);
    Launch(nSelectedItemIndex);
    SelectCategory(nSelectedItemIndex);
    return 1;
}

// FUNCTION: LOCO 0x44f750
// Formats the pending notification pair into the ini token NotifyOrLaunch matches against:
// "(<subcode>)" when there is no code, "(<subcode>,<code>)" when there is. The two arms use
// SWAPPED stack slots for their two scratch buffers, which is why they are written with their
// own per-arm declarations in the order the original allocated them.
void TutorialWnd::FormatNotifyToken(char *pszOut) {
    if (lastNotifyCode == 0) {
        char szNum[20] = "";

        _itoa(lastNotifySubcode, szNum, 10);

        char szToken[20] = "(";
        strcat(szToken, szNum);
        strcat(szToken, ")");
        strcpy(pszOut, szToken);
    } else {
        char szNum[20] = "";

        _itoa(lastNotifySubcode, szNum, 10);

        char szToken[20] = "(";
        strcat(szToken, szNum);
        strcat(szToken, ",");
        _itoa(lastNotifyCode, szNum, 10);
        strcat(szToken, szNum);
        strcat(szToken, ")");
        strcpy(pszOut, szToken);
    }
}

// FUNCTION: LOCO 0x44f9a0
// Maps the pending notification pair onto the categoryRecords index the tutorial view should
// open on, or -1 when the pair has no tutorial page of its own. The outer switch is on the
// SCREEN that raised the notification (lastNotifySubcode); only the two screens that raise many
// distinct notifications -- 4 and 7 -- need an inner switch on the code itself.
//
// Case order is the ORIGINAL's, read off the jump table at 0x44fa78 by mapping each entry to
// its block and sorting the blocks by address (docs/CODEGEN.md's jump-table-order lever), which
// is why it is neither numeric nor grouped. The trailing no-op case groups carry no code of
// their own but are what size each table: the outer one spans 0..15 (`cmp eax,0xf`), case 7's
// spans 0x2406..0x240f and case 4's 0xc54..0xc5c, so the max label in each has to be present.
int TutorialWnd::MapNotifyToItemIndex() {
    switch (lastNotifySubcode) {
    case 6:
        return 4;
    case 7:
        switch (lastNotifyCode) {
        case 0x2406:
            return 8;
        case 0x2409:
            return 0x10;
        case 0x240a:
            return 0xc;
        case 0x240b:
            return 0xb;
        case 0x2408:
            return 0xa;
        case 0x240d:
            return 0xd;
        case 0x2407:
            return 9;
        case 0x240c:
        case 0x240e:
        case 0x240f:
            return -1;
        }
        break;
    case 8:
        return 5;
    case 1:
        return 0x14;
    case 2:
        return 0x1b;
    case 3:
        return 0x1c;
    case 4:
        switch (lastNotifyCode) {
        case 0x818:
            return 0x23;
        case 0x848:
            return 0x22;
        case 0xc5c:
            return 0x20;
        case 0xc54:
        case 0xc56:
        case 0xc58:
        case 0xc5a:
            return 0x24;
        }
        break;
    case 11:
        return 0x1d;
    case 12:
        return 0x1e;
    case 0:
    case 5:
        return 1;
    case 9:
        return 0xf;
    case 10:
    case 13:
    case 14:
    case 15:
        return -1;
    }
    return -1;
}
