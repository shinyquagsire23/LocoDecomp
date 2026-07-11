// The game-window widget-list collection singleton (DAT_004a9994) and its access probe.
//
// Relocated here 2026-07-25 (v407) from src/DPlaySessionMgr.cpp -- a pure text move, that TU
// now includes this header from the exact position the definitions used to occupy, so its
// preprocessed output is unchanged (the same technique src/Obj0x477798Family.h's own
// relocation note records). The move was forced by a SECOND consumer:
// PlacementCursorMaybe::RefreshFootprintHighlightMaybe (0x410d20) scans the list's own item
// ARRAY at +0x4, which the old DPlaySessionMgr-local view buried inside `char pad0x0[0xc]`.
#pragma once

class AnimDescRefObj0x477488;

// A value-type global collection object holding placed world objects/widgets: same 0x10-byte
// collection family as NetSessionEventQueue's pEvents (src/Obj0x477798Family.h -- vtbl @ +0x0,
// item array @ +0x4, capacity @ +0x8, live count @ +0xc; GetItem @ vtbl+0x20 / slot 8).
// Declared NON-polymorphic (no virtuals of its own) so the GetItem call, reached by
// reinterpret-casting to the probe below, keeps the original's indirect `call [eax+0x20]`
// dispatch -- a virtual declared on this known global's own type would let MSVC devirtualize
// it to a direct call. (Mirrors src/NetSessionEventQueue.cpp.)
struct GameWindowWidgetList {
    void **vtbl;                        // +0x0 -- dispatched only via the probe below
    AnimDescRefObj0x477488 **paItems;   // +0x4 -- the item array, nCapacity slots wide
    unsigned int nCapacity;             // +0x8 -- allocated slot count
    unsigned int nItemCount;            // +0xc -- live item count
};
extern GameWindowWidgetList g_gameWindowWidgetList;  // DAT_004a9994

// Padded-vtable probe for the widget list's own GetItem @ vtbl+0x20 (slot 8).
struct GameWindowWidgetListProbe {
    virtual void *_v00(); virtual void *_v01(); virtual void *_v02(); virtual void *_v03();
    virtual void *_v04(); virtual void *_v05(); virtual void *_v06(); virtual void *_v07();
    virtual AnimDescRefObj0x477488 *GetItemImpl(unsigned int idx); // vtbl+0x20 (slot 8)
};
