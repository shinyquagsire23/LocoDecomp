// BigObjSeqRecordMaybe -- the 0x34-byte record the InsertSeq / MobileSeq / TotalVisits .dat
// keywords each parse into. Three of them sit back-to-back in the Obj0x4779e0 field block, at
// +0x55c, +0x590 and +0x5c4 (the last one ends at +0x5f7, exactly where lEEReplayDelay
// begins -- zero slack, which is what pins the 0x34 size).
//
// Layout ground-truthed by Obj0x4779e0::ParseSeqRecordMaybe (0x41f2b0, EXACT --
// src/Obj0x4779e0.cpp), which reads the whole thing token by token; the effect tail
// (+0x20..+0x33) is pinned independently by its consumer TilePlacedObj::SpawnSeqRecordEffectMaybe
// (0x4588b0) and the +0xc/+0x10 pair by TilePlacedObj::ApplySeqRecordChangeMaybe (0x458820).
//
// Reads as one goal-and-reward rule: a threshold plus a list of qualifying tile kinds, then
// what to do when the goal is met -- retarget the object to another descriptor (or just poke
// its anim), and spawn an effect at a point in one of three coordinate spaces.
//
// ⚠ +0x10 / +0x18 / +0x24 were modeled as `float` until 2026-07-25 and are really SIGNED
// SHORTS: the parser reaches them through 0x464bc0, which is
// `istream::operator>>(short &)` -- it clamps to [-0x8000, 0x7fff] and stores a WORD. The
// byte-match could never have caught the difference, because tools/match.py MASKS the call's
// relocation, so `>> (float &)` and `>> (short &)` compare byte-identical. The consumers settle
// it from the other side: 0x458820 reads +0x10 with `movsx ecx, WORD PTR [ecx+0x10]` and
// 0x4588b0 reads +0x24 with `mov ax, WORD PTR [eax+0x24]`. Each short is followed by 2 bytes
// the parser never writes.
//
// NOTE: src/CursorDesc.h deliberately still spells all three record spans as FLAT SCALARS
// rather than as members of this type -- a named seq-record struct in that shared header
// rotates DPlaySessionMgr.cpp's TU codegen and breaks SelectGridCellFromPointMaybe's EXACT
// (v331 bisect). Folding them in wants its own measured session.
#pragma once

struct BigObjSeqRecordMaybe {
    unsigned int ulUnk0x0;   // +0x00 -- lead ulong; the goal THRESHOLD (the caller at 0x434f45
                             //          compares a running count against it)
    unsigned int ulValueCount; // +0x04 -- element count, capped at 0x2d by the parser
    int *paValues;           // +0x08 -- owned `new long[ulValueCount]` array of tile kind ids
    int lTileIdA0xcMaybe;    // +0x0c -- retarget descriptor id; -1 = record inactive,
                             //          0 = poke the anim instead of retargeting.
                             //          Parser forces -1 unless its category is 2/4/0xc/0xd.
    short wSubFrameAMaybe;   // +0x10 -- sub-frame passed alongside lTileIdA0xcMaybe
    unsigned char pad0x12[2]; // +0x12 -- never written by the parser
    int lTileIdB0x14Maybe;   // +0x14 -- parser forces -1 unless its category is 7 (a minifig)
    short wUnk0x18Maybe;     // +0x18 -- the +0x14 id's counterpart to wSubFrameAMaybe; no
                             //          consumer read yet
    unsigned char pad0x1a[2]; // +0x1a -- never written by the parser
    int lUnk0x1cMaybe;       // +0x1c
    int lEffectKindIdMaybe;  // +0x20 -- EffectSpawner kind id; <= 0 means "no effect".
                             //          Load() inits it to -1.
    short wEffectMobilityMaybe; // +0x24 -- the effect's mobility-flag word
    unsigned char pad0x26[2];   // +0x26 -- never written by the parser
    // +0x28 -- a char, stored SIGN-EXTENDED into the full dword. Doubles as the effect's
    // coordinate space and its direction code: 'S' = scroll-relative, 'W' = absolute world,
    // and anything else = relative to the owning object's own rect.left/top. 'U' and 'D' are
    // additionally passed through to the spawner as the direction char; every other value
    // spawns as 'W'.
    int lSpaceCharMaybe;     // +0x28
    int lEffectXMaybe;       // +0x2c
    int lEffectYMaybe;       // +0x30
};
