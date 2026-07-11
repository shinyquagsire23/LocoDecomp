// NetResource free-function leaves -- helpers of the missing-appearance/appearance-request
// network path (GameNetThreadState::NetResource_RequestMissingAppearances, 0x438e40 -- transcribed
// EXACT in src/GameNet.cpp as of v467; the "0x438f80" this comment used to cite is an address
// INSIDE that one function's body, not a separate function). Moved out of src/phase2_probe2.cpp
// 2026-07-22 (v322).

// --- 0x445910: free function, byte bit-combine (`__stdcall` -- RET 0x8 pops both args) ---
// Packs a (kind, subkind) appearance pair into one DecalSlot::packedKind byte,
// `(nSubkind-1) | (nKind<<3)` -- the exact inverse of the unpack done by
// PostBag_BuildClipartFilePath and ClipartBitmapCache_GetOrLoad. Used for the
// missing-appearance request list (both callers inside
// GameNetThreadState::NetResource_RequestMissingAppearances, src/GameNet.cpp -- which calls it
// through the PostBagCacheBundle member declared in src/PostBag.h, because both call sites load
// ecx=g_pPostBagCache; this body never reads `this`, which is why the free spelling byte-matches).
// EXACT since v352 (was parked from v2 because the original stays pure 8-bit -- AL/CL, no
// widening -- while every attempt widened). Changing the PARAM types was the wrong lever:
// `char` and `unsigned char` params both still promote to `int` inside a single
// `return (nSubkind - 1) | (nKind << 3);` expression, so the widening came from the EXPRESSION,
// not the signature. Accumulating into a `char` LOCAL instead is what keeps VC5 in 8-bit --
// it narrows each step to the destination's width. The statement split also fixes the
// operand order for free: the original loads nSubkind first (`[esp+8]` before `[esp+4]`)
// because `nSubkind - 1` is the first statement.
// FUNCTION: LOCO 0x445910
unsigned char __stdcall PostBag_PackDecalKind(char nKind, char nSubkind) {
    char r = nSubkind - 1;
    r |= nKind << 3;
    return r;
}
