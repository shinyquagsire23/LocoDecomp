// The universal context-menu-item node used throughout the Widget family's menu-node linked
// lists (WidgetBaseObj0x4784c8::pMenuListHead and friends). See docs/subsystems.md's
// "RESOLVED 2026-07-13 (v59)" writeup -- named by its vtable address per project convention.
#pragma once

#include "WidgetBase.h"
#include "CursorDesc.h" // the node's icon/cursor descriptor (TileKind-shaped);
                              // canonical definition, shared with WindowBase.cpp

class SavedFileEntry; // WidgetPicker.h -- only used as a pointer here

// Base context-menu-item node (0x58/88 bytes). Ctor 0x40cfa0, dtor 0x40d040 (cascades delete
// down pNext -- an OWNING intrusive singly-linked list), scalar dtor 0x40d020. 9 vtable
// slots: dtor + 5 inherited RectFlagObj0x477820 stubs + 3 own (SetFrameAndNotify/slot 6,
// TickAdvanceFrame/slot 7, Draw/slot 8) -- all 3 own virtuals now transcribed.
class MenuNodeObj0x477568 : public RectFlagObj0x477820 {
public:
    WidgetBaseObj0x4784c8 *pOwner; // +0x24
    MenuNodeObj0x477568 *pNext;    // +0x28 -- owning intrusive-list link
    short wModeFlagsMaybe;              // +0x2c -- ctor's 3rd param; bit 2 = carousel-enabled
    // +0x30 -- retyped 2026-07-16 from PromoteOrSelectSaveEntry: written by
    // RelocateSavegameSelection (one per visible slot node, tracking which sorted-list
    // entry that slot currently displays) and read back here to locate the entry's embedded
    // ThumbnailBmp (`&pEntry->embeddedThumbnailBmp`, confirmed against
    // WidgetPickerObj0x477cc8::Unk0x498's own "last-displayed-thumbnail" cache).
    SavedFileEntry *pEntry;   // +0x30
    RECT rectLocal;                // +0x34 -- (0,0,descWidth,descHeight); Draw's sprite-sheet sub-rect
    CursorDesc *pIconDesc;        // +0x44
    short wState;                  // +0x48 -- target of SetNodeState
    int nFrameIndex;               // +0x4c
    int nTickCounter;              // +0x50
    short wSelIndexMaybe;               // +0x54 -- sentinel 0xffff
    bool bVisible;                    // +0x56 -- "visible/enabled" gate for Draw, init 1

    // Ctor (0x40cfa0). Chains RectFlagObj0x477820's ctor, flat field-inits, then tail-calls
    // InitOwnerAndDescMaybe below.
    MenuNodeObj0x477568(WidgetBaseObj0x4784c8 *pOwner, CursorDesc *pDesc, unsigned short wModeFlags);
    virtual ~MenuNodeObj0x477568();
    // slot 6, 0x40d2a0. Sets nFrameIndex/rectLocal's left+right (sprite-sheet sub-rect for
    // nFrame), redraws (slot 8), resets nTickCounter.
    virtual void SetFrameAndNotify(int nFrame);
    // slot 7, 0x40d2f0. Real return type unsigned char (bare mov al,1/xor al,al, no EAX-wide
    // clear -- CLAUDE.md's bool-return lesson). Auto-advance: increments nTickCounter every
    // call while bValid; if wState==1 and pIconDesc's variant count (nButtonFrameCount) is
    // >3, returns 1 -- advancing nFrameIndex (wrapping at count-3) via SetFrameAndNotify once
    // every 3rd tick, only if the frame actually changed. Returns 0 if bValid is false,
    // wState != 1, or the variant count is <=3.
    virtual unsigned char TickAdvanceFrame();
    // slot 8, 0x40d340. EFFECTIVE MATCH -- PARKED (see src/MenuNode.cpp's own comment).
    // Positions the node (carousel-shifted), clip-tests against pOwner->rectViewport, and if
    // visible blits pIconDesc->pShadowBitmap onto pOwner->pKindDesc->pOwnedObjA via
    // LocoBitmap::BlitOntoBitmap, then dirty-marks via WorldBoardMaybe::MarkRectDirty.
    virtual void Draw();

    // Ordinary (non-virtual) member, 0x40d170 -- this-typed 2026-07-16 (was a free function in
    // the "Widget" namespace; genuinely reads/writes this->wState, so a real method fits).
    // If bValid and the new state differs, sets wState and notifies via slot 6 (mode
    // 1/2/3) or does carousel-reposition arithmetic + a recursive call (mode 4).
    void SetNodeState(short state);
    // Ordinary (non-virtual) member, 0x40d0b0. Sets pOwner/pIconDesc; if either is null marks
    // bValid false, else computes rectLocal/base rect from pDesc's shadow-frame dims, sets
    // wState=1/wModeFlagsMaybe/wSelIndexMaybe, and (if wModeFlagsMaybe bit 2 set) grows
    // pOwner's own nCarouselMaxIndex to fit this node's icon width. Param deliberately named
    // pOwnerArg -- would otherwise shadow the pOwner member (CLAUDE.md's naming-shadowing trap).
    void InitOwnerAndDescMaybe(WidgetBaseObj0x4784c8 *pOwnerArg, CursorDesc *pDesc, unsigned short wModeFlags);
    // Ordinary (non-virtual) member, 0x40d470 -- the grid-placement half of
    // InitOwnerAndDescMaybe, run once per freshly created icon node by the tool-menu
    // populate loops. Converts pIconDesc's still-in-GRID-UNITS column/row (+0x2e/+0x30,
    // both < 4 on entry, which is the guard) into pixels in place (col*0x39 - 0x32,
    // row*0x39 - 0x28), rebuilds the node's own rect around them at the descriptor's
    // shadow-frame size, and -- for carousel-enabled nodes -- grows pOwner's
    // nCarouselMaxIndex to reach this column.
    void PlaceIconInGridMaybe();
};

// Icon+text leaf variant (0x68/104 bytes), built by the shared factory
// TutorialWnd::ResourceRefCategoryTable_GetOrCreateIconItemMaybe (0x4546d0) when an icon/text
// id is supplied. Ctor 0x448f30, dtor 0x449000 (frees pText then chains the base dtor),
// scalar dtor 0x448fe0. Overrides slot 0 (scalar dtor), slot 7 (TickAdvanceFrame,
// 0x449190), slot 8 (Draw, 0x4491d0) -- all 3 own overrides now transcribed.
class UiIconListItem : public MenuNodeObj0x477568 {
public:
    bool bTextRedrawEnabled;    // +0x58 -- gates the extra per-tick text redraw in TickAdvanceFrame
    int nTextLen;   // +0x5c -- ctor's param_1; also SetLabelText's strncpy cap
    char *pText;    // +0x60 -- heap-allocated label text, ctor-initialized to the empty string
    int hFontLabel;    // +0x64 -- ctor's param_4 verbatim; confirmed 2026-07-20 (Draw's own
                            // SelectObject(hdc, hFontLabel) call) -- the label's HFONT.

    // Ctor (0x448f30). Chains MenuNodeObj0x477568's ctor, allocates pText (nTextLenArg+1
    // bytes) and seeds it with the empty string.
    UiIconListItem(int nTextLenArg, WidgetBaseObj0x4784c8 *pOwner, CursorDesc *pDesc, int hFontLabelArg, unsigned short wModeFlags);
    virtual ~UiIconListItem();

    // Returns pText.
    char *GetLabelText();
    // strncpy's pszText into pText (capped at nTextLen), then uppercases pText in place
    // over strlen(pszText) chars (the UNTRUNCATED input length, not nTextLen -- CRT
    // toupper() via FUN_00467710, an intrinsic-shaped leaf, not a private helper). Returns
    // pText. Param deliberately NOT named pText (would shadow the member -- CLAUDE.md's
    // naming-shadowing trap).
    char *SetLabelText(char *pszText);
    // strlen(pText). Real return type is unsigned (confirmed 2026-07-16, v123 -- an
    // `<= 0` comparison against it needs the unsigned jbe/jle codegen split to byte-match; a
    // signed int return only ever produces jle/je regardless of the source operator).
    unsigned GetLabelTextLength();
    // 0x449100 -- in-place single-key text editor for this node's own label, used by the
    // widget key handlers that let the user type a savegame name straight into a list slot
    // (WidgetPickerObj0x477cc8::OnKeyDownMaybe is one of the two callers). bTextRedrawEnabled
    // doubles as the "this label is editable" gate: returns 0 immediately (key NOT consumed)
    // when it is clear. Otherwise VK_BACK/VK_DELETE drop the last character (no-op on an
    // already-empty label) and any printable key (0x20..0x7e) appends one while
    // strlen(pText) < nTextLen; each accepted edit repaints via the virtual Draw(). A
    // rejected edit -- backspace on empty, append past nTextLen -- skips the repaint. sic: the
    // PlaySoundAtScreenPos(0x5460, x, y, 4) feedback fires on EVERY consumed key, not only the
    // rejected ones -- the accepted path calls Draw() and then falls straight into it. A
    // non-printable key is silently ignored. Always returns 1 once editable, i.e. an editable
    // node consumes EVERY key. EFFECTIVE MATCH (src/MenuNode.cpp).
    // Return type is `bool`, NOT the `unsigned char` this used to say (corrected v508):
    // WorldActionCursor::OnKeyDownMaybe (0x45b3a0) assigns the result straight into a bool
    // with no 0/1 normalization anywhere (`mov bl,al`, plain), which only a bool->bool
    // assignment produces; both prior callers are insensitive (WidgetPicker's explicit
    // `? true : false` ternary normalizes either way, the other discards the result), so the
    // uchar spelling cost nothing until this third caller could see the difference.
    bool HandleTextEditKey(unsigned int nKey);

    // slot 7, 0x449190. Real return type unsigned char (bare mov al,0/1, no EAX-wide clear --
    // CLAUDE.md's bool-return lesson): always 0 if !bVisible, else always 1. Calls the base
    // override first; if the base did NOT do a frame-count auto-advance (returned 0) and
    // bTextRedrawEnabled and nTickCounter is a multiple of 3, also redraws (virtual Draw()).
    virtual unsigned char TickAdvanceFrame();
    // slot 8, 0x4491d0. Draws the base icon (base Draw()) then the text label: guards on
    // pOwner/pOwner->pKindDesc/bVisible/pOwnedObjA->pSurface all non-null, allocates a
    // (nTextLen+2)-byte scratch buffer, computes a label RECT from rect (left+4, shifted up 2
    // rows when wState==2), GetDC's the owner's canvas surface, and DrawTextA's either pText
    // directly or (if this node's owner is the active tab AND bTextRedrawEnabled AND the
    // shared blink toggle is set) a "<pText>_" blink-cursor variant into the scratch buffer
    // first -- alternating every call via g_bLabelBlinkToggle. Text color: silver (0x808080)
    // when wState==3, else near-black (0x202020).
    virtual void Draw();
};
