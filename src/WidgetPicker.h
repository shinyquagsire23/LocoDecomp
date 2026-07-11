// The load/save-game + backdrop file-picker widget. See docs/subsystems.md's widget-family
// table and the RESOLVED 2026-07-13 write-ups for WidgetPickerObj0x477cc8/SavedFileEntry.
#pragma once

#include "WidgetBase.h"
#include "LocoBitmap.h"
#include "MenuNode.h"
#include "ThumbnailBmp.h"

// (ThumbnailBmp moved to the shared src/ThumbnailBmp.h -- confirmed 2026-07-16 while transcribing
// EnumerateFiles: SavedFileEntry's own ctor/dtor fully INLINE at both their call sites in
// that function, but the embedded ThumbnailBmp sub-object's ctor/dtor stay real out-of-line calls
// at every site -- modeling it as a byte-array member instead of a real class would have silently
// dropped those calls.)

// One enumerated file record (a savegame or backdrop bitmap) in WidgetPickerObj0x477cc8's own
// sorted doubly-linked list.
class SavedFileEntry {
public:
    char szShortName[11];
    char szFileName[0x41];
    ThumbnailBmp embeddedThumbnailBmp;
    SavedFileEntry *pPrev;
    SavedFileEntry *pNext;

    // Both bodies defined inline (not out-of-line in WidgetPicker.cpp) -- confirmed 2026-07-16
    // via EnumerateFiles's raw disasm: its own stack-local sentinel object's ctor/dtor
    // fully INLINE at their one use site there (unlike the embedded ThumbnailBmp member's own
    // ctor/dtor, which stay genuine out-of-line calls -- see ThumbnailBmp above), matching this
    // codebase's established "define in the class body to force inlining" precedent
    // (LocoBitmap::~LocoBitmap). The compiler auto-generates the embedded ThumbnailBmp
    // member's construction/destruction either way -- neither body needs to touch it directly.
    SavedFileEntry() { szShortName[0] = 0; szFileName[0] = 0; pPrev = 0; pNext = 0; }
    virtual ~SavedFileEntry() {}
};

class WidgetPickerObj0x477cc8 : public WidgetBaseObj0x4784c8 {
public:
    // this+0xe0/this+0x2ea were previously mismodeled as lone `bool`s + inert padding -- the
    // ctor's `=false` writes are really just zeroing each buffer's first byte, and the
    // manual strcpy/strcat copy loops in HandleSavegameMenuNode/FUN_00429a10/FUN_00429b20
    // write well past that first byte into what was modeled as padding (RESOLVED 2026-07-16,
    // v121 -- see docs/subsystems.md's v119 recon writeup). Both are flat char/path buffers;
    // sizes are the full undefined span up to the next known field (no bounds-checked write
    // pins a smaller real extent -- every copy is an unbounded strcpy/strcat, consistent with
    // this file's other manual-copy-loop = CRT-intrinsic evidence).
    // ⚠ The +0xe0..+0x2e9 span was modeled as ONE 0x20a-byte buffer until v454, on the stated
    // "no write site found yet" basis below. ReloadActiveSaveState (0x429ef0) IS that write
    // site, and it writes at `this + 0x1e5` -- the span's exact midpoint (`add edx,0x1e5` at
    // 0x42a057, on the saved `this`). 0xe0 + 0x105 = 0x1e5 and 0x1e5 + 0x105 = 0x2ea (exactly
    // where the next known field starts), so the span is really TWO 0x105 = 261-byte buffers,
    // not one. 261 is corroborated three ways: szPendingSavePath below is 0x106 = 262 only
    // because the struct then pads 1 byte to 4-align embeddedIcon at +0x3f0 (0x2ea + 261 =
    // 0x3ef), and ReloadActiveSaveState's own path local occupies 264 stack bytes = 261 rounded
    // up to 4. So all three of this class's path buffers are `char[261]`.
    //
    // +0xe0: the "current save" path (category-1 overview tab, matching the "~curr" string
    // literal). Read-only in this project's transcribed code so far -- source arg to _splitpath
    // in ActivateTab (extracts just the fname portion, becomes the current-slot node's label)
    // and the LHS of a case-insensitive compare (confirmed 2026-07-20: FUN_00471480 is the real
    // CRT _stricmp) in HandleSavegameMenuNode case 1. Still no write site found.
    char szActiveSavePath[0x105]; // +0xe0 .. +0x1e4
    // +0x1e5: the currently-applied BACKDROP name -- the bare slot label (not a path), written
    // by ReloadActiveSaveState once the "backdrop\<label>" descriptor has actually realized a
    // bitmap (pOwnedObjA != NULL). No read site found yet in transcribed code.
    char szActiveBackdropName[0x105]; // +0x1e5 .. +0x2e9
    // +0x2ea: built fresh as "savegame\\<slotLabel>.sav" (relative path) by
    // HandleSavegameMenuNode case 1 and FUN_00429a10/FUN_00429b20 (Load/Save
    // hypotheses, still unconfirmed -- see docs/subsystems.md's v119 recon), then used as
    // the compare/save-target path. Declared 0x106 rather than 0x105 to absorb the struct's
    // own 1-byte tail pad -- see the span note above.
    char szPendingSavePath[0x106]; // +0x2ea .. +0x3ef

    AnimDescRefObj0x477488 embeddedIcon; // +0x3f0
    LocoBitmap embeddedBitmap;            // +0x478
    // +0x498 -- last-displayed-thumbnail cache: NULL, or the ThumbnailBmp currently mirrored
    // into embeddedBitmap (either &pEntry->embeddedThumbnailBmp for a savegame slot, or NULL
    // for the backdrop-fallback/placeholder paths -- see ReloadBackdropPreview). Compared as a
    // raw address in PromoteOrSelectSaveEntry to detect "the entry whose thumbnail is currently
    // shown got deselected."
    ThumbnailBmp *pShownThumbnailBmp;
    short nCategory;       // 5 = backdrop .bmp, else savegame .sav (RESOLVED 2026-07-13)
    // +0x4a0/+0x4a4/+0x4a8/+0x4ac -- the 4 individually-selected category tab buttons (2/3/4/5),
    // retyped 2026-07-16 from ActivateTab: each is lit (its own MenuNodeObj0x477568::+0x56
    // bool set) exclusively when its matching category becomes active, and stashed into the
    // base's pBaseCandidateUp as the keyboard-nav "up" target. +0x4ac was previously modeled
    // as 4 bytes of unexplained real padding -- resolved, it's the 4th button, just never written
    // by a caller this project had transcribed yet.
    MenuNodeObj0x477568 *pTabButtonNode0; // +0x4a0, category 2
    MenuNodeObj0x477568 *pTabButtonNode1; // +0x4a4, category 3
    MenuNodeObj0x477568 *pTabButtonNode2; // +0x4a8, category 4
    MenuNodeObj0x477568 *pTabButtonNode3; // +0x4ac, category 5
    // +0x4b0 -- always repositioned/tested regardless of which category (1-5) is entered;
    // ActivateTab's case 1 treats it as the 4th corner of a 2x2 grid alongside
    // +0x4a0/+0x4a4/+0x4a8, while cases 2-5 always reposition it to a single fixed spot
    // (0x51,0x9d) -- hypothesized as the "current save" overview tab's own button (category 1's
    // tab, which needs to move out of the way once a specific category is selected). Never
    // touches its own +0x56 highlight bool in this function.
    MenuNodeObj0x477568 *pOverviewTabButton; // +0x4b0
    // +0x4b4/+0x4b8 -- retyped 2026-07-16 from RelocateSavegameSelection: highlighted based
    // on whether a prior/further entry exists in the sorted list (pCurrentEntry->pPrev
    // for the up arrow; whether the last visible slot, arrSlotNodes[5], has any label text for the
    // down arrow) -- the list's own scroll-up/scroll-down arrow nodes.
    MenuNodeObj0x477568 *pScrollUpArrowNode;   // +0x4b4
    MenuNodeObj0x477568 *pScrollDownArrowNode; // +0x4b8
    // +0x4bc -- the currently-selected savegame-slot icon+text node (retyped 2026-07-16 from
    // TestMenuCommand: label text is copied onto it from whichever slot node was clicked).
    UiIconListItem *pCurrentSlotNode;
    // +0x4c0 -- the 6 visible on-screen slot nodes (always UiIconListItem in practice, per every
    // known call site's cast, but modeled at the shared base type like the tab buttons above).
    MenuNodeObj0x477568 *arrSlotNodes[6];
    SavedFileEntry *pLinkedListHead;
    // +0x4dc -- retyped 2026-07-16 from ActivateTab's sorted-position search loop: read at
    // offsets +0x228/+0x22c, which land EXACTLY on SavedFileEntry's own pPrev/
    // pNext fields (confirmed via the live Ghidra struct: vtable+szShortName+szFileName+
    // thumbnail = 0x228, +pPrev = 0x228..0x22c, +pNext = 0x22c..0x230). Read
    // immediately after each RelocateSavegameSelection call, so hypothesized as a "current
    // selection" cursor into the SavedFileEntry linked list, paired with pCurrentSlotNode.
    SavedFileEntry *pCurrentEntry;

    WidgetPickerObj0x477cc8();
    virtual ~WidgetPickerObj0x477cc8();

    // vtable slot 17 -- general "test menu command, highlight node if hit" dispatcher across
    // all picker categories (counterpart to the category-1-specific TestTabSwitchMenuCommandMaybe).
    // 0x428770, src/WidgetPicker.cpp -- vtable slot 3, this class's override: move the widget
    // through the base, then place embeddedIcon at pKindDesc's own "button" offset from the
    // widget's new position.
    virtual void RepositionWithHotspot(int x, int y);

    // Declared as the real slot-17 OVERRIDE since v552 (it was an ordinary non-virtual member
    // named TestMenuCommand, which left our emitted vtable inheriting the base's slot while the
    // image's own dword at 0x477cc8+17*4 is 0x4287b0 -- vtable_audit's "MISSING OVERRIDE
    // DECLARATION"). Name and signature are therefore the BASE's, not this class's own
    // vocabulary: C++ has no covariant parameters, so the original's slot 17 took a
    // MenuNodeObj0x477568* and downcast in the body, exactly as the sibling overrides
    // WorldActionCursor:: and SelectedObjWidgetMaybe::HitTestNodeSecondary do. Byte-neutral:
    // UiIconListItem derives from MenuNodeObj0x477568 at offset 0, there are no call sites
    // (the slot is the only entry point), and the residual is unchanged at DIFF(286).
    virtual char HitTestNodeSecondary(MenuNodeObj0x477568 *pNode, int x, int y);
    // 0x4289a0, src/WidgetPicker.cpp (EXACT v518) -- vtable slot 19, the category-1-specific
    // counterpart of TestMenuCommand:
    // while the "current save" overview tab (nCategory == 1) is active, a hit on one of the four
    // tab-switch command nodes (resourceId 0x2c02/0x2c03/0x2c04/0x2c05) dispatches the embedded
    // icon's slot-7 ReleaseChannelAndDispatch with that tab's own code (1/2/4/3) and consumes the
    // command; every other category or resource id is passed on (returns 0).
    unsigned char TestTabSwitchMenuCommandMaybe(UiIconListItem *param_1, int param_2, int param_3);
    // Reloads/rebuilds embeddedBitmap and re-blits it via RedrawPreviewIntoOwnerIconMaybe below
    // (0x428400). `pEntry` is a SavedFileEntry*, or 0 to just refresh the CURRENT contents. If
    // pEntry==0 or embeddedBitmap already holds real pixel data, first resets to an 8x8
    // placeholder. Then, if pEntry!=0: for the backdrop category (nCategory==5), loads
    // "<install>backdrop\<pEntry's own filename>" fresh off disk into embeddedBitmap at the
    // world board's own column/row dimensions (a full-board-sized backdrop preview); for every
    // other category, mirrors pEntry's own embeddedThumbnailBmp (bailing out if it isn't loaded
    // yet) into embeddedBitmap at the thumbnail's own decoded width/height.
    void ReloadBackdropPreview(SavedFileEntry *pEntry);
    // Called only from ReloadBackdropPreview, right after it (re)builds embeddedBitmap: blits
    // it into the fixed preview panel (15,26 .. 142,121) onto the widget's own realized icon
    // canvas (pKindDesc->pOwnedObjA) -- scaled 2x for savegame-thumbnail widths (50/64), unscaled
    // for backdrop-preview widths (72/80); any other width (e.g. ReloadBackdropPreview's own
    // 8x8 "no thumbnail" fallback) skips the blit outright. Always dirty-marks this widget's own
    // rect afterward regardless of whether a blit happened. 0x428550.
    void RedrawPreviewIntoOwnerIconMaybe();

    // Activates category tab param_2 (0-5; stores the triggering menu node into the base's
    // pLastActivatedNode first). 0 = reset to the "current save" overview and close the
    // tool menu; 1 = the combined overview tab itself; 2-5 = the 4 individually-numbered
    // category tabs. Scans/repopulates the file list (EnumerateFiles) and relocates the
    // selection to keep it sorted (RelocateSavegameSelection + the search helper below).
    // Not yet transcribed.
    unsigned char ActivateTab(MenuNodeObj0x477568 *param_1, unsigned short param_2);
    // 0x427580 (ex-FUN_00427580, named 2026-07-26), extern -- not yet transcribed. The exact
    // structural twin of BuildToolButton::InitMenuIconsMaybe, for this widget's own tab bar:
    // loads the 0x2c00/0x2c01 descriptors, then (first call only) walks TileKind ids
    // 0x2c00..0x2c13 building a menu icon node per available one, caching the load/save/
    // reset/delete/scroll/select nodes into its own named fields. Called by
    // BuildToolButton::InitMenuIconsMaybe on its embedded regionBMaybe.
    unsigned char InitMenuIconsMaybe();
    // vtable slot 21 -- click handler for the tab-switch command nodes: guards on the node's
    // own wSelIndexMaybe, toggles its hover state, then maps the clicked node's resourceId onto
    // an ActivateTab category (0x2c02/0x2c03/0x2c05 -> categories 2/3/4) and re-invokes it
    // with the base's own pLastActivatedNode; 0x2c04 instead resets straight to category 0
    // (passing the clicked node itself, not pLastActivatedNode) and skips the trailing
    // BuildToolButton reposition call below.
    // ⚠ REALLY vtable slots 21 and 22 (the vtable dwords at 0x477d1c/0x477d20 are 0x428af0 and
    // 0x428ba0), and as of v533 they are DECLARED virtual, which is what the slot-20 override
    // below needs: it reaches both through `call [vtbl+0x54]` / `[vtbl+0x58]`, and a vtable
    // DISPLACEMENT is not a relocation -- the slot INDEX has to be right in our model too, not
    // just the target. They land at 21/22 because WidgetBaseObj0x4784c8's chain ends at slot 20
    // and these are this class's first two NEW virtuals, in this declaration order. Do not
    // insert another new virtual between them or above them.
    virtual unsigned char HandleTabSwitchMenuNode(MenuNodeObj0x477568 *param_1);
    // Moves the current selection to list position param_1 (0x429850), repopulating the 6
    // visible slot nodes plus the scroll-up/scroll-down arrows and current-slot node.
    void RelocateSavegameSelection(SavedFileEntry *param_1);
    // Scans the savegame/backdrop directory (glob "savegame\*.sav"/"backdrop\*.bmp" under
    // g_pInstallPathPrefix, category 5 = backdrop) and rebuilds the sorted SavedFileEntry
    // list from scratch: clears the existing list, then re-populates via a local sentinel-head
    // insertion sort (case-insensitive by szShortName, the _splitpath'd filename-without-
    // extension) over every non-dotfile _findfirst/_findnext match. Falls back to
    // CreateDirectoryA(installPath + "savegame") if the glob finds nothing at all (fresh
    // install, directory doesn't exist yet). 0x429490.
    unsigned char EnumerateFiles();
    // Called only from HandleSavegameMenuNode (slot 22) with some OTHER visible slot node
    // than the current selection (0x428f90). If that node's label differs from the current
    // selection's and it's currently hover-highlighted (state 2), demotes it back to state 1
    // (and clears the backdrop preview if it was the one being shown). If the labels are
    // EQUAL and the node is in the plain state 1, promotes it to state 2 (selected/
    // highlighted) and reloads the backdrop preview from its entry.
    void PromoteOrSelectSaveEntry(UiIconListItem *param_1);
    // vtable slot 22 -- click/command handler for a savegame-slot menu node (0x428ba0).
    // Dispatches on the node's own resourceId: 0x2c02 Load (FUN_00429a10), 0x2c03 Save
    // (builds "savegame\\<label>.sav" into szPendingSavePath, prompts overwrite via
    // BuildToolCursorWnd::ShowTool(6,0) if that name collides with szActiveSavePath or an
    // existing list entry, else commits via FUN_00429b20), 0x2c04 reset to the overview tab
    // (ActivateTab(param_1,0)), 0x2c05 delete-confirm (ShowTool(7,0)), 0x2c07/0x2c08
    // scroll up/down (RelocateSavegameSelection to pCurrentEntry->pPrev/
    // pNext, then re-highlights the node if the cursor is still over it post-scroll),
    // 0x2c09 promote/select (PromoteOrSelectSaveEntry), 0x2c0c commit-to-current-save
    // (ReloadActiveSaveState). All paths return 1 unconditionally (confirmed: every one of the
    // function's ~8 return/epilogue sites sets AL=1, no path ever returns 0).
    virtual unsigned char HandleSavegameMenuNode(UiIconListItem *param_1);
    // Real vtable slot 20 override (0x428a80, Ghidra: RouteMenuNodeTestByCategoryMaybe) -- the
    // family's "execute the command on this node" half, which this class uses purely as a
    // ROUTER: decrement the node's own wSelIndexMaybe if it is not already the 0xffff sentinel,
    // then hand the node to the tab-switch handler (category 1) or the savegame-slot handler
    // (categories 2-5), and in every case tick the node's animation afterwards. Always returns
    // 0 -- unlike its two callees, this class never reports a command consumed through slot 20.
    // Body in src/WidgetPicker.cpp.
    virtual char HandleMenuCommandMaybe(MenuNodeObj0x477568 *pNode);
    // vtable slot 16 -- keyboard handler (0x4290a0). Offers the key to the base first and
    // returns its verdict if it consumed it; otherwise only the four per-category list tabs
    // (nCategory 2-5) do anything at all -- the overview categories 0/1 always report the key
    // unconsumed. VK_UP/VK_DOWN just "press" the matching scroll arrow node (state 1 -> 2 plus
    // the wSelIndexMaybe=6 keyboard-focus marker), leaving the actual scroll to the arrow's own
    // command handler, and are always reported consumed. Every OTHER key is typed straight into
    // the current slot's editable label (UiIconListItem::HandleTextEditKey) and then the list is
    // re-scrolled so the edited name stays sorted-visible; VK_BACK/VK_DELETE report whatever
    // HandleTextEditKey said (i.e. 0 when the label isn't editable), all other keys report
    // consumed unconditionally.
    virtual bool OnKeyDownMaybe(unsigned int nKey);
    // Real vtable slot 10 override (0x4282b0, Ghidra: TickMenuNodesAndIconMaybe) -- the
    // family-wide per-frame "Tick" slot, overriding AnimDescRefObj0x477488::
    // AdvanceAnimFrameMaybe. Walks pMenuListHead dispatching each node through this widget's
    // own slot 19 (hit-test-shaped, short-circuited once any node reports a hit) and slot 20
    // (tick-shaped execute half), then, when the cursor is over the embedded icon or NOT over
    // the widget's own rect, drives the icon's hover animation (slot 7 with 0 while
    // nSubFrame is in [1,5), then the icon's own slot-10 tick unconditionally).
    virtual void AdvanceAnimFrameMaybe();
    // Real vtable slot 11 override (0x428380, Ghidra: PropagateDirtyRegionMaybe) -- chains
    // WidgetBaseObj0x4784c8's own slot-11 blit, then, ONLY on the category-1 overview tab,
    // repaints the embedded icon over the same clip rect with a zeroed flags word (the other
    // categories fill that area with the slot list instead). Body in src/WidgetPicker.cpp.
    virtual void BlitAnimFrameMaybe(RECT rect, char flag, unsigned int flags);
    // Real vtable slot 15 override (0x427520) of WidgetBaseObj0x4784c8::ClearOwned -- an
    // OVERRIDE of an existing slot, so it adds no new slot and the "first two NEW virtuals"
    // ordering note above is unaffected. Deletes the whole pLinkedListHead chain of
    // SavedFileEntry records, clears this widget's own descriptor AND the embedded icon's, then
    // chains the base body (0x454630). Body in src/WidgetPicker.cpp.
    virtual void ClearOwned();
    // Reloads the "current save" state from a chosen slot's label (0x429ef0, __thiscall, not
    // yet this-typed in Ghidra) -- called by HandleSavegameMenuNode's 0x2c0c command.
    // Not yet transcribed.
    void ReloadActiveSaveState(char *param_1);
    // Called only from HandleSavegameMenuNode's 0x2c02 (Load) case (0x429a10). Builds
    // "savegame\\<slotLabel>.sav" into szPendingSavePath (same 3-piece strcpy/strcat shape
    // as the 0x2c03 Save case) then hands it to
    // NetSessionEventQueue::PlaceEdgeLinksAndFlush -- medium confidence this IS the
    // real load path (the disk read isn't visible in this function's own body, so the actual
    // deserialize happens inside PlaceEdgeLinksAndFlush, not yet transcribed elsewhere).
    void LoadActiveSlot();
    // Called only from HandleSavegameMenuNode's 0x2c03 (Save) case (0x429b20), after the
    // overwrite-collision check already built szPendingSavePath. Confirmed genuine
    // save-commit: tears down/frees the whole pLinkedListHead list, calls
    // NetSessionEventQueue::SaveBoardLayout (builds a local ThumbnailBmp + writes game state
    // via ThumbnailBmp::ThumbnailBmp_Save), re-enumerates the directory
    // (EnumerateFiles), then relocates the "current entry" cursor to the freshly-saved
    // slot's neighbor in the newly-rebuilt sorted list.
    void SaveActiveSlot();
    // Called from AppWndProc (0x462b91) once the delete confirmation armed by
    // HandleSavegameMenuNode's 0x2c05 case (ShowTool(7,0)) is answered (0x429dd0). Builds
    // "<install>savegame\<slotLabel>.sav" into szPendingSavePath (a FOUR-piece
    // strcpy(install-prefix)/strcat shape, unlike Load/SaveActiveSlot's 3-piece relative
    // form), tears down/frees the whole pLinkedListHead list (same loop as SaveActiveSlot),
    // DeleteFileA's it, then re-enumerates. On failure just calls GetLastError (discarded)
    // and bails; on success clears the current slot's label and relocates the selection.
    void DeleteActiveSlot();
};
