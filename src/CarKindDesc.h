// CarKindDesc -- the TRAIN/CAR kind descriptor (vtable 0x477610, ctor 0x40e600, Load 0x40e690).
// Kept in its OWN header, not in src/CursorDesc.h where its CursorDesc base and its
// Obj0x4779e0/BigObj siblings live: adding it there rotates src/Obj0x4779e0.cpp's and
// src/DPlaySessionMgr.cpp's /Og TU state and costs each of them one EXACT match (measured this
// session -- 7+1/12 -> 6+2/12 and 39+25/64 -> 38+26/64, 655 B). Exactly the same shared-header
// churn hazard, and the same fix, as src/TimeOfDayMaybe.h's own v331 note.
#pragma once

#include "CursorDesc.h"   // CursorDesc -- the direct base

// CarKindDesc -- RENAMED v482 from the address-based name `Obj0x477610` (grep that if you are
// following an older note). Every identified thing in its 0x644-byte tail is car/train data:
// LoadHeadingOffsetTablesMaybe reads the shared "trains\\train.dat" resource into the two
// per-heading {dx,dy} tables, and wCarIdAMaybe/wCarIdBMaybe are the kind's first/last
// selectable car ids. No `Maybe`: the content is certain. The one thing still unexplained is
// the BREADTH of the id space the factory hands to this class (see below) -- that is a
// question about the factory, not about what this class holds.
// The TRAIN/CAR kind descriptor -- a SIBLING of Obj0x4779e0/BigObj, not a derived class: it
// extends CursorDesc directly with its own 0x644-byte tail and is 0x7ac bytes. The descriptor
// factory (0x446840) picks it for every EVEN kind id below 0x1866 and for every id at or above
// it, allocating `operator new(0x7ac)` and running its own ctor at 0x40e600 (vtable 0x477610,
// hence the name -- same vtable-identified-class convention as Obj0x4779e0/TimeOfDayMaybe); the
// ODD ids below 0x1866 get a plain 0x168-byte CursorDesc instead. Train kinds are 0x1804/0x1806/
// 0x1808 (all even), so a CarNetObj's inherited AnimDescRefObj0x477488::pKindDesc -- declared
// `BigObj *` in src/WidgetBase.h because most consumers are BigObj kinds -- really points at
// one of THESE for a train car, and reaching the tail needs a cast.
// DONE (v407): src/PeerTrainNode.cpp's SettleClaimedSocketMaybe/RepositionForHeadingMaybe used
// to reach this class's +0x168 table through BigObj's `aFootprintOccupancyMask +
// heading*4 -6/-4` (0x16e - 6 == 0x168), which read as an unaligned load into the wrong class.
// All eight reads now go through aHeadingOffsetTableMaybe[h*2]/[h*2+1] via an explicit
// `(CarKindDesc *)pKindDesc` cast. The feared codegen risk did not materialise: the fold was
// byte-neutral in both functions (0x40d8e0 was and remains an exact MATCH).
class istream; // <fstream.h>/<strstrea.h> -- same forward-declaration precedent as CursorDesc.h

// ⚠ MEASURED PRICE of the method block below (2026-07-27, when src/CarKindDesc.cpp landed):
// declaring BOTH `ParseTokenField` and `LoadMaybe` rotates src/PeerTrainNode.cpp's /Og state and
// turns CarNetObjAnchorPartial::CheckCarClearedDepotMaybe (0x40e520) from EXACT into DIFF(124),
// -220 B. It is the PAIR, not either one alone -- {dtor}, {PTF, LHOT} and {LHOT, LM} are all
// free, and all three declaration ORDERS of the trio were probed and make no difference. The
// virtual destructor and the data-model corrections below are free. Accepted deliberately: the
// TU it unlocks is +1219 B, so the block nets +999 B. Do NOT "fix" this by moving the
// declarations into a TU-local view struct -- that is the lint_alias.py hazard class.
class CarKindDesc : public CursorDesc {
public:
    // 0x40e600 -- src/CarKindDesc.cpp. Declared HERE, on the real class, rather than on the
    // TU-local ctor view it used to live on: UIResources::TileKind_CreateDescriptor `new`s this
    // tier, so against a TU-local view every one of those calls bound to a generated
    // do-nothing stub instead of to the body in this class's own TU, and the car descriptors
    // came back unconstructed. Same defect class -- and same fix -- as CursorDesc's own ctor
    // (CODEGEN #161). Measured free: it costs neither this header's consumers nor its own TU.
    CarKindDesc(unsigned int kindId, char *pszDefinition);
    // 0x40e680 / 0x40e660 (??_GCarKindDesc) -- src/CarKindDesc.cpp. Two separate COMDATs in the
    // original (??_G calls ??1), so the body is defined out of line, not in the class.
    virtual ~CarKindDesc();
    // vtable slot 3 (+0xc) override -- 0x40e8d0, src/CarKindDesc.cpp. Reads this tier's own
    // 4-token header record off the stream and then unconditionally (re)loads the two heading
    // tables below. LoadMaybe dispatches through it virtually (`call [vtable+0xc]`).
    virtual unsigned char ParseTokenField(istream *pStream);
    // 0x40e950 -- zero-fills both tables and reads the SHARED "trains\train.dat" resource into
    // them (RF archive first, loose file second). Not a virtual: called directly from the
    // override above.
    unsigned char LoadHeadingOffsetTablesMaybe();
    // 0x40e690 -- really this tier's vtable slot 4 (+0x10) `Load(kindId, pszDefinition)`
    // override, declared NON-virtual for exactly the reason src/Obj0x478118.h documents:
    // CursorDesc::Load is deliberately mis-declared no-arg and widening it there rotates
    // src/Obj0x4779e0.cpp for a measured -489 B. `kindId` is unused by this override.
    void LoadMaybe(unsigned int kindId, char *pszDefinition);

    // +0x168 -- per-heading {dx,dy} offset pairs. 400 shorts, NOT the `short[320]` + 160 bytes
    // of padding an earlier session carried (Ghidra's own shape, never ground-truthed): the
    // zero-fill at the head of LoadHeadingOffsetTablesMaybe is `mov ecx,0xc8; rep stosd` at
    // +0x168 -- 200 dwords == 800 bytes == 400 shorts, running exactly to +0x488 where the
    // twin below starts. Layout-neutral correction (the two arrays plus the id pair still sum
    // to 0x7ac, this class's `operator new` size), but it makes the array bound real: the
    // loader fills 160 of the 200 pairs, so the tail 40 pairs are genuinely spare, not padding.
    short aHeadingOffsetTableMaybe[400];
    // +0x488 -- the opposite-end twin of the table above, same 400-short shape and same
    // 200-dword zero-fill; the loader interleaves both from one row of 4 values.
    short aOppositeEndHeadingOffsetTableMaybe[400];
    // +0x7a8/+0x7aa -- the kind's own car-id endpoint pair, parsed straight out of the .dat
    // stream by this class's Load (0x40e690, which also zeroes both). PeerTrainNode's ctor
    // copies them into wSelectedCarIdAMaybe/wSelectedCarIdBMaybe (and latches A as the initial
    // wSelectedCarId), and PeerTrainNode_UpdateSelectedCar toggles the train's selection
    // between exactly these two ids -- so they are the first/last selectable car of the kind.
    // UNSIGNED, pinned by the parse site: ParseTokenField reads both through
    // `??5istream@@QAEAAV0@AAG@Z` (operator>>(unsigned short&), 0x464750), not the signed
    // `AAF` overload the heading tables above use.
    unsigned short wCarIdAMaybe;
    unsigned short wCarIdBMaybe;
};

// 0x40eb60 -- map a car/train kind id onto the small 0-4 category enum a CarNetObj carries in
// nCarCategory. A pure switch over the same id space this file's descriptor class covers, and
// the LAST function of this .obj's address range (0x40eb60..0x40eb97, immediately before
// src/DSoundChannel.cpp's first COMDAT), which is what puts its home here rather than beside
// its callers. Body in src/CarKindDesc.cpp. Three callers: CarNetObj::SetCarTypeAndCategory
// (src/CarNetObj.cpp) and two PeerTrainRoster train-spawn sites (0x44de2f, 0x44df01).
//   1 = the three LOCOMOTIVE kinds (0x1804/0x1806/0x1808 -- the even train ids)
//   2 = 0x1866/0x1868/0x186a,  3 = 0x186c/0x186e,  4 = 0x1870/0x1871 (the hand-off pair)
//   0 = everything else
int MapCarTypeIdToCategoryMaybe(int nCarTypeId);
