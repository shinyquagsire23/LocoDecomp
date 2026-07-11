// ResourceRef -- a small (0x24/36-byte) polymorphic resource-handle wrapper, widely
// reused across the UI (NetSetupWnd::InitFields 0x440fa0, ApplSetupWnd::InitFields 0x408b20,
// EditCardWnd::InitFields -- see src/EditCardWnd.cpp). Layout confirmed via
// Ghidra's own struct DB (built in an earlier session): vtable, a RECT (the button/icon's own
// screen rect -- confirmed as a real RECT, not 4 loose ints, by a SECOND independent consumer,
// EditCardWnd's WindowBase vtable+0x1c override at 0x417180, which writes all 4 sub-fields as
// a coherent CenterRectInRect-style group), a CursorDesc* realized lazily when the
// resource is actually loaded, a realized-handle int, the raw resource id, and one more
// trailing int.
//
// Ghidra currently labels the ctor "InitResourceId" (an earlier session's hypothesis, before
// its shape was fully understood). Confirmed this session (see EditCardWnd.cpp): every call
// site's "new_alloc + null-check + call" dance is the compiler's own /GX new-expression
// alloc-protection scaffolding (see CLAUDE.md), not hand-written code -- this genuinely IS
// ResourceRef's real C++ constructor (it sets the vtable pointer, which only a real
// ctor/dtor legitimately does). Modeled here as a real ctor so `new ResourceRef(id)`
// compiles idiomatically; not itself transcribed/address-marked this session (declared only,
// like any other not-yet-transcribed callee).
//
// CORRECTED (EditCardWnd_DtorMaybe session): no explicit `pVtable` field -- like LocoBitmap.h,
// the compiler synthesizes the vtable pointer at +0x0 automatically because the class has a
// virtual dtor; a hand-declared field here would double-count it and push every real field 4
// bytes late. The dtor itself is declared (not defined) purely so every `delete
// pResourceRefMaybe;` call site reproduces the observed `(**(vtbl))(1)` scalar-deleting-
// destructor call shape (confirmed at ~35 EditCardWnd call sites, see EditCardWnd.cpp).
#pragma once

#include <windows.h>
#include <ddraw.h>

#include "CursorDesc.h"

class ResourceRef {
public:
    RECT rect; // +0x4..+0x13 -- the button/icon's own screen rect, see header comment
    CursorDesc *pCursorDesc; // lazily realized when the resource is actually loaded
    int nRealizedHandle;
    int resourceId;
    int nM_20;

    ResourceRef(int resourceIdArg); // 0x454b50 (Ghidra: ResourceRef::InitResourceId)
    // DECLARED ONLY, and that is a measured choice, not an omission. Unlike IniFile /
    // ThumbnailBmp / LockableMaybe, this class has NO out-of-line ??1ResourceRef anywhere in
    // the image -- only the ??_G thunk at 0x454b70, which carries ReleaseRealized's body
    // inlined into it -- so the real dtor must be in-class. But writing it that way here costs
    // src/TutorialWnd.cpp its RestorePresenterBackdrop (0x452b00) match, 249 B, while buying
    // nothing: cl emits ??_G for the declared-only form too (30 B vs 36), and 0x454b70 stays a
    // residual either way. See src/ResourceRef.cpp's 0x454b70 autopsy for the full numbers.
    virtual ~ResourceRef();

    void ReleaseRealized(); // 0x454bc0 -- see src/ResourceRef.cpp

    // 0x454bf0 -- realizes pCursorDesc via g_UIResources.TileKind_GetOrLoadDescriptor(resourceId)
    // (ex-FUN_00446ea0), then nRealizedHandle = pCursorDesc->GetOrLoadFrameBitmap(0, 0).
    // Returns whether BOTH steps produced something.
    bool Load();

    // 0x454c30 -- if realized (nRealizedHandle != 0, reinterpreted as a LocoBitmap*),
    // blits it to pTargetSurface (defaults to g_pDDrawWorkSurface when NULL) at `rect`,
    // offset horizontally by nFrameIndex*pCursorDesc's own frame width when nFrameIndex != 0
    // (a multi-frame strip draw, same idea as WindowBase's animated-cursor strip). Called by
    // AlbumCardWnd::DrawOrEraseCardSlot on each populated slot's paCardGrid[6+i] entry.
    void DrawFrame(int nFrameIndex, IDirectDrawSurface *pTargetSurface); // 0x454c30
};
