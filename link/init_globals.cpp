// SMOKE-TEST SCAFFOLDING ONLY -- NOT part of the byte-match sources.
//
// Runs the constructors of stubbed global OBJECTS. link/gen_stubs.py gives every global no
// transcribed TU defines a zeroed slot in the .bss mirror, which is the right call for plain
// data -- but a zeroed slot for an object with a CONSTRUCTOR means that constructor never runs,
// and nothing in the build says so.
//
// ⭐ THE ORIGINAL'S OWN LIST, not a guess. VC++ 5.0 emits one dynamic-initializer thunk per
// global object with a constructor and collects the pointers in .CRT$XC; the linker merges that
// into .data and hands the bounds to __cinit, which is the SECOND of the two _initterm calls at
// 0x4684a0:
//
//     0x4684b5  _initterm(0x47e03c, 0x47e04c)   // __xi_a/__xi_z -- C initializers
//     0x4684c7  _initterm(0x47e000, 0x47e038)   // __xc_a/__xc_z -- C++ dynamic initializers
//
// The XC table at 0x47e000 has exactly THIRTEEN entries. Each thunk is the same shape -- load
// the object's address into ecx, call its ctor, then atexit() a matching thunk that calls the
// dtor -- so it names both the global and its constructor outright:
//
//   #   thunk       object     ctor       what
//   1   0x45c530  0x4a9910   0x448040   ScreenSaver             g_screenSaver
//   2   0x45c560  0x4855e8   0x445f70   UIResources             g_UIResources
//   3   0x45c590  0x4a98b0   0x44d800   PeerTrainSlotQueueMaybe g_PeerTrainSlotQueue
//   4   0x45c5c0  0x4854c8   0x410510   PlacementCursorMaybe    PlacementCursorMaybe_004854c8
//   5   0x45c5f0  0x485448   0x434500   DecorObjMgrMaybe        DecorObjMgrMaybe_00485448
//   6   0x45c620  0x4a9990   0x41d250   NetSessionEventQueue    g_NetSessionEventQueue
//   7   0x45c650  0x4a99b0   0x41f480   ScriptEventLoader       (g_easterEggMgrMaybe's bytes)
//   8   0x45c680  0x4fd220   0x4238c0   EffectSpawner           DAT_004fd220
//   9   0x45c6b0  0x4aa5b8   0x449430   BuildToolButton         g_BuildToolButton
//  10   0x45c6e0  0x4a9ef0   0x4589b0   WorldActionCursor       g_worldActionCursor
//  11   0x45c710  0x4852a0   0x42cce0   SelectedObjWidgetMaybe  SelectedObjWidgetMaybe_004852a0
//  12   0x45c740  0x4aad08   0x454cf0   WorldBoardMaybe         g_worldBoard
//  13   0x45c770  0x4a9ad0   0x461610   ThreadWrapper           g_worldLoadThread
//
// All thirteen ctors are transcribed. TWELVE of the thirteen objects are mirror stubs, so their
// ctors never ran; only #13 is a real C++ definition (src/FrameDriver.cpp defines
// g_worldLoadThread outright), which is why it alone is absent below -- our own CRT constructs
// it for us. The ORDER here is the XC table's order, verbatim: the original's initializers ran
// in exactly this sequence and some of these objects reach for each other.
//
// The per-object hooks live in the owning src/ TU inside `#ifdef LOCO_PORT`, not here. They have
// to: several of these layouts are modeled by a TU-LOCAL view struct (EffectSpawner's real
// layout is EffectSpawner.cpp's EffectSpawnerCtorViewMaybe -- src/EffectSpawner.h models only
// entry points, so a placement-new of `EffectSpawner` would construct nothing at all), and
// WorldBoardMaybe's ctor is transcribed in the free __fastcall escape-hatch form.
//
// g_UIResources is constructed here rather than through a hook, and is the case that first
// mattered: UIResources::UIResources (0x445f70) is the only code anywhere that identity-fills
// m_pKindSlotPtrsMaybe, the redirect table TileKind_GetOrLoadDescriptor reaches EVERY descriptor
// through. Zeroed, the registry answers NULL to every lookup however well the descriptors
// themselves loaded -- and SplashWnd::EnsureArtLoaded dereferenced that NULL.
//
// Constructed IN PLACE rather than defined for real in src/: the mirror deliberately preserves
// the original's ALIASING, and g_RFIndex IS g_UIResources+0x18 (src/BigObj.cpp,
// src/CursorDesc.cpp and src/CarKindDesc.cpp all read the RF archive through it). A separate
// real definition would give g_UIResources a new address and silently orphan g_RFIndex. The
// same reasoning covers every hook below -- none of these globals may be promoted to a real
// definition while the mirror is what preserves their aliasing.
//
// ⚠ This file is compiled TWICE: by tools/build_port.sh with `-D LOCO_PORT -I port`, and by
// tools/link_check.sh with neither. The hooks only exist in the port build, so everything below
// XC 2 is inside `#ifdef LOCO_PORT`; the link_check smoke exe keeps the old g_UIResources-only
// behaviour. The include is by relative path for the same reason (`/I port` is not on
// link_check's command line).
#include <new.h>

#include "../src/UIResources.h"
#ifdef LOCO_PORT
#include "../port/PortGlobalCtors.h"
#endif

extern "C" void Smoke_ConstructGlobals(void) {
#ifdef LOCO_PORT
    Port_Construct_g_screenSaver();          // XC 1
#endif
    new (&g_UIResources) UIResources();      // XC 2
#ifdef LOCO_PORT
    Port_Construct_g_PeerTrainSlotQueue();   // XC 3
    Port_Construct_PlacementCursor();        // XC 4
    Port_Construct_DecorObjMgr();            // XC 5
    Port_Construct_g_NetSessionEventQueue(); // XC 6
    Port_Construct_ScriptEventLoader();      // XC 7
    Port_Construct_EffectSpawner();          // XC 8
    Port_Construct_g_BuildToolButton();      // XC 9
    Port_Construct_g_worldActionCursor();    // XC 10
    Port_Construct_SelectedObjWidget();      // XC 11
    Port_Construct_g_worldBoard();           // XC 12
    // XC 13 (g_worldLoadThread) is a real definition in src/FrameDriver.cpp -- our own CRT
    // already emits its initializer, so constructing it again here would double-construct it.
#endif
}
