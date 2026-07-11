// ResourceRef -- see src/ResourceRef.h for the class overview.

#include "ResourceRef.h"
#include "LocoBitmap.h"   // the realized frame bitmap DrawFrame blits
#include "UIResources.h"  // g_UIResources, the kind-descriptor registry Load resolves through

// DAT_004fd3c4 -- the shared off-screen compositing surface, declared file-locally exactly as
// src/ApplSetupWnd.cpp / src/AlbumCardWnd.cpp / src/LoadingScreen.cpp already do.
extern IDirectDrawSurface *g_pDDrawWorkSurface;  // DAT_004fd3c4

// FUNCTION: LOCO 0x454b50
// Real C++ constructor (moved out of phase2_probe3.cpp 2026-07-22, v322): sets the vtable
// pointer (implicit, via the virtual dtor) and zeroes everything but the stored resource id.
// sizeof 0x24 pinned by the sole allocation site FUN_00401fd0 (pushes operator new(0x24)).
ResourceRef::ResourceRef(int resourceIdArg) {
    pCursorDesc = 0;
    nRealizedHandle = 0;
    resourceId = resourceIdArg;
    nM_20 = 0;
}

// FUNCTION: LOCO 0x454bc0
// Releases the lazily-realized CursorDesc reference (only if actually loaded, i.e.
// pCursorDesc->pOwnedObjA != 0) via CursorDesc::ReleaseRef (vtable slot 2,
// a refcount-decrement release -- see WindowBase.cpp), then unconditionally clears
// pCursorDesc/nRealizedHandle. Self-guarding: callers (e.g.
// EditCardWnd::TeardownBuiltUi) call this on every button handle with no null check of
// their own.
void ResourceRef::ReleaseRealized() {
    if (pCursorDesc && pCursorDesc->pOwnedObjA) {
        pCursorDesc->ReleaseRef();
    }
    pCursorDesc = 0;
    nRealizedHandle = 0;
}

// EFFECTIVE MATCH -- 30 B vs 65, DIFF(19), and BOTH ways of closing it were measured and cost
// more than they pay. The original inlines the whole dtor -- which is ReleaseRealized's body --
// into this thunk. (1) Writing the dtor in-class in src/ResourceRef.h gets this to 36 B / DIFF(20)
// and costs src/TutorialWnd.cpp its RestorePresenterBackdrop (0x452b00) match, -249 B for
// nothing. (2) ALSO making ReleaseRealized in-class does close it -- 0x454b70 EXACT at 65 B and
// this file 4/4 at 333 B -- but its ~35 consumer call sites then stop calling and start inlining,
// and SEVEN TUs regress: AlbumCardWnd -163, ApplSetupWnd -676, EditCardWnd -362, MailWnd -743,
// MapWnd -494, NetSetupWnd -478, TutorialWnd -249. Repo-wide that is 119164 B / 499 funcs as
// written against 116275 B / 489 fully in-class, i.e. -2889 B. 0x454bc0's own 38-byte COMDAT
// only exists because ReleaseRealized stays out of line, too.
//
// FUNCTION: LOCO 0x454b70 (??_GResourceRef scalar deleting dtor -- compiler-generated; the real
// ~ResourceRef is in-class in the original but is declared-only here, see src/ResourceRef.h)

// FUNCTION: LOCO 0x454bf0
// Lazily realizes the resource: resolves the kind descriptor out of the shared UI-resources
// registry, then asks it for frame 0's bitmap. Both steps are allowed to fail, and the caller
// gets a single "is it usable now" answer.
bool ResourceRef::Load() {
    pCursorDesc = g_UIResources.TileKind_GetOrLoadDescriptor(resourceId);
    if (pCursorDesc == 0) {
        return false;
    }
    nRealizedHandle = (int)pCursorDesc->GetOrLoadFrameBitmap(0, 0);
    return nRealizedHandle != 0;
}

// FUNCTION: LOCO 0x454c30
// Strip draw: the source rect is always one whole native-sized frame, slid right by
// nFrameIndex frames for a multi-frame resource. Silently does nothing when the resource has
// never been realized -- callers (AlbumCardWnd::DrawOrEraseCardSlot) rely on that.
void ResourceRef::DrawFrame(int nFrameIndex, IDirectDrawSurface *pTargetSurface) {
    if (nRealizedHandle != 0) {
        if (pTargetSurface == 0) {
            pTargetSurface = g_pDDrawWorkSurface;
        }
        RECT srcRect;
        srcRect.left = 0;
        srcRect.top = 0;
        srcRect.right = pCursorDesc->nativeWidth;
        srcRect.bottom = pCursorDesc->nativeHeight;
        if (nFrameIndex != 0) {
            OffsetRect(&srcRect, nFrameIndex * pCursorDesc->nativeWidth, 0);
        }
        ((LocoBitmap *)nRealizedHandle)->RestoreOverlapBlt(rect, pTargetSurface, srcRect, 0);
    }
}
