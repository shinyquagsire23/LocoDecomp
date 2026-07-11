
## ⛔ READ FIRST — the toolchain changed in v358, and it invalidates old numbers

**`Loco.exe` was built with VC++ 5.0 *Service Pack 3*, not RTM.** `toolchain/bin/cl` now
defaults to `toolchain/vc50sp3`. Two independent proofs and the repo-wide A/B live in
`toolchain/README.md`; the short version is that SP3's `link.exe` stamps **5.10.7303**, exactly
the `linker 5.10` in the PE header, and RTM's `c2` emits the **wrong 3-byte code-alignment
filler** as a whole-image constant (`nop; mov edi,edi` where every one of the shipped image's
35 three-byte pads is `lea ecx,[ecx+0]`). Switching cost nothing and gained 9 byte-matches.

**Consequences for this document and for `docs/PARKED.md`:**
- **Every `// EFFECTIVE` autopsy and every PARKED row written before v358 was measured against
  the WRONG compiler.** Their diff counts, and often their conclusions, are stale. Re-measure
  before believing any of them — several collapsed outright (0x458c90 986 → 5 raw diffs,
  0x447400 863 → 12, 0x4216f0 823 → 48).
- **"Not source-steerable" verdicts from before v358 are the least trustworthy of all**, because
  a residual that was really a toolchain constant reads exactly like an allocator coin-flip.
  Two v358 re-tries of explicitly "tried, zero effect" parks succeeded immediately
  (`WindowBase::Create` 0x425b70 and `WorldActionCursor` 0x458c90, +1700 bytes between them).
- **Score a toolchain hypothesis REPO-WIDE, never on one symptom.** v330 A/B'd SP3 against a
  single sparse-switch residual, found it byte-identical there, and correctly-but-narrowly
  concluded "SP3 is not a fix" — which then read as "SP3 is not the toolchain" for 28 sessions.
  This is the same discipline v353 already established for the flag question.
- A 3-byte interior pad only exists when the function's code length leaves a 3-byte gap before
  an interior jump table, so the RTM defect was invisible in most functions and fatal in a few.
  **Any residual that is purely alignment filler is a toolchain fact, not a source fact.**

## ⭐ KEY VC++ x86 byte-matching lessons (inherited from Yodecomp, proven on MSVC 4.2)

Full text: `~/workspace/Yodecomp/PLAN_COMPLETED.md` "KEY codegen lessons" #1–#33 (cite as
"Yoda lesson #N"). All proven on cl 10.20; VC5's optimizer is a close descendant — expect
most to hold, re-verify each the first time it matters here, and note VC5
confirmations/refutations in this list.

- **#1 COMDATs:** each C++ function → its own `.text` COMDAT with `/Gy` (default-on for C++).
- **Inlining is `/Ob1`, and that is a REAL, permanent asymmetry — an original call site that
  inlines a function you also byte-match standalone cannot be reproduced from one TU.** VC5's
  `/O2` implies `/Ob1`: ONLY explicitly-`inline`/in-class functions are inlined, so a plain
  out-of-class member definition always emits a `call`, no matter how small. When the original's
  disasm shows a known function's body pasted in (the tell is a DEAD guard the standalone body
  needs and the call site cannot: `lea edx,[esp+N] / test edx,edx / je` — a null test on a stack
  address, from an inlined `if (pOut != 0)`), you get exactly two options and BOTH were measured
  in v410 on `TilePlacedObj::SpawnOwnedActorMaybe` (0x458430) inlining `GetFootprintRectMaybe`:
  (a) mark the definition `inline` — it DOES inline at the call site (+74 B of the gap closed),
  but cl then stops emitting the out-of-line COMDAT entirely and the standalone function's own
  99-byte EXACT disappears from the TU; (b) `/O2 /Ob2` — gives BOTH the inline AND a
  byte-identical standalone COMDAT, but is **refuted repo-wide**: 87077 B / 382 funcs against
  `/O2`'s 89321 / 395 (losing matches across DPlaySessionMgr, EditCardWnd, GameNet, MenuNode,
  PeerTrainNode, PopupWndBase, TutorialWnd, WidgetBase; only ScopedTimestampMaybe and
  ThreadWrapper gain). `/O2` — i.e. `/Ob1` — stands, Phase 0 unchanged. The consistent reading
  of (a)+(b) is that the ORIGINAL declared such a function `inline` and its standalone COMDAT
  came from a DIFFERENT TU's copy that the linker kept — so the right fix is to MOVE the marker,
  not to change flags. `tools/cc.sh` now honours `$LOCO_OPT` (default `/O2`) the same way
  `progress.py` does, so this class of hypothesis is one command to test on a single TU.
- **The INVERSE asymmetry — the original makes a real `call` where your TU inlines — is fixed by
  `#pragma inline_depth(0)` bracketed with `#pragma inline_depth(8)`** (v518,
  `GameNetThreadState::GameNetThreadState` 0x438bc0 calling the in-class-inline
  `GNetManager::ProbeComPort`): the pragma suppresses ALL inline expansion inside the bracketed
  function, the call becomes a real `call`, and the TU then also EMITS the out-of-line COMDAT
  (0x45ee60, 93 B, byte-EXACT on first compile). VC5 inlines with whole-TU visibility, so the
  in-class body must ALSO leave the shared header: declare-only in the header, out-of-class
  `inline` definition in the .cpp (placing it after the caller in the TU does NOT help — the
  inliner sees it anyway). Refuted for this job: `#pragma auto_inline(off)` (only affects /Ob2
  auto-inlining), argument spelling, buffer size, call-context shape. ⚠ Place the `#pragma`
  BEFORE the function's `// FUNCTION: LOCO` marker, never between the marker comment and the
  body: lint_ghidra_sync derives the marker's source name from the first code line after the
  comment block and reports a phantom `src=inline_depth` MISMATCH otherwise.
- **A shared header's declaration COUNT is a threshold, not a per-declaration cost.** v410:
  six new declarations on `TilePlacedObj` re-rolled two documented register coin-flips in TUs
  three includes away (`RoadVehicleActor::CanStandAtMaybe` 504 B, DIFF 13;
  `CarNetObjAnchorPartial::CheckCarEnteredDepotMaybe` 220 B, DIFF 15). Bisecting by removing
  ONE declaration at a time found nothing — every single-removal still scored the loss. Removing
  ALL SIX restored both, and so did removing any five, so the trip point is at one or two
  declarations. **Bisect a declaration-set regression by COUNT (binary-search how many you can
  keep), not by identity (which one is guilty) — the identity search reports "not this one" for
  every candidate and reads as a refutation of the whole hypothesis.**
- **#2 Comparisons are emitted literally** — `v >= 0x5b` (`CMP 0x5b;JL`) ≠ `v > 0x5a`
  (`CMP 0x5a;JLE`). Mirror the exact operator/constant from the DISASM, not Ghidra's
  normalized form. Re-confirmed on VC5 in v364 (`0x4143e0`, WM_TIMER frame-count guard):
  `count < 2` emits `cmp WORD PTR [edx+0x160],2 / jae`, `count <= 1` emits
  `cmp ...,1 / jbe`. Identical predicate on integers, different constant AND different
  mnemonic — VC5 does NOT canonicalize, so the `jbe`-vs-`jae` mismatch in an `asmscore
  --dump` row is a source-spelling bug you can fix, not scheduling noise. **Read the
  polarity carefully: `jbe`'s inverse is `ja`, not `jae`** — an orig-`jbe`/mine-`jae` pair
  means the CONSTANT differs, whereas orig-`jbe`/mine-`ja` would mean only the arm order does.
- **#3 Return shape:** result-through-variable to a single `return` ≠ per-branch
  `return CONST` (`mov reg,VAL; mov eax,reg` vs `mov eax,VAL`).
- **#4 Stores before calls survive** (callee might read `this`/globals).
- **#5 x87/register allocation depends on LOCAL DECLARATION ORDER** — reorder locals to match.
- **#5b For a small struct local that is only ever passed BY VALUE, the order of its FIELD
  assignments is load-bearing too — it decides which parameter lands in which register (v397,
  `TutorialWnd::HitTestControl`/0x451e90).** `POINT pt; pt.x = x; pt.y = y;` and `pt.y = y;
  pt.x = x;` give the same perfectly-aligned 126/126 instruction stream, but the first parks x
  in EDI and y in EBX while the original (and the second spelling) does the reverse — 18 bytes
  of pure register swap, and EXACT on the flip. ⚠ This does NOT contradict
  `PopupWndBase::RedrawSoftwareCursor`'s standing note that reordering ITS rect-field
  assignments is inert across three tried variants: that RECT has its address taken and is
  really materialized on the stack, so cl reschedules the stores freely. The distinction is
  whether the struct is ever materialized. A never-materialized by-value struct has no stores to
  reschedule — the assignment order IS the register assignment — so try the flip; a struct whose
  address is taken will ignore you.
- **#6-bis CMP operand order is a FREE last-two-bytes fix, worth trying in BOTH spellings
  before autopsying anything (v450, `PlacedObjRegistryMaybe::SortRangeMaybe`/0x435aa0).** That
  quicksort's partition exit sat at DIFF(2) with 84/84 identical instructions purely because the
  source said `if (j < i) break;` where the original says `if (i > j) break;` -- same predicate,
  opposite operand order, `cmp edi,ebx; jg` vs `cmp ebx,edi; jl`. One character of source; EXACT.
- **#6 CMP operand order** (`cmp a,b;jg` vs `cmp b,a;jl`) is internal instruction selection,
  often NOT source-steerable — otherwise-identical + swapped operands + inverted jcc = park
  material. Verify claimed matches by direct disasm diff, not only match.py.
- **#6b …BUT when one operand is a REGISTER-resident local and the other a MEMORY operand, the
  source's own operand order decides it, and it is a free fix (v420, `EditCardWnd::OnMouseMove`
  /0x41ce50, EXACT).** `pt.y > rectUnk0x288.bottom` emits `cmp ebx,[esi+0x294] / jle` while
  `rectUnk0x288.bottom < pt.y` emits the mirrored `cmp [esi+0x294],ebx / jge` — same predicate,
  swapped operands, inverted jcc, exactly the shape #6 says to park. The distinction is whether
  BOTH sides are interchangeable: with two registers cl picks freely (#6 stands), but with one
  register and one memory operand the register side is whichever the source wrote FIRST. So on a
  bounds test against struct fields, always mirror the disasm's own left/right before parking it
  — check `cmp reg,[mem]` vs `cmp [mem],reg` in the `--dump` row, and put the register-resident
  side (a local, a parameter, a `POINT` member) on the side the original has it.
- **#7 Register allocation is TU-context-dependent (the big one).** Identical source gets
  different allocations by position in the `.obj`. Context-sensitive functions need the whole
  TU in source order; simple leaves are context-insensitive and match piecemeal. **Corollary
  (v331, 0x41e9f0): a contiguous address cluster can be its OWN compile unit** — placing
  0x41e9f0 in the neighboring `CursorDesc.cpp` left a TU-position /Og residual that vanished
  when the function moved to a fresh TU (`src/Obj0x4779e0.cpp`, the 0x41e600–0x41f2b0
  cluster). When a transcription is structurally right but residually off, probe a minimal
  fresh TU before hunting source constructs.
- **VC5 istream state + grid-fill idioms (v331, `Obj0x4779e0::ParseTokenField`/0x41e9f0).**
  `!pStream->eof() && !pStream->fail()` lowers to ONE `test byte [ios+8],7` — write the two
  state calls, not a manual mask. A 2-D grid fill only matches as ARRAY INDEXING
  (`a[r + c*stride] = …` — strength reduction builds the spilled base + stride-N inner
  pointer); every explicit-pointer formulation lowers differently. Unsigned-char count
  guards are `> 0` (`jbe`), clamps `(unsigned char)v > N` (byte compares).
  **Additions (v332, same TU):** numeric stream reads are `unsigned short` temps
  (`istream_ExtractUShort`) even for byte-typed fields — and reusing ONE temp (the loop
  read reuses the first dim temp) is what pins the frame size + param-slot coalescing; a
  separate loop temp costs a frame slot. A chained `*pStream >> a >> b >> c` emits
  right-to-left batched arg pre-pushes + a bare `mov ecx,eax` call chain; separate
  statements emit serial `push`/`call` with ecx reloaded each time — match the form to the
  original's shape. A count guard written `!= 0` compiles `je`, the original's `> 0`
  compiles `jbe` (unsigned) — one-byte tell.
- **Don't hoist a common trailing store out of if/else branches (v332, 0x41f0c0).** VC5
  keeps a statement written in both branches duplicated (the original has per-branch
  copies); hoisting it shrinks the function and mismatches. Write the duplication.
- **`-1`-filling a short table is `memset(arr, 0xff, sizeof arr)` (v332, 0x41e6e0),** not
  per-element stores — VC5's inline expansion emits exactly the original's merged
  dword+dword+word stores. A record-span `memset` with a magic size trips the idiom lint —
  give the span a TU-local struct and use its `sizeof`.
- **VC5 iostream ctor/dtor shapes (v332):** `ifstream`/`istrstream` ctors take a hidden
  most-derived flag (a pushed literal 1) since `ios` is a virtual base —
  `ifstream f;`/`new istrstream(buf, len)` both push it. `f.open(name, ios::nocreate)`
  (0x20) with the default prot arg loads `filebuf::openprot` from its `.rdata` static.
  `is_open()` inlines to the vbase-adjusted `[[obj+voff]+0x4c] != -1` fd check.
- **#8b Dtor for a class whose shared header must stay scalar (v332, 0x41e620):** model it
  on a SECOND TU-local layout-identical view with the embeds TYPED (`Obj0x4779e0DtorModel`)
  — only real typed members reproduce the compiler's member-then-base destruction chain
  with its EH unwind states (body at state N, then per-member states down to base at -1);
  explicit `->~T()` calls on raw fields emit no SEH frame at all.
- **#8 The TU-phase dial is driven by EMITTED CODE + called-signature SHAPES,** not bare
  decls — inert declarations don't rotate a TU. **Caveat (v325):** not always inert —
  adding 3 extern METHOD declarations to `src/PeerTrainNode.h` rotated
  DPlaySessionMgr.cpp's TU and broke `SelectGridCellFromPointMaybe`'s EXACT (bisect-proven;
  fields in the same header were inert). Mitigation that worked: a layout-identical
  SUBCLASS view carrying the new decls in the consumer's own header
  (`PeerTrainNodeTrackPartial` in `src/NameAnchorMaybe.h`), leaving the shared header
  untouched for older TUs. **Re-confirmed v329 on `src/WidgetBase.h`:** adding the
  `AnimDescRefObj0x477488::RepositionWithHotspot` override decl there rotated
  DPlaySessionMgr.cpp the same way (same victim function, bisect-proven again) — fix was
  the same subclass-view pattern (`AnimDescRefHotspotPartial` in `src/AnimEffectObj.h`,
  with an inlined forwarding ctor so the class-qualified direct call keeps its shape).
  **v331 refinement (`src/CursorDesc.h`, same victim 0x40aba0, all single-variable bisects):**
  THREE independent perturbation sources, not just method decls — (1) any method decl WITH
  PARAMETERS on the class (a parameterless method or a new virtual slot is inert), (2) a
  bare `#include` of a header declaring a POLYMORPHIC class (`TimeOfDayMaybe.h`), (3) a named
  struct definition (`BigObjSeqRecordMaybe`). Scalar fields remain inert. Fix that worked:
  keep the shared header all-scalar (flatten embeds/records to named longs + pads) and put
  the methods on a TU-local subclass view (`Obj0x4779e0ParsePartial` in
  `src/Obj0x4779e0.cpp`) — freed the victim AND kept the new function EXACT.
- **#8f A PARAMETER'S TYPE IDENTITY is a TU-phase perturbation all by itself, with zero
  semantic content — and that makes most "wins" from it NOISE, not evidence (v475,
  0x40cb10).** Promoting `src/CarNetObj.h`'s class from a 0x430-byte partial to the real
  0x450-byte `CarNetObj` changed one method's parameter from `CarNetObjModePartial *` to
  `CarNetObj *`. Same pointer, same offset 0, same layout, identical call-site bytes — and it
  cost an unrelated LATER function in the same TU (`NameAnchorMaybe::
  ExtendAcrossTileBoundaryMaybe`, which never touches `CarNetObj`) its EXACT match: 260 B,
  DIFF(111), **insns 98/99**, nearly every `--dump` row a bare `r` rename in a whole-function
  ebp/ebx/edi/esi permutation. ⭐ **Two decisive compiles nailed it:** re-declaring the
  parameter as an *empty* `struct X : CarNetObj {}` restores the match exactly, and does so
  **regardless of the type's name length** (tested with a 20-char name and a 10-char one), so
  it is neither the layout nor the mangled-symbol size — it is the type identity rotating
  `/Og` state that carries forward in source order.
  ⚠ **The trap is that this is a one-line, zero-risk-looking "fix" that recovers real bytes
  while encoding nothing true.** The original source named one class; an empty derived struct
  is not a thing a human wrote. Treat a recovery from this knob the way you would treat one
  from renaming a local: it is a coin-flip landing the other way, so **take the correct source
  and park the residual.** The legitimate use is diagnostic — it is the cheapest way to prove
  a residual is pure TU-phase carry rather than a source-shape error, since it changes nothing
  a human could see. Same family as #8/#8-caveat's declaration-count rotations, but strictly
  weaker in what it justifies: those at least trade one real declaration's placement.
- **#9 Loop rotation:** early-`return` `for` loops don't rotate — write the explicit
  `if (n > 0) { do {…} while (i < n); }`; hoist counts to locals.
- **#9b Top-of-body `INC`:** a loop whose original enters with the counter zeroed and INCs
  at the TOP of the body is a pre-incrementing `i = 0; while (i < n) { i++; … }`, not any
  `for` spelling (`for (i = 1; i <= n; i++)` initializes to 1, `for (i = 0; …; i++)`
  increments at the bottom). With the bound spelled as the loop condition, a per-iteration
  re-called `Count()` lands in the right place for free (0x434720, v517).
- **#9c The condition's first evaluation IS the guard:** when the original shows
  `call Count; test; jbe out` above the loop and `inc; call Count; cmp; jc back` at the
  bottom, that is a PLAIN `for`/`while` with the bound as the condition — do NOT add an
  explicit `if (Count() != 0)` guard around a do-while (the do-form peels/rotates: +74 B on
  0x4348a0, v519) and do NOT wrap a for in the guard either (it double-calls Count at the
  loop head: +42 B). The unguarded `for` fell out at the exact original length, and the only
  remaining dial was the gate literal: `<= 0` (emits `cmp 0; jg`) vs `< 1` (`cmp 1; jge`) —
  write the comparison constant the original compares against. (0x4348a0, DIFF(264) → DIFF(2)
  → EXACT.)
- **#10** `x <<= s; x &= m;` as separate statements never combine; a single expression
  canonicalizes mask-first.
- **#11 Switch, not if-ladder, for multi-way dispatches** — compares up front, out-of-line
  arms, per-case constant pushes cross-jumped into a shared call tail.
- **#13 Slot-vs-register residency:** original reloads a derived value from a STACK SLOT at
  every use ⇒ the source had NO local (repeated expression CSE'd to a slot); a local puts it
  in a register. Match the residency you see.
- **#13b The CONSTRUCTIVE twin of #13 — a member load HOISTED to the top of a block and SPILLED,
  then reloaded from that spill slot much later, means the source declared a LOCAL at the top of
  that block (v477, 0x40e160, DIFF(176) → EXACT in one compile).** Where #13 reads a stack slot
  as evidence of NO local, this reads one as evidence of one; the discriminator is WHERE the
  load happens. If the original loads `[this+0x40]` in the first few instructions of a block,
  parks it in a stack slot, and only dereferences it in the last few — while your version loads
  it directly at the point of use — the difference is not scheduling, it is that a human wrote
  `BigObj *pKind = this->pKindDesc;` as the block's first statement. cl loads an
  initialized-at-declaration local right there and then spills it under register pressure; an
  expression evaluated at the use site is loaded at the use site, always.
  ⚠ **The tell is easy to misread as pure register noise**, because it presents as a scatter of
  `r` rows plus a 2-instruction count gap (the hoist + the spill), with your version's single
  direct load pairing against the original's reload. Count the instructions: if the original has
  exactly 2 more than you and one of them is a spill store of a value used once at the end, this
  is the lever. It also explains WHY the rest of the allocation differs — in 0x40e160 the
  original spends ESI on the last argument (destroying `this`), which it can only afford because
  the descriptor pointer is already safe in a slot.
  ⭐ Generalizes to any pointer walked more than once, and it is worth trying speculatively on
  any residual whose only structural anomaly is a hoisted load: caching a member in a local is
  ordinary, idiomatic source, so unlike #8f this lever costs nothing in faithfulness.
- **#14 16-bit call-site arithmetic comes from the CALLEE's `short` params,** not source
  casts — explicit `(short)` locals create word-slot traffic the original lacks.
- **#15 Block layout is trace-driven and mostly NOT source-steerable** — deferred-block/
  merge-tail placement residuals: annotate EFFECTIVE and park.
- **#16 Jump tables:** per-label arm indices in value order; grouped empty cases widen the
  byte table; a lone empty case folds away. Read the dword table to recover source labels.
- **#16c A `sub reg,imm; je` CHAIN (rather than `cmp`/`je`) is a SWITCH with too few / too
  sparse arms for a table — and the second immediate is the DELTA, not the case value (v457,
  `DecorActorBase::PickReachableDestNodeMaybe`/0x4343f0).** The original's
  `and eax,0xff; sub eax,0x80; je A; sub eax,0x7f; je A` reads at a glance like arithmetic, but
  0x80 + 0x7f = 0xff, so it is `switch (x) { case 0x80: case 0xff: ... }`. Writing the same test
  as `if (x != 0x80 && x != 0xff)` gives an ordinary `cmp al,0x80` / `cmp al,0xff` pair instead
  and never converges (DIFF 67 → 26 on the switch spelling). **Two tells that a `sub`-chain is a
  switch and not a comparison: the operand is widened to a full register with `and reg,0xff`
  first (a switch expression is promoted to `int`; a byte compare would stay in `al`), and the
  immediates ACCUMULATE rather than each standing for a case value. Sum the chain left to right
  to recover the real labels.**
- **#16d A switch on a char/byte-valued CALL needs a named byte LOCAL to reproduce the original's
  stack round trip (v457, same function).** `mov byte ptr [esp+N],al; mov eax,dword ptr [esp+N];
  and eax,0xff` — a byte store immediately followed by a DWORD reload of the same slot — is what
  cl emits for `unsigned char b = f(); switch (b)`. Switching directly on the call expression
  (`switch (f())`) keeps the value in a register and drops those two instructions; on 0x4343f0
  that alone was the difference between 148 B and the original's exact 165 B. The store/reload
  pair looks like pointless spill noise, which is exactly why it is easy to write off — treat it
  instead as positive evidence that the original had a named variable there.
- **#16b A DWORD jump table that comes out too NARROW is usually a `break;` that should be a
  `return;`, not a missing case label (v397, `TutorialWnd::OnLButtonDown`/0x451540).** The
  table's width is set by the highest case label that SURVIVES to codegen, and a do-nothing arm
  spelled `case 8: case 9: break;` does NOT survive: its target is already the switch's end
  label, which is also where `default` goes, so cl prunes the trailing entries — the recompile
  after adding those two labels was byte-for-byte unchanged, still `cmp eax,6` and a 7-entry
  table. Spelling the same arms `case 8: return 0; case 9: return 0;` keeps both labels, widens
  the table to 9 and flips `cmp eax,6` → `cmp eax,8`; the two blocks then cross-jump into the
  same shared epilogue, so the MACHINE CODE is identical either way and only the table differs.
  Practical consequences: (a) when a table is short, re-spell the do-nothing arms before
  concluding case labels are missing; (b) a table can legitimately be wider than the callee's
  real value range (this one has entries for 9 while its hit-test tops out at 8), so table width
  is evidence about the SOURCE's labels, not about reachable values; (c) case labels whose slot
  points at the default target are unrecoverable from bytes — you cannot tell `case 4:` written
  explicitly from `case 4:` absent.
  **RE-CONFIRMED and generalized (v401, `TutorialWnd::MapNotifyToItemIndex`/0x44f9a0, EXACT):
  this applies to a switch whose default is a `return`, not just one whose arms `break`, and it
  applies independently to EVERY table in a nest.** That function has three (an outer one over
  0..15 plus inner ones over 0x2406..0x240f and 0xc54..0xc5c); with the do-nothing arms spelled
  `case 10: case 13: case 14: case 15: break;` the outer table came out 13 entries wide
  (`cmp eax,0xc`) and the inner one 8 (`cmp eax,7`), and the recompile as `... : return -1;` --
  the SAME value the post-switch fall-through returns -- widened both to the original's 16 and
  10 with the code bytes unchanged. Its whole 216-byte code body was already byte-identical
  before the fix; only the table extents differed, which is exactly the failure mode to expect.
  ⚠ Diagnose this from the CANDIDATE's COMDAT LENGTH, not from an `asmscore --dump`: with
  `--len` at the true COMDAT extent the masked table entries flood the dump with
  `add byte ptr [eax], al` noise, and with `--len` at the code extent the dump goes silent
  about the tables entirely. Length short by a multiple of 4, code identical ⇒ a pruned table.
- **#17 memcpy/memset intrinsic forms are operand-provenance-tracked** (field-vs-param
  operands decide the `rep movsb` vs `movsd+movsb` split; provable 4-alignment drops the
  tail).
- **#18 Cross-jump geography:** bare `return CONST` merges into the epilogue only as a branch
  target; duplicated per-arm calls cross-jump the common tail leaving per-arm constant pushes.
- **#18b NESTING an early-out so its `return` becomes the SAME STATEMENT as the fall-through
  return is what buys the epilogue merge — but it does not generalize (v364, `0x4143e0`).**
  A flat guard chain (`if (a == 0) return 0; if (b) { f(0); return 0; } f(1); return 0;`)
  emits a private 6-instruction epilogue per `return`. Rewriting it nested — `if (a != 0) {
  if (b) { f(0); } else { f(1); return 0; } } return 0;` — makes the first and last `return 0`
  literally one statement, so VC5 emits ONE epilogue and branches to it: worth exactly 12 bytes
  per site removed. ⚠ The same rewrite applied to a sibling arm in the SAME function made it
  WORSE (DIFF 986→999). Treat #18b as a probe to try per site, never a rule to apply
  file-wide, and measure each site independently.
- **#18l Count the ORIGINAL's distinct `return CONST` epilogues FIRST, and make the source
  have exactly that many `return` STATEMENTS (v413, `DecorObjMgrMaybe::TickObjSeqGoalsMaybe` /
  0x434d70).** The constructive form of #18b, and the single highest-value probe on a
  guard-clause prologue. 0x434d70 opens with three tests that each bail; the original has
  exactly THREE `xor al,al; pop…; ret 4` blocks, but the SECOND and THIRD tests share the third
  one — test 2's failure `jl`s forward into it. Written as three flat guard clauses this scored
  DIFF(565) with a private 7-instruction epilogue inlined after test 2; folding tests 2 and 3
  into ONE statement — `if ((cooldown != 0 && delay < 0) || (int)cooldown > delay + tick)
  return 0;` — made it EXACT on the next compile. So the epilogue count is a direct, countable
  read on how many `return` statements the source had, and which adjacent guards were written
  as one `||`. Do this before any register-level analysis: it is one `grep -c` over the disasm.
- **#18n #18b/#18l generalize to a shared `return <VARIABLE>;`, and the lever there is making
  the LAST candidate block FALL OUT rather than return (v432,
  `NameAnchorMaybe::TryMatchSocketMaybe` / 0x40b880, 616788 → 257497 in two compiles).** The
  bullets above are written around `return CONST`; the same geometry governs a non-constant
  shared tail (`mov eax,edi; pop…; ret`). 0x40b880 tries four socket matches in sequence and
  has four failure exits that all `return pNeighbour;`. Written as a flat ladder — `if (ext == 0)
  return pNeighbour; if (nX != …) return pNeighbour; …` — VC5 gave each one a private
  6-instruction epilogue (27 surplus instructions). Rewriting only the LAST block as a nested
  `if (ext != 0) { if (point matches) { …; return 0; } }` with ONE trailing `return pNeighbour;`
  collapsed them onto a single shared block reached by `jne`, because every failing test now
  falls OUT of the nest to the same statement instead of naming its own. #18l's counting trick
  then finishes the job: the two remaining consecutive `if (a) return X; if (b) return X;`
  guards had to become one `if (a || b) return X;`. **Two practical corollaries.** (1) The
  original having SOME duplicated copies of the shared epilogue is not evidence against the
  rewrite — 0x40b880 keeps two local duplicates (each the fall-through of an inverted `je`)
  beside its one shared block, and the correct source shape reproduces that 3-copy count
  exactly; match the COUNT, not the uniformity. (2) Judge this by `insns A/B` and the `+`-run
  fingerprint (#18c), never by `cc.sh`'s `DIFF(n)`: across these two compiles DIFF went
  724 → 748 → 724 while the score fell 2.5×, because DIFF is a raw byte count that re-aligns
  on any length shift.
- **#18m A `(T *)` POINTER CAST AT A CALL SITE IS NOT CODEGEN-FREE AT TU SCOPE (v413).** Adding
  `(long *)` casts to three call sites inside `Obj0x4779e0::ParseTokenField` (0x41e9f0) — to
  bridge a corrected `unsigned long` field type to a callee still declared `long *` — left
  0x41e9f0 itself byte-identical, and rotated a SIBLING function in the same TU:
  `ParseEntryExitMaybe` (0x41f0c0) went MATCH -> DIFF(19) at 137/137 instructions, the whole
  residual being one `mov al,1` rescheduled past a `shl`. Changing the CALLEE's parameter type
  instead (so no call-site cast is needed) kept every function in the TU EXACT. **When a type
  correction forces casts at call sites, fix the signature, not the call sites** — and if a
  signature fix is impossible, measure the whole TU before accepting the casts. Same family as
  the `/Og` TU-state rotations this exact TU is already documented for (the v331 bisect).
- **#18i To GET a cross-jump, write the duplication out — hoisting the common part into a local
  or a helper gives VC5 nothing to merge (v397, `TutorialWnd::OnUnhandledMessage`/0x4518b0 and
  `OnLButtonDown`/0x451540).** This is #11/#18 read in the constructive direction, and it is the
  first probe to try when a switch's arms are structurally right but a few bytes long. A switch
  computing a per-arm value and calling `f(v); g();` below the switch via a local
  (`int v; switch(...){case 1: v=5; break; ...} f(v); g();`) materializes the value into a
  register per arm and emits ONE `push reg` — 43/43 instructions but 9 bytes over. Repeating
  `f(5); g();` / `f(6); g();` / `f(7); g();` / `f(mirror); g();` inside the arms instead gets the
  original exactly: per-arm `push 5`/`push 6`/`push 7`/`push eax` followed by a `jmp` into ONE
  shared copy of `call f; add esp,4; call g`. The same lever scales to whole multi-statement
  blocks — `OnLButtonDown` writes its `OnExit()` + 4-way state-transition ladder out three
  times, and VC5 cross-jumps the three copies' constant arms into one shared block each while
  leaving their non-constant `default` arms separate (those load through different registers, so
  they are not identical and do not merge). Corollary for reading a residual: a shared tail with
  per-arm prologues is NOT evidence that the source factored anything out; it is evidence the
  source did NOT.
- **#18i-ter The same constructive-duplication lever governs a whole BAIL-OUT BLOCK reached from
  two places, and there a `goto` to one shared copy is measurably WRONG (v469,
  `GameNetThreadState::AttemptJoinOrHostSession`/0x43aa00).** When two different failure paths
  each post the same "teardown" message, the instinct is one block plus a `goto` — but that
  compiles one instruction FEWER and 3 bytes SHORTER than the original, because VC5 then simply
  branches straight to the block. The original's shape is `push 0x1c; jmp <block>` from the early
  path, i.e. cl HOISTED the common `push <sizeof>` above the later branch (so both successors
  share it) and fixed the other predecessor up with its own push — a fix-up it only performs when
  it is tail-merging two SOURCE copies. Writing the block out twice took the function from
  insns 143/144 to **144/144** and total 52359 → 12340. Read it as an oracle: a `push CONST; jmp`
  entering a block whose fall-through predecessor pushes the same constant above the branch means
  the source had TWO copies, not a `goto`.
- **#18m Two early `return X;` guards: `if (a) return X; if (b) return X;` vs `if (a || b)
  return X;` decides whether the exit epilogues are DUPLICATED inline or share ONE tail block —
  and which one wins is layout-dependent, so probe both (v469, 0x43aa00).** In a
  failure-block-first layout the two-`if` form made cl inline a full 5-instruction epilogue at
  EACH guard (insns 148/144, total 172518) while the `||` form collapsed them into the single
  shared tail the original has (143/144, **52359** — a 3.3× swing from one `||`). ⚠ The polarity
  is NOT universal: in the earlier success-block-first layout of the same function the two forms
  were byte-identical, and once the final layout was in place splitting the `||` back apart cost
  132499. So `||` is not a rule to apply blind — it is a cheap two-compile probe whose answer
  depends on the surrounding block order, and it must be re-probed after any layout change.
- **#R1 A register coin-flip on a GLOBAL LOAD changes the function's LENGTH, so a small byte
  shortfall is NOT proof of missing body (v469, 0x43aa00).** `mov eax,[mem32]` assembles to the
  5-byte `A1` accumulator-only encoding; every other register needs the 6-byte `8B /r disp32`
  form. A function whose instruction sequence pairs 144/144 can still be 2 bytes short purely
  because cl parked a global in EAX where the original used ECX — twice. Count INSTRUCTIONS
  before reading a length gap as absent code, and when the gap equals the number of
  EAX-vs-other global loads, it is a pure allocator tie-break. (Same caution family as the
  `--len` traps in CLAUDE.md, from the opposite direction.)
- **#R2 A loop-bottom GLOBAL STORE through EAX (`A3`) makes "EAX == the global's value" an
  invariant at the loop top, and losing that invariant cascades (v507,
  `GameNetThreadState::~GameNetThreadState`/0x438cc0).** The original's send-queue drain ends
  `mov eax,edi; cmp edi,ebx; mov [head],eax; jne`; the candidate storing through EDI instead
  (`89 3D`, also +1 B) breaks the invariant, and three differences a screen away follow from
  THAT, not from their own source lines: the no-call path can no longer skip the head RELOAD
  (the original's `je` lands past it, straight on the store, reusing the loop-top EAX), the
  post-delete re-read of the global then has a single definition and gets CSE'd away (the
  original keeps TWO `A1` reloads — the candidate's missing instruction), and the vtbl
  dispatch spends EAX where the original spends EDX. So when a residual clusters around a
  loop over a global, fix the loop-bottom store's register FIRST and re-measure before
  grinding the downstream items. Store-base levers measured: a fresh `pNode = g_global;` read
  placed BEFORE the store through it lands the store base in EAX (`A1` form); spelling the
  store as `g_global->field = 0` flips it to ECX (`8B 0D`). Chained `g = pNode = pNext` vs
  two statements is inert.
- **#R3 `delete p`'s inner null check FOLDS when p is a local already tested non-null in the
  same block, but NOT when the argument is re-read through a pointer (v507, same function).**
  `delete (T *)pPayload` inside `if (pPayload != 0)` drops the `cmp/je`; the one-word
  respelling `delete (T *)pNode->pPayload` keeps it — which is what the original's unfolded
  check needs. (Same construction the EXACT `EnqueueOrFreeNode` already used at its own
  type-0xe arm; this confirms the spelling is the lever, not the surroundings.)
- **#18o When a compound `||` guard's LAST-tested disjunct is a conjunction whose success jumps
  to a SHARED body while the NEXT condition falls through to a LOCAL copy of the same body, the
  source is `if (A || B || C) X; else if (D) X;` — the conjunction C belongs INSIDE the guard's
  `||`, not in its own `else if` (v497, `WorldBoardPartial::CaptureBoardToBitmap`/0x457080).**
  Written as `if (A||B) X; else if (C) X; else if (D) X;`, C's success compiles a LOCAL
  write+jmp instead of the shared-body jump (one compile to tell apart, worth 100+ of asmscore).
  ⚠ The converse residual does NOT always yield, though: with the shape fully right, VC5 may
  still cross-jump the four per-case `[cmp/test; jcc; store; jmp]` D-tails into two shared pairs
  where the original keeps all four — a merge that inserts a 5-byte jmp to delete a 10-byte
  fall-through tail. That one is robust to else-if/else-{if}/nesting/volatile/label-stacking/
  compiler-version (RTM=SP3, byte-identical) and to the host TU's declaration-count dial, so it
  sits in the v348 better-optimized-candidate class as a PARK — except there the copies differed
  by register and here they differ only by branch WIDTH (near early, short late), so there is no
  allocator asymmetry left to chase. ⭐ **Same session, adjacent dial datum: `#include`ing a real
  header (src/LocoBitmap.h) into a .cpp rotates that TU's OWN already-EXACT functions** (it cost
  0x457ce0 in the same file at a pure reg-rename) — the declaration-count dial counts consumed
  headers, not just edited ones, and a TU-local methods-only view is the measured antidote.
- **#18n WHICH ARM IS THE FALL-THROUGH IS A DIRECT SOURCE-POLARITY ORACLE ON AN `if/else`, and
  reading it wrong costs a systematic diff on every copy of the shape (v470,
  `GameNetThreadState::TrainNet_TryBoardEdgeHandoffMaybe`/0x43c160).** VC5 compiles
  `if (C) A else B` to `<test C>; j!C <B>; A; jmp <join>; B:` — so the block the conditional jump
  SKIPS INTO is the `else`, and the block that falls through is the `then`. Two independent
  polarity errors in one function scored 236608 where the corrected form scores 70133: the
  original's `cmp eax,ebx; je <rehome>` with the list-unlink falling through means the source is
  `if (pPrev != NULL) pPrev->pNext = ...; else pListHead = ...;` (NOT the `== NULL` form that
  reads more naturally), and its `cmp ecx,ebx; ja <send>` with the empty-slot call falling
  through means `if (providerId <= 0) HandleEmpty(); else Send();` (NOT `if (providerId > 0)
  Send(); else HandleEmpty();`). ⚠ Both are shapes a human would naturally write the OTHER way
  round, so do not "fix" them to read better. ⚠⚠ And do not generalize from one site inside one
  function: the SAME function's first arm really is the natural `if (providerId > 0 && bDirty)
  Send(); else HandleEmpty();` — its Send block is the fall-through. Derive the polarity per
  `if`, from that `if`'s own block layout.
  ⭐ **v479 (`BigObj::IsPlainRunMaybe`/0x41f430) is the cheapest confirming instance yet, and the
  one to reach for as a first move on a small predicate: it was the WHOLE 49-byte residual on a
  69-byte function, and flipping the polarity took it from DIFF(49) to MATCH in one compile.**
  The original's `cmp al,3; je <far>` sends the category-3 arm OUT OF LINE, so the source is
  `if (categoryByte != 3) { <the category-0xc case> } else { <the category-3 case> }` -- the
  negated test with the SECOND-listed case as the `if` body, again the shape a human would
  naturally write the other way round. On a function this small the dump reads as one contiguous
  block of `+`/`-` rows rather than scattered noise, which is the tell: an all-or-nothing block
  displacement with an identical instruction multiset is a polarity flip, not a codegen mystery.
  ⭐ **v477 (0x40e160) is the cleanest confirming instance, and it shows the rule pays off in
  BULK: four consecutive if/elses, all four wanting the same reading, and getting all four right
  moved the function from DIFF(188) to DIFF(176) and set up the #13b lever that finished it.**
  Each clamps one edge of a source rect to either a default (`rectViewport.<edge>`) or a computed
  value, and in every one the DEFAULT is the fall-through block -- so the source is
  `if (rect.left >= rcTile.left) { rcSrc.left = viewport.left; } else { rcSrc.left = <computed>; }`
  and NOT the `if (rect.left < rcTile.left) { <computed> } else { <default> }` that reads more
  naturally (the interesting case first). Mechanically: find the `j<cc>` target and the
  fall-through; the FALL-THROUGH is the source's `if` body, and the condition is whatever the
  `j<cc>` INVERTS. When several sibling `if`s in one function share a shape, expect them to share
  the reading too, and flip them as one batch rather than one per compile.
- **#18o A `return` that is the FINAL `else` of an if/else-if chain is laid out LAST, after every
  other epilogue; the same `return` written as an early guard INSIDE the last arm is laid out
  INLINE with a skip-jump (v470, 0x43c160).** The original's `cmp eax,edx; jl <end-of-function>`
  jumping past all four arms' `mov al,1; ret` epilogues to a lone `xor al,al; ret` is the tell for
  `} else if (x >= cols - 1) { ... } else { return 0; }`; writing it as
  `} else { if (x < cols - 1) return 0; ... }` instead emits the 4-instruction epilogue inline at
  the guard. Zero semantic difference, ~5 instructions of layout difference. Companion to #18m
  (which governs whether two epilogues MERGE); this one governs WHERE the single one lands.
- **#18p ⭐ DUPLICATING an identical assignment into BOTH arms of the final `if`/`else` — instead
  of writing it once above the branch — is what stops cl folding a `return 0;`/`return 1;` pair
  into `setcc`, and it is byte-FREE to try (v471, `RFIndex::Open`/0x45caa0, DIFF(31) → EXACT).**
  The tell is an exit of the form `mov ecx,<val>; mov <field>,eax; cmp ecx,eax; jne` + TWO private
  epilogues (`xor al,al; …; ret` and `mov al,1; …; ret`), where EAX is a materialized ZERO left
  over from a preceding intrinsic (here the inline `strcpy`'s `xor eax,eax`) rather than an
  immediate. Written the natural way —
  `Unk0x8 = 0; if (pFile == NULL) return 0; return 1;` — cl emits `mov <field>,0` (immediate),
  allocates the tested value to EAX, and collapses both returns into one `test eax,eax; setne al`
  epilogue, leaving the function ~8 instructions short. Written as
  `if (pFile == NULL) { Unk0x8 = 0; return 0; } else { Unk0x8 = 0; return 1; }` cl hoists the
  common store back above the branch **so the emitted store is identical either way** — but the
  duplicated form leaves a zero REGISTER live across the branch, which is exactly what forces the
  `cmp reg,reg` and the two epilogues. ⚠ **No return-statement spelling reaches this**: seven were
  probed and all folded identically (explicit `else`, polarity flip, `return p != NULL;`,
  `return IsOpen();`, the `?:` ternary, a `bool`/`unsigned char` local, and comparing against an
  explicit `FILE *pNull = 0;` local). The lever is the DUPLICATED STORE, not the return. This is
  the constructive twin of #18l: #18l says count the epilogues and match the `return` count; #18p
  says that when the count already matches and cl folds anyway, look for a store the original
  duplicated into each arm. Generalizes to any assignment common to both arms whose RHS is the
  same constant the exit test compares against.
- **#18q Two NEGATIVES on the `unsigned char`-returning predicate, both worth knowing before you
  spend compiles on one (v477, 0x40e250).** (1) **Splitting a leading `&&` conjunct out into its
  own early-return guard is BYTE-IDENTICAL** -- `if (a != b) return 0; if (c && d && e) return 1;
  return 0;` and `if (a == b && c && d && e) return 1; return 0;` compile to the same 71 bytes
  with the same diff. cl normalizes the short-circuit chain before layout, so this is not a lever;
  do not burn a compile on it. Distinct from #18b/#18n, which move real BLOCKS around -- here
  there is no block to move. (2) **The accumulator form (`unsigned char b = 0; if (...) b = 1;
  return b;`) is a REAL and different codegen, not a spelling variant** -- it buys the early
  `xor al,al` that a `return 1;`/`return 0;` pair does not emit, matching an original whose
  first guard exits with AL pre-zeroed. But keeping the accumulator live in AL across the whole
  test denies EAX to the body, and cl pays for that with an extra callee-saved register: a
  `push edi`/`pop edi` pair the original lacks. So it trades two bytes of residual for two bytes
  of prologue. ⭐ **The decision rule: use the accumulator only when the original's own body
  ALSO avoids EAX.** If the original spends EAX on a loaded value (as 0x40e250 does on its
  coordinate), it did not have an accumulator live, and the early `xor al,al` you are chasing is
  the zero-register RESIDENCY class (#the v375 class) rather than a source shape -- park it.
- **#R2 The `x < 0` FLAG-REUSE PEEPHOLE (`js` vs a re-materialized `cmp <val>, <zero-reg>; jl`)
  is NOT source-steerable, and VC5 is inconsistent about it WITHIN ONE FUNCTION — so seeing both
  forms side by side in the original is evidence of a compiler coin-flip, not of two different
  source shapes (v470, 0x43c160).** The same inlined `i >= 0 ? &arr[i] : 0` accessor is reached
  four times there. Where the index was computed with `sub`, the original reuses the flags
  (`sub eax,edx; js`) — and we match it exactly. Where it was computed with `add`, `inc` or
  `dec`, the original throws the flags away and re-tests (`add eax,edx; cmp eax,ebx; jl`) while
  cl gives us the bare `js` at all four. One extra instruction per site, plus a knock-on
  operand-order flip on the commutative `add` (which of the two addends ends up in EAX). ⚠ The
  `inc`/`dec` sites settle it: their arithmetic is byte-identical on both sides and ONLY the
  following test differs, so there is no operand, type, scope or spelling left to vary. Measured
  INERT: the `if (i >= 0) { return &arr[i]; } return 0;` spelling of the accessor; swapping the
  add's operands (cl normalizes); `n = a; n += b;`; block-scoping the index/pointer locals per
  arm. Measured WORSE: inverting the accessor to `i < 0 ? 0 : &arr[i]`. Park it.
- **#L1 LOOP FORM (`for` vs `do/while`) is a real backedge-POLARITY lever on a bottom-tested
  loop, and it is a cheap first probe on any few-byte loop residual (v450,
  `PeerTrainSlotQueueMaybe::SelectCarAtPositionMaybe`/0x44e830).** Same 59 instructions either
  way, but `do { ...; i++; } while (i <= n);` emits `ja <exit>; jmp <top>` where
  `for (i = 0; i <= n; i++) { ... }` emits the original's `jbe <top>; jmp <exit>` -- VC5 does NOT
  canonicalize the two, even when it can prove the first iteration always runs. Worth DIFF(4) →
  DIFF(1) on its own. Try both spellings before calling a loop residual intrinsic.
- **#L2 …but a loop whose exit test sits in the MIDDLE (a `break` between two halves of the
  body) IS canonicalized, and VC5 will rotate it however it likes regardless of spelling (v450,
  `Obj0x477758Base::ReserveMaybe`/0x435d10).** THREE spellings of the same trailing-NULL trim
  walk -- `do { if (p[i-1] != 0) break; i--; } while (i > cap);`, the top-tested
  `while (p[i-1] == 0) { i--; if (i <= cap) break; }`, and the compound
  `while (p[i-1] == 0 && --i > cap) {}` -- compile to BYTE-IDENTICAL code, and all three peel the
  first load above the loop where the original keeps it at the loop top. **#L1 and #L2 together:
  the loop form is a lever only while the test is at one END of the body; once the body has a
  mid-loop `break`, rotation is the optimizer's call and no source shape reaches it.** Stop
  after the second spelling.
- **#L3 A loop's SCALAR counter must be DECLARED BEFORE its induction pointers, or its
  initializer sinks (v450, `WorldActionCursor::RefreshVariantMenuIconsMaybe`/0x45a400).**
  Declaring `unsigned char i = 0;` after the two walked pointers puts `xor bl,bl` below both
  `lea`s instead of above them -- DIFF(19) vs DIFF(3), for a pure declaration-order edit. Same
  family as the sibling-locals stack-slot lever (v449), but this one is about instruction
  ORDER in the prologue rather than about slot assignment.
- **#L4 To make a memory read land BEFORE a global read in a comparison, give the memory read
  its own TEMP (v450, `WorldActionCursor::ClampRectIntoViewMaybe`/0x459720).** `if (this->rect.left
  < g_scrollX)` written against a pre-loaded `int nScrollX` temp emits the global load FIRST;
  adding `int nLeft = this->rect.left;` ahead of it restores the original's order. ⚠ But do NOT
  reach for the no-temp spelling to fix it: writing the compared expression inline in an
  `else if` AND again in the call argument breaks VC5's cross-jump of the two identical calls
  outright (180 B → 212 B, DIFF(10) → DIFF(112)). The temps are load-bearing in both directions.
- **#18i-bis The same constructive cross-jump lever applies to a plain `if`/`else`, and the tell
  is a CONSTANT materialized into a register instead of pushed as an immediate (v450,
  `WorldActionCursor::RefreshCategoryMenuIconsMaybe`/0x45a480).** An if/else picking a label and
  then calling `SetLabelText(pszLabel)` ONCE below it compiles the pooled empty-string arm as
  `mov eax,<offset>; push eax`; writing the call out in BOTH arms instead gets the original's
  `push <offset>` immediate over one shared `mov ecx,...; call` tail -- and that was the whole
  difference between DIFF(22) and EXACT. **Read a lone `mov reg,<const>; push reg` where the
  original has `push <const>` as "the source called this twice, not once".**
- **#18j `sub reg,K / je / dec reg / jne` on a state variable is SWITCH lowering, and the chain
  is sorted by CASE VALUE while the bodies are laid out in SOURCE order — so the case whose body
  is the dispatch's FALL-THROUGH is the one written FIRST (v420, `WorldBoardMaybe::
  ResolveWorldClickMaybe`/0x455d60, 1475673 → 686356 on this one lever).** An `if (s == 3) … else
  if (s == 4) …` chain emits `cmp/je` pairs instead, so the decrementing chain alone tells you to
  write `switch`. Then read the block geography: 0x455d60 tests 3 first (`sub ecx,3 / je far`)
  but falls through into case FOUR's body, which only `case 4:` before `case 3:` in the source
  reproduces. Same family as the case-label-order oracle further down, one level up: that one
  orders arms WITHIN a table-dispatched switch, this one orders arms of a compare-chain switch
  against the chain's own sorted test order.
- **#18k A `mov BYTE PTR [esp+N],1` immediately followed by `mov al,BYTE PTR [esp+N]` — a store
  and a reload of the same byte local in ONE basic block — means the source did NOT `return` there
  (v420, 0x455d60).** VC5 constant-folds `bRet = 1; return bRet;` into a plain `mov al,1`, so
  the redundant reload only survives when control leaves the block some other way: a `break` out
  of a switch, or a fall-through, reaching ONE `return bRet;` at the end whose 6-instruction
  epilogue VC5 then REPLICATES at each exit. Read it as an oracle in both directions — a `mov
  al,1` in the original means a real `return CONST` at that site (#3), a store+reload at EVERY
  exit means exactly one `return <variable>;` in the whole function.
- **#18l On a two-call short-circuit, which spelling you use decides which arm is the
  FALL-THROUGH, and the original tells you directly (v420, 0x455d60, 402057 → 338028).**
  `if (a() != 0 || b() != 0) { hit = 1; } else { hit = 0; }` puts the FAILURE store on the
  fall-through path and collapses the second test into `test al,al / setne` — one call's result
  materialized rather than branched. `if (a() == 0 && b() == 0) { hit = 0; } else { hit = 1; }`
  is the same predicate but emits two forward `jne`s to a shared `mov byte,1` with the `mov
  byte,0` on the fall-through, which is what the original has. A stray `setne`/`sete` in your
  compile where the original branches is the tell: flip the test polarity AND the arms together.
  Third spelling in the same family (v503, 0x42ce10): `if (a() == 0 || b() == 0) { v = 0; }
  else { v = 1; }` emits two forward `je`s to a shared `xor bl,bl` with the `mov bl,1` on the
  fall-through — the exact mirror of the `==0 &&` form, and the one that closed a 5-byte
  `setne bl` residual.
- **#18m Widening a local's SCOPE to hoist its computation above a branch can pay for itself in
  a spill — measure, do not assume the hoist is free (v420, 0x455d60, REFUTED at 416476 vs
  338028).** The original computes one switch arm's two tile coordinates before the dispatch
  (they are scheduled into the prologue's `push`es), which only a function-scope pair
  reproduces; but at function scope VC5 must keep them live across the OTHER arm's own
  recompute and spills one, growing the frame from `push ecx` to `sub esp,8` and shifting every
  stack displacement in the function. Reassigning a shared pair in the second arm rather than
  declaring a second pair scores bit-identically, so there is no third spelling — this is a real
  register-pressure wall, not a missing shape. Symptom to recognize: a `+ sub esp,N` row at
  offset 0 of the `--dump` paired with `mov [esp+N],reg` spill stores.
- **#18m Two register-steering spellings for a global-backed value used across calls (v520,
  `AlbumCardWnd::BeginModalCapture` 0x402590, score 6006 → 0 EXACT).** (a) SEED-AND-RESULT IN
  ONE NAMED LOCAL: when the original keeps `x - x % n` in ONE callee-saved register, write
  `nStart = nStart - nStart % nVisible;` (reuse the seed local as the result) — computing into a
  fresh local instead makes VC5 evaluate in `ecx` and late-copy to a saved reg, costing an extra
  `push` and a copy. (b) REASSIGN A HOISTED POINTER LOCAL AFTER A CALL to force exactly one
  original-style global reload: with `pCache = g_pX;` hoisted, a later `pCache = g_pX;` after an
  intervening call makes VC5 reload the global ONCE into the same register for the tail stores —
  re-reading the global at each store produces TWO reloads, keeping the stale local produces
  ZERO. Both are invisible in the decompile; read the original's reload count and register
  residency, then pick the spelling.
- **#18c A `+`-only run of epilogue instructions in `asmscore --dump` (`xor eax,eax; pop…;
  ret N`) with no matching `-` run is the reliable fingerprint of a missed tail-merge** —
  distinct from ordinary scheduling noise, and worth chasing because each one is a whole
  instruction block. Contrast with an apparent BLOCK SWAP (a `-` run and a `+` run of the same
  instructions a few dozen bytes apart): that is usually an ALIGNER artifact from accumulated
  upstream drift, not a real arm-order defect — see #18d.
- **#18d VC5 canonicalizes if/else arm order, so "swap the arms to match the emitted order" is
  a NO-OP (v364, `0x4143e0` WM_MOUSEMOVE — verified byte-identical COMDATs before/after).**
  `if (A == 0 && B != C) { X; return 0; } Y; return Z;` and
  `if (A != 0 || B == C) { Y; return Z; } X; return 0;` compile to the same bytes. So when a
  dump shows the original's two arms in the opposite order from yours, do NOT rewrite the
  source: either it is aligner drift, or the real cause is upstream (an extra instruction
  earlier that shifted every subsequent offset). **Confirm any suspected no-op rewrite by
  diffing the COMDAT bytes directly** (`match.coff_functions` → compare the named entry's
  `data`) rather than by watching the headline score, which also moves when you change `--len`.
- **#18e ⚠ #18d's "arm order is a no-op" does NOT generalize — it only holds when BOTH arms
  TERMINATE (each ends in its own `return`). When neither arm returns and control falls through
  to shared tail code, VC5 emits the SOURCE-FIRST arm INLINE and branches over it to the second,
  so arm order is fully source-steerable and worth real bytes** (v379, `SplashWnd::BeginModalCapture`
  0x4206b0, isolated with a single-variable A/B compile: `if (state == 0) { A } else { B }` scores
  58031 while the identical `if (state != 0) { B } else { A }` is EXACT — 0). This is the same
  reason #18d's own case is inert: with two terminating arms there is no fall-through join, so the
  De Morgan-equivalent spellings canonicalize. Read the original's `je`/`jne` sense and which arm
  sits inline, then write the source in that order.
- **#18j ⚠ #18e's arm-order lever only bites when the condition is a SINGLE comparison. Put a
  short-circuit `&&`/`||` in front of the same two non-terminating arms and VC5 canonicalizes
  the arm order again, exactly as in #18d — the flip goes back to being a no-op** (v398, both
  halves measured in one session on two functions with the same fall-through shape).
  `TutorialWnd::ResourceRefCategoryTable_ParseCategoryRecordsMaybe`/0x44fc80 has the simple
  form: `if (szToken[0] != '\\') { SetRectEmpty ×2 } else { 40-line rect parse }` is EXACT,
  while the natural `== '\\'` spelling is 311/311 instructions but costs **106 align** and one
  byte — pure #18e, and it was that function's ENTIRE residual. `TutorialWnd::RefreshClientRect`
  /0x450d60 has the compound form: `if (p->pDesc == 0 || p->nHandle == 0) { SetRectEmpty }
  else { long block }` and its De Morgan twin `if (p->pDesc != 0 && p->nHandle != 0) { long
  block } else { SetRectEmpty }` are **byte-for-byte identical**. Both functions fall through to
  shared tail code and neither arm returns, so #18e alone predicts the flip should matter in
  both — it does not. Practical rule: on a residual that looks like an arm swap, check the
  condition first; one comparison ⇒ flipping is the lever, `&&`/`||` ⇒ do not bother, the cause
  is elsewhere.
- **#18k A one-shot "did I do anything?" flag that the original tests with `cmp bl, 1` is a
  `char`, not a `bool` — declare it `char bFlag = 0;`, assign `1`, and test `== 1`** (v398,
  `TutorialWnd::AdvancePresenterFrame`/0x450450: that single instruction was the whole residual,
  67/67 insns and DIFF 19 → MATCH). A `bool`/`bFlag` truth test compiles to `test bl, bl`; only
  an explicit comparison against 1 produces `cmp bl, 1`. Cheap to spot in a dump and cheap to
  fix, and it recurs — the same `== 1` spelling is already load-bearing in
  `TutorialWnd::OnExit`'s `bBoardScrollFlagAtNotify` test. Note this is about the TEST site, not
  the storage: the flag being one byte is not itself the tell.
- **#18m To reproduce an original that carries a switch/if result in a CALLEE-SAVED register
  (`xor bl,bl` at entry, `mov bl,1` per case arm, one shared `mov al,bl` tail): declare
  `unsigned char bResult = 0;` BEFORE the guards, assign `bResult = 1; break;` in each case arm,
  `return bResult;` at the end — and write the default arm as `default: bResult = 0; break;`,
  NOT `default: return 0;`** (v518, `WidgetPickerObj0x477cc8::TestTabSwitchMenuCommandMaybe`
  0x4289a0, DIFF(163) → MATCH). The tells and their exact semantics: the entry-time `xor bl,bl`
  is the entry-initialized declaration; each arm's `mov bl,1; mov al,bl; pop…; ret` is
  `bResult = 1; break;` with the return cross-jumped per arm; a default block of `xor bl,bl`
  FALLING INTO the shared `mov al,bl` tail is `default: bResult = 0; break;` (the compiler
  re-zeroes bl because it does not track the value across the indirect jump-table dispatch),
  while a guard-fail path that jumps to the shared tail with NO re-zero is the function-level
  `return bResult;` (bl provably 0 by dominance). `default: return 0;` instead materializes a
  fresh `xor al,al; pop…; ret` and splits every other tail apart too. Same function family as
  0x4287b0's long-standing "the original keeps its result in EBX" park — that one is worth a
  retry with this exact shape.
- **#18f A `?:` (or an if/else assigning a constant to a temp) whose arms are CONSTANTS compiles
  BRANCHLESS — VC5 emits `neg al; sbb eax,eax; and eax,<K>`. If the original BRANCHES and pushes
  the constant inside each arm, the source did not select a value at all: it wrote out TWO
  separate call statements, and cl 11.00 cross-jumped their identical `mov ecx,<recv>; call <fn>`
  tails back together** (v379, `SplashWnd::BeginModalCapture` 0x4206b0: `SetMode(bSkip ? 3 : 0)`
  and the if/else-into-a-temp form both score 52023, while `if (bSkip) { mgr->SetMode(3); } else
  { mgr->SetMode(0); }` is EXACT). The emitted shape is `push 3; jmp tail; push 0; tail: mov ecx;
  call` — a per-arm constant push over a shared call tail, i.e. #18's cross-jump geography
  arriving from the opposite direction.
  ⭐ **Confirmed FOUR times in one function (v472, `WorldActionCursor::
  RefreshDecorCategoryCandidatesMaybe` 0x45a330, DIFF(123) → DIFF(133) at the right length), and
  the folded forms are more varied than the `neg/sbb/and` above — learn to recognize the family,
  not the instruction:** `x == y ? 2 : 1` → `xor edx,edx; cmp; sete dl; inc edx`; `n > 0 ? 1 : 3`
  (unsigned) → `xor edx,edx; cmp edx,edi; sbb eax,eax; and al,0xfe; add eax,3`; `p != NULL ? 1 : 3`
  → `neg esi; sbb esi,esi; and esi,0xfffffffe; add esi,3`. Any `sete`/`setne`/`sbb reg,reg`
  feeding an `and`+`add` chain where the original has a plain `jcc` + two `push`es is this lever.
  ⭐ **It also fires when the selected value is a POINTER ARGUMENT rather than a small integer** —
  `SetLabelText(pDecor != NULL && pDecor->bValid ? pDecor->szCategoryName : "")` folds the same
  way, and splitting it into two full `SetLabelText` call statements inside an if/else is what
  restores the original's `lea eax,[edi+0x7c]; push eax; jmp` / `push <empty>` shape. So the rule
  is not "ternaries over constants" — it is **any value selection feeding one call; write out the
  whole call in each arm and let cl cross-jump the shared `mov ecx; call` tail back together.**
  Confirmed again v504 (`SelectedObjWidgetMaybe::SelectObjMaybe` 0x42d040): `SetNodeState(
  nModeMaybe == 6 ? 1 : 3)` folded to `mov cx,[mem]; sub cx,6; neg; sbb; and 2; inc; push`,
  and the split two-call if/else restored the original's `cmp word[mem],6 / jne / push 1 /
  jmp / push 3` over the shared tail — DIFF(38) → EXACT.
- **#18q A `bool ? &global : NULL` ternary compiles BRANCHLESS as `neg/sbb/and`, and an extra
  `dec`+`not` wrapper on the flag byte pins the source spelling as `b == true ? ... : NULL`**
  (v504, `SelectedObjWidgetMaybe::SelectObjMaybe` 0x42d040's deselect tail: bare
  `g.bActive ? (void *)&g : 0` emits `mov dl,[b]; neg dl; sbb edx,edx; and edx,&g` (truth-test,
  `!= 0` semantics); the original's `dec dl; neg dl; sbb; not; and` is the `== 1` comparison —
  the pointer-argument twin of the `== true` cmp-bl-1 test-side lesson (see #18k).
- **#18g The SAME lever runs BOTH ways, so read which side the original landed on before
  rewriting. Where the arms' bodies differ (so cl cannot fold them), a call written ONCE after
  the if/else is what compiles branchlessly — duplicate it into BOTH arms to make cl branch**
  (v379, `SplashWnd::SetState` 0x4208f0's state-3 arm: `if (c) state = 5; else state = 4;
  pPage->BeginModalCapture();` computes the 4-or-5 store with `setne cl; add ecx,4` and scores
  90393 at insns 153/159, while duplicating the call into each arm restores the original's
  branch + fully duplicated tail at insns 159/159 and score 24014). Rule of thumb: cl 11.00
  merges what you duplicate and straight-lines what you share, so **write the source in whichever
  shape the ORIGINAL's branch structure implies and let the optimizer move it back**, rather than
  trying to write the emitted shape directly.
- **#18h `f(g(), &local)` evaluates `&local` FIRST — VC5 does the `lea`/`push` of the address
  before calling `g()`. If the original calls `g()` first, hoist it to an explicit temp**
  (v379, `SplashWnd::Create` 0x4204d0: `GetClientRect(GetDesktopWindow(), &rect)` scored 36695;
  `HWND h = GetDesktopWindow(); GetClientRect(h, &rect);` is EXACT). The temp also re-seats the
  local's own stack slot, so this fixes offset-drift residuals in the whole rest of the frame,
  not just the two instructions at the call.
- **#20b ⚠ `delete p` where `p`'s class is only FORWARD-DECLARED in this TU compiles to a bare
  `operator delete(p)` call — NO virtual-destructor dispatch, and cl 11.00 issues no diagnostic
  at `/W3`.** In an `asmscore --dump` it reads as an innocuous `push eax; call <fn>; add esp,4`
  against the original's `mov eax,[ecx]; push 1; call [eax]`, which is easy to dismiss as
  register-allocation noise. The fix is an `#include`, not a source rewrite (v379,
  `SplashWnd::~SplashWnd` 0x4203c0: DIFF(178) → EXACT the moment `ApplSetupWnd.h` was included;
  the sibling `delete pNetSetupWnd` was already correct only because `NetSetupWnd.h` happened to
  be included). **Whenever a `delete` site shows a direct call where the original dispatches
  through slot 0, check the include set before touching the source.**
- **#20c A `cmp eax, <register that holds 0>` where you emit `test eax, eax` means the original
  captured the call's result in a NAMED LOCAL and tested the LOCAL — not the call expression
  inline** (v464; found on `EditCardWnd::BeginEdit` 0x416b80 and confirmed on
  `PopupWndBase::CommitScreenUpdate` 0x414c20 the same session). Both encodings are 2 bytes, so
  this never shows up as a length or instruction-count difference — only as an `S` row, and only
  in functions where VC5 has promoted the literal 0 into a callee-saved register (which it does
  whenever the function has enough zero uses: NULL arguments, zero-stores, `!= 0` guards). The
  inline spelling gets the canonical `test reg,reg`; routing the value through a local instead
  makes VC5 treat the zero as an ordinary register-resident operand and emit `cmp`. **This is the
  COM/HRESULT idiom in particular** — `if (pSurf->Blt(...) != 0)` vs
  `HRESULT hr = pSurf->Blt(...); if (hr != 0)` — because DirectDraw call sites are exactly where
  a function accumulates enough NULL/0 uses to trigger the promotion. It closed 0x416b80 outright
  and took 0x414c20 from DIFF(66) to DIFF(62). Note the original may even swap WHICH callee-saved
  register holds the zero between two blocks of the same function; that part is not steerable.
- **#20d For a struct-typed member, prefer the WHOLE-STRUCT ASSIGNMENT over field-wise stores —
  the tell is a `lea` of the destination base plus RELOADS of the source's own stack slots**
  (v464, `PopupWndBase::CommitScreenUpdate` 0x414c20: `rectPrevCursor = rectCursor;` took
  DIFF(462) → DIFF(66) by itself). VC5's struct copy materializes the destination address into a
  register and emits load/store pairs, then copy-propagation folds away whichever loads are still
  live in registers — so ONE field can come straight from a register while its neighbours reload
  from the stack. That mixed pattern reads like scheduling noise but is the signature of a struct
  assignment; four field-wise stores instead keep every value in registers and never emit the
  `lea`. Same lesson `WindowBase::RefreshClientClipRect` already carries for `rectClipBounds`.
- **#20e Two blocks of duplicated code will only SHARE stack slots if they are SIBLING scopes.**
  (v464, 0x414c20.) A block sitting directly in the enclosing scope has locals that outlive a
  nested block earlier in the same scope, so VC5 cannot overlap them and the frame grows. If the
  original's frame is exactly packed and yours is 8/16 bytes larger with every slot shifted,
  check for this before chasing register allocation: giving the second copy its own `{ }` — in
  practice writing `} else { ... }` after a `return`-terminated `if` — is the fix.
- **#20f A hand-built API descriptor's FIELD-ASSIGNMENT ORDER is a source fact worth real bytes**
  (v464, 0x416b80's DDSURFACEDESC: 12 of the 16 residual bytes). Loads-and-arithmetic fields
  schedule ahead of the constant stores regardless, but their order RELATIVE TO EACH OTHER
  survives, and the constant stores come out in source order. The obvious
  `dwSize/dwFlags/dwWidth/dwHeight/ddsCaps` spelling was wrong in BOTH pairs here — the original
  is `dwSize`, `dwWidth`, `dwHeight`, `ddsCaps.dwCaps`, `dwFlags`. When a descriptor-building
  function is otherwise clean, permute the field order before concluding anything about registers.
  ⭐ v466 confirms this order verbatim on a THIRD and FOURTH site (`PopupWndBase::Create`
  0x413de0 and `LoadCursorSlots` 0x414130, both EXACT): `dwSize`, `dwWidth`, `dwHeight`,
  `ddsCaps.dwCaps`, `dwFlags` is now the house DDSURFACEDESC spelling — copy it, don't re-derive.
- **#20g For a RUN OF INDEPENDENT MEMBER STORES, the disasm's STORE ORDER is NOT the source's
  statement order — mirroring the disasm is actively wrong when one value can be sunk past a
  register-clobbering block** (v466, `PopupWndBase::Create` 0x413de0; both of that function's
  two remaining levers after the first compile were this, and nothing else). The prologue there
  stores nClientWidth, hwndOwner, Unk0xdc, Unk0xe0 and then — past the WNDCLASSA `rep stos` —
  nClientHeight, which reads as "nClientHeight is last in source". It is not: VC5 parks nHeight
  in `edx` (the one scratch `rep stos` does not clobber) precisely BECAUSE the store is EARLY in
  source and can therefore be deferred. Writing the disasm's own order costs 6 bytes, entirely in
  which parameter lands in which register. The exact source is
  `nClientWidth, nClientHeight, hwndOwner, Unk0xdc, Unk0xe0`. Practical method: when a run of
  N independent stores is your whole residual, do NOT reason about it — permute the statements
  mechanically and score each (a scratch Python driver over ~12 orders takes a few minutes and is
  decisive; here 2 of 12 hit EXACT and the rest cost 4–17 bytes). Same family as #20f, but the
  target is member stores rather than a descriptor, and the trap is the opposite one: with a
  descriptor the constant stores DO come out in source order, with members they need not.
  Confirmed again in v467 (0x438e40's dedup-list node init, the function's LAST residual — two
  instructions, `add esp,4` vs the `pNext` store, worth the whole match): the disasm emits
  `bDescByte, pNext, bIndexByte`, but the source is the natural `bDescByte, bIndexByte, pNext`
  (fill the payload, then link) and VC5 sank the link store into the middle. Six permutations,
  ~15 seconds of compiling, one EXACT — again decisively cheaper than reasoning.
- **#20h Which operand of `a + b` gets destroyed (`add ecx,eax`) versus preserved
  (`lea edx,[eax+ecx]`) is steered by WHEN THE OTHER LOCALS ARE FIRST REFERENCED, not by the sum
  itself** (v466, 0x413de0's `rectScreenBounds` fill — 22 of a 28-byte residual). The original
  fills `left, top, right, bottom`; the disasm STORES `left, right, top, bottom`, and writing the
  source that way is what breaks it. Referencing `y` (for `top`) before the `x + nWidth` sum gets
  `y` loaded into a register of its own early, which frees VC5 to spend `x`'s register on the sum
  destructively; deferring `y` leaves both sum operands live and forces the 3-byte `lea` plus a
  cascade of different register picks downstream. A `lea` where the original has a 2-byte `add`
  is therefore a statement-ORDER signal, not a register-allocation tie-break — check it before
  parking anything as `// EFFECTIVE`.
- **#20i A field read that lands BEFORE an `operator new` it logically follows is ARGUMENT
  EVALUATION OF AN INLINED HELPER — introduce the locals** (v467,
  `NetResource_RequestMissingAppearances` 0x438e40; worth 217k asmscore points, the single
  biggest lever on that function). Writing the obvious
  `pMsg = new T; pMsg->a = pNode->a; pMsg->b = pNode->b;` reads the two fields AFTER the
  allocation call, because that is where the statements are. The original reads BOTH before
  `push 6; call ??2@YAPAXI@Z`, parking one in `ebx` and SPILLING the other to a stack byte it
  would not otherwise need — a compiler will not invent that spill, so it is a source fact. The
  fix is to hoist them into locals (`unsigned char bIdx = pNode->b; unsigned char bDesc =
  pNode->a; pMsg = new T; ... = bIdx; ... = bDesc;`), which is exactly what an `inline` helper
  `SendRequest(bDesc, bIdx)` compiles to. Two corroborating tells that this is a real inlined
  helper rather than hand-written locals: (a) the SAME preload appears at a sibling call site
  where nothing else could force it (there `bl` is loaded from a struct field right before the
  `new`), and (b) the DECLARATION ORDER of the two locals is load-bearing — swapping them flips
  which register each value gets and costs `reg_pen`. ⚠ Distinguish the SMALL helper (just the
  allocate-and-send) from a whole-block one: if the enclosing block's helper were real, the
  index byte would have to be SPILLED AND RELOADED across the intervening calls; a FRESH re-load
  from the struct instead refutes the bigger helper and confirms the small one.
- **#20j The prologue's `sub esp,N` is a HARD ORACLE for both the local COUNT and the array
  SIZES — reconcile it before grinding anything else** (v467, 0x438e40). Locals are laid out with
  the first-declared at the HIGHEST address, so a frame that is short by exactly one slot means a
  missing local and a frame short by an odd-looking amount means a mis-sized array. 0x438e40 came
  out at `0x50c` against the original's `0x514`: 4 of the 8 bytes were a missing spill slot that
  appeared on its own once #20i fixed the register allocation, and the other 4 were the path
  buffer being `char szPath[0x500]` instead of this codebase's own established
  **`char szPath[0x504]`** (1284 — the size used at ~20 sites across `src/`; grep a sibling before
  inventing a buffer size). Note the corollary: every esp-relative offset below the array can
  already MATCH while the total is still wrong, because the mis-sized array is at the top of the
  frame — so a clean-looking offset diff is not evidence the frame is right.
- **#19 Aliasing dictates locals:** per-statement member reloads = NO caching local in the
  source; a real `T *p = &field;` shows as lea+spill+deref. Match the reload pattern, not
  your taste.
- **#21 Vtable DATA-xrefs are identity proofs** for thin dtors/tiny overrides that byte-match
  anything.
- **#22 Stubs poison silently:** systematic displacement/width deltas across users ⇒ audit
  the stub struct layout FIRST.
- **#23b Before fearing a class promotion, check whether the consumer TU ALREADY sees the base
  class's header transitively — if it does, the promotion is free** (v379: promoting the padded
  `ApplSetupWndPartial` struct to a real `class ApplSetupWnd : public WindowBase` was measured
  byte-for-byte inert on `src/DPlaySessionMgr.cpp`, 42/65 and 6039 B unchanged with every DIFF
  residual identical in length, because that TU already pulled `WindowBase.h` in through
  `SplashWnd.h`). The #23 include-dial hazard is about ADDING a header a TU did not already have;
  a promotion that only changes a declaration's SHAPE, with no new include, does not turn the
  dial. `grep -n '#include' ` the consumer plus one level of its headers, and measure the TU
  before and after anyway — but do not pre-emptively contort the model to avoid a cost that
  is not there. (When the header genuinely IS new, v378's lever still applies: split the needed
  type into its own minimal header rather than dragging the big one in.)
- **#23 #line provenance rotates the dial:** adding/removing source LINES mid-file displaces
  every function below. Prefer end-of-file additions / same-line decls in matched TUs.
  **Confirmed the same dial applies to bare `#include` lines, not just function bodies (VC5,
  `DPlaySessionMgr.cpp`, v302): adding 3 new `#include` lines near the TOP of an already-matched
  TU (for `LoadIndexFileMaybe`'s `istrstream`/`ifstream`/RF-globals) regressed
  `SelectGridCellFromPointMaybe` — a wholly unrelated, textually-LATER function — from EXACT to
  DIFF(130), independent of where the new function itself was placed.** Isolated by testing the
  includes alone (no new function) against the untouched committed source — the includes alone
  reproduced the regression. Fix: `#include` the needed headers immediately above the new
  function at the END of the file (a `.cpp`'s `#include`s don't have to be TU-initial), not
  file-top, in any TU that already has EXACT matches you don't want to disturb. This is a MORE
  general form of the lesson than "reposition the new function" — the new function was ALREADY
  at the true end-of-file when this regression was caught, so end-of-file placement alone isn't
  sufficient if its own required `#include`s still sit up top. **v323 extension: it's the
  include's CONTENT (new declarations entering the compiler's symbol state), not the line
  count** — re-adding the same `#include` of a header at the identical line position regressed
  `SelectGridCellFromPointMaybe` EXACT→DIFF(130) again, while pure comment-line additions at
  the same spot were inert. Within a shared header, adding FIELDS inside existing padding was
  safe for that TU but adding METHOD declarations (layout-neutral!) was not — when a header
  method block is needed but the consumer TU is position-sensitive, keep a TU-local
  methods-only probe struct instead (the `PeerTrainSlotQueueEdgePartial` precedent; no data
  members = no layout to drift, so it doesn't violate the never-duplicate-structs rule).
  **⛔ v457 REFUTES the v323 extension: it is NOT the include's content.** An `#include` of a
  header containing NOTHING BUT `#pragma once` — zero declarations, zero fields, zero anything
  entering the symbol table — reproduced the regression exactly (`src/DecorActor.cpp`,
  `PlacedObjRegistryMaybe::CompareEntriesMaybe` 0x4361e0, MATCH 156 B → DIFF(76) 152 B). The
  v323 experiment was right that a comment line is inert and a re-added `#include` is not; it
  was wrong to conclude the difference is the declarations, because it never tested an EMPTY
  header. So the real dial is the `#include` DIRECTIVE ITSELF — cl 5.0's `/Og` tie-breaks move
  when the count of source files it has opened changes, regardless of what is in them. Practical
  consequences: (a) you cannot make an include "safe" by trimming what it declares, and a
  forward declaration instead of an include is a real fix while a leaner header is not; (b) the
  empty-header probe is the CHEAPEST way to tell this class apart from a genuine
  declaration-count rotation (#24b) — one compile, and if the empty header also regresses, stop
  looking for the offending declaration because there isn't one; (c) v457 also measured that
  moving the new code to the END of the file does NOT help when the includes stay at the top,
  which is consistent with the original #23 fix above (put the `#include`s next to the new
  function at end-of-file) and NOT with "reposition the function". **The structural fix, when a
  TU has grown a tail that belongs to a different class, is to SPLIT THAT CLASS INTO ITS OWN
  TU** — v457 did that for `PlacedObjRegistryMaybe` and got a small, stable include set that the
  parent TU can no longer disturb. Look for adjacent `.text` to confirm the split is the real
  `.obj` boundary rather than a convenience (0x435aa0 and 0x435c00 are adjacent; they are one
  original TU).
- **⭐ WHICH of two duplicated-COMDAT TWINS your source produces is decided by the TU, not by the
  source (v457).** When identical source is compiled into two `.obj`s the linker cannot fold, the
  two copies differ only in register allocation / operand evaluation order — and the natural
  assumption, that the source's own operand order picks which one you get, is WRONG.
  `PlacedObjRegistryMaybe::CompareEntriesMaybe` compiled character-for-character identically
  gives 0x4361e0's order (156 B) inside `src/DecorActor.cpp` and 0x435c00's order (152 B) in a
  small TU of its own, where it matches EXACTLY. **Consequence for triage: a twin parked as
  "structurally unreachable without a big refactor" should be re-tested from a DIFFERENT TU
  before that verdict is believed** — v451 had concluded 0x435c00 needed the registry struct
  split into two real derived classes, and it needed nothing but a different `.cpp`.
- **/Og's constant-in-register form (`mov ebx,1` + ~45 bl/bx-form uses across a whole
  function) is real but TU-context-sensitive, not source-steerable (VC5, `WorldActionCursor::
  HandleMenuCommandMaybe`/0x45aa50 + `TestMenuCommandMaybe`/0x45a880, v323).** An `int nOne =
  1` source local is constant-folded away; artificial extra 1-uses and mini-replica TUs don't
  move it; prepending dummy functions to the TU measurably shifts it (same #23 dial). Park
  with the Yoda #23 class; retry once the original TU's earlier siblings are transcribed
  ahead of the function.
- **#24 A 1-byte `CALL [reg+disp]` diff is a WRONG-VIRTUAL-METHOD bug, never benign** — the
  disp IS the vtable slot; check for an adjacent-slot mistranscription. Same family (#25): a
  small constant DISP diff on a matching base reg = wrong inlined-accessor FIELD.
- **#24b Adding a virtual to a SHARED BASE is free — but only if every derived class's own
  placeholder for that slot SHARES ITS NAME, or the placeholder stops overriding and opens a new
  slot, shifting every slot after it by one (v427).** This is the specific mechanism behind what
  otherwise reads as generic "shared header declaration-count rotation", and it is worth
  distinguishing because the rotation story says *park it* while this one says *fix it and pay
  nothing*. Symptom: a repo-wide `progress.py` after the header edit shows a sibling TU losing
  whole functions, each scoring **`DIFF(1)`** — one single byte — and a masked byte compare
  (`align=0`, `byte_diff=1`, so `--dump` prints NOTHING; use `tools/match.py`'s
  `coff_functions`/`mask`) pins that byte as a `call [edx+0xNN]` displacement off by exactly 4.
  Cause: derived classes routinely carry `virtual void *_v19();`-style dummies declared *only*
  to position a later real slot (`src/BuildToolButton.h` says so in its own comment). A base
  declaration whose SIGNATURE matches such a dummy (`void *()`) silently absorbs it as an
  override and costs nothing; one whose NAME differs — even with an identical signature — does
  not, and everything below it slides. Fix by giving the base slot and every derived
  placeholder for it ONE name; that is also the truer model, since these placeholders were
  always the same slot. In v427 unifying `WorldActionCursor::HandleMenuCommandMaybe` with
  `BuildToolButton`'s and `WidgetTagObj0x478378`'s own slot-20 dummies took two new
  `WidgetBaseObj0x4784c8` virtuals from **−819 B / −2 EXACT funcs to exactly byte-neutral
  repo-wide**, and dropped naming debt by one as a side effect. ⚠ Verify with a full
  `progress.py` table diff either way — the *count* of declarations really does also matter
  elsewhere, so a name unification is a hypothesis to measure, not a guarantee.
- **A local compared against a struct member must have the member's EXACT signedness to keep a
  sub-word comparison sub-word (v427, 0x459da0).** `short w = p->wFoo;` compared against
  `p->wBar` where both members are `unsigned short` makes the two operands differ in signedness,
  so C++ promotes both to `int` and VC5 emits `xor`/`movsx` widening plus a 32-bit `cmp` — and,
  because the widened value no longer fits an 8/16-bit register slice, it loses its register to
  something else and spills. Spelling the local `unsigned short` restores the original's single
  `mov bx,[esi+0x58]` and its two 16-bit `cmp bx,[esi+0x24]`. Worth 125528 → 45028 on its own.
  Generalizes: whenever a `--dump` shows the original comparing in a 16-bit register where the
  candidate widens first, suspect the LOCAL's declared type before suspecting the allocator. **v446 extends it to PARAMETERS,
  and one byte wide:** `PeerTrainSlotQueueMaybe::ReleaseSlotsForOwnerMaybe`'s guard compares its
  argument against the `unsigned char` field `bOwnerByteA`. Declared `unsigned char`, that is the
  original's bare `mov dl,[eax+0x78]; cmp dl,cl`; declared `char`, the mismatch promotes BOTH
  operands and cl emits `movsx edx,byte [esp+0x18]` / `xor ecx,ecx; mov cl,[eax+0x78]` /
  `cmp ecx,edx`. That single promotion was the entire DIFF(88) — everything else in the dump was
  the register renaming that cascaded from it, which is the trap: the residual LOOKS like an
  allocator fight and is actually one word in a declaration.
- **⚠ A `bool` argument or return type is a REAL, load-bearing distinction from `char` in this
  codebase, worth its own probe on any byte-sized value — v446 pinned FOUR in one session.** The
  family convention is `char` for byte-ish returns, which makes `bool` easy to miss, but the two
  generate different code wherever a conversion sits at the boundary:
    * `AnimDescRefObj0x477488::SetReadyStateMaybe` (0x4061b0) — argument. The body's
      `bReady = bIsReady` into a `bool` member is a bare `mov [esi+0x24],bl` only if the argument
      is already `bool`; with `char` it gains `test bl,bl; setne bl`. DIFF(38) at 54 B vs 47.
    * `PeerTrainSlotQueueMaybe::DispatchActiveSlotsMaybe` (0x44dbd0) — argument forwarded verbatim
      to a `bool` parameter. Same normalization, DIFF(23) at 68 B vs 61.
    * `WorldActionCursor::ContainsIncludingIconMaybe` (0x459d60) — RETURN type. `int` materializes
      the two literals full-width (`xor eax,eax` / `mov eax,0x1`); `char` narrows them to
      `xor al,al` / `mov al,1` and the function comes up five bytes short. A caller that tests
      `al` is consistent with either, so the CALLEE's own constant width is the only witness.
    * `WidgetBaseObj0x4784c8::OnKeyDownMaybe` (0x454ae0) — return type, inherited from v445's pin
      via its override, and confirmed by this body matching EXACTLY as `bool`.
  **cl announces the char→bool direction out loud: `warning C4800: 'char' : forcing value to bool`.
  Treat any C4800 on a function you are grinding as a type bug, not noise** — three of the four
  above were found by reading that warning. The `int`-vs-`char` return direction gets no warning;
  its tell is the constant WIDTH in the `--dump`.
- **Loop shape: put a loop's exit tests in the body as `break`s inside a plain COUNTED `for`, and
  VC5 will not rotate the loop; hoist the counter test to the bottom of a `for (;;)` and it
  will** (v446, `PlacedObjRegistryMaybe::InsertInSortedPositionMaybe`, 0x4362b0). The two spell
  identical logic. With the counted `for`, the counter test is the only back edge, cl emits its
  usual `if (!c) skip; do {...} while (c);` and the body's calls stay in one copy — EXACT at 100
  bytes. Written as `for (;;) { if (!A) break; if (B) break; i++; if (i >= n) {...} }` the loop
  rotates: the first exit test is duplicated into the back edge and the function grows 26 bytes
  (DIFF(99) at 126 B). **Diagnostic signature: the candidate is LONGER than the original, its back
  edge jumps to the SECOND call in the body rather than the first, and the first call appears
  twice.** Related and separately measured at the same function: which arm of the leading
  `if/else` is written first decides which one gets the duplicated tail — the arm that must
  JUMP to the shared tail is the one written as the `else`.
- **A `sete`-materialized byte parked in a STACK SLOT and reloaded before each use proves a real
  named local, not a compiler CSE temp (v427).** The instinct on seeing `cmp edx,2; sete al;
  test al,al; mov [esp+0x13],al` followed by `mov al,[esp+0x13]; test al,al` at two later sites
  is that the original repeated the comparison in source and VC5 cached it. It is the opposite:
  VC5 does **not** CSE a member read across an intervening non-inlined call (it reloads from
  `[esi+0x5c]`), so re-spelling the three tests as the repeated member expression makes the
  score strictly WORSE and deletes the `sete` entirely. Keep the named `bool`/`char` local; the
  stack slot is just what the local gets when the callee-saved registers are already spoken for.
- **The idiomatic struct-pointer walk REPRODUCES a hand-computed non-zero-offset induction
  pointer — model the field, don't hand-write the byte offset (VC5, `BuildPaletteLUTMaybe`).**
  When the original binary anchors a loop's induction pointer at a NON-zero member offset (e.g.
  a `lea eax,[base+0x29]` walking a `RGBQUAD` array by +4, reading `[eax]`/`[eax-3]`/`[eax-5]` =
  the green/red/blue bytes at RGBQUAD offsets +1/+2/+0), that anchor is /O2's own choice — it
  points the induction register at whichever member is accessed FIRST so that first access is
  displacement-free. So the faithful source is the plain idiomatic `RGBQUAD *p = arr; ...
  p->rgbGreen ... p->rgbRed ... p->rgbBlue; p++;` walk (green textually first), NOT a
  hand-rolled `(unsigned char*)base + 0x29` byte pointer with `[-1]/[0]/[1]` indexing. Modeling
  the scratch buffer as a real struct and using named-field access reproduced the exact
  green-anchored codegen AND even improved the byte-match by 1 — the idiom-cleanup and the match
  are aligned, not in tension. General rule: an odd-looking non-zero pointer anchor in the disasm
  is a signal to MODEL THE STRUCT and let the optimizer re-derive the anchor, not to transcribe
  the raw offset (the idiom lint's class A is often pointing at exactly this).
- **Caching a bare MEMBER pointer in an explicit local right before a hot loop (e.g.
  `unsigned short *pPal = pPalette;`) can close a real "reloads the member from `this` twice
  per iteration instead of hoisting it once" gap on ONE function, but does NOT transfer to a
  structurally-near-identical sibling — verify per-function, never batch-apply (VC5,
  `LocoBitmap::PixelCopyColorKeyBlit` vs. 3 siblings, v311).** `PixelCopyColorKeyBlit`'s bare
  `pPalette[0]=...; ...=pPalette[index];` compiled to TWO separate `mov reg,[this+0x14]` reloads
  per pixel where the original loop-invariant-hoists the member into a register once; declaring
  `pPal` explicitly forced the same one-time hoist and closed ~19% of the function's structural
  score gap (292001→235305). Tried the IDENTICAL fix on 3 siblings that also read `pPalette`
  every pixel — `MirrorColorKeyBlit` (branch-based, no self-store), `UpscaleBlit2x2ColorKey`
  (multiply-indexed), and even `ShadowBlit` (the SAME `pPalette[0]`-self-store branchless idiom
  as the original target) — and ALL THREE REGRESSED, reverted in each case. The lever depends on
  the specific function's OWN register-pressure profile at its TU position (Yoda #7), not on the
  self-store idiom shape being shared; a source-level "fix" proven on one leaf must be
  independently re-measured (never assumed) on every sibling before counting it as a family-wide
  win, even when the siblings look line-for-line identical in shape.
- **A per-element loop over `SomeStruct arr[N]` steers /O2's induction-pointer ANCHOR by whether
  it uses direct `arr[i].field` subscripting vs. a hoisted `SomeStruct *p = &arr[i];` local —
  the two are NOT interchangeable even though both strength-reduce to the same down-counter +
  stride pointer walk (VC5, `DPlaySessionMgr::ResetProviders`, v235, closed a byte_diff 27→0).**
  When the loop body has a hot INNER loop (here a `while (arr[i].pChainHead)` node-free walk)
  touching one high-offset field repeatedly, /O2 anchors the induction register at THAT field's
  offset (making it zero-displacement) — but only with direct-subscript access. A hoisted
  `T *p = &arr[i]; ...p->field...` local instead anchors the register at offset 0 (the
  first-accessed low field, per the BuildPaletteLUTMaybe first-access heuristic), shifting EVERY
  member access's displacement by a constant (here +0x38) — a large systematic byte_diff that
  reads like an intrinsic tie-break but is fully source-steerable. Also a distinct 2nd lever on
  the same function: a `pField = 0;` store reached by BOTH a branch arm AND a nested `if
  (pField != 0){...}` block cross-jumps into ONE shared tail store, so a case where the field is
  already null SKIPS the redundant zero-store (a `je` past it) — put the `= 0` INSIDE both the
  arm and the `if` block, NOT unconditionally after the branch, or the compiler emits an
  always-executed store (off by one `je` displacement byte). Prefer direct array subscripting
  (the idiom sibling methods already use) over a hoisted element pointer when matching per-slot
  reset/scan loops. **The same subscript-vs-hoisted-pointer lever applies to a SINGLE array
  element accessed at multiple fields — NOT just loops (VC5, `DPlaySessionMgr::LayoutSet_
  LoadSlotBitmapMaybe`, v237, closed a byte total 138715→16670 in one switch, then EXACT).**
  When `arr[idx]` is touched at several field offsets and one field's ABSOLUTE offset happens to be
  a clean multiple of the element stride (here `aProviderSlots[idx]` at base 0x518 stride 0x4c;
  the +0x40 field's absolute offset 0x518+0x40 = 0x558 = 0x12*0x4c), /O2 folds THAT field's
  address into `(idx + 0x12)*0x4c` index arithmetic while anchoring the OTHER fields at
  `this + idx*0x4c` with large `+0x55c`/`+0x554`/… displacements — a split, per-field addressing
  scheme that ONLY appears with direct `arr[idx].field` subscripting at every site. A hoisted
  `T *p = &arr[idx];` local collapses everything to one `&arr[idx]`-anchored (+0x518) view with
  small displacements, mis-anchoring every access and also dropping a whole `this`-in-register
  (the original keeps `this` in ebp AND the element anchor in esi). Write out the full
  `arr[idx].field` at each site (verbose but faithful) when a single element is read/written at
  several offsets, not just in loops.
- **A companion to the same family: caching an ARRAY INDEX in a local (`int idx = ...;`) can pin it
  into a CALLEE-SAVED register when it's live across intervening calls — stealing a register from a
  long-lived `this` and cascading into a wider stack frame + extra spill traffic (VC5,
  `GameNetManager_HandleQueuedEvent` case 0x16, v246).** The original re-reads the slot index
  (`mov eax,[node+0x10]`) at EACH of two `if (idx >= 0)` guards so it stays in a scratch register
  (eax), keeping `this` in the preferred callee-saved reg (ebx) — my `int idx = pNode->slotIndex;`
  local instead forced idx into ebx (callee-saved, to survive the `operator delete`/`new`/`memcpy`
  calls), pushing `this` to ebp AND widening the frame by 4 bytes (the displaced `&slot` needed a
  stack home). Fix: DON'T cache the index in a local — re-read `arr_index_expr` at each guard so it
  stays scratch; hoist only the element POINTER (`T *p = &arr[idx];`) WITHIN the one block that
  actually reuses it across several field writes (matching the original's cached `&slot`). This is
  the inverse pressure of the subscript-vs-hoisted-pointer lever above: there the fix is direct
  subscripting; here the fix is re-reading the index (both keep the value out of a callee-saved reg).
  Symptom: a pervasive `this` register swap (ebx↔ebp) plus a 4-byte-wider `sub esp,N` frame.
- **The MIRROR-IMAGE fix from the same family: a find-and-remove list walk's `prev` pointer must be
  DECLARED/zero-initialized at the very TOP of the function (before any preceding calls), not just
  before the loop — otherwise /O2 hoists a loop-invariant IMPORT-CALL address (e.g. `CloseHandle`'s
  IAT slot) into a callee-saved register, stealing it from `prev` which then spills to scratch and
  cascades the WHOLE allocation (VC5, `GameNet_HandlePlayerLeft`/0x43a5c0, v255, DIFF 192→EXACT in
  one edit).** The original zeroes `prev` (edi) in the prologue — the same spot it pins the 0 constant
  (ebx) and loads `this` (ebp) — using all four callee-saved regs for this/0/prev/pRec, so there's
  none free for `CloseHandle`'s address (it loads `call [0x4770a0]` fresh at each call). My
  `PlayerRecordNode *pPrev = 0;` declared AFTER the two pre-loop method calls let /O2 free a reg and
  hoist `mov ebp,[CloseHandle]` before the loop (`call ebp`), displacing `this`→ebx and `pPrev`→scratch
  eax. Moving the `pPrev = 0;` declaration to the FIRST statement of the function pinned it into edi
  and matched exactly. Symptom: an import call rendered `call <reg>` (hoisted) instead of `call [IAT]`,
  plus a full `this`/pinned-0 register-role reshuffle. General rule: zero-init a walk's `prev`/anchor
  pointer at function top when the loop body makes any dllimport call.
- **A PARALLEL-ARRAY reconcile loop (copy/compare `srcArr[i]` into `destArr[i]` for fixed N) can have an
  INTRINSIC induction-variable-elimination residual that NONE of the three usual source forms reproduce
  (VC5, `DPlaySessionMgr::ApplyProviderSnapshot`/0x43fc50, v251, parked EFFECTIVE at 152/151 insns).**
  When the source base is a pointer that can't be cached across an intervening call (here `pMsg->pPayload`,
  which the `CopyFrom` call might alias), the original RE-READS the base inline at every source access AND
  derives the source's `i*stride` scaled offset from the DESTINATION induction pointer `esi` — spilling the
  invariant `-this-destBaseOff` (`mov reg,-destBaseOff; sub reg,this`) and adding `esi` per iteration — rather
  than maintaining a separate `i*stride` stride IV. The faithful inline form
  (`((T*)pMsg->pPayload)[i]`) re-reads correctly but the compiler builds its OWN `i*stride` IV (one extra
  `add reg,stride`, +1 insn, cascading a register swap through the whole loop body). The other two forms are
  worse: direct `destArr[i]` subscripting spawns MORE IVs (156 insns), and caching `T *pSrc = pMsg->pPayload`
  in a local DROPS the original's faithful re-reads (147 insns, wrong memory-access pattern). So the inline
  re-read form is both closest AND the only one matching the original's actual loads — keep it and park; this
  specific "re-read base + derive scaled offset from the dest IV" configuration is not a source-steerable lever
  (distinct from the subscript-vs-hoisted-pointer family, where one of those forms DOES win).
- **A struct field's SIGNEDNESS is pinned by `movsx` vs `movzx`/`xor;mov` at any sign-relevant use
  (a multiply operand, a widening compare) — a 1-instruction tell, distinct from the field-WIDTH
  tells (VC5, `DPlaySessionMgrProviderSlot::wLayoutCols` +0x40, v237, the last residual byte
  before EXACT).** `LoadSlotBitmap`'s `cols*rows` product read the +0x40 field with `movsx`
  (sign-extend), so the field must be a signed `short`; an `unsigned short` compiles `xor
  reg,reg; mov rx,[field]` (zero-extend) instead — a single differing instruction that reads like
  scheduling noise. Cross-check a bitmap-dim / count field's declared signedness against the
  extend-instruction at its widening use site (multiply, `int`-context compare) before trusting
  an unsigned default; the sign flip is free to change and doesn't affect the field's zeroing or
  same-width copies.
- **A field zeroed by TWO adjacent 16-bit stores (`mov word [x],0; mov word [x+2],0`) where a
  sibling consumer reads/writes the SAME 4 bytes as ONE dword is a byte-level ALIAS (union), NOT
  "codegen noise for clearing a dword" — the byte-match is the arbiter, and it REFUTES the noise
  hypothesis (VC5, `DPlaySessionMgrProviderSlot::unkDword1Maybe` +0x40, v235).** A single
  `unsigned int f = 0;` compiles to ONE dword store; the original's TWO word stores are one
  instruction longer, so the field genuinely has two logical 16-bit halves on that write path
  even if another path (a wire (de)serializer) touches all 4 bytes as a dword. Model it a `union
  { unsigned int dwWhole; struct { unsigned short wLo, wHi; }; }` exactly like the already-
  established `dwTailAliasMaybe` +0x32 alias (Ghidra keeps the dword view — unions can't be
  modeled inline — while `src/` carries the union). Same family as the "non-`sizeof`-aligned
  scalar ⇒ packed struct" tell: a small store-WIDTH/count mismatch vs. the field's declared type
  is real layout evidence, never dismissed as noise. This one overturned a prior session's
  explicit "RESOLVED: genuinely one dword, the 2×16-bit stores are noise" doc claim.
  **Corollary — the SAME two-word-store tell with NO dword consumer anywhere means two genuinely
  SEPARATE `unsigned short` fields, not a union (VC5, `PeerTrainNodePartial` +0x74/+0x76, v241).**
  A freshly-`new`'d node zeroed a dword-modeled `dwHeadingMaybe` (+0x74) as `mov word[+0x74],0; mov
  word[+0x76],0` (two word stores, reusing an already-0 word register) — since nothing ever reads
  all 4 bytes as one dword (only the low half, via `(unsigned short)dwHeading`), split it into two
  plain `unsigned short` fields (`wHeading`+`wUnk0x76`), NOT a union. The split is safe across a
  whole cluster: every reader that used `(unsigned short)dwField` compiles `movzx` IDENTICALLY
  whether the field is a low `short` or the low half of an `int`, so the sibling functions' byte
  totals don't move. (Union only when a second path genuinely touches the full dword; plain split
  when the dword view was never real.)
- **A stack MESSAGE BUFFER where only the low bytes are written and the rest ships uninitialized
  needs a MULTI-FIELD struct local, not a single narrow scalar — a `short local = K;` compiles to a
  full DWORD store (zeroing the high word), but a `struct { u16 lo; u16 pad; } msg; msg.lo = K;`
  emits the 16-bit `mov word[..],K` the original has, leaving the high word as leftover stack (VC5,
  `GameNet_TeardownAndFlushQueues`'s 0x3fd "leaving" opcode, v256, EXACT).** The original reserves a
  4-byte slot via `push ecx` (the standard single-dword local reservation, so the high word holds the
  incoming `this`), writes only the low 2-byte opcode (`mov WORD PTR [esp+N],0x3fd`, 66-prefixed), and
  sends 4 bytes — Ghidra renders it `CONCAT22((short)(this>>16), 0x3fd)`. A `short opcode = 0x3fd;`
  local instead compiles `mov DWORD PTR [esp+N],0x3fd` (a 32-bit store zeroing the high word, +1 byte,
  the whole tail shifts) — MSVC treats the lone small local as a full dword init. The 2-field struct
  (write `.lo` only, `.pad` deliberately uninitialized — a `// sic`) is what forces the compiler to
  preserve the high word and emit the word store. Reach for it whenever a small opcode/control message
  is built on the stack and sent with a length larger than the field(s) actually written.
- **To reproduce the /GX new-alloc-protection SEH scaffolding for a `new T(ctorargs)` whose T is
  out of scope, model T's partial view at its FULL `sizeof` (from the `operator new(N)` size) plus a
  DECLARED-ONLY ctor matching the real ctor's arg shape — `new PartialT(args)` then emits `operator
  new(sizeof)` + the alloc-protection state transitions + the ctor call, all byte-identical (the
  ctor + vtable are masked relocs) (VC5, `RebuildOrEnqueueTrainCars`'s `new PeerTrainNode`,
  0x43e690, v241).** The whole-function SEH frame (`fs:0` push, the `local_4` state going -1→0→-1
  around the ctor) falls out of the `new` expression automatically — no manual `__try` needed. The
  partial must be grown to the exact `new` size (pad the tail) or the `push <sizeof>` immediate
  diverges; a declared-but-undefined ctor is enough (the project compiles+diffs COMDATs, never
  links). Same principle as the header-inline-dtor / declared-only-dtor precedents: a declared
  signature is sufficient to shape a call/alloc site without the callee's body.
- **A repeated early-exit `delete`/`return CONST` that /O2 keeps INLINE at N sites (with per-site
  registers) vs. TAIL-MERGES into one shared block is steerable by which block sits at the FUNCTION
  END (VC5, `RequestTrainMoveOrReleaseNode`, 0x43e560, v241, 280342→44012).** A trailing
  identical block becomes a tail-merge MAGNET: writing the rare `if (cond==X){ release; return; }`
  guard INLINE early, then the common body, let /O2 route the other N enqueue/return sites into that
  trailing block (wrong — the original keeps all N inline with distinct per-site registers).
  Wrapping the whole body in `if (cond != X) { ...N inline sites... }` and putting the rare release
  as the shared END block the body falls PAST reproduced the original's inline-everywhere layout.
  Which specific delete stays inline vs. shares the end is itself per-site (the joined-empty-slot
  delete stayed inline while the dwUnk0x04 release shared the end) — match each individually.
- **⚠ The tail-merge dial has a HARD STOP: once a function has ANY far shared `return CONST`
  block, VC5 pulls EVERY other same-constant return into it, and no arm-order spelling splits
  them again (v395, `BuildToolButton::InitMenuIconsMaybe` 0x449600).** The original there mixes
  the two treatments in one function: guards 1-2 `je` a shared `xor al,al; pop*4; ret` block
  while guard 3 keeps its OWN al-already-zero epilogue (no `xor` — cl proved AL==0 from the
  `test` that branched there). Both readings were compiled. Early-return everywhere gives cl no
  shared block to build, so it inlines guards 1-2's epilogues too (12 extra insns, total
  112031). Writing guards 1-2 as NESTED ifs over a single trailing `return 0` — which is exactly
  what `je <far shared xor block>` means, and what Ghidra renders — DOES reproduce those two
  jumps, but the trailing block it creates then swallows guard 3 and the final guard as well
  (3 MISSING insns, total 164371, worse). So: when the original SPLITS same-constant returns
  between a shared block and inline copies, reproduce whichever side is larger and park the
  rest — the split itself is cl's own size threshold, not a source-shape you can dictate.
  Corollary for reading a dump: an `S`-row `je <far>` vs `jne <near>` pair with a `+`-run of
  `pop`/`ret` underneath is a MERGE-POLICY difference, not the arm-order lever — check whether
  the far target is a `return CONST` block before spending a compile on swapping arms.
- **#26b A NEWLY EXTRACTED shared header is a header-set change like any other — bisect it
  against the individual `#include` lines, not against your new code (v395).** Consolidating
  `NetSessionEventQueue` out of its .cpp into `src/NetSessionEventQueue.h` was byte-neutral for
  its OWN TU (a pure text relocation: the .cpp includes it from exactly the position the
  definitions used to occupy, so preprocessed output is unchanged), and the `WidgetBase.h`
  member-declaration promotion done the same session was byte-neutral repo-wide. But merely
  INCLUDING the new header in a second TU rotated an unrelated already-close residual there
  (`BuildToolButton::RepositionWithHotspot` 119 → 572) — because the header drags in `Pair16.h`
  plus two inline-bodied record types. The bisect that isolated it took three compiles and is
  the one worth copying: measure HEAD's .cpp against the NEW header first (separates header
  from your new code), then add the new `#include`s one at a time to HEAD's .cpp, then try
  function ORDER within the TU. Here order was innocent and exactly one include was guilty.
  A heavy header is a cost to weigh, not a reason to write a duplicate partial view.
- **#26 The TU's HEADER SET is a real dial input** — audit the include set before grinding
  when a TU's first-emitted functions are the non-exact ones (here: windows.h/ddraw.h/
  dsound.h/dplay.h sets and their order).
- **#28 Final-EXE address order ≠ source/.obj order** — don't reorder source to chase
  addresses; emission order matters only WITHIN an .obj (allocator forward-carry).
- **#29/#30 The symmetric-register-swap residual class is INTRINSIC** (function body + header
  set) — position, COMDAT set, and every flag proven inert; /O2 uniquely optimal. Don't
  re-grind; annotate EFFECTIVE. **Confirmed on VC5** (0x45ca10/0x470c30/0x45cdf0/0x46c480):
  source can't steer which of two equivalent register assignments (`this`→eax vs ecx) the
  allocator picks. Weak 2-data-point signal: more writes/register pressure may pin the
  choice — don't over-index.
- **A prologue `xor eax,eax; cmp GLOBAL,C; sete al; test al,al; je` MATERIALIZATION of a leading
  `if (GLOBAL == C && ...)` condition is an INTRINSIC register-SCHEDULING artifact, not a
  source-condition form — no `&&`/explicit-bool/cached-local phrasing reproduces it (VC5,
  `GameNetMsgQueue::EnqueueOrFreeNode`/0x4393d0, v261, EFFECTIVE).** /O2 spills the compare
  flag to a byte via `setcc` specifically when it loads GLOBAL into a callee-saved register and
  then REUSES that register for another value (here `pNode`) BETWEEN the `cmp` and the branch; a
  faithful transcription that instead keeps the other value in that register from entry and loads
  GLOBAL into a scratch reg emits a plain `cmp/jne` (8 bytes shorter). Every source phrasing of the
  condition folds to the direct branch — the fork is purely which register the allocator picks for
  GLOBAL, which C++ can't dictate. Same `sete`-materialization appears in the sibling
  `EnqueueOrProcessLocalNodeMaybe` (0x43f140), confirming it's this shutdown-check idiom's
  deterministic codegen. ⚠ The shorter prologue cascades EVERY downstream jump displacement, so
  `cc.sh`'s raw byte DIFF looks huge (~192) while `asmscore.py --dump` shows every instruction
  aligned (byte_diff ~29) — trust the instruction-aligned view before concluding a structural gap.
- **⭐ The same fold-side class fires on BYTE tests, not just pointer null-checks (v435, 0x45de40 /
  0x440ea0).** An original that emits `call f; test al,al; jne L; test al,al; jne L` — the SAME
  register tested twice with no reload, both branches to the SAME target — is that redundant-check
  class, not a source shape you are missing. Reading it as `if (a == 0 && a == 0)` and writing the
  redundancy out literally (`if (!bOk) { if (!bOk) { … } }`) compiles BYTE-IDENTICALLY to the plain
  single `if`, so there is nothing to grip. Budget 4 bytes per site and move on. Two useful
  discriminators found while chasing it: the check that does NOT get the doubled test is the one
  whose callee returns a real `bool` (a `char`-returning callee is what leaves the extra
  normalization behind); and a lone dead `test eax,eax` on a `BOOL`-returning API's result, sitting
  between the next call's argument push and the call itself with no branch consuming it
  (`WriteFile` at 0x440f03), is the same thing one register wider.
- **⚠ Do NOT reach for `bool bOk = f();` to try to reproduce a doubled byte test — it produces the
  OPPOSITE shape (v435).** A `char`→`bool` conversion at an ASSIGNMENT compiles to the full
  `neg al; sbb eax,eax; neg eax` normalization idiom, which is 3 instructions the original does not
  have. That is item (4) of the byte-predicate lever below restated from the caller side: at a
  RETURN the `? true : false` ternary steers it, at an assignment feeding a branch nothing does.
- **⭐ At a RETURN, `return f() != 0;` and `if (f()) return 1; return 0;` are NOT the same codegen,
  and the RETURN TYPE is not the lever (VC5, v444, `BuildToolCursorWnd::Create`/0x436c50,
  DIFF(14) → EXACT).** The original narrows a byte-returning callee's result with the byte-wide
  `test al,al; setne al`. The obvious `return Base::Create(...) != 0;` spelling instead compiles
  the dword-wide `neg al; sbb eax,eax; neg eax` — and it does so under BOTH a `bool` and an
  `unsigned char` return type, so changing the declared return type (the first thing you will
  reach for) moves NOTHING. Only the statement shape does: the explicit
  `if (expr) { return 1; } return 0;` form emits `test al,al; setne al` under both types. Same
  three-instruction normalization idiom as the `bool bOk = f();` bullet above, just at the return
  rather than at an assignment; `TutorialWnd::Create` had already been written the `if`-form way,
  which is what made the two callers of the same base method disagree. When a small function's
  ENTIRE residual is a trailing `neg/sbb/neg` against the original's `setne`, rewrite the return
  statement before touching anything else.
- **A REDUNDANT pointer null-check that a dominating branch already proved non-null is KEPT by the
  original binary (`cmp reg,ebx; je`) but FOLDED by our cl 11.00 via redundant-branch elimination —
  an intrinsic, unsteerable optimizer-STRENGTH difference, the fold-side mirror of the do-while-peel
  family (VC5, `GameNetThread_TickLoop`/0x439240, v262, EFFECTIVE, THREE folds in one function).**
  All three variants folded regardless of source shape: (a) an `if (p == 0) break;` recheck AFTER
  `delete p` (cl 11.00 does NOT model the deleted pointer as indeterminate — `delete p` and
  `operator delete(p)` fold identically); (b) a `while (p != 0)` loop's rotation-entry guard,
  redundant with its enclosing `if (p != 0 && ...)` guard (`for` vs `while` rotation fold identically —
  same class as, but opposite direction to, the do-while PEEL lessons: fold REMOVES a check, peel ADDS
  one); (c) a `delete (Poly*)p`'s own null-check inside an enclosing `if (p != 0)`. When a function's
  whole residual is a handful of these `cmp reg,ebx; je` checks the original has and yours lacks
  (with all register ROLES otherwise matching), it is this intrinsic class — don't grind it. Two
  DOWNSTREAM artifacts ride along fold (a): the value materialized by the folded check picks a
  different register (e.g. a switch pivot `mov edx,[eax]` vs `mov eax,[eax]`), and a byte-flag test
  compiles `mov al,[m]; cmp al,bl` (load-then-compare) instead of the original's shorter direct
  `cmp byte[m],bl` — both resolve only if the fold itself could be prevented, so park them together.
- **Ctor vs dtor from a bare vtable-store stub (VC5):** `mov [ecx],&vtbl; ret` with NO
  `mov eax,ecx` is a DESTRUCTOR — this toolchain's ctors always chain `this` through eax. A
  sibling storing the SAME vtable literal plus member inits is likely the paired ctor — grep
  for the shared vtable address.
- **A "sets vtbl + inits an embedded member" shape is NOT sufficient to call something a
  constructor** — check which of the embedded member's OWN two lifecycle functions is actually
  being called first. v51 misnamed `SavedFileEntry`'s DESTRUCTOR as its constructor because
  it plausibly-read as "set vtbl, init embedded `ThumbnailBmp`" — but the callee was
  `ThumbnailBmp`'s own destructor (confirmed by its body calling a `CloseStreamsMaybe`-shaped
  cleanup method), and a constructor never calls another object's destructor. Before trusting a
  ctor/dtor call in an ambiguous stub, identify the CALLEE's own ctor/dtor pair first (its real
  ctor zero/field-inits, its real dtor tears down/closes resources) — this is Yoda lesson #24's
  family: don't trust a plausible shape without checking the thing it dispatches to.
- **Ctor member-init ordering is source-syntax-sensitive (VC5):** a mem-initializer list
  schedules the compiler's vtable store LAST; body assignments put it first (matching the
  original's vtable-first shape). Try body assignments when a ctor's only residual is
  store ordering.
- **⭐⭐ N STORES AHEAD OF THE VPTR STORE IS A LAYOUT FACT, NOT A SCHEDULING TIE-BREAK: it
  proves those N stores come from a MEMBER SUB-OBJECT'S INLINED CONSTRUCTOR, which in turn
  proves the field is a member CLASS and not a bare array** (v459, the whole `CarNetState` /
  `CarNetStateAlt` cluster — three ctors, 609 B, all EXACT once modeled). Nothing you can write
  in a ctor BODY will ever emit ahead of the implicit vptr store; only mem-initializers do, and
  an inlined member default-ctor *is* a mem-initializer. So when a ctor's sole residual is "the
  vptr store is N instructions too early", stop looking for a scheduling lever (the previous
  bullet's body-vs-list dial does not reach it) and read the N stores as a sub-object's ctor.
  The discriminator costs ONE compile: put any existing member in a real mem-init list
  (`: wSignature(0x66)`) and confirm its store lands *before* the vptr; that also tells you the
  target position is reachable at all.
  - **The stores' BASE REGISTER then tells you how many sub-objects, and the source that
    consumed one.** Three stores off one base = one sub-object; the original here addressed the
    third off its OWN hoisted `lea` (later reused as a `memcpy` destination), which is only
    reproducible with two separate members, each with its own `this`. A single combined
    two-field sub-object folds the third store into `[base+0x15]` and still misses.
  - **Corollary — the sub-objects' constructors are ASYMMETRIC when the fields are, and that
    is real evidence about the original, not noise.** Here `CardNameA() { sz[0]=0; sz[20]=0; }`
    vs `CardNameB() { sz[0]=0; }`: `nameA`'s last byte doubles as a flag, written `= 1`/`= 0`
    and tested elsewhere around a `strcpy` short enough to leave it intact. Independent
    call-site evidence like that is how you tell two member classes from one.
  - **Adding `operator char *()` to such a name class keeps EVERY existing call site
    compiling and byte-identical** — `strcpy(p->nameA, ...)`, `p->nameB[0]`, `sizeof(p->nameA)`
    (still the class's own 21) all keep working, so this refactor costs zero call-site churn
    across consumer TUs. Reach for it before rewriting 25 call sites to `.sz`.
- **Dtor vtbl-store position follows source order (VC5):** for a non-virtual dtor assigning
  the vtable as an ordinary member, the store's position among body statements is
  source-order-sensitive, not fixed-first (the `Obj0x477798` family needed it as the 2nd
  statement, after the first member zero).
- **⭐ A RUN OF INDEPENDENT SCALAR `param -> field` COPIES IS EMITTED ROTATED LEFT BY ONE from
  source order — so you must WALK THE WHOLE CYCLE, not try one swap** (VC5, v358,
  `WindowBase::Create`/0x425b70, which this closed to EXACT after being parked as intrinsic).
  Source `x,y,height` emitted `y,height,x`; source `y,height,x` emitted `height,x,y`; source
  `height,x,y` emitted `x,y,height` — byte-exact. Because the transform is a rotation and not
  an identity, a SINGLE reordering trial lands on a different wrong rotation and reads as
  "reordering has no effect / this is an allocator coin-flip" — which is exactly the wrong
  conclusion this function carried for many sessions. **Symptom to recognize:** N field-store
  disp32s (and their matching param-load disp32s) that are a cyclic PERMUTATION of the
  original's, with insn counts equal and `align`/`reg_pen`/`identity_miss` all 0. With N
  statements there are only N orders — just try them all, it is N cheap compiles.
  Requires the copies to be genuinely independent (no aliasing), which pure `field = param;`
  runs are.
- **The two memory operands of a single binary arithmetic expression are NOT order-steerable**
  — VC5 canonicalizes which one gets loaded into a register and which stays a memory operand,
  independent of how the source is written. Confirmed v358 on `LocoBitmap::Fill` (0x42aa90,
  `imul`: `width*height`, `height*width`, and an explicit `int n = height; n *= width;` temp
  ALL compile identically, while the original loads the other field first) and previously on
  `AlbumCardWnd::OnKeyDown`'s subtract. Distinguish this from the rotation lever above: that
  one is about the order of SEPARATE STATEMENTS (steerable); this is about operands WITHIN one
  expression (not steerable). Park operand-order-only residuals; walk the cycle on
  statement-order ones.
- **A local's DECLARATION SITE inside a function can be the actual root cause of a wrong SEH
  PROLOGUE instruction order, not the SEH scaffolding itself.** `DSound_InitDeviceAndChannelPool`
  (v85): a `WAVEFORMATEX` local declared+initialized at the TOP of the function (matching its
  first *use* in source flow) got all 7 of its field-init stack stores hoisted to function
  entry by `/O2` (no dependencies to block it) — that unrelated stack-store traffic, not the
  `/GX` unwind scaffolding itself, was what pushed the two SEH-frame pushes ahead of the
  `mov eax,fs:0` read relative to the real function's order. The real source declares/inits
  `WAVEFORMATEX` immediately before its ONE actual use (a `SetFormat` call deep in the
  function), not at the top — moving the declaration down to its point of use recovered the
  exact real prologue instruction order. When an SEH/frame-setup residual looks like pure
  instruction scheduling noise, check whether an EARLY-DECLARED-BUT-LATE-USED local is
  leaking unrelated stack traffic into the prologue before concluding it's intrinsic.
- **A trivial-constant local's initializer can legitimately be hoisted ABOVE an intervening
  call, and only reproduces when the source places the declaration before that call.**
  `DSound_InitDeviceAndChannelPool` (v85): `BOOL bSelectBestDevice = 1;`'s `mov bl,1` executes
  in the real binary BEFORE a `DSound_Teardown(...)` call that textually/logically comes
  first — safe because `ebx` is callee-saved under this calling convention, so setting it
  early can't be clobbered by the call. Declaring the local textually before the call (not
  after, even though nothing else depends on ordering) was necessary to reproduce this.
- **A vtable slot whose ENTIRE body is `__amsg_exit(0x19)` is a genuine pure-virtual placeholder,
  not a benign default.** `0x19`=25 decimal is the CRT runtime-error id for R6025 "pure virtual
  function call" — MSVC emits this as the auto-generated body for a declared-but-never-defined
  virtual (no `/GX` C++ EH needed, it's a straight CRT call). Confirmed on
  `WidgetBaseObj0x4784c8`'s slots 17-20 (all four pointing at one shared `0x467e90` body):
  treat as abstract and expect every instantiated leaf to override it, rather than reading it as
  "the base's own real behavior, just a no-op."
- **Virtual dtors auto-emit a free "scalar deleting destructor"** (30B `??_G` COMDAT: call
  dtor, conditionally `operator delete`) — add `virtual` to a matched class's dtor and it
  appears as a byproduct (the vtable-store bytes are a masked reloc either way). Marker:
  `// FUNCTION: LOCO 0xADDR (??_G<Class> scalar dtor)` — the parenthetical pairs a COMDAT
  with no source line. Does NOT apply to classes whose dtor is genuinely non-virtual. For
  deleting dtors that tail-jmp into a NON-trivial base dtor: a declared-but-undefined base
  emits a defensive test/adjust thunk, an empty-body base gets inlined away — both wrong; the
  base dtor's REAL body must be modeled first, no shortcut exists (VC5-confirmed on
  0x40e680/0x424ba0). **A polymorphic class's CTOR (its `mov [this],&vtbl` prologue store)
  and its `??_G` scalar dtor can BOTH be byte-matched with the real dtor left DECLARED-ONLY
  (no body anywhere in the TU) — VC5-confirmed, `DPlaySessionMgr` ctor 0x43d0a0 + ??_G
  0x43d110, v233, both EXACT first try.** Declaring `virtual ~T();` (no definition) plus a
  defined ctor is enough: emitting the ctor forces the vtable COMDAT, which references `??_G`
  at slot 0 → the compiler synthesizes `??_G` (calling `~T` as an EXTERNAL, a masked reloc)
  AND the ctor's vtable-store — neither needs `~T`'s body. So a large/complex real dtor
  (here 396B of queue-draining) does NOT block landing the ctor + scalar-dtor; defer the dtor
  and still bank the two. (The dtor just won't appear in the `.obj` — no marker, left
  PARTIAL.) **Corollary — do NOT assume a class with its own vtable is a subclass of a
  vtable-sharing sibling: a standalone-polymorphic model (just `virtual ~T()`, no base) will
  byte-match the ctor whenever the ctor calls NO base ctor.** Confirm non-derivation two ways:
  (1) the ctor stores its vtable directly with no preceding base-ctor `call`; (2) a
  slot-by-slot vtable compare — a true single-inheritance subclass shares the base's
  non-overridden slots at the SAME offsets, whereas merely reusing shared helper BODIES shows
  them at SHIFTED offsets (DPlaySessionMgr's 0x4781c8 vs WindowBase's 0x477c30: shared bodies
  at slots shifted by +2, so NOT a WindowBase subclass despite an earlier doc claim).
- **The declared-only ctor/dtor + partial-view technique SCALES to a whole multi-class glue
  function — a repetitive `new`/`Create`/`delete` bootstrap over N heterogeneous classes matches
  EXACT with each callee left declared-only (VC5, `Bootstrap_ConstructSingletonWindows`/0x406f90,
  v253, whole 2052B EXACT — 8 window classes).** For each `new PartialT(args)` the /GX
  alloc-protection state machine (`[esp+state]=N` after `operator new`, `=−1` after the ctor)
  falls out automatically; the callee ctor/`Create`/dtor need only DECLARATIONS (masked-reloc
  calls, never linked). Two reusable specifics: (1) **`delete g_pGlobalPtr` on a global POINTER
  whose static type has a DECLARED-ONLY `virtual ~T()` stays INDIRECT** (`mov ecx,[g]; test; je;
  mov eax,[ecx]; push 1; call [eax]` = slot-0 scalar-deleting dtor) — MSVC does NOT devirtualize
  a delete through a global pointer even when the pointee's concrete type was `new`'d in the same
  function (contrast the known-concrete-global-OBJECT devirtualization lesson: a global OBJECT
  devirtualizes, a global POINTER does not). So a declared-only virtual dtor is all you need to
  reproduce the indirect teardown. (2) **To make an EXISTING pad-only partial view polymorphic
  without pulling the real base header into an already-matched consumer TU, declare a standalone
  `virtual ~T();`** — the synthesized vtable ptr occupies +0 (replacing the first 4 pad bytes),
  every later field offset and `sizeof` are preserved, and the decl is inert for the consumer
  (which only field-accesses through the pointer); this dodges the "embedding WindowBase would
  rotate this TU's codegen" hazard while still giving `delete` its slot-0 dispatch. ⚠ Process
  caution from the same session: when reverse-engineering per-path data by grepping the raw
  disasm for a global-ADDRESS PATTERN (e.g. delete chains via `ds:0x4fd3..`), make the pattern
  cover EVERY relevant global — a range-narrow pattern SILENTLY drops a global stored outside the
  main cluster (here `g_pBuildToolCursorWnd` at 0x485258, outside the 0x4fd378.. window-pointer
  cluster), producing a false "omitted / leaked" conclusion that only surfaces as a shorter
  candidate at compile time. Re-extract with the full global set before trusting a "this window
  is missing from the cleanup" finding.
- **A stubborn opaque `FUN_` in the CRT-boundary zone (past `TOTAL_APP_END`/0x463800,
  `tools/progress.py`'s own documented app/CRT split) can be IDENTIFIED FOR FREE by extracting
  the real `.obj` module straight out of `toolchain/vc50/LIB/LIBCMT.LIB` and byte-diffing it
  against the original disasm — no source-writing, no guessing (VC5, `FUN_00466d60`→`sprintf`
  + its two callees `FUN_0046ba20`→`_output`/`FUN_0046b8f0`→`_flsbuf`, v312).** `.lib` files are
  plain `ar`-format archives; GNU `ar`'s own `x`/`p` extraction chokes on this MS-lib's
  long-name table (silently writes a 0-byte file, no useful error) — parse the archive by hand
  instead: read `ar t <lib>` for the ordered member NAME list, separately walk the archive's
  raw 60-byte headers in file order collecting `(data_offset, size)` for every member whose
  name field is NOT exactly `/` or `//` (the two special symbol/longname members `ar t` doesn't
  list), then zip the two same-order lists together to get `name → (offset, size)` and slice the
  bytes directly. Once extracted, `objdump -d -M intel -b coff-i386 module.obj` disassembles it
  straight (no `.exe`/PE wrapping needed for a raw `.obj`). A `sprintf.obj` guess that first
  looked like `vsprintf.obj` (one `mov`-vs-`lea` byte off at the 3rd `_output` argument — `mov`
  reads an already-a-`va_list` parameter value, `lea` computes one from the function's OWN
  varargs, i.e. cdecl `...` vs. an explicit `va_list` parameter) was the tell to try the sibling
  module instead — `sprintf.obj` then matched the original byte-for-byte (relocation-masked:
  the two `call`s point at whichever internal symbols the .lib itself names, `__output`/
  `__flsbuf`, i.e. `_output`/`_flsbuf` with the compiler's cdecl leading underscore). This
  technique is a strictly stronger oracle than the usual "transcribe idiomatic source and
  recompile" loop for anything past the CRT boundary — MS shipped the exact compiled bytes, so
  there's no source to write and no register-allocation tie-break to chase; only try it on a
  function you suspect is a static-CRT/runtime-library body (a `.text`-tail address, or a shape
  matching a known libc-family function), not on real game code (whose `.obj`s were never
  shipped anywhere to extract). Renamed all 3 in Ghidra (`rename_function`); updated every
  known call site to call the real `sprintf()` via `<stdio.h>` instead of a custom `extern
  FUN_00466d60(...)` shim (byte-identical either way post-relocation-mask, but removes a
  U-tier naming-debt entry and the shim declaration entirely) — since these addresses are past
  `TOTAL_APP_END`, this doesn't move `progress.py`'s EXACT/PARTIAL app-region totals, only
  `lint_names.py`'s naming debt.
- **The LIBCMT.LIB-extraction technique scales to a whole sweep of the CRT-boundary zone, not
  just one-off lookups — `tools/extract_lib_member.py` is now a saved, reusable tool (v314)
  covering both discovery paths.** (1) **Symbol lookup**: when a candidate function's disasm
  shows a `call`/relocation whose target NAME you already know (e.g. from a sibling function's
  own extracted `.obj` relocations, via `objdump -r`) but you don't know which `.obj` DEFINES
  it, don't guess filenames — parse the archive's own FIRST special member (name `"/"`, the
  linker's symbol index: a big-endian `[count][offsets...][NUL-separated names]` blob) to get
  an exact `symbol name → member file offset` map, then find which member's header brackets
  that offset. This is exact and free of guessing (confirmed: `__ld12tod` → `intrncvt.obj`,
  found this way after `fltinf.obj`'s own relocations named it as a call target with no
  same-named `.obj` anywhere in the archive). (2) **Bulk section-size scan**: when you have a
  candidate function's real byte SIZE (from Ghidra's `Body: start - end`, corrected per the
  extent-mis-measurement gotcha below) but no name lead at all, iterate every member in the
  archive, `objdump -h` each, and filter for any `.text` section whose size is close to the
  target — narrows ~627 members down to 2-4 real candidates in one pass (confirmed: found
  `x10fout.obj`'s `_$I10_OUTPUT`, the classic MSVC long-double-to-decimal-string conversion
  routine, this way after by-name guessing through `cvt.obj`/`fcvt.obj`/`gcvt.obj`/`ctype.obj`
  all failed) — beats sequential by-name guessing once 3-4 plausible object names have already
  missed. **A function's Ghidra-recorded `Body` extent can be genuinely TOO SHORT when the
  function ends in a jump table + case-body data that Ghidra hasn't claimed as part of the
  function** (confirmed: `_strgtold12`/0x4730e0's Ghidra body read 1631 bytes, but the real
  `.obj`'s matching `.text` section is 1936 bytes; the next real function turned out to start
  EXACTLY 1936 bytes after 0x4730e0, proving the extra 305 bytes — raw jump-table/constant data
  disassembling as garbage instructions — genuinely belongs to the same function and Ghidra
  simply hadn't extended the body over it). When a byte-diff against an extracted `.obj`
  candidate looks aligned everywhere except a tail region that disassembles as nonsense on
  BOTH sides, that's confirmation (not refutation) of the match — the "diff" is just two
  different tools rendering the same raw data bytes as fake instructions differently. Session
  v314 total: renamed 8 more past-`TOTAL_APP_END` internals this way — `_input`/0x46cea0 (the
  scanf engine) plus its 4 small static helpers bundled in the same `input.obj`
  (`_hextodec`/0x46dbe0, `_inc`/0x46dc20, `_un_inc`/0x46dc50, `_whiteout`/0x46dc70),
  `_isctype`/0x46b0c0 (shared by several of the above), `_strgtold12`/0x4730e0 (decimal-string
  → 12-byte extended-float parser, the scanf-side counterpart to `_input`), and
  `_I10_OUTPUT`/0x473870 (`$I10_OUTPUT`, extended-float → decimal-string, the sprintf-side
  counterpart) — none referenced from `src/`, so (like the `sprintf` find) this doesn't move
  `progress.py`'s EXACT/PARTIAL/idiom-debt totals, only closes Ghidra-side `FUN_`/naming-debt
  gaps in the CRT region. Untried follow-up: the archive's linker-member symbol index covers
  the WHOLE library, so a promising next step for any remaining opaque CRT `FUN_` is to grep
  its callers'/callees' relocations for named symbols FIRST (path 1) before falling back to the
  size-scan (path 2).
- **A FOURTH discovery path (v321): the reverse sweep — slide every reloc-masked function body
  in a whole `.LIB` over the exe's `.text` and print every zero-diff location.** (`tools/sweep.py`
  — promoted out of the v321 session scratchpad in v322.) `extract_lib_member.py`
  verifies one candidate; the sweep finds functions whose address you don't know at all. Anchor
  on the longest ≥6-byte unmasked byte run (`bytes.find`) before the full masked compare, or the
  O(members × .text) cost is untenable. ~230 CRT/iostream identifications in one pass. Three
  caveats learned: (1) byte-identical twins are common — `istream`/`ifstream` vbase dtors,
  `_fread`/`_fwrite`, `__lock_file`/`__unlock_file`, `_read`/`_lseek`/`_write` all match at the
  same address because the distinguishing call target is inside a masked relocation — always
  disambiguate multi-symbol hits by decompiling and checking the actual call target/callee, never
  rename from the sweep line alone; (2) short matches can be FALSE POSITIVES on real app code
  (a `stdiobuf`-dtor-shaped hit at 0x44f510 was an app SEH ctor — skip app-region hits unless the
  decompile confirms); (3) CRT objs pack MULTIPLE functions per `.text` section (no /Gy), so any
  per-section-length equality check silently misses everything but the first function — slice
  from the symbol's value, not the section start.
- **The exe's statically-linked CRT is a MIX of source revisions, not a single stock
  `LIBCMT.LIB` (v321).** Most modules byte-match `toolchain/vc50/LIB/LIBCMT.LIB` exactly, but
  the `mbctype`/`aw_str`/`aw_map` cluster is a different revision: `_setSBUpLow` exists as a
  real outlined function where stock inlines it, and the exe's `__crtGetStringTypeA`/
  `__crtLCMapStringA` call their `__imp__` slots directly where stock uses the `f_use`
  cached-pointer branch. When a whole cluster fails to byte-match while everything around it
  matches, don't re-grind lib revisions — identify semantically against the CRT source
  (`icidicf/library` GitHub mirror has VC6-era `CRT/SRC/*.C`) and rename with the semantic
  evidence recorded.
- **Ghidra MCP endpoint quirks (v321):** `rename_function` wants JSON POST body
  `{"old_name":...,"new_name":...}` with `?program=Loco.exe` in the query string (GET params
  `old_name`/`new_name` are silently ignored → "Old function name is required"; JSON without the
  query param acts on the CURRENT program, which may be another project's). `create_function`
  same pattern (`{"address":"0x..."}`), works to claim raw-bytes regions Ghidra never made a
  function for. `get_xrefs_to?program=...&address=...` works as plain GET. **⚠
  `rename_function` with an `Ns::Name` new_name bakes the full string as the function's NAME
  inside its CURRENT parent (→ `OldNs::NewNs::Func`), it does NOT re-namespace (v329).** To
  move a function into a (partial-view) namespace use `run_script_inline`:
  `st.getOrCreateNameSpace(globalNs, "Ns", USER_DEFINED)` + `f.setName("Func", USER_DEFINED)`
  + `f.setParentNamespace(ns)` — the 0x44ce10/`PeerTrainNodeLayoutPartial` precedent.
- **A THIRD discovery path (v315): follow a known CRT entry point's own EXTERN call graph one
  hop at a time, matching each hop's Ghidra body size against the LIBCMT.LIB member that
  defines the called symbol.** `setlocal.obj`'s `_setlocale` calls `___init_time`/
  `___init_numeric`/`___init_monetary`/`___init_ctype`/`___init_collate` as externs (sec-0 in
  its own symbol table = declared, not defined) — `--symbol` lookup on each found the 5
  defining `init*.obj` members in one shot, no size-guessing needed. Inside `inittime.obj`,
  `___init_time` itself calls a FILE-LOCAL static (`__get_lc_time`) whose body-size (0x380)
  matched a previously-opaque 881-byte app-region-adjacent `FUN_` almost exactly — confirmed
  byte-for-byte (relocation-masked), closing v314's parked "locale-info bulk loader" lead in
  one session. Chase a CRT entry point's OWN declared-but-undefined callees before falling back
  to name-guessing or the size-scan — it's exact, not probabilistic. **Corollary: this path can
  also surface a genuine Ghidra function-boundary gap, not just a naming gap** — `___init_time`
  itself (the caller of `__get_lc_time`) turned out to be a 174-byte stretch of `.text`
  (`0x471c40`–`0x471ced`) that Ghidra had NEVER boundaried as a function at all (sitting
  between two already-recognized neighbors with no function claiming it) — `create_function`
  at its real start (found by walking backward from the first confirmed instruction until
  hitting the preceding function's `ret`+padding, same recipe as the established "Ghidra-missed
  function gaps" bullet) then let the byte-diff confirm it. Always check `get_function_by_address`
  on a CALLER address before assuming it's already boundaried — "no function found" is the tell.
- **A FOURTH discovery path (v318): when one entry of a data TABLE (not just one call graph) is
  already known, read the surrounding memory as fixed-stride rows and every sibling entry falls
  out at once — faster than chasing each sibling's own call graph individually.** `_setlocale`'s
  per-category init-function table was found this way: v315 had already pinned `__init_time`'s
  address via a DATA xref to `0x4842cc`; reading raw dwords backward from that address in fixed
  0xc-byte (3-dword) rows (`{pCategoryNameStr, pCurrentLocaleNamePtr, pInitFunc}`) surfaced the
  other 4 categories' init-function addresses (`__init_collate`/`__init_ctype`/
  `__init_monetary`/`__init_numeric`) in one read, in the CRT's own canonical category order —
  no `--symbol`/size-guessing needed, and all 4 needed `create_function` first (same
  Ghidra-boundary-gap pattern as discovery path 3). Use `run_script_inline` with
  `Memory.getInt(address)` over a small range to dump the raw dwords; row size/stride is usually
  guessable from one already-known good entry's neighboring values (a repeated shared-default
  pointer like `0x484280` here marks the row boundary). Reach for this whenever a "one hop at a
  time" call-graph chase (path 3) would need to repeat the same lookup pattern across several
  known-related callees — the table walk gets them all simultaneously.
- **A "shared helper" called from every error path with a `push &local; push <fixed constant>;
  call` shape is likely a bare `throw <value>;`, not a real function (VC5-confirmed,
  `Rf_ThrowLoadErrorMaybe` ex-`FUN_00466ce0`, `src/Wav.cpp`).** Ghidra's decompile of the
  callee showed it copying an 8-dword template from a fixed global, splicing in the 2
  caller-passed pointers, then calling `RaiseException(code, flags, nArgs, &args)` — textbook
  MSVC C++-EH-via-SEH codegen for throwing a POD (the callee IS the compiler's own
  `_CxxThrowException`-equivalent, statically linked, unnamed by Ghidra). The tell at each call
  site: `push &<local>` (address of the value being thrown, NOT the value itself) plus a FIXED
  constant second arg (the compiler-synthesized `_ThrowInfo` descriptor for that type — the
  same constant at every site sharing that thrown type). Modeling the call site as a real
  extern function call (passing the value BY VALUE, one push) produces a wildly different,
  much-larger structural diff at every site (closed ~90% of a 566-byte residual by switching
  `Rf_ThrowLoadErrorMaybe(0xe102);` → `throw 0xe102;` — the surrounding `try`/`catch(...)`
  makes this especially easy to miss since the call SITE already compiles fine and LOOKS like a
  normal opaque cross-TU call). Before modeling any "shared error/log/assert helper" called from
  many error branches as an ordinary function, check `xrefs_to` the vtable/call target's own
  decompile for a `RaiseException`/copy-template-then-call shape first.
- **Never transcribe a real `try`/`catch` as `catch (...) { }` without first decompiling the
  catch funclet itself — Ghidra's exception-handling analyzer names and decompiles it
  directly (VC5-confirmed, `Wav_ParseAndLoad`, `src/Wav.cpp`).** Ghidra auto-labels the
  funclet `Catch_<addr>` (found right after the guarded function's own `ret`, before the next
  function marker) and its decompile shows the REAL handler body — in this case
  `*(ebp-0x20) = *(ebp+8); return <continuation-addr>;`, i.e. `catch (int nErr) { nReturnCode
  = nErr; }`, not an empty swallow. The tell that our `catch (...) {}` transcription was wrong
  wasn't visible in the try-block's own code at all — it showed up as the optimizer
  constant-folding the return value to `xor eax,eax` (since an all-empty catch really does
  leave `nReturnCode` provably 0 everywhere), while the original's `mov eax,[[stack slot]]`
  load proved the real return value was NOT constant. **An unexpectedly-constant-folded return
  variable near a `try`/`catch` is a signal to go re-read the catch handler's actual body, not
  just the try block.** Separately: the compiler reuses an already-dead PARAMETER's stack slot
  ([ebp+8], `pszPath`, unused past the resource-acquisition step) to stash the caught
  exception's value during unwind dispatch — expect this "dead slot repurposed as EH scratch"
  pattern generally, don't assume a funclet's `[ebp+N]` reads necessarily mean "reads that
  parameter" once past the point the real parameter was last used.
- **A user `try` block's own extent (not just its handler) is a real, source-position-relevant
  fact recoverable from the SEH state-number sequence — don't wrap more into `try { }` than the
  original does (VC5-confirmed, `Wav_ParseAndLoad`).** `new T(args)` under `/GX` gets
  compiler-automatic alloc-protection (free the raw block if the ctor throws) REGARDLESS of
  whether it sits inside a user `try` — this uses its own small integer states (0, 1, ...) that
  always reset to **-1** ("no protection active", the same sentinel used for code genuinely
  outside any try). A user `try` block gets ITS OWN next-integer state, entered only at the
  try's real opening brace — if that state is a DISJOINT number from the `new`-protection
  states (never reachable by resetting to -1 first), the `new` calls are NOT lexically inside
  the user's `try`, even if a naive first-pass transcription put them there. Symptom when wrong:
  every `new` call's own protection states appear correctly, but the try-block's own state
  number is simply ABSENT from the compiled output, and the function's total stack frame is
  short by however many bytes that missing state-transition's surrounding codegen would have
  used. Fix: narrow the `try` to start at the first statement that can actually reach a
  matching `throw`, moving prior resource-acquisition code (which only needs the automatic
  `new`-protection, not the user handler) above/outside it.
- **An unreferenced-but-reserved stack-offset gap between two named locals often means a
  CALLEE writes more than the caller's own field count into that buffer — check the callee's
  write size before assuming an unmodeled local sits in between (VC5-confirmed,
  `Wav_ParseAndLoad`/`Wav_ReadOrFindChunkMaybe`, `src/Wav.cpp`).** `chunkHdr` sat at `ebp-0x38`
  in the original vs. our `ebp-0x30` (an 8-byte/2-dword gap) even though `ebp-0x28`/`ebp-0x2c`
  were never directly referenced anywhere in the caller's own disasm. Root cause:
  `Wav_ReadOrFindChunkMaybe`'s chunk-copy loop unconditionally writes **5 dwords** into
  whatever `pOut` buffer the caller passes, regardless of how many fields the caller actually
  reads back (2-3 of them are documented "sic" garbage) — the caller must still reserve the
  callee's full write size (`int chunkHdr[5]`, not `[3]`) or the two extra dwords land on
  whatever's stack-adjacent. Fixed by widening the array; closed the offset gap and the
  associated structural score gap exactly. General move: when a size mismatch between a local's
  declared size and its accesses shows up as an inexplicable stack gap, read the SIZE CONTRACT
  of whatever function populates it (its own loop bound / write count), not just what the local
  is read back as.
- **Paired throw-site/catch-funclet dead-parameter-slot reuse can be a genuinely intrinsic
  SWAPPED pair, not independently steerable (VC5, `Wav_ParseAndLoad`).** Every `throw CONST;`
  call site reuses one dead parameter's stack slot as scratch for the value-to-throw (`lea
  eax,[ebp+N]; mov [ebp+N],CONST; call <throw-machinery>`), and the catch funclet separately
  reuses a (possibly different) dead parameter's slot to stash the caught value. In this
  function the original picks `pOut` for the throw-site scratch and `pszPath` for the
  catch-funclet scratch; a straightforward transcription picked the exact opposite for BOTH,
  consistently at every one of the function's throw sites — i.e. one coupled compiler
  tie-break, not several independent ones. Both parameters are legitimately dead at every site
  in both versions (the ABI/signature can't be changed to break the tie), so treat this as the
  same intrinsic class as the documented register-swap residuals (Yoda #29/#30) extended to
  stack-slot reuse — don't sink further budget re-probing it without a genuinely new idea.
- **⭐ Local DECLARATION ORDER is load-bearing whenever one of the locals is a non-POD (VC5,
  v396, `TutorialWnd::ResourceRefCategoryTable_LoadCategoryFile` 0x44fb10).** A constructor call
  is a statement, so it is a hard barrier the initializers of the surrounding PODs cannot be
  scheduled across. Here `char szPath[0x105] = "";` (which compiles to the pooled-empty-literal
  byte copy plus a `rep stosd` tail zero) and `char bLoaded = 0;` both have to be declared
  BEFORE `ifstream fileStream;` to reproduce the original's `szPath-init, stream-ctor` order;
  declaring the stream first sinks both inits past the ctor and past the following `sprintf`.
  Worth 168870 -> 89366 -> 77362 on its own, in two compiles. Read which side of the ctor call
  the buffer-zeroing block lands on in the original and order the declarations to match — this
  is a cheap, high-yield first probe on any function that mixes a stream/RAII local with
  initialized buffers.
- **⭐ A redundant-but-harmless store that Ghidra prints is not necessarily in the source, and
  writing it costs a real instruction (VC5, v396, same function).** Ghidra decompiled the
  archive branch as `cVar6 = '\0'; if (pStream != 0) {...}`, but `bLoaded` is provably still 0
  from its own initializer at that point, so cl cannot fold the redundant store away — emitting
  it gives insns 113/112, dropping it gives 112/112 (and 77362 -> 61358). **When the decompiler
  shows a store of a value the variable demonstrably already holds, check the instruction count
  both ways before believing it.** The decompiler is reconstructing dataflow, not source: it
  will happily materialize an assignment for a value that was only ever established by an
  initializer further up.
- **The `bool <name> = <ptr> != NULL;` intermediate generalizes across TUs, not just within
  one.** The RF-archive guard (`g_RFIndex.pFile != NULL`) needs it in BOTH
  `CreditsWnd::BuildResourcePath` and `TutorialWnd::ResourceRefCategoryTable_LoadCategoryFile`
  to produce the original's `xor r,r; cmp; setne rl; test rl,rl` rather than a bare `cmp`/`je`.
  When you meet the same library idiom in a new TU, port the lever with it.
- **`bool` return widens to full EAX; `unsigned char` + early-return doesn't (VC5, ×7):** if
  the original ends in bare `setz al`/`sete al` or `xor al,al`/`mov al,1` with no EAX-wide
  clear, use return type `unsigned char` AND the `if (cond) return 1; return 0;` shape (a
  plain `return cond;` still widens even under `unsigned char`). Apply by default.
  Confirmed again v505 (0x42cf90's tail: `bool`/`char`/`unsigned char`-temp spellings all
  emitted the widening `xor eax,eax; cmp cl,1; sete al`; only `unsigned char` return +
  early-return reproduced the original's `mov al,[f]; cmp al,1; sete al`).
- **⚠ The member→free-`__fastcall` conversion is byte-neutral at the CALL SITES but NOT
  necessarily inside the CALLEE (v505, 0x42d280).** The CloseMaybe-pattern free function
  (`void __fastcall F(T *pThis, ...)` standing in for a member) emits the same bytes at
  every caller, but the callee's own /Og state shifts with the extra explicit parameter:
  0x42d280 matched EXACT as a real member and dropped to a one-row scheduling swap
  (86/86 insns) as the free form, with every source spelling refuted. When the free form is
  forced by a header-parity victim, re-verify the function itself — it may have traded its
  own EXACT for the parity fix. Related, same session: retiring the just-added declarations
  restored the parity victim (0x4588b0) exactly, and an elaborated `class T*`
  parameter-specifier still counts as a declaration toward the dial (0x457ce0) — only
  spelling the free callees as same-named methods on the TU's existing view struct kept the
  declaration count unchanged.
- **⭐ A TU-local DERIVED VIEW class (`struct FooView0xADDR : public RealClass { ... }`)
  carries member bodies at ZERO declaration-dial cost and compiles byte-identical to
  member-of-class (v506).** When a shared header's declaration budget is already spent
  (WorldActionCursor.h admits exactly ONE new declaration — measured by bisecting 4→3→2→1 —
  before 0x4588b0 loses its EXACT; the dtor spent it), declare the remaining new methods on
  a derived view in the owning .cpp: no other TU sees them, and unlike the free-`__fastcall`
  form (bullet above) the callee's /Og state does NOT shift — all three of
  SelectedObjWidgetMaybe's vtable-tail bodies (0x42d6b0/0x42d770/0x42d400) stayed EXACT on
  the view. Tag each definition `// TODO: sync (TU-local view)` for lint_ghidra_sync.
  Multi-arg bodies can't use the free form anyway (VC5 `__fastcall` puts arg2 in EDX where
  the original's thiscall passes it on the stack).
- **⚠ A shared-header dial can have SEVERAL simultaneous victims, and a SIGNATURE change
  spins it exactly like an added declaration (v506).** Retyping WidgetBase.h's slot-19
  placeholder (`virtual void *_v19()` → the honest `(MenuNode*,int,int)->char`, pinned by
  0x44ec50's own dispatch) cost 0x4588b0 (143 B) AND WorldBoardMaybe.cpp's 0x457ce0
  (951 B) their EXACTs at once — reverted; the call went through a TU-local
  CarNetObjVtblProbe-pattern struct instead (`// TODO: idiom`-tagged, class F). When the
  honest modeling is a shared-header edit, ALWAYS price it against a probe fallback before
  keeping it.
- **⭐ A `char`/`int` PARAMETER stored into a `bool` FIELD costs an
  `xor r,r; test bl,bl; setne rl` triple — if the original just does `mov [reg+off],bl`, the
  parameter is `bool` (VC5, v353, `PeerTrainNode::SetCarsReady` 0x44d500).** The store site is
  the diagnostic, not the call sites: every caller passes a literal `0`/`1`, so the parameter's
  declared type is invisible from outside and both spellings compile the calls identically. This
  is the store-side twin of the `movsx`/`movzx` signedness tell — a `setne` in front of a byte
  store is a TYPE fact, never a scheduling tie. Retyping the parameter took the function from
  DIFF(108) to byte-EXACT in one compile.
- **⭐ A "changed?" test written as the obvious `(a != 0) != (b != 0)` folds to ONE test of each
  operand; the original's DOUBLE test of one operand means the source spells out both conjuncts
  (VC5, v353, same function).** `if ((bFlag && !p->bReady) || (!bFlag && p->bReady))` evaluates
  `bFlag` twice — once per conjunct — and MSVC emits exactly that: `test bl,bl; je L2;` …
  `test bl,bl; jne next; L2:` with a fall-through into L2 that is provably never taken. That
  apparently-dead re-test is the fingerprint. Do not "simplify" it to `!=`; the redundancy is
  the source shape, not compiler noise.
- **⭐ `delete p;` emits its own null re-test even inside `if (p != 0)` — but only if the operand
  is RE-READ from memory, not cached in a local (VC5, v353,
  `PeerTrainNode_AllocCarSlot` 0x44c220).** MSVC's `delete` lowering inserts `test reg,reg; je`
  before the vtable-slot-0 dispatch at a stage before redundant-branch elimination, so a
  `delete this->slots[i];` keeps the test while a `pCar = this->slots[i]; … delete pCar;` lets
  value-numbering fold it away. If a `delete` site is missing that redundant test, stop caching
  the pointer in a local and re-read it at the delete (total 34020 → 22016 here).
- **A byte-sized field's PRE-reassignment value, spilled to a stack slot for a later
  end-of-function comparison, needs the LOCAL declared `unsigned int`, not the field's own
  narrower type, to reproduce the spill (VC5, `EditCardWnd::SelectDecalSubkind`/
  `CycleDecalKindHighlight`, v218).** `unsigned char byOld = narrowByteField;` compiles the
  spill/reload as a plain byte store/load; the original zero-extends to a full dword at the
  spill point (`xor eax,eax; mov al,field; mov [slot],eax`) even though the field itself stays
  1 byte everywhere else. This is the SAME family as the already-documented `unaff_EDI`/
  `unaff_BL` stack-slot-mistracking artifact (a Ghidra decompile bug, not a hidden parameter) —
  but fixing the mistracking diagnosis alone isn't enough; the local's WIDTH is an independent
  lever that must also be corrected before the spill instruction sequence matches byte-for-byte.
- **The MIRROR of the byte-spill lesson: narrowing an INT source field to a BYTE store
  (`byteField = intField`) byte-loads by default (`mov al,[src]`), but the original often
  materializes the full 32-bit source first (`mov eax,[src]; mov [dst],al`) — reproduce it with
  an `unsigned int` temp read AFTER any preceding stores so it isn't hoisted (VC5,
  `DPlaySessionMgr::LayoutNet_RequestLayoutList`, v236, closed an EXACT match's last byte).**
  Ghidra renders the original as `(char)*(undefined4*)(src)` (a full-dword read cast to char) —
  that dword read is the tell. `(char)`/`(unsigned char)`/plain-assignment narrowings ALL
  byte-load (the compiler proves only 8 bits are needed and reads just the byte); only forcing an
  intermediate 32-bit value keeps the dword load. But declaration POSITION matters: an
  `int idx = field;` declared at function top HOISTS the load to the prologue (into a
  callee-saved reg, wrong); declared just before the byte store — AFTER the preceding field
  stores it must not precede — it lands the dword load in place. Same `unsigned int`-width lever
  as the byte-spill lesson above, inverted (source-narrowing vs value-widening).
- **A struct field's WIDTH (not just its offset) can be wrong even after a prior session
  "confirmed" it via a `pad`-array split — check the WRITE instruction's operand size, not just
  its offset (VC5, `EditCardWnd::bNoMoreRosterRows`, v218).** A field split out of a
  generic `pad0xNN[N]` block in an earlier session (idiom lint class D) had been declared
  `unsigned int` (4 bytes) purely because that was the pad block's own natural dword alignment
  — but the writer function's real disasm showed `mov BYTE PTR [x],0/1`, a genuine 1-byte
  write. Fixing the width (shrinking to `bool`+3 real padding bytes) improved FOUR already-
  transcribed, unrelated functions' own byte-match scores as a side effect, not just the one
  being actively worked — a wrong field width desyncs the compiler's struct-layout/alignment
  reasoning for every later-offset field too, even though the actual byte offsets don't move.
  Always cross-check a "confirmed" pad-split field's write-instruction WIDTH against its
  declared C++ type before trusting it, especially if it was only ever inferred from block
  alignment rather than a real write site.
- **A boolean value passed as a call ARGUMENT often needs literal `if (cond) { call(1,...); }
  else { call(0,...); }` branches, not a computed `cond ? 1 : 0` / bare `cond` expression (VC5,
  `EditCardWnd::SyncDecalPickerButtonHighlights`, v218).** The original pushes a literal
  `push 0`/`push 1` at each branch's own call site (reached via ordinary conditional jumps);
  a computed boolean expression compiles instead to `cmp/setcc/movzx`-style codegen feeding a
  single shared call — same content, structurally different bytes. Distinct from (but
  compositionally stacks with) the already-documented "duplicated-call `if`/`else` shape for a
  multi-arg variadic's format string" lesson — this one is about a single boolean ARGUMENT, not
  the whole call.
- **A whole-struct copy from a source RECT/similar aggregate, with one field immediately
  overwritten afterward, needs the OVERWRITE's own input value computed BEFORE the copy
  statement, not after (VC5, `EditCardWnd::RedrawColorChannelBars`, v218, closed a
  133246→6003-point residual by reordering alone).** `fillRect = rectUnk0x2NN; int h = calc();
  fillRect.top = fillRect.bottom - h;` scored dramatically WORSE (314470) than `int h = calc();
  fillRect = rectUnk0x2NN; fillRect.top = fillRect.bottom - h;` — same 3 statements, reordered.
  The original reads all 4 source fields via one cached base pointer (even the one about to be
  discarded) specifically in this position relative to the height calc; extends the "faithful
  struct-pointer walk reproduces a hand-computed offset" family (`BuildPaletteLUTMaybe`) with a
  STATEMENT-ORDER dimension, not just a field-access-shape one — when a struct-copy-then-
  overwrite pattern's first attempt scores worse than a field-by-field baseline, try moving the
  copy to a different position relative to the overwrite's own input computation before
  concluding the struct-copy hypothesis is wrong.
- **VC5 emits a jump-table switch's CASE BODIES in SOURCE CASE ORDER (v327,
  `PeerTrainNodeLayoutPartial::LayoutCarAnchorsMaybe`):** when the original's case-block
  stream order is non-numeric (e.g. bodies laid out 2/8, 4/0xa, 1/7, 3/9 while the table
  maps them numerically), reorder the SOURCE `case` labels into that exact order — the
  compiler lays blocks down in written order. Single biggest lever for that function
  (asmscore 762k→599k). Corollary: the same dispatch value can be a jump-table switch in
  one loop and an if/else-if CMP-chain in another of the SAME function — check the
  original for a table before writing `switch`; a cmp-chain with two tail-merged shared
  blocks = source if/else-if with duplicated tails the linker/optimizer merged.
- **The `/GX` EH-STATE LADDER IS A SOURCE-CASE-ORDER ORACLE — read it off the disassembly
  BEFORE writing the `switch` (v355, `UIResources::TileKind_CreateDescriptor`/0x446840).**
  Every protected `new T(...)` gets a `mov dword ptr [esp+N], <state>` immediate, and those
  immediates are assigned in CODE-LAYOUT order — which, per the v327 lesson above, IS source
  case order. So on a switch whose arms are all `new` expressions, sort the arms by their
  state number and write the `case` labels in exactly that order. This turned an entirely
  non-numeric source ordering (`2/4`, `12/13`, `7/8`, `6`, `3`, `1`, `5`, `9/10/11`, `14`,
  `default`, with the codegen-free `case 0:` anywhere) into a first-compile byte-identical
  switch body — no bisecting of case permutations at all. Generalizes past `switch`: any two
  protected `new`s tell you which one the author wrote first.
- **Put the store INSIDE each switch arm, not once after the switch — a jump-table entry that
  points PAST the shared store block proves it (v355, same function).** 12 of the 14 arms end
  in the identical `m_apKindDescriptors[kindId] = pNew;` and the compiler tail-merges them
  into ONE shared block; a no-op `case 0: break;` whose table entry lands AFTER that block is
  the tell that the store cannot have been written once below the switch (a `break` from a
  post-switch store would have to run it). Writing `p = new X(...); break;` + one store below
  gets the two special arms that skip the store wrong; writing the store per-arm and letting
  the optimizer merge gets all 14 right.
- **`delete p;` keeps its implicit null test only when the operand is a MEMORY expression
  (v355, same function; DIFF 918 → 809 together with the next bullet).** MSVC 5 eliminates
  `delete`'s `if (p)` when `p` is a local it has just proved non-NULL a few lines above, but
  NOT when the operand is re-read from memory (`m_array[i]`) each time — CSE still collapses
  the loads to one register, so the asm looks like a cached local either way. If the original
  keeps `test ecx,ecx; je` in front of the vtable-slot-0 call, write the whole guard chain AND
  the `delete` against the member/array expression instead of caching it in a local.
- **A byte result spilled at function entry (`mov byte ptr [esp+N], 1`) and reloaded at only
  ONE return path, while another path materializes the constant with `xor al,al`, means a
  real `unsigned char` local — not `return 1;`/`return 0;` literals (v355, same function).**
  Shape: `unsigned char b = 1; ... if (bad) { ...; b = 0; } return b;`. The reject path's
  `b = 0` is copy-propagated into `xor al,al` because the store and the `return` end up in the
  SAME basic block; the accept path jumps into the return from elsewhere and must load the
  spill. Literal returns delete the local entirely, and with it the prologue's `push ecx`
  frame slot — which shifts every `[esp+...]` displacement in the function, so this is worth
  far more than the two instructions it looks like.
- **⭐⭐ SOLVED (v356) — the `sete`-MATERIALIZED BRANCH is a BYTE-RETURNING INLINE PREDICATE.**
  When the original tests a global against a constant as
  `mov r,[g]; xor r2,r2; cmp r,K; sete r2l; test r2l,r2l; je/jne` instead of a plain `cmp; je`,
  the source is NOT `if (g == K)`. It is `if (SomePredicate())` where the predicate is an
  `inline unsigned char` function returning `g == K` — the byte return type forces the value
  through a byte register, and the `if` on that byte is the `test rl,rl`. This class had been
  parked since v334 across five functions in four TUs and repeatedly declared "unsteerable";
  the earlier diagnosis (that MSVC was spilling the compare flag because a register got reused
  between the `cmp` and the branch) was simply wrong. Measured wins, one source change each:
  `GameNetMsgQueue::EnqueueOrFreeNode` (0x4393d0) **total 12113, byte_diff 3, insns 86/86** —
  down from a whole-function cascade that read as DIFF(~192); `WorldBoardPartial::UpdateDirtyTiles`
  (0x456150) 509762 → 424323; `WorldActionCursor::SelectDecorObjAndDispatchModeMaybe` (0x459180)
  513668 → 479365; `UIResources::SoundBank_PreloadWavRange` (0x446cc0) 206415 → 149517.
  A shared app-state global read at several different constants wants ONE PREDICATE PER TESTED
  STATE (`IsInGameModeMaybe()`, `IsNetShuttingDownMaybe()`, …), which is almost certainly how
  the original source was written. **Grep any parked autopsy for "sete" and retry it.**
  ⚠ Two caveats. (1) The predicate must be `unsigned char`, not `int` and not `bool` — an `int`
  return gives back the plain `cmp; je`. (2) It is a LOCAL fix whose global effect must still be
  measured: at 0x456700/0x456890 the original genuinely has the sete shape, yet routing that one
  site through the predicate nets the function WORSE (478854 → 488419) because of
  register-allocation fallout elsewhere. Score the whole function, not the gate.
  Keep the predicates TU-LOCAL rather than hoisting them next to the shared `extern` they wrap —
  adding declarations to a shared header rotates other TUs (v340/v355/v356).
  ⚠ (3) NEW SUB-CASE (v394): an original whose `setcc` has NO `xor r,r` in front of it is only
  PARTIALLY steerable. At 0x449a6f the original is `mov eax,[g]; cmp [eax+0x7c4],2; setne al;
  test al,al; je` — it reuses EAX for both the pointer and the byte and never zero-extends.
  The predicate lever still EARNS ITS KEEP there (it supplies the `setne`+`test` pair the plain
  `if` does not: 352/352 vs 350/352 instructions, total 6228 vs 12005), but the widening itself
  resisted every spelling tried: an `unsigned char` inline predicate, a `bool` inline predicate,
  an `unsigned char` predicate taking the pointer as a PARAMETER, an `unsigned char` LOCAL
  assigned from the comparison, and an explicit `(unsigned char)` cast inside the `if` all
  compile BYTE-IDENTICALLY, always emitting `xor eax,eax` and pushing the pointer into ECX.
  So: use the predicate, expect the 3-byte `xor`+register residual, and don't re-grind it.
  ⚠ (4) The same non-steerable widening shows up WITHOUT any predicate, whenever a `char`-returning
  callee's result is normalized into a variable that must survive intervening calls (v395,
  `BuildToolButton::InitMenuIconsMaybe`): the original is `test al,al; setne bl`, ours is
  `mov bl,al; neg bl; sbb ebx,ebx; neg ebx` — cl keeps the value in a 32-bit register and uses
  the `neg/sbb/neg` bool idiom instead of a byte `setcc`. `bool x = f();` (the implicit C4800
  conversion), `bool x = f() != 0;` and `unsigned char x = f() != 0;` are all BYTE-IDENTICAL.
  Two instructions; budget for it and move on.
  ⭐ (5) **BUT the RETURN-STATEMENT case IS steerable, and a plain `?true:false` ternary is the
  lever (v434, `GNetManager::DPlay_ProbeModem` 0x45eec0 — DIFF 19 → EXACT, 451 → 449 B).** For a
  `bool`-returning function, `return byteExpr != 0;` and the implicit `return byteExpr;` both make
  VC5 emit a full-width `xor eax,eax` ahead of the `setne` — which ALSO evicts the loaded byte
  from al into cl, so the residual reads as a register coin flip on top of one extra instruction.
  Spelling it `return byteExpr != 0 ? true : false;` (or the inverted `== 0 ? false : true;` —
  both are byte-identical) drops the `xor` and reuses whatever eax already holds, exactly like the
  original's bare `test al,al / setne al`. Same family as the v433 lever on
  `WidgetPickerObj0x477cc8::OnKeyDownMaybe`'s VK_BACK arm, where the ternary was likewise the only
  spelling that avoided the `neg/sbb/neg` normalization — so treat "a lone `xor eax,eax` or
  `neg/sbb/neg` next to a bool materialization" as **always worth one ternary probe**, at a return
  statement or an assignment alike. It is item (4)'s intervening-call case that is genuinely
  non-steerable, not bool materialization in general.
  ⭐ **(5b) NEW (v454) — item (5)'s ternary lever is NOT limited to `bool`-returning functions or
  to byte-valued expressions.** `NameAnchorMaybe::ExtendAcrossTileBoundaryMaybe` (0x40cb10) returns
  **`unsigned char`** from an **`int == 0`** comparison on a struct field, and behaves identically:
  `return dwTileExtendPhaseMaybe == 0;` emits the full-width `xor eax,eax` ahead of the `sete` AND
  evicts the loaded field from EAX into ECX, while `return dwTileExtendPhaseMaybe == 0 ? true :
  false;` drops the `xor` and reuses EAX, exactly matching the original's `mov eax,[esi+0x1c]; cmp
  eax,ebp; sete al`. DIFF(13) → **EXACT**, both at the original's exact 260 B, on the first probe.
  So the trigger is the RETURN-STATEMENT bool materialization itself, whatever the declared return
  type or the operand width — widen the (5) heuristic accordingly, and note that mixing `true`/
  `false` into an `unsigned char`-returning function is the faithful spelling, not a type error.
  ⚠ Diagnostic caveat that nearly hid this one: on a switch-heavy function the `insns A/B` headline
  overstates the gap badly (here 99/99 was fine, but its twin 0x40cc90 read `63/56` when the real
  content diff was ZERO extra instructions — everything past the last code byte is the jump table
  decoding as masked-relocation noise). Judge by the dump rows ABOVE the table boundary only.
  ⭐ **(5c) NEW (v461) — (5b) confirmed TWICE MORE in one session, and the STORE-TO-A-MEMBER case
  is the one to remember, because nothing about it looks like a return statement.**
  (i) `GeomUtil.cpp`'s `IsPointOnSegmentMaybe` (0x45c7c0) returns `unsigned char` from an
  `int == int` comparison: the plain `return a == b;` emits `xor ecx,ecx; sete cl; mov al,cl`
  where the original has a bare `sete al`; `return a == b ? true : false;` is **EXACT on the first
  probe**. (ii) `EditCardWnd::StepDecalPickerScroll` (0x419260) stores the flag into an
  `unsigned char` MEMBER twice — `field_0x2b5 = nSlot > 0;` — and each site emits a spurious
  `xor r,r` ahead of its `setg`; `= nSlot > 0 ? true : false;` drops both (total 162975 → 140961).
  So widen (5) once more: the trigger is bool materialization at ANY assignment target, member
  fields included, not just `return`. ⚠ And note the ANTI-lever measured alongside (i): collapsing
  a guard-plus-comparison function into a single `&&` chain (`return inRange && cross == 0;`) is
  strictly WORSE than the nested `if` — cl stops using `setcc` entirely and branches
  (`jne` / `mov eax,1` / `xor eax,eax`), which is the opposite of what you want. Keep the guard as
  a real `if` and put the ternary on the inner comparison.
  ⭐⭐ **(6) NEW (v452) — the lever EXTENDS TO MULTI-CONDITION PREDICATES, where there is no
  `setcc` at all and so nothing looks like the v356 signature.** When a short-circuit `&&` guard
  is materialized into a VALUE — `cmp a,0 / jne F / cmp b,0 / jne F / mov eax,1 / jmp T /
  F: xor eax,eax / T: cmp al,<zero-reg> / je` — that 5-instruction block is an inline
  `unsigned char` predicate whose body is the whole `&&`, NOT the condition spelled at the call
  site. Written inline (`if (a == 0 && b == 0)`) VC5 folds the value away into two direct `jcc`s
  and emits none of it. ⚠ **A named LOCAL does not substitute**: `bool b = (a==0 && b==0); if (b)`
  and the `unsigned char` local, and the fully explicit `if (...) f=1; else f=0; if (f)`, ALL fold
  identically — only the predicate keeps the block. Measured on
  `Obj0x478118::LoadMaybe` 0x436490 (DIFF(51) → EXACT, the last residual in the function). This is
  the class `Obj0x4779e0::LoadMaybe` (0x41e6e0) parked as "9 guard phrasings tried, ALL fold" —
  the missing 10th phrasing was a predicate, and none of the 9 were one. ⚠ But retrying it THERE
  still loses (see docs/PARKED.md): the shape lands and the function still scores worse, because
  a lone lever on a function with other live residuals can rotate more than it fixes. The
  predicate's real home is the class whose members it reads; when that is a rotation-locked shared
  header (`CursorDesc.h`), park it on the derived class that needs it and say so.
- **⭐⭐ ACCUMULATE INTO THE BY-VALUE PARAMETER ITSELF, don't copy it into a local first — the
  choice decides which value VC5 parks in the long-lived callee-saved register (v456,
  `AnimDescRefObj0x477488::BlitAnimFrameMaybe` 0x405e60, DIFF(27) → DIFF(14), total 898 → 672;
  baked into its twin 0x405fd0 too).** A function whose `unsigned int flags` parameter is ORed
  three times over a long span (`flags |= nBlitFlags;` … `flags |= 0x20;` … `flags |= 0x40;`
  before finally passing it on) keeps the PARAMETER's own loaded value in EDI for the whole
  function. Writing the idiomatic-looking `unsigned int blitFlags = flags | nBlitFlags;` and
  accumulating into THAT instead makes cl park the MEMBER (`nBlitFlags`) in EDI and re-read the
  parameter as the scratch operand — the two loads swap registers AND swap their order relative
  to the `push edi` that saves the register, which then cascades. The diagnostic is a pair of
  adjacent `r` rows near the top of the dump where the SAME two loads appear in both columns with
  their destination registers exchanged, and where one column's stack displacement for the
  parameter is 4 bytes off the other's (that offset delta is the tell: it means the two sides
  disagree about whether the callee-saved push has happened yet). ⚠ **The commutative operand
  order is NOT a substitute lever and is inert** — `nBlitFlags | flags` and `flags | nBlitFlags`
  compile byte-identically, because cl canonicalizes `|`; only which VARIABLE is the assignment
  target moves anything. Generalize: when a parameter is the natural accumulator in the original,
  spell it that way rather than introducing a differently-named local for readability.
- **⚠ GHIDRA MISREADS A FUNCTION THAT PASSES TWO BY-VALUE `RECT`s AND HAS `unaff_` VARIABLES —
  treat `unaff_EBX`/`unaff_ESI` in a decompile as "this output is unusable", not as a hint (v456,
  `AnimDescRefObj0x477488::BlitOverlayFrameMaybe` 0x405fd0).** On that function Ghidra invents two
  `unaff_` locals and reads BOTH by-value RECT arguments 8 bytes low, which silently splits ONE
  stack local into two and makes two `SetRect` calls that target the same address look like writes
  to different variables. Transcribing it verbatim would have produced code that both compiles and
  looks plausible while modelling the wrong data flow — and would have hidden a real original bug
  (the first computed rect is dead; see docs/engine-bugs.md). Recovery recipe, cheap and worth
  running whenever `unaff_` appears: derive the frame by hand from the raw disasm, then PIN it with
  an independent anchor — pick an instruction whose operand you already know from the calling
  convention (here `cmp BYTE PTR [esp+0x44],1` had to be the `char flag` parameter, and it resolved
  to entry_esp+0x14, exactly where `flag` sits for a `__thiscall(RECT, char, unsigned int)` with
  `ret 0x18`). If the anchor lands where the convention says it must, the whole model is pinned.
  Same standing rule as the delegated-agent one in CLAUDE.md: re-derive the memory layout yourself
  before committing a by-value-struct-argument function to `src/`.
- **A negative result on the swap-two-sibling-locals lever is INFORMATIVE, not just a dead end —
  a much WORSE score pins the original's declaration order (v456, 0x405e60).** Swapping the two
  `int` locals feeding a normalize-then-`SetRect` pair went DIFF(14) → DIFF(224) and shortened the
  function by 5 bytes, which is far too large to be a tie-break: it means the declared order is
  load-bearing and already correct. Compare with v454's 0x427580, where the same lever scored
  IDENTICALLY — there the contest was between a constant and a local, so the lever had nothing to
  grip. Read the three outcomes as: better ⇒ take it; identical ⇒ the lever does not apply to this
  residual; much worse ⇒ you already have the right order, stop probing it.
- **⭐ A scalar `= 0` INITIALIZER on a local declared alongside a NON-TRIVIAL CLASS local gets
  HOISTED ABOVE that object's constructor; the same assignment written as a STATEMENT after the
  declaration block stays below it (v452, `CarKindDesc::LoadMaybe` 0x40e690, DIFF(29) → EXACT).**
  With `int nSize = 0; ... ifstream fileStream;` in one declaration block, cl schedules the
  `mov [esp+N],0` before the `ifstream` ctor call; the original does it after, alongside the
  function's other field zeroing. Diagnostic: a small scheduling-only residual (identical
  instruction COUNT, `reg_pen 0`, a couple of `+`/`-` rows clustered in the first ~0x40 bytes)
  where the moved instructions are all zero-stores. Fix: declare the scalar bare and assign it in
  the body. Same family as the v435 `= NULL` lever above — that one says the initializer EXISTS,
  this one says WHERE it was written.
- **Wrapping a whole function body in `if (arg != NULL) { … }` vs `if (arg == NULL) return;`
  decides whether VC5 DUPLICATES the destructor tail of a non-trivial local (v452,
  `CarKindDesc::LoadMaybe` 0x40e690, DIFF(370) → DIFF(29), and `Obj0x478118::LoadMaybe` 0x436490
  first-compile).** With an `ifstream` (or any class local) in scope, the early-`return` form emits
  a SECOND copy of the dtor+epilogue sequence on the guard's taken edge and jumps to the shared
  frame teardown; the wrapping-`if` form branches to the single shared epilogue instead. The tell
  is unmistakable in a dump: a whole 5-6 instruction block (`lea ecx,[esp+N] / mov [esp+EH],-1 /
  call ~ifstream / call ~ios / jmp <epilogue>`) present on one side and absent on the other,
  right after the guard's `jcc`. Read the polarity off the guard: `jne <body>` means the NULL case
  falls through into its own teardown, i.e. an early return. Same family as v451's
  "which arm falls through decides cross-jumping" lever.
- **⭐ An UNINITIALIZED-LOOKING local that the original explicitly zeroes before its first real
  use is a real `= NULL` INITIALIZER in the source, and it is worth a whole match (v435,
  `LoadingScreen_Show` 0x45e090, DIFF 214 → EXACT).** The tell is a lone `mov dword ptr [esp+N],0`
  scheduled into an unrelated call's argument-push group near the top of the function, where `N`
  resolves to a plain local slot (not the `/GX` EH state slot two dwords higher). It reads as
  noise — the local is an out-param handed straight to `IDirectDrawSurface::GetDC(&hdc)` a few
  instructions later, so the store is provably dead — but writing `HDC hdc = NULL;` instead of
  `HDC hdc;` not only supplies the store, it flips the callee-saved register pair the whole
  function uses (ebx/esi rather than esi/edi). Always resolve such a store's `[esp+N]` against the
  frame layout before dismissing it: locals and the EH state variable live at different offsets and
  only the arithmetic tells them apart.
- **⭐ THE CONVERSE HOLDS TOO: an EXTRA lone local zero-store in YOUR compile that the original
  lacks means the source has NO such init — delete the assignment (v498, `CursorDesc::Load`
  0x424bf0, DIFF(348) → EXACT).** Copied from the `CarKindDesc::LoadMaybe` sibling shape, the
  transcription carried `nSize = 0;` (the out-size local handed straight to
  `RFIndex::LoadResource`, which writes it unconditionally); the compiled output had one extra
  `mov dword ptr [esp+0x14],ebx` scheduled into the prologue's field-zeroing group — the only
  diff, insns 150/149. ⚠ Do not pattern-match the store to the new-expression's EH alloc temp by
  slot alone: the same `[esp+0x14]` ALSO receives the `operator new` result later, but the
  prologue store was the plain dead init. And do not assume a guard-init is per-FAMILY:
  `CarKindDesc::LoadMaybe` genuinely needs its `nSize = 0` statement (v452), this sibling does
  not. When the whole residual is one extra zero-store, try deleting inits before probing
  scheduling levers.
- **A SMALL sparse switch (≈15 cases, dense only within ±0xa0 clusters) lowers to a pure compare
  tree from a plain `switch` — first-try EXACT (v498, `LocoBitmap_GetDSoundErrorString`
  0x45c2e0).** The parked two-level-table cluster-merge class (0x45bbc0's 129-case sibling) does
  NOT bite at this scale; write the switch in source order and leave it alone.
- **⭐ A short constant string copied into a local `char` array is an ARRAY INITIALIZER, not
  `strcpy` — the two compile to completely different shapes under `/O2 /Oi` (v434,
  `GNetManager::ProbeComPort`, the helper inlined 4x into 0x45eab0).** VC5's `strcpy` intrinsic is
  the GENERIC runtime sequence even when the source is a compile-time literal:
  `mov edi,<lit>; or ecx,-1; xor eax,eax; repne scasb; not ecx; sub edi,ecx; ...; shr ecx,2;
  rep movsd; and ecx,3; rep movsb` — ~13 instructions that measure the literal at RUN time.
  `char buf[5] = "COMn";` instead copies the 5-byte template straight out of `.data` as
  `mov edx,[tmpl]; mov [buf],edx; mov al,[tmpl+4]; mov [buf+4],al`. On 0x45eab0 that one change was
  933 → 906 compiled bytes and 327 → 283 instructions (four inlined copies). **Read the residual
  the other way too:** a `repne scasb`/`rep movsd` pair in YOUR output facing a small fixed
  dword+byte (or dword+word) store sequence in the original always means the original used an
  initializer and you wrote `strcpy`. The template's address is an ordinary `.data` object (under
  `/Gf` it sits alongside the pooled string literals), so an `objdump -s -j .data` at the loaded
  address tells you the exact initializer text — including any placeholder character the code then
  overwrites (`"COMn"`, whose `'n'` is replaced by the port digit).
- **A value the original SPILLS to a stack byte before a call and reloads after it is an explicit
  named local in the source, not a `return`ed expression (v434, `GNetManager::ProbeComPort`).**
  The original's shape was `mov byte ptr [esp+0x17],1` … `call CloseHandle` … `mov al,[esp+0x13]`;
  writing the helper as `… ; CloseHandle(h); return true;` puts the constant AFTER the call
  (`mov al,1`) and never creates the stack home. Restructuring to `bool bOk; if (fail) bOk = false;
  else { bOk = true; CloseHandle(h); } return bOk;` — i.e. assigning the result BEFORE the call so
  it is live across it — reproduced it exactly (DIFF 565 → 39). Generalizes: a stack-slot
  store/reload straddling a call is the compiler telling you where a variable's lifetime begins in
  the source.
- **Inventory of the byte-predicate sites (v356).** 74 `SETE r8` + `TEST r8,r8` pairs exist in
  `.text`; every one is a candidate for the lever above. Re-derive with a scan of the `.text`
  blob for `0F 94 C0..C7` followed within 4 bytes by `84 C0/C9/D2/DB/E4/ED/F6/FF`, then map
  each address through Ghidra's `getFunctionContaining` — or, faster and with no Ghidra round
  trip, bucket the hits locally against `toolchain/test/app_funcs.txt`'s start+size list (the
  `run_script_inline` bridge is JAVA, not Python; a Python-shaped `code` payload fails to compile
  there).
  ⭐ **RE-DERIVED v357 and the already-transcribed set is now CLOSED.** Fixed: 0x4393e0,
  0x43f14c, 0x446d20, 0x45584c+0x45585b, 0x456169+0x45617d, 0x4591ee, 0x46325e+0x4633bf. Left
  inline on purpose: 0x45689a (inside 0x456700 — the predicate reproduces the gate but nets that
  function worse). Everything else sits in not-yet-transcribed code — write the predicate in
  FIRST when you get there rather than transcribing the inline `==` and re-discovering the
  residual. v357 did exactly that for `WorldBoardMaybe::MarkRectDirty` (0x45584c/0x45585b), which
  then went straight to byte-EXACT on the FIRST compile, and for
  `GameNetMsgQueue_EnqueueOrProcessLocalNode` (0x43f14c). Best remaining targets by density:
  `FUN_0041fd00` (6 sites), `LocoBitmap::FUN_0045c3c0` (4), `FUN_0045dde0` (4), and the pairs at
  0x42cc60, 0x44e020, 0x4553e0, 0x457380. `FUN_00446050`'s site (0x446211) is now CLOSED too —
  v358 transcribed that function (`UIResources::Init`) to EXACT, and its gate came for free out
  of the already-written `IsShuttingDownMaybe()` inside the inlined `TileKind_LoadDescriptorRange`.
  26 of the 74 hits land outside any function Ghidra has defined (largely the
  0x461a2c-0x462d91 run) — ignore those until the region is analyzed.
- **⭐ `test al,al` where you emit `test eax,eax` means the CALLEE'S RETURN TYPE IS A BYTE, not
  that the call site needs a cast** (v358, `WidgetBase::SetDescriptor`, which was declared
  `virtual unsigned` and is really `virtual unsigned char`). The width of the `test` after a
  call is a free, exact readout of the declared return type — so a cluster of `test al,al`
  vs `test eax,eax` diffs at every call site of ONE function is a type fact about that
  function, findable without reading its body. Fixing the declaration took
  `WorldActionCursor::InitTrainCouplingMenuIconsMaybe` (0x458c90, 1257 B) from 5 diffs to EXACT
  and changed nothing anywhere else in the repo. This is the return-value twin of the
  already-documented `jl`/`jg`-vs-`jb`/`ja` and `sar`-vs-`shr` signedness tells: **treat operand
  WIDTH in the disasm as declared-type evidence, the same way you already treat SIGNEDNESS.**
  Fix the declaration, never paper over it with a cast at the call site.
  ⚠ **EXCEPTION (v413): when the callee is a Win32 API you cannot redeclare, the byte-ness is
  in the LOCAL, not the callee.** `PtInRect` genuinely returns `BOOL`, so `if (PtInRect(...))`
  correctly emits `test eax,eax` — but both 0x434d70 and 0x435580 do `test al,al` on it. The
  source assigns the result to a BYTE-TYPED local first (`unsigned char bInRect =
  (unsigned char)PtInRect(&rc, pt);` then `if (bInRect)`), and VC5 then only tests the low byte.
  Worth exactly one byte per site, and it is the difference between DIFF(5) and DIFF(4) on
  0x435580. So the rule generalizes as: `test al,al` says the VALUE BEING TESTED is one byte
  wide — usually because the callee returns a byte, but on an unchangeable declaration it is
  the receiving local instead.
- **⭐⭐ WHICH POINTER THE SOURCE READS A FIELD THROUGH DECIDES WHICH VALUE WINS THE
  CALLEE-SAVED REGISTER (v413, `DecorObjMgrMaybe::TickObjSeqGoalsMaybe` / 0x434d70).** When a
  function holds both a base pointer (`pKind`) and an ALIAS into it
  (`pRec = (Rec *)&pKind->firstField`), VC5 gives the callee-saved home to whichever one the
  source keeps LIVE LONGEST — and reading a comparison through the alias is what kills the base
  early. Reading every threshold/array test through `pRec->…` put the ALIAS in ebp, demoted
  `pKind` to a reloaded volatile, and scored 186658; routing the identical comparisons through
  `pKind`'s own flat fields — leaving the alias used ONLY as a call argument, which is what the
  original does — put `pKind` in ebp like the original and scored 124736, then EXACT after one
  more fix. Read it off the disasm directly: whichever pointer the original's `[reg+disp]`
  operands are relative to is the one the source read fields through.
  Corollary on WHERE to declare the alias: **VC5 hoists an alias local's `lea` to its
  declaration point if the alias is used early, and SINKS it to first use if it is not.** In
  0x434d70 the original `lea`s the MobileSeq alias in the prologue (declared early, spilled to a
  stack slot) but `lea`s the TotalVisits one 400 bytes later, immediately before the reward
  block — so the source declares the second one late. Declaring both early made the compiler
  CSE and spill BOTH addresses and cost an extra stack slot (`add esp,0x24` vs `0x20`). Match
  each alias's declaration point to where its `lea` appears in the original.
- **⭐⭐ AN EARLY-EXIT `return` LANDS INLINE; A TRAILING FALL-THROUGH TAIL LANDS SUNK — so the
  ORIGINAL'S BLOCK ORDER TELLS YOU WHICH SHAPE THE SOURCE USED (v461, three sites in one
  session).** VC5 lays basic blocks out in source order, so `if (bad) { fail; return X; } …rest…`
  puts the failure block BEFORE the rest, while `if (!bad) { …rest…; return Y; } fail; return X;`
  sinks it past the success epilogue. Read it straight off the disasm: a failure block sitting
  physically AFTER the function's `return true` epilogue was written as the trailing tail, not as
  an early return. Worth a lot and it is a one-line rewrite —
  `LocoBitmap::HasOpaquePixelInRect` (0x42c950) scored 254870 with the natural-reading
  `if (bConverted != 0) return twin(r);` early exit and **146584** once the raw scan became the
  fall-through with the forward as the tail; `EditCardWnd::StepDecalPickerScroll` (0x419260) has
  BOTH shapes in one body (its `nIdx > 64` guard is a genuine inline early return, while its two
  null-thumbnail failures are sunk tails) and needed each spelled the original's own way, 399335
  → 196998. Corollary: two byte-identical `return 0` blocks that the original did NOT tail-merge
  are a layout fact too — cl merges them when they are reachable as one block and duplicates when
  an unrelated block (here `return 1`) sits between them.
- **⭐ AN `if`/`else` THAT ONLY SETS A FLAG STILL RECORDS WHICH ARM THE SOURCE WROTE FIRST
  (v461).** `if (n > 0) f = 1; else f = 0;` and `if (n <= 0) f = 0; else f = 1;` are semantically
  identical and both compile to a two-block branch — but cl emits the FALL-THROUGH arm for
  whichever the source wrote first, so the jump condition flips. The original at 0x419260 does
  `cmp eax,ebp; jg <set-1>` — it jumps to the `= 1` arm, which means the ZERO case is written
  first. Cheap to read, cheap to fix, and worth 196998 → 186995 there. Same diagnostic family as
  the ternary-polarity lever: when a `jcc` in the original is the logical NEGATION of your guard,
  you wrote the arms in the wrong order.
- **⭐ WHEN A LOOP CURSOR AND ITS INDEX ARE INTERCHANGEABLE, THE BYTES CANNOT TELL YOU WHICH THE
  SOURCE USED — stop probing and pick the simpler one (v461, 0x419260).** Walking an explicit
  `LocoBitmap **ppThumb` cursor (`*ppThumb`, `ppThumb++`) and subscripting the array with the
  index the loop already carries (`paDecalThumbCache[nSlot]`, `nSlot++`) compile
  **BYTE-IDENTICALLY** — cl strength-reduces the subscript to exactly the same element-address
  induction variable, and it does NOT change which value wins a contested callee-saved register.
  Tested twice on the same function, at two different residual levels. This is the counterpart to
  v356's `SoundBank_PreloadWavRange` finding (where an explicit index local DID pin the induction
  variable): there the contest was between two DIFFERENT addressing forms; here the two forms are
  the same expression, so there is nothing to steer. Do not spend a probe on it again.
- **⭐ A LOOP CURSOR THAT IS `--`'d AT THE TOP AND `++`'d BACK ON EVERY BREAK IS THE ORIGINAL'S OWN
  SOURCE, not a compiler restore (v461, 0x419260's backward half).** cl never invents a
  compensating increment on an exit edge, so `if (--nSlot < 0) { nSlot++; break; }` and its
  forward-half mirror `if (++nSlot > 64) { nSlot--; … break; }` are literal transcriptions. Seeing
  `dec r` at a loop head and `inc r` on two separate break targets means write it that way; do not
  "clean it up" into a pre-tested bound.

- **⭐ A dword stack slot that is `inc`'d and stored FULL-WIDTH, then masked `and r,0xffff`
  before every USE, is an `unsigned short` LOCAL (v413).** VC5 keeps a `short`/`unsigned short`
  local int-wide in its slot and masks lazily at each read rather than storing 16-bit — so the
  absence of a `mov WORD PTR [esp+K],cx` is NOT evidence the variable is an `int`. Two of
  0x434d70's locals (its loop index and its per-tick match tally) are this shape, and both the
  `GetAt(i)` argument push and the `i < Count()` guard consume the MASKED value. Related tell:
  the guard then compares `unsigned short` against an `unsigned` return with `jb`, which is how
  that function's `Count()` was pinned unsigned.
- **⭐ An explicit index LOCAL can pin VC5's choice of induction variable (v356,
  `SoundBank_PreloadWavRange`/0x446cc0).** Writing `m_array[i - BIAS]` at every use site makes VC5
  strength-reduce to a FULL element-address induction variable (`lea r,[this + i*4 + disp]`),
  which occupies a register, spills `this` out of its callee-saved home and rotates the entire
  loop. Hoisting the subscript into its own local (`int nSlot = i - BIAS;` once, then
  `m_array[nSlot]`) makes VC5 keep the plain `i*4` scaled index and address as
  `[this + i*4 + disp]` — matching the original, and additionally letting it fold an unrelated
  range test onto the same scaled register. Worth 206415 vs 271470 on that function. Symptom to
  look for: the original recomputes `[base + idx + disp]` at several sites while the candidate
  computes the address once into a register.
- **A constant index bias is folded into the ADDRESSING DISPLACEMENT, and the folded base can
  land inside a DIFFERENT member (v356).** `m_apSoundBankEntries[soundId - 0x5000]` on an array
  at +0x20034 compiles to `[this + soundId*4 + 0xc034]`; taken literally, +0xc034 sits in the
  middle of two earlier arrays. When a `this`-relative displacement appears to overlap an
  already-confirmed member, suspect a folded index bias before you suspect the layout — solve
  `realBase - bias*elemSize` and check whether the result is a member boundary.
- **⭐ VC5 widens `unsigned char` → `int` THROUGH MEMORY, not with `movzx` — and applies the
  `and 0xff` LAZILY (v356).** The idiom is `mov [slot],al ; mov reg,[slot]` followed by
  `and reg,0xff` *only when a consumer actually needs the upper 24 bits zeroed*; when the
  consumer is byte-wide the mask is simply omitted and the store/reload pair stands alone.
  Ground truth: 0x446883–0x44689b (`TileKind_CreateDescriptor`), where the widened byte feeds
  a `switch` — `mov [esp+0x24],al ; mov eax,[esp+0x24] ; and eax,0xff ; cmp eax,0xe ; ja …`.
  **Consequence for triage: a byte store immediately followed by a DWORD reload of the SAME
  slot is a WIDENING, not a register-pressure spill.** Don't go hunting for what "forced the
  spill" — look for where the source uses the byte variable as an `int` operand. (v355 read
  the 0x446030 pair as a spill and sent the next session after register pressure; it isn't.)
- **UNSOLVED CLASS — the `sbb`-mask clamp (v355/v356, `TileKind_GetCategory`/0x446030 and its
  three inlined expansions inside 0x446840).** The original clamps a byte with
  `mov [slot],al; cmp al,0x10; mov ecx,[slot]; sbb al,al; and eax,ecx` — a BYTE-width `sbb`
  mask ANDed against the widened byte (per the bullet above). **29 source shapes are now
  probed** (v355's 8: ternary at both return widths, mask-and in both operand orders,
  `c * (c<16)`, byte-typed mask local, signed-char mask local, `if (c>=16) c=0;`, parameter
  self-assign; v356's 21: `-(unsigned char)(c<16)` at both widths, separate mask local,
  address-taken local, `m=0; m-=(c<16);` at char/int/unsigned widths, `(c>=16)?0:c`,
  `(c<16)?0xFF:0`, `~((c<16)-1)` at both widths, `bool`, `c<=15`, an unsigned-borrow rewrite,
  and four EXPLICIT-widening forms — `int v = c;`, `(int)c &`, `unsigned int v = c;`,
  `-b & (unsigned int)c` — in both operand orders). **Best standalone shape:
  `unsigned char b = (c < 16); return c & -b;`** — `--len 22` total 28014, `reg_pen=0
  identity_miss=0`, insns 8/8, with the prologue, the `cmp al,0x10`, the `and eax,ecx` operand
  order and the epilogue all byte-identical. Every shape hits the SAME hard plateau: VC5
  materializes the compare as a 0/1 bool and negates it (`sbb ecx,ecx; neg; neg`) rather than
  emitting the direct −1/0 mask (`sbb al,al`); because its mask never lands in AL it never
  clobbers the value's register and so never needs the widening round-trip either — **the
  missing store/reload is a CONSEQUENCE of the missing byte-width `sbb`, not a second
  difference.** Don't re-probe on the operand-shape, mask-type or explicit-widening axes; the
  only open question is what makes VC5 emit a byte-width `sbb r8,r8`.
- **⚠ The standalone and INLINED optima for the same expression can be different source
  shapes — score both ways (v356).** The mask form above wins standalone (28014) but is
  materially worse inlined into 0x446840 (352802, insns 351/341) than the plain ternary
  `c < 16 ? c : 0` (305655, insns 357/341), which in turn beats `if (c>=16) c=0;` (399236)
  in situ while LOSING to it standalone. Inlining changes which register already holds the
  value, which changes the whole lowering. A shape proven at a leaf is a hypothesis, not a
  result, at an inline site — and vice versa.
- **⚠ Comparing candidate shapes by `cc.sh`'s `DIFF(n)` alone is unsound when they compile to
  DIFFERENT LENGTHS (v356).** `DIFF` counts differing bytes over the compared window, so a
  body that is simply too short scores artificially well: v355 recorded `if (c>=16) c=0;` as
  "closest at DIFF(7)/len 14" against a 22-byte original, but on the fair
  `asmscore.py --len 22` basis it is 32011 vs. the real winner's 28014. Rank candidates by
  `asmscore --len <true length>` total, never by `DIFF` across unequal lengths.
- **Shared-header churn, refined (v355):** the v331/v340 hazard is specifically about
  DECLARATIONS THE HEADER'S OTHER CONSUMERS PARSE — a new parameterized method decl, or a new
  class body. Two changes measured codegen-NEUTRAL repo-wide this session, both in headers
  included by dozens of TUs: (1) splitting a `pad0x18[2]` into a named byte + a pad byte in
  `src/CursorDesc.h`, and (2) retyping `int m_kindBackingMaybe[0x4000]` to
  `CursorDesc *m_apKindDescriptors[0x4000]` (plus the matching `int **` → `CursorDesc ***`)
  in `src/UIResources.h`. EXACT stayed at 38850 B / 237 funcs to the byte. So field splits and
  pointer-type corrections do NOT need the TU-local-view workaround; only new declarations do.
- **Two more VC5 /Og transcription levers (v328, `PeerTrainNode_UpdatePlacementTickMaybe`):**
  (1) a boolean flag the original carries in AL with its stores tail-merged across switch
  arms only reproduces when the `flag = true;` is written as a PER-CASE statement — a
  `bool flag = true;` declaration init pins the flag in a different register (BL) and
  un-merges the tails; (2) the byte-counter loop shape (`XOR BL,BL` preheader; body indexed
  by a dword; `INC BL` + a `[esp+0xNN]` byte spill + reload-`AND 0xff` in the tail) comes
  from a TWO-variable loop — `unsigned char i` counter plus an `int u` index assigned
  `u = i` per iteration in a do-while — NOT from `for (unsigned char i = 0; ...)` (that
  compiles to a plain register-int loop with no spill).
- **Four more VC5 transcription levers (v329, the PeerTrainNode cluster):** (1) a loop with a
  mid-body `break` inside a do-while gets its first iteration PEELED/rotated by /Og — writing
  the exit as part of the bottom test (`do { x = f(); } while (x != 0 && ++n < LIM);`)
  reproduces the original's un-peeled `call; test; jz exit; inc; cmp; jl` shape; (2) a
  3-outcome range adjust (`1..2 -> A`, `4 -> B`) the original compiles as
  `test; jle out; cmp 2; jle armA; cmp 4; jnz out; armB` comes from a NESTED
  `if (m > 2) { if (m == 4) armB; } else armA;`, not an if/else-if chain (the chain lays the
  arms out in the wrong memory order); (3) a `== K ? p : 0` ternary on a pointer compiles to
  the branchless `XOR r; CMP; SETNZ; DEC; AND` select, and a small if/else that only SELECTS
  a pointer needs a single shared use site (a `pName` local + one trailing expression) or
  /Og tail-duplicates the whole select into both arms; (4) comparing a `short` arg against an
  `unsigned short` field promotes to 32-bit MOVSX/MOVZX compares — cast the field `(short)`
  to get the original's 16-bit `CMP AX, word [...]`.
- **New /Og residual class: loop-entry guard vs array-LEA scheduling swap (v329, parked
  `ReverseDirectionMaybe` 0x44cb10 + `SetSoundStateMaybe` 0x44d740, 9 bytes each).** The
  original emits a for-loop's entry test (`CMP word [count],reg; JB exit`) BEFORE the
  pointer-walk `LEA EDI,[arr]`; this toolchain schedules the LEA between the CMP and the JB
  (same encodings, one-insn order swap). No source lever found (declaration order, i-inside-
  for, indexing vs pointer walk — all neutral or worse). If it ever cracks, retry both plus
  `CanReverseDirectionMaybe`'s 6-byte EAX/EDX test-reg coin-flip (docs/PARKED.md rows).
- **New /Og residual class: sparse-switch cluster-merge / two-level table (v330, parked
  `Ddraw_HResultToString` 0x45bbc0).** A sparse 129-case switch whose values form dense
  sub-runs separated by modest gaps (< ~120) gets its whole region merged by c2 into ONE
  byte-index-compressed two-level jump table (`xor ecx,ecx; mov cl,[eax+tbl]; jmp
  [ecx*4+tbl2]`); the original kept a pairwise sweep + separate direct dword tables per
  dense run. Order-independent (c2 sorts cases), insensitive to /O level, /Os-/Ot,
  /Og-, /G3-G6, frontend (C/C++), VC4.2, AND VC5-SP3 c2 (byte-identical .obj — SP3
  refuted as a toolchain fix, toolchain/vc50sp3). Before hunting source constructs on a
  big switch, verify the case SET first: union the original's sweep `cmp eax,imm` values
  + jump-table non-default entries and diff against the transcription's cases.

- **Confirms Yoda lesson #11 (switch over if-ladder) with a new corollary: a real `switch`'s
  DISPATCH SHAPE for 2-3 dense case values can be a plain decrement-chain compare, not a jump
  table — and that shape is WHY a shared constant register can stay live across all case bodies
  (VC5, `EditCardWnd::AdjustIdentityColorChannel`, v218).** `sub eax,0; je case0; dec eax;
  je case1; dec eax; /* falls into case2 */` has no intervening jump that would abandon a
  register's liveness, so the original keeps ONE hoisted `edi=0xff` (and separately `edi=1`)
  register shared across all 3 cases' own internal comparisons — closing the gap needed
  `switch(param_1){case 0: ... case 1: ... case 2: ...}` over an `if/else-if` chain (341894 vs
  151402 total score at the correct length, confirmed by testing both at once), but the
  register-sharing itself wasn't reproduced by the `switch` alone; hoisting an explicit
  `int nMax = 0xff;` local before the switch had ZERO effect (constant-propagated back to the
  literal regardless) — not a viable lever via source syntax found this session.
- **VC5 uses pre-standard `for`-scope:** a `for (int i = ...)` loop's counter LEAKS into the
  enclosing block scope (not scoped to the loop, unlike modern C++). Reusing the same counter
  NAME across sibling `for` loops in one function is a hard `C2371` redefinition error at
  compile time — give every loop a distinct counter name (`i2`/`i3`/`i4`/...), even when a
  *different* tracking variable used inside those loops is deliberately the SAME one shared
  across all of them (e.g. one `nChosen` index reused by 3 search loops, Yoda lesson #7's
  register-reuse family — the shared variable and the per-loop counter are independent
  decisions).
- **Independent plain `return CONST;` sites do NOT auto-share one epilogue under `/O2`; a
  shared `goto <label>;` tail does (VC5-confirmed, `LocoBitmap::BuildPaletteLUTMaybe`).** When
  the original has ≥2 no-cleanup early-return sites sharing ONE physical tail block (confirmed
  via disasm: several call sites jump to the same address), transcribing each as an independent
  `return 0;` compiles to N DUPLICATED inline epilogues instead of sharing one — rewrite every
  such site as `goto fail;` with a single trailing `fail: return 0;` to force the sharing.
  Contrast with `throw CONST;` sites (`Wav_ParseAndLoad`), which DO naturally share via the
  compiler's own EH dispatch machinery — this is specifically about plain early returns.
  **The sharing is LOCAL/trace-driven, not global across every `goto` to the same C-level
  label — 3+ gotos to one label can split into 2+ DISTINCT physical tail blocks if some of
  them are far apart in the jump-table's own case-body layout order (VC5,
  `GameNet_DispatchMessage`, v267, closed 568,617→501,479).** Cases 6 and 7 (adjacent in
  jump-table order) and cases 0xa/0xb/0xf (elsewhere in the table, reached via an intervening
  `resolve_and_enqueue:` block) all `goto`'d ONE shared `enqueue_tail:` label — but the
  original only merges the two ADJACENT cases (6's body ends with a bare `jmp` straight into
  7's own inline tail) while 0xa/0xb/0xf keep their own separate physical copy far away; a
  single C label for all 5 sites merged cases 6+7 into the FAR copy too, adding a spurious
  extra reload/call at the near site. Fix: give each physically-local cluster of gotos its
  OWN distinct label (here `case6_7_enqueue:`, placed inside case 6's own body) rather than
  reusing one label function-wide. Caveat when the label must sit inside a block that
  declares a local with an initializer before it: VC5 raises `C2362` ("initialization of
  'x' is skipped by 'goto'") for a forward goto into that scope — hoist the local to a
  plain (uninitialized-at-declaration) function-top variable and assign it via a bare `=`
  statement instead (a plain assignment isn't an "initialization" the goto can skip).
- **A branch that only picks a `wsprintfA`/similar variadic call's FORMAT-STRING argument
  compiles differently as two separate `if (cond) {call A} else {call B}` statements vs. a
  `pszFmt = cond ? A : B; call(dest, pszFmt, arg);` ternary-local (VC5-confirmed,
  `SaveGame_ScanSavFilesMaybe`).** When the call's OTHER arguments are invariant across both
  branches (e.g. a compile-time-constant global array address), the original hoists that
  invariant argument's push to ONE site before the branch, then cross-jumps both arms'
  differing pushes (format string + dest buffer) into a single shared call site (an extension
  of the cross-jump-geography family, Yoda lesson #18/#15). The ternary-local form computes
  the selected format-string pointer INTO A REGISTER first, then pushes the invariant argument
  AFTER that selection — a different, non-matching instruction order — even though both forms
  are semantically identical and call the exact same function once. Prefer the duplicated-call
  `if`/`else` form whenever a multi-arg variadic call's format string is the only thing that
  varies by branch.
- **A big scratch buffer that is providably 100% dead in the ORIGINAL binary too (zeroed via a
  `memset`-shaped `rep stos`, never read anywhere) still gets its zeroing `rep stos` KEPT by
  `/O2` — only a single paired scalar dead-store gets eliminated (VC5-confirmed,
  `SaveGame_ScanSavFilesMaybe`'s unused `szDirPrefix`).** The big-buffer idiom is just
  `char buf[N] = "";` (see the aggregate-initializer bullet below) — when the whole
  buffer is later unused, the compiler drops the initializer's single literal-byte copy (ordinary
  backward local dead-store elimination) but does NOT eliminate the tail zero-fill, even
  though whole-buffer liveness analysis would prove it's dead too. Declaring an apparently
  "pointless" always-zeroed-but-unread local exactly this way is the right transcription when
  disasm shows a zeroing loop with no matching first-byte store and no later reads — don't
  assume it must secretly be used somewhere, and don't omit the declaration just because it
  looks like dead code. **2nd confirmation (`ShowFatalErrorMessageBoxMaybe`, `0x463600`):** the
  SAME idiom applied twice back-to-back can share ONE physical stack slot across two DISTINCT,
  disjoint-scope locals (a dead `szUnusedMaybe` immediately followed by a real, used `szMsg`) —
  the tell distinguishing this from a single buffer double-initialized is the reservation size
  (one `sizeof(buf)`-worth of `sub esp`, not two) plus the exact repeated instruction sequence
  (loop-loop-store-loop, i.e. the dead buffer's OWN memset with no preceding store, immediately
  followed by the live buffer's store+memset) — matched byte-for-byte modeling it as two
  separate `char[N]` locals in nested/sequential scope on the first attempt.
- **⭐ A `char buf[N] = "...";` AGGREGATE INITIALIZER is what emits "copy the literal's bytes out
  of the literal's own storage, then `rep stos` the tail" — and that shape is NOT evidence of a
  global scratch buffer.** cl 11 does not fold a short string initializer into immediate stores:
  for `char szTitle[0x100] = "";` it emits `mov dl,ds:<addr of "">` / `mov [buf],dl` then
  `mov ecx,0x3f; xor eax,eax; lea edi,[buf+1]; rep stosd; stosw; stosb` (1 + 255 = 256), and for
  `char szInitialDir[0x104] = "c:";` it copies THREE bytes as `mov ax,ds:<"c:">` +
  `mov cl,ds:<"c:"+2>` and then zero-fills 257. With `/Gf` every `""` in the image pools to ONE
  address, so a single `mov dl,ds:0x4851d0` gets CSE'd across every `= ""` buffer in a function —
  which is exactly why `0x4851d0` looks like a shared 1-byte global with no writers anywhere.
  **It is not a global; it is the pooled empty string literal** (v384: `0x4851d0` was modelled
  repo-wide as `extern char g_szScratchText[1]` and hand-spelled as
  `buf[0] = g_szScratchText[0]; memset(buf + 1, 0, sizeof(buf) - 1);`; converting all 40 sites to
  plain `= ""` initializers left EXACT at 73275 bytes / 322 functions, i.e. byte-identical, and
  every `g_szScratchText` PASSED AS AN ARGUMENT was simply `""` — `SetLabelText("")`,
  `strcpy(pText, "")`, `CreateWindowExA(..., "EDIT", "", ...)`, `dpname.lpszShortNameA = ""`).
  The two spellings are byte-equivalent for the `""` case, which is why the wrong model survived
  so long — but they DIVERGE the moment the literal is not empty: the hand-rolled form spells
  `szInitialDir[0] = 'c'; szInitialDir[1] = ':';` which emits IMMEDIATE byte stores, not the
  original's 3-byte load from the literal (caught in `EditCardWnd::ImportDecalImageMaybe`,
  `0x419b10`, whose score improved 788250 → 658373 on the fix alone). Whenever disasm shows a
  buffer seeded by a LOAD FROM A CONSTANT ADDRESS followed by a `rep stos` tail, write the
  aggregate initializer; reserve a real extern for addresses that actually have writers.
- **A local declared inside an inner block can still have its aggregate initializer emitted in
  the function's top-of-body init run.** `ImportDecalImageMaybe`'s `szSuffix` is used only inside
  one nested `if`, yet the original zero-fills it up front alongside the other five buffers — so
  it belongs at the top of the function with them, not at its point of use.
- **#31 Link with `/OPT:REF`** for the endgame image. **#32:** the REF-vs-NOREF
  dropped-symbol diff is a precise "reference-graph gap" oracle.
- **A manual byte-pair compare loop matching MSVC's `strcmp` intrinsic shape, duplicated
  byte-for-byte at N call sites, is genuine evidence for a plain `strcmp()` call (relying on
  `/O2`'s implied `/Oi` intrinsic substitution), not a shared helper function that got inlined N
  times (VC5, `ActivateTabMaybe`'s 4 sorted-position search blocks).** Confirmed via a read-only
  raw-disasm diff: all 4 copies were IDENTICAL down to register choice (zero allocation drift
  across call sites) — a genuine function inline is expected to show at least some
  register-pressure-driven drift by call-site context (Yoda lesson #7), while a compiler
  INTRINSIC substitution is deterministic regardless of context, which is the cleaner
  explanation for true byte-identity. When N call sites of a "small algorithm" are byte-identical
  in the original, check first whether it's an intrinsic-shaped leaf (memcpy/memset/strcmp/
  strlen) before assuming a private helper — the intrinsic hypothesis needs zero source
  restructuring to test (just call the CRT function) and is falsifiable in one compile.
  **Extended to `strcpy` (VC5, `EditCardWnd::RebuildLocalPlayerCard`, v137, EXACT match):** the
  `repnz scasb`-for-length then `rep movsd`+`rep movsb`-for-copy shape at 2 independent call
  sites (copying different strings into different destination fields) modeled as 2 plain
  `strcpy()` calls compiled byte-identical on the first attempt — no manual-loop transcription
  needed, don't hand-write the scan/copy when this shape appears.
  **`memset` is a DISTINCT lever from plain pointer-caching for a "zero N consecutive fields via
  one shared base address" shape (VC5, `NetSessionEventQueue::BigObj_RemoveMaybe`, v208).**
  When the original computes ONE `lea` into a multi-field block and stores 0 via offset-relative
  addressing from it (`[edx],[edx+4],[edx+8],[edx+0xc]`), writing 4 separate `pObj->fieldN = 0;`
  statements through the SAME already-in-scope struct pointer produces 4 independently-computed
  `[base+offN]` stores instead (no shared `lea`) — and caching that base address in an explicit
  local (`T *p = &pObj->firstField; p[0]=0; p[1]=0; ...`) has ZERO effect on the generated code
  (tried and confirmed inert). Only `memset(&pObj->firstField, 0, N*sizeof(T))` reproduced the
  original's shared-base shape, closing a large structural residual (asmscore score
  ~103k→~56k) in one step. Treat this as its own intrinsic-substitution case (alongside memcpy/
  strcmp/strcpy) rather than assuming any pointer-caching idiom generalizes to "shared address
  computation" — check whether the site is really a memset-shaped bulk zero before hand-
  rolling per-field stores or a bare cached pointer.
- **A scratch-buffer local that's only USED inside a subset of a switch's cases can still be
  DECLARED unconditionally at function scope, and the original zeroes/inits it on EVERY call
  regardless of which case runs (VC5, `WidgetPickerObj0x477cc8::ActivateTabMaybe`).** Symptom:
  transcribing the buffer's declaration+init INSIDE just the cases that use it makes the compile
  repeat the zero-init sequence once per case, while the original does it exactly once, always,
  at function entry (visible as the same "load a byte from a global, zero-fill the rest"
  instruction sequence appearing right after the prologue instead of inside the case bodies).
  Fix: hoist the local's declaration AND initialization above the `switch`, even though it's only
  read from 2 of 6 cases — this is a real signal about the ORIGINAL function's own local layout,
  not an optimization choice available to steer.
- **Comparisons against a tri-state -1/0/1 sign/strcmp-style result still pick their exact
  instruction shape from the LITERAL constant compared, not just the operator (VC5, extends Yoda
  lesson #2).** `iVar7 >= 1` and `iVar7 > 0` are mathematically identical when `iVar7` only ever
  holds -1/0/1, but compile differently: `> 0` against a value already known nonneg-vs-neg
  produces a cheap `test eax,eax; jle`, while `>= 1` written as a literal comparison against the
  constant 1 produces `cmp eax,1; jl`. Ghidra's decompile NORMALIZES a `test;jle` sign-check back
  to the semantically-equivalent `< 1` textual form, which is misleading if copied verbatim —
  when a comparison's LHS is provably tri-state, prefer the `>0`/`<0`/`==0` zero-comparison form
  that matches a raw `test reg,reg` in the disasm over a literal non-zero constant, even if
  Ghidra's own printed condition uses the constant.
  **⚠ The SAME lever runs in the opposite direction on a value VC5 cannot prove tri-state, and
  there the literal-constant form is the faithful one (v456, `NameAnchorMaybe::
  TryAdvanceAcrossPointsMaybe` 0x40c460, DIFF(2) → EXACT).** A guard on a signed 16-bit STRUCT
  FIELD compiles to `cmp WORD PTR [m],1; jl` from `>= 1` but to `cmp WORD PTR [m],0; jle` from
  `> 0` — cl compares against the source's own literal in both cases and never normalizes one to
  the other, because the field is an arbitrary memory word rather than a known sign result. The
  two encodings are the same length, so the function is byte-length-identical and the whole
  residual is 2 bytes in 2 instructions, which reads exactly like an ordinary tie-break. ⚠ Ghidra
  prints BOTH forms as `0 < (short)field`, so the decompile cannot tell you which to write —
  read the constant out of the raw disasm's `cmp` operand. Generally: **when a one- or
  two-instruction residual sits on a comparison, check the immediate in the original's `cmp`
  against the literal in your source before writing an autopsy** — same family as the v455
  "check the data model before parking a 1-instruction residual" lesson, and cheaper to test.
- **A provably-dead redundant re-check (already excluded by an earlier early-return on the SAME
  value) survives in the original's codegen ONLY if written as an unsigned RANGE compare
  (`> 0`/`<= 0`), not as a literal EQUALITY compare (`== 0`/`!= 0`) against the exact constant an
  earlier branch already tested (VC5, `PostBagFileCache::LoadIndexedFileMaybe`, v153, EXACT
  match).** After `if (dwSize == 0) return; if (dwSize == (DWORD)-1) return;`, a third
  `if (dwSize == 0) {...} else {alloc+read}` guard is unreachable in BOTH the original and any
  recompile — but VC5's own redundant-branch elimination only fires when the third check is a
  literal re-match of a constant an earlier branch already tested to false; writing it instead as
  `if (dwSize > 0) {alloc+read} else {...}` (matching the original disasm's own `cmp reg,0; jbe`
  unsigned-range shape, not a `test reg,reg; je` zero-test) is a DIFFERENT comparison the
  optimizer doesn't correlate to the earlier `==0` check, so the whole branch — dead in both
  binaries alike — survives untouched. This single operator swap also incidentally resolved an
  unrelated ebx/edi register-swap residual spanning the entire rest of the function (structural
  score 31432→6003, byte_diff 32→3, closing the match outright) — when a function has a
  redundant-but-present dead check AND unrelated-looking register-swap noise, fixing the dead
  check's operator form first may resolve both at once rather than being two separate problems.
- **#33 Content oracles beat byte-%:** vtable-slot and dispatch-table scans catch functional
  bugs (wrong slot, missing entry) that per-function byte-% shrugs off. The LIVE GAME is a
  first-class oracle once running.
- **A small virtual dtor's cross-call-site INLINING depends on header-vs-.cpp placement, not
  just /O2 + same-TU position (VC5, `LocoBitmap::~LocoBitmap`).** An out-of-line `.cpp`
  definition compiled to a genuine out-of-line `CALL` from BOTH its real use sites (the
  auto-generated scalar-deleting-destructor AND a stack-local's automatic teardown) — matching
  NEITHER original, both of which show the body inlined directly. Moving the out-of-line
  definition's POSITION within the same `.cpp` (end-of-file vs. right after the ctors) had ZERO
  effect. Only moving the definition INTO the class body in the header (making it implicitly
  `inline`) fixed it, confirming `src/DSound.h`'s prior single-line `~DSound(){DSound_Teardown
  (0);}` precedent generalizes: for a small dtor that needs to inline at multiple sites under
  this toolchain, define it in the header, not out-of-line. **Extended (v115): the SAME
  header-inline definition can inline at same-TU call sites while staying a genuine
  out-of-line `CALL` at a call site in a DIFFERENT TU** — `WidgetPicker.cpp`'s
  `~WidgetPickerObj0x477cc8` destroys a member `LocoBitmap` and our compile INLINES it there
  (unlike the original's plain `call 0x42a370`), even though the identical header-inline
  definition correctly stays inlined at `LocoBitmap.cpp`'s own 2 sites. No source-level fix
  exists under this toolchain: `__declspec(noinline)` isn't available (VC5/cl 11.00 rejects it,
  `error C2485`), and moving the definition out-of-line would break the same-TU sites that
  need it inlined. Treat cross-TU inlining of an otherwise-matched header-inline method as its
  own intrinsic residual class, separate from (but related to) the header-vs-.cpp placement
  question above — confirmed via `WidgetPickerObj0x477cc8`'s dtor, `src/WidgetPicker.cpp`.
- **A "cache the pointer, don't cache the offset" reload pattern can apply to ONE side of a
  copy loop and not the other, keyed by aliasing, not symmetry (VC5,
  `LocoBitmap::CopyOverlapRaw`).** Blitting from `this`'s OWN member (`pPixels`) into a
  caller-supplied PARAMETER buffer: the dest pointer (a plain parameter, no aliasing concern) is
  cached as a real incrementing pointer local, while the source is walked via a plain `int`
  offset re-added to `pPixels[offset]` fresh each iteration (Yoda #19's no-cached-pointer-for-
  member-reads rule) — the two sides of the SAME loop get different treatment because only one
  of them is a member access. Also: `*pDst++ = arr[i++];` as ONE combined post-increment
  expression is NOT interchangeable with the 3-statement `*pDst = arr[i]; i++; pDst++;` even
  though they're semantically identical — the split form produced an extra `-1`-corrected
  addressing mode (increment-before-use needing compensation) that the combined form doesn't.
- **A switch statement's case BODIES are laid out in the final `.text` in SOURCE DECLARATION
  ORDER, not case-value/numeric order, once the compiler picks a jump-table dispatch (VC5,
  `WidgetPickerObj0x477cc8::HandleSavegameMenuNodeMaybe`).** The jump table itself maps each
  value to whatever address its case body landed at, but the ORDER those bodies appear in
  memory follows the order the `case` labels were WRITTEN in source, independent of their
  values — writing an 8-case switch in ascending command-id order when the original's cases
  were declared in some other order (recoverable by reading the raw jump-table dwords via
  `objdump`/a `struct.unpack` over the PE file bytes, then mapping each target address back to
  which case body starts there) misplaces WHOLE case bodies relative to each other, producing a
  huge structural diff (700+ bytes on a ~1000-byte function) that looks unrelated to any single
  case's own content. Before transcribing any multi-case jump-table switch, read the jump table
  first and order the source's `case` labels to match the table's target-address order, not the
  case values' numeric order.
  - **⭐ RE-CONFIRMED v444 on a 14-case switch (`BuildToolCursorWnd::LoadCursorBitmapSet`/0x437670,
    DIFF(436) → EXACT with NO other change), and with the diagnostic signature that identifies it:
    the candidate has MORE instructions than the original, not fewer.** cl cross-jumps/tail-merges
    only ADJACENT case bodies, so a wrong ordering does not merely move blocks — it denies the
    merges, and the candidate comes out longer. Here ascending order (0, 1/3/6, 2/4/7/8, 5, 9, 10,
    11, 12, 13) gave `insns 186/169`: 17 instructions the original had merged away and ours had
    not. The original's true order, read straight off its `.text` block layout, was
    (10, 9, 11, 12, 13, 5, 0, 1/3/6, 2/4/7/8) — under which cases 10 and 12 collapse into a shared
    tail, 9 and 13 into another, and case 0 into 2/4/7/8's, each becoming a bare `push <id>; jmp`
    stub exactly as the original has. So: `insns MINE/ORIG` with MINE materially HIGHER on a
    switch-heavy function is a case-ORDER symptom, not a missing-optimization one. Note the
    ordering can look thoroughly unnatural (10 before 9 before 11) — that is real, organically
    grown source, and it is load-bearing; say so in a comment so a later cleanup pass does not
    "tidy" it back to ascending.
  - **Check the jump table BEFORE assuming the lesson applies: if the case-body target addresses
    are already ascending, the source order was already ascending and there is nothing to win.**
    Both sibling switches in the same TU (`StartSlotDAnimation`/0x438280 and
    `AdvanceSlotDAnimation`/0x438590) have strictly ascending tables, so their large residuals are
    a different class entirely — reading the table first is a 30-second check that avoids a
    pointless permutation hunt.
- **CONTRAST — a SUBTRACT/COMPARE-CHAIN switch (sparse values, no jump table) lays its case bodies
  out INDEPENDENT of source order; reordering the `case` labels is INERT (VC5,
  `DPlaySessionMgr::HandleQueuedPlacementEvent`, 0x440150, v245).** For a 3-value dispatch
  (0x12/0x15/0x17) compiled as a cumulative `sub eax,K; je` chain (testing values ASCENDING
  regardless of source order), the bodies land in a fixed compiler-chosen layout — the ascending
  chain's fall-through case (highest-tested value, 0x17 here) is emitted FIRST, then the je-target
  bodies. Writing the source cases in that memory order (0x17/0x15/0x12) vs. ascending (0x12/0x15/0x17)
  produced BYTE-IDENTICAL output (same score, same diff). So unlike the jump-table case above, do NOT
  spend effort reordering a compare-chain switch's source cases to match `.text` layout — it can't
  move it. (The jump-table lesson's source-order-steerability is specific to the jump-table dispatch.)
- **⭐ …but the CHOICE of `switch` vs. `if`/`else if` is very much steerable, even at two cases, and
  the compare CHAIN is how you read which one the original used (v434,
  `DPlay_SelectConnectionDlgProc` 0x461020 -- DIFF(294) -> EXACT).** A `switch` lowers the tests as
  a cumulative chain up front (`sub eax,0x110 / je / dec eax / jne`) and moves EVERY case body out
  of line, so the first-written arm's body ends up physically LAST. An `if`/`else if` ladder over
  the same two values instead re-compares from scratch around each arm (`cmp eax,0x110` ... body
  ... `cmp eax,0x111` ... body) and lays the first arm's body INLINE. Two corollaries: (a) a
  subtract-then-decrement chain is positive evidence for `switch` even when there are far too few
  cases for a jump table -- do not reason "only two cases, so it must have been an `if`"; (b) for a
  16-bit selector the switch also emits an `and eax,0xffff` zero-extension where the if-ladder
  emits a 16-bit `cmp ax,K`, so a stray `and reg,0xffff` in the original is the same tell. A window
  proc's `switch (uMsg)` with a nested `switch (LOWORD(wParam))` is the canonical shape.
- **`extern "C"` on a function DEFINITION breaks this repo's marker->COMDAT pairing (v434).** The
  marker silently pairs with some unrelated COMDAT (0x45fbd0 paired with a 15-byte ctor and
  reported DIFF(9) at len=15) rather than failing loudly. Linkage never changes the emitted bytes,
  so just don't spell it on the definition; put shared declarations in the header, where
  `extern "C"` is fine. Related to `lint_idiom.py`'s class I, which already forbids `extern "C"`
  in a `.cpp`.
- **A switch whose lowest USED case value is > 0 needs an explicit `case 0:` (grouped with any
  other no-op cases) and NO separate `default:` label to keep the jump table anchored at 0 (VC5,
  `AlbumCardWnd::FUN_00403ba0`).** When case 0's target is identical to `default`'s (e.g. both a
  bare no-op), writing `case 0: case 7: case 8: default: break;` makes the compiler treat case
  0's target as redundant with `default` and drop it from jump-table range consideration —
  normalizing the table to start at the lowest DISTINCT case value instead (here 1), which emits
  an extra `dec eax` (or equivalent subtract) before the range check/index that the original,
  whose table genuinely starts at 0, doesn't have. Dropping the separate `default:` label
  entirely — `case 0: case 7: case 8: break;` as the ONLY no-op path, relying on the switch's own
  implicit fall-past-the-end for truly out-of-range values (behaviorally identical to an empty
  `default`) — restored the plain 0-based `cmp eax,N; ja <tail>` with no subtraction. Check for
  this whenever a transcribed switch's bounds-check instruction includes an unexplained
  `dec`/`sub` immediately before the `cmp`/`ja`.
- **A jump-table switch's SOURCE case order is readable straight off the table plus the block
  addresses, and a codegen-free `case 0:` with NO `default:` is what anchors the table at 0 (v399,
  `TutorialWnd::RedrawControl` 0x451fb0 and `RedrawControlPressed` 0x4527b0 — both EXACT on the
  FIRST compile, 448 B and 296 B).** Recipe, ~2 minutes and no probing: dump the 10 table dwords,
  map each to the block that starts there, sort the blocks by ADDRESS, and write the `case` labels
  in exactly that order — here a distinctly non-numeric 9, 1, 2, 3, 4, 5, 6, 7, 8. Every table
  entry pointing at the shared epilogue is a no-op case; group them all under one label list
  (`case 0: case 4: case 5: break;`) and write NO `default:`, which is what keeps the dispatch a
  plain `cmp eax,9; ja; jmp [eax*4+tbl]` with no `dec`/`sub`. The arm whose block sits LAST falls
  through into the epilogue instead of getting its own `ret`; that is automatic, not something to
  spell.
- **⭐ GENERALIZATION of the above — an "empty" case that is semantically identical to the
  default gets FOLDED INTO IT, and losing that case label can drop the switch below VC5's
  jump-table threshold entirely (v361, `DSoundChannel::IsReclaimable` 0x40eeb0).** A 4-case
  switch (values 1-4) over an accumulator initialized to 0: writing `case 3: break;` makes case 3
  indistinguishable from `default`, MSVC merges them, sees only THREE live case labels, and emits
  an if-chain (`je/dec/je/sub/je`) instead of the original's `dec eax; cmp eax,3; ja; jmp
  [eax*4+tbl]`. Restoring the case's own REDUNDANT store — `case 3: result = 0; break;` — brings
  the label back, and the table with it: DIFF 45/55 → 14, then EXACT once the cases were also
  declared in the original's block order (3, 2, 1/4). The redundant store is NOT optimized away
  at /O2; it survives as a second `xor bl,bl` with its own epilogue, distinct from the prologue's
  initializer. **So an "unsteerable jump-table-vs-if-chain threshold" verdict is a smell** — count
  the case labels the compiler can actually SEE (after folding no-op cases into the default)
  rather than the ones you wrote, and read the original's block layout for how many distinct
  bodies really exist. This retired a park that had stood since v79.
- **Corollary — a SEPARATE standalone `if ((unsigned)(x-1) > N) return 0;` bounds check
  immediately followed by a `switch(x)` covering exactly `[1,N+1]` should be a single switch
  with the top value as its own `case`, not `default:` (VC5, `GNetManager::
  DPlay_InitConnection`, v281, closed byte_diff 1691→1140 across TWO separate switches on the
  same variable).** When source writes the bounds check and switch as two statements with the
  top value folded into `default:`, the compiler emits them as two separate instruction
  sequences (a standalone bounds compare + branch, THEN the switch's own compare + jump table).
  The original FUSES them into one: `dec eax; cmp eax,N; ja fail; jmp [eax*4+table]` — only
  possible when the switch's own case set is the exact same dense range the bounds check
  validates, i.e. `case 1: ... case N+1: ...; default: return 0;` with NO prior standalone `if`.
  Confirmed on two independent switches in the same function (protocol→GUID selection,
  protocol→address-element-building) — both needed the same restructuring. Tell: a decrement
  chain (`dec eax; je case1; dec eax; je case2; ...`) or a separate `lea reg,[eax-1]; cmp
  reg,N` appearing in the recompile where the original has one fused `dec;cmp;ja;jmp[reg*4]`
  sequence.
- **A `do { ... } while (cond)` loop entered only after an outer `if (list != 0)`-style guard
  can get its FIRST iteration PEELED by /O2, producing a duplicated null-check-free copy of the
  loop body's own call — even though the original doesn't peel it (VC5,
  `WidgetPickerObj0x477cc8::HandleSavegameMenuNodeMaybe`'s duplicate-savegame-name search).**
  The compiler proves the first iteration's pointer is non-null from the outer guard and emits
  a standalone copy of the loop-body call (e.g. an `_stricmp`) for iteration 1 alone, then falls
  back to the normal null-checked loop body for iterations 2+ — visible as an extra ~7
  instructions (`je`/`lea`/2×`push`/`call`/`add esp`/`test`) that don't correspond to anything
  in the original. The equivalent `for (p = head; p != 0; p = p->next) { ... }` shape (test-at-
  top, no separate outer guard needed since the loop's own condition subsumes it) did NOT
  trigger the peel and closed the residual outright (DIFF 426→185 on this function, the single
  largest fix of the session) — when a `do-while` loop guarded by a redundant-looking outer
  null-check shows an unexplained duplicated call near the loop, try the plain `for`/`while`
  shape before assuming it's an intrinsic residual. **Confirmed on a SECOND, structurally
  different trigger (VC5, `PostBagFileCache::LoadCardAtIndexMaybe`, v155): the guard doesn't
  have to be a linked-list traversal** — a plain `if (pRecords != NULL) do {...}
  while(boundCheck)` (a member-pointer null guard, unrelated to the loop's own per-iteration
  bound check) peeled identically, duplicating the ENTIRE loop body including two downstream
  calls (dominant structural gap, asmscore total 281431→22239 once fixed). Fix was the same:
  factor the null check out as a separate early return above the loop, then write the loop as
  `while (boundCheck) {...}` so its own condition is the sole entry test. General rule: ANY
  do-while whose entry is gated by a condition distinct from its own back-edge test is a peel
  candidate, not just null-terminated list walks. **Confirmed on a THIRD trigger shape (VC5,
  `NetSessionEventQueue::BigObj_RemoveMaybe`, v208): a re-queried collection-size guard**
  — `int nCount = pColl.Count(); if (nCount != 0) { do {...; nCount = pColl.Count();} while
  (i<nCount); }` — peeled the same way; the `while (i < pColl.Count())` rewrite (re-evaluating
  `Count()` as the loop's own top condition each pass, no separate outer guard or cached
  `nCount` local) fixed it. Also needed `i` declared `unsigned int` here, not `int` — the
  original's own comparison form was unsigned (`jbe`/`jb`), and a signed `int i` compiled a
  structurally-different signed comparison even after the peel itself was fixed; check the
  loop VARIABLE's signedness against the disasm's comparison opcode as a separate lever from
  the peel fix itself. **Fourth trigger (v514, `RFIndex::LoadResource` 0x45cd00): a guard +
  do/for(;;) with an in-loop null `break` peeled AND tail-merged the break straight into
  `return 0`** (the original falls out of the loop into a shared re-test instead). Fix: drop
  the outer guard and fuse BOTH tests into the loop's own condition —
  `while (pRec != NULL && _stricmp(...) != 0) { ...; pRec = pRec->pNext; }` — one shared call
  site, null test only at the back edge, EXACT.
- **BUT the peel is NOT always source-steerable: a find-first-matching-node loop with the
  match test at the loop TOP and the advance+null-check at the BOTTOM (a two-exit "mid-exit"
  loop) gets PEELED/rotated by /O2 identically no matter how it's written (VC5,
  `GameNet_DrainPendingTrainQueue`/0x43e010, v238 — parked EFFECTIVE).** The original keeps
  ONE shared loop-top load of the match field; every recompile (do-while + `goto found`/`goto
  after` matching the exact original CFG, plain `while (cond)`, and `for(;;) { if(...) break;
  ... }`) duplicates it — a peeled iter-0 copy at the top PLUS a rotated bottom copy — net-neutral
  on insn count (150/151) but cascading into the null-exit routing (my compile jumps the node==0
  edge straight to the epilogue; the original routes it through the after-merge's redundant node
  re-test) and a symmetric ecx/edx/eax swap in the trailing unlink block. Unlike the
  list-walk/collection-size cases above, NONE of the loop-shape rewrites nor a fresh-field re-read
  of the loop head fixed it — this specific two-exit shape rotates intrinsically here. Steerable
  levers that DID help the surrounding code (bank them, then park the peel): hoisting the queue-head
  load + the `prev=0` clear ABOVE the rate-gate (reproduced the original's early scheduling +
  fixed the `prev` register), and the 16-bit `unsigned short` push (Yoda #14). Budget the loop-shape
  probes, then annotate EFFECTIVE — don't keep re-grinding a mid-exit loop's rotation.
- **CONTRAST (find-first scan that DOES steer): a find-first-with-`break` scan whose match test
  is a call at the loop TOP (not a field re-read) reproduces /O2's call-at-top rotation from a
  plain `for (i=1; i<=count; i++, pp++) { if (Call(*pp)) { found=true; break; } }` — the explicit
  `if (i<=count){ do{}while }` form MIS-rotates it to increment-first instead (VC5,
  `HandleQueuedTrainPlacement`/0x43e370, v240).** Whether a find-first scan rotates
  intrinsically (the drain's 0x43e010 mid-exit shape above) or steers cleanly (this one) turns on
  where the exit test sits: a match test that is a CALL at the top rotates like a normal
  `for`-with-break (call-at-top, forward `jne found`, `jmp after`), and the `for` form is the one
  that matches. The cost of the `for` form is that it hoists the induction-pointer `lea` one slot
  ABOVE the entry guard `jb` (the original keeps it inside the guarded block) — a minor
  misalignment; the if+do-while form keeps the `lea` inside but rotates the whole loop wrong, a
  worse trade. Try `for`-with-break FIRST for a call-at-top find-first scan; reserve the explicit
  if+do-while for count-guarded loops whose body has no early exit. **CAVEAT (VC5, v243,
  `DPlaySessionMgr::FUN_004408b0`/0x4408b0): the `for`-with-break fix can WRECK register allocation
  when the scan is NOT the first loop in a multi-loop function.** That function has FOUR consecutive
  car-slot loops all sharing the node pointer in ebp; converting only the 2nd (a find-first-with-break
  "any car still has detail" scan) to the `for` form hoisted its own element pointer into a
  callee-saved register, which SPILLED the shared node pointer to the stack and desynced the whole
  allocation across all four loops (reg_pen 1→28, total score 60135→147196 — far WORSE than the peel it
  fixed). The `if+do-while` form's mid-exit peel (3 extra insns, everything else byte-aligns) was the
  lesser evil and was kept. So the "for-form first" rule is for a find-first scan in ISOLATION; when it
  shares a base pointer with sibling loops, weigh the peel it removes against the allocation it may
  break, and prefer whichever scores lower — the peel is often cheaper.
- **A do-while loop's ENTRY GUARD reuses the loop-counter register when written `if (i <= count)`
  (with `i` already initialized to its start value) instead of `if (count != 0)` — a real, cheap lever
  (VC5, `DPlaySessionMgr::FUN_004408b0`, v243, dropped a 4-loop function 138172→60135).** The original
  emits `mov i-reg,1; cmp count,i-reg; jb skip` (comparing the count against the counter register that
  already holds the loop's start value, unsigned `jb`); the semantically-identical `if (count != 0)`
  guard instead emits `cmp count,0; je skip` — a different constant and a different jcc, misaligning the
  guard AND cascading offsets through the whole loop. Write the guard as `if (start <= count)` using the
  same variable the `do{...}while(i <= count)` back-edge uses, and make the counter `unsigned int` so the
  compare is `jbe`/`jb` (matching a `movzx`'d `unsigned short` count) rather than the signed `jle`/`jl`
  a plain `int` counter produces. Companion to the "reuse the counter variable" register-coalescing
  family (Yoda #7). **REFINEMENT (VC5, `GameNet_RemovePeerTrainsForPlayer` car-slot loop, 0x43b770,
  v255): when the count-guarded loop follows an intervening FIELD STORE, the explicit `int i=1; if (i
  <= count) { do {...} while (i <= count); }` form can SPLIT the guard `cmp` from its `jb` — /O2 hoists
  the independent count-read `cmp` above the store (both read the same base register but don't depend on
  each other), leaving `cmp` 9 bytes early while `jb` stays put (byte_diff 8, everything else aligned).
  The plain `for (int i = 1; i <= count; i++)` form kept `cmp`+`jb` together after the store AND still
  reused the counter (`cmp count,di; jb`) — EXACT.** So for a count-guarded slot loop whose guard's
  count-read can be hoisted past a preceding store, prefer the compact `for` form over the explicit
  if+do-while: it reuses the counter (this lesson's main lever) AND doesn't let the guard `cmp` float
  free. Here the guard stayed a 16-bit unsigned `cmp word[base],di; jb` while the back-edge was a
  32-bit signed `cmp edi,edx; jle` (count zero-extended) — the two comparisons differ in width/sign, a
  normal artifact of a `for` whose entry test reuses the just-set counter vs. its own `movzx`'d back-edge.
- **A "walk to the list TAIL" loop matches with the single-variable `while (p->pNext != 0) p =
  p->pNext;` form, NOT the two-variable `for (q = p->pNext; q != 0; q = q->pNext) p = q;` form —
  the two-variable form seeds the loop in a different register (a 1-byte swap), EXACT (VC5,
  `DPlaySessionMgr::HandleQueuedTrainConnect`, 0x43e2e0, v247).** Both compile to the identical
  advance-and-trail CFG (`mov eax,[reg+off]; cmp; je out; mov reg,eax; ...`), but the `for` form's
  separate seed `q = p->pNext` lands the tail pointer's initial value in the just-freed `this`
  register (eax) instead of reusing the register the earlier null-check already loaded it into (ecx)
  — one isolated `mov eax,[eax+0x70]` vs `[ecx+0x70]`, align=0, reg_pen=1. The `while` form
  (re-reading `p->pNext` as both the test and the advance) keeps the tail pointer in the null-check's
  register throughout. **Do NOT try to fix this by caching the head in a local before the null-check**
  (`T *pHead = member; if (pHead == 0)...`) — that pins the value into a callee-saved register
  (ebx + a `push`/`pop` pair), spilling the frame and scoring far WORSE; the original re-reads the
  member fresh (scratch reg) at the null-check and reuses that same scratch reg for the walk, which
  is exactly what the inline `while (member->pNext...)`-shaped source (no cache local) produces.
- **A `bool` found-flag set inside a scan loop stays in a REGISTER (not spilled to a stack byte)
  only when a LATER loop's counter is a SEPARATE variable that /O2 can coalesce into the flag's
  register — reusing ONE counter variable across the scan AND the follow-up loops leaves the flag
  homeless and it spills (VC5, `HandleQueuedTrainPlacement`, v240).** The original keeps
  `bFound` in ebx and, right after `test bl,bl`, does `mov ebx,1` — reusing ebx as the NEXT loop's
  counter (the flag's range ends exactly where the counter's begins, so they coalesce). If the
  source reuses the scan's own counter `i` for the follow-up loops (one variable, one register,
  typically esi), ebx is never needed and the allocator parks `bFound` in a stack byte
  (`mov byte[esp+N],0/1`) instead — ~6 extra insns. Fix: give each follow-up loop its OWN counter
  (`i`/`j`/`k`, also required by VC5 for-scope leak) so one of them claims the callee-saved register
  the flag dies into. Extends Yoda #7's register-reuse family: a distinct-per-loop counter is
  sometimes BETTER than reuse, precisely because it gives the allocator a coalescing target.
- **When a find-index loop's result must stay live across many later calls AND the function
  heavily reuses the constant 0 (a clear block with many zero-stores), SPLIT the loop counter
  from the result variable so /O2 can dedicate a callee-saved register to the 0-constant and spill
  the RESULT to a stack home — matching the original's memory residency (VC5,
  `DPlaySessionMgr::RemovePeerTrainsAndSlot`/0x43f940, v249, 299590→228336 asmscore).** A
  single `int slotIndex = 0; do{ if(match) goto found; slotIndex++; ... }` uses ONE variable as
  both counter and result, so /O2 parks it in a callee-saved reg (ebp) throughout — stealing ebp
  from the whole-function 0-constant the clear block needs (so the original's `mov word[x],bp`
  zero-stores can't reproduce). Writing `int i=0; int slotIndex; do{ if(match){slotIndex=i; goto
  found;} i++; ... }` (counter `i` scratch, result committed to `slotIndex` at loop exit) frees ebp
  for 0 and forces `slotIndex` to a `[esp+N]` stack home reloaded at each use — exactly the
  original's shape. The inverse of Yoda #7's "reuse the variable" lever: here SPLITTING is what
  matches, because the two roles (transient counter vs long-lived result) want different residencies.
- **A branchy bool-flag (`if (c) flag = 1; else flag = 0;` merge-tested after the if) must have
  its ZERO assignments written at the branches/merge point, NOT as a `= false` initializer at
  declaration — the hoisted init spills the flag to a callee-saved register (VC5,
  `ThreadWrapper::SetPriority`/`PollAndResume`/`Start`, v322, all EXACT).** Writing
  `char bAlive = 0; if (...) { if (wfs0 == WAIT_TIMEOUT) bAlive = 1; else pending = false; }`
  lets /O2 hoist the zero-init to the prologue (`push ebx; xor bl,bl`) because the flag then
  appears live across the call; the original zeroes AL at the shared merge point instead. Fix:
  no initializer, assign `flag = 0` on both failing paths (`else { pending = false; flag = 0; }`
  plus the outer `else`) so nothing is live across the call and the flag stays in AL. Also the
  tell that the original is NOT an inlined shared helper: an `inline` out-of-line definition
  either doesn't inline (CALL rel32) or drops the standalone copy entirely — check both before
  committing to the helper-inline theory.
- **A scalar deleting dtor (`??_G`) INLINING its base dtor's body vs CALLing it is source-
  steerable only by moving `~T` into the class body — but that costs the standalone `??1`
  copy** (VC5, `ThreadWrapper`, v322). With the dtor out-of-line, `??_G` CALLs it (the original
  INLINED it); moving it in-class makes `??_G` EXACT but cl drops the standalone `??1` when
  every same-TU use inlines it — the original's standalone copy exists only because ANOTHER TU
  (an untranscribed caller) emits the COMDAT. Bank whichever marker matters more; full closure
  waits for the caller's TU (see `docs/PARKED.md`).
  **⚠ v440 (`ThumbnailBmp`, 0x447b60) adds a THIRD cost nobody had measured: the in-class form
  also reshuffles every CONSUMER TU, because each one that destroys the object now emits its own
  dtor COMDAT.** Moving `~ThumbnailBmp()` in-class made `??_G` EXACT at 36 B (+ the file 8/8),
  but cost `src/DPlaySessionMgr.cpp` an unrelated 345 B EXACT (`ApplSetupWnd::SendSelectRequestMaybe`,
  0x40ac50 → DIFF(4)) — net +69 B in-class against +389 B out-of-line. So the decision is NOT
  just "which of `??_G`/`??1` do I want"; **always re-run a FULL `progress.py` and diff the
  per-file table before banking the in-class form**, exactly as for any other shared-header edit.
- **An in-class dtor in a SHARED header is inlined at every local's scope exit in every consumer,
  and that is a whole-function-sized residual you cannot fix locally.** `ThumbnailBmp_Save`
  (0x447e30, v440) is 446 B against 288 and 132 instructions against 76 — and the ENTIRE gap is
  cl inlining `src/LocoBitmap.h`'s in-class `~LocoBitmap` at the function's two exits, where the
  original emits a plain `lea ecx,[esp+8]; call LocoBitmap::~LocoBitmap`. Everything above the
  dtor lines up instruction-for-instruction. The tell is a pair of IDENTICAL multi-block
  free/release sequences at the two exits of a function whose only local is a class object. The
  lever is the HEADER (move that dtor out of line), so treat it as a repo-wide experiment, not a
  reason to distort the function being transcribed.
- **A find-and-remove list walk matches the original's single-register node pointer when written
  ADVANCE-IN-PLACE (`pPrev = p; p = p->next;`), NOT as a two-variable `while ((cur = next) != 0)`
  form — the two-variable form costs a per-iteration `mov cur,next` register copy the original
  lacks (VC5, `RemovePeerTrainsAndSlot`/0x43f940, v249, killed 3 `mov ecx,eax` copies,
  reg_pen 38→17).** Even for a "remove-and-restart-from-head" walk, keep ONE node variable
  (`pNode`): in the no-match branch do `pPrev = pNode; pNode = pNode->pNext;` (order matters — capture
  prev before advancing); in the match branch, after unlink+delete, reset `pNode = head; pPrev = 0;`.
  The decompiler's `while (cur = next, cur != 0)` comma rendering is an artifact of its own SSA
  splitting, NOT evidence the source used two variables — transcribe the single-variable in-place walk.
- **A struct field the caller ZERO-EXTENDS before pushing as a call argument (`xor eax,eax; mov
  ax,[field]; push eax`) proves the callee parameter is 32-bit (`int`/`unsigned int`), NOT the
  field's own narrower type — model the param 32-bit even when Ghidra prints a `ushort`/`byte`
  arg (VC5, `SetTrainPlacementResult`'s wTrainId arg, v240).** A `short`/`unsigned short`
  param instead lets the caller push the 16-bit value with garbage upper bits (`mov dx,[field];
  push edx`, one insn shorter) — structurally WRONG (drops the zero-extend the original has), even
  though it scores lower on a weighted diff. The zero-extend at the call site is the arbiter of the
  callee's real param width; the byte/word-with-garbage-upper push (see the SetTrainPlacementResult
  owner-byte args) is the tell for a genuinely-narrow param. Which scratch register the extend
  lands in (eax vs edx) is a separate, non-steerable reg tie-break.
- **⭐⭐⭐ A DEAD `jl -> (ptr = NULL)` signed-index guard in front of `&member_array[i]` is NOT a
  codegen artifact — it is an implicitly inline BOUNDS-GUARDED ACCESSOR in the source (VC5,
  v360; this REFUTES the long-standing "confirmed unreproducible" verdict of v238/v242).** The
  original materializes the element pointer via `lea base+i*size` behind `if ((int)i < 0) ptr =
  0;` and only then derefs; a direct `&arr[i]` subscript folds the guard away because the
  compiler proves the index non-negative. Every attempt to force it from the SUBSCRIPT side
  fails (unsigned index, `int` local `= param & 0xff`, a signed-int param, an explicit
  `if (&slot != 0)`, an `(int)` cast) — because the guard was never at the subscript. Write the
  accessor the original had:
  ```cpp
  DPlaySessionMgrProviderSlot *ProviderSlotAt(int i) { return i >= 0 ? &aProviderSlots[i] : 0; }
  ```
  and call it. Details that matter:
  * **Spell it `i >= 0 ? ptr : 0`, NOT `i < 0 ? 0 : ptr`.** VC5 makes the ternary's FIRST arm the
    fall-through; the original's fall-through is the pointer arm. Getting this backwards leaves
    the branch polarity inverted and costs the match.
  * **In-class body = implicitly inline**, which is what lets it vanish into every caller. It
    leaves no function in the binary, so there is nothing to name in Ghidra.
  * **Inside a loop VC5 if-converts the same accessor to a BRANCHLESS `xor; test; setl; dec;
    and`** — a `setl`-shaped residual and a `jl`-shaped residual can be the SAME accessor seen in
    two contexts. Don't diagnose them as separate classes.
  * **Whether the CALLER null-checks the returned pointer is a separate, observable source fact
    and it varies per call site.** In one function two lookups checked it and the third
    dereferenced straight through (`ProviderSlotAt(i)->field`, a `// sic:` NULL-deref that can
    never fire); a sibling checked only its create path. Read each site's `test reg,reg; je` in
    the raw disasm rather than assuming the function is internally consistent.
  * The old counter-argument — that a loop-counter-indexed `&arr[i]` elsewhere emits NO guard in
    either binary, "same compiler, opposite decisions, so not steerable" — was exactly backwards:
    that site simply didn't call the accessor.
  * **Downstream effects come back for free.** A "fused `[base+i*4+off]` load vs. the original's
    materialized `lea` + `[ptr+off]`" residual is usually the SAME missing guard, not an
    independent scheduling difference — without the select there is no reason to materialize the
    base first.
  * Payoff when this was recognized: three functions went straight from DIFF 125/142/268 to
    EXACT, and two more dropped by 50-60%. **When several functions in one subsystem all carry
    "the same intrinsic guard", suspect a shared inline accessor before parking any of them.**
- **`test eax,eax` vs `cmp eax,<zero-reg>` for the SAME zero-comparison is chosen by the SHAPE of
  the compared value, not by register pressure (VC5, v360, `SoundBankEntry::EnsureLoaded`
  0x448d60).** With a dedicated zero register (`xor esi,esi`) equally live at both sites, VC5
  emits `test eax,eax` when the value is an INLINE CALL RESULT (`if (Foo() != 0)`) and
  `cmp eax,esi` when it has been stored into a NAMED LOCAL first (`hr = Foo(); if (hr != 0)`).
  Diagnostic: a 2-byte diff pair `85 c0` vs `3b cX` (or the reverse) at a zero-check right after
  a call. `if (x)` vs `if (x != 0)` is NOT the lever — those are byte-identical. Beware the
  converse: a named local can be load-bearing for a different reason (it pins WHERE the value is
  read, see 0x40ac50, where inlining the read lost a hoist and scored worse) — so change one site
  at a time and keep whichever the disasm agrees with.
- **An inline `strlen(buf)` used as a CALL ARGUMENT compiles differently depending on whether it
  is spelled inline or hoisted into a named local — and the hoisted form is usually the original's
  (v399, `TutorialWnd::DrawEllipsis` 0x4526b0, DIFF 24 -> EXACT in one probe).** Written inline as
  `DrawTextA(hdc, buf, strlen(buf), &rc, flags)`, cl pushes the later arguments FIRST and then
  runs the inline `repnz scasb` sequence between the pushes. Hoisted as `int nLen = strlen(buf);`
  on its own line, the whole scasb sequence is emitted BEFORE any of the call's pushes. Diagnostic:
  a `-`/`+` pair in the dump where the original's `lea edi,<buf>; or ecx,-1; repnz scasb; not ecx;
  dec ecx` block sits entirely above a `push <flags>` that your compile has above it. Same family
  as the v360 named-local lever — a local pins WHERE a value is computed — but here it applies to
  an intrinsic expansion rather than a call result. Note this cuts the OPPOSITE way from the v361
  "don't hand-hoist a field read" bullet below: hoist an intrinsic, don't hoist a plain field read.
- **Don't hand-hoist a field read into a local just because BOTH arms of an if/else use it —
  VC5 CSEs it into the common predecessor anyway, and does so with BETTER scheduling (v361).**
  Reading the field directly in both arms let VC5 place the load AFTER the `cmp` that guards the
  branch, so the compared value dies at the `cmp` and the hoisted load REUSES its register; the
  hand-written local pinned the load BEFORE the `cmp`, leaving both values live and forcing a
  second register. Same instruction count either way, so the diff reads as a pure register
  coin-flip — it isn't. `EditCardWnd::CycleDecalKindHighlight` (0x41a460): 15 → 5 bytes from
  deleting one `UINT uIDEvent = hDecalHitTestTimer;`. Corollary to the bullet above — a named
  local pins WHERE a value is read, which cuts both ways; the transcription default should be the
  direct read, with a local only where the disasm shows an early load.
- **⭐ A SMALL AGGREGATE PASSED BY VALUE vs. its fields passed as separate scalar params is
  OBSERVABLE — and the only tell is which dead param slot a spill reuses (VC5, v361).** For a
  `POINT`/2-dword struct the two spellings — `f(POINT pt)` and `f(int x, int y)` + a local
  `POINT pt; pt.x = x; pt.y = y;` — generate *byte-identical* code everywhere except stack-slot
  reuse: two scalar params leave two INDEPENDENT 4-byte dead slots and VC5 reuses the HIGHER one
  (`y`'s), while one dead 8-byte aggregate param is a SINGLE slot whose base is the LOWER address,
  so a later spill lands there for free. Diagnostic: an otherwise-perfect function whose only
  residual is N copies of the same `orig=14 ours=18`-style displacement on one spilled local
  (`insns X/X`, `align=0`, `reg_pen=0`). Fix: pass the aggregate by value.
  **This retired a 100-session-old wrong verdict** — `AlbumCardWnd::HitTestUiElement` (0x403cd0)
  was parked from v141 as an "intrinsic dead-parameter-slot-reuse tie-break" because every probe
  flipped some spelling of the LOCAL (`pt.x`/`pt.y` assignment order, aggregate `= {x,y}` init,
  hoisting the counter's declaration, widening the counter), i.e. tested the wrong axis entirely;
  one signature change made it EXACT (429 B). Same lever then closed
  `EditCardWnd::ApplyColorSwatchPreset` (0x418340, DIFF 155 → EXACT 270 B) and halved
  `CycleDecalKindHighlight` (0x41a460, 29 → 15).
  **Generalize, but per-function:** the call sites are the tell — a caller that already holds a
  `POINT` and passes `pt.x, pt.y` almost certainly passed `pt`. It is NOT universal, though:
  `EditCardWnd::HandleRosterClick` (0x41a650) matches EXACTly with two scalar params, so read
  each function's own evidence. Where the callee has no spilled local, the two spellings are
  indistinguishable and the choice is free.
- **⭐ A "dead" `mov ecx,<reg>` immediately before a call is a `this` PASS, not a dead reload —
  the callee is a `__thiscall` member that never reads `this` (v361).** Never autopsy one as an
  intrinsic register tie-break without first checking EVERY call site of that callee in the whole
  binary: `objdump -d` the image and grep `-B 8` for `call <addr>`. If they all set ecx — from a
  singleton global outside the class, from `this` inside it — it is a member, full stop. A callee
  that ignores `this` is invisible in its own body (it may even clobber ecx immediately, as
  0x4446f0 does with `mov ecx,0x140`), and `__stdcall` with N args and `__thiscall` with N stack
  args purge the SAME `ret N`, so the callee's own bytes cannot distinguish the two — only the
  call sites can. `src/PostBag.h` models four of these (0x445400, 0x445700, 0x445f20, 0x4446f0):
  declare it as a member so call sites reproduce the ecx load, and keep a free `__stdcall` extern
  for the definition (which Ghidra also types `__stdcall`).
  `PostBagCacheBundle::DeleteCardFileAndRefreshCount` (0x444fb0) → EXACT on that one change,
  parked since v100 as a "genuinely dead in the original too" reload.
  **Sibling call sites that already match are not evidence against this** — MSVC omits the ecx
  setup entirely where `this` is provably still live from the prologue, so a `this->` call there
  is byte-identical to a free-function call. Only a site where an intervening call clobbered ecx
  (here `DeleteFileA`) shows the pass at all. Converting the callee left both such siblings
  (0x443686, 0x445177) matching untouched.
- **⭐ `static` is the other spelling of this bug, and it hides better than `__stdcall` does
  (v362).** A `static` member emits no ecx setup at its call sites, exactly like a free function —
  so the same missing instruction appears, but the declaration *looks* like it already belongs to
  the class and so survives review. Worse, it invites the inverted inference explicitly: because
  the callee provably never reads `this`, an autopsy concludes the callers' `mov ecx` must be dead
  and writes `static` to document that conclusion. That is backwards — "the callee ignores `this`"
  is the PREMISE of the whole class, not evidence against it. `PopupWndBase_RebindClipperToActiveScreen`
  (0x414ef0) carried that reasoning in its header from v191; all 10 of its call sites pass ecx.
  Dropping `static` closed `PopupWndBase::OnMouseMove` (0x414a80) and `OnPaint` (0x414ae0)
  outright. **Grep your own headers for `static` members and re-derive each one from its call
  sites** — a genuine static has NO ecx setup anywhere.
- **⭐ Sweep for this class in bulk instead of meeting it one autopsy at a time (v362).** Two
  complementary read-only sweeps, both ~2 minutes and neither needing a compile; they surface
  DIFFERENT rows, so run both. (1) Whole-binary: tally every `call <imm>` in the app region by
  whether the immediately-preceding instruction writes ecx, keep targets with ≥2 ecx sites and a
  ≥80% fraction, then ask Ghidra which are not already `__thiscall`. (2) Aligner-based: for each
  already-transcribed non-exact function, run `asmscore._decode`/`_align` and report every
  ORIG-only gap instruction that writes ecx and is immediately followed by a `call`. Sweep (1)
  found 0x414ef0; sweep (2) found 0x41d5c0 (only 2 call sites, so (1)'s threshold excluded it).
  Standing results and the remaining 40-row queue live in `docs/THISCALL_AUDIT.md`.
  **Sweep (2) has a high false-positive rate on LARGE functions** — inside a big misaligned
  region the aligner emits orig-only gaps that mean nothing, and a `mov ecx,<imm>` gap there is
  usually a singleton-member call you ALREADY model correctly. Check the `ours:` side of the gap
  before believing it: if it is empty, you are looking at alignment noise, not a missing
  instruction (this is what made 0x404f60 and 0x41ac10 false positives, as v361's 0x42b050 and
  0x40d170 were).
- **A Ghidra `__fastcall` on ordinary app code is nearly always a `__thiscall` member (v362).**
  VC5 does not emit `__fastcall` for this codebase; the convention appears only because Ghidra's
  analyzer saw ecx read as an input and invented a first register parameter. Such a function's
  call sites already pass ecx correctly, so retyping it is a decompiler-output-quality win (typed
  `this->field` instead of `param_1[N]`) rather than a byte-match one — but leaving it wrong
  poisons every future transcription of its callers. 35 of these are queued in
  `docs/THISCALL_AUDIT.md`.
- **⭐ A decompiled signature `T *f(T *out, …)` that RETURNS `out` is usually MSVC's HIDDEN
  STRUCT-RETURN BUFFER, not a real out-param (VC5, v361).** Tell: `ret N` purges one dword MORE
  than the declared args (a `__thiscall` with two `int`s that purges `0xc`), the leading pointer
  arg sits at `[esp+4]` ahead of them, and `eax` is reloaded with that same pointer right before
  the return. Transcribing it literally as an out-param is *close* but never exact.
  `RectFlagObj0x477820::ComputeLocalPos` (0x436a40) went DIFF 23/32 → EXACT the moment it became
  `POINT ComputeLocalPos(int x, int y)` — parked as a "field-fetch scheduling residual" since v3.
  **The caller side is the loudest evidence, and it's easy to misread:** each call site gets its
  OWN compiler temporary, so a literal transcription grows a family of hand-declared
  `int aFoo[2]` / `int local[2]` scratch buffers — one per call site, named by whoever transcribed
  them — whose only use is to receive the call and be read back as `[0]`/`[1]`. Two such buffers
  in one function that are never otherwise touched is the signature. Fixing the callee retires all
  of them at once (v361: four call sites across three TUs).
  Caveat on the caller: `T t = f(…);` gets MSVC's return-slot optimization (the hidden arg IS
  `&t`, so reads come from `[esp+N]`), while the original may instead read through the returned
  `eax`. Separate declaration + assignment is NOT the fix — that emits a real aggregate copy and
  scores far worse; `const T &t = f(…)` folds away to the init form. Still unsolved (0x4549e0).
- **Two INDEPENDENT adjacent stores into a by-value struct take their registers from SOURCE
  ORDER (VC5, v360).** `desc.dwBufferBytes = a; desc.lpwfxFormat = b;` and the reverse differ by
  an eax/ecx role swap: whichever value is computed first lands in eax. This is the store-side
  companion to the rule below that a single expression's OPERAND order is *not* steerable —
  separate statements are, one expression's operands are not.
- **Adjacent independent `+=` ACCUMULATION statements can emit in FLIPPED order (VC5, v515,
  `TimeOfDay_IsDateTimeInWindowMaybe`/0x412790, EXACT).** Three `n += rec->field;` statements
  written A;B;C emitted as B;A;C with the two memory loads parked in esi/ebx in the EMITTED
  order — and writing them B;A;C to compensate flips them right back (DIFF(4) both ways, the
  two orders are the only spellings; pick the one that matches). Same class as the
  by-value-struct store-order rule above: statement-level order is steerable, but the mapping
  from source order to emitted order is NOT guaranteed identity — try the flip.
- **⭐⭐ The strong form of the above, and it makes the residual DECODABLE: a struct built
  field-by-field DIRECTLY into an outgoing by-value argument slot emits its loads and stores in
  FIELD-OFFSET order regardless of source order, but allocates registers in SOURCE-ASSIGNMENT
  order, following the fixed sequence `eax, ecx, edx, edi/esi/ebx` (VC5, v386).** Because the
  emitted order carries no information and the register assignment carries all of it, you can read
  the original's source field-assignment order straight off the disassembly: pair each field's
  register with its position in that sequence, and that IS the order the fields were written in
  1998. Two byte-matches in one session from this alone — `MapWnd::OnTimerDefaultMaybe` (0x4323e0)
  wanted `left=edx, top=ecx, right=eax, bottom=edi` ⇒ source order right, top, left, bottom; and
  `MapWnd::DrawPeerScreenshotMaybe` (0x431a10) wanted `left=eax, right=ecx, top=edx, bottom=esi`
  ⇒ source order left, right, top, bottom. Both had previously read as pure register coin-flips.
  **The rule is specific to the outgoing-argument case** — the compiler owns that block's layout,
  so it normalizes to offset order. A struct local whose ADDRESS IS TAKEN (`OffsetRect(&r, …)`,
  `InflateRect(&r, …)`) has an observable memory home, so its stores DO come out in source order
  and the trick does not apply; there, reorder to match the emitted store order instead
  (`MapWnd::OnKeyDown` 0x430ef0 — probing left/top/right/bottom against the emitted
  right/left/top/bottom scored 50154 vs. an incumbent 27-byte residual, i.e. the emitted order is
  the source order and there is nothing to decode).
  **⚠ v388 — READING that emitted store order off the disasm is where this goes wrong, because
  the stores are spread across DIFFERENT `esp` bases and none of them names the rect.** Do it
  mechanically: pin the local's base from the `lea reg,[esp+K]` that feeds the address-taken
  call, tracking `esp` through every intervening `push`, then resolve each store's own
  `[esp+N]` at ITS `esp` and subtract. On 0x430ef0 the four stores appear at `[esp+0x14]`,
  `[esp+0x14]`, `[esp+0x18]` and `[esp+0x20]` — two of them at the SAME literal displacement
  while meaning different fields, because three pushes land between them. Resolved against the
  base (E+8, confirmed independently by the four reload displacements of the later by-value
  push) they are right, left, top, bottom. Guessing from the literal displacements instead
  yields a different permutation and burns a session on the wrong order.
  **The companion trap: fixing the ORDER can strand the LOADS.** 0x430ef0's original hoists both
  16-bit field loads to the top (killing the descriptor pointer early) yet still stores `bottom`
  last; source order right/left/top/bottom alone puts the height load after the stores, and cl
  only hoists it if the value is routed through a named local — which buys a byte-exact rect
  block but pays a one-step `eax→ecx→edx` rotation across the two by-value RECT argument copies
  (DIFF 40 / 31720 vs. the incumbent 27 / 16023). When the true order measures worse than a
  false one, say so in the source comment and keep the cheaper form — but record the ORDER, so
  the next session attacks the rotation instead of re-probing the permutation.
- **A named struct local passed by value can keep a DEAD stack home that the arg copy does not
  elide (VC5, v386, `MapWnd::OnActivate` 0x431310, UNSOLVED).** The original writes the four
  fields to a local at `[esp+0x38..0x44]` (never read again) AND to the outgoing arg slot,
  interleaved — 4 extra instructions. Neither hoisting the declaration to the top of the function
  nor merging it with the function's other RECT reproduces it (the two rects have distinct homes,
  E+0x14 and E+0x24, so they are NOT one reused variable). Park rather than fake it with an
  artificial address-taking.
- **The operand order of a COMMUTATIVE multiply is canonicalized and is NOT a lever (VC5,
  v360, `LocoBitmap::Fill` 0x42aa90).** `width * height` and `height * width` compile
  byte-identical, and so does hoisting either operand into its own named local first (the copy is
  folded). So when a residual is purely "which of the two memory operands got loaded into the
  register and which got folded into the `imul`", there is no known source lever — park it.
- **`/GX` EH state tags (`mov [esp+N], <state>` before each protected `new`) are numbered in
  LEXICAL source order, but VC5 never inverts an if/else — the `then` arm is always the
  fall-through (v360, `NetSessionEventQueue::BigObj_CreateAndInsert` 0x41dd80).** When the
  original's state numbers imply one source order and its block layout implies the opposite,
  plain if/else polarity cannot satisfy both: flipping it fixes 4 immediate bytes and costs ~260
  bytes of layout. Diagnose via the immediates alone — a rotation like orig 3,0,1,2 vs. ours
  0,1,2,3 at otherwise identical offsets means only the state numbering differs.
- **Ghidra's `uint`/`int` return type can be a MIS-INFERENCE for a genuinely `void` function
  whose bare-return exits leave dead eax — the tell is a SHARED epilogue reached with DIFFERENT
  eax values on different paths (VC5, `GameNet_DrainPendingTrainQueue`, v238).** Here the
  one epilogue is reached with eax = the idiv quotient (no-op gate path), = 1 or the loop
  counter (car-scan paths), and = a callee's result (sound path) — a REAL `uint`-returning
  function needs a single consistent value per path, so three disjoint dead values converging
  on one `pop;pop;pop;ret` proves the return is unused and the function is `void` (the `ret`
  is `c3`/0-arg, not `ret 0xN`, confirming no stack cleanup either). Modeling it `void` (bare
  `return;`/fall-through everywhere, side-effecting statements not `return <expr>`) is both
  faithful AND much simpler to transcribe than chasing Ghidra's phantom return value.
- **The mirror-image peel bug: a do-while's OWN top-of-loop guard check can be silently
  FOLDED AWAY (not duplicated) on the peeled first iteration when the loop is written as
  `if (0 < N) { do { if (guard) break; ...; } while (cond); }` — writing the equivalent
  plain `while (cond) { if (guard) break; ...; }` instead keeps the guard checked
  unconditionally every iteration, matching the original (VC5,
  `TutorialWnd::DrawDescriptionChunks`, v184, closed a 30046-point residual to an EXACT
  match in one step).** With the do-while form, `/O2` can prove the loop-counter is exactly 0
  on the peeled first pass (reached via a direct jump that skips the loop's own
  back-edge-only reload code) and folds the top guard away as trivially-false for that pass,
  instead merging an equivalent check into the back-edge test — but the ORIGINAL's actual
  codegen re-checks the guard unconditionally on every iteration including the first (reached
  via a jump that skips only the back-edge's own register reload, not the guard check itself).
  Tell: `asmscore.py --dump` shows the original's top-of-loop guard `cmp`/`jge` as a `-`
  (missing from the candidate) while an equivalent `cmp`/`jle` shows up as a `+` near the
  back-edge instead. Fix is purely syntactic — same guard, same position in source, just a
  `while` instead of `if + do-while` — and combines with (doesn't replace) the other
  independently-needed levers on the same function: literal-comparison-form matching
  (`>= 200` over `199 <` inverted, and `i > strlen(...)` over `strlen(...) < i`, both from the
  "comparisons are emitted literally"/tri-state families) and never caching a dereferenced
  out-pointer parameter (write `*pHdc` at every GDI call site instead of `HDC hdc = *pHdc;`
  once — Yoda lesson #19's aliasing family, extended here to a plain output pointer, not just
  a member).
- **A `while (p != 0)` loop whose entry is provably-non-null gets its top guard ELIDED (do-while
  shape) by /O2 — UNLESS the loop head is a source-level-DISTINCT memory read (which CSEs to the
  same load but keeps the guard) rather than a proven-non-null cached local (VC5,
  `~DPlaySessionMgr`'s case-2 sub-list drain, v234, closed the whole match in one edit).** A
  queue-drain wrote `void *payload = node->pPayload; if (payload != 0) { switch(...) { case N:
  { T *p = (T*)payload; while (p != 0) {...} } } }` — because `p` derives from the already-null-
  checked `payload` local, /O2 proves `p != 0` at loop entry and drops the `while`'s top guard,
  compiling `do {} while` and mis-matching the original by exactly 2 insns (the missing
  `cmp reg,ebx; je exit`). Re-initializing the head from a FRESH read of the same field
  (`T *p = (T*)node->pPayload;`) makes the compiler CSE the load back to the cached register (no
  extra instruction) yet treat the value as an unproven memory read, so the top guard SURVIVES —
  matching the original. Tell: `asmscore.py --dump` shows the original's loop-entry `cmp/je` as a
  lone `-` (missing from the candidate) with the rest of the code aligned. This is the inverse of
  the do-while-peel family above (there the fix removes a spurious guard; here it restores a real
  one), same root cause: /O2's non-null range propagation into a loop head is steerable by whether
  the head is a cached local vs. a re-read field.
- **An `if`/`else` pair's BRANCH ORDER can be a genuine correctness lever, not just cosmetic
  block layout (VC5, `PostBag_ScanCategoryCrdFilesMaybe`).** Writing `if (p != NULL) {A} else
  {B}` vs. `if (p == NULL) {B} else {A}` is semantically identical but NOT interchangeable
  under this toolchain: the original puts one specific branch on the fall-through path (no
  forward jump), and picking the wrong source order can compile into a genuinely WRONG call at
  one of the two branches' own call sites — observed here as a `wsprintfA` call silently
  DROPPING one of its arguments (6 pushes/`0x18` cleanup instead of the required 7/`0x1c`) when
  the branch order was inverted from the original's. This is a real generated-code bug from
  picking the losing branch as fall-through, not the usual "block layout/cross-jump geometry is
  mostly not source-steerable" noise (Yoda #15) — when a branch's own arg-count/stack-cleanup
  looks wrong after an otherwise-faithful transcription, try swapping which condition is
  positive (`!= NULL` vs `== NULL`) before assuming the field/call model itself is wrong.
  **Counter-example (VC5, `PostBag_SaveCardToCategoryMaybe`, v150): this lever does NOT always
  work — a guard clause's fall-through direction can be genuinely NOT source-steerable when the
  earlier fix's own root cause (a dropped call ARGUMENT) isn't present.** A
  `if (!x) return 0;`-shaped guard whose original compiles with SUCCESS as the fall-through
  (failure jumps away to a small tail at the function's end) came out with the OPPOSITE
  polarity (failure as fall-through, an inline early-return epilogue right after the check) no
  matter which of 3 semantically-equivalent phrasings were tried (`if (!x) return 0;`,
  `if (x==0) return 0;`, `if (x) {<rest>} return 0;`) — all three compiled BYTE-IDENTICAL to
  each other (the 3rd, structurally different shape, scored WORSE, not just equal). Unlike the
  `PostBag_ScanCategoryCrdFilesMaybe` case, there was no dropped/malformed call argument
  downstream — just a pure block-placement choice, which IS in the "mostly not
  source-steerable" bucket after all. Don't assume every guard-clause polarity mismatch has a
  surface-syntax fix; verify there's an actual call-argument or content bug before spending
  budget on branch-order variants.
  **Third confirmation the lever DOES work (VC5, `AlbumCardWnd::FUN_004048e0`, v163, closed a
  366/468-byte residual in one step):** a null-check-first `if (pCard == NULL) {<erase-path>}
  <draw-path>` transcription (matching Ghidra's own literal decompile order) put the WRONG body
  (erase) on the fall-through path; rewriting as `if (pCard != NULL) {<draw-path>} else
  {<erase-path>}` (draw case first) immediately matched. Unlike the `PostBag_
  SaveCardToCategoryMaybe` counter-example (a trivial one-line guard return), this was two
  SUBSTANTIAL, differently-shaped bodies — suggesting the lever reliably applies when the two
  branches have real, unequal-sized content (the compiler has a genuine "which is more likely/
  cheaper as fall-through" choice to make), and is more of a coin-flip specifically for tiny
  single-statement guard clauses where there's little for source order to influence.
- **A `__thiscall` member function that never reads its own implicit `this` still forces the
  CALLER to materialize `this` in ECX — modeling the call as a plain free function (no `this`)
  leaves that materialization unexplained as a byte-diff (VC5, `UIResources::
  FUN_00447930`/`FUN_004479d0`, extends the already-documented `Widget::
  TestAndToggleMenuNodeHoverMaybe` precedent to a SECOND, independently-confirmed class).**
  Confirmed via the callee's own raw disasm: `FUN_004479d0`'s body only ever reads `[esp+...]`
  stack args, never `ecx` — yet every call site loads `mov ecx, <UIResources address>`
  immediately before the `call`, and that load is otherwise completely unused (clobbered again
  right after). Modeling such a function as a member of a minimal partial-view struct (`this`
  supplied implicitly by the call syntax) reproduces the load; modeling it as an ordinary free
  function does not. When TWO different functions on the same singleton show this same
  "this-in-ecx but never read" shape, treat it as the class's default calling convention rather
  than a one-off — check every other opaque call to that singleton for the same pattern.
  **Extended (v239, the NetSessionEventQueue edge-placement quartet 0x41d8f0/0x41d920/
  0x41d950/0x41d980): a thiscall-ignoring-`this` helper that ALSO returns a small (≤4-byte)
  struct BY VALUE is byte-identical on the CALLEE side to a `void __stdcall Fn(T *out)` model
  (the hidden return-pointer IS the stack out-param; ECX unread; both `ret 4`), which is why the
  callee's `__stdcall(out)` transcription still matches — but a CALLER modeled that way misses
  BOTH the `mov ecx,<singleton>` load AND the `mov reg,[eax]` deref of the returned pointer.** To
  byte-match a caller, model them on a methods-only partial view returning the struct BY VALUE
  (`Pair16 ComputeX();`) and invoke via the singleton — the compiler then emits the ecx
  load, allocates a per-call-site hidden return temp, and reads `*eax` into the destination, all
  matching. The two models coexist harmlessly (callee `.obj` and caller `.obj` are matched
  independently; the mismatched mangled name is a masked reloc). Don't "fix" the EFFECTIVE
  `__stdcall(out)` callee definitions to chase this — the caller-side by-value model is the only
  side that needs it. **A NEW caller can force reclassifying an ALREADY-MATCHED helper: a free
  `__stdcall` function that matched in TU A only because that caller happened to have ecx=this already
  live (so the `mov ecx,this` a thiscall would emit was a no-op there) will MISS that load in a new
  caller B where ecx holds something else — reclassify it to a this-ignoring `Class::` method (VC5,
  `LayoutNet_PostSimpleOpcode`/0x43d250, v251: SetMode's call had ecx=this live from a prior
  `[this+0x7f0]` store, so `__stdcall` matched; ApplyProviderSnapshot's call needed the explicit
  `mov ecx,this`).** The callee body is byte-identical either way (ignores ecx, both `ret N`), so its
  own match is unaffected, and the old caller stays matched (a thiscall call with `this` already in ecx
  emits no reload). ⚠ Because it is DEFINED+matched (has a `// FUNCTION:` marker), the src def-name FQN is
  checked by `lint_ghidra_sync` — so you MUST also move it into the class namespace IN GHIDRA (set
  convention `__stdcall`→`__thiscall` via `set_function_prototype`, then `set_function_this_type Class*`),
  unlike the declared-only this-ignoring-thiscall precedents (`GameNetMsgQueue_EnqueueOrProcessLocalNodeMaybe`
  etc.) which dodge the check because their header decls carry a bare `// 0xADDR` comment the lint ignores.
- **A Ghidra `__thiscall(void* this, ARG, ...)` free function with 2+ args must be modeled as a real
  C++ MEMBER of the `this`-class, NOT a free `__fastcall(T*, ARG)` — thiscall passes the SECOND (and
  later) args on the STACK with callee cleanup (`ret N`), whereas `__fastcall` passes arg2 in EDX
  (VC5, `GameNetThreadState::GameNet_RemoveOrRehomeNode`/0x43a6d0, v254, EXACT).** The tell in a
  `--dump` diff: the original does `mov ebp,[esp+0x14]` (2nd arg from stack) while a `__fastcall`
  recompile does `mov ebp,edx` (2nd arg from register) — a large reg-alloc cascade that reads like
  noise but is a pure convention mismatch. ⚠ This is exactly why a ONE-arg free `__fastcall(T*)` model
  DOES byte-match (a 1-arg thiscall and 1-arg fastcall are identical — the sole arg is ecx either way,
  `ret 0`): the divergence only appears at the 2nd argument. So a whole family of Ghidra-`GameNet`-
  namespaced `void*this` handlers on one manager object are really thiscall methods of that object —
  model them `void Class::Handler(args)` (in a `struct Class{...; void Handler(args);}` partial view),
  which emits thiscall AND cleans up Ghidra's decompile via `set_function_this_type`. `lint_ghidra_sync`
  stays happy either way (its last-`::`-component match treats src `Class::Handler` == Ghidra
  `GhidraNamespace::Handler`), but this-typing them in Ghidra promotes it to an exact-FQN match.
- **A byte-narrowed struct-field store to a call/notify node (`node->byteField = (uchar)mgr->intField`)
  byte-loads by default, but the original often materializes the full dword (`mov edx,[edx+0x7d0]`) —
  same v236 fix (an `unsigned int` temp read just before the store), confirmed a SECOND time (VC5,
  `GameNet_DrainBlockedTrainListAsNotify`/0x43cc40, v254, the sole byte between DIFF(1) and EXACT).**
- **A struct-copy assignment `dst = src` (both small memory locals) can be REGISTER-PROMOTED by
  /O2 — coalescing `src`→`dst`→a register and eliding the original's memory materialization —
  while a FIELD-WISE copy (`dst.lo = src.lo; dst.hi = src.hi;`) forces the memory round-trip the
  original does (VC5, `DPlaySessionMgr::AttemptQueuedTrainPlacement`, v239, closed a
  102384→56456 residual).** When the original stores a computed struct to a named stack slot and
  reads it back field-by-field (word-by-word for a 2×`short` struct) before a by-value push, but
  your `dst = src` compiles to a single register that's pushed directly, rewrite the copy
  field-wise — it reproduces the per-field stores/loads and the fixed stack home. Same family as
  the "faithful struct-pointer walk reproduces a hand-computed offset" lesson, extended to a
  whole-struct copy's register-vs-memory residency. **A companion lever on the same function: an
  explicit intermediate `tmp` local between a by-value-returning call and its final consumer stops
  /O2 from SPECIALIZING a constant-args sibling branch into a DUPLICATED call** — here the
  connecting-mode path builds a const `(0,0)` coord + 0 quadrant, and writing `coord = ComputeX()`
  directly let /O2 inline a whole separate slot-registry call for that path; routing every case
  through `tmp = ComputeX(); coord.lo=tmp.lo; coord.hi=tmp.hi;` forced the shared-tail cross-jump
  (one call reached by both paths) the original uses.
- **Reusing ONE variable as both a `switch` key and a later-computed tag keeps it in the preferred
  callee-saved register; splitting into a second variable spills an extra register and de-syncs
  the whole allocation (VC5, `AttemptQueuedTrainPlacement`, v239).** The original reuses
  `dir` (the heading `switch`-key, in esi) as the quadrant tag it later passes on — a separate
  `char quadrant` local forced the allocator to pull in ebx and push `dir`/`pNode` off esi/edi,
  turning a clean register match into ~20 points of reg_pen plus a quadrant byte-slot. Extends
  Yoda #7's register-reuse family: when the original passes a `switch`-key-derived value onward,
  reuse the key variable rather than introducing a second one, even when a separate name reads
  cleaner.
- **A lone `if (x > 0)`/`test reg,reg; jbe` shape can survive on a genuinely SIGNED field
  without widening the field's own declared type, via an explicit unsigned cast at just that
  one comparison site (VC5, `AlbumCardWnd::PurgeDuplicateCardsMaybe`, v163, EXACT match).** The
  same field (`nStartIndex`, a real signed `int` — confirmed by TWO other same-function
  comparisons against it correctly needing signed `jl`/`jge`) compiles its own standalone
  `!= 0`-style guard as unsigned `test reg,reg; jbe` (`jbe` reduces to a pure zero-test after
  `test`, since `test` always clears CF — behaviorally identical to `je`, but a DIFFERENT
  opcode byte the tooling still flags as a structural mismatch). Widening the field itself to
  `unsigned int` would have flipped those OTHER, already-correct signed comparisons to unsigned
  too (a signed/unsigned mixed comparison forces the signed operand to convert) — instead,
  writing `if ((unsigned int)nStartIndex > 0)` at only the one site needing it reproduced
  the exact `jbe` without touching the field's real type or any other comparison. Extends the
  already-documented `LoadIndexedFileMaybe` "redundant recheck needs a RANGE not EQUALITY
  compare" lesson to the case where the field can't simply be redeclared unsigned wholesale.
- **A `WindowBase`-derived (or any virtual-dtor-bearing) class's Ghidra struct has an EXPLICIT
  `vtable` component at `+0x0` that the class's OWN `src/*.h` header deliberately OMITS** (the
  compiler synthesizes it; see the header's own comment) — reading the header's field list
  naively and counting offsets from 0 is off by 4 bytes for EVERY real member (VC5,
  `WindowBase`, v163). Ground-truthed by dumping Ghidra's live struct DB directly: `+0x0
  vtable`, `+0x4 hInstance`, `+0x8 hwndSelf`, `+0xc hwndOwner`, `+0x10 resourceId`, `+0x14
  pCursorBitmap` — a naive header read (hInstance/hwndSelf/hwndOwner/resourceId assumed
  at 0/4/8/0xc) would have mis-called `+0x8` as `hwndOwner` instead of the real `hwndSelf`.
  Before writing a new call site that reads an inherited base-class field by a "obviously
  matches the header's Nth listed member" guess, cross-check the LIVE Ghidra struct dump (a
  `this`-typed decompile of ANY already-analyzed sibling method showing that field name is the
  cheapest confirmation) rather than counting header field declarations by hand.
- **A candidate that is MORE uniformly register-allocated than the original can lose a
  byte-match by being BETTER optimized — cross-jumping is downstream of register allocation
  (v348, parked `CarNetObjAnchorPartial::CheckCarClearedDepotMaybe` 0x40e520).** A 4-case
  jump-table switch whose four case bodies end in the SAME two statements
  (`this->field = 4; this->Virtual(1);`) compiled to ONE cross-jumped shared copy in the
  candidate, but the ORIGINAL keeps TWO physical copies (cases 7/8 share one, 9/10 another).
  The cause is not a source-shape difference: the original's OWN allocator emitted two
  structurally IDENTICAL cases (8 and 10) with OPPOSITE EAX/EDX assignments for the same two
  scratch temps, which left its two settle blocks holding the vtable pointer in different
  registers — hence byte-different, hence un-mergeable by the tail cross-jumper. The candidate
  allocated the same registers in all four cases, so all four tails WERE byte-identical and got
  merged. **Diagnostic value: when the original duplicates a small block that your candidate
  merges (or vice versa), check whether the two copies differ only by register before hunting
  for a source-level structure that would force the duplication — if they do, the duplication
  is an ARTIFACT of an allocator asymmetry you cannot express in source, and the function is a
  park, not a puzzle.** ⭐ **v377 turned the OTHER half of this into a real, function-winning
  lever, and v378 confirmed the split — run the diagnostic FIRST, it tells you which half you
  are in.** Dump both of the ORIGINAL's copies and compare:
  - **Byte-identical apart from the jump displacement ⇒ a SOURCE lever exists: write the tail
    out TWICE.** v377, `SplashWnd::OnEnterCommitAndDispatch` (0x422660) sat at DIFF 14 with
    142/142 instructions and identical registers; the original hoists a `push 1` ABOVE the
    compare guarding a conditional call and re-pushes it inside the guarded arm, which is only
    reachable when the guarded arm is an early `return` with its whole three-call tail REPEATED
    in the source. The natural single-tail form (`if (bSkip) { SetMode(1); }` + one shared tail)
    compiles to the same instruction count and costs 14 bytes. Duplicating it took the function
    EXACT.
  - **Different registers and/or scheduling ⇒ PARK, no source shape reaches it** (the v348 case
    above; v377's `SplashWnd::OnActivate` 0x421be0; v378's `SplashWnd::OnUnhandledMessageMaybe`
    0x420ec0). The original duplicated BEFORE register allocation, cl 11.00 cross-jumps BEFORE
    it, and two textually identical source blocks cannot be told apart downstream.
  - **⭐ A THIRD case that reads like the second but is actually the FIRST: the two copies differ
    only in WHERE THE SAME VARIABLE LIVES — register on one path, spill slot on the other (VC5,
    v386, `MapWnd::OnMouseMove` 0x432540).** That is positive evidence FOR source duplication, not
    against it. A single shared block has ONE predecessor-merge point, so the variable has to be
    materialized into ONE location before it; the compiler would insert a reload on whichever path
    clobbered the register (exactly what the candidate emitted). Two copies each reading the
    variable from wherever it happens to live on THEIR path can only come from two source blocks —
    here `if (nHoverSlotMaybe >= 0) { PlaySoundAtScreenPos(0x5015, pt.x, pt.y, 4);
    nHoverSlotMaybe = -1; }` written out once per exit path, one reading `pt.y` from EBP and one
    from the spill slot (EBP having been reused for `rectGrid.top` on that path). cl tail-merged
    them from the `push pt.x` down but left each its own guard. Hoisting it into a shared tail via
    an `int nSlot` carrying the value cost 4 instructions and 69 bytes; duplicating took the
    residual 105 B → 36 B. **Diagnostic: compare the two copies OPERAND BY OPERAND, not just
    register-by-register — a `mov reg,[esp+N]` appearing in only ONE copy is the tell.**
  - **⭐ A FOURTH tell, and by far the cheapest to check: a STORE SUNK INTO A CALL SETUP proves
    the tail it sits in is BRANCH-PRIVATE — so look at where the equality branch LANDS, not at
    how many tails exist** (VC5, v388, `MapWnd::OnMouseMove` 0x432540, which this took from
    DIFF 36 to DIFF 14 with `align=0`). The original's `je` at 0x43262c jumps clear OVER a full
    inlined copy of the `RequestModeTransitionFromSource(...)` tail and lands on the shared tail
    at 0x4326a4, 0x5f bytes further on. The two tails are otherwise identical, so nothing about
    their contents explains why both exist — but the branch-private one carries
    `mov [esi+0x270],edi` wedged at 0x432655, between `mov ecx,esi` and the `call`. A shared
    tail has other predecessors that must NOT perform that store, so the scheduler can never sink
    it there; it can only fill a slot in a block reached solely by the path that owns the store.
    **Source form: the guarded branch ends in its own `return`, repeating the tail, instead of
    falling through** (`if (nSlot != nHoverSlot) { PlaySound(...); nHoverSlot = nSlot;
    Transition(...); return 0; }` rather than `if (...) { PlaySound(...); nHoverSlot = nSlot; }
    Transition(...); return 0;`). Written the second way the store is stranded ahead of the whole
    argument setup and every copy of the tail merges. **Recognition rule: any lone `mov`
    interleaved into an argument-push/`mov ecx,this` sequence is a duplicated-tail signature —
    follow the branch that skips it and see which copy it reaches.**
- **The merge direction matters and both directions happen. When your SOURCE naturally contains
  two identical tails — e.g. two `switch` cases that end with the same call sequence — cl 11.00
  merges them even though the original kept both copies inline (v378, 0x420ec0: 20 instructions
  / 65 bytes, the entire residual on an otherwise content-complete 818-byte function).** The
  symptom is asymmetric and easy to misread: the candidate's guard becomes a `jne` FORWARD into
  the other case's copy where the original has a `je` that falls straight into its own. Run the
  same two-copy diagnostic above before spending any time on it.
- **C cannot express a CONDITIONAL fallthrough between `switch` cases without a `goto`, and
  writing that `goto` is much worse than leaving the merge alone (v378, 0x420ec0: total 165222
  → 454764).** When the original's failure path jumps into the NEXT case's body while its
  success path falls through inline, the `if (ok) { ...; break; } /* fall through */` form is
  the right transcription even though it merges — the explicit
  `if (!ok) goto next_case;` form additionally REORDERS the surrounding blocks and loses far
  more than the merge costs. Five source shapes were tried and all compiled BYTE-IDENTICALLY (VC5
  canonicalizes past each): commutative addend order, three groupings of a `+ k` term inside
  the same expression, `return 0;` per case body vs a shared `break`, and per-case named bound
  locals with case braces. Confirms the existing "operand order of a commutative `+` is not a
  usable lever" observation and extends it: **neither is regrouping the constant term.**
- **⭐ Commutative operand order IS a usable lever in a plain frame, but an EH edge earlier in the
  same `try` LOCKS cl into canonicalizing by ascending field offset — measure the frame, not the
  expression (v442, 0x447ba0 `ThumbnailBmp::ThumbnailBmp_Load`, DIFF 2 of 520 B).** The symptom is
  the smallest possible residual: two displacement bytes deciding which of two same-width field
  loads feeds the DESTINATION register of a commutative `imul`, with `insns N/N`, `align 0`,
  `reg_pen 0`. A standalone probe settles the mechanism in one compile: for
  `n = (unsigned short)a * (unsigned short)b;` inside a `/GX` try block, cl HONOURS the source
  order (`b * a` and `n = b; n *= a;` both put `b` in the dest) — but add a `throw` on an earlier
  check INSIDE the same try, ahead of the multiply, and every spelling collapses to the same
  output with the LOWER field offset in the dest. Nine shapes were refuted against the real
  function: both operand orders, both compound-assignment orders, a function-scope declaration
  with either order, `int` vs `unsigned int` for the product, and an `unsigned short` temp for
  either operand. So: before spending probes on operand order, check whether the expression sits
  downstream of a `throw` (or any other EH edge) in its own try — if it does, the lever is gone
  and the residual is intrinsic. Related and probably the same class: `LocoBitmap::Fill`'s own
  2-byte `width * height` residual (see the `WorldBoardPartial` declaration-lottery bullet).
  **Also confirmed there: repeating the size expression at every use site instead of holding it in
  a local is NOT a neutral restyle** — cl recomputes rather than CSEs (DIFF 2 → DIFF 127, and the
  function grows 44 bytes), so an explicit local is the right transcription of a value the
  original computes once.
- **A shared header's TRANSITIVE closure can rotate an unrelated TU's codegen, and the fix is to
  split the class you actually need into its own minimal forward-declaring header (v378).**
  Merging `GameNetMsgQueue` into `GameNetThreadState` meant `src/DPlaySessionMgr.cpp` needed the
  complete type, but `#include "GameNet.h"` (which also carries the whole msg-0x3ea..0x3fd wire
  record set) knocked `ApplSetupWndPartial::SelectGridCellFromPointMaybe` (0x40aba0) off its byte
  match, 166 → 170 B. **The rotation is position-INDEPENDENT** — four include slots probed, all
  identical — so do not waste probes reordering includes; bisect the CONTENT instead
  (`ThreadWrapper.h` and a lone `extern "C"` decl were both inert; GameNet.h as a whole was not).
  Because every `GameNetThreadState` member is a pointer, a new `src/GameNetThreadState.h` with
  plain forward declarations restored the match exactly. **Prefer this split to giving up on the
  merge: one class per header, forward-declare the rest.**
- **When a vtable slot's default body says `ret N` but the C++ declaration takes no arguments,
  that is unfinished modelling, not a quirk — fix it the moment you find a real call site
  (v378).** `src/WindowBase.h` had documented slot 0x14's default (0x426130) as `ret 0x4`, one
  stack arg, for many sessions while declaring `virtual void *NoOpVirtualMaybe();`. The first
  known call site (`SplashWnd::StartGameNetThread`'s worker-thread-failed path) passes a literal
  0, and the declaration has to grow the parameter before the `push 0` can be emitted. Adding
  the parameter was inert repo-wide — nothing else calls the slot — so this is cheap; the
  expensive version is discovering it mid-transcription and mistaking the missing push for a
  scheduling residual.
- **⭐ SWEEP THE FUNCTION'S POSITION IN THE TU BEFORE PARKING ANY register-allocation residual
  (v349).** This is the single highest-yield probe found so far and it is nearly free: move the
  function's whole block to each other slot in its own `.cpp`, re-score, keep the best. Yoda #7
  says allocation is TU-context-dependent; v349 turned that from an explanation-after-the-fact
  into an ACTIVE lever. Three functions in `src/PeerTrainNode.cpp` went from a 19-register-rename
  residual to BYTE-EXACT purely by being hoisted above `UpdateCarPlacementTickMaybe` —
  `CheckCarEnteredDepotMaybe` (0x40e440), `StartFrameSoundMaybe` (0x40e130) and, crucially,
  `CheckCarClearedDepotMaybe` (0x40e520), which v348 had parked as intrinsic. Not every function
  responds (`CompleteViewportExtendMaybe` 0x40e340 and `CanReverseDirectionMaybe` 0x44d630 score
  IDENTICALLY at all 12 positions — those are the genuinely intrinsic ones), which is exactly
  what makes the sweep a clean two-way classifier. Automate it: extract the block between its
  leading comment run and its closing `}`, re-insert before each other marker, call
  `asmscore.py` per variant. **Always re-run the full `tools/cc.sh` after accepting a move** —
  the position that fixes one function can perturb its neighbours — and record the dependency in
  the function's own comment ("keep this block here") so a later tidy-up doesn't silently undo it.
  ⚠ Run only ONE such sweep at a time: two concurrent sweeps both rewriting the same `.cpp`
  interleave their writes and corrupt each other's baseline (hit for real in v349).
- **Triage which residuals are worth a position sweep by `asmscore`'s `reg_pen` (v349).** The
  lever acts on register ASSIGNMENT, so a residual with `reg_pen=0` cannot move — its diff is
  instruction selection or a structural gap, and v349 confirmed this on three PeerTrainNode
  functions (`ReverseDirectionMaybe` 0x44cb10 reg_pen=0 insns 246/223, `SetSoundStateMaybe`
  0x44d740 reg_pen=0, `ComputeHeadingAngleMaybe` 0x40df80 reg_pen=0): all three score IDENTICALLY
  at every legal position. `reg_pen>0` is necessary but not sufficient — 0x40e340 (reg_pen=8) and
  0x44d630 (reg_pen=1) are also flat across all positions. So: skip the sweep on `reg_pen=0`, run
  it on anything with `reg_pen>0`, and treat a flat sweep as the real proof of intrinsic-ness.
  A big `reg_pen` relative to the function's size (19 of ~64 instructions, in v349's three wins)
  is the strongest signal that the whole residual is one allocation coin-flip waiting to be
  flipped.
- **⚠ v350 RE-SCOPES the position lever: it is REAL but NARROW, and `reg_pen>0` is a weak
  filter.** All three v349 wins were in ONE TU (`src/PeerTrainNode.cpp`) on small, closely
  related functions. v350 swept seven functions across six OTHER TUs — 0x41a360, 0x43f880,
  0x447400, 0x437900, 0x40d340, 0x40d170, 0x40cd60 — and EVERY sweep was flat with spread 0
  (table in `docs/PARKED.md`). 180 of the repo's 219 non-exact functions have `reg_pen>0`, so
  that test alone selects almost everything. Two sharper discriminators, both from v350:
  (a) **WHERE the register differences sit beats HOW MANY there are.** 0x41a360
  (`EditCardWnd::SelectDecalSlot`) scores `align=0 reg_pen=2 byte_diff=1` — one byte from exact —
  yet its entire residual is a single instruction's SIB base/index ORDER
  (`[esi+edx+0x370]` vs `[edx+esi+0x370]`), an artifact of the source expression's operand order,
  not an allocation choice. Nothing positional can touch it. v349's winners had `reg_pen` spread
  over ~30% of their instructions; a `reg_pen` concentrated in one or two instructions is an
  encoding/expression-order artifact. (b) **A large `align` cost means a source-shape gap, and
  the sweep cannot fix those either — fix the shape first.** On 0x40cd60 three source-shape
  findings took the score from 604568 to 468196 while the sweep moved it by zero.
- **Switch-case SOURCE ORDER is physical block order, and the jump table proves what it was
  (v350, `ApplyDirectionReversalMaybe`/0x40cd60 — worth ~130k of score, the single biggest lever
  on that function).** Both of its phase switches emit `case 1`'s block at a LOWER address than
  `case 0`'s (read the jump tables at 0x40cf30/0x40cf68 directly — each entry is the absolute
  target of one case value, so sorting entries by target address recovers the original's block
  layout). Writing the cases in numeric order costs real score even though the code is
  semantically identical. The order also carries a second effect: laying `case 0` immediately
  before `case 4/5` is what lets `case 0`'s tail FALL THROUGH into the store those cases share,
  which is how the original avoids a duplicate immediate store. So when a switch residual shows
  duplicated stores or an unexpected `jmp`, dump the jump table, sort by target, and re-order the
  source cases to match before reaching for anything else.
- **A flat `if (A && B) … else if (A)` chain and nested `if (A) { if (B) … } else …` are NOT
  interchangeable (v350, 0x40cd60).** Only the FLAT form makes VC5 re-test `A` on the
  B-mismatch path — the original's otherwise-inexplicable redundant `test eax,eax; je` at
  0x40ce6c, where the tested register is provably already 0. A redundant-looking re-test of an
  outer condition in the original is a positive fingerprint of a flattened chain in the source.
  (Hoisting `A` into a local is byte-neutral either way — VC5 already CSEs the field read.)
- **Repeated `mov reg, <small constant>` rematerializations in the CANDIDATE are a register-budget
  fingerprint, and they tell you exactly which register the original spent differently (v350,
  0x40cd60).** The original pinned the constants 1/4/3 in ebx/ebp/ecx for the whole function and
  RELOADED its pointer parameter from `[esp+0x14]` at each of its two uses; this compile instead
  cached that parameter in ebp and, having no callee-saved register left for the constant 1, had
  to rematerialize `mov edx,1` at FOUR separate sites — which was the entire 228-vs-224
  instruction overshoot — and additionally folded two global reads into their `cmp` operands
  rather than loading them to a register first. Counting the duplicated constant loads tells you
  how many registers of budget you are off by; there is no source lever for "do not cache this
  parameter", so this class parks (a full position sweep on it was flat).
- **A pointer local used TWICE defeats VC5's address-folding, and that is source-recoverable
  (v349, `StartFrameSoundMaybe`/0x40e130).** `short id = p->arr[i].field; if (id) f(id);` lets VC5
  fold the whole address into the load's own `[base + idx*8 + disp]` operand; the original instead
  emitted a standalone `lea` for the element address, which only reproduces when the element
  POINTER itself has two uses — `T *pE = &p->arr[i]; if (pE->field) f(pE->field);`. Score went
  10230 -> 224 on that one change, where four other shapes (naming the row pointer, binding a
  reference, hoisting the index into a local) were all byte-identical to each other. When a diff
  shows the original materializing an address the candidate folded away, look for a second use of
  that address in the source rather than for a scheduling trick.
- **⚠ A minimal-TU probe scoring IDENTICALLY to the full-TU compile does NOT prove a residual is
  intrinsic — v348 claimed this and v349 refuted it.** A standalone probe TU carries its own
  optimizer state into the function, and that state coincides with SOME position in the real TU —
  possibly a bad one. v348's minimal-TU probe of 0x40e520 reproduced the full-TU score exactly and
  was read as "intrinsic, retry never"; the function is in fact position-sensitive and now matches
  byte-for-byte. (The probe is not useless — for 0x40e440 it happened to land on the GOOD position
  and correctly signalled that a matching allocation existed. But it can only ever produce a
  positive hint, never a negative proof.) Use the position sweep above as the actual classifier.
- **⭐ A shift-INSTRUCTION mismatch (`sar` vs `shr`) is a TYPE fact, never a scheduling or
  allocation tie — chase the operand's signedness, not the surrounding code.** v352's
  edge-placement quartet (0x41d8f0/0x41d920/0x41d950/0x41d980) sat parked from v3 as "pure
  instruction scheduling" while emitting `shr` against the original's `sar`; the cause was
  `g_worldBoard.wCols`/`wRows` declared `unsigned short` when the fields are signed `short`.
  Fixing the shared header's field types alone was worth ~10000 asmscore points across the four.
  The same applies to `div`/`idiv`, `movzx`/`movsx`, and `ja`/`jg`-family branch selection: the
  compiler picks these from the STATIC TYPE, so a mismatch always means a wrong type somewhere,
  and no amount of statement reordering can move it.
- **⭐ The RETURN convention is a matching lever; the PARAMETER convention (for a single leading
  pointer) is not.** MSVC returns a struct too large/non-trivial for EAX through a HIDDEN buffer
  pointer, and the callee leaves that pointer in EAX at `ret`. **That trailing
  `mov eax, <buffer>` before the epilogue is a reliable ABI fingerprint** — if you see the
  function load a stack argument into EAX right before returning and never use it, you are
  looking at a by-value struct return, not an out-param. v352: modelling the edge-placement
  quartet as `Pair16 __thiscall Method()` instead of `void __stdcall F(Pair16 *out)` took all
  four from parked to EXACT, because the convention is what pins the buffer load to EAX and
  forces it LATE (which had been misread as a scheduling residual for 349 sessions). VC5 does
  NOT return even a 4-byte POD in EAX here. Contrast 0x440390, where converting
  `__fastcall F(T *p)` to a `__thiscall` member was byte-for-byte NEUTRAL: with one leading
  pointer those two conventions are ABI-identical (ECX, no stack args, plain `ret`), so that
  flip is never a lever — and correspondingly never evidence either way about which the source
  used.
- **⚠ When two TUs model the same function with different signatures, ONE of them is wrong and
  the byte-match will not tell you which** — each side matches its own call/return shape locally.
  v352: `src/DPlaySessionMgr.cpp` had the edge quartet as by-value-returning members (right)
  while `src/NetSessionEventQueue.cpp` defined them as `__stdcall` out-param free functions
  (wrong), and both compiled "fine" for 349 sessions. When a cross-TU signature disagreement
  turns up, treat it as an unresolved bug, not a stylistic split — and prefer the CALLER's model
  when the caller is already matching.
- **Keep a value ALIVE or DESTROY it — that choice, not register naming, decides `add` vs `lea`.**
  `lea edx,[eax+4]` means the source still needs the un-advanced value; `add ecx,4` means it does
  not. v352's 0x46c480 was parked as an "ADD-vs-LEA instruction selection" tie; it was really a
  liveness statement. `int *p = *cursor; *cursor = p + 1; return *p;` keeps both pointers live
  (forcing `lea`), while advancing in place and indexing back — `*cursor += 1; return (*cursor)[-1];`
  — destroys the old value and matched first compile. Read a stray `lea` as "I have one live
  value too many."
- **8-bit arithmetic comes from the DESTINATION's width, not the parameter types.** Any `char`
  operand promotes to `int` inside a single expression, so `return (b - 1) | (a << 3);` widens no
  matter how the params are declared — permuting `char`/`unsigned char` params is a dead end.
  Accumulating into a `char` LOCAL (`char r = b - 1; r |= a << 3; return r;`) makes VC5 narrow
  each step to the destination, reproducing the original's pure AL/CL sequence (v352, 0x445910,
  parked since v2 on exactly the param-type dead end). The statement split is also what fixes
  operand ORDER for free: the original loads `b` first because `b - 1` is the first statement.
- **⭐ A switch's jump-table BASE is set by the minimum CASE LABEL, so a `dec`/`lea`-normalized
  table means your source is missing a low case label — usually one you folded into `default:`.**
  Merging `case 0:` into the `default:` arm removes 0 from the case set, so MSVC subtracts the
  new minimum and emits a smaller table; keeping `case 0:` as a distinct arm (even with an
  identical body) yields the original's flat table with the gap entries pointed at `default`.
  v352 solved a residual mis-parked across ~12 PostBag sites for 12 sessions this way, roughly
  halving DIFF at 5 of them. Read the table's ENTRY COUNT and the `cmp` bound off the raw bytes
  to recover the true case set before theorising.
- **⚠ VC5 tail-merges two switch arms with identical bodies; the original build did not. The
  lever is `/Gf` STRING POOLING, and it is NOT one you may pull (v353).** Two arms assigning the
  same string literal only become identical BLOCKS because pooling collapses the two literals
  into one symbol; the optimizer then merges them, losing one 7-byte block, whereas `Loco.exe`
  keeps both and points every reference at the second. Turning pooling off (`/Ox`, which is
  exactly `/O2` minus `/Gf`) or forcing textually different literals both suppress the merge and
  take 0x445400/0x445510/0x445930 to EXACT — but keep NEITHER. `/O2` == `/Ox /Gf` == `/Ox /GF`
  to the byte repo-wide, and `/Ox` alone costs 3401 B / 2 funcs elsewhere. See the next bullet
  for why this is closed rather than merely parked.
- **⭐ A byte-EXACT result is not automatically a SEMANTIC match — check the `.obj`'s own
  relocations before crediting a toolchain hypothesis (v353).** Every arm of a string-dispatch
  switch is `mov eax,<masked reloc>; jmp <common tail>`, so blocks that differ only in which
  string they load are INDISTINGUISHABLE to the matcher. That is how `/Ox` "matched" 0x445930
  while producing different code: in the `/Ox` `.obj` all 10 blocks are LIVE, whereas the
  original's first block is DEAD (zero references image-wide — confirmed by scanning `Loco.exe`
  for the dword 0x00445973). The original MERGED and left the orphan behind; cl 11.00.7022 never
  does that under any `/O`, `/G` or `/Ob` combination, so the residual is a VC5 compiler-BUILD
  difference, not a command-line one. Read jump-table entries and `ja` targets out of the raw
  bytes and compare against the candidate's relocation list whenever a flag experiment claims a
  win.
- **⭐ Rule out per-file flag overrides with ADJACENCY, not with plausibility (v353).** If two
  functions demand opposite settings of the same flag and their COMDATs abut in `.text`, they
  are in the same `.obj` and no flag assignment can satisfy both — hypothesis dead, one lookup.
  0x445930 (wants pooling off) ends at 0x445a3f and 0x445a40 `PostBag_ReadDatFile` (wants
  pooling on: DIFF 16 with `/Gf`, DIFF 112 without) begins at the next byte.
- **Whole-repo flag experiments are a one-liner: `LOCO_OPT=/Ox tools/progress.py`.** `$LOCO_OPT`
  overrides the optimization level alone, leaving `/MT /Gy /GX` and the defines intact, so a
  candidate flag is scored against all 235 matches in ~25 s instead of one TU's local opinion.
  Always score a flag hypothesis repo-wide — `/Ox` looks like a 2-function WIN on
  `src/EditCardWnd.cpp` alone and is a 2-function, 3401-byte LOSS overall.
- **⚠ `asmscore.py --len` wants the COMDAT extent, and BOTH obvious sources give you the CODE
  extent instead.** `toolchain/test/app_funcs.txt` and Ghidra's `Body:` span stop at the last
  instruction, excluding the jump table that the linker copies as part of the same COMDAT. On a
  switch-heavy function that truncates the compare window and manufactures a fake "body is
  missing" reading — 0x445400 scored `insns 88/51` under app_funcs' `--len 193` (reads as ~37
  instructions absent) versus a true `insns 88/88` under its real `--len 0x110` (272 = 200 code
  + 72 table). Derive `--len` from the NEXT function's start address whenever the function
  contains a switch. This is the mirror of the already-documented trap of omitting `--len`.
- **Most of `align` on a switch-heavy function is masked jump-table noise, so read the DUMP, not
  the score.** The candidate's table entries are relocations (masked to zero, decoding as
  `add byte ptr [eax], al`) while the original's are real addresses, so the two never align.
  0x445400 showed `align=242` — of which every row past the code was table garbage and the real
  code disagreement was THREE instructions. Judge a switch function by the diff rows above the
  table boundary; the headline score is unusable for it.
- **⭐ Re-read a member you JUST WROTE, don't re-read the source it came from — MSVC forwards
  the stored value as a register COPY, and that copy is the fingerprint (v354).** A pair of
  stores `a = expr; b = expr;` compiles to two loads of `expr`; `a = expr; b = a;` compiles to
  ONE load plus a `mov cx,dx`-style copy, because MSVC's redundant-load elimination forwards the
  just-stored value instead of re-reading memory. So an apparently pointless register-to-register
  copy sitting between two stores of the same value is telling you the SECOND store's source
  operand is the FIRST store's DESTINATION in the original source, not a second evaluation of the
  common expression. In `PeerTrainNode`'s ctor this single rewrite —
  `this->wSelectedCarId = this->wSelectedCarIdAMaybe;` in place of `= pKind->wCarIdAMaybe;` —
  moved DIFF 233 → 174. The mirror-image tell is documented below for `delete`: whether a value
  is CACHED or RE-READ is a source fact you can steer, in both directions.
- **`++mem` used as a VALUE re-reads memory after the store; `int n = mem + 1; mem = n;` keeps it
  in a register (v354).** MSVC's aliasing is conservative enough that a store through a global
  pointer invalidates BOTH the field and the pointer load itself, so
  `int n = ++g_p->field;` compiles to load-ptr / load-field / inc / store-field / **re-load-ptr /
  re-load-field**. Hoisting the arithmetic into a local first (`int n = g_p->field + 1;
  g_p->field = n;`) gives the original's single pointer load and keeps the incremented value live
  in a register. Same family as the cached-vs-re-read `delete` lever: pick the form that matches
  the loads you actually see.
- **A byte-sized VALUE feeding two byte stores comes from an `int` LOCAL, not from two byte-typed
  reads (v354).** `x = (unsigned char)mgr->intField; y = (unsigned char)mgr->intField;` compiles
  to two `mov al,byte ptr [..]` loads; the original's single `mov eax,dword ptr [..]` followed by
  two `mov [..],al` stores means the source held the value in an `int` local and let the two byte
  stores narrow it. The load WIDTH is the tell, and it is a source-shape fact, not scheduling.
- **⭐ How a byte argument is PUSHED tells you the PARAMETER's declared width (v354).** Three
  distinct shapes, and only the third means "the parameter is wider than the argument":
  (a) `mov dl,[..]; push edx` — a 1-byte param, pushed with don't-care garbage upper bits;
  (b) `mov [esp+N],al; mov eax,[esp+N]; push eax` — ALSO a 1-byte param, the same don't-care
  widening done through memory (often a dead incoming-parameter stack slot); which of (a)/(b) you
  get is allocator-driven noise, so do NOT read the asymmetry as a type difference;
  (c) `xor edx,edx; mov dl,[..]; push edx` — a real zero-extension, i.e. the parameter is `int`/
  `unsigned int` and the argument is a byte. Seeing (c) in your build where the original has (a)
  or (b) is a genuine, fixable TYPE bug in the callee's declaration. Corroborate from the callee:
  a body that forwards the incoming stack dword verbatim into another call (`mov eax,[esp+arg];
  push eax`) while only ever comparing its LOW byte is a 1-byte param being passed through.
  Fix the whole call family at once — fixing one member while its siblings still say `unsigned
  int` makes the shared body WORSE, which reads as a refutation when it is really an
  inconsistency (v354: `bOwnerB` across the 4 placement-result functions, free of any exact-match
  cost and worth DIFF 321→268 and 208→113 on two other functions).
- **⚠ A Ghidra decompile that indexes a pointer as `p[1].field` where the offset equals
  `sizeof(declared type) + k` is a DECLARED-TYPE artifact, not a real array access — the object
  is a LARGER SIBLING class (v354).** `pKindDesc[1].base.bBitmapOccupancyRowsMaybe` at +0x7a8 was
  not the next descriptor in a table: 0x7a8 == `sizeof(BigObj)` (0x63c) + 0x16c, and the pointer
  really targets a 0x7ac-byte `CarKindDesc` that shares only the `CursorDesc` base. Confirm by
  finding the allocation site (`push 0x7ac` into `operator new` in the class's factory) and a
  WRITER of the offset (here a stream-extraction `lea`), then model the sibling and cast at the
  consumer. Believing the array reading instead would have produced a real semantic bug that
  still byte-matched.
- **⚠ Putting a NEW class in a widely-included shared header costs matches even though nothing
  existing changed size (v354).** Adding `CarKindDesc` to `src/CursorDesc.h` rotated
  `src/Obj0x4779e0.cpp` and `src/DPlaySessionMgr.cpp` and cost each one exact match (655 B);
  moving the identical definition into its own `src/CarKindDesc.h`, included only by the one
  consumer, restored both to the byte. Same hazard and same fix as `src/TimeOfDayMaybe.h`'s v331
  note — a new class's canonical home is its OWN header unless a sibling genuinely needs it.
  Corollary for measurement: when a session touches a shared header AND changes a type, diff the
  per-file table before blaming the type change — v354 nearly reverted a correct type fix because
  the header churn was charged to it.
- **⭐⭐ A local BUFFER can be WIDER than the size argument passed alongside it — derive its size
  from the FRAME, never from the API call (v357, the whole TileKind/locale loader family).** All
  three string loaders in `src/UIResources.cpp` declare a **264-byte** (`0x108`) buffer and then
  hand LoadStringA a cap of `0x104`. Writing the obvious `char buf[0x104]` + `sizeof(buf)` leaves
  the frame exactly 4 bytes short, and the symptom is unmistakable once you know it: EVERY stack
  displacement above the buffer, plus both `sub esp,N`/`add esp,N` constants, read exactly N+4 in
  the original — a long, uniform, same-delta run in `cc.sh -v`'s byte list with no instruction
  ever changing. Fixing it took `TileKind_GetOrLoadDescriptorNoAlias` (0x4470b0) from DIFF(6) to
  byte-EXACT and took 6 bytes off `SoundBank_PreloadWavRange`'s residual. **Ghidra tells you the
  real width for free** — its `CHAR local_108 [264]` is derived from the frame layout, so read the
  decompile's array bound rather than assuming the API argument is `sizeof`. Generalization: a
  uniform ±k on every displacement is a FRAME-SIZE fact (a mis-sized or missing local), not a
  register-allocation residual; do not start hunting codegen classes until the frame matches.
- **⭐ A jump-table switch: block IDs are numbered by ascending case VALUE, but the case BODIES
  are laid out in SOURCE order (v357, 0x43f140).** These are two independent facts and confusing
  them wastes a session. (a) The byte-index/jump-table pair is invariant under source reordering,
  and each `case` LABEL gets its own jump-table slot even inside a grouped `case A: case B:` pair
  — so a table with six slots pointing at four distinct addresses is the NORMAL output of four
  bodies with two grouped pairs, NOT evidence of six source bodies. (b) The emitted body order is
  the source declaration order. So when the original's bodies sit in a different order than yours,
  **reorder the `case` labels in the source and the tables do not move**: doing that at 0x43f140
  was worth 345831 → 177833 in one compile. ⚠ Do NOT "fix" (a) by splitting a grouped label pair
  into two identical bodies — VC5 5.0 does not tail-merge them, and it cost 11 instructions
  (364050) when tried.
- **⭐ VC5 CSEs a repeated field LOAD but does NOT propagate the null-ness an enclosing branch
  already proved (v357, 0x43f140).** The original re-tests a pointer inside a `switch` case even
  though the enclosing `if (p != 0)` guarantees it — while reusing the register the first load
  produced. That is not a missed optimization to paper over: it means the source re-reads the
  FIELD (`pNode->pPayload`) in the case body instead of the outer local, and writing the local
  elides the guard. Symptom: the original has an extra `cmp reg,0 / je` your version lacks, with
  no extra load beside it.
- **⭐ Walk a linked list by advancing the cursor at the TOP of the loop (v357).** `pCur = pList;
  pList = pList->pNext; ... use/free pCur;` compiles to the original's `mov cur,p / mov p,[p]`
  pair; the trailing form (`pNext = p->pNext; ... p = pNext;`) needs an extra register copy at the
  loop bottom. One instruction, but it also shifts the loop-back branch displacement.
- **⭐ A "dead" accumulator plus a degenerate range loop = an INLINE HELPER instantiated with equal
  arguments (v357, the TileKind loaders).** 0x446ea0/0x4470b0 both emit a first..last loop — clamp,
  `<=` guard, and a counter that is accumulated and then never read — over a range that can only
  ever hold one element. None of that is dead code to be dropped: it is an inline
  `LoadDescriptorRange(first, last)` called as `(id, id)`, whose returned count the caller
  discards. Writing it that way is byte-identical to writing the loop flat, and the SAME helper
  then takes the twin function to EXACT. Whenever a function contains machinery that is provably
  redundant for its own arguments, suspect an inline call before you suspect the decompiler.
- **⭐ An unconditional call cannot live inside an `&&` chain (v357, 0x447330).** The original
  calls LoadStringA, THEN tests two conditions (`n = f(); if (a && n == 0)`); writing
  `if (f() == 0 && a)` makes the call the first operand of a short-circuit and restructures the
  whole tail — DIFF(8) → DIFF(62). Read the disasm for whether the call site is dominated by the
  other test before choosing the `&&` order; if the call always executes, it needs its own local.
- **⚠ A range test on an `unsigned` parameter that emits `jl`/`jg` (not `jb`/`ja`) means the
  SOURCE CASTS TO INT (v357, 0x447330).** Signedness of a comparison is a type fact like `sar`
  vs `shr` — `(int)stringId >= 100 && (int)stringId <= 500` on a `UINT` parameter, reproduced
  verbatim rather than tidied.
- **Original engine bugs are reproduced, not fixed** — document in `docs/engine-bugs.md`,
  mark reproduction sites `// sic:`.
- **⭐⭐ SIB base/index role: `arr[i]` and `(char *)arr + i` are DISTINGUISHABLE even when they
  compile to the identical instruction stream (v359).** For an address that needs two registers,
  `[ptr + idx + disp]` can be encoded either way round (SIB base=ptr/index=idx or the reverse);
  both are the same address, one byte apart. VC5 SP3 picks the role from the SOURCE EXPRESSION'S
  SHAPE, not from the register contents: **an ARRAY ROW SUBSCRIPT puts the array pointer in the
  BASE slot; explicit flat byte arithmetic puts the OFFSET in the base slot.** This holds even
  though MSVC strength-reduces the row subscript down to exactly the same stride-N induction
  variable, so the two forms are otherwise instruction-for-instruction identical — the encoding
  bit is the ONLY observable difference. `EditCardWnd::BuildPlayerRosterList` (0x416e00, 366 B)
  sat at DIFF(2) for ~40 sessions with an autopsy declaring it an intrinsic tie-break; rewriting
  `for (i += 0xd) { p = (char *)pCache->aEasterNames + i; … }` as
  `for (j++) { p = pCache->aEasterNames[j]; … }` took it to EXACT with no other change.
  - **Diagnostic:** an `r` row in `asmscore --dump` whose two sides are the same mnemonic with
    the same two registers in swapped order inside `[...]`, with `align`/`insns` both clean.
  - **It is a real source fact, so it cuts both ways** — do NOT sweep the whole repo converting
    flat walks to subscripts. `PostBag_LoadEasterNameCache` (0x443260) walks the SAME
    `aEasterNames[16][13]` array, and there the subscript form costs +90 diffs and 12 bytes: the
    original wrote its two loops over that array in two different styles. Test per call site.
  - **Refinement, for the `this`-relative case:** when one operand is `this` + a constant member
    offset (so the "pointer" never exists as its own register), the slot goes to whichever
    operand is the result of an ARITHMETIC op. `arr[i]` and `arr[pOther->field]` (subscript is a
    plain variable / a bare memory load) keep base=this; any computed subscript takes the base
    slot itself. This is why `EditCardWnd::SelectDecalSlot` (0x41a360) is still 1 byte off — the
    original masks its subscript (`and edx,0xff`) and STILL gets base=this, and none of ~12
    probed spellings of that mask reproduce it. See the in-source autopsy for the refuted list.
- **⭐⭐ Walk a list tail with ONE cursor, not two (v359, 0x4393d0 — took it EXACT).** A
  two-cursor tail walk (`pCur = pTail->pNext;` … `while (pCur) { pTail = pCur; count++;
  pCur = pTail->pNext; }`) and a one-cursor walk (`while (pTail->pNext) { pTail = pTail->pNext;
  count++; }`) are semantically identical, but only the one-cursor form matches. With two
  cursors MSVC can see `pTail == pCur` and reads `->pNext` through the register the value is
  ALREADY in, so the load has no dependency on the just-written cursor and there is nothing to
  schedule into its shadow. With one cursor the load must go through the freshly written
  register — which both pins its base register and lifts the unrelated `count++` up into the
  stall slot ahead of it.
  - **Diagnostic:** a 2–3 byte residual where the original's loop body has an independent
    increment/scheduling filler BEFORE the `mov reg,[reg+N]` link load, and the load's base
    register differs from ours. Two symptoms, one cause — do not treat them separately.
  - This is why the obvious probes all fail: reordering `count++`, a `for`-loop form, and
    `pCur = pCur->pNext` vs `pTail->pNext` were all probed at 0x4393d0 with zero effect, because
    every one of them KEEPS TWO CURSORS. Change the cursor count, not the statement order.
  - Distinct from the v357 "advance the cursor at the TOP of the loop" bullet above: that one is
    about *where* a single cursor is advanced in a consume-and-free loop; this one is about *how
    many* cursors exist. A walk that genuinely needs two (unlinking, which needs a `pPrev`) is
    unaffected.
- **⭐⭐⭐ `dec eax; jne` on a value compared to 1 is a SINGLE-`case` `switch`, not `if (x == 1)`
  (v366, `ScreenSaver::InitAndPlayIntroMusic`/0x4480c0 — took it EXACT in one probe).** VC5's
  switch lowering NORMALISES the switch value by subtracting the lowest case label before it
  tests the range, so a switch with exactly one `case 1:` emits `mov eax,[x]; dec eax; jne
  <past the body>` — 1 byte for the compare. Plain `if (x == 1)` emits `cmp eax,1` (3 bytes) and
  nothing else differs, so the whole residual reads as a benign instruction-selection tie-break
  when it is actually a source-shape difference you can flip. Ghidra's own decompile prints the
  tell verbatim (`iVar1 = x + -1; if (iVar1 == 0)`, plus a `CONCAT31(iVar1 >> 8, …)` return that
  shows the subtraction result sitting dead in EAX). The same `mov eax,ds:0x4a9918; dec eax; jne`
  shape appears at 0x461923 in the app window procedure, so this spelling is used more than once
  in this codebase. **Generalises: a 2-or-3-case sparse `switch` whose lowest label is non-zero
  is spelled by a leading `sub`/`dec`, and successive cases by CUMULATIVE `sub`s** —
  `and eax,0xfff0; sub eax,0xf060; je A; sub eax,0xd0; je B; sub eax,0x10` is
  `switch (x & 0xfff0) { case 0xf060: … case 0xf130: … case 0xf140: … }` (0x4484a0's inner
  WM_SYSCOMMAND switch), NOT an if/else-if chain — a chain would emit independent
  `cmp eax,imm32`s. A trailing `sub` with NO `je` after it means that case's body is the same
  block as `default`'s, i.e. they share a label list (`case SC_SCREENSAVE: default:`).
- **⭐⭐ A switch emits a DIRECT `jmp [reg*4+table]` only if `default:` shares its label list with
  a real case group; a SEPARATE `default:` block holding identical code is NOT tail-merged into
  it, and the extra block forces a two-level BYTE-INDEX table (v366,
  `ScreenSaver::Tick`/0x448120 — took it EXACT).** Writing
  `case 3: case 4: case 5: case 6: break; default: <same code> break;` produced
  `xor ecx,ecx; mov cl,byte ptr [edx]; jmp dword ptr [ecx*4]` plus a byte-index table, where
  `case 3: case 4: case 5: case 6: default:` on ONE label list produced the original's direct
  7-entry dword table. Diagnostic: the two-level `mov cl,[table]` indirection in your output
  against a plain `jmp [reg*4+imm]` in the original means you have one arm too many, not a
  density/threshold problem.
- **⭐⭐ An inlined byte-bool predicate must spell its true return as an explicit `return 1;`
  inside an `if`, not `return (expr) == 1;` (v366, 0x4484a0).** `return (pfn(1) & 1) == 1;`
  makes VC5 materialise the comparison the long way (`and al,1; cmp al,1; sete al`, or at /O2
  without the `sete` peephole `dec al; neg al; sbb eax,eax; inc eax`). The branch form
  `if ((pfn(1) & 1) == 1) { return 1; } return 0;` lets VC5 notice that AL ALREADY HOLDS 1 on
  the taken edge of `and eax,ebx; cmp al,bl; je` and emit **no instruction at all** for
  `return 1` — the true edge jumps straight past the `xor al,al` that materialises the false
  value, so only the false edge pays for the value. Same family as v356's byte-predicate lever
  (the `unsigned char` return type is still load-bearing); this is the *body* half of it.
- **⭐⭐ A pointer-walk loop gets a DOWN-COUNTER only if the index is unused in the body (v366,
  `ScreenSaver::Tick`/0x448120).** `for (i = 0; i < 3; i++) { … arr[i] … }` keeps the byte
  OFFSET as the induction variable and compares it against the end offset
  (`cmp esi,0x14` for `aSlots` at +8 × 3 entries) — the array subscript keeps `i` live. Hoisting
  the cursor (`pp = arr; for (i = 0; i < 3; i++) { … *pp … pp++; }`) makes `i` a pure trip
  counter, and VC5 then reverses it into `mov edi,3` / `dec edi; jne` with the pointer walking
  separately. Diagnostic: an original with BOTH a dedicated down-counter register and a pointer
  register, against your single offset-compared register, means the body must stop naming the
  index. Related to the two-cursor list-walk bullet above, but the opposite direction: here you
  ADD the cursor.
- **⭐ Two switch arms that make the same call with different arguments are TWO CALL STATEMENTS in
  the source, tail-merged by the compiler — not one call below the switch (v366, 0x448120).**
  The tell is that each arm loads the argument into a DIFFERENT register (`mov dx,[ecx+0x24];
  push edx` in one arm, `mov ax,[ecx+0x26]; push eax` in the other) before a single shared
  `call`. Writing the switch to assign a temp and calling once afterwards forces one register for
  both arms and never matches; writing `obj->Method(obj->fieldA);` / `obj->Method(obj->fieldB);`
  in the two arms reproduces the per-arm register choice AND the merged call.
- **⭐⭐ A jump-TABLE switch emits its case bodies in SOURCE order; a compare-TREE switch does
  not (v368, `Main::AppWndProc` 0x4618c0).** When VC5 lowers a switch to `jmp [reg*4+table]`
  nothing constrains body placement, so the order the bodies appear in `.text` IS the order the
  cases appeared in the source — read it off the jump table's targets sorted by address and
  write the cases in that order. At 0x4618c0 the WM_USER+1 `switch (wParam)` turned out to start
  at **case 8**, not case 0; reordering to match took the function from total 2135102 to
  1975015 in one compile. The converse is equally load-bearing: for a compare-tree switch the
  body order is fixed by the tree (the last case in each compare chain becomes the
  fall-through), so source order there is **byte-neutral** — swapping WM_CHAR/WM_KEYDOWN in the
  same function changed nothing at all. Don't spend probes on case order for a tree; do read it
  off the table for a jump table.
- **A compare-CHAIN dispatch whose case bodies appear in REVERSE order in `.text` is a
  `switch`, not an `if`/`else if` chain (v499, `EditCardWnd::OnTimerDefaultMaybe`/0x41a8a0 —
  worth total 325678 → 34678 in one compile).** A 2-3-case switch lowered to
  `cmp;je / cmp;je / cmp;jne` lays the LAST case's body down as the fall-through (first in
  memory) and outlines the earlier cases after it in reverse, while the semantically
  identical if/else-if chain lays every body inline in source order. The tell is a dispatch
  whose tests all jump FORWARD over bodies that run in the opposite order from the tests.
  Companion to the v368 tree bullet below (which covers why source case order is byte-neutral
  WITHIN the chain): this one is the lever that picks which STATEMENT the original used.
- **On a switch-heavy function, derive `--len` from the NEXT function's entry point, not from
  Ghidra's `Body` span (v368 re-confirmation, and the cheapest way to be wrong by a lot).**
  0x4618c0's `Body` ends at 0x462df6 but its COMDAT runs to 0x462e90 (LocoWinMain) — the 0x98
  bytes in between are its five jump tables. Scoring with the `Body` length hides the tables and
  under-reports the window; scoring with `--len 0x15d0` gives the honest `insns 1739/1740`.
  Corollary: `insns` within ±1 on a 5 KB function means the transcription is content-complete
  and every remaining point of the score is allocation/layout, so go straight to the autopsy
  instead of hunting for missing logic.
- **The two residual classes that dominate a large WndProc-shaped function are REGISTER
  RESIDENCY and CROSS-JUMPING, and neither is source-steerable (v368, 0x4618c0, PARKED).**
  (a) Residency: the original dedicates a callee-saved register to a repeatedly-read GLOBAL
  (`ebx = g_pApp`, reloaded after each call that could write it) and re-reads a PARAMETER from
  its stack home at every use; ours does the exact opposite. Every `mov eax,[esp+<param>]` that
  shows up as original-only in the dump is this one choice, and it accounted for ~74 of 112
  original-only rows. Reversing the operand order of the first comparison so the global is
  evaluated first — the obvious probe, since the original's own instruction order loads the
  global first — is a 1-byte REGRESSION and changes no allocation. (b) Cross-jumping: VC5 SP3
  tail-merges identical `...; return 0;` blocks more aggressively than our build does, and the
  un-merged copies show up as a very characteristic pile of extra `pop`/`add esp`/`ret` rows
  (20/26/5 here). A related tell: at a block the original DID cross-jump, it pushes the switch
  case value as a LITERAL, while at un-merged sites it pushes the live register — so our
  constant-folding `push 0x115` where the original has `push edi` is the same axis, not a
  separate source question.
- **New /Og residual class: switch CLUSTER ABSORPTION (v366, parked `ScreenSaver::FilterMessage`
  0x4484a0).** VC5 can swallow an isolated sparse case into a neighbouring dense cluster and
  table the whole widened range with a two-level byte-index table. At 0x4484a0 the original
  builds a decision tree over {0x1c},{0x20},{0x100},{0x104-0x106},{0x112} plus ONE 8-entry dword
  table for 0x200..0x207 (32 bytes); ours absorbs 0x112 and tables 0x112..0x207 as a 24-byte
  dword table plus **246 bytes of byte indices**, and the top-level tree pivot moves with it
  (0x20 vs 0x1c). **Judge such a function by CODE length, never the headline DIFF/len** — ours is
  792 code bytes to the original's 814, i.e. 22 bytes SHORTER, and the entire 214-byte "residual"
  is table. Refuted axes (do not retry): case-group source order (the case-body EMISSION order
  already matches — see the v327 source-case-order bullet); an explicit `default: return 0;`
  inside the switch vs a trailing `return 0` after it (no-op here, unlike the label-list lever
  above, which is about an EXTRA arm); moving the absorbed case last (worse); densifying the
  target cluster with extra no-op cases in its gaps (worse — the byte table just grows).
- **⭐⭐⭐ A call/jmp-target sweep CANNOT find every function — the ones Ghidra misses are exactly
  the ones reached only through a DATA pointer (v367, `tools/find_unanalyzed.py`).** Sweeping
  every direct `call`/`jmp`/jump-table target reachable from every known function body over the
  whole of `Loco.exe` returned **zero** orphans, while a second sweep — enumerate the `.text`
  ranges covered by NO Ghidra function body, then create a function at every 16-byte-aligned
  non-padding run start — found **80 real functions, ~22 KB**, including the two largest in the
  app region: `Main::AppWndProc` (0x4618c0, 5351 B, referenced only by the WNDCLASS
  store at 0x406ef4) and 0x4028b0 (4833 B, referenced only from a vtable slot at 0x47740c). The
  data-referenced classes to expect: WNDPROCs, vtable slots, thread procs handed to
  `ThreadWrapper::Start`, and CRT `atexit` init/dtor thunk pairs. **Re-run the sweep whenever a
  function's caller cannot be found** — "no xrefs" is far more often an unanalyzed data-referenced
  caller than a genuinely dead function. Two traps in writing such a sweep: (1) `0x00` is NOT a
  padding byte for run-extent purposes (zero bytes are everywhere inside immediates — treating
  them as padding truncates every run at its first zero and hides the whole init-thunk table),
  only `0x90`/`0xcc` are; (2) a VC5 two-level switch table starts with real dword entries and
  continues with BYTE indices, so "is the LEADING dword a `.text` address" is the load-bearing
  jump-table test, not the fraction of dwords that are.
- **⚠ `toolchain/test/app_funcs.txt` is a hand-refreshed SNAPSHOT, and a stale one silently
  inflates every percentage `progress.py` prints (v367).** It had drifted to a 2026-07-18 state
  missing 150 app-region functions / 43546 bytes — including functions transcribed many sessions
  earlier (e.g. 0x4484a0). Regenerating it from the live Ghidra DB moved the headline from an
  inflated "54.93% transcribed" to a true **48.75%**, with no source change whatsoever, and made
  the "largest unclaimed" list useful again. Regenerate it after any sweep that creates functions,
  and treat a jump in the percentage that no commit explains as a denominator bug, not progress.
- **⭐⭐⭐ SOURCE ORDER AND STORE ORDER OF TWO INDEPENDENT FIELD ASSIGNMENTS ARE NOT THE SAME
  THING — read the disasm's COMPUTE order, not its STORE order (v370, `AlbumCardWnd::
  RefreshClientClipRect` 0x4028b0, and it was worth 1042136 → 0 on a 4833-byte function).**
  The original stores `rc.bottom` first and `rc.right` second in all 20 card-slot/label blocks,
  so the obvious transcription is `rc.bottom = rc.top + 200; rc.right = rc.left + 300;`. That is
  WRONG: the correct source is the reverse. VC5's scheduler emits the `lea` that computes
  `right` (a 3-byte-displacement `lea eax,[ecx+0x12c]`) AHEAD of the 1-cycle `add` that
  computes `bottom`, then commits both stores in the OTHER order — so with the intuitive
  source the compiler produces the same instruction MULTISET but interleaved
  compute/store/compute/store instead of compute/compute/store/store, and the whole downstream
  register assignment rotates. Symptom: perfect `insns N/N` parity, balanced `-`/`+` row counts
  made up entirely of `mov`/`add`/`lea`, hundreds of `r` (register-swap) rows, and a compiled
  length a handful of bytes SHORT of the original. **When a residual looks like pure
  register-allocation noise but the instruction count is exactly right, try swapping adjacent
  independent assignments before declaring it intrinsic** — this class is cheap to probe (one
  compile per swap) and, unlike a real allocator tie-break, it moves the score enormously.
  Corollary: the same swap must be applied to EVERY structurally-identical block in the
  function; getting 10 of 20 right leaves a score that still reads as intrinsic noise.
- **A whole-struct member assignment (`p->rect = rc;`) and four separate member stores
  (`p->rect.left = rc.left; ...`) are DIFFERENT codegen, and Ghidra's decompile routinely folds
  the former into the latter (v370).** VC5 lowers `p->rect = rc` as "materialize `&p->rect`
  once (`mov reg,[this+off]; add reg,4`), then four moves off it", and can then reuse that
  adjusted pointer to reach the object's OTHER fields. Four separate member stores instead
  force a RELOAD of `p` before each one — the stores could alias `this`. Tell: the original
  loads the member pointer once where your transcription loads it 3-4 times. Ghidra makes this
  hard to see because it attributes the intermediate `[esp+N]` writes (which are really the
  local `rc`'s own `right`/`bottom` slots) to the destination struct's fields, presenting a
  whole-rect copy as per-field assignment. Cross-check against raw disasm: if the four stores
  come off ONE base register with contiguous `+0/+4/+8/+0xc` displacements, it is a struct
  assignment.
- **Caching a pointer in a local is only a win when the value is re-read AFTER an intervening
  store (v370).** In the same function, hoisting `pRef = pPageIndicatorIcon;` was required
  (its rect is written and then `rc.bottom` re-read), while hoisting the equivalent for the
  seven button-icon blocks and twenty grid blocks made things WORSE (−14 instructions vs. the
  original) — there the compiler already CSEs the load and an explicit local just changes
  where it schedules it. Probe the hoist per block shape, not per function.
- **A guard can wrap the base-class call too — check where the `jcc` sits relative to the
  `call`, not what the class hierarchy makes sensible (v370).** `AlbumCardWnd::
  RefreshClientClipRect` tests its `bWantEraseBlit` byte flag BEFORE chaining
  `WindowBase::RefreshClientClipRect()`, so an unrealized window skips the base refresh
  entirely — the opposite of its sibling override `EditCardWnd::RefreshClientClipRect`, which
  always chains first. Hoisting the base call out of the guard "because that is what the base
  contract obviously requires" is a measurable regression (1042136 vs 1182910). Read the two
  instructions before the `call`.
- **⭐ An ADDRESS-TAKEN local struct cannot be enregistered, and its every field read/write is
  a real memory access — this masquerades perfectly as "the original spills far more than we
  do under register pressure" (v371).** `EditCardWnd::RefreshClientClipRect` (0x417180) was
  parked across several sessions on exactly that misreading: the original round-trips values
  through `[esp+0x18..0x24]` in essentially every one of its ~25 button blocks, which reads as
  an allocator under strain. It is not. There is ONE `RECT rect` local, its address is taken
  once (`CenterRectInRect(pRVar1, &rect)`), and from that point VC5 must keep it in memory;
  each block computes into `rect` and ends with `pBtn->rect = rect;`. Restoring that one source
  shape (plus the three below) took the function from `insns 733/938` / total 2885608 to
  `insns 940/938` / total 391073 — an 86% cut with zero allocator work. **Diagnostic: a
  CONSECUTIVE 16-byte window of `[esp+N]` slots, written field-by-field and then read back as a
  group into one destination pointer, is a local struct, not spill slots.** Spill slots are
  scattered and reload the SAME value; a local struct's slots hold four DIFFERENT fields at
  fixed relative offsets. Ghidra will never show you this — it folds the trailing whole-struct
  assign into per-field stores on the destination (see the v370 bullet above) and shows the
  local's slots as if they were the destination's fields.
- **The corollary for parked residuals: "the original spills more than we do" is not a
  conclusion, it is a symptom with two very different causes (v371).** Real register pressure
  reloads one value repeatedly at unrelated offsets. An address-taken local produces a tidy,
  contiguous, field-shaped slot group. Before parking anything as an intrinsic pressure
  tie-break, check whether the "spill" slots are contiguous and whether any local has its
  address taken anywhere in the function — including as an argument to a helper you have
  already transcribed and stopped looking at.
- **⭐ The MIRROR form, and it is the more common one: SPLITTING the original's ONE
  address-taken local into two locals in your source (v372).** `EditCardWnd::OnLButtonDown`
  (0x41ac10) was 192 instructions short of the original with balanced call parity — no missing
  branch bodies, just 150 missing `mov`s. Cause: the original homes the click `POINT` at
  `[esp+0x10]`/`[esp+0x14]` and reloads it **119 times** across the function, because exactly
  ONE site — a `ClientToScreen(hwndSelf, &pt)` / `SetCursorPos(pt.x, pt.y)` pair ~600 source
  lines away from where `pt` is initialized — takes its address. Our source had used a separate
  `POINT ptScreen` there, so `pt` was never address-taken and VC5 enregistered it, dropping all
  that reload traffic. **Reusing the one local: `insns 2259/2451` → `2351/2451`, total 6576410 →
  6164892. One line, 92 instructions.** Ghidra names every by-value `POINT` copy it synthesizes
  for `PtInRect` separately (`pt`, `pt_00` … `pt_27`) but keeps the REAL local under its own
  name (`local_d8`) — so when Ghidra shows `&local_d8` at a far-away call site, that is the same
  local as the one initialized at the top, not a new one. Reuse it. Before splitting a local
  "for readability", check whether the original's slot is one home or two.
- **Verify a reused-local rewrite is behavior-preserving by checking control flow, not
  proximity (v372).** `ClientToScreen` MUTATES `pt` in place, and a later
  `PlaySoundAtScreenPos(0x5273, pt.x, pt.y, 4)` sits only 15 source lines below — but the
  mutating block ends in a `goto`, so the two uses are on sibling paths and the sound site never
  sees screen coordinates. That makes it a faithful transcription, NOT an engine bug. A
  post-mutation read that is merely *textually* below is not evidence of anything.
- **⚠ Do NOT count `lea reg,[esp+K]` sites to find address-taken locals — `K` shifts with push
  depth, so ONE address appears under several different `K` (v372).** In
  `GameNet_HandleFileTransferBlock` (0x43a140) `lea ecx,[esp+0x2c]` and `lea edx,[esp+0x18]`
  are the same `szPath` buffer, 0x2c appearing only because five `push`es had already moved
  `esp` by 0x14. A raw orig-vs-candidate `lea` count therefore reports phantom "missing locals"
  (it flagged 22 functions repo-wide; nearly all were this artifact or a pure position shift).
  **Use stack-slot READ PRESSURE instead** — count reads of each `[esp+K]`/`[ebp-K]` operand on
  both sides and compare the hottest slots. It has no push-depth ambiguity, and it is what
  actually pinned 0x41ac10 (original `[esp+0x14]`×60, `[esp+0x10]`×59; ours had no comparable
  hot slot — after the fix, 61 and 60). **Two caveats before reading a hot slot as a missing
  local:** (1) absolute `K` is only comparable across the two sides when the FRAME SIZES agree —
  otherwise compare the shape of the distribution, not slot numbers; (2) a hot slot that is only
  ever READ and never written, with no `lea` anywhere, is an incoming stack PARAMETER, not a
  local. Both traps fired on the main WndProc (0x4618c0), whose `[esp+0x36c]`/`[esp+0x374]`
  (×13/×18, absent from our candidate) are just `hwnd` and `wParam` reloaded per use — with
  `insns 1739/1740` it is content-complete and not this bug class at all.
- **A large instruction deficit with BALANCED call parity means missing memory traffic, not
  missing code (v372).** Diff the mnemonic MIX, not just the total: 0x41ac10 was `mov` 1021 vs
  871 while `call` was 248 vs 244 and `push` 575 vs 545. Calls are the reliable content oracle —
  if they match, every branch body is present and the gap is allocation/homing. ⚠ When tallying
  call parity from source, grep for YOUR OWN names, not Ghidra's: two "missing" calls on
  0x41ac10 were `CarNetState_HitTestDecalMaybe`/`CarNetState_AddDecalMaybe` in Ghidra but
  already transcribed as `RemoveDecalAtPoint`/`AddDecal`.
- **`SetRect(p, l, t, r, b)` in your source vs four inline field stores in the original is not
  a wash — it can cost a callee-saved register function-wide (v371).** With two `SetRect` calls
  present, VC5 hoisted the import thunk's address into `ebp` and called indirectly (`mov
  ebp,[__imp_SetRect]` / `call ebp`), removing `ebp` from allocation for the whole function and
  perturbing register choices in every block after it. The original simply stores the four
  fields. Tally the original's `call` sites FIRST (`grep -c call` over the raw disasm) and make
  your source's call count match exactly before reading any register-level diff — a spurious
  call is a whole-function perturbation, not a local one.
- **Ghidra's `unsigned short` temporaries for 16-bit struct fields are decompiler artifacts,
  and keeping them changes codegen (v371).** VC5 zero-extends AT THE LOAD when the value feeds
  an int expression directly (`xor ebx,ebx; mov bx,WORD PTR [edi+0x16]` — the Pentium-friendly
  pair, not `movzx`). Declaring the Ghidra-suggested `unsigned short uVar3 = p->nativeWidth;`
  instead keeps a 16-bit value live in a partial register and masks it at each use (`and
  ebx,0xffff`). Widening such temporaries to `int` — or inlining them into the expression — is
  semantically identical (they already integer-promote at every use) and is worth ~1 instruction
  per site, which across 25 near-identical blocks is a large, entirely source-steerable delta.
- **Pinning a comparison's literal from the `jcc` mnemonic can still get the POLARITY backwards
  (v371).** An earlier session correctly read `cmp ecx,0x320; jle` and concluded the source
  must say `width <= 0x320` (right constant, right operator — per Yoda lesson #2). But the
  `jle` jumps TO the plain arm, i.e. it SKIPS the subtracting arm, so the source is
  `if (width > 0x320) { left -= 0x32; } else { left = ...; }`. The constant and the comparison
  operator are recoverable from the `cmp`; which arm is the `then` is only recoverable from
  where the branch LANDS relative to the two bodies. Check the jump target, not just the
  mnemonic.

- **⭐⭐ CALL PARITY DISCRIMINATES SOURCE-LEVEL DUPLICATION FROM COMPILER CROSS-JUMPING — use it
  before you "simplify" a duplicated tail away (v374, `SplashWnd::OnLButtonDown` 0x422930).**
  MSVC cross-jumps (tail-merges) the longest common SUFFIX of two branches, so a source that
  duplicates a whole tail can compile to something that *looks* shared. The direction only goes
  one way: merging can REMOVE a duplicate call, nothing ever ADDS one. So if the original binary
  shows a callee **twice**, the source really does have two call sites — no amount of "the
  compiler duplicated it" explains it away. On 0x422930 the play-alone arm and the fall-through
  arm both end in `RedrawSettingRectsMaybe(); CommitScreenUpdate(...); RequestModeTransition...;
  PlayUiSound(...); return 0;`, and the original's tally is `0x422010` ×2 but `CommitScreenUpdate`
  ×3 (not ×4) — i.e. the source duplicates all four statements and MSVC merged from
  `CommitScreenUpdate` onward. A single shared tail scored *better* (138084 vs 162759) yet is the
  WRONG source shape: it merges more than the original ever could and contradicts the ×2 tally.
  **Trust the call tally over the score when the two disagree** — the score is a proxy, the tally
  is evidence about what the programmer wrote.
- **The merge POINT itself is a register coin-flip, not a source lever (same function).** MSVC
  stops cross-jumping at the first instruction whose bytes differ, so which suffix survives is
  decided by register allocation one level up. On 0x422930 the original loads
  `pPointCursorDesc`/`pPointCursorRect`/vtable into eax/ecx/edx in BOTH copies, making the blocks
  byte-identical so they merge; ours picks eax/ecx/edx in one copy and ecx/edx/eax in the other,
  blocking the merge and leaving an extra `CommitScreenUpdate` +
  `RequestModeTransitionFromSource` pair. Diagnose it by comparing the two copies' operand
  registers, and park it — there is no source construct that names a register.
- **⚠ VC5 can keep DEAD stores to a by-value struct local that we optimize away, and this is NOT
  the address-taken-local class (v374, 0x422930).** The original writes a `RECT` local's four
  fields to `[esp+0x20..0x2c]` in both button arms and never reads them back — the by-value
  argument copy is built separately from the same registers — while our compile forwards the
  registers straight into the argument and drops the local entirely (4 instructions per arm).
  Before reaching for v371/v372's address-taken fix, check for a `lea` of the slot: if there is
  none anywhere in the function, the local is not address-taken and hoisting it to function scope
  will do **nothing** (measured: 162759 → 162759, byte-for-byte identical).
- **⚠ When deciding whether a stack slot is live, normalize for PUSH DEPTH before matching
  offsets (v374, 0x422930 — nearly a wrong "the local is read" conclusion).** The same absolute
  `[esp+0x2c]` names two different variables at two points in one function: at entry depth it is
  the `artRect` local, but inside an argument build (`push` + `sub esp,0x10` = 0x14 deeper) it is
  the CopyRect local that lives at `[esp+0x10]` in the unshifted frame. This is the read-side
  twin of v373's "do not count `lea reg,[esp+K]` sites" trap, and it fires just as easily.
- **⭐⭐ A vtable slot's INDEX is `(pointer address − vtable base)`, and Ghidra's xref gives you
  the pointer address, not the base — deriving the base wrong renames the function after the
  wrong message (v374).** Ghidra had 0x422930 as `SplashWnd::OnLButtonUp`; its pointer lives at
  0x477a30, and taking that as slot 0 (or guessing a base) puts it anywhere. Recover the base by
  finding INHERITED DEFAULTS in the dump and solving for the offset that lines them up with the
  base class's known slot table: `WindowBase`'s `OnMouseActivate` (0x426950) at slot 0x5c,
  `OnSize` (0x426960) at 0x68, `OnPaint` 0x6c, `OnSetCursor` 0x70, `OnEraseBkgnd` 0x78,
  `OnDestroy` 0x7c all agreed on base = 0x4779f8, making 0x422930 slot **0x38 =
  WM_LBUTTONDOWN**, not 0x3c. Cross-check with any slot a PRIOR session named independently
  (`OnKeyDown` sat at 0x54, exactly where the table says WM_KEYDOWN is). Six-plus agreeing
  defaults is proof; one guessed base is not.
- **⭐⭐⭐ BLOCK-SCOPING repeated locals lets VC5 REUSE — and SWAP — their stack slots between
  iterations of a copy-pasted block; declaring them ONCE at function scope pins the assignment
  (v375, `SplashWnd::BuildDrawTargetCompositeMaybe` 0x4216f0 — took a 5-session-old parked
  residual EXACT).** The function composites 5 icons through 5 near-identical blocks. Written as
  `{ CursorDesc *pDesc; LocoBitmap *pBmp; RECT srcRect; RECT destRect; ... }` per icon, block 1
  got `srcRect` at `[esp+0x14]` / `destRect` at `[esp+0x24]` and blocks 2–5 got them **swapped** —
  48 bytes of pure `[esp+N]` displacement noise at `align=0 reg_pen=0`, which is exactly what a
  finished-looking near-miss looks like. Hoisting all four declarations above the first block
  made it byte-exact. **This is the mirror of the v371/v372 address-taken-local lever and of the
  "SPLIT one local into several" lever — all three are the same question (how many distinct stack
  slots does the source ask for?), and the answer differs per function, so try BOTH directions.**
  Diagnostic: if `--dump` prints no differing instruction rows at all yet `byte_diff` is nonzero,
  the disagreement is entirely in displacement/immediate bytes — dump the masked COMDAT against
  the masked original and group the differing offsets into runs; a run of `orig 0x20 / mine 0x30`,
  `orig 0x14 / mine 0x24`, `orig 0x24 / mine 0x14` triples IS this pattern.
- **⚠ A "tried, confirmed NO EFFECT" note is only valid against the header shape it was measured
  under (v375, same function).** v306 recorded that block-scoping this function's locals changed
  nothing (148624 → 148631) and concluded the residual was an intrinsic allocator tie-break. That
  measurement was honest and the conclusion was wrong: with the OLD padded `SplashWndPartial`
  view still in play, register allocation was so far off that the stack-slot question could not
  show through. v374's promotion of `SplashWnd` to a real `WindowBase`-derived class dropped the
  score 148631 → 48 on its own and the scoping lever then closed the rest. **When a class's
  header shape changes, re-run the parked probes for every function in that TU** — and treat
  every pre-refactor "no effect" line in `docs/PARKED.md` as expired, the same way v358's
  toolchain switch expired the pre-v358 numbers.
- **⭐⭐ A two-case `switch` with a `break` falling through to a shared tail lays blocks out with
  the TAIL INLINE and both case bodies sunk out-of-line; the equivalent `if (…) {…return;}`
  chain keeps each body inline instead (v375, `SplashChildSubclassProc` 0x420b20 — took it
  EXACT, 89 bytes).** Symptom: the original's first compare is `je <far forward>` (jump TO the
  special case) while yours is `jne <near>` (jump PAST it), and your special-case bodies appear
  in source order right after the dispatch while the original's appear after the default path.
  Both forms are perfectly idiomatic WndProc C, so pick by the branch senses, not by taste.
- **⭐ `switch (msg & 0xffff)` on an `int` emits signed `jg` in the binary-search dispatch; on a
  `UINT` it emits `ja` (v375, `SplashVideoSubclassProc` 0x4207c0 — a 2-byte miss, `jg` vs `ja`
  at both tree nodes).** The mask makes the two provably equivalent, so nothing but the declared
  type of the switch variable distinguishes them. Same family as the existing "`jl`/`jg` on an
  unsigned parameter means a signed local" bullet — read the branch mnemonic, then declare the
  local to match.
- **⭐⭐ BRANCH POLARITY of an `if/else` over a boolean flag is a first-class source lever, worth
  hundreds of thousands of points of block-layout churn on a repetitive function (v375,
  `SplashWnd::RedrawSettingRectsMaybe` 0x422010 — 448685 → 204010 from two one-line flips).**
  `if (flag != 0) {A} else {B}` emits `test; je Belse` and lays A down as the fallthrough;
  `if (flag == 0) {B} else {A}` does the reverse. Read which arm is the fallthrough straight off
  the first `jcc`'s target and match it. ⚠ Do NOT assume a function is internally consistent:
  this one tests the SAME flag twice and uses the opposite polarity each time (`!= 0`-first for
  the play-alone/connect-online pair, `== 0`-first for the remembered-choice pair).
- **`cmp reg, <zero-register>` where the original has `test reg,reg` is a zero-register residency
  tie-break, not a source-shape difference — and the way to PROVE it is to find a block where
  both compiles agree (v375, same function).** Five of six art-stamp blocks disagreed this way;
  the sixth, where VC5 happened to park the zero in ESI rather than EDX, emitted `test reg,reg`
  on BOTH sides. That asymmetry is only explicable by register residency, so no `if (p)` /
  `if (p != NULL)` / store-reorder rewrite can reach it (measured: `if (p)` is inert to the
  point; reordering the RECT stores is strictly worse, 262600).
- **⭐⭐⭐ DUPLICATING A TAIL IN THE SOURCE is a real lever, and it is the MIRROR of the
  call-parity rule above (v376, `SplashWnd::OnEnterCommitAndDispatch` 0x422660 — took it EXACT,
  444 B).** Symptom: instruction counts are EQUAL (142/142) and registers agree, but the original
  hoists a `push <const>` ABOVE the compare that guards a conditional call, then re-pushes the
  same constant inside the guarded arm — while your compile pushes it once inside the arm and once
  at the merge point. That hoist is only reachable when the source writes the guarded arm as an
  EARLY `return` with its whole tail repeated:
  ```c
  if (flag == 0) { Tail(); Dispatch(); return; }   /* tail copy 1 */
  SetMode(1);
  Tail(); Dispatch();                              /* tail copy 2 */
  ```
  The natural single-tail form `if (flag) { SetMode(1); } Tail(); Dispatch();` compiles to the same
  instruction count with identical registers but costs 14 bytes — VC5 cannot express the shared
  push slot when there is only one tail. Cross-check with CALL PARITY first: if the original's call
  tally shows a callee appearing MORE times than your source calls it, the source duplicated a tail
  and the compiler merged it; if the tallies already match but a constant push is hoisted above a
  compare, the source duplicated a tail that the compiler did NOT merge. Same family as the
  `RedrawSettingRectsMaybe` "one arm inlines the helper" finding.
- **⭐⭐ THE DUPLICATED-TAIL LEVER ALSO CONTROLS BLOCK ORDER, not just a hoisted push — and the
  diagnostic is "which side of the shared tail does the BIG block sit on?" (v474,
  `CarNetObjPartial::CarNetObj_ApplyNetState` 0x40d770, 287 B -> 276 B against 275, score
  222314 -> 128395 -> 52130 in three compiles).** cl lays basic blocks out in SOURCE order, so
  when the original's shared tail sits ABOVE a large block, the source did NOT reach that tail by
  falling out of an `if`/`else if` — it reached it by REPETITION. Worked example, a three-way
  latch (`p` given + not latched = take it; `p` given + already latched = refuse; no `p` = clear):
  ```c
  /* WRONG (128395, 287 B): one shared tail => cl emits the copy block BEFORE the tail */
  if (flag) { if (p) return 0; }
  else if (p) { flag = 1; big = *p; return 1; }
  flag = 0; return 1;

  /* RIGHT (52130, 276 B): tail written out TWICE, cl cross-jumps them back into one */
  if (flag) { if (p) return 0; flag = 0; return 1; }
  if (p == NULL) { flag = 0; return 1; }
  flag = 1; big = *p; return 1;
  ```
  Both forms have identical semantics and instruction counts; only the layout differs. Read the
  order off the ORIGINAL first: `test flag; je L2 / [arm1] test p; je TAIL; xor al,al; ret /
  L2: test p; jne BIG / TAIL: ... / BIG: ...` — the `jne BIG` that falls through INTO the tail is
  the tell, because cl inverts a branch to fall through to the block that follows it in SOURCE
  order. ⚠ Also check the OUTER guard's polarity before blaming anything else: writing
  `if (!flag)` instead of `if (flag)` put the copy block on the wrong side entirely and cost
  another 94000 points of score (222314 vs 128395) at the same length.
- **⚠ The duplicated-tail lever has a hard limit: when the original's two tail copies differ only
  in REGISTER ALLOCATION, the non-merge is a phase-ordering artifact and no source shape reaches
  it (v376, `SplashWnd::OnActivate` 0x421be0, 18 instructions, PARKED).** Diagnostic: dump both
  copies and compare them instruction-for-instruction. If they are byte-identical apart from the
  jump displacement, source duplication is worth trying. If one copy holds a value in `eax` and
  needs no `mov ecx,esi` (because ecx still holds `this`) while the other holds it in `ecx` and
  therefore must reload `this`, then the original DUPLICATED FIRST and ALLOCATED SECOND — our
  cl 11.00 cross-jumps before allocation, sees two identical blocks, and merges them no matter how
  the source is written. Measured inert there: writing the dispatch as two separate
  `if (…) { Tail(); return; }` statements (the natural form, already duplicated in source) AND as
  `switch (state) { case 0: …; case 7: … }` — both give the identical score, to the point.
- **⚠ `rename_function` BY NAME silently renames the WRONG function when the name is not unique
  across namespaces (v376 — clobbered `PopupWndBase::OnPaint` while trying to rename
  `SplashWnd::OnPaint`).** The endpoint takes `old_name`, matches the first symbol with that name
  anywhere in the program, reports `"status":"success"`, and the function you actually meant is
  untouched — so the obvious readback ("is my target renamed?") shows the rename "failed" and
  gives no hint that a DIFFERENT function was just renamed. Method-name reuse across window classes
  (`OnPaint`, `OnActivate`, `OnMouseMove`, `Create`, …) makes this the common case in this project,
  not the rare one. **Always rename by ADDRESS via `run_script_inline`**
  (`fm.getFunctionAt(af.getAddress("00421be0")).setName(…, SourceType.USER_DEFINED)`), and if you
  have already used the name form, enumerate every function carrying the OLD and NEW names before
  assuming nothing else moved.
- **A vtable slot's real signature can contradict a "no-op placeholder" declaration, and the
  `ret N` is what settles it (v376, WindowBase slots 0x20/0x24).** Slot 0x24's default body
  (0x4661a0) is a bare `ret`, so the slot is `void (void)`; slot 0x20's default (0x426130) is
  `ret 0x4`, so that slot takes one stack argument that overriders are free to ignore
  (`SplashWnd::OnActivate` and `AlbumCardWnd::OnActivate` both do). A `_vNN()` dummy declared with
  the wrong arity blocks a derived class from declaring a real override in that slot; read the
  default body's epilogue before believing the placeholder. Beware the name collision this creates:
  `SplashWnd`'s slot-0x20 override is NOT WM_PAINT (that is slot 0x6c, left at the base default),
  so naming it `OnPaint` shadows a genuinely different virtual on the same class.
- **A `POINT`/small-struct local's MEMBER INITIALIZATION ORDER picks which callee-saved registers
  its members land in (v380, `MailWnd::HitTestButton` 0x430090).** When an 8-byte struct is only
  ever passed BY VALUE (here to `PtInRect`), cl 11.00 never materializes a stack slot — it keeps
  both members in registers and pushes them at each call. Which register each member gets is
  decided by the ORDER THE MEMBERS ARE ASSIGNED, so `pt.x = x; pt.y = y;` and `pt.y = y;
  pt.x = x;` differ by a whole-function `ebx`/`edi` swap. The symptom is unmistakable and worth
  recognizing on sight: `asmscore` reports `align=0`, `insns N/N`, and a dump consisting of
  NOTHING but paired `r` rows on the `push` instructions. `pt.x`-first cost 16 reg_pen there;
  `pt.y`-first was EXACT. `AlbumCardWnd::OnLButtonDown` already used the `pt.y`-first spelling,
  so prefer it as the default when transcribing a fresh hit-test.
- **A `switch` whose live cases start above 0 gets an index-bias instruction the original may not
  have; spell out the dead low cases to pin the jump table's base (v380,
  `MailWnd::OnLButtonDown` 0x430190).** cl 11.00 biases by the SMALLEST case value, so cases 2..9
  compile to `add eax,-2; cmp eax,7; ja default; jmp [table]` over an 8-entry table. The original
  emitted `cmp eax,9; ja default; jmp [table]` over a TEN-entry table at 0x430798 whose entries 0
  and 1 both point at the default — i.e. the source enumerated `case 0: case 1:` explicitly.
  Adding them removed the `add eax,-2` and took the function to EXACT. **Read the original's
  jump table length before writing the switch:** `(next_function - table_start) / 4` entries tells
  you the real case range directly, including dead entries the decompiler folds into `default`.
  Related and needed at the same site: the switch VALUE must be UNSIGNED (the helper returns
  `unsigned int`), else cl must prove non-negativity before indexing.
- **⚠ The NUMBER of member-function declarations on a shared class can move an UNRELATED TU's
  codegen (v380, `MailWnd` vs `src/Main.cpp`'s `AppWndProc` 0x4618c0).** Bisected one declaration
  at a time with everything else held fixed: <=4 declarations on `MailWnd` compile `AppWndProc` to
  5440 B / DIFF(3830); >=5 give 5464 B / DIFF(3873). It is the COUNT, not any particular name
  (swapping which five are present reproduces 5464 exactly). Individually inert at the same site:
  the class's data members, the `class`-vs-`struct` keyword, a sibling probe struct's position in
  the header, and a `virtual` override declaration. This is the same family as the v378
  include-position dial — a whole-TU tie-break knob, not a local one. **Do not trim a class's real
  method set to chase it**; model the class correctly and record the dial where the next session
  grinding that TU will see it (here, a banner comment in `src/MailWnd.h`).
- **⚠ CONFIRMED AGAIN from the other direction, and the cost can be a whole EXACT function (v399,
  `TutorialWnd`'s method block vs. `MailWnd::RefreshClientClipRect` 0x42f8b0).** `src/MailWnd.cpp`
  includes `TutorialWnd.h` only for `g_pTutorialWnd`, yet growing that class's declaration block
  from v398's two placeholder methods to v399's real seven moved `RefreshClientClipRect` from
  1332 B EXACT to 1334 B / DIFF(150) — a 1332-byte regression in a TU nothing else in the session
  touched. Ruled out individually at the same site: the two struct-MEMBER renames (identifier-only,
  as expected) and the added `virtual` override declaration (inert, matching v380). Probing with
  dummy declarations at 8, 9 and 10 reproduced 1334/DIFF(150) exactly, so it is a THRESHOLD, not
  an oscillation, and no legitimate count gets the byte back. **Two consequences worth
  internalising: (1) after ANY session that adds declarations to a SHARED class header, re-run
  `tools/progress.py` and diff the per-file table — a single-TU `cc.sh` will never show it; and
  (2) when the correct class model flips a previously-EXACT function in another TU, that function
  had a REAL residual all along that the wrong (too small) model was masking. Treat the flip as
  newly-exposed information, not as damage to undo.**
- **⚠ Ghidra's decompiler can REUSE one display variable across a pointer reassignment, silently
  attributing a later dereference to the wrong field (v380, `MailWnd::RefreshClientClipRect`
  0x42f8b0).** With `this` typed, the listing opens `ptVar2 = &this->rectScreenMaybe` (+0x614) and
  then shows a dozen later `CopyRect(&local, ptVar2)` calls — but the raw disasm reloads the same
  register (`lea edi,[esi+0x634]` at 0x42f944) so every one of those really reads
  `rectLayoutBaseMaybe` (+0x634). The UNTYPED listing of the same function had it right. When a
  typed and untyped decompile of one function disagree about which field a repeated access
  targets, `lea`-grep the raw bytes for the pointer register — do not assume the typed view wins.
- **⭐ cl TAIL-MERGES two sibling call statements that differ in exactly one argument — hoisting
  their common LEADING pushes as well as merging their trailing `call`. So the source that
  produces a branchy argument is two separate calls, NOT a ternary (v381,
  `MailWnd::SelectNextCardMaybe` 0x42dd50 and its three siblings).** The original reads
  `mov al,[ebp+0x606]; push ebx; test al,al; je L2; push 1; jmp L3; L2: push 2;
  L3: mov ecx,[g_pPostBagCache]; call` — one shared `push ebx` (the NULL second argument), one
  shared call, and only the differing first argument duplicated. `f(cond ? 1 : 2, NULL)` does NOT
  produce that: cl compiles the ternary branchlessly (`neg al; sbb eax,eax; add eax,2`), and
  hoisting it into a local `int nCategory` first changes nothing. Writing it as
  `if (cond) { p = f(1, NULL); } else { p = f(2, NULL); }` was EXACT on the first compile at four
  separate sites. The same lever closed `MailWnd::DrawFlagFrameMaybe` (0x42e4e0), where the trailing
  blit call belongs inside BOTH arms of the if/else rather than after it, and it explains the
  single shared `call` at the bottom of `MailWnd::RestoreCardBackdropMaybe` (0x42e5e0). **Tell:
  a branch whose two arms differ only by an immediate that is then consumed by one call.**
- **A count compared against zero with `test ax,ax; jbe` means the source wrote `n > 0`, not
  `n != 0`** (v381, three sites across `MailWnd::OnButtonMouseUp`/`OnMouseMove`). `test` always
  clears CF, so `jbe` is semantically identical to `je` — but cl only picks the `jbe` encoding
  when the comparison is a genuine UNSIGNED relational (`> 0` on an `unsigned short`). `!= 0`
  gives `jne`/`je`. One byte each, and it is the last byte to fall on an otherwise-exact function.
  The mirror rule for the range test: `n <= 4` gives `cmp ax,4; ja`, while `n < 5` gives
  `cmp ax,5; jae`.
- **A repeated block that appears N times in the decompiler output, with the merged value living
  in EAX rather than the register the block itself computed in, is an INLINED HELPER FUNCTION —
  model it as one** (v381, `MailWnd::GetViewedCategoryCardCountMaybe`). The tell is a pair (or
  more) of `mov eax,<the block's own register>` copies, one at the end of each arm of the block's
  own internal if/else, immediately before the join — those are the helper's `return n;` sites,
  and they cost an extra move that a plain inline variable never would. Spelling the block out
  literally at each call site puts the value in the block's own register (EBP here) and adds a
  `mov ebp,eax` on the OTHER arm instead, missing by ~10 instructions per site. Declaring an
  ordinary member function and defining it `inline` in the .cpp reproduces the original exactly;
  no out-of-line COMDAT is emitted. The helper here ends up inlined at nine call sites.
- **A `switch` over a value whose live cases nearly, but not quite, cover a contiguous range gets
  a TWO-LEVEL table (`xor edx,edx; mov dl,byte ptr [eax+<bytemap>]; jmp [edx*4+<table>]`) instead
  of a direct one** (v381, `MailWnd::OnMouseMove` 0x430800). Cases 0,2,3,4,5,6,7,9 out of 0..9 was
  enough for cl to prefer the compressed form; adding the two dead labels (`case 1: case 8:
  return 0;`) restored the original's plain `jmp [eax*4+table]` over a full 10-entry table. Same
  family as v380's `add eax,-K` index bias, and diagnosed the same way — derive the entry count
  from `(next_function - table_start)/4` before writing the switch.
- **cl's copy-elision of a by-value struct argument is NOT guaranteed, and when the original did
  NOT elide it you can see the whole 16-byte frame slot in the prologue** (v381, still-open
  residual on `MailWnd::OnButtonMouseDown` 0x42e150). The original reserves `sub esp,0x10`, fills
  a `RECT` local there field by field, then copies it dword-by-dword into the argument block; cl
  here builds the argument block directly and never materializes the local, coming out 9
  instructions short. Declaring the local at function scope rather than inside the block does not
  change it. When the SAME function's sibling path takes the local's address (`CopyRect`/
  `OffsetRect`), the slot appears for free and the path matches — so the question is only ever
  what forces addressability on the path that does not.
- **⚠ A hand-written class's first data member is NOT at offset 0 once the class has any virtual
  function — the compiler's synthesized vptr occupies +0** (v381, one wrong byte in
  `MailWnd::RedrawCardAreaMaybe`). `src/WindowBase.h` lists `hInstance` first and declares no
  explicit vtable field (deliberately — see its own header comment), so the raw offsets run
  +0 vptr, +4 hInstance, +8 hwndSelf, +0xc hwndOwner. Reading a raw `[esi+8]` off the disasm and
  reaching for the header's *second* named member gives `hwndOwner` and compiles to `[esi+0xc]`
  — a clean single-byte miss with no diagnostic. Count the vptr before mapping any raw offset
  onto a polymorphic class's member list.

- **⭐⭐ A Win32 API call spelled as an ARGUMENT to another API call is evaluated in its
  right-to-left ARGUMENT position; the original almost always hoists it into a local first
  (v383, `MailWnd::OpenAttachmentMaybe`/0x42f250 — took a DIFF(356) function EXACT in one
  compile).** `FormatMessageA(FLAGS, NULL, GetLastError(), lang, &psz, 0, NULL)` emits the four
  constant pushes above `GetLastError()` FIRST and only then calls it; the original calls
  `GetLastError` before touching the argument block at all, i.e. the source reads
  `DWORD dwErr = GetLastError(); FormatMessageA(FLAGS, NULL, dwErr, ...);`. **The blast radius is
  the diagnostic, not the call site**: the function was already 296/296 structurally identical
  instructions, yet the late call's register pressure flipped the esi/edi import-thunk cache
  assignment across the whole body AND reordered the `mov ecx,<singleton>` in two unrelated
  `__thiscall` helper calls further down. A whole-function `r`-row cascade with a correct
  instruction COUNT and one API call sitting in an argument slot ⇒ hoist it, don't chase the
  registers. Applies per copy: six copies of the same report-and-continue block needed six hoists.
- **⭐ `if (cond) return 1; return 0;` is what emits a bare `test al,al; setne al` (v383,
  `MailWnd::Create`/0x42edb0).** With an `unsigned char` return, `return cond;`,
  `return cond != 0;` AND `return cond ? 1 : 0;` all take the long widening route
  (`neg al; sbb eax,eax; neg eax`); only the branch form lets cl peephole the whole thing back
  into a `setne`. This EXTENDS the existing "`bool` return widens to full EAX" bullet above —
  that one says to use `unsigned char` + the early-return shape when the original ends in
  `xor al,al`/`mov al,1`; this says the SAME source shape is also what produces the `setne`
  ending, so the early-return form is the default for any byte-returning function regardless of
  which of the two tails the original has.
- **⭐ A `test al,al` (not `test eax,eax`) on a called function's result pins the CALLEE's
  return type to a BYTE (v383, `MailWnd::OnActivate`/0x42e420).** The caller is a legitimate
  oracle for a not-yet-transcribed callee's declared return type: `TutorialWnd::NotifyOrLaunch`
  was declared `unsigned int` on a guess, its only value-reading call site tested `al`, and
  narrowing the declaration to `unsigned char` closed the single differing byte. Safe to do
  when every other call site discards the result — a declared-only function's mangled name is
  irrelevant to COMDAT matching, so the narrowing moves no other TU. Mirror of the
  `sete`-predicate lever: the byte width of a return value is visible from OUTSIDE the function.
- **⭐ An unsigned relational on a COUNTER needs the FIELD to be unsigned, not just the literal
  (v383, `MailWnd::OnTimerDefaultMaybe`/0x42fff0).** `cmp eax,0x14; jb` on a tick counter is an
  unsigned `>= 20`; with the member declared `int` the same `>= 20` emits a signed `jl` and no
  amount of casting at the comparison reads as natural source. Companion to the v382
  `test ax,ax; jbe` ⇒ `n > 0` bullet: there the SPELLING carried the signedness, here the
  DECLARATION does. Note the constant in the original's `cmp` is the one the source wrote —
  `cmp 0x14` is `>= 20`, not `> 19` (which would be `cmp 0x13; jbe`).
- **⭐ A two-case Win32 message handler whose COMPARE order is the reverse of its BODY order is
  a `switch`, not an if/else-if chain (v383, `MailWnd::OnUnhandledMessageMaybe`/0x42ee20).**
  The original compares `0x112` then `0x5f5` but lays the `0x5f5` body out FIRST and the
  `0x112` body second; an `if (msg == 0x112) {A} else if (msg == 0x5f5) {B}` emits A first and
  B second, a straight DIFF(80) block swap at identical instruction count. `switch (msg) { case
  0x112: A; break; case 0x5f5: B; break; }` reproduces the original exactly. Diagnostic: match
  the compare sequence against the block sequence — when they disagree, you have the wrong
  control-flow construct, not a scheduling problem.
- **⭐ cl constant-folds a just-stored member through an inlined helper's own guard on it
  (v383, `MailWnd::BeginModalCapture`/0x42f5e0).** The original stores `bViewingOutboxMaybe = 1`
  and then contains only the outbox arm of the `if (bViewingOutboxMaybe)` helper that follows —
  which reads as "the helper was not called here, the block was spelled out literally". It was
  called: cl forwards the stored constant into the immediately-following load (no intervening
  call) and the dead arm folds away. Call the helper; do NOT hand-specialize it. Companion to
  v382's "a block repeated N times is an INLINED HELPER" bullet — this is the case where the
  inlined helper does not LOOK repeated because one copy got specialized.
- **⭐⭐ A `ret 0xNN` that pops MORE than your parameter list accounts for means a DEAD TRAILING
  PARAMETER, and it is worth two whole functions (v385, `MapWnd::DrawSlotPlate`/0x431560).**
  `DrawSlotPlate(RECT rect, int nSlotIndex)` is five dwords = `ret 0x14`, but the original is
  `ret 0x18`. Adding an unread `int reserved` (0 at both call sites) fixed not just this
  function but the two CALLERS, whose missing `push 0` had read as a mysterious two-instruction
  shortfall each. Same idiom as `AlbumCardWnd::DrawButtonIcon`/`PlayButtonPressFeedback`. **Check
  the `ret` immediate against your own signature on EVERY thiscall member before doing anything
  else** — it is a one-line arithmetic check that no amount of `--dump` reading substitutes for.
- **⭐⭐ Whether a base-class default is reached as a FREE function or as a base-qualified
  virtual call is visible in one instruction, and both spellings exist in this codebase (v385,
  `MapWnd::OnTimerDefaultMaybe`/0x4323e0 vs `MailWnd::OnSetFocus`).** Both compile to
  `call 0x422ea0` (the shared `DefWindowProcStub`), but the base-qualified form
  `WindowBase::OnTimerDefaultMaybe(...)` emits `mov ecx,this` immediately before the call and
  the file-local free-function declaration does not. `MailWnd` needed the free declaration;
  `MapWnd` needs the qualified call. Look for the ECX load before deciding which declaration to
  write — guessing costs 2 instructions plus the register-allocation fallout of a spurious
  `this` liveness.
- **⭐ `if (cond) f(A); else f(B);` and `f(cond ? A : B)` are DIFFERENT codegen, and the original
  usually wants the former (v385, `MapWnd::DrawOwnerDot`/0x431ed0, `DrawPeerTrainDotsMaybe`,
  `DrawSlotPlate`).** The ternary materializes the value (`mov eax,7; je; mov eax,4` or
  `xor ecx,ecx; sete cl`) and emits ONE call; two separate calls emit `push 7 … jmp` / `push 4`
  with the argument pushes duplicated per arm and the call itself cross-jumped/tail-merged. The
  tell is a `jmp` that lands INSIDE the other arm's push sequence. Ghidra's decompiler
  re-merges these into `iVar = 7; … iVar = 4; … f(iVar)`, so its output actively hides the
  distinction — read the raw disasm for duplicated pushes. Worth 250652→132355 on one function.
- **⭐ A rect derived from another rect is spelled as a DERIVATION CHAIN, not re-anchored on the
  common base (v385, `MapWnd::RefreshClientClipRect`/0x430fe0).** The original emits
  `lea eax,[edi+0x16]` (grid.left) then `add eax,0x242` (grid.right) — cl does NOT CSE
  `base.left + 600` back into `grid.left + 578`, so seeing the second constant as a DELTA off
  the first proves `rectGrid.right = rectGrid.left + 578;` rather than
  `rectGrid.right = rectLayoutBase.left + 600;`. The same reading recovers `bottom = top + 20`
  where a naive transcription writes `bottom = top + 10` after `top -= 10`. Whole-function win:
  380962→248922 across two functions. Corollary: the DELTA constants (578, 434, 20) are the
  numbers a human wrote; the absolute ones (600, 0x1d5) are Ghidra's own re-anchoring.
  **v386 adds the sequencing half of the same lever: GROUP EACH CHAIN, don't interleave them.**
  On a rect whose four fields form two independent chains (`left → right` off one base,
  `top → bottom` off another), writing them interleaved as left, top, right, bottom hands the
  first register to whichever chain the scheduler emits first; writing them chain-grouped as
  left, right, top, bottom pins the `left` chain to eax and matches the original's whole
  register cascade. On 0x430fe0 that one reordering of `rectGrid`'s four assignments — nothing
  else changed — dropped reg_pen 59→44 and the residual 381 B → 316 B, and moved the first
  divergence from offset 0xc7 to 0x123. **Read the emitted STORE order to check the hypothesis
  before reordering:** member stores (unlike outgoing-argument stores) follow source order, so
  a rect whose stores come out left, right, bottom, top was NOT written left, top, right, bottom.
- **⭐ `dest.rect = src;` (struct assignment) vs. four member assignments: the tell is a base
  pointer (v385, `MapWnd::DrawSlotPlate`, `RefreshClientClipRect`).** A struct assignment
  computes `&dest.rect` once (`add edx,4` / `mov eax,ebp`) and stores at `[edx+0..0xc]`; four
  member assignments store at `[obj+4..0x10]` off the object pointer and re-load the object
  pointer per statement. `RECT a = b;` also leaves the SOURCE readable through a pointer
  (`mov eax,ebp; mov ecx,[eax]; …`), which is how `RECT rectTmp = rectClipBounds;` is
  distinguishable from four `rectTmp.left = rectClipBounds.left;` lines.
- **⭐ A loop-invariant GLOBAL field read inside a loop that also CALLS something will not be
  hoisted by cl — if the original hoists it, the SOURCE hoisted it (v385,
  `MapWnd::OnActivate`/`OnTimerDefaultMaybe`).** `mov edx,[g]; mov ebp,[edx+0x7d0]; cmp ebp,edi`
  once before the loop, versus `mov edx,[g]; cmp [edx+0x7d0],edi` every iteration, is a
  one-line source difference (`int nLocalSlot = g->selectedProviderIndex;`). And the
  DECLARATION ORDER of the loop's own locals is visible too: the original inits `i` before the
  rect cursor, which under VC5's leaky for-scope means the source declared `int i = 0;` at the
  top and wrote `for (; i < 9; i++)`. That last detail alone was DIFF(12)→DIFF(4).
- **⭐ Two rects built in the same function are two DIFFERENT locals unless the codegen says
  otherwise (v385, `MapWnd::OnActivate`).** Merging them into one reused local (tempting, since
  the original stores the second one to a stack slot AND into the by-value argument block)
  measured 61246→122420. The stack-slot store is just what an already-address-taken local does;
  it is not evidence of reuse. Conversely, hoisting the blit SOURCE bitmap into a named local
  (`LocoBitmap *pIcon = pExitIconBitmap;`) before the `OffsetRect` IS load-bearing — it frees
  `this` for the rect's base register (`add esi,0xf0` instead of `lea edx,[esi+0xf0]`) and was
  DIFF(183)→DIFF(27) on `MapWnd::OnKeyDown`.
- **⭐ Grid-index arithmetic: match which axis is computed FIRST, and where the `+1`/`-1` live
  (v385, `MapWnd::OnMouseMove`/0x432540, 158741→24118 in one edit).** The original computes
  column then row, adds 1 to BOTH, compares both against their limits, then forms
  `(nRow - 1) * nCols + nCol - 1`. Writing it as row-then-column with the `+1` on only one axis
  is the same arithmetic and a 12-instruction shortfall. The `dec eax; imul eax,ecx;
  lea edi,[eax+edi-1]` tail is the direct readout of where the source's `-1`s are.

- **⭐⭐ `test al,al` followed by `jbe` (not `je`) means the SOURCE comparison was an UNSIGNED
  relational on an unsigned field, even though `jbe` and `je` are behaviourally identical after
  a `test` against itself** (CF is always cleared, so `<=` degenerates to `==`). cl keeps the
  unsigned branch flavour from `if (field > 0)` on an `unsigned char`/`unsigned int` member,
  while the more obvious `if (field != 0)` emits a plain `je`. One-instruction tell, one-token
  fix, and it is invisible in a decompile — Ghidra renders both as `!= 0`. Confirmed v390 on
  `DecorActorBase::TickIdleDecayMaybe` (0x433c50): `if (nMoodMaybe != 0)` → `je`,
  `if (nMoodMaybe > 0)` → `jbe`, closing that function to EXACT. Sibling of the existing
  "an unsigned relational on a COUNTER needs the FIELD to be unsigned" bullet: that one is about
  the field's declared type, this one is about the OPERATOR chosen at the comparison site.

- **⭐⭐ A virtual whose base implementation IGNORES its parameter still has that parameter, and
  the ONLY evidence is the base body's `ret 0xNN`** — the decompiler renders such a function as
  0-param because nothing in the body reads the stack slot. This is a distinct instance of the
  existing `ret 0xNN`-pops-more-than-your-params tell, but with a specific and very common
  cause: a polymorphic per-tick/per-event slot where the leaves consume an argument and the
  shared base body does not. Cross-check the CALLER: v390's `DecorActorBase::TickIdleDecayMaybe`
  (0x433c50) reads as 0-param, but `WalkerActor::TickMaybe` pushes its own incoming argument
  (`mov edi,[esp+0x10]; push edi`) immediately before calling it, and the callee ends `ret 4`.
  Declaring the ignored parameter (`void TickIdleDecayMaybe(int /*nArg*/)`) was the whole
  remaining diff. Fix the Ghidra prototype at the same time, so every sibling call site
  re-renders with the argument visible.

- **⭐ A hand-written `padN[SIZE]` field silently mis-anchors every UNALIGNED field after it,
  and only the compiled bytes will tell you** — restating CLAUDE.md's rule with a live v390
  example worth internalizing. `src/TilePlacedObj.h` declared
  `... unsigned char Unk0x8eMaybe; unsigned char pad0x90[20]; unsigned char pad0xa4[20]; unsigned int Unk0xb8Maybe;`
  which actually placed `pad0x90` at `+0x8f` and `pad0xa4` at `+0xa3` (the trailing `unsigned
  int` re-anchored itself to `+0xb8` by alignment, hiding the drift from every downstream
  field). Nothing warned: not the compiler, not `lint_idiom.py`, not the match score — the
  affected TU's diff count was IDENTICAL at the wrong and the right offsets. What exposed it was
  giving one of the pads a real 4-byte-aligned type (`DecorActorBase *apSpawnedActorMaybe[5]`),
  which moved the emitted displacement from `[esi+0xa3]` to `[esi+0xa4]` and showed up as a
  2-byte COMDAT delta in the before/after byte-diff. **Whenever a bulk-zeroed region is modelled
  as pads, read the original's own `lea`s for that region and pin each pad's start address
  against them** — here `lea eax,[esi+0x90]` / `lea edx,[esi+0xa4]` were the ground truth all
  along.

- **⭐ Resource ids are a first-class naming oracle in this binary, not just content.** A
  TileKind id IS an `RT_STRING` id: `UIResources::TileKind_GetOrLoadDescriptor` (0x446ea0) feeds
  it straight to `LoadStringA`, and `TileKind::TileKind_GetCategory` (0x446030) is just
  `(kindId >> 10) & 0xff`. So any magic id or id RANGE in a body can be resolved to a resource
  path with `tools/resstrings.py` (full table: `docs/TILEKIND_IDS.md`). v390 used this to
  identify two whole classes — the categories a factory switches on turned out to name
  `people\*`/`animals\*` and `vehicles\*` directly — and to explain an opaque
  `rand()%0x31 + 2` / `rand()%0xb + 0x33` pair as "pick a random male / female first name".
  Reach for it before guessing at any unexplained small-integer constant that reaches a
  descriptor lookup.

- **⭐⭐ A branchy 0/1 bool MATERIALIZATION around a multi-term comparison means the source
  called an INLINE PREDICATE — and when both polarities appear, the original has TWO of them
  (v391).** This is the `&&`/`||` sibling of the `sete`-materialized byte predicate above.
  When the original computes
  `cmp a1,b1; jne L0; cmp a2,b2; jne L0; mov r,1; jmp T; L0: xor r,r; T: test rl,rl; je …`
  instead of just branching on the compare, the source is NOT an inline `if (a1==b1 && a2==b2)`
  — it is `if (SomePredicate(a1,a2,b1,b2))` whose body is that expression. `src/WalkerActor.cpp`
  has ~10 such sites, all comparing two world (x,y) positions (or testing one against the
  -1/-1 "none" sentinel), and routing them through
  `static inline bool ArePositionsEqualMaybe(int ax,int ay,int bx,int by)` moved
  `WalkerActor::FollowLeaderStepMaybe` from 59813 to 30353 with the materialization block
  matching instruction-for-instruction, and supplied the instructions four sibling functions
  were SHORT by (0x4327b0 365→398 of 400 bytes, 0x433860 419→445 of 448).
  **Read the short-circuit to pick the polarity:** which test jumps to the 1-result tells you
  whether the body is `ax==bx && ay==by` (the `jne` goes to the 0-result) or
  `ax!=bx || ay!=by` (the `jne` goes to the 1-result). Both shapes occur in the same TU, so
  the original really does declare an equality helper AND its inverse twin rather than
  negating one at the call site — a `!` applied to an already-materialized 0/1 would compile
  to an inversion, not to a re-ordered branch. Keep the predicates TU-local (same
  header-churn reason as the `sete` family).

- **⭐ A `dec eax; je / dec eax; je / dec eax; …` chain is a `switch`, not an if/else-if
  chain (v391).** VC5 lowers a `switch` over a small dense range of small integers by
  repeatedly decrementing the selector and testing zero; an `if (n==1) … else if (n==2) …`
  over the same values compiles to `cmp eax,1 / jne … / cmp eax,2 / …` instead. The layout
  differs too: the switch puts the LAST case's body at the fall-through and jumps FORWARD to
  the earlier cases' bodies, which sit past the shared tail. `WalkerActor::TickMaybe`
  (0x4327b0) went 250120 → 96039 on this one change. Corollary for reading a decompile: Ghidra
  renders both forms as an if/else-if chain, so the `dec` chain in the raw disasm is the only
  tell — check it before transcribing any 3-or-more-way dispatch on a small int.

- **⭐ Two duplicated function epilogues in the candidate = the original returned a VARIABLE
  through one shared tail, and its locals were declared at the TOP (v391).** When the original
  reaches its epilogue via `jne <tail>` / `je <tail>` from several early exits, and that tail
  is `mov al,[esp+N]; pop…; ret`, the return value lives in a stack slot and every exit is a
  `goto` (or a `return theVariable`) rather than `return 1`. A candidate that writes
  `return 1;` at those sites gets the value constant-folded into a private `mov al,1` +
  a full epilogue copy per site — `WalkerActor::CanStandAtMaybe` carried 16 EXTRA instructions,
  two whole epilogues, for exactly this reason. The fix has a hard prerequisite: **VC5 rejects
  a `goto` that jumps past an initializer** (`error C2362`), so the original must have declared
  every local at the top of the function without initializers — the ordinary 1998 house style,
  and worth adopting by default in any function with more than one early exit. 470178 → 315794
  from the shape change alone.

- **A category/enum byte compared against a range is usually widened to a SIGNED `int` local
  first (v391).** `and ecx,0xff; cmp ecx,2; jl` is a SIGNED compare on a zero-extended byte —
  i.e. the source read the byte field into an `int` and tested `>= 2`. Modelling the local as
  `unsigned char` instead gives `cmp cl,1` (unsigned, and against the off-by-one constant),
  which reads as ordinary noise but is a real source-shape difference. Same site: prefer the
  explicit `if (p == 0) n = 0; else n = p->field;` over the ternary `n = p ? p->field : 0` —
  VC5 lays the explicit form out with the ZERO case as the fall-through (`test; jne <load>`),
  the ternary the other way round.

- **Branch ORDER within an if/else is recoverable from which arm falls through, and it is
  worth checking on every two-arm test (v391).** `WalkerActor::AdvanceMovementMaybe` opens
  `mov eax,[esi+0xf0]; test eax,eax; jne <far>` — the fall-through is the pointer-is-NULL arm,
  so the source is `if (p == 0) { … } else { … }`, not the `!= 0`-first form the decompile's
  own ordering suggests. Writing it inverted cost 671641 vs 74690, i.e. it looked like a
  whole-function allocation failure rather than a one-line shape bug. Same lever settled the
  `nCategory > 4` vs `<= 4` arm order in `CanStandAtMaybe` (315794 → 177858).

- **⭐⭐ A struct-returning call whose result is read through the RETURNED POINTER (`call f;
  mov edi,[eax]; mov ebx,[eax+4]`) means the source declared the struct at the TOP of the
  function and ASSIGNED to it — not `T v = f(...)` at the point of use (v392).** With
  `POINT pt = ComputeStepTargetMaybe(...)` VC5 makes `pt` itself the hidden return buffer and
  every later read comes off `esp` (`mov ecx,[esp+0x1c]`). Hoisting the declaration to the top
  of the function and writing `pt = ComputeStepTargetMaybe(...)` instead makes the buffer an
  unnamed temporary, and the reads then go through the pointer the call left in `eax` — which
  is what the original does. On `RoadVehicleActor::AdvanceMovementMaybe` (0x453eb0) that one
  change took the residual from 20679 to 4. This is the struct-return sibling of #5/#13.

- **⭐ Two adjacent stack ints whose SLOTS refuse to swap when you reorder their declarations are
  really ONE struct (v392).** VC5 assigns each scalar local its own slot and (here) ignored
  declaration order entirely: `int nPrevLeft; int nPrevTop;` and `int nPrevTop; int nPrevLeft;`
  both produced left@`esp+0x14`/top@`esp+0x10`, while the original wanted left@`0x10`/top@`0x14`.
  A struct's member order, unlike sibling locals' declaration order, IS binding — replacing the
  pair with `POINT ptPrev;` (declared before the other struct local, so it takes the lower
  slots) closed the last 4 bytes and made 0x453eb0 EXACT. **Read "declaration order didn't move
  the slots" as evidence about the TYPE, not as an intrinsic tie-break.**

- **⭐ `xor cl,cl` / `mov cl,[r+N]` / `and ecx,0xff` around a `switch` or compare = the zero arm
  of a ternary is BYTE-typed, i.e. the source has `(p == 0) ? (unsigned char)0 : p->byteField`
  (v392).** A plain `(p == 0) ? 0 : p->byteField` makes the ternary's common type `int`, and
  VC5 then emits `xor edx,edx` (a full-width zero) plus a single `movzx dx,byte ptr [r+N]` for
  the other arm — no separate widening step. The byte-typed form instead produces both arms as
  byte moves into the same low register followed by ONE widening instruction, which is the
  original's shape. Worth ~80k of residual on `RoadVehicleActor::CanStandAtMaybe` (0x454050,
  which it made EXACT) and again on both of that TU's other category tests.
  **v414 found the SAME lesson in a THIRD, differently-shaped disguise, and it is the one to
  recognise when the widening step is already right** (`DecorObjMgrMaybe::DeregisterEntryMaybe`
  0x434b60, DIFF 26 -> EXACT): there the `int` form did NOT emit a `movzx` at all — VC5 noticed
  the tested pointer register is already 0 on the null arm and replaced the whole ternary with
  `je` past the load, letting the trailing `and eax,0xff` produce the 0 from the pointer itself
  (`test; je done; xor ecx,ecx; mov cl,[eax+8]; mov eax,ecx; done: and eax,0xff`). That is the
  SAME 9 bytes as the original's `jne load; xor al,al; jmp done; load: mov al,[eax+8]`, so the
  score reads like pure branch-layout noise. Casting ONLY the literal — `(unsigned char)0` — is
  what blocks the rewrite and restores the original shape; casting only the field arm is inert.
  Diagnostic: a `je`/`jne` polarity flip at a `p == 0 ? 0 : p->byteField` where the byte load and
  the widening instruction both already agree.

- **A `movzx dx,dl` + `cmp dx,0xD` pair (16-bit, not 8- or 32-) pins the local holding a byte
  field to `unsigned short` (v392).** Loco's two actor pathfinders both read a tile's
  `categoryByte` into a 16-bit local; an `int` local gives `and ecx,0xff` + `cmp ecx,0xd` and a
  `char` gives `movsx`. Related: when the SAME null test feeds two extractions
  (`nCategory` and `nKindId`), the original re-tests the pointer between them
  (`test ecx,ecx` twice) — that is two separate ternaries, not one shared `if/else` writing
  both.

- **A `cmp eax,0x900; ja` is an UNSIGNED `> 0x900` on an `unsigned int` local, not a signed
  `< 0x901` (v392).** Same family as #2 (comparisons are emitted literally), but the tell here
  is the mnemonic's signedness rather than the constant: assigning a distance-returning
  `int` function's result to an `unsigned int` local and testing `> 0x900` gives `ja`, while
  testing the call directly with `< 0x901` gives `jge`. Cheap to get wrong because both read
  identically in the decompile.

- **MSVC 5 needs FOUR dense cases before it emits a jump table; three compile to a `sub/je`
  compare chain (v392).** So a `switch` whose emitted form is `cmp ecx,3; ja <default>;
  jmp [ecx*4+<table>]` over a 4-entry table where one entry aliases the default has an EXPLICIT
  no-op `case` label in the source. `RoadVehicleActor::ApplyLaneOffsetMaybe` (0x453d70) has
  four nested switches with the same three-of-four shape; only the first spells out its no-op
  case, and adding that one label is what turned its chain into the original's table and made
  the function EXACT. Sibling of #16.

- **A `cmp`'s OPERAND ORDER follows the source's operand order, so a clamp written the wrong
  way round flips every `jg`/`jl` in the chain (v393).** `if (nX > nLimit)` emits
  `cmp edi,eax; jg`, while the semantically identical `if (nLimit < nX)` emits `cmp eax,edi;
  jl`. This is invisible in a decompile — Ghidra normalizes both to the same C — but on a
  function that clamps against several candidate limits in an `||` chain it costs one
  misaligned branch per arm. `BuildToolButton::RepositionWithHotspot` (0x449dc0) went from
  DIFF 634 to 342 on this alone. When a residual is "the right instructions with the wrong
  jump conditions", re-read the raw `cmp` and copy its operand order literally.

- **An explicit `== true` on a `bool` member can be load-bearing: it lets MSVC reuse a
  constant 1 that is already live in a register (v393).** `if (bFlag)` compiles to
  `mov al,[esi+N]; test al,al; je`, but `if (bFlag == true)` compiles to `cmp [esi+N],al`
  where `al` holds a 1 the surrounding code already needed — in 0x449dc0 the same `mov eax,1`
  serves both the preceding `nButtonStateMaybe != 1` compare and this test. So a `cmp <mem>,
  <reg>` against a boolean field, with no separate load, is positive evidence the source wrote
  the redundant-looking `== true`. Same family as the existing `== false`/arm-order lever.
  ⚠ But do NOT then apply that spelling to every test of the same field — see the next bullet.

- **The SAME `bool` field can be spelled two different ways inside ONE function, and the raw
  `cmp byte,1` vs `test al,al` is the only evidence of which (v394).** `bDraggingMaybe`
  (+0x90) is tested three times in `BuildToolButton::AdvanceAnimFrameMaybe`: twice as
  `cmp BYTE PTR [esi+0x90],1` (the `== true` spelling) and once as
  `mov al,[esi+0x90]; test al,al` (the plain spelling), and transcribing all three the same
  way costs an instruction at whichever sites disagree. Humans are not consistent; read every
  test site's own bytes rather than normalizing them to one form.

- **⭐ Branch-ARM ORDER is read straight off the guard's own jump target, not guessed from
  which arm "feels" primary (v394, extends v393's arm-order lever).** For
  `mov al,[esi+0xad]; test al,al; je <far>`, the FALL-THROUGH block is the arm the source
  wrote FIRST and the predicate is the PLAIN one — i.e. `if (bFlag) { <fall-through> } else
  { <far> }`. Writing the equivalent `if (bFlag == false) { <far> } else { <fall-through> }`
  is the same predicate with the arms swapped and produces a `jne` plus a different block
  layout. On `BuildToolButton::RepositionWithHotspot` (0x449dc0) swapping the two arms of one
  such `if` was worth DIFF 252 → 119 AND closed the function's last byte of length gap
  (753/753). Whenever a `--dump` shows a lone `je`↔`jne` mismatch with otherwise-aligned
  blocks on either side, the fix is an arm swap, not a register hunt.

- **⭐ `mov [t],v; mov [t+4],0; fild qword [t]` is `(double)` of an UNSIGNED int — a plain
  `int` gives `fild dword` (v394).** MSVC has no direct unsigned-to-x87 conversion, so it
  zero-extends through a 64-bit stack temp; the giveaway is the explicitly stored ZERO high
  dword. This is a free signedness oracle for any struct field that reaches floating-point
  arithmetic exactly once: `AnimDescRefObj0x477488::nAnimValueCache` was `int` in `src/` and
  `dword` in Ghidra, and the single `(double)nAnimValueCache / …` site in
  `BuildToolButton::AdvanceAnimFrameMaybe` settled it — correcting the field type took that
  function from DIFF 883 to 345 in one edit. Same family as the `movsx`/`movzx` load-side and
  `setne`-store-side signedness tells.
  **The BYTE-width form of the same oracle (v457): `xor eax,eax; mov al,[field]; mov [t],eax;
  fild dword [t]` widens with a ZERO-EXTEND, so the field is `unsigned char`; a plain (signed)
  `char` has to `movsx` there instead.** `AppWindow::minVehicleFps` was `char` in `src/` on no
  evidence at all — the field had exactly one consumer in the whole image
  (`AnimDescRefObj0x477488::AdvanceAnimFrameMaybe`'s frame-rate throttle) and that consumer is a
  float compare, so this was the only site that could ever decide it. Note the sequencing
  difference from the unsigned-int case above: a byte still fits in a dword temp, so there is no
  64-bit slot and no stored zero HIGH dword — the tell is the `xor`/`mov al` pair feeding the
  temp rather than a `movsx`.

- **⭐ RETURN TYPES: a virtual whose result is consumed only through EAX at ONE call site is
  still pinned by that site (v457).** `AnimDescRefObj0x477488` slot 14 was declared `void` for
  many sessions because its two known callers ignore the result; `AdvanceAnimFrameMaybe`'s bounce
  arm then turned up doing `call [edx+0x38]` and immediately using EAX as the frame index it
  publishes through slot 8. cl will not assume EAX holds anything after a `void` call, so the
  original's declaration cannot have been `void`. **When a call's EAX is live across the
  instruction that follows it, that is proof of a non-void return, not a scheduling accident —
  and changing a virtual's return type is layout-neutral, so it is one of the few shared-header
  edits that is cheap to test** (measured repo-wide here: zero collateral bytes).

- **Inside a DESTRUCTOR, MSVC 5 devirtualizes `this->Virtual()` into a direct call; routing
  the call through a base-class pointer local suppresses it (v393).** The dynamic type is
  known during destruction, so `SetDescriptor(0,-1,0)` in `~BuildToolButton` compiled to a
  direct `call` to the WidgetBaseObj override where the original dispatches through the
  vtable (`mov edx,[esi]; call [edx+0x18]`). `WidgetBaseObj0x4784c8 *pSelf = this;
  pSelf->SetDescriptor(...)` reproduces the original exactly. The tell is a dtor whose
  decompile shows a VIRTUAL dispatch on `this` — that cannot come from a plain
  `this->Method()` call in the source. Note the identical statement in a non-dtor member
  (`ClearOwned`) needs no hop and matched first try: outside a ctor/dtor there is no dynamic
  type to fold. Related: writing the dtor as a plain delegating `ClearOwned();` is NOT the
  answer — MSVC emits a real 127-byte `call`, not an inline expansion, so an original whose
  dtor repeats the body really did repeat it in source.

- **A by-value struct argument is copied with `sub esp,N` + per-field stores; N separate
  scalar params are individual `push`es (v393).** `BuildToolButton::BlitAllRegionsMaybe`
  (0x449c00) was 48 bytes short and structurally wrong as
  `(LONG left, LONG top, LONG right, LONG bottom, char)`; respelling the first four as one
  by-value `RECT` made it EXACT. The `mov eax,esp` / `mov [eax],r1` / `mov [eax+4],r2` … shape
  at a call site is the signature — count the stores, not the pushes, and check the callee's
  `ret N` for the total. The same applies at the RECEIVING end: a forwarded `rect` keeps the
  struct shape through nested calls.

- **When N scalars and one by-value struct are CALLEE-indistinguishable, the CALLER picks the
  true model (v516).** A `cdecl` callee reading `[ebp+8..+0x14]` compiles byte-identically
  whether its params are declared `(int l, int t, int r, int b)` or `(RECT rect)` — same stack
  slots, same `ret`. So an EXACT callee proves nothing about its own param shape. The caller
  does: `DDraw_DarkenRect` (0x401540) was EXACT as 4 ints, but its only caller (0x454900)
  passes the rect with the `sub esp,0x10` + field-store idiom at BOTH sites, not 4 pushes —
  remodeling the param as a by-value `RECT` kept the callee EXACT (`@@YADHHHH@Z` →
  `@@YADUtagRECT@@@Z`, same bytes) and took the caller from DIFF(105)/28 B short to EXACT.
  Check `get_xrefs_to` first: the remodel is only free when every call site agrees.

- **Decl order can kill a base pointer's register early enough for /Og to reuse it for the
  derived pointer (v516).** `GameNet_BroadcastRosterSnapshot` (0x440070): declaring
  `pSlot = pMgr->aProviderSlots` and the loop counter BEFORE the four header stores kept
  `pMgr` live in its own register (`edi`) beside `pSlot` (`esi`) — DIFF(139), 9 B long.
  Moving the two declarations AFTER the stores lets `pMgr`'s register die at the
  `add esi,0x518`, and the later `field_0x8`/grid-dim reads rematerialize as
  `[esi-0x510]`/`[esi-0x50c]`/`[esi-0x508]` — the original's exact one-register aliasing
  (DIFF 139 → 6, register shape paired). Same family as the store-order levers above:
  statement order is steerable, and here it prices a whole register.

- **Inserting an empty intermediate class into a polymorphic hierarchy costs 6 bytes in every
  derived ctor (v393).** MSVC stores the vptr once per polymorphic class in the chain, so a
  do-nothing `struct Partial : Base {}` used only to declare an extern method adds a
  `mov DWORD PTR [esi],<vtable>` to the derived ctor — `BuildToolButton`'s ctor regressed
  144 → 150 bytes until the partial was removed. The `…Partial` view pattern in
  `src/AnimEffectObj.h` is safe only where the ctor is not itself being matched; when it is,
  declare the method on the real base class instead.

- **The "any declaration added to `src/WidgetBase.h` rotates `DPlaySessionMgr.cpp`" rule
  (v325/v329) is narrower than recorded — MEASURE, don't assume (v393).** v393 added a
  `RepositionWithHotspot` method declaration to `WidgetBaseObj0x4784c8`, filled in signatures
  for two placeholder slots (`_v11` → `BlitAnimFrameMaybe`, `TryInvokeCallbackA`), retyped
  `pEffectSpawner`, and split a pad into three named fields — and `DPlaySessionMgr.cpp`
  (42+23/65, 6039 B), `WidgetBase.cpp` (376 B) and `WidgetPicker.cpp` (1728 B) were all
  byte-unchanged. The cheap check is one `tools/cc.sh` run per sensitive TU straight after the
  header edit; that is far cheaper than routing around the header with a partial view that
  carries its own codegen cost (see the bullet above).
  **v394 pushed the same measurement further and it held again**: changing
  `AnimDescRefObj0x477488::nAnimValueCache` from `int` to `unsigned int`, retyping
  `pEffectSpawner`, naming base slot 16 `OnKeyMaybe(unsigned int) -> char`, splitting a pad on
  `SelectedObjWidgetMaybe`, and retiring `src/Main.cpp`'s local 17-slot
  `ActiveTabWidgetVtblProbe` onto that real base declaration left `Main.cpp` (DIFF 3833/426),
  `WidgetPicker.cpp` (1728 B) and `WorldActionCursor.cpp` (1257 B) byte-for-byte unchanged.
  ⚠ The one thing a shared-header type change DOES reliably break is compilation elsewhere:
  `progress.py`'s per-file table showed `COMPILE FAILED` on two unrelated TUs (a local `int *`
  that pointed at the now-unsigned field, and a stale extern of the retired probe). Always
  grep the per-file table for `COMPILE FAILED` after touching a shared header — a failed TU
  silently zeroes its whole contribution to the EXACT/PARTIAL totals.

- **⭐ `char buf[N] = "lit";` COPIES the literal's bytes out of `.data` and zero-fills the rest,
  and it does so AT THE DECLARATION POINT, not at the top of the block (v401, two EXACT matches
  in `src/TutorialWnd.cpp`).** Two halves, both load-bearing:
  (a) **The tell.** A lone `mov al, ds:0x<data addr>` / `mov [buf], al` in front of a run of
  zero stores is `char buf[N] = "";` — a 1-byte copy of the pooled EMPTY string literal, NOT a
  plain zero-fill. `mov cx, word ptr ds:0x<addr>` / `mov [buf], cx` + zero-fill is the same
  thing for a 1-character literal (`= "("`). Do not read that byte load as a global being
  consulted; read the literal's address out of the image and it will be `""` or `"("`. Ghidra
  renders it as `buf[0] = s__004851d0[0];` with 19 separate `= '\0'` lines, which is faithful
  but reads like hand-written nonsense.
  (b) **The lever.** cl emits the copy+zero-fill exactly where the declaration is, and does NOT
  hoist it to the block top. So when the original's fill lands AFTER an intervening call, the
  declaration goes after that call in the source. `TutorialWnd::FormatNotifyToken` (0x44f750,
  582 B) went DIFF(401) → EXACT on one edit: moving `char szToken[20] = "(";` from above the
  `_itoa(...)` call to below it, in both arms. The same move fixed the corresponding block in
  `NotifyOrLaunch`. This is the mirror image of the v400 `strlen`-hoist lever — there you pull
  an intrinsic UP into a named local, here you push a declaration DOWN past a call.

- **⭐ VC5 coalesces two same-size locals onto ONE stack slot only when their scopes are
  DISJOINT SIBLINGS — a function-scope declaration plus an inner-block one does NOT coalesce
  (v401, `TutorialWnd::NotifyOrLaunch`/0x44f560).** That function builds the same 20-byte token
  buffer and 1024-byte ini-value buffer twice, in two arms, and the original's frame is exactly
  `0x414` = 20 + 1024 — i.e. ONE pair. Written as a function-scope pair plus a second buffer
  declared inside the arm, the frame came out 20 bytes too big and every `[esp+...]`
  displacement in the function shifted. Written as two sibling `{ }` blocks each declaring its
  own pair, cl packed them onto the same slots and the whole frame matched. **The frame size is
  the oracle here, and it is visible for free**: `sub esp,N` in the first instruction of the
  original vs. the candidate, long before any instruction-level diffing. VC5's stack packing is
  LEXICAL-scope-based, not live-range-based, so a variable whose scope merely extends past its
  last use still holds its slot.

- **A `bool` struct field is not interchangeable with `unsigned char` when the value copied in
  is already a byte (v401).** `TutorialWnd::bBoardScrollFlagAtNotify` modeled as `bool` turned
  the original's plain `mov [ebx+0x14d], dl` into `test dl,dl; setne cl; mov [ebx+0x14d], cl` —
  the implicit `unsigned char` → `bool` normalization. Model a field `unsigned char` unless
  something actually proves it is a `bool`; a straight byte-to-byte copy at any write site is
  proof that it is not. (Same family as the `bool`-vs-`unsigned char` return-type rule, and as
  the sete-materialized-predicate caveats above.)

- **A switch's search-tree compare pins the switch variable's SIGNEDNESS (v401).** When cl
  splits a sparse switch into a compare chain plus a jump table, the split compare is `ja` for
  an unsigned control expression and `jg` for a signed one — `cmp ecx,0x848; ja` in
  `MapNotifyToItemIndex` is what forced `TutorialWnd::lastNotifyCode` from `int` to
  `unsigned int`. One instruction, but it is free evidence about a struct field's real type,
  and unlike the jump-table range check (which is `ja` either way, because it is a two-sided
  range test) it is a genuine signal.
- **⭐ `MAKELPARAM(x, y)` is a DISTINCT codegen shape from `(y << 16) | (x & 0xffff)` — the
  macro is one instruction SHORTER, and that instruction is the whole residual (v402,
  `PlacementCursorMaybe::SetCursorCapture` 0x411dc0).** Packing a `POINT` back into a
  message's packed lParam by hand compiles to a lone `shl eax,0x10` — cl folds the `& 0xffff`
  away, because the OR's other operand is shifted left by 16 and it can prove those bits
  never survive. `MAKELPARAM` does NOT fold: its expansion casts each half to `WORD` first
  (`((WORD)(l)) | ((DWORD)((WORD)(h))) << 16`), so the low half keeps a real
  `and eax,0xffff` and the high half's own truncation is what gets folded into the shift —
  exactly the original's `and eax,0xffff` + `shl edx,0x10` pair. Source ORDER is irrelevant
  here (writing `x & 0xffff | y << 16` scored bit-identically to `y << 16 | x & 0xffff`); the
  MACRO is the lever. Took the function 16120 → **EXACT** on that one edit. Generalise:
  **when the residual against a Win32-shaped value is a single missing mask or shift, try the
  Win32 MACRO before touching the expression's shape** — `MAKELPARAM`/`MAKELONG`/
  `MAKEWPARAM` and the `LOWORD`/`HIWORD` pair all carry casts a hand-written expression
  doesn't.
- **⭐ Zeroing a small fixed array member is `memset(arr, 0, sizeof(arr))`, not element
  stores — the two are trivially distinguishable and the difference is large (v402,
  `PlacementCursorMaybe`'s ctor 0x410510, 90989 → 6002 on one edit).** *The tell*: cl's
  inline `memset` intrinsic for a small constant size materialises a dedicated zero register
  (`xor ecx,ecx`), copies the destination into a SCRATCH pointer (`mov edx, eax`), and stores
  through it at TINY displacements — `mov [edx],ecx; mov [edx+4],ecx; mov [edx+8],ecx`.
  Element-by-element source instead reuses whatever zero register is already live and stores
  at the members' FULL displacements off `this` (`mov [esi+0xf0],ebx; mov [esi+0xf4],ebx; …`).
  So: **a scratch-pointer copy in front of a run of small-displacement zero stores means
  `memset`; big `this`-relative displacements mean individual assignments.** The `lea` that
  feeds the scratch pointer is often shared with a following call that takes the array's
  address, which is what makes the pattern easy to misread as ordinary field writes.
- **⭐ A chain of `dec`/`sub` against ONE value is a `switch`, not an if/else ladder (v402,
  `PlacementCursorMaybe::UpdateCursorForAppStateMaybe` 0x411760, 66024 → EXACT).** cl compiles
  a small non-contiguous case set (here 1, 3, 4 + default) into a *subtract-and-test chain*
  that never re-reads the variable: `dec eax; je …; sub eax,2; je …; dec eax; je …` — each
  step biases the register by the gap to the next label. Written as `if (x == 1) … else if
  (x == 3) …`, cl instead emits an independent `cmp eax, <imm>` per arm. **Count the
  arithmetic: repeated `cmp reg, imm` = if/else ladder; `dec`/`sub reg, imm` threaded through
  the same register = `switch`.** A `case` whose body is empty (here `case 1: break;` — the
  front-end state the placement cursor deliberately ignores) is part of the chain and must be
  spelled out; it is not the same as excluding the value.
- **⭐ Comparing a `bool` member against a `char` parameter costs a `test`/`setne` widening
  pair at EVERY comparison — make the PARAMETER `bool` (v402,
  `PlacementCursorMaybe::SetCursorCapture` 0x411dc0).** `bReady == (bool)bCapture` with a
  `char` parameter compiles to `xor eax,eax; test bl,bl; setne al; cmp cl,al` — cl normalises
  the char to 0/1 before every compare, because a `bool` is guaranteed 0/1 and a `char` is
  not. The original's `cmp byte ptr [esi+0x24], bl` is a bare byte compare, which only happens
  when both sides are already the same 0/1-valued type. This is the mirror of the v401 lesson
  that a `bool` FIELD is not interchangeable with `unsigned char`: **whenever a `bool` member
  is compared or assigned against a function parameter, the parameter's own declared type is
  pinned too, and three `setne` pairs is a loud tell.** Callers passing `0`/`1` literals are
  unaffected. Worth 378 → 287 here before the branch-order fix below.
- **⭐ Which arm of an `if/else` the compiler makes the FALL-THROUGH is a source-order
  question, and on a large two-armed function it is worth hundreds of bytes (v402,
  `SetCursorCapture` 0x411dc0, 287 → 116).** cl lays the `if` body out inline and jumps to the
  `else`, so **whichever arm the source spells FIRST is the one that lands immediately after
  the test**. Diagnose it without reading a single instruction: find one anchor instruction
  unique to each arm (here `mov byte ptr [0x4855ae], 0`, the singleton-qualified flag write in
  the release path) and compare its OFFSET in the original against the candidate's. Anchor
  near the top in one and near the bottom in the other ⇒ the arms are swapped ⇒ invert the
  condition (`if (!bCapture) { release } else { acquire }` → `if (bCapture) { acquire } else
  { release }`). Same lever, smaller scale, for a guard whose cold path is an early `return`:
  `if (p == NULL) return false; <body>` puts the return inline, `if (p != NULL) { <body>;
  return x; } return false;` sinks it to the tail (v402, `PreloadPlacementSoundsMaybe`
  0x410750, DIFF(29) → EXACT).
- **⭐ Initialise-true-then-clear beats an `&&` fold when a function accumulates a success flag
  across several independent steps (v402, `PreloadPlacementSoundsMaybe` 0x410750, 227577 →
  70013).** Collecting three `if (p != NULL) { … }` results and folding them at the end
  (`bool ok = pA != NULL && pB != NULL && pC != NULL;`) forces cl to keep all three pointers
  LIVE to the fold point, which burns three callee-saved registers, spills the zero register,
  and re-materialises each pointer for its own `test`. The original instead sets `bool ok =
  true;` up front and adds an `else { ok = false; }` to each step, so every pointer dies at
  the end of its own block and ONE scratch register is reused throughout. *The tell*: a
  `mov byte ptr [esp+N], 1` before any of the work, plus `mov byte ptr [esp+N], <zero reg>`
  inside branches — versus a run of `test`/`je` on distinct registers just before the return.
- **⭐ An override must reuse the BASE's method name, so a Ghidra name for an overriding
  function is evidence about nothing (v402, `PlacementCursorMaybe` slot 10).** Ghidra had
  0x410840 as `ReleaseOwnedMaybe` and `docs/subsystems.md` called it `TickMaybe`; the vtable
  says it is slot 10 of `AnimDescRefObj0x477488`, whose base body (0x405c40) is
  `AdvanceAnimFrameMaybe` — so the C++ declaration has to be
  `virtual void AdvanceAnimFrameMaybe();` however unlike the body reads. **Dump the derived
  vtable against the base's before naming ANY method of a derived class**: the diff is the
  override set (here exactly two entries, slot 0's dtor and slot 10), it tells you how many
  NEW virtuals the class adds (here none — the tables are the same length), and it renames
  the overrides for you.
- **⭐⭐ A repeated sub-expression is CHEAPER to spell twice than to hoist into a local — the
  local pins a register and costs a shuffle (v403, `PlacementCursorMaybe::
  ClampToGridBoundsMaybe`, 37760 → EXACT).** `int nLimit = A - B; if (x > nLimit) x = nLimit;`
  and `if (x > A - B) x = A - B;` both compute `A - B` exactly ONCE under /O2 — CSE handles the
  second form — but they allocate registers differently. The named local forces cl to commit
  the value to one register for the whole live range, which here pushed the struct pointer out
  of `eax` into `ecx` and cost two `mov` shuffles per clamp (85 insns vs. the original's 83);
  writing the expression out at both sites let cl keep the pointer in `eax` and land the whole
  function byte-identical. **The tell is a two-instruction register-shuffle pair (`mov edx,
  eax` / `mov ecx, eax`) around a load that the original does not have, with your struct
  pointer living in a different register from the original's.** Reach for this whenever a
  clamp/min/max residual is otherwise structurally identical — and note it is the exact
  OPPOSITE of the usual instinct to hoist, so try both.
- **⭐ A `shr` where you emitted `sar` on a `>> 16` unpack means the FIELD is `unsigned`, not
  the shift (v403, `PlacementCursorMaybe`'s five `packed*` members).** A packed `LPARAM`-shaped
  member unpacked as `(v & 0xffff, v >> 16)` gives `sar` from an `int` field and `shr` from an
  `unsigned int` one. Fix the member's TYPE in the shared header rather than casting at the
  use site — the five packed positions here were all `int` and all wrong, and one function's
  disasm was enough to correct every consumer at once. Same family as the v401 `bool` vs
  `unsigned char` field lesson: the widening/shift instruction is a direct readout of the
  declared type.
- **⭐ `LOWORD(v)` is NOT interchangeable with `v & 0xffff` — the macro's `(WORD)` cast makes cl
  re-LOAD the low half from memory (v403).** `LOWORD` expands to `((WORD)((v) & 0xffff))`, and
  for a struct member cl services the `(WORD)` by emitting `xor edx,edx; mov dx, word ptr
  [esi+N]` — a fresh 16-bit load — instead of masking a register copy of the dword it already
  has. `HIWORD` has no such cost (the `shr` already clears the top half, so the cast folds
  away). So the mirror of v402's "try the Win32 macro" lever has a limit: `MAKELPARAM`/`HIWORD`
  are free, `LOWORD` is not. If the original shows `and reg,0xffff` on a register, the source
  wrote the mask.
- **⭐ An `unsigned char` ternary compared against 0 must be spelled with an `unsigned char`
  local, or cl widens it to `int` and costs a `mov` (v403,
  `SelectCursorTypeAutoCurveMaybe`).** `if ((p == NULL ? 0 : p->categoryByte) != 0)` makes the
  `0` literal an `int`, so the whole ternary is `int` and cl emits `mov dl,[ecx+8]; mov ecx,edx;
  test ecx,ecx`. Binding it first — `unsigned char nCategory = p == NULL ? 0 : p->categoryByte;
  if (nCategory != 0)` — gives the original's `mov cl,[ecx+8]; test cl,cl`. Note this is the
  opposite direction from the hoisting lesson above: here the local is what FIXES it, because
  what it pins is the TYPE, not a register.
- **⭐ `jg imm` vs `jge imm+1` are the same predicate but different bytes, and they tell you
  which relational operator the source used (v403).** `y < 0x11` compiles to `jge 0x11` when
  inverted; `y <= 0x10` compiles to `jg 0x10`. A decompiler normalizes both to `< 0x11`, so
  Ghidra's rendering is NOT evidence — read the original's own mnemonic and immediate. Three
  such flips (plus one fall-through inversion, and the `unsigned char` ternary above) were the
  entire residual on an otherwise-complete 226-instruction function.
- **⭐⭐ On an UNSIGNED counter, `> 0` and `!= 0` are the same predicate but different bytes, and
  the difference cascades (v404, `PlacementCursorMaybe::ReleaseHoverObjMaybe`/0x411580).**
  `if (n > 0)` on an `unsigned` gives `test reg,reg; jbe`; `if (n != 0)` gives `test reg,reg;
  je`. Same for the `unsigned char` case (`ja` vs `jne`). It holds for a MEMORY operand and an
  `unsigned short` field too, which is where v414 hit it: `pCantHave->nLiveInstanceCountMaybe > 0`
  gives `cmp word ptr [eax+0x158],si; jbe`, the `!= 0` spelling `… ; je` (0x4349d0). Note the
  same function's OTHER live-count test, three instructions earlier, really is `== 0` and really
  does emit `jne` — so do not "fix" a function to be internally consistent. This is the unsigned sibling of the
  `jg`/`jge` lesson above, and it is worth checking FIRST on any function that walks a count:
  three of these (two capacity/count guards and one mood floor) took a 164-instruction function
  from `total=154837` to `total=1109` in a single compile. cl does not canonicalise between the
  two, and Ghidra renders both as `!= 0`.
- **⭐⭐ The same `jbe`/`je` tell also distinguishes a real `for` loop from an
  `if (n != 0) { do ... while }` (v407, `PlacementCursorMaybe::RefreshFootprintHighlightMaybe`
  /0x410d20).** cl's zero-trip guard for `for (i = 0; i < n; i++)` with an unsigned bound is the
  NEGATION of `0 < n`, i.e. `test reg,reg; jbe` — whereas an explicit `if (n != 0)` wrapper
  around a `do/while` gives `test reg,reg; je`. Ghidra decompiles the `for` AS the `if`+`do`
  form, so transcribing its output literally reproduces the wrong guard every time. Two of these
  in one function (a collection walk and a linear search) went 162281 -> 116274 together. Rule:
  a `jbe`/`ja` guard in front of a loop means write a `for`, not a guarded `do/while`.
- **⭐ A virtual's RETURN-type SIGNEDNESS is visible in the CALLER, so a tail test pins the
  declaration (v407, `Obj0x477758::Count`).** `if (Count() > 0)` compiles to `test eax,eax; jle`
  when the method is declared `int` and to the original's `jbe` when it is declared
  `unsigned int` — one instruction byte, and the only evidence anywhere that the getter is
  unsigned (its own body, `mov eax,[ecx+0xc]; ret`, is signedness-free). Worth flipping on any
  count/size accessor whose caller compares with the wrong-signedness jump; changing it here was
  byte-neutral for the other consumer TU.
- **⭐⭐ A multi-dimensional array must be DECLARED multi-dimensional — hand-rolled row/cell
  pointers over a flat array do not reproduce cl's own strength reduction (v407,
  `CursorDesc::aFootprintOccupancyMaskMaybe`, 116274 -> 20788 in one compile).** The faithful-
  looking transcription of a doubly-nested walk — `unsigned char *pRow = arr;` ... `pCell += 7;`
  ... `pRow += 0x3f;` — is NOT what the original compiled from, even though it emits the same
  two induction pointers. Declaring the field `[13][9][7]` and simply writing `arr[x][y][0]`
  lets cl build those pointers itself, and it then keeps the row base LIVE IN A REGISTER across
  the outer loop and sinks its initialisation below the loop's zero-trip guard; the hand-rolled
  version spills it and hoists the init above the guard, because a source-level local must be
  initialised where it is written. The strides ARE the shape: a row stride of 63 with a cell
  stride of 7 over an 819-byte field is `[?][9][7]` with 819/63 == 13 rows. Generalise: whenever
  a residual shows the original holding an address in a register where you spill it, check
  whether you hand-rolled arithmetic that a real array declaration would have given you.
- **⭐ A local that caches a struct field across an if/else costs an eax/ecx role swap when the
  original re-reads the field at every use (v404, same function).** The mood adjust
  `if (p->n > 0) p->n = p->n - 1; else if (p->n <= 6) p->n = p->n + 1;` is byte-exact; hoisting
  `unsigned char n = p->n;` first is 9 bytes off across the whole block, because the named local
  keeps the PRECEDING call's return value pinned in eax past the point where the original has
  already recycled that register for the field read. Same family as v403's "spell it twice
  rather than hoist", and note it points the opposite way from the `unsigned char` TYPE-pinning
  lesson above — hoist to fix a type, don't hoist to fix a value.
- **⭐ A per-item pointer that a loop passes to several calls DOES want its own local, though
  (v404, same function).** The sorted-insert scan calls three virtuals per iteration with the
  same object; the original keeps it in a callee-saved register for the whole loop, which is what
  `T *p = this->field;` before the loop produces. Written as the member expression at each use, cl
  reloads it after every call. So: local for a value that must survive calls, no local for a value
  the original re-reads anyway. The disasm tells you which — look for a callee-saved register.
- **⭐ The same lever again, one rung up: a local caching a POINTER the function only dereferences
  costs register pressure too (v406, `PlacementCursorMaybe::CommitPendingRotateMaybe`).**
  `BigObj *pKind = pKindDesc;` followed by five `pKind->` reads scored 85279; deleting the local
  and writing plain `pKindDesc->` at every use scored 84604 in one compile. cl CSEs the pointer
  load into a single `mov eax,[esi+0x40]` either way, so the local buys nothing and only pins a
  register. Rule of thumb now: introduce a local ONLY when the value must survive a call
  (previous bullet); for anything the compiler can re-derive for free, spell the member.
- **When your candidate has MORE instructions but FEWER bytes than the original, look at the
  prologue before anything else (v406).** `insns 114/111` on a body 11 bytes SHORT was entirely
  `push ebx`/`push edi`/`pop edi`/`pop ebx`: cl had kept two extra values in callee-saved
  registers where the original spilled a struct temp and re-read the members. Four one-byte
  instructions read as a structural gap until you count them. The reverse of the same tell is
  worth knowing: a candidate that is shorter in BYTES almost always has fewer instructions too,
  so an instruction count going the other way is a register-pressure signal, not a missing block.
- **⭐⭐ Giving an unmodeled `void *pVtbl` field its real C++ `virtual` slots can byte-match an
  UNRELATED, already-transcribed function for free (v404, `PlacedObjRegistryMaybe`).** Promoting
  that one field to 19 virtual declarations (the layout is unchanged — the field WAS the vptr)
  closed `RoadVehicleActor::CanStandAtMaybe`/0x454050 (504 B) with no source edit whatsoever, and
  left the other two consumers of the shared header byte-identical. The standing fear that real
  virtuals "make the compiler want to emit a vtable" does NOT apply to a class that is only ever
  reached through an `extern` object: with no constructor in any TU, cl emits no vtable. Worth
  trying on any residual whose call sites are raw `(*(void(**)())( *(char**)p + N))(...)` slot
  math — that is lint class F, so the model is owed regardless.
- **A shared header's method declarations are a per-consumer lottery, so measure REPO-WIDE and
  keep the loser TU-local (v404).** Adding two method decls to `WorldBoardPartial`
  (28 consumers) cost `src/LocoBitmap.cpp` one whole EXACT function (-124 B) while changing
  nothing else; the same session added six method decls across four other shared headers with
  zero rotation. There is no way to predict which; `tools/progress.py`'s per-file table diff is
  the only oracle. When a header loses, put the declarations on a TU-local methods-only view and
  reach the object by CASTING the shared extern (`((View *)&g_worldBoard)->Method(...)`) rather
  than declaring a second extern — that keeps one symbol per TU, and records the measurement in
  the comment so the next session does not re-run it. **CONFIRMED TWICE and the remedy is now
  the default (v407).** A THIRD method declaration added to `WorldBoardPartial` cost
  `src/LocoBitmap.cpp` the SAME single function (`LocoBitmap::Fill`, -124 B) — the loss is
  intrinsic (the 2 residual bytes are the operand order of a commutative `imul`; `width *
  height` vs `height * width` and an explicit temp were both refuted), and declaration POSITION
  inside the class does not matter either (moving it to the end of the class scored identically).
  Moving that one declaration onto the TU-local view instead recovered the 124 B with the
  consuming function's own score completely unchanged. So: for `WorldBoardPartial` specifically,
  do not even try the shared header — go straight to the view.
- **⭐ A "codegen-risky, do not attempt" label on a MODELING fix is a hypothesis, not a finding —
  measure it before deferring again (v407, `CarKindDesc::aHeadingOffsetTableMaybe`).** Two
  sessions deferred folding six raw reads onto their real class because a header comment called
  the pass risky. The fold turned out to be byte-neutral in both affected functions — the one
  that was already an exact MATCH stayed exact across the rewrite. Pointer-arithmetic-to-named-
  field rewrites that do not change the computed ADDRESS almost never change codegen, because
  cl folds the constant either way; the cheap test (rewrite, recompile, diff) costs one compile
  and settles it. Reserve the risk label for changes that alter a shared header's DECLARATION
  set — those are the ones that genuinely rotate other TUs.

- **⭐⭐⭐ A compiler-generated COPY CONSTRUCTOR is the most precise struct-layout oracle in the
  binary — better than any allocation size, and better than reading field accesses.** `sizeof`
  only pins the total; access sites only pin the offsets you happen to have read. An implicit
  copy ctor renders EVERY member of the whole inheritance chain, in declaration order, at its
  EXACT width, and SKIPS every byte that is padding rather than a member. Find one (a
  `new T(src)` inlined at some call site) and diff it, and each disagreement names a specific
  modeling error. From one such function (0x412140, `new TilePlacedObj(src)`), four
  long-standing errors fell out in a single diff, each with its own signature:
  - **A member modeled one byte too wide** shows up as a wider move at the end of a block copy:
    `szCategoryName[12]` copies as dword+dword+DWORD, `[11]` as dword+dword+WORD+BYTE. Both
    classes are still 0x88 bytes, so nothing else could ever have told them apart.
  - **Two adjacent scalars that are really one small struct** show up as one move where you emit
    two. VC5 NEVER coalesces adjacent scalar members in a memberwise copy — the three separate
    byte moves at +0x8c/+0x8d/+0x8e in the very same copy prove it — so a single DWORD move at an
    offset you modeled as two `unsigned short`s means the original had a 4-byte struct there.
  - **A hand-added `padN` member that is really alignment padding** shows up as a move the
    original does not make at all. Padding is invisible to a copy ctor; a declared member never
    is. Corollary: the member AFTER the gap must have alignment ≥ the gap, which retypes it for
    you (`unsigned char pad0x90[20]` at +0x8f cannot be right if the original copies 20 bytes
    starting at +0x90 — it has to be a DWORD-aligned `int[5]`).
  - **A group of members you helpfully wrapped in a struct** shows up as `rep movsd` where the
    original walks the members individually. VC5 copies a POD struct member BITWISE (inline
    dword moves at ≤ 16 bytes, `rep movsd` above that) but walks a run of flat members one at a
    time. So `TileGraphSlot aGraphSlot[2]` → one `rep movsd` of 0x12, two separate
    `TileGraphSlot` members → two of 9, and six flat members → the original's six sub-copies.
    A struct wrapper is a modeling INVENTION unless the copy ctor recurses into it.
  - **⭐ A member assigned to the wrong CLASS in the hierarchy** — right offset, right width,
    right total size, wrong owner — shows up as a misplaced VPTR STORE, and nothing else in the
    binary can see it. The inlined chain emits each base's vptr store at the END of that base's
    OWN member copies, immediately before the next class's members begin, so **every vptr store
    is a class boundary marker**. v484: `DecorActorBase`'s 0x477f18 store lands between the
    +0xe8 and +0xec copies, which says the base ends at 0xec and the +0xec int opens the LEAF —
    where the header had carried it as the base's last member. Both spellings are
    layout-identical for every leaf, so only a memberwise copy could ever distinguish them.
    ⚠ The two instantiations reported the SAME error with OPPOSITE symptoms, which is the part
    that misleads: `WalkerActor` has a member after the misplaced one, so both vptr stores
    survived and it merely mis-SCHEDULED (an equal 165/165 instructions, DIFF(14)); its sibling
    `RoadVehicleActor` has nothing after it, so with the field in the base the two stores became
    adjacent and VC5 DEAD-STORED the first away, leaving that body one instruction SHORT
    (162/163). A "missing instruction" on one instantiation and a "scheduling swap" on another
    were one cause. When several instantiations of one template disagree, diff them against
    EACH OTHER before treating either as its own problem.
  - **A `char` vs `unsigned char` member** shows up wherever the field is USED, not copied:
    `xor eax,eax; mov al,[esi+N]` (zero-extend) is unsigned, `movsx eax,byte ptr [esi+N]` is
    signed. Cheap, unambiguous, and it costs 4 bytes a site — worth checking on any byte member
    a function reads arithmetically (v484, `AnimEffectObj0x477a90::bUnk0x94Maybe`).
  The same rule read forwards is a transcription lever: a whole-struct ASSIGNMENT is not the
  same code as spelling out its field assignments. `*pRect = pDesc->rcFootprint;` matched
  0x4583c0 exactly; the four equivalent `pRect->left = ...` lines cost 11 bytes.

- **⭐⭐ A vtable's slot 0 is NOT necessarily the destructor — read the table before believing a
  model that assumes it is.** MSVC places the destructor at the slot its DECLARATION occupies,
  so any virtual declared ahead of it in the class body pushes it down. The Obj0x477758 family
  carried a model with the dtor at slot 0 for many sessions; the real tables put the shared
  reserve helper there and the scalar deleting dtor at slot 1. Nothing caught it until a body
  needed to CALL slot 0. Correcting it (insert the real slot-0 virtual, move the dtor to 1, drop
  one placeholder so the count is unchanged) was byte-neutral across all three consuming TUs —
  a slot RESHUFFLE at constant declaration count is cheap; see the next bullet for what is not.

- **⭐⭐ The shared-header declaration-set lottery has a DIRECTION: pay only for declarations that
  carry a body.** Correcting the same class's ABI grew its declaration set by 7 (moving slots
  3..13 onto the base, plus re-declaring every derived override and one new slot). That rotated
  register allocation in `src/RoadVehicleActor.cpp` — a TU that never names the class, and reaches
  the header three levels down through `PlacementCursorMaybe.h` — and cost a 504-byte exact match.
  The fix is not to abandon the correction but to trim it: re-declaring an override that has NO
  body in this project buys nothing, because the slot index (and therefore every call site's
  codegen) comes from the base declaration. Keeping only the two overrides actually defined in
  the TU recovered the 504 bytes with the ABI fix fully intact. Generalise: when a shared-header
  fix costs an unrelated TU, bisect the CHANGE rather than reverting it — declaration count is
  the dial, and most of the declarations are usually optional.

- **A member function's declaration is part of that dial too, so a PARTIAL transcription can be
  net-negative and the right move is to drop it.** Adding two ordinary (non-virtual) member
  declarations to `TilePlacedObj.h` cost `src/PeerTrainNode.cpp` a 220-byte exact match, to land
  two functions that were themselves still residuals. Removing the pair restored it. Weigh a new
  declaration against the EXACT bytes it might rotate away, not against the transcription
  coverage it adds — and prefer to batch several members of one class into a single session so
  the declaration-set change is paid for once.

- **A 4-byte struct return goes through a hidden return-buffer pointer under VC5, not EAX** —
  `TilePlacedObj::GetFrontRowTilePosMaybe` (0x458310) builds both halves in a 4-byte stack temp
  and copies it out with one DWORD move through the buffer, exactly as the already-documented
  8-byte `POINT` returns do. Useful the other way round too: a hidden-buffer return of only 4
  bytes is itself evidence that the returned type is a STRUCT and not an `unsigned int`.

- **A bool temp materialized FULL-WIDTH but tested NARROW (`mov ebx,1` / `xor ebx,ebx` … `test
  bl,bl`) is a `bool`, and re-declaring it `int` makes things worse, not better.** The `int`
  spelling widens the test to `test ebx,ebx` as well; on 0x458350 that took the residual from 42
  bytes to 92. The remaining 5-byte gap (this emits the byte-width `mov bl,1` / `xor bl,bl` pair)
  is a materialization-width tie-break with no source spelling found yet — do not reach for `int`.

- **`a - b + c` can be reassociated below the source level, and parentheses cannot stop it.**
  On 16-bit unsigned operands cl evaluated `rows - ySteps + pos.wPosY` as `(pos.wPosY + rows) -
  ySteps`, one instruction more than the original's `(rows - ySteps) + pos.wPosY`. Parentheses
  are a no-op (the expression already parses that way); an explicit `(unsigned short)` cast
  around the difference and hoisting it into its own named local were both refuted with an
  identical score. Park it rather than grinding — the association is chosen after parsing.

- **⭐⭐⭐ A byte-match CANNOT verify which external function a call resolves to — the relocation
  is MASKED — so any type decision encoded purely in an overload choice is UNTESTED by an EXACT
  score.** `tools/match.py` masks reloc bytes (it has to: the `.obj` holds a symbol reference
  where the image holds a resolved address), so a `call rel32` to a CRT function compares equal
  no matter WHICH function it names. That makes every `operator>>` / `operator<<` overload
  interchangeable to the scorer, and with them the TYPES of the operands they pin. Hit for real
  in v411: `BigObjSeqRecordMaybe`'s +0x10/+0x18/+0x24 were modeled as `float` from v332 onward on
  the strength of `Obj0x4779e0::ParseSeqRecordMaybe` (0x41f2b0) being EXACT with
  `*pStream >> (float &)pRec[4]` — but the original calls **0x464bc0**, which disassembles as
  `istream::operator>>(short &)` (it clamps to [-0x8000, 0x7fff] and stores a `WORD`), and
  changing the source to `(short &)` KEEPS the EXACT match while flipping the emitted relocation
  from `??5istream@@QAEAAV0@AAM@Z` to `AAF@Z`. Three fields were the wrong type and three struct
  offsets carried phantom 4-byte extents for eight sessions. **The check, whenever a call's
  identity encodes a type:** read the callee ADDRESS out of the ORIGINAL's disasm, disassemble
  THAT address to see what it actually is, and cross-check against a consumer that reads the same
  field (here 0x458820's `movsx ecx, WORD PTR [ecx+0x10]` and 0x4588b0's `mov ax, WORD PTR
  [eax+0x24]` both independently say 16-bit). `llvm-objdump -r <obj>` prints the candidate's own
  relocation symbols and is the fastest way to see which overload YOUR source picked.

  **v412 automated that check repo-wide as `tools/lint_calls.py`, and it found exactly one more
  wrong overload plus a second, larger class of finding.** The tool pairs each marker to its
  COMDAT, and at every call relocation whose preceding byte is `0xE8` it decodes the ORIGINAL's
  own rel32 to learn the address the ORIGINAL calls — building a `target address -> mangled
  symbol our source emitted` map across all EXACT functions. Two things fall out of it:
  * **A parameter-letter audit against the six `istream::operator>>` overloads the image uses**,
    whose real signatures were pinned by disassembling each callee: `0x4646c0` = SIGNED 32-bit
    (`strtol`, stores a DWORD; `int&`/`long&` both land here), `0x464f70` = **UNSIGNED** 32-bit
    (`strtoul`), `0x464bc0` = `short&`, `0x464750` = `unsigned short&`, `0x4649f0` = `char*`,
    `0x464ef0` = `char&`. The finding: `SoundBankEntry::ParseSettingsLine` (0x448c90) emitted
    `operator>>(int&)` for calls to BOTH `0x4646c0` and `0x464f70` — impossible, since the
    linker maps one symbol to one address. `SoundBankEntry::nMaxInstances` is `unsigned int`,
    not `int` (corroborated by the `(unsigned int)` cast its only consumer needed, now deleted).
  * **A CONFLICT report — one original address reached under two different mangled symbols
    anywhere in the repo.** This is the general form of the same trap and catches disagreements
    a single TU can never see: differing return type, differing owning class, and differing
    CALLING CONVENTION. Most hits are the known deliberate duplicate views, but read every one.
  **Corollary worth internalizing: an EXACT score validates the bytes of the function you are
  looking at, and nothing about the DECLARATIONS it was compiled against.** Overload choice,
  calling convention, `this`-ness and return type are all invisible to it.

- **⭐⭐ "It never reads `this`" makes a `__thiscall` member and a free `__stdcall` function
  byte-identical on BOTH sides of the call — use `tools/ecxcheck.py`, not the callee's body, to
  tell them apart.** A member whose body ignores `this` clobbers ecx in its prologue, so Ghidra
  types it `__stdcall` and the decompiled body offers no evidence either way; the callee is
  simply the wrong place to look. The evidence is at the CALL SITES: if the original establishes
  ecx before every call, it is a member. v412 settled all thirteen `PostBag_*` functions this way
  (25/25 sites load `mov ecx,[g_pPostBagCache]`). Two traps in doing it:
  * **Adjacency is the wrong test** — VC5 schedules argument pushes AFTER the ecx load, so
    `mov ecx,[g]` is often several instructions back. Walk the basic-block tail (stop at the
    previous call/branch) instead. An adjacency test reported 5/23 on a function that is really
    21/23.
  * **When backward-disassembling to a known instruction boundary, take the LARGEST window that
    decodes exactly onto the target, not the smallest.** x86 is self-synchronizing, so a short
    misaligned decode will also land on the boundary — and a window that starts after the ecx
    load reports a false "elided". This bug alone flipped the verdict on three of five
    already-confirmed members.
  * A genuinely elided site is NOT counter-evidence: an in-class caller whose own `this` is
    still live in ecx from the prologue correctly emits nothing.

- **⭐⭐⭐ WHAT THE DIAL ACTUALLY IS, and the rule that keeps it honest (imported from Yodecomp's
  v95/v96 measurements, 2026-07-27; that project A/B-tested the mechanism directly with a
  dedicated sweep tool, we have only ever observed it indirectly here as a "declaration-count
  parity bit").** The dial is a **pure file-scope SYMBOL COUNT visible to the TU** — not
  "declaration parity", not line count, not identifier length. Measured consequences:
  - an **`enum` is `1 + <enumerator count>` symbols**, so a mis-transcribed enum is a *quantified*
    dial error and even never-referenced enumerators are dial-active. The same constants written
    as `#define` cost **nothing** — macros never enter the symbol table. ⇒ inside a TU that is
    byte-matching, prefer `#define` over `enum` for shared constants.
  - **identifier LENGTH is irrelevant**; renaming is free (which is why this repo's naming sweeps
    are byte-neutral, and why a rename that ISN'T byte-neutral means a real bug — see the
    member/local shadowing rule in CLAUDE.md).
  - a **new `#include` FILE in a byte-matched TU's chain costs codegen even when the file is
    EMPTY**, and **`sizeof(T)` substituted for the equal literal is a dial input too** despite
    folding to the same constant. Those two are SEPARATE mechanisms from the symbol count, not
    instances of it. ⇒ new shared constants belong at the **tail of an already-included header**,
    never in a new file.
  - ⭐ **THE DIAL IS AN INSTRUMENT, NOT A KNOB.** The exact count is steerable by ambient
    declaration state, so it can be gamed — don't. **A free gain (+N/−0) is the fingerprint of a
    recovered fact; a TRADE (+N/−N) is the fingerprint of padding.** Legitimate: find a REAL
    missing declaration and add it, letting the dial move as a consequence. Forbidden: adding
    filler declarations, padding an enum, or keeping a wrong field count because it scores better
    — that encodes a NUMBER, not a fact, and poisons the source as a reference.
  - ⭐ **Corollary, and the reason this matters right now: do not CHASE dial-related exacts before
    the repo is fully transcribed.** Every dial position is provisional while headers are still
    growing, so a match bought with a dial move is a match that a later real declaration will take
    back. v483's `~PostBagCacheBundle` attempt is the worked example: the body is EXACT on the
    first compile, but the two includes it needs traded +2/−3 across four TUs for a net −190 B
    (see the ⛔ block in `src/PostBag.h`). Transcribe first; re-baseline the dial once the header
    set is real.

- **⭐⭐ A "declaration-count dial" residual is a signal to go hunting for REDUNDANT declarations
  in the included headers — not only to accept the model and park the function.** `MailWnd::
  RefreshClientClipRect` (0x42f8b0, 1332 B) was parked v399–v411 after growing `TutorialWnd`'s
  method block flipped it to DIFF(150), with dummy counts at 8/9/10 all reproducing the same
  diff. v399 read that threshold behaviour as proof the earlier EXACT had been accidental and a
  real residual was being unmasked. The mechanism was right and the conclusion wrong: the dial
  is the size/shape of the TU's ENTIRE included declaration set, and the excess declarations
  were in a different header (`src/PostBag.h` carried nine duplicates — five functions declared
  both as members and as free `extern __stdcall` twins, plus three whose parameter types
  disagreed between the two spellings). Deleting them put the function back to EXACT with zero
  change to `src/MailWnd.cpp`. **Before accepting "the correct model costs this byte-match",
  audit the included headers for declarations that are redundant rather than merely numerous** —
  a duplicate declaration is free to delete and costs nothing to be wrong about.

- **⚠ Reconciling a type that two duplicate declarations disagree on is a MEASUREMENT, not a
  judgement call.** When `src/PostBag.h`'s member and free spellings of the same three functions
  disagreed (`unsigned short` vs `unsigned int` for an id parameter), picking the definition's
  `unsigned int` — the spelling that was already byte-matching in its own TU — cost `MailWnd.cpp`
  869 bytes and 2 functions, because the CALLERS were the side pinned by the other type.
  `unsigned short` on both sides is correct. Score both candidates repo-wide with
  `tools/progress.py` before keeping either; the definition's own TU is not the only vote.

- **⭐⭐ A guard block that appears in BOTH arms of an if/else is often REAL SOURCE, not compiler
  tail duplication — try spelling it twice before writing it off.** `TilePlacedObj::
  ApplySeqRecordChangeMaybe` (0x458820) sat at DIFF 43 with a single post-merge copy of a
  two-test early-return guard; moving that guard verbatim into both the `if` and the `else` took
  it to **EXACT (132 B) in one compile**. The duplicated-looking block is cheap to test and the
  test is decisive. ⚠ It does NOT always work — the same session's 0x458430 has a genuinely
  compiler-duplicated ~112-byte block that cl cross-jumps straight back together when the source
  spells it twice (the score got WORSE, DIFF 495 -> 532). The discriminator seems to be the
  block's TAIL: 0x458820's copies each end in `return`, so there is nothing to merge, whereas
  0x458430's copies fall into a shared call. **Duplicated blocks ending in distinct exits =
  probably source; duplicated blocks converging on a shared tail = probably the compiler.**

- **⭐⭐ A 3-way dispatch whose DEFAULT arm falls through in the middle while the named cases sit
  out of line at the tail is a `switch`, not an if / else-if / else chain.** An if-chain over the
  same three constants compiles the bodies INLINE in source order (`cmp; jne` past each), so the
  first case is the fall-through; the `switch` inverts it to `cmp; je <out-of-line body>` per case
  with the default falling through between the tests. On `TilePlacedObj::SpawnSeqRecordEffectMaybe`
  (0x4588b0) that one rewrite fixed the whole block layout (the original tests `'S'` then `'W'`,
  bodies at the tail, object-relative default in the middle). Cheap to check and it re-aligns
  every downstream row of the dump, so try it BEFORE reading the residual as register noise.

- **A pointer parameter modeled as `int` is not free just because it byte-matches — retyping it
  can be a zero-cost cleanup, so measure instead of assuming a declaration change is risky.**
  `WorldActionCursor::SelectDecorObjAndDispatchModeMaybe`'s parameter was declared `int` with the
  body's first line casting it straight to `AnimDescRefObj0x477488 *`; retyping the declaration
  (and `SelectedObjWidgetMaybe::pSelectedObjMaybe` with it) scored **identically repo-wide**
  (89453 B / 396 funcs both ways) while deleting three `(int)`-cast-of-pointer sites across three
  TUs. The shared-header declaration lottery is real, but it is about the declaration SET's size
  and shape — swapping one parameter's type in place changes neither.

- **⛔⛔ Forward-declaring a `struct` as `class` (or vice versa) is NOT cosmetic under VC5: the
  LAST-SEEN tag decides the whole type's DEFAULT ACCESS, so a stray `class Foo;` after `struct Foo
  {...}` turns every member private and the TU stops compiling (v414).** Loco declares
  `struct LocoBitmap` in `src/LocoBitmap.h` but four headers forward-declared it, two of them as
  `class`. Adding `#include "LocoBitmap.h"` to a TU that also saw a `class LocoBitmap;` produced
  `warning C4099: type name first seen using 'struct' now seen using 'class'` — and then, in three
  OTHER TUs, a cascade of `error C2248: cannot access private member declared in class
  'LocoBitmap'` at every member use. Modern compilers treat C4099 as harmless; cl 11.00 does not.
  **Two things make this dangerous out of proportion to the fix.** (1) The break lands in TUs you
  never edited, so a per-file `tools/cc.sh` run on the file you touched is all green — only a full
  `tools/progress.py` shows it, and it shows up as `COMPILE FAILED` rows worth thousands of exact
  bytes (here −9125 B / −26 funcs). (2) The trigger is INCLUDE ORDER, so which TUs break depends
  on which header happens to come last. Fix: make every forward declaration use the same tag as
  the definition, repo-wide, in one commit — `grep -rn "^\(class\|struct\) <Name>;" src/` — and
  re-run `progress.py`, not just the TU you were working on. Treat any C4099 as an error to fix
  immediately, never a warning to live with.

- **⭐⭐⭐ On a function with a by-value STRUCT parameter and no prototype set, Ghidra's decompile
  can invent a plausible-looking DEAD-CODE BUG out of nothing — read the disassembly instead of
  believing it (v415, `DecorObjMgrMaybe::TestRectAgainstAllActorsMaybe` 0x435020, 469 B, taken
  EXACT first try from raw `objdump`).** Ghidra mis-frames the stack on these, and the specific
  failure mode is that it ALIASES a small scalar local onto one of the frame's `RECT`s — here the
  `int nResult` slot at `[esp+0x10]` became `local_24.left` — so every store to the result
  disappears into rect-field assignments and the tail renders as a bare `return 0`. Two prior
  sessions read that as an engine bug ("discards every mask result, all 469 bytes are dead work"),
  wrote it into `docs/engine-bugs.md`, propagated it into `src/DecorObjMgrMaybe.h`'s decl comment,
  and — worst — used it in `docs/subsystems.md` to declare a whole `else` arm of a DIFFERENT
  already-transcribed function (`NameAnchorMaybe::AdvanceAlongTrackMaybe`'s level-crossing case)
  dead code, which then got baked into `src/NameAnchorMaybe.cpp` as a wrong TU-local `extern` and
  two wrong comments. The disassembly says `mov [esp+0x10],7` / `mov [esp+0x10],8` /
  `mov eax,[esp+0x10]` in plain sight. **A "this whole function is dead work" conclusion drawn
  from a decompile is a red flag, not a finding** — shipped 1998 game code rarely contains a
  469-byte no-op, and the cheap disproof (three `grep`s of an `objdump` dump for stores to the
  returned slot) costs a minute. Generalises the v414 lesson one step: it is not only that the
  decompile is unusable on these, it is that it fails in a DIRECTED way that reads as a discovery.

- **⭐⭐ `switch` with only three case labels gets a COMPARISON CHAIN; adding the fourth (even an
  empty `case 0: break;`) is what buys the JUMP TABLE (v415, `OnArriveAtDestinationMaybe`
  0x434100).** Symptom: the original has `cmp eax,3` / `ja default` / `jmp [eax*4+<table>]` with a
  4-entry table whose slot 0 points at the default label, and your compile has
  `dec eax`/`je`/`dec eax`/`je`/`dec eax`/`jne` instead. Do NOT read the table's slot-0-is-default
  entry as proof that `case 0` is absent — MSVC bases the table at 0 rather than subtracting the
  minimum, so that slot is padding either way. Count the LABELS, not the table entries: three live
  cases plus a fourth label is the threshold. Worth 38000 points here.

- **⭐⭐ A tail that must be reached from some cases but skipped by others is written by
  DUPLICATING it into each case, not by a `goto` past it — the `goto` form costs the jump table
  (v415, same function).** The natural-looking single-copy shape
  (`switch { case 2: … goto done; } tail; done:`) forces the switch's cases to end in explicit
  jumps and VC5 then abandons the table for a compare chain (390122 vs 214739). Writing the tail
  line into all three cases and letting VC5 cross-jump the copies back into one block reproduces
  the original exactly. Same family as the v376 duplicated-tail lever, one level up: there the
  duplication bought a hoisted push slot, here it buys the dispatch form itself.

- **⭐⭐ An inline predicate returning a widened byte must widen in ONE step to its real type, and
  its null guard must be written POSITIVELY — both are visible in the diff and both are worth
  double-digit instruction counts (v415, `FindFootprintTileOfCategoryMaybe` 0x433ec0, DIFF 220536
  → 4).** (1) `return p == 0 ? 0 : GetByteThing(p);` in an `unsigned short`-returning helper
  promotes the ternary to `int` and emits `and ecx,0xff`; the two-`return` form
  (`if (p != 0) { return GetByteThing(p); } return 0;`) converts the byte straight to 16 bits and
  emits the original's `movzx cx,cl`. **`and reg,0xff` where the original has `movzx cx,cl` is a
  TYPE-WIDTH tell, not noise.** (2) With the guard written `if (p == 0) return 0;` the zero arm is
  laid out INLINE and the two remaining legs share one merged widening; written `if (p != 0)
  return …;` the zero arm goes out of line at the bottom, which forces both legs to jump to the
  join and VC5 then duplicates the widening into each — a 12-instruction structural gap that
  reads like scheduling noise until you flip the test.

- **⭐⭐ Read the switch's BYTE INDEX TABLE, not just its DWORD jump table — a case whose body is
  identical to `default`'s still gets its own index slot, and that is the only surviving evidence
  the case label exists (v416, `UIResources::Locale_DetectLanguage`/0x4463c0, DIFF 26 → EXACT).**
  A two-level VC5 switch is `movzx ecx, byte [eax + <byteTable>]` then `jmp [ecx*4 + <dwordTable>]`.
  When two index slots hold the SAME block address, they are two distinct case labels whose bodies
  got cross-jumped onto one block — here `case LANG_ENGLISH` (slot 3) and `default` (slot 10) both
  point at the `m_nLocaleId = 3` block. Two further consequences: (1) an explicit case that merely
  duplicates the default's body is invisible in the decompiler's output and in the disassembly's
  code, so it can only be recovered from the index table; (2) it must be written as its OWN
  `break`-terminated body — the natural-looking `case LANG_ENGLISH: default:` sharing one body
  collapses the two slots into one and the jump table comes out exactly one DWORD short. That
  4-byte length shortfall against a known-good `--len` is the tell.
  Related to #16b (a too-narrow jump table = a missing label), and the same "count the LABELS, not
  the table entries" discipline, pushed one level down into the byte table.

- **⭐ Two calls that differ only in one argument are written as TWO WHOLE CALLS in an if/else, not
  as one call with a ternary in that argument (v416, `DecorActorBase`'s ctor/0x433a20, DIFF 66 →
  EXACT).** VC5 cross-jumps the identical call tails back together and hoists only the leading
  constant push above the branch, so the original pushes the SHARED arguments twice — once per arm
  — but calls the function once. The ternary form instead evaluates the whole differing argument
  before starting the push sequence, so every shared push appears once, after the branch. **The
  tell is a shared argument's `lea`/`push` appearing TWICE against a single `call`**: that is a
  cross-jumped pair of full calls, not one call with a conditional operand. Same family as the
  v415 duplicated-tail lever and the v376 one.

- **`>= N` and `> N-1` are NOT the same to VC5 — it emits the compare against the literal exactly
  as written (v416, `DecorObjMgrMaybe::TickCategory7OnlyMaybe`/0x434870, DIFF 2).** `x >= 2`
  gives `cmp …,2; jb`, `x > 1` gives `cmp …,1; jbe`. Two bytes apart and trivially bisected, but
  worth knowing before hunting for something structural on a tiny function.

- **⚠ A PURE-DECLARATION header edit is a real lever on OTHER TUs' allocator state, in BOTH
  directions (v416).** Adding two `virtual` declarations to `PlacedObjRegistryMaybe` in
  `src/DecorObjMgrMaybe.h` — no field changes, no layout change, no new code — flipped
  `RoadVehicleActor::CanStandAtMaybe` (0x454050, 504 B) from an EFFECTIVE residual back to EXACT,
  in a TU that was not otherwise touched. It is the same dial that BROKE that function in v402
  (`PlacementCursorMaybe.h` growing from a padded view to the real class), turned one notch
  further. Two practical consequences: a full `progress.py` after any header change is mandatory
  in both directions (the win is as easy to miss as the regression), and a residual whose autopsy
  says "no source shape behind it" may still be curable by an unrelated declaration-count change
  — so re-check the parked list opportunistically rather than treating those rows as closed.

- **⭐ The pure-declaration dial is a COUNT, not an identity or a position (v418, measured three
  ways on `src/WorldBoardMaybe.h`).** Adding ONE member-function declaration
  (`TestOrClearFootprintMaybe`) cost `src/WalkerActor.cpp`'s `UpdateFacingFromStepDeltaMaybe`
  (0x4331b0, 112 B) its EXACT. Moving that same declaration to the very end of the class changed
  nothing. DELETING one unrelated, *declared-only* method from the class restored the match
  exactly. So a shared header carries a fixed declaration BUDGET for the TUs that include it: to
  add one, retire one — or fix the casualty at its own source (see the next bullet, which is what
  was actually done, since every other declaration in that header turned out to have a live
  caller). This sharpens v416's "declarations are a lever" note into something directly
  actionable: when a header edit costs an EXACT elsewhere, first check whether the class has a
  declared-only method you can park.

- **A casualty of a declaration-count rotation is often curable IN THE CASUALTY, and cheaply
  (v418).** `UpdateFacingFromStepDeltaMaybe` came back at DIFF(10) with `insns 58/58` — pure
  register-role noise — and went straight to EXACT by swapping the DECLARATION ORDER of its two
  locals (`int nDeltaY` before `int nDeltaX`, while the test still reads X then Y). The
  declaration order decides which value VC5 loads first and therefore which of the two inlined
  `abs()` expansions gets `esi` vs `edi`. Note the near-miss: rewriting the COMPARISON instead
  (`abs(nDeltaY) < abs(nDeltaX)`) only got to DIFF(2) — the dial is the LOAD order, not the
  compare order. Prefer this over reverting the header edit: it keeps the model improvement and
  the byte-match.

- **⭐ A named local for a value used at two distant sites can cost you a re-computation the
  original really does perform (v418, `WorldBoardMaybe::FindNearestObjOfCategoryMaybe`/0x457ce0,
  asmscore 217216 → 25501 and the instruction count closing to 327/327).** The four edges of a
  ring scan each need bounds derived from the ring radius. Hoisting them into
  `int left/top/right/bottom` lets VC5 common-subexpression the `max(left, 0)` that the FIRST and
  LAST edges both need, so the last edge reloads a stale copy from a spill slot; the original
  recomputes those five instructions. Writing the bounds as INLINE EXPRESSIONS at each use fixes
  it — and VC5 still CSEs the cheap `nCol +/- ring` subtractions itself, which is exactly where
  the original's own spill slots come from. **The tell is a `mov` from a spill slot where the
  original has an arithmetic sequence, in a function that is otherwise instruction-aligned.**

- **Two `short` locals that the original packs into ONE 4-byte stack slot are a `POINTS`, not two
  `short`s (v418, same function, the last 4 bytes of its residual).** Separate `short` locals each
  get their own DWORD spill slot; a single 4-byte object packs, and that is the only way to
  reproduce a pair of `mov WORD PTR [esp+0x44]` / `[esp+0x46]` stores into what is otherwise the
  dead 4th parameter's home slot. `POINTS` (windef.h's `SHORT x, y` pair) is already in scope
  wherever `windows.h` is, and it is what a 1998 Win32 codebase would reach for. **DECLARATION
  ORDER matters too**: the pair only lands in the reused parameter slot when it is declared AFTER
  the function's other locals. Same family as the bullet above — a plain `short[2]` scores
  identically, so the packing is the real signal and `POINTS` is the idiomatic spelling of it.

- **Prefer `arr[i]` repeated over a cached `T *p = &arr[i]` when the original folds the index
  (v418, `WorldBoardMaybe::PlaceObject`/0x4550c0, asmscore 442255 → 313881).** Writing
  `BoardTileCellMaybe *pCell = &aTileCellsMaybe[idx];` and then using `pCell->slots[n]` /
  `pCell->count` forces VC5 to materialise the cell address, then offset from it twice. Indexing
  `aTileCellsMaybe[idx]` at each use instead lets it fold the cell index and the slot index into a
  single scaled address (`[esi + idx*4 + 0x48]`) exactly as the original does, and compute the
  count byte's address independently. Also from the same function: a high-water-mark update is
  spelled `slot >= (char)count`, not `(char)count <= slot` — VC5 emits the operands in source
  order, so the two forms differ by a `jl`/`jg` and everything downstream (another 28k).

- **A count/index byte's SIGNEDNESS in the struct is load-bearing, and getting it wrong turns a
  cheap 8-bit down-counting loop into a spilled two-counter loop (v419,
  `WorldBoardMaybe::FlushQueuedBigObjMaybe`/0x455ab0, asmscore 433375 → 234224 from this one
  change).** The board cell's plane counts were modeled `unsigned char`; the loops that scan a
  plane use a `char` slot index running DOWN while `>= 0`, and compare that index against the
  count. With the field unsigned, `count == slot` is a mixed-signedness comparison, so VC5 cannot
  narrow it and emits `and ecx,0xff` / 32-bit `cmp` — and, having widened the index's live range,
  it then spills the loop counter and adds a SECOND trip counter with a `jne` at the bottom
  instead of the original's `dec bl` / `test bl,bl` / `jge`. **The tells are (a) a 32-bit compare
  where the original has an 8-bit one, and (b) an extra `mov [esp+N],imm` initialising a trip
  count the original does not have.** The fix is in the header, not the function: a count byte
  whose resting "empty" value is -1 is a signed `char`. Sync it to the Ghidra struct too.

- **The shared-header declaration-COUNT rotation is NOT specific to one header — `CursorDesc.h`
  has it as well (v419).** Adding a single member-function declaration to `BigObj` cost
  `src/WalkerActor.cpp`'s `UpdateFacingFromStepDeltaMaybe` and `src/Obj0x4779e0.cpp`'s
  `ParseEntryExitMaybe` their EXACT matches at once, each flipping to a MIRRORED operand-load
  order in a subtraction (`sub ecx,eax` → `sub eax,ecx`, with the two byte loads swapping which
  register they land in). Removing the one declaration restored both exactly. When the casualty
  list is longer than one, prefer the escape hatch over curing each: declare the method on a
  **TU-local methods-only struct deriving from the real class** (`struct XView0xNNNN : X { ...
  };`) and cast at the single call site. The derived-class pointer cast is a no-op, the call is
  still `call <addr>` with the same `this`, the symbol is an unresolved extern either way (so
  `match.py` masks it), and no other TU sees a changed declaration set. This is the same pattern
  `src/WorldBoardMaybe.cpp` already uses for `WorldBoardDirtyTileView0x456150` /
  `PeerTrainSlotQueueView0x456150`.

- **⭐⭐ A declaration-count knife-edge is a SUNK COST once spent — audit the whole repo for other
  work that was parked on the SAME casualty, because it all becomes free at once (v428).**
  `src/MailWnd.cpp`'s `RefreshClientClipRect` (0x42f8b0, 1332 B) had been the single documented
  price of THREE independent parked items: `src/AppWindow.h`'s MEASURED DIAL (v422/v423),
  the `PostBagCacheBundle`/`PostBagFileCache` virtual-dtor conversion (v426), and — discovered in
  v428 — any growth of `src/EditCardWnd.h`. Each had been individually declined because each alone
  netted negative (−1642, −501, −459). Paying the 1332 ONCE, for the cheapest of them, made the
  other two free and turned the combined trade into **+1013 B / +2 EXACT functions**. The lesson is
  procedural, not about codegen: when a residual's parked reason is "it costs function X", **grep
  the repo for every other note that also names X** before pricing the trade, and price the BUNDLE.
  Corollaries proven the same session: (a) the trigger header set is almost always wider than any
  one note claims — establish it with a neutral dummy declaration (`void ZZProbeUnrelated();`)
  in each candidate header rather than trusting the historical list; (b) `virtual` is irrelevant,
  the dial counts DECLARATIONS (a non-virtual dummy costs exactly the same); (c) declaration
  POSITION within the header is irrelevant; and (d) **a stale price is a hypothesis, not a
  measurement** — v423's carefully-measured second casualty (`src/WorldBoardMaybe.cpp`'s
  `FindNearestObjOfCategoryMaybe`, 951 B) simply did not recur at the v428 dial position, so the
  realised cost was 1332, not 2283.

- **⭐ `delete p` where `p` is a `void *` compiles to a bare `call operator delete` + `add esp,4`,
  NOT the vtable dispatch — so a singleton modeled as `extern void *g_pFoo;` silently mis-compiles
  its own teardown (v428).** The original's shape for deleting a polymorphic object is
  `mov edx,[ecx]; push 1; call [edx]` (the `??_G` scalar-deleting-dtor thunk, arg 1 = "also free
  the memory"). A `void *` cannot express that, and the resulting residual reads as ONE extra
  instruction plus a long cascade of `eax`/`edx` alternation in every FOLLOWING delete — easy to
  misread as ordinary register noise. `AppWindow::SaveWindowAndCleanExit` (0x4077a0) went from
  DIFF(138) straight to EXACT on the single edit of typing `g_pGameNetMsgQueueLock` as
  `LockableMaybe *`. Keep the shared header light with a forward declaration
  (`class LockableMaybe;` + `extern LockableMaybe *g_pFoo;`) and include the complete type only in
  the TU that actually deletes it — that gets the correct codegen without growing the header's
  declaration set for every other consumer (see the knife-edge bullet above).

- **⭐ A 16-bit argument is pushed WITHOUT zero-extension when the FORMAL PARAMETER is 16-bit —
  `mov cx,[r+N]; push ecx` with garbage in the high half — and that is a reliable way to read a
  callee's parameter type off its CALL SITES (v428).** If the parameter were `unsigned int`, VC5
  must widen, emitting an extra `xor ecx,ecx` (or routing through another register and paying a
  `mov`). `EditCardWnd::OnRButtonDown` (0x41ca80) was one `xor ecx,ecx` away from EXACT purely
  because `PostBagCacheBundle::PostBag_DeleteAttachmentFiles` was modeled as taking
  `unsigned int nId`; all SIX original call sites across two TUs push a bare 16-bit load, which
  settles it. ⚠ Do NOT let the CALLEE's own body talk you out of it: reading a 16-bit parameter
  back out of its (4-byte-aligned) stack slot compiles as `mov esi,dword ptr [esp+N]; and
  esi,0xffff`, not `movzx` — VC5 over-reads the slot and masks, because it can. That `and`
  looks exactly like a hand-written `nId & 0xffff` on a 32-bit parameter and had been transcribed
  as one. A `movzx` would have meant a 16-bit MEMBER (where over-reading is not allowed); from a
  parameter slot, `mov`+`and` means a 16-bit parameter.

- **`p[i] |= m` and `p[i] = p[i] | m` are the SAME to VC5 — a 3-instruction read-modify-write in
  the original is not reachable by spelling (v419).** Where the original emits
  `mov cl,[edx] / or cl,al / mov [edx],cl`, VC5 folds every source form tried — `|=`, the
  longhand assignment, an explicit `unsigned char` temp holding the old value, and hoisting the
  base pointer into a local — into one `or byte ptr [edx],al`. Treat it as a scheduler coin flip,
  not a source shape. Corroboration that it IS a coin flip: `WorldBoardMaybe::PlaceObject` stamps
  the *same* dirty bitmap with the *same* expression and the original emits the folded
  `or [ecx],al` there.

- **Two `imul`/`add` operand orders are indistinguishable in the score but not in the semantics —
  check index expressions against the registers, not the instruction shapes (v419).** `src/`
  carried `wCols * col + row` where the original computes `wCols * row + col`; both compile to one
  `imul` plus one `add`, so asmscore charges the difference as ordinary register-role noise and it
  survived several sessions inside a PARTIAL function. When transcribing any 2-D index, resolve
  which register holds which loop variable from the CELL index computation (whose scale factor is
  unambiguous) and then read the second expression's operands off that, rather than assuming the
  two expressions in a function agree.

- **A dead-looking struct copy is REAL CODE, and it is often the whole size gap (v421).** Both
  board scrollers (`AppWindow::ScrollBoardHorizontal`/`Vertical`, 0x407ae0/0x407bf0) read only TWO
  of `g_rectAppClientBounds`'s four fields, yet the original opens with `sub esp,0x10` and stores
  the OTHER two into dead stack slots. That is `RECT rect = g_rectAppClientBounds;` followed by
  using `rect.left`/`rect.right`: VC5 forwards the two loaded values straight into the subtraction
  and drops THEIR stores, but its dead-store elimination does not remove the two with no reload.
  Reading the two globals directly compiles to 235 bytes against the original's 262 and never
  matches; the copy makes both twins EXACT on the next compile. Symptom to watch for: a prologue
  `sub esp,N` whose slots are written once and never read, plus loads of struct fields the
  arithmetic never uses. Do not "clean up" such a copy — it is the source.

- **#18j confirmed again on a THREE-case switch, and it is worth checking on any compare chain of
  2–3 cases (v421).** `AppWindow::BuildTool_SetAutoCurveConnectModeMaybe` (0x4089d0) lowers as
  `sub eax,0 / je / dec eax / je / dec eax / jne` — switch lowering, not an if/else chain — and
  its bodies sit in memory as case 2, then case 1, then case 0. Writing the `switch` with
  `case 2:` first and `case 0:` last matched all 84 bytes on the first compile. The `sub reg,0`
  (rather than a plain `test`) is the tell that VC5 is normalizing a switch selector, even when
  the lowest case is 0.

- **A `HWND`/pointer local initialized to NULL at FUNCTION scope is a real, load-bearing source
  shape when the prologue has a bare `xor reg,reg` (v421).** In
  `AppWindow::ApplyDisplayModeMaybe` (0x407d20) the zero register is set up before the first
  branch and only consumed in the arm ~600 bytes later that actually assigns `HWND_TOPMOST` to
  it; VC5 borrows it in the meantime for the other arm's dword `= 0` stores and its NULL
  argument. Declaring the variable down in the arm that uses it scored 311861 against 229933 for
  the function-scope spelling. Corollary for reading residuals: a register that VC5 folds into
  BYTE compares/stores (`cmp [mem],bl`, `mov [mem],bl`) is a *dedicated zero constant*, while one
  the compiler leaves out of byte ops (original spells those `mov al,[mem] / test al,al` and
  `mov byte ptr [mem],0`) is a *variable that happens to hold zero* — so an extra `xor` plus
  byte-op folding means you have one register more free than the original, not a missing statement.

- **`x = x + f() + (a - b);` and `x += f(); x += a - b;` are NOT the same to VC5 (v421).** Written
  as one expression VC5 reassociates it into `f() + ((a - b) + x)` and folds the call result in
  last; split into two statements it emits the original's `add reg, eax` (x + call result) first
  and keeps the running value in a register with a single store at the end. Worth 14k of asmscore
  on 0x407d20's frame-adjust block, and explicit parentheses around the first pair do NOT
  reproduce it — only the statement split does.

- **⚠⚠ A CLASS'S MEMBER-FUNCTION DECLARATION COUNT IS A CODEGEN INPUT FOR EVERY TU THAT INCLUDES
  ITS HEADER — measure the whole repo after ANY header declaration, not just the TU you were
  working on (v422).** `src/MailWnd.h` had already recorded one direction of this dial (adding a
  5th declaration to `MailWnd` moves `src/Main.cpp`'s `AppWndProc`). v422 found it runs the other
  way too, and there it destroys a real match: `MailWnd::RefreshClientClipRect` (0x42f8b0)
  byte-matches at 1332 B only while every header MailWnd.cpp includes is left alone. Adding ANY
  declaration to `src/AppWindow.h` **or** `src/DPlaySessionMgr.h` — a member function or a plain
  free function, at any position in the file, under any name, and no matter how many are added —
  pushes it to 1334 B / DIFF(150), always identically. Bisected one declaration at a time with
  everything else held fixed. What moves is register pressure, not content: at 397/396 insns the
  original zero-extends a `WORD` field straight into edx (`xor edx,edx / mov dx,[ecx+0x14]`) and
  keeps eax free for its `lea eax,[esi+0x66c]`, while the perturbed build routes the value through
  eax and spends an extra `mov edx,eax`; the other 18 identity_miss rows are that one choice
  cascading. Properties of the dial, all measured:
  - **It is not name-dependent.** A dummy `char AaaZzz();` moves the consumer exactly as much as
    the real declaration does.
  - **For a class member it is not monotone — but do not count on that as an escape hatch
    either (v423).** In v422 adding one member to `AppWindow` broke `src/WorldBoardMaybe.cpp`'s
    `FindNearestObjOfCategoryMaybe` (0x457ce0) and adding a second and third put it back. v423
    retested the same ladder one commit later and it did NOT reproduce: 1 and 3 added declarations
    cost `FindNearestObjOfCategoryMaybe` AND `MailWnd::RefreshClientClipRect` byte-for-byte the
    same. The dial had saturated. Neither "more declarations fix it" nor "more declarations make
    it worse" is a law — measure each state, and re-measure after any OTHER header moves, because
    that changes which state you are in.
  - **A free function declared at file scope in the same header is NOT always free.** It cost
    nothing in WorldBoardMaybe.cpp but cost MailWnd.cpp the full 1332 B, so "declare it outside
    the class" is a probe worth trying, never an assumption.
  - **Adding `#include`s and file-local declarations to a `.cpp` is always free** — only edits to
    a header other TUs read propagate. When a shared header edit turns out to be expensive, a
    TU-local declaration in the one `.cpp` that needs it is the cheap fallback (record the
    measured reason at the site, as `src/AppWindow.cpp` does for
    `GameNet_TeardownAllSessionState`).
  - **⭐ The dial's damage is often PAYABLE AT THE CONSUMER, for free (v423) — check this before
    abandoning a correct header edit.** The rotation is register pressure, so a consumer whose own
    residual is a *local* register-assignment coin flip can simply be re-flipped in its own source.
    `WalkerActor::UpdateFacingFromStepDeltaMaybe` (0x4331b0, 112 B) fell out of EXACT when v423
    added declarations to three headers `WalkerActor.cpp` includes (each flipped it
    INDEPENDENTLY, so the parity is over the TOTAL count across the TU's headers, not any one
    header). It came straight back by swapping the declaration order of the function's two `int`
    locals — the load order decides which of two inlined `abs()` expansions gets esi vs edi. This
    is the second rotation of that same function (v418 was the first, in the opposite direction),
    so treat a `-`/`+` pair plus an esi/edi swap at insns N/N as "swap the two locals", not as a
    reason to revert the header. Only give up on the header edit when the consumer's residual is a
    whole-function cascade instead (0x42f8b0 is the standing example — see the next sub-bullet).
    **The same dial is worth turning on ANY function whose residual is dominated by `reg_pen` /
    an esi-edi swap, not only on one that a header edit just rotated (v432, 0x40b880: swapping
    `int nX`/`int nY` and then `nOriginX`/`nOriginY` was worth 257497 → 244937 with zero
    semantic change).** Cheap enough to be a routine probe: sibling locals of the same type
    have no ABI-visible order, so the swap is one compile and can never change behaviour. Probe
    each pair INDEPENDENTLY and re-probe a pair after an earlier swap lands — on 0x40b880 the
    nOriginX/nOriginY order that WON before the nX/nY swap LOST after it (248609), i.e. the dial
    positions interact and are not separable.
  - **Corollary for modeling:** a match that survives only at one particular declaration count is
    a knife edge, NOT evidence that the incomplete class is the correct model. Our partial class
    views have arbitrary member counts; the original's were complete. Prefer the correct model
    when it is cheap, park the promotion with a measured note when it is not, and never revert a
    real duplicate-struct fix to preserve a lucky alignment. When you do park one, PRICE it and
    write the number down: v423 established the `AppWindow` promotion is worth exactly +641 B
    (`AppWindow_StartGame` 0x408350 goes EXACT) against −1332 B (MailWnd 0x42f8b0) and −951 B
    (WorldBoardMaybe 0x457ce0), i.e. net −1642, so the decision is a standing arithmetic fact and
    nobody has to re-run the experiment to re-derive it.

- **⭐ A branchy `push 0 / jmp / push 1` bool ARGUMENT is a CROSS-JUMPED if/else pair, not a
  comparison expression (v423).** When a bool-valued argument materializes as
  `cmp / jcc / push 0 / jmp / push 1 / call`, the source did NOT pass a comparison — it wrote two
  separate calls and VC5 tail-merged them:

      if (cond) { Foo(0); } else { Foo(1); }      // <- the real source

  Passing the comparison instead (`Foo(a <= b)`) compiles to `setcc` for a relational test, or to
  the `neg / sbb / neg` idiom for an `x != 0` test, and is 1 instruction SHORTER each time. Two
  such sites were the whole residual of `AppWindow::EnterBuildMode` (0x4086f0) — rewriting both as
  if/else pairs took it from DIFF(180) to a 732-byte EXACT match. Reading the branch direction:
  MSVC emits `test/cmp; j<NOT-cond> -> Lelse; <then>; jmp Lend; Lelse: <else>`, so the
  FALL-THROUGH after the conditional jump is the THEN block and the jump target is the ELSE. The
  original's `cmp ecx,eax / jge L / push 0 / jmp / L: push 1` therefore means the source tested
  `<` with `0` in the then-arm, not `>=` with `1` — getting this backwards costs one wrong `jcc`
  and nothing else, which is easy to misread as noise.

- **⭐ `mov ecx, <absolute global>` immediately before a `call` is a `__thiscall` tell even when
  the callee's body never touches `this` (v423).** A `__thiscall` member with one stack argument
  and a `__stdcall` free function with one argument compile to the IDENTICAL body — both end
  `ret 4` — so a this-ignoring member byte-matches perfectly when modeled as a free function, and
  nothing in that function's own diff ever reveals the mistake. It shows up only in the CALLERS,
  as a missing `mov ecx, <global>` per call site with `reg_pen`/`identity_miss` both 0.
  `AppWindow_StartGame` (0x408350) scored insns 159/164 with all five `--dump` rows being the same
  deleted `mov ecx, [0x4aa4a0]`; modeling `DrainQueuedMouseInput` as a real `AppWindow` member
  took it to a 641-byte EXACT match and left 0x4085e0 itself still matching. So: **when a residual
  is nothing but N identical missing loads of a singleton pointer, the callee is a member of that
  singleton** — Ghidra's own namespace usually already says so. (Whether you can AFFORD the
  promotion is a separate question — see the declaration-count dial above.)

- **`and al, 0xf8` vs `and ecx, -8` is a REGISTER tell, not a source difference (v422).** VC5's
  select-between-two-constants lowering (`B + (-(cond) & (A - B))`, emitted as
  `dec / neg / sbb reg,reg / and / add`) encodes its mask step in 2 bytes when the value happens
  to live in eax and 3 bytes anywhere else. On `AppWindow::CreateMainWindow` (0x406ed0) that one
  byte is the entire length difference between a 64/64-instruction candidate and the original: the
  original hoists three of `CreateWindowExA`'s four non-immediate arguments into registers before
  the pushes begin and defers the fourth to the register recycled by an earlier push, giving eax
  to the select and deferring nWidth; the candidate defers the select instead. Hoisting the
  ternary into its own `DWORD dwExStyle` statement and flipping it between `!= 1 ? 0 : X` and
  `== 1 ? X : 0` are both bit-identical — the operand order of the sbb predicate is decided by the
  lowering, not the spelling, so do not grind it from the source side.

- **A `while (PeekMessageA(...) > 0)` pump with a `switch` on `msg.message` and a stop flag
  transcribes literally, first compile (v422).** `AppWindow::DrainQueuedMouseInput` (0x4085e0)
  matched all 264 bytes as a plain rotated `while` — first call before the loop with
  `test eax,eax / jle exit`, the loop-back test `jg`, and the `TranslateMessage`/`DispatchMessageA`
  import slots hoisted into the preheader after that first test — with `char bStop = 0;` declared
  inside the loop body, set only in `default:`, and tested by an `if (bStop) break;` after the
  switch. Two further confirmations from the same function: `> 0` (not `!= 0`) is what
  `test/jle`+`jg` means; and a switch whose cases are a contiguous high run with one hole
  (0x201/0x202/**0x203 absent**/0x204/0x205) plus two low outliers lowers as
  `cmp / ja / je / cmp / jne default` for the outliers and a jump table for the run, with the hole
  routed to the default arm — ascending case order reproduced the body layout directly, so #18j's
  reordering was not needed here.

- **`memset(&wc, 0, sizeof(wc))` on a `WNDCLASSA` local is the source of the
  `mov ecx,0xa / xor eax,eax / lea edi,[..] / rep stos` prologue (v422)** — VC5 inlines the
  constant-size intrinsic, and the dword count is `sizeof` / 4, which doubles as a check that you
  have the right struct. In `AppWindow::CreateMainWindow` (0x406ed0) the ten field assignments
  that follow are in plain WNDCLASSA declaration order; the apparent hoist of
  `wc.hInstance = this->hInstance` above the earlier `wc.style` stores in the decompiler output is
  just scheduling, because the same loaded register also feeds the `LoadIconA` push.

- **A raw `vtable + N` in Ghidra's output is a SLOT NUMBER, not a name — never map it onto a
  method by analogy with the neighbouring function you just matched (v424).** The screen-state
  dispatcher `AppWindow_SetScreenState` (0x408130) transcribed and compiled to DIFF(4) on 536
  bytes, and all four differing bytes were the same one-byte displacement: `call [eax+0x8]` in the
  original against `call [eax+0x4]` in the candidate. The cause was reading
  `(**(code **)((int)(g_pSplashWnd->base).vtable + 8))()` and reaching for the name its
  address-adjacent sibling `AppWindow_EnterBuildMode` (0x4086f0) uses at the same shape —
  `EndActiveSession()`, which is `WindowBase` slot **4**. Slot **8** is `BeginModalCapture()`, its
  begin-half counterpart. The two functions are semantic mirrors (one LEAVES a screen, one ENTERS
  one), so the analogy was exactly backwards, and both spellings compile and lint cleanly. Resolve
  every `vtable + N` against the class's own declaration-order slot map (`src/WindowBase.h` labels
  each `virtual` with its slot) before writing the call, and treat a residual that is N identical
  one-byte displacement diffs as an adjacent-slot mistranscription — the same tell #25 already
  records for a wrong `this` offset.

- **A long run of `if (p) { delete p; p = NULL; }` teardown blocks lowers with a ZERO REGISTER, so
  the null test reads `cmp reg,edi` rather than `test reg,reg` (v424 recon, 0x4077a0).** VC5
  hoists one `xor edi,edi` above the run and then uses edi both as the comparison operand and as
  the stored null, which turns each block into `mov ecx,[g] / cmp ecx,edi / je next / mov eax,[ecx]
  / push 1 / call [eax] / mov [g],edi`. Do not read the `cmp` against a register as evidence of a
  comparison against some other live variable, and do not try to reproduce it by spelling the
  source `p != someZeroLocal` — it falls out of the repetition count alone. Same family as the
  zero-reg entries already recorded for 0x456150 and 0x462e90.

- **A named `static const char *const` for a string a function uses TWICE compiles to a MEMORY
  LOAD, not a `push <imm32>` (v426, `AppWindow_LoadConfigDirectories` 0x4068d0).** Hoisting the
  registry path into `static const char *const kszRegPathKey = "..."` looked like the tidy way to
  say "same key at both the open and the create site", but VC5 gives the *pointer variable* its
  own `.data` cell and emits `mov ecx,[kszRegPathKey] / push ecx` where the original emits
  `push 0x47e238`. Under `/Gf` the two literal occurrences already pool to one address, so
  repeating the literal is both the original's shape and free. *The tell*: a `mov reg,[<reloc>]`
  immediately before a `push reg` where the original pushes an immediate, plus a spurious extra
  register copy near the call. Applies to any `const char *const` / `const int` named constant —
  spell the literal at each site instead.

- **VC5 overlays a block-scoped local's stack slot with the compiler temp `operator new` needs,
  and failing to reproduce that overlay costs one frame dword and shifts EVERY displacement in
  the function (v426, 0x4068d0, DIFF 21 with align=0 / reg_pen=0 / 242 of 242 instructions
  matching).** `p = new T(args)` needs a temp to hold the raw allocation between `operator new`
  and the ctor call; VC5 will happily park that temp in a dead earlier local's slot. The original
  reuses the `DWORD dwType` it passed to `RegQueryValueExA` 200 bytes earlier — which is why
  **Ghidra shows the two as ONE variable** (`IniFile *local_a3c`, also passed as the `lpType`
  out-param): the decompiler is reporting a shared slot, not a shared purpose. ⚠ *Do not "fix"
  that by giving the C++ one variable of one type* — they are genuinely two locals. *The tell*:
  the instruction stream matches 1:1 but `sub esp,N` is 4 (or 8, ...) too large and every
  `[esp+disp]` past the un-overlaid slot is uniformly shifted. Refuted levers on this instance:
  moving the earlier local between block and function scope, and reordering its declaration
  against its neighbour (VC5 picks the slot order itself, both spellings scored identically);
  moving a SECOND local into the same block scored strictly worse.

- **Frame archaeology pins buffer sizes that look like typos — read the displacements, don't
  round to a familiar constant (v426, 0x4068d0).** Two adjacent stack buffers there are 1280 and
  **1284** bytes, not the `MAX_PATH`-ish numbers one would guess: the frame runs szInstallDir at
  `+0x40..+0x53f` and abRegValue at `+0x540..+0xa43`, and 0x504 = 1284 is independently what the
  function seeds its `RegQueryValueExA` size out-param with. Deriving a local array's size from
  the NEXT local's displacement (and cross-checking it against any size the code itself passes to
  an API) is exact; guessing 260/1024/1300 because the number "looks wrong" is not. Same
  discipline as the struct-`sizeof`-from-the-allocation-site rule in CLAUDE.md.

- **A `goto` to a shared fallback label is real 1998 source, and its tell is a status value that
  is TESTED AND DISCARDED (v426, 0x4068d0, DIFF 313 -> 26 on this one rewrite).** Two failure
  checks that both land on the same recovery block cannot be written as `if/else` without either
  duplicating the block or keeping a status/flag variable alive across it. Spelling it with one
  reused `LONG lResult` costs an extra `mov esi,eax` right after the first call, because the
  value must now survive to a second test; the original instead does `cmp eax,ebx / jne fallback`
  with no save at all, then a `jmp` over the fallback from the success path. *The tell*: the
  original compares a call result **in eax, immediately, with no spill**, at two sites that share
  a branch target. Give each call its own short-lived named local (see the named-local-vs-inline
  zero-test rule above, which wants the local anyway) and `goto` the shared label.

- **`g_p = new T;` under `/GX` needs NOTHING hand-modelled — but T must have a DECLARED ctor and
  a `sizeof` equal to the original's `operator new` argument.** The whole SEH scaffold the
  decompiler shows (`local_4` stepped `-1 -> N -> -1` around each allocation, the raw pointer
  spilled to a frame slot the body never reads, the `je`/`xor eax,eax`/`jmp` null-check merge)
  falls straight out of the plain source statement; writing sixteen of them in a row reproduces
  sixteen consecutive trylevel steps with no help. Confirmed v429 on `0x406ba0`, six singletons,
  first compile. Pin every `sizeof` FIRST with a throwaway TU of
  `char a[sizeof(T)==0xNNN?1:-1];` — one compile checks all of them at once, and a partial view
  that is the wrong size silently changes the `push SIZE` immediate rather than failing.
- **A `__thiscall` method that takes a STACK argument cannot use the `__fastcall` free-function
  escape hatch** — `__fastcall`'s second parameter goes in `edx`, not on the stack, so the hatch
  only reproduces a this-call whose argument list is EMPTY. When a TU needs `obj.Method(arg)` on a
  class it has no header for, a methods-only view struct is the cheapest correct answer; keep the
  view's name TU-scoped (`FooView0xADDR`) so it cannot collide with the real class later.
- **VC5 defers a reloc-bearing non-push instruction until AFTER an argument-push group, and this
  is a scheduling tie-break you cannot reach from the source.** The two shapes are
  `mov ecx,[glob] / push / push / push / mov ds:X,eax / call` (original) versus the same
  instructions with the store hoisted to just after the `mov ecx`, and
  `push OFFSET str / mov ecx,OFFSET obj / call` versus `mov ecx / push / call`. Before spending
  probes on it, run the census — a regex over every built `.obj` for `\x68....\xb9....\xe8`
  against `\xb9....\x68....\xe8` told v429 that VC5 emits the original's order 152 times and the
  other order once, and that the identical source construct in a sibling TU compiles the RIGHT
  way round. That census is what proves the residual is context, not spelling, and it costs one
  script instead of six compiles.
- **A SELF-RECURSIVE helper that appears both as a standalone function AND expanded one level
  inside a caller is an `inline` function, and spelling it so is a free, exact match.** VC5 still
  emits the out-of-line COMDAT (the recursive self-call odr-uses it), while the caller gets the
  body expanded with only the recursion left as a real `call` to that copy — which is exactly the
  shape the decompiler shows: the caller ends in the loop plus a direct `operator delete`, and the
  callee exists at its own address anyway. v430's `TrackGraph::FreeRouteTreeMaybe` (0x45d810) went
  from a plain call to EXACT, and took its caller `BuildRoutesFromNodeMaybe` (0x45d5f0, DIFF(16)
  and 33 bytes short) to EXACT with it, from one `inline` keyword. ⚠ Within a single TU you can
  write `inline void C::M(...)` at namespace scope and keep the `// FUNCTION:` marker on it — the
  definition does NOT have to move into the header, so this costs no cross-TU risk at all. It does
  have to be defined BEFORE the caller, so move it out of address order and say why.
- **A loop bound that the original computes as a VALUE is not interchangeable with its
  algebraically equal rearrangement.** `for (i = 0; i < n - 1; i++)` compiles to
  `lea edx,[eax-1]; test edx,edx` (form the bound, then guard on it); the equivalent
  `for (i = 0; i + 1 < n; i++)` compiles to `cmp eax,1` and then diverges through the whole loop's
  induction-variable choice. v430's `StampRoutePairsMaybe` (0x45dad0) went from DIFF(164) to EXACT
  on that one substitution. Read the guard shape off the disassembly before choosing the spelling.
- **When the original materializes a comparison into a byte register (`mov dl,1` / `xor dl,dl`
  with a `jmp` merge) instead of recomputing it, that is a real local — and it must be set in BOTH
  arms of a real if/else, not pre-initialised ahead of a one-armed `if`.** The one-armed form
  hoists the `xor` above the branch and loses the merge. v430's `GetStepDirectionMaybe` (0x45dd80)
  went DIFF(87) → DIFF(35) and to the original's exact length on that alone.
- **`x &= MASK; return x;` and `return x & MASK;` are NOT the same codegen for a narrow type.**
  On an `unsigned char`, the compound form stays 8-bit (`and al,3`); the expression form (and an
  explicit `(unsigned char)` cast on it, which does not help) promotes to int and emits
  `and eax,3`. Same family as the `unsigned short` compare lesson — when the original works in a
  byte register, keep every step of the source in the narrow type.
- **`&arr[i]` cached in a pointer vs. re-subscripting `arr[i]` at each use is a real lever, and
  the ORIGINAL usually re-subscripts.** VC5 CSEs the address itself into one `lea` for the stores
  while still emitting the indexed form (`mov al,[ecx+eax]`) for the load; a hand-cached
  `unsigned char *` collapses all of them onto the one pointer and changes the base register.
- **Control-flow spelling is NOT a lever for VC5's cold-path sinking.** Writing the body as the
  `if` branch with the failure case falling out the bottom, versus two early returns in front of a
  flat body, compiles to byte-identical code (v430 probe on `SolveRouteMaybe`, 0x45dbc0). VC5
  decides on its own whether to leave an early-exit epilogue inline at the top or sink it to the
  bottom and cross-jump the copies together — so a residual whose whole diff is two displaced
  epilogues at `reg_pen 0`/`identity_miss ~2` is intrinsic. Do not spend probes reshaping the
  source; prefer the spelling that reads like the original and park it.
- **A `new T` whose site zeroes exactly `sizeof(T)/4` dwords immediately afterwards is
  `new T; memset(p, 0, sizeof(T))`, not a zeroing constructor.** A default ctor that zeroed the
  members would emit them field-by-field; the dword countdown loop plus a dead
  constant-zero-count remainder loop is the inlined `memset`. Conversely a `new T` followed by a
  store of 0 to ONE field IS a real constructor — v430's `TrackGraphEdge() { Unk0x4 = 0; }` is
  pinned exactly that way, by the single `mov [eax+4],0` between the allocation and the caller's
  own three field stores.
- **`memset(p, 0, n * sizeof(T *))` for a pointer array compiles to a dword loop guarded by
  `n & 0x3fffffff`** — that mask is the compiler proving `(n * 4) >> 2` cannot overflow, not a
  source-level mask. Seeing it is confirmation the size argument really was written with
  `sizeof`, so transcribe it that way rather than as a bare byte count.
- **⭐ Normalizing a non-`bool` byte into a `bool` local: the TERNARY is the lever, and it is the
  ONLY one.** `bResult = f();` (where `f` returns `unsigned char`), `bResult = f() != 0;`, and even
  `bResult = !!f();` ALL compile to VC5's 4-instruction 32-bit normalization
  `mov bl,al; neg bl; sbb ebx,ebx; neg ebx` — identical scores, so probing among those three is
  wasted budget. Only `bResult = f() ? true : false;` (and its exact equivalent, an explicit
  `if/else` assigning `true`/`false`, which compiles byte-identically) gives the original's
  2-instruction `test al,al; setne bl`. v433, `WidgetPickerObj0x477cc8::OnKeyDownMaybe` 0x4290a0
  — this one line was the sole remaining diff on an otherwise 347/347-instruction match, and the
  ternary closed it to EXACT. The `?:` is the tidier spelling; prefer it over the if/else.
- **A `setne`-free `mov bl,al` at a call return site PINS THE CALLEE'S RETURN TYPE to `bool`.**
  Corollary of the above: if the destination is a `bool` local and the original does NOT normalize,
  the callee must already return a normalized `bool`. On a VIRTUAL this propagates — C++ forces an
  override's return type to equal its base's, so one derived function's byte-match can pin an
  entirely untranscribed base's signature (v433: `WidgetPickerObj0x477cc8::OnKeyDownMaybe` pinned
  `WidgetBaseObj0x4784c8::OnKeyDownMaybe`/0x454ae0 to `bool`; spelling the pair `char` breaks the
  match outright, DIFF 620). Complements the existing "`bool` return widens to full EAX" bullet:
  that one reads the CALLEE's own epilogue, this one reads the CALLER's use site.
- **⭐ Funnel every exit through ONE result local + ONE trailing `return` when the original has a
  single shared epilogue.** Per-arm `return 0;`/`return 1;`/`return bResult;` statements are
  semantically identical but make VC5 re-materialize the constant at each site instead of sharing
  the original's one `mov al,bl; pop; pop; pop; ret N`. Worth a large score swing on a
  multi-exit dispatcher before any register-level probing — v433 got 198743 → 118361 from this
  restructure alone on a 347-instruction switch. The tell is an original in which several
  early-exit branches all `jmp`/`jcc` to the SAME epilogue address while holding the result in a
  callee-saved byte register.
- **⚠ Adding a single member DECLARATION to a shared header can flip an unrelated TU's
  already-EXACT function, and placement is not a workaround.** v433 added one non-virtual
  `unsigned char HandleTextEditKey(unsigned int)` declaration to `UiIconListItem` in
  `src/MenuNode.h`; `TilePlacedObj::SpawnSeqRecordEffectMaybe` (0x4588b0) — in a TU that never
  names that class and never calls that function — went EXACT → DIFF 79, losing a callee-saved
  register (four pushes down to three). Removing the declaration restores EXACT; moving it to
  three different positions in the class body changes nothing, so it is the declaration's
  EXISTENCE perturbing the /Og register budget, not its ordering. Two lessons: (1) this is why
  CLAUDE.md demands a FULL `progress.py` per-file TABLE diff after ANY header edit — the headline
  byte total went UP that session and hid the regression completely; (2) when the new declaration
  is faithful to the real class, KEEP it and park the collateral damage — the model is the
  deliverable, and a function that only ever flipped from unrelated TU context was never really
  "solved" in the first place.
- **⭐ An if/else-if chain that the original dispatches with CHAINED SUBTRACTS (`mov r,x; sub r,K1;
  je …; sub r,K2-K1; je …`) is a `switch` in the source, not an `if`.** Obvious in hindsight, but
  the tell is easy to miss because a 2-case switch over far-apart values compiles to plain `cmp`s,
  so the chained-subtract form is the only unambiguous signature. Two v437 sites closed on this one
  change: `NetSetupWnd::OnUnhandledMessageMaybe` (0x442150) DIFF(86) → EXACT once `if (msg ==
  WM_SYSCOMMAND) … else if (msg == WM_CTLCOLOREDIT) …` became `switch (msg) { case …: break; }`,
  and `NetSetupWnd::BeginModalCapture` (0x441870) DIFF(16) → EXACT on the same rewrite of a
  two-armed provider-type test inside a list walk. The sibling that ALREADY matched in the same
  file (`RefreshProviderAvailability`) had been written as a switch all along — **when two
  functions in a TU do the same test and only one matches, copy the matching one's control-flow
  spelling before probing anything else.**
- **A `switch` also relocates its CASE BODIES after the dispatch and the fall-through, which is a
  second, independent tell — visible even when the dispatch itself is plain `cmp`s.** If the
  original's early-`return` bodies sit at the END of the function with the common tail in the
  MIDDLE, that is switch layout; an if-chain inlines each body where it is tested. v437's
  `NetSetupWnd_EditSubclassProc` (0x4417e0) has `cmp eax,0x20; je <+0x65>` — a plain compare — yet
  both handler bodies live at +0x45/+0x65 with the `CallWindowProcA` fall-through at +0x23. Written
  as an if-chain it scored 136807 with the bodies inline; written as `switch (msgId) { case
  WM_SETCURSOR: … case WM_KEYDOWN: … } return CallWindowProcA(…);` it went to byte_diff 1.
- **A lone `mov [esp+N],0` for a plain local is a real `= NULL`/`= 0` INITIALIZER (v436), and the
  ADJACENT STORE ORDER of a field-init helper is source order (v437).** `NetSetupWnd::InitFields`
  (0x440fa0) sat at DIFF(4) with every one of its 130 instructions paired; the whole residual was
  two `S` rows swapping `mov [esi+0xec],ebx` and `mov byte [esi+0xe8],bl`. Writing the two
  assignments in the original's emitted order made it EXACT. Cheap first probe on any ctor/
  `InitFields` whose only diff is a short run of adjacent same-shape field stores — but note this
  is NOT general: cl reorders freely across calls and around address-taken locals (see the
  `LoadOrInitFromDisk` frame-slot park, where SIX declaration orderings scored identically).
- **⚠ `UINT_PTR` does not exist in the VC5 SDK** — it is a Win64-era typedef. `SetTimer`'s return
  and any handle-sized integer field must be spelled `UINT`. Ghidra's decompiler emits `UINT_PTR`
  freely (it types against a modern SDK), so this bites on transcription, not on modelling.
- **⚠ `CallWindowProcA`'s first parameter is `FARPROC`, not `WNDPROC`, in the VC5 SDK** (no
  `STRICT`), so a `WNDPROC`-typed field needs an explicit `(FARPROC)` cast at the call — the cast
  is the SDK's, not a modelling compromise, and `src/SplashWnd.cpp` already spells it that way at
  two sites. The field itself stays `WNDPROC`.

- **⭐ A per-arm CONSTANT PUSH over a shared `mov ecx,<obj>; call <fn>` tail means the CALL is
  written out inside every arm, NOT one call taking a computed argument (v438).** This is #18's
  cross-jump geography seen from the other side, and it is the single most productive shape-lever
  found this session — it closed two functions on its own. Symptoms, in the `--dump`: the original
  shows `push K1 / jmp / push K2` (or a whole `push dy; push dx` pair per arm) converging on one
  call, while the candidate materialises the value into a register first (`sete`, or
  `xor r,r / mov r,K / or r,-1` chains) and pushes the register. Fix by DUPLICATING the call
  statement into each arm and letting cl tail-merge it:
    * `pIpxBtn->DrawFrame(g_pNetSettings->rememberedProtocolSecondary == 4, NULL);` →
      `if (…== 4) pIpxBtn->DrawFrame(1, NULL); else pIpxBtn->DrawFrame(0, NULL);`
      (`NetSetupWnd::DrawStatusTextMaybe` 0x441b40, DIFF(193) → EXACT)
    * a `switch` computing `dx`/`dy` locals then ONE `OffsetRect(&r, dx, dy)` →
      `OffsetRect` written out in all four arms (`OnTimerDefaultMaybe` 0x4423d0)
    * a 4-way `if/else` computing a `UINT uFormat` then ONE `DrawTextA(…, uFormat)` →
      `DrawTextA` written out in all four branches (same function; the two fixes together took it
      DIFF(415) → EXACT).
  Corollary worth keeping: **two `else`-arms that assign the SAME constant are not a bug in your
  reading.** 0x4423d0's third and fourth alignment branches both push 0x20 because they are
  `DT_SINGLELINE|DT_TOP` and `DT_SINGLELINE|DT_LEFT` and both `DT_TOP` and `DT_LEFT` are 0 — the
  dead `cmp` in front of two byte-identical blocks is the tell that a real 4-way source construct
  collapsed, not that one branch is unreachable.
- **⭐ The inverse lever: NAMING a call's most complex argument into a local lets cl schedule the
  simple pushes INTO that argument's computation (v438).** Written inline as the argument
  expression, cl 11.00 emits all the simple pushes FIRST and then computes the complex one; hoisted
  into a named local on the line above, it starts the computation and fills the latency with the
  pushes — which is what the original does. `g_UIResources.PlaySoundAtScreenPos(rand() / 0x1fff +
  0x500f, pt.x, pt.y, 4)` → `int nSoundId = rand() / 0x1fff + 0x500f;` + the call took
  `NetSetupWnd::OnLButtonDown` (0x441c80) from DIFF(26) to EXACT, with the three
  `push 4 / push ebx / push edi` landing interleaved into the `imul`/`sar` divide sequence exactly
  as the original has them. Note this is the OPPOSITE direction from the bullet above: duplicate
  the CALL to move constants into arms, hoist the ARGUMENT to move pushes into a computation.
- **⭐ Adjacent stores that build one struct are emitted in SOURCE order, and on a RECT that order
  is recoverable field-by-field from the disassembly (v438).** `RefreshClientClipRect` (0x441360,
  951 B) is nine consecutive RECT builds and got to EXACT purely by reading each block's store
  order out of the original and writing the source that way — no register-level work at all. The
  orders are NOT uniform and are not guessable: a left/top-anchored rect is `left, right, top,
  bottom` (right derived from left, bottom from top), a right/top-anchored one is
  `top, bottom, right, left`, and the input-box rect is `left, right, bottom, top`. Getting one
  block wrong is worth ~100 of total score, so fix them one block at a time and re-measure.
  The corollary from the same function: **the deviation the scheduler WILL make is hoisting a
  store of a shared zero/constant register a slot or two early** — `left = 0` and `bottom = 0`
  landing next to each other in the output does not prove they are adjacent in the source.
- **⭐ Chained `sub`/`dec` dispatch (`sub eax,0; je; dec eax; je; dec eax; je`) is a dense-`switch`
  tell even with no jump table** (re-confirmed v438 on 0x4421d0 and 0x4423d0, both 0..2 + default).
  Do not read it as an if-chain; the `sub r,0` that looks like a no-op is the switch's own
  normalisation of the first case value.
- **⚠ Source POSITION inside a mega-TU can cost a function 1000+ lines away its EXACT match, with
  no header or declaration involved (v438).** `src/DPlaySessionMgr.cpp` aggregates several of the
  original `.obj`s, so where a newly transcribed function goes is our convention, not ground truth.
  Adding two functions in ADDRESS order (mid-file, after `ReconcileCarHandoff`) turned
  `ApplSetupWnd::SelectGridCellFromPointMaybe` (0x40aba0, ~1200 lines further down) from EXACT
  at 166 B into DIFF(130) at 170 B — identical code, reshuffled. Moving the same two functions,
  byte-for-byte unchanged, to the END of the file restored it. That is the same function
  `src/NetSetupWnd.cpp`'s `??_GNetSettings` autopsy found hypersensitive to declaration
  visibility, so the sensitivity is the FUNCTION's, not any one lever's. Practical rule: on a
  mega-TU, treat placement as a free tuning knob and always read the per-file `progress.py` table
  after inserting into one — the headline total went UP that run while the match was being lost.
- **A field the original RELOADS where cl 11.00 common-subexpression-eliminates it has no
  source-level lever when nothing stores in between** (v438, 0x440a80). The original reads
  `pCar->pKindDesc` twice across `(… == 0 ? -1 : …->resourceId) == 0x1870 || … == 0x1871`; cl keeps
  the first load alive, which cascades into the whole first ternary computing into a different
  register. Splitting the `||` into `if/else if` makes it worse (cl then sinks the second test past
  the flag store). Park it. Two levers in that same expression DO work and are worth trying first:
  spell the ternary NULL-FIRST (`p == 0 ? -1 : p->field`) so the `-1` is the fall-through — the
  `!= 0 ? … : -1` polarity inverts the branch and costs ~95k — and make a loop counter's decrement
  an explicit FIRST statement of the loop tail rather than a `for`-increment, which otherwise sinks
  below the following pointer read.
- **`DT_CALCRECT` is 0x400 and `DT_SINGLELINE` is 0x20, so a `DrawTextA` flag word of 0x420 is
  `DT_CALCRECT|DT_SINGLELINE`, not `DT_CALCRECT|DT_WORDBREAK`** (0x10). Trivially checkable, and
  wrong flags are invisible to the byte-match only until you compare the immediate.
- **#18h Naming a nested call's result pins ARGUMENT EVALUATION ORDER, and that is a real source
  tell, not tidying.** `GetClientRect(GetDesktopWindow(), &rect)` makes cl 11.00 evaluate the
  argument list right-to-left and emit the `lea` of `&rect` BEFORE the inner call; writing
  `HWND h = GetDesktopWindow(); GetClientRect(h, &rect);` emits the call first. Tell in the dump:
  a `+ lea …/- call …` pair right at the top of the function with everything below shifted.
  Found on `ApplSetupWnd::Create` (0x408f00) and it also closed `NetSetupWnd::Create` (0x4412f0),
  whose autopsy had recorded the swap as an "effective" register rotation for many sessions —
  worth re-reading any park whose residual is described as *one scheduling swap across a call*.
- **#18i The per-arm-duplication lever (#18f/#18g) fires on a call taking a per-arm CONSTANT even
  when the constant is not the only argument, and it fires N-ways, not just twice.**
  `ApplSetupWnd::RefreshConnectStatusText` (0x409360) has THREE `LoadLocaleString(<id>, buf, 0x80)`
  calls, one per arm of a nested if/else; hoisting the id into a local and calling once collapses
  the three `push <id>` sites into one and costs the whole match. cl cross-jumps them back onto a
  shared `mov ecx,<recv>; call` tail — and will even hoist a common `push 0x80` of an INNER pair
  out ahead of that pair's test while leaving the outer arm its own copy, which looks like
  hand-factored code in the disasm but is purely the optimizer. Same lever, same session, closed a
  per-row `SetTextColor(hdc, <colour>)` inside a loop (0x4094b0) and a `DrawProviderList(<list>)`
  whose argument came from a 2-way choice (0x409280) — in the latter, BOTH a ternary in the
  argument and an if/else over a local produce a single post-merge `push`; only writing the whole
  call out in each arm reproduces the original's per-arm push.
- **⚠ Stack traffic that LOOKS like a source-level local struct is often cl's own spill decision —
  do not "discover" a local from it.** `ApplSetupWnd::RefreshClientClipRect` (0x408f70) builds each
  of four button rects into the same four slots `[esp+0x10..0x1c]` and only then does a
  4-load/4-store copy into the object, and one block even reads its first value out of the
  PREVIOUS block's still-live slot — which reads exactly like one shared `RECT` scratch local
  reused per button. Writing it that way is the WORST of the three shapes measured (481503 vs
  427399 for plain `pBtn->rect.field = …` writes): cl folds the copy away again and then has one
  more live value to schedule. Judge a suspected local by whether it IMPROVES the score, never by
  the stack-slot pattern alone.
- **On a rect/geometry builder, the ADDEND in each `lea`/`add` names which rect the source chained
  off — flattening the chain into page-relative absolutes is a real, costly rewrite even though
  the arithmetic is identical.** 0x408f70's grid rect is `grid.right = grid.left + 0x1ee` and
  `grid.bottom = grid.top + 0x173`, and its text area is `grid.left - 2` / `grid.bottom + 0xf`;
  written as the `page.left + 0x209` / `page.top + 0x19a` / `page.left + 0x19` / `page.top + 0x1a9`
  absolutes those work out to, the score is 444752 instead of 427399 and eight instructions
  vanish. Read the operands, not the geometry: if a constant in the disasm is not one you wrote,
  the source anchored that field to a different rect than you assumed.
- **cl does NOT cross-jump two copies of a block once it gets long — a shared tail that shows up
  ONCE in the original has to be shared in the SOURCE too.** `ApplSetupWnd::OnKeyDown` (0x40ae20,
  v440) has a select-and-repaint tail reached from both its `VK_UP` and `VK_DOWN` arms; writing it
  out per arm (the #18f/#18i per-arm-duplication lever, which is right for a per-arm CONSTANT over
  a shared call) left BOTH copies in the output and cost 200 bytes. Hoisting the tail below the
  switch — arms compute only the index and `break`, `default:` returns early — dropped it to the
  original's own layout. **Rule of thumb: per-arm duplication is for a call whose ARGUMENTS differ
  per arm (a few instructions); a multi-call block that differs in nothing was shared in the
  source.** The same function shows the boundary in one place: its `DrawProviderList(<list>)` call
  DOES want writing out per arm (#18i), while the six-call tail below it does not.
- **When a function reads as fully matched except for one instruction at a two-predecessor merge,
  stop probing — the construct is under-determined and cl converges.** `OnKeyDown`'s last residual
  is `mov eax,[esi+0x104]; cmp edi,eax` vs `cmp edi,[esi+0x104]` where its two arms join: BOTH
  arms already hold the value in eax on their own incoming edge, so it is purely which edge cl
  rematerialized on. FOUR different source shapes (combined `||` guard, two separate `if`s, an
  arm-local guard plus a shared one, per-arm duplication of an unrelated tail call) compiled to
  BYTE-IDENTICAL output. Spend the remaining budget on the shapes that DO move the number — for
  this function they were compare-operand order (10k) and switch-case source order (72k).
- **Case-body LAYOUT order is the tell for switch-case SOURCE order, and it is worth one compile
  to confirm.** `OnKeyDown`'s jump table lists its cases by ascending key code (0xd, 0x1b, 0x26,
  0x28) but its BODIES are laid out 0xd, 0x1b, 0x28, 0x26 — and writing the source in that
  DOWN-before-UP order scores 88365 against 160512 for the ascending order. Read the body
  addresses out of the jump table, sort by address, and write the cases in that order.
- **Give a `??_G` thunk its own comment-only `// FUNCTION: LOCO` marker even when you are not
  writing a body for it, or it will STEAL the destructor's marker.** `tools/cc.sh` pairs markers
  to COMDATs by scanning forward for a definition; with an unmarked `??_G` COMDAT in the object,
  the `~T()` marker binds to `??_G` instead of `??1` and the whole file reports garbage lengths
  (v440: `0x447b90 DIFF(21) len=30` against a real 11-byte dtor). The convention already in
  `src/ApplSetupWnd.cpp` is the right one: a bare `// FUNCTION: LOCO 0x<addr> (??_G<Class>
  scalar deleting dtor -- compiler-generated around ~T() below; no source of its own)` line with
  no definition following it.
- **When a new class is a SIBLING of an already-matched one, diff the function SIZES first and
  transcribe the twin's body as a copy.** `ApplSetupWnd::OnTimerDefaultMaybe` (0x40b180, 822 B)
  and `ApplSetupWnd::LayoutAndDrawLabel` (0x409770, 511 B) both landed EXACT on the FIRST compile
  as near-copies of `NetSetupWnd`'s same-named methods — the two setup pages share a label-crawl
  animation, a layout pass and a button-flash idiom almost statement for statement. Six of
  `src/ApplSetupWnd.cpp`'s thirteen functions were first-compile EXACT for this reason. Check the
  sibling's file before reading the disasm.
- **⭐ A DEAD `test eax,eax` on a discarded call result is REAL SOURCE, not noise — it means the
  original branched on that result into two arms that then cross-jumped (v442, 0x4532a0 /
  0x453320).** The tell is a function that compiles 2 bytes SHORT with an otherwise perfect
  structure, and a `--dump` whose only complaint is a missing flag-setting instruction between an
  argument push and the next `call`. Do NOT try to reproduce it by making the result *look* used:
  a discarded `BOOL`/`bool` local, an empty `if (f()) {}` and an empty `if (!f()) {}` were all
  refuted — cl dead-codes the flag set in every one. What works is an actual branch whose arms
  both do the same thing; `if (!f()) { g(); return; } g();`, `if (!f()) g(); else g();` and
  `f() ? g() : g();` all compile BYTE-IDENTICALLY (the arms tail-merge, the conditional jump goes
  away, and only the `test` survives), so pick whichever reads as real code — an early-return
  error path usually does. This is the constructive counterpart to the existing tail-merge
  bullets: there, cross-jumping was the thing to work around; here it is the thing to reproduce.
- **⭐ A char-array LOCAL of exactly 16 bytes gets 8-ALIGNED in the frame and is placed ABOVE a
  4-byte scalar; a 13/14/15-byte one is only 4-aligned and sinks to the bottom (v443,
  `LocalPlayerIdentity::LoadProfile` 0x4530c0).** The tell is a residual consisting purely of
  displacement bytes on two locals that have swapped slots, at otherwise-identical length and
  instruction count — here `szName` at `esp'+0x08` with `dwWritten` at `+0x04`, against the
  original's `szName` at `+0x04` / `dwWritten` at `+0x14`. `char szName[13]` closes it, and 13 is
  also the honest size (it mirrors the class's own `name[13]`). **Local DECLARATION ORDER is NOT
  the lever here and probing it is a waste of compiles: all 24 permutations of the leading four
  locals were compiled and every single one produced the byte-identical frame.** Reach for the
  ARRAY SIZE first whenever two adjacent locals have swapped slots; reach for declaration order
  only when one of the locals is a non-POD (see the ctor-barrier bullet above).
- **A constant that several member stores share is materialized at the SOURCE POSITION of the
  first assignment that uses it, even though the stores themselves are scheduled elsewhere
  (v443, `BuildToolCursorWnd::BuildToolCursorWnd` 0x436b20).** The ctor ends with two `-1` stores
  which cl sinks to the very end of the body regardless; what moves is the `or reg,0xffffffff`
  that feeds them, and with it whether the vptr store lands before or after. Writing the two -1s
  last — the obvious spelling — costs DIFF(55) at 24/24 instructions; hoisting them to the top
  costs DIFF(53); only a MID-body position reproduces the original's `or ecx,-1` early / vptr
  store late pairing. When a ctor's residual is "the vptr store is in the wrong place", do not
  look for a lever on the vptr: slide the sentinel-valued assignments through the body instead.
- **A `void`-returning function whose body is a bare `ret` cannot have been declared with a
  return type — cl 11.00 rejects falling off the end of a value-returning function as an ERROR
  (C2561), not a warning (v443, `WindowBase::NoOpVirtualMaybe` 0x426130).** Under later MSVC this
  is the C4716 warning, so a `void *` placeholder on a shared 3-byte no-op vtable stub reads as
  harmless and can sit in a header for many sessions. It is not harmless: it is a provable
  statement about the original's declaration. Any no-op stub whose whole body is `ret`/`ret 0xN`
  with no `mov eax` returns `void`, full stop.
- **⚠ Adding a parameterized method declaration to a shared, position-sensitive header can cost
  far more elsewhere than the function it unblocks is worth — MEASURE BEFORE KEEPING (v443,
  `src/CursorDesc.h`).** `CursorDesc::CursorDesc` (0x424af0, 67 B) transcribes EXACT on the first
  compile, but declaring it — or, independently, widening the sibling `virtual void Load()` to its
  real two-argument signature — each rotate `src/Obj0x4779e0.cpp` and turn its
  `ParseEntryExitMaybe` (0x41f0c0) from EXACT into DIFF(19) at identical length, −489 B. Net
  −422 B, so the ctor stays parked and `Load` stays deliberately mis-declared as no-arg. The
  header already carried a warning that "ANY parameterized method decl flips it"; a bisect
  confirmed that covers a CONSTRUCTOR declaration too. The general rule: on any header a sibling
  TU has flagged as position-sensitive, a new declaration is a repo-wide change — run the full
  `progress.py` table and read the OTHER TUs' rows before believing a local win.
- **⭐⭐ The DECLARATION SCOPE of an address-taken aggregate local is a real lever, worth one
  compile whenever a rect/point/struct is built inside a loop.** VC5 hoists the storage to the
  same stack slots either way, so the frame layout tells you nothing — what changes is how
  conservative the optimizer is about the stores through it. Declared INSIDE the loop body, the
  local has not escaped yet at the point of the stores, so VC5 proves the surrounding
  `this->`-relative loads cannot alias it: it CSEs each repeated field read down to one load and
  collapses `rc.left = A; rc.left -= B;` into a single store. Hoisted to FUNCTION scope, the
  `&rc` that later escapes to the API call makes it treat every store as potentially aliasing,
  so it keeps both stores and RE-READS each field afterwards. Confirmed on
  `CarNetState::RemoveDecalAtPoint` (0x442d30, v445): identical source otherwise, inner scope
  gave DIFF(135) at 176 B / 70 insns, function scope gave EXACT at 193 B / 76 insns — the entire
  6-instruction gap was two suppressed byte re-loads, their two zeroing `xor`s, and two
  collapsed stores. **Diagnostic signature: the candidate is SHORTER than the original and the
  missing instructions are all redundant re-loads of the same few fields, interleaved with
  stores to a local that is passed by address later.** Note this is the opposite polarity from
  the usual "cl re-uses what the original reloads" register-budget park (which has no source
  lever) — here there IS one, and it is free.
- **`pKind->table[i * 2]` where the index is used TWICE loses its scale and can flip the SIB
  base/index order — a 2-byte residual with no known source lever.** As long as the index
  appears once, VC5 folds the `* 2` into the addressing mode as a scale (`[table + i*4]`) and
  matches. Use the same index expression at two sites and it CSEs the doubling into its own
  register, drops to scale 1, and then which of the two registers becomes the SIB *base* is a
  coin flip: the original may encode `[offset + table]` where the candidate encodes
  `[table + offset]`. Same instruction count, same registers, 2 bytes. Seen on
  `NameAnchorMaybe::NameAnchorMaybe_ResolveTileAnchorMaybe` (0x40b740, v445) whose own scan loop,
  one block earlier, keeps the folded scale and matches exactly. Measured and byte-neutral:
  swapping the addition operands, and `2 * n` vs `n * 2`. Measured and strictly WORSE: hoisting
  the doubled index into a named local (it also kills the table-pointer reload the original
  issues twice). Park it.
- **⚠ A class-qualified call `Base::Method(...)` to a method the intermediate class does not
  DECLARE silently binds to whatever ancestor does declare it — and relocation masking means the
  byte-match cannot tell you.** `verify.py`/`match.py` mask relocations, so a direct call to the
  wrong function is byte-identical to the right one; `lint_idiom.py`, `lint_calls.py` and
  `lint_ghidra_sync.py` are all blind to it too. Found in v445:
  `PlacementCursorMaybe::SetTypeMaybe` calls `AnimDescRefObj0x477488::RepositionWithHotspot(...)`,
  but that class carried no such declaration, so it resolved to the root
  `RectFlagObj0x477820` member and emitted a call to 0x436a60 where the original calls 0x405c00 —
  and the function had been reported EXACT for many sessions. **Whenever a header comment says a
  virtual is "overridden by X at 0x… — not declared here", grep for `X::Method` call sites before
  believing them:** every one is binding somewhere else. The fix (declaring the override) may
  itself be a position-sensitive header change worth measuring repo-wide.
- **⚠ …and v446 generalized that from one instance to a whole CLASS, with `tools/lint_alias.py`
  to find it: a function TRANSCRIBED under name N, but declared and called from another TU under
  a different name M, emits a call to a symbol that exists NOWHERE in the project — and every
  check in this repo passes.** Same masking reason as the bullet above, plus two more: we never
  link, so the undefined symbol never surfaces; and `lint_ghidra_sync.py` accepts BOTH the
  fully-qualified Ghidra name and its `::`→`_` flattened spelling, so `WindowBase::CenterRectInRect`
  and `WindowBase_CenterRectInRect` can each individually agree with Ghidra while disagreeing with
  each other. That is the v446 example, found by accident: `src/WorldActionCursor.h` declared its
  own `void WindowBase_CenterRectInRect(int *, int *)` alias and its inline
  `CenterModeAnimOverWidgetMaybe` called it — inlined at all five
  `SelectDecorObjAndDispatchModeMaybe` sites — while the real function is `CenterRectInRect(RECT *,
  RECT *)` in `src/WindowBase.h`. **The oracle is the marker set:** an address carrying a
  `// FUNCTION: LOCO 0x…` marker HAS a definition, so any OTHER spelling of that same address is
  a call that cannot reach it. `tools/lint_alias.py` reports exactly that (informational, never
  gates a build, like `lint_names.py`); it filters `X`-vs-`~X` declaration-line artifacts, `_vNN`
  vtable placeholders, and addresses with more than three declarations (those are ICF-folded
  shared stubs like 0x422ea0's `WindowBase_DefWindowProcStub`, which ~18 genuinely distinct
  default vtable slots share). It found **16** live instances at v446. **The fix is always the
  same and is the same work as the class-I idiom debt:** delete the local alias and include the
  header that already declares the transcribed name — so treat a class-I finding as a
  wrong-call-target hazard, not merely a type-drift one. Every such fix is a header change and
  needs a full `progress.py` per-file table diff. ⚠ **The lint's address scan reads COMMENTS
  too (v500):** writing a DEFINED function's `0x4xxxxx` address in a comment (e.g. an
  idiom-exempt justification or an autopsy) anywhere a nearby code line pairs a DIFFERENT
  `Name(` with it produces a spurious "aliased as `Name`" finding. Spell the address without
  the `0x` prefix in prose (`4588b0`), or keep the comment far from any `Name(` line.
  The `lint_ghidra_sync.py` twin of the same trap (v517): a trailing comment that names a
  global by its full `DAT_004xxxxx` spelling puts that address on the CODE line too, and a
  function-style cast `(Type *)&DAT_…` then parses as an fn-decl → spurious PARSEFAIL —
  keep `DAT_…` spellings out of trailing comments on cast/call lines.
  Related: a `// idiom-exempt` / `// TODO: idiom` tag only counts on the SAME LINE as the
  finding (lint_idiom reads the line's own comment) — a multi-line comment block above the
  flagged line does NOT tag it.
- **⚠ Corollary worth internalizing: the TU-LOCAL METHODS-ONLY VIEW STRUCT, this project's
  standard cross-TU callee-declaration pattern, is the main *producer* of that class.** A view
  struct declares the callee as a MEMBER, so it mangles with the view's own class name; the moment
  the same function is transcribed somewhere as a free function or as a member of its REAL class,
  the view's call site is pointing at nothing. Eleven of v446's sixteen are exactly this shape
  (`AppWindowMainView0x462e90::LoadConfigDirectories` vs the transcribed free
  `AppWindow_LoadConfigDirectories`, `UIResourcesMainView0x462e90::FUN_00447330` vs
  `UIResources::LoadLocaleString`, and so on). The pattern is still correct for a callee that is
  genuinely UNTRANSCRIBED — nothing to bind to either way — but it must be retired the moment a
  real definition lands.

- **⚠⚠ A RECORDED HEADER PRICE IS A HYPOTHESIS, NOT A MEASUREMENT — v448 re-measured three of
  them and TWO were wrong, both in our favour.** This repo carries measured "dial" notes on
  several shared headers saying that adding a declaration costs a named function its EXACT.
  Those notes are written at one dial position and go stale as soon as any other header moves.
  Re-measure before believing one:
  * `src/WorldActionCursor.h` recorded (v341) that promoting `InitTrainCouplingMenuIconsMaybe` +
    `GetOrCreateIconItemMaybe` onto the real class "measurably rotated the OTHER 4 already-EXACT
    sibling functions in that same TU". At the v448 position it rotated NOTHING — and it
    additionally handed `src/TilePlacedObj.cpp` back `GetFrontRowTilePosMaybe` (0x458310), +60 B.
  * `src/AppWindow.h` recorded (v423) that the dial is SATURATED — "1 and 3 added declarations
    cost byte-for-byte the same". It is not; it is on a PARITY CYCLE worth 951 B (see the next
    bullet).
  * The one that DID hold: `src/DPlaySessionMgr.cpp` still loses
    `ApplSetupWnd::SelectGridCellFromPointMaybe` (0x40aba0) to any new declaration set, exactly as
    recorded. That TU's sensitivity is real and independent of the others.
  Corollary: when a promotion is blocked ONLY by a stale price, re-pricing it is often the
  cheapest byte-gain available — cheaper than transcribing a new function.

- **A shared header's declaration-count dial can be a PARITY CYCLE, not a monotone cost — so
  count your declarations and measure EACH step, not just the endpoint.** `src/AppWindow.h`,
  v448: promoting `LoadConfigDirectories` + `InitSubsystemsAndWindows` (+2 declarations) knocks
  `src/WorldBoardMaybe.cpp`'s `FindNearestObjOfCategoryMaybe` (0x457ce0) off a 951-byte EXACT;
  adding a THIRD (`CreateMainWindow`) brings it straight back, with zero collateral anywhere in
  the other 19 consumers. The victim's residual at the bad position is `insns 327/327, align=8,
  reg_pen=13` — a pure zero-register coin-flip (original keeps the value in ecx, the perturbed
  build picks eax), i.e. nothing structural changed and nothing about the SOURCE is wrong. A
  two-declaration measurement that reads "-951 B, reject" is therefore not the answer to "what
  does this promotion cost"; the three-declaration position was +723 B. PARKED.md's own
  0x42f8b0 row had already noticed the non-monotonicity ("a second and third moved it back") —
  v448 is that observation paying out. Practical rule: if a promotion prices out negative, add
  the NEXT pending promotion on the same header before giving up.

- **Spelling a `__thiscall` member as a free `__fastcall(Class *)` function is byte-identical AT
  THE CALL SITE but not necessarily INSIDE the body — promoting it can close a residual on its
  own.** `AppWindow::InitSubsystemsAndWindows` (0x406ba0) sat at DIFF(8) for many sessions as
  `int __fastcall AppWindow_InitSubsystemsAndWindows(AppWindow *pApp)`, parked as a pure
  three-slot instruction-scheduling residual with `insns 209/209, reg_pen 0, identity_miss 0`
  and eight refuted source probes. Re-spelling it as the member Ghidra reads it as — same body,
  `pApp->` becoming `this->` — took it to a full 723-byte EXACT with no other change. The
  escape hatch is only argument-passing-equivalent; VC5's scheduler still sees a different
  function. Any parked residual on a function modeled with the `__fastcall(Class *)` hatch is
  worth one re-spelling probe before it is called intrinsic.

- **A byte-sized return type can be load-bearing at the CALL SITE while the callee is completely
  indifferent to it** — so a `char`-vs-`unsigned char` bug can hide in a function that already
  matches. `AppWindow::LoadConfigDirectories` (0x4068d0) compiles to the same DIFF(21) body
  either way, but `LocoWinMain`'s call site goes DIFF(426) -> DIFF(748) when the return is
  spelled `char` instead of `unsigned char`. This is the caller-side half of v447's
  `bool`-vs-`char` lesson: v447 found the type by reading cl's own `C4800` warning on the
  CALLEE, which is exactly the signal that is absent here. When a caller's residual jumps after
  you touch a callee's declaration, re-check the byte-sized return before hunting registers.

- **A shared victim that several header levers all knock out is a SATURATING flip, not an
  additive cost — and that is usually bad news, not good news.** v442–v445 each parked a
  different in-class-dtor / declaration lever with the same reasoning: "it costs 0x452b00 its
  249 B, but 0x452b00 can only be lost ONCE, so together the cluster may be net-positive."
  v449 measured it: the premise is correct (each of the three dtors applied ALONE produces the
  *identical* `DIFF(32)` on the victim, so the cost really is paid once) and the conclusion
  still fails, because the cluster's whole available gain was +99 B against a fixed 249. **Before
  banking on a shared-victim sweep, add up the gains FIRST and compare them to the single cost
  — if the sum doesn't clear it, the sweep is dead on arrival and needs no compile at all.**
  Contrast with v448's `src/AppWindow.h` PARITY CYCLE, where a further declaration brought the
  victim BACK: parity cycles are recoverable, saturating flips are not. Distinguish them by
  applying the levers one at a time and reading the victim's residual — an identical DIFF from
  each lever means saturating; a residual that changes or vanishes as levers are added means
  parity.

- **⚠ Price a shared-header lever against the WHOLE per-file table, never against the victims a
  prior session recorded — a big victim can hide for five sessions.** The `WidgetBase.h`
  `RepositionWithHotspot` declaration had been priced at −85 B across v445 and re-quoted three
  times. Its real cost is **−852 B**: a third victim, `WorldBoardMaybe::
  FindNearestObjOfCategoryMaybe` (0x457ce0, **951 B**), was never in any of those rows. Nothing
  local to the lever hints at it; only a full `progress.py` per-file diff shows it. Note the
  aggravating factor — 0x457ce0 is the SAME function v448 pinned as the parity canary for a
  DIFFERENT shared header (`src/AppWindow.h`). **A function that is known to be sensitive to one
  header's declaration count should be treated as a suspect for every other shared header too,
  and named explicitly in the check list when pricing any of them.**

- **Two sibling locals of the same type are a real declaration-order lever for STACK SLOT
  ASSIGNMENT, and it can move a residual a long way — but verify it in the configuration you
  actually ship.** On `TutorialWnd::RestorePresenterBackdrop` (0x452b00, 83 instructions),
  swapping `RECT rectDest; … RECT rectWindow;` to `RECT rectWindow; RECT rectDest;` moved the
  residual `DIFF(32) -> DIFF(4)`, landing `align=0 reg_pen=0 identity_miss=0 insns 83/83` — the
  schedule, the register assignment AND both structs' stack slots all agreed, leaving only 4
  displacement bytes. This is the same family as the documented swap-two-sibling-locals lever,
  but note it acts on the SLOTS, so it is only available when the function has two same-type
  aggregates. ⚠ The catch: that swap is EXACT-BREAKING in the other direction — it scores
  DIFF(30) in the config where the checked-in order is EXACT. **A declaration-order win measured
  under an experimental header change is evidence about that configuration only; re-measure it
  against HEAD before checking it in, or you will "fix" a function into a regression.**

- **⭐⭐⭐ A STRUCT-TO-STRUCT ASSIGNMENT and the equivalent run of FIELD assignments are different
  code, and only the former produces VC5's inline block copy (v451, `WindowBase::
  RefreshClientClipRect` 0x425d30 — took it EXACT, 136 B).** Symptom: the candidate is ~22 bytes
  SHORT on a function whose only interesting statement is "copy one RECT into another", and the
  original shows a POINTER being materialised (`lea edi,[esi+0xc4]` / `lea eax,[esi+0xd4]`) and
  then four `mov edx,[edi+N] / mov [ecx+N],edx` pairs walking it, while the candidate emits four
  direct `mov` pairs with absolute displacements off `this` and no pointer at all. Writing
  `rectClipBounds.left = rectClient.left; …` four times is NOT the same source as
  `rectClipBounds = rectClient;` — the latter is what VC5 lowers through a base register. Applies
  to any small POD member (RECT, POINT, a 2-int pair); when a function copies one whole struct
  member into another, spell it as one assignment first and only fall back to field-by-field if
  that is what the pointer-free displacement form proves.

- **⭐⭐⭐ WHICH ARM FALLS THROUGH decides whether VC5 CROSS-JUMPS two duplicated tails back
  together — duplicating the tail in the source is necessary but NOT sufficient (v451,
  `WindowBase::SetCaptureMode` 0x425f20 — took it EXACT, 168 B).** This is the missing half of the
  tail-duplication lever already recorded for 0x422660. Symptom: `insns 46/60`, the candidate ~40
  bytes short, and the dump shows one copy of a tail the original emits twice. The source already
  had the tail written out in both arms; the bug was that the arms were in the wrong ORDER. Read
  the guard's polarity straight off the first `jcc`: `test al,al / je <arm B>` means arm B is the
  JUMPED-TO arm and arm A is the FALLTHROUGH, so the source tests for A's condition and
  early-`return`s out of it. With the arms the other way round VC5 recognises the two tails as
  identical and folds them; with the original's order it does not, because the fallthrough arm's
  tail ends up allocated to different registers than the early-return arm's. Corollary: two
  source-identical tails that the ORIGINAL keeps separate are evidence about arm order, not about
  whether to duplicate.

- **⭐⭐ A REDUNDANT third compare of a condition already known to hold means the source tested it
  in TWO INDEPENDENT `if` STATEMENTS, not one if/else (v451, `WindowBase::ScheduleModeTransition`
  0x426020 — took it from total 54695 to DIFF(2) on this change alone).** Symptom: the original
  reads a field once into a register and compares it twice (`mov eax,[esi+0x14] … cmp eax,ecx /
  jne <body> … cmp ecx,ebx / je <ret> … cmp eax,ecx / je <skip>`), where the third compare is
  provably redundant — control only reaches it when the two are already known equal. An if/else
  CSEs that away and emits a memory-operand compare instead (`cmp [esi+0x14],ecx`) plus an extra
  `jmp`. The shape that reproduces it is:
  ```c
  if (a == b) { if (b == NULL) return; }   /* first statement */
  if (a != b) { …assign… }                 /* second, separate statement */
  ```
  VC5 keeps the redundant test because the two `if`s are separate statements, but is still smart
  enough to jump the first one's `jne` straight into the second one's body. Same family as the
  loop-form levers: when the original looks like it is doing pointless work, the source usually
  said it twice.

- **⭐⭐ A `char` FLAG SEEDED TO 1 AND CLEARED IN A BRANCH is a real local, not a comparison at the
  call site (v451, 0x426020).** Symptom: the original reserves a stack slot in the prologue
  (`push ecx` with nothing pushed), stores `mov byte ptr [esp+0xc],1` at function entry, stores
  `mov byte ptr [esp+0xc],bl` (the zero register) inside one branch, and later just pushes that
  slot as an argument — while the candidate emits the sete-materialization idiom
  (`xor eax,eax / cmp cl,bl / sete al / push eax`) at the call. Those are different sources:
  `char bFullRedraw = 1; … if (bResetCounters) { bFullRedraw = 0; … } … F(bFullRedraw);` versus
  `F(bResetCounters == 0)`. The two are semantically identical here, so nothing but the byte
  pattern distinguishes them — treat any `sete` the original does NOT have as a hint that the
  value was carried in a variable rather than recomputed.

- **The `mov reg,<const>; push reg` vs `push <const>` tell (#18i-bis) reconfirmed on a two-arm
  TIMER call (v451, 0x426020).** `KillTimer(...); SetTimer(hwnd, id, 50, NULL);` in one arm and
  the same pair with `120` in the other compiles to VC5 cross-jumping the `SetTimer` CALL itself
  and leaving only the two `push <period>` immediates duplicated. Hoisting the call out of the
  branch into a single site fed by a `UINT uElapse` local instead forces the period through a
  register. Same rule as for a function called twice: write the call out in both arms and let VC5
  merge whatever it wants to.

- **⭐⭐ A DESTRUCTOR DEFINED IN-CLASS is what folds `??1` into the compiler-generated `??_G`
  scalar deleting destructor (v451, `??_GVideoPlayer` 0x454330 — took it EXACT, 77 B).** Symptom:
  the marker's COMDAT compiles to ~30 bytes of thunk (`call ??1 / test flag / call operator
  delete`) while the original is 77 bytes with the destructor's whole body inlined. An
  out-of-line `T::~T()` in the `.cpp` always leaves a separate `??1` COMDAT that `??_G` calls;
  defining the body inside the class declaration makes it inline, so `??_G` absorbs it and the
  unreferenced `??1` COMDAT is discarded by the linker. Same shape as `~DSound`. ⚠ Definition
  ORDER in the `.cpp` is NOT a substitute — VC5 declined to auto-inline a later-defined member
  under `/Ob2`, and moving its definition above the callers changed nothing. ⚠ Keep the
  `// FUNCTION: LOCO <addr> (??_G… )` marker in the `.cpp` even though the body now lives in the
  header: a marker parked in a header is counted by nothing, and removing it silently drops the
  function from `progress.py`.

- **Before deciding whether a body needs a `call` or an inlined copy of a small member function,
  check the ORIGINAL's call sites for that member (v451, `VideoPlayer::CloseWindow`).** Xrefs
  showed all five callers in another TU issuing real `call 0x4544a0`, which proves the function is
  NOT inline in the header — so the two in-TU sites that clearly contain its body inlined must
  have that body written out in the source, not a call. One `get_xrefs_to` settles a question that
  is otherwise several compile-and-look cycles.

- **VC5 CROSS-JUMPS identical tails but not identical CALLS in the middle of a block, so which
  statement you duplicate matters (v451, `UiIconListItem::HandleTextEditKey` 0x449100).** The
  original has exactly one `Draw()` call site and one feedback-sound call site, with `Draw()`
  falling straight through into the sound. Writing the accepted-edit path as
  `if (ok) { …; Draw(); }` in both arms and then one shared sound call after the branch produces
  TWO `Draw` calls (insns 62/57) — VC5 folded the sound tails but left the two calls. Writing the
  REJECTED paths as early `return`s that each carry their own copy of the sound call, with the
  accepted path falling out of the if/else into a single `Draw(); Sound(); return 1;`, produces
  the original's one-of-each (insns 59/57, total 69236 → 36783). Rule of thumb: duplicate the
  statements at the END of a path (which fold), not ones with code after them (which do not).

- **An unsigned length guard's OPERAND ORDER survives into the encoding — `cmp ecx,[mem] / jae` vs
  `cmp [mem],ecx / jbe` (v451, 0x449100).** `if (nLen >= (unsigned)nTextLen)` and
  `if ((unsigned)nTextLen > nLen)` are the same predicate and the same instruction count, but VC5
  puts whichever operand the source names FIRST into the register slot. Read the original's
  `cmp` and write the comparison with that operand on the left. Same family as the
  `m_count > nCapacity` lever recorded for 0x435d10.

- **A SIGNED `(short)` cast forces a 16-bit `cmp`; an `(unsigned short)` cast lets VC5 widen it to
  32-bit (v462, `HasOpaquePixelInWorkSurfaceRect` 0x42c9f0).** The original's colour-key test
  compares its red channel with `cmp ax,0x1f` (4 bytes) and its blue channel the same way. Writing
  the red side as `(unsigned short)((g_nRBitMask & pixel) >> g_nRedShiftPos) != 0x1f` emits a
  3-byte `cmp ebx,0x1f` instead: the operand was just masked with `0xffff`, so cl can PROVE the
  upper 16 bits are zero and a 32-bit compare is equivalent — and it prefers the shorter encoding.
  Spelling the cast `(short)` removes that proof (a signed 16-bit value's register form carries no
  guarantee about the upper half) and restores the original's `cmp ax`. The blue side needs no
  such cast because its operand is the raw 16-bit `mov ax,[edx]` load, whose upper bits are
  genuinely unknown. Rule: when the original compares a 16-bit register and yours compares the
  32-bit one, look for a mask that gave cl the proof, and flip the cast's signedness.

- **WHERE a loop-invariant global's load sits, relative to the loop's zero-trip guard, tells you
  whether the ORIGINAL had a named local (v462, 0x42c9f0).** A source-level `int n = g_global;`
  is evaluated where it is DECLARED, i.e. BEFORE the guard. Compiler LICM instead sinks the load
  into the loop PREHEADER, i.e. AFTER the guard. So a load sitting after the `jbe`/`jle` that
  skips the loop entirely is the compiler's own hoist, and adding a source local to "explain" it
  is wrong even when the local happens to score better — it moves the load to the wrong side of
  the guard. Read the guard position before inventing the local.

- **`div` vs `idiv` on a `rand() % count` is a TYPE ORACLE for the count field's signedness
  (v462, `NetSessionEventQueue::PickRandomBigObjByCategory` 0x41e1f0).** `rand()` returns `int`,
  so the whole expression's signedness is decided by the OTHER operand: an `int` member gives
  `cdq; idiv`, an `unsigned` member converts the `int` and gives `xor edx,edx; div`. One such site
  can pin a field the rest of the codebase is indifferent to — here it retyped
  `nPlacedObjectCount` from `int` to `unsigned int`, byte-neutral in every other TU. Same shape of
  evidence as `LocoBitmap::width`'s own `div`-vs-`idiv` discriminator. Whenever a `%` or `/` on a
  struct field disagrees in signedness, retype the FIELD and measure repo-wide before reaching for
  a cast at the site.

- **Commutative `&` operand order is canonicalized, exactly like the commutative multiply
  (v462, 0x42c9f0).** `g_nRBitMask & pixel`, `pixel & g_nRBitMask`, and the same two spellings for
  the blue mask, plus moving the width cast onto the mask instead of onto the result, are all four
  BYTE-IDENTICAL. Do not spend probes on AND operand order; the multiply lesson generalizes.

- **VC5's register allocator is LIVE-RANGE based, not SCOPE based — re-confirmed at function scale
  (v462, 0x41e1f0).** Hoisting a 350-instruction, five-arm switch function's loop locals
  (`i`/`nTarget`/`nSeen`) out of the per-case blocks up to function scope, VC5-old-for-scope style,
  is byte-identical. This is the same finding as v461's per-branch scoping probe on
  `StepDecalPickerScroll`, now shown to hold for whole nested blocks and not just for two arms of
  one `if`. Declaration SCOPE is never a lever; only a declaration that changes a value's actual
  live range (e.g. an extra copy, or a use added outside the loop) can move the allocator.

- **⚠ A `push ecx` local-slot reservation DESTROYS Ghidra's frame model, and the decompiled output
  is confidently wrong rather than obviously broken (v462, 0x41e1f0).** VC5 reserves a single
  4-byte local with `push ecx` instead of `sub esp,4`. Ghidra mis-attributes the slot, and the
  fallout is three-fold: (1) the reserved slot never appears as a variable, so a function that
  really ends `mov eax,[esp+0x10] / ret` decompiles with every arm reading `return 0`; (2) a
  stack PARAMETER one slot further up surfaces as the pseudo-variable `unaff_retaddr`, which
  reads like a decompiler artifact to be ignored but is in fact a real argument — here the entire
  `category` selector; and (3) blocks get dropped with "Removing unreachable block" warnings.
  Treat `unaff_retaddr` in a `__thiscall` body as a signal to re-derive the frame by hand from
  the raw disasm, counting the pushes; do NOT transcribe from that decompile.

- **⭐ Write an if/else so the arm the ORIGINAL FALLS THROUGH INTO comes SECOND — branch-arm
  ORDER is a cheap, reliable, source-steerable lever (v463, 0x44ef70).** `if (a >= b) X; else Y;`
  and `if (a < b) Y; else X;` are semantically identical but compile to opposite jump polarities:
  cl emits the condition that jumps AWAY to the second arm and lays the first arm inline. So a
  `cmp eax,ecx; jl <far block containing Y>` with X inline pins the source as `if (a >= b) X;
  else Y;` — the `>=` arm written FIRST. On 0x44ef70 that single reordering, applied to the two
  carousel-arrow tails, was the ENTIRE residual: DIFF(5) over 532 bytes → EXACT.
  **The tell is unmistakable in `asmscore.py --dump`: an `S` (swap) row on a pair of conditional
  jumps whose TARGET is the same on both sides** (`jl 0x1e6` vs `jge 0x1e6`) means only the arm
  order is wrong, nothing else — check for this before reading any other row.

- **⭐ MSVC's compare-chain switch layout is a SOURCE-ORDER oracle, but it must be read in
  REVERSE; a jump-table switch's layout is read FORWARD (v463, 0x44ef70 / 0x44e940).** For a
  chain (`sub eax,K; je A; dec eax; je B; dec eax; jne end;`) cl lays the LAST case inline right
  after the chain and places earlier cases after it in DESCENDING order — so blocks appearing as
  4,3,2 mean source order 2,3,4. For a JUMP TABLE the case blocks appear in genuine source order,
  and that order is NOT necessarily sorted: 0x44e940's five arms are laid out 0x2804, 0x2802,
  0x2801, 0x2803, default, and transcribing them in exactly that (unsorted) order byte-matched on
  the first compile. Refines v461's "block layout is a SOURCE fact" — it is, but which direction
  you read it depends on which switch form cl chose.

- **`sub reg,K; je` chains vs `cmp reg,K; jne` is the switch-vs-if/else-if tell, and it OUTRANKS
  a better asmscore (v463, 0x44f190).** Rewriting a two-case switch as `if (nKey == VK_LEFT) …
  else if (nKey == VK_RIGHT)` scored better on bytes (132 B / DIFF 94 vs 134 / DIFF 111) but
  emitted `cmp edi,0x25` where the original has `mov ecx,edi; sub ecx,0x25; je; sub ecx,2; jne`.
  Only a `switch` produces the subtract-chain. Same family as v462's "read the guard position
  before inventing a local": structural evidence beats the headline number.

- **`and eax,0xffff` where the original has `xor eax,eax; mov ax,[mem]` is a REGISTER-PRESSURE
  symptom, NOT a wrong field type (v463, 0x44ece0).** Both spellings ARE the `unsigned short` →
  `int` widening; the original merely had a spare register and could pre-clear a fresh one, while
  cl reused the register that already held the base pointer and had to mask after the fact. Do
  not "fix" it by retyping the field `int` — that removes the widening ENTIRELY and is strictly
  worse (DIFF 121 / 150 B vs DIFF 98 / 155 B here). The widening's mere PRESENCE is the evidence
  that the field is a 16-bit unsigned; only `movsx` would argue for signed (see the signedness
  bullet above).

- **⚠ A shared header's "declaration COUNT is the rotation budget" rule is TOO COARSE in one
  direction and TOO PESSIMISTIC in the other — price both cases, assume neither (v463).**
  (a) Changing an EXISTING declaration's SIGNATURE with the count held constant rotates consumers
  just as hard as adding one: retyping `src/WidgetBase.h`'s placeholder `virtual void *_v19();`
  to its real `virtual char UpdateNodeHoverMaybe(MenuNodeObj0x477568*, int, int)` cost **−1163 B**
  across three TUs (`WorldBoardMaybe.cpp` −951 via the `FindNearestObjOfCategoryMaybe` 0x457ce0
  canary yet again, `PlacedObjRegistryMaybe.cpp` −152, `TilePlacedObj.cpp` −60) against a +616 B
  upside, so it was reverted. This refines v355/v356's "it is DECLARATIONS, not fields": it is
  the declaration LIST as a whole, not merely its length.
  (b) Conversely, ADDING a declaration to a header with a rotation-hazard reputation is not
  automatically negative: `src/UIResources.h` (29 consumers, and carrying two standing v340/v356
  "do not declare things here" notes) gained `TileKind_GetOrLoadDescriptorNoAlias` at ZERO cost to
  `DPlaySessionMgr.cpp` **and** a free +60 B, flipping `TilePlacedObj::GetFrontRowTilePosMaybe`
  (0x458310) DIFF → MATCH. Measure the specific change; the note on the header is a prior, not a
  verdict.
  (c) **A shared header's `#include` LIST is a rotation lever in its own right, independent of any
  declaration it owns — and the choice between `#include <sys.h>` and a forward declaration of the
  same type is NON-MONOTONIC, so measure BOTH spellings repo-wide (v471).** Modelling `RFIndex`'s
  `.RFD` handle as a real `FILE *` required the type in `src/DSoundChannel.h` (29+ transitive
  consumers). Pulling in `<stdio.h>` scored **135695 B / 578 funcs**; the minimal
  `#ifndef _FILE_DEFINED / struct _iobuf; typedef struct _iobuf FILE;` forward declaration — which
  adds strictly FEWER declarations and is the instinctive "safer" choice — scored **135420 / 574**,
  275 B and 4 functions WORSE. Neither spelling dominates per-TU either: the heavy include flipped
  `PeerTrainNode.cpp` +474, `DPlaySessionMgr.cpp` +166 (recovering 0x40aba0's long-standing 166 B
  debt for free), `TilePlacedObj.cpp` +83 and `LocoBitmap.cpp` +124 while costing
  `WorldBoardMaybe.cpp` −572 (0x4553e0, the largest coin-flip casualty on record). "Fewer
  declarations" is NOT a proxy for "less rotation" — only the per-file table is.
  (d) ⭐ **(a) is real but too broad: within a signature change it is specifically the PARAMETER
  LIST that moves the dial. Names are free and RETURN TYPES are free (v485, bisected in four
  steps).** Giving six of `src/Obj0x477798Family.h`'s declared-only placeholder slots
  (`_v02`…`_v12`) their real identities cost `RoadVehicleActor::CanStandAtMaybe` (0x454050) 504 B
  and `TutorialWnd::RestorePresenterBackdrop` (0x452b00) 249 B. Bisecting the change into its
  three independent components isolated it cleanly:
    - **Renaming all six** — `_v02` → `ReleaseStorage`, `_v05` → `RemoveAll`, … — **FREE**, both
      canaries stayed MATCH. (Consistent with the standing "a pure identifier rename can never
      change generated code" rule, and worth knowing it holds for a header's whole declaration
      list, not just a function's locals.)
    - **Changing all six RETURN TYPES** on top of that — `void *` → `void`, `int`,
      `TilePlacedObj *` — **also FREE**, even though every mangled name changes length.
    - **Adding ONE parameter** to any one of them — `RemoveAndDeleteAt()` →
      `RemoveAndDeleteAt(unsigned int)` — **costs both canaries**, and it makes no difference
      WHICH of the three declarations receives it.
  It is a **THRESHOLD, not a parity bit**: two added parameters and three added parameters cost
  exactly the same as one, and there is no "pay it back by adding a fourth". That distinguishes it
  from the declaration-count dial, which IS partly compensable — at +2, +3 and +4 trailing
  declarations `WorldBoardMaybe.cpp`'s 0x457ce0 canary came back and stayed back, and
  `src/WalkerActor.cpp`'s 112 B came back as soon as slot 21 was declared, while 0x454050 came
  back at NO declaration count tried. **Practical rule: when a shared header's placeholder slots
  need real identities, take the names and the return types immediately — they are documentation
  for free — and treat each added PARAMETER as a priced change to bisect on its own.**
  (d) **A standing "do not declare things in this header" note is a PRIOR THAT EXPIRES — re-price
  it rather than obeying it (v472, second independent confirmation of (b)).** `src/UIResources.h`
  carried v340's explicit warning that ANY new method declaration rotates `DPlaySessionMgr.cpp`'s
  /Og state and breaks `SelectGridCellFromPointMaybe` (0x40aba0). Adding
  `UIResources::SoundBank_PreloadWavRange` — the declaration that note was written about — did
  not touch `DPlaySessionMgr.cpp` at all, cost nothing anywhere, and flipped v471's 572 B
  `WorldBoardPartial::TestOrClearFootprintMaybe` (0x4553e0) park back to EXACT for free with its
  source untouched. That is the THIRD recorded instance of an unrelated header edit un-flipping a
  parked coin (after `TilePlacedObj`'s 0x458310 / 0x4588b0). ⭐ **Corollary for park hygiene: a
  row parked as "/Og TU-state rotation, source untouched" should be RE-CHECKED after any header
  work in the same session, not carried forward on faith** — v471 predicted exactly this and it
  paid out one session later. The measurement is free (you must run the full per-file table diff
  after a header change anyway); what costs is believing the note.

- **⚠ "The shared address means the linker ICF-folded it" is a HYPOTHESIS, and in this binary it
  is FALSE — test it before building a class model on top of it (v485).** The Obj0x477798
  collection family looks exactly like a class template: six vtables, and the slots that never
  name the element type hold ONE address across all six while the slots that do name it hold a
  distinct address per instantiation. Five sessions of notes read that as ICF folding identical
  template instantiations. The disproof is one `diff` of two disassembly ranges: the two
  registry categories' slot-10 bodies (0x4359a0 / 0x436040) and slot-17 bodies (0x435b60 /
  0x436140) are **byte-identical apart from their own branch targets**, down to naming the same
  `.rdata` growth constant — and they were NOT folded. A linker that declined to fold those
  cannot be what produced the single addresses elsewhere.
  The true model is simpler and pre-STL: an ORDINARY, non-template base class compiled exactly
  once, plus a small per-element-type subclass that overrides only the members that genuinely
  need the type — the destructor, "copy-construct an element", "store an element destroying the
  old", and (on the sorted derived) "insert" and "compare". Slots with one address have one
  because one copy was ever compiled.
  **Two things follow that are worth generalising.** (i) Byte-identical twins at two addresses
  are POSITIVE EVIDENCE ABOUT THE LINKER, not a curiosity to note and move past — they place a
  hard upper bound on what folding can explain anywhere else in the image. (ii) A per-instantiation
  address whose body names no type is telling you the SOURCE was duplicated (a second `.obj`, a
  macro, a copy-paste), not that the type leaked in; do not go hunting for the type reference.
  Same family as the vtable-slot and `padN` rules: a model merely CONSISTENT with the bytes is not
  a model that is TRUE.

- **⭐⭐ VC5's `/O2` implies `/Ob1`, NOT `/Ob2` — cl NEVER auto-inlines a function that is not
  marked `inline`, and this is a load-bearing structural fact, not a tuning detail (v472).**
  If an original function is obviously a copy of a helper that also exists out-of-line elsewhere
  in the image, the source MUST have marked that helper inline; transcribing the helper plainly
  and calling it will compile to a fraction of the original's length (here 58/53 B against
  153/149 B) and read as a badly incomplete transcription rather than as an inlining difference.
  ⚠ **But the converse move is a trap with no good outcome under these flags: marking the helper
  `inline`/`__inline` makes cl emit NO out-of-line COMDAT for it at all.** If the helper has real
  callers in other TUs — check with `get_xrefs_to` before deciding, 0x4472b0 had ~30 — that copy
  genuinely exists in the image and must be claimable, so the inline spelling both loses it and
  leaves its `// FUNCTION:` marker with no COMDAT to pair against, which silently mis-pairs every
  LATER marker in the file and corrupts their scores too (the mis-paired dump looks like ordinary
  register noise). The two requirements are mutually exclusive; take whichever side is worth more
  bytes, and say so in an autopsy so the next session does not re-derive it. Tells that you are
  looking at an inlined copy rather than a different function: the inlined copy addresses the
  callee's member arrays ABSOLUTELY off the singleton (`[esi*4+0x49161c]`) where the out-of-line
  copy goes through `this` (`[ebx+esi*4+0xc034]`), and it loads `ecx` with the singleton's
  literal address for onward member calls instead of inheriting it.

- **⭐ Naming a CONCRETE GLOBAL OBJECT as the receiver lets cl DEVIRTUALIZE a virtual call into a
  direct one; the original often does not, so route it through a POINTER LOCAL (v472).**
  `DecorCategoryMgrMaybe_00485494.GetCategoryObjByIndexMaybe(n)` compiles to a plain
  `call <target>`, whereas the original dispatches (`mov edx,[0x485494]; mov ecx,0x485494;
  call [edx+0x20]`). Hoisting `DecorCategoryMgrVtblProbe *pMgr = &DecorCategoryMgrMaybe_00485494;`
  and calling through `pMgr` restores the indirect form. Worth DIFF(133) → EXACT on 0x45a330 by
  itself. The residual is unmistakable in `--dump`: a lone `-` row for the vtable load
  (`mov edx, dword ptr [0x485494]`) with the following `call dword ptr [edx+0x20]` marked `S`
  against a direct `call`. ⚠ This cuts the other way too — if the ORIGINAL has the direct call,
  do NOT introduce the pointer local.

- **⭐ PROCESS: once ONE method of a leaf class byte-matches, dump that class's VTABLE and mine
  the whole thing as a cluster (v463 — 4 functions, 1578 B, out of one class in one session).**
  `objdump -s -j .rdata --start-address=<vtbl> --stop-address=<vtbl+0x80>` gives every slot's
  target address in one read; cross it against `grep "FUNCTION: LOCO" src/` and every unclaimed
  entry is a sibling that already has (a) a home TU with the right includes, (b) a proven field
  model, (c) a typed `this` in Ghidra, and (d) its slot NUMBER for free — which is most of the
  signature. The caveat is (a) above: a sibling whose base declares it only as an `_vNN()`
  placeholder may be unreachable by name at an affordable price, so check that BEFORE starting.

- **⭐⭐⭐ A COMPILER-GENERATED `operator=` IS A STRUCT-LAYOUT ORACLE — the bytes it DECLINES to
  copy are a direct readout of which members are real (v474, `CarNetObj_ApplyNetState` 0x40d770
  corrected three `CarNetState` members at once).** A memberwise copy-assignment moves exactly
  `sizeof()` per member, in declaration order, and skips BOTH the vptr and every byte of implicit
  alignment padding. So when a function's whole body is `bigMember = *pSrc;`, reading its gaps
  settles questions no access site can:
  * A gap where you have a DECLARED member ⇒ that member is really implicit padding. `Unk0x06`
    died because a WORD moves at `+0x04` and the very next move is the dword at `+0x08`; a
    `pad0x396[2]` died because the array copy ends at `+0x396` and the next move is `+0x398`.
    ⚠ This is only conclusive for a DECLARED member — a declared `unsigned char pad[2]` WOULD be
    copied, which is exactly what makes its absence proof.
  * A member copied SHORT of its declared size ⇒ the declared size is wrong. `CardNameB` shrank
    21 -> 20 because `nameA` moves 21 bytes (`rep movs` 5 dwords + 1 byte) while `nameB` moves
    only 20 (5 dwords flat), with `+0x39` left as pad before a 2-byte-aligned word.
  * ⭐ The COPY MECHANISM also discriminates the member's TYPE: a nested CLASS member gets
    `rep movs`, while a plain `char[N]` array member gets a BYTE LOOP and a small struct array
    gets an unrolled per-element loop. That is how `szDescription` was confirmed as `char[80]`
    (byte loop over `0x43..0x92`) rather than Ghidra's `dword[20]`, and it independently
    re-confirms the `CardNameA`/`CardNameB` member-class model.
  Sanity check the result the usual way: every gap should be explained by ordinary alignment and
  the total should close at the `operator new` size. ⭐ And expect the correction to DISSOLVE
  `// sic:` notes rather than add them — four separate "only 20 of the 21 bytes are ever copied"
  autopsies across three TUs were all describing one phantom byte that does not exist, and the
  three `memcpy(..., sizeof(x) - 1)` calls written to compensate became plain `sizeof(x)`.
  Corroborate against sibling types before committing: a strcpy DESTINATION sized `[20]` by the
  original programmer, and two wire-record views that need the same pad byte, all agreed.

- **⭐⭐ A CONSTRUCTOR IS THE OTHER HALF OF THAT ORACLE — and the cheapest way to VERIFY a layout
  you just changed (v475, `CarNetObj::CarNetObj` 0x40d500, EXACT on the first compile).** Where
  `operator=` reads out size and padding, a ctor reads out the field SET and its declaration
  ORDER: it stores to every field the class initializes, at literal offsets, in one function, and
  the compiler emits the base-ctor call, the member-ctor calls and the vtable store around them in
  a fixed order you can check off. ⭐ **So the move after promoting or extending a struct is to
  transcribe its ctor immediately.** 0x40d500 writes all nine fields of the +0x430 tail that had
  just been promoted out of two TU-local views; matching it byte-for-byte on the first compile
  confirmed every one of those offsets, the base-ctor argument list (`(nKindId, -1, 0, 0)`), and
  the fact that the whole tail is populated only under the base's `bValid` — none of which any
  single access site could have established. A layout error anywhere in the range would have shown
  up as a wrong displacement, not as a plausible-looking near-miss.
  Two mechanical notes that made it land first try: **do NOT hand-write the vtable store** when
  the class (or its base) declares real `virtual`s — cl emits it itself, and an explicit
  `this->pVtbl = g_vtable0xNNN;` only compiles for classes modeled with an explicit vtable FIELD
  (the `PeerTrainNodePartial` pattern); and a `virtual` called on `this` from inside a ctor
  **devirtualizes to a direct `call`**, which is exactly what the original shows — so write the
  natural `this->SetAnimFrame(...)`, not a hand-qualified call.
- **A field store that lands BETWEEN the base-ctor call and the derived vtable stamp belongs
  to the BASE ctor's own body — that is how a ctor tells you which LEVEL of the chain owns the
  field (v514, `BigObjTrackingSetsMaybe::CtorMaybe` 0x4238c0).** The derived ctor's fixed
  emission order is: base-ctor call → own-vtable store → mem-initializers → body. So a zero
  store sitting after the reserve call but before the derived stamp cannot be the derived
  half's at all: carry that field on the BASE class and zero it at the base ctor's tail.
  (Here: the registry's live count `m_0c`, which the sibling Obj0x477758 pair carries on the
  DERIVED half instead — same family, two levels, and only the ctor's store order separates
  the readings.) The scheduler can still float an independent store across the vtable store in
  either direction, so confirm against a second instantiation before moving a field.

- **⚠ A HEADER THAT NAMES A TYPE IT DOES NOT DECLARE is a delayed-action bomb, and it detonates
  in TUs you never opened (v474, `src/PostBag.h` + `IDirectDrawSurface`).** `PostBag.h` used
  `IDirectDrawSurface *` in five signatures without declaring it, relying on every consumer to
  have included `<ddraw.h>` first — which they all happened to do, for years. The moment an
  unrelated header (`src/CarNetObj.h`) started including a header that reaches `PostBag.h`, FOUR
  TUs that had never needed `ddraw.h` became `COMPILE FAILED` at once. Fix is a one-line forward
  declaration in the header that names the type (pointer-only use needs nothing more), and it is
  byte-neutral. ⚠ Use the SAME tag the real declaration uses — `<ddraw.h>` line 54 says
  `struct IDirectDrawSurface;` — because VC5 takes a type's default access from the LAST-SEEN
  `class`/`struct` tag (see the v414 bullet). Same detection rule as always: only a FULL
  `tools/progress.py` per-file table shows this, never the TU you were editing.

- **#34 A BY-VALUE STRUCT RETURN TELLS YOU WHETHER THE SOURCE INITIALIZED OR ASSIGNED, and the
  two are NOT interchangeable (v479, `WorldBoardPartial::CheckInsertSeqPerimeterMaybe`/0x456d90).**
  VC5 returns a struct through a hidden out-param pointer that the caller pushes and the callee
  hands back in EAX. `T v = f();` passes `&v` DIRECTLY, so the callee writes the named local and
  nothing else happens on return. `T v; ... v = f();` cannot: an assignment needs a materialized
  temporary, so the caller passes `&temp`, then copies `temp` into `v`. In the disassembly that
  is exactly one extra slot and one extra copy — `lea eax,[esp+0x24]; push eax; call f;
  mov eax,[eax]; mov [esp+0x28],eax` (temp at +0x24, the real local at +0x28) versus a bare
  `mov ecx,[esp+0x28]` reading the local the callee already filled. Splitting
  `TileGridPos start = pObj->GetFrontRowTilePosMaybe();` into a declaration plus an assignment
  took 0x456d90 from DIFF(666) to DIFF(56) in one compile. ⚠ The tell is easy to misread as a
  redundant compiler copy and dismiss; count the STACK SLOTS instead — two distinct slots holding
  the same 4 bytes means two source objects, hence an assignment. Cheap to try either way, and
  unlike a shape probe it costs nothing in faithfulness: both spellings are ordinary source.

- **#35 `x > N` and `x >= N+1` are DIFFERENT CODEGEN, and the difference is one opcode byte
  (v479, `TrackGraph_GetLinkCostMaybe`/0x457b60).** cl does not canonicalize an integer relational
  against a literal: `resourceId > 0x3010` emits `cmp ...,0x3011; jle`, while `resourceId >= 0x3011`
  emits `cmp ...,0x3011; jl` — same immediate, different conditional-jump opcode. Two sites of
  this were the ENTIRE residual on an otherwise byte-identical 181-byte function (DIFF 4 = 2 bytes
  x 2 sites, insns 64/64, reg_pen 0). ⚠ Do not read the immediate to decide which spelling the
  source used, because both spellings produce the SAME immediate here — read the `jl`/`jle` (or
  `jb`/`jbe`) and work backwards. A `--dump` whose only rows are `S`-marked jcc mnemonics at
  identical addresses is this and nothing else. Related to but distinct from #18n: no block moves,
  only the comparison's strictness.

- **#36 A LOOP WHOSE INDEX IS STILL LIVE IN THE BODY NEEDS AN EXPLICIT SECOND COUNTDOWN VARIABLE
  IN THE SOURCE; cl will not invent one (v480, `WorldBoardPartial::IsTrackGraphNodeAMaybe`/
  0x4573e0).** When a `for (i = 0; i < N; i++)` body uses `i` for anything besides subscripting
  the array — as a call argument, or in an expression like `a[(i + 1) & 3]` — cl keeps `i` and
  tests it directly: `inc i / cmp i,N / jl`. The original instead emits `dec n / jne` beside a
  still-incrementing `i`, i.e. a THIRD induction variable that exists only to count trips. That
  only comes from source that declares it:
  ```c
  dir = 0;
  for (n = 4; n != 0; n--) { ... uses dir ... ; dir++; }
  ```
  Writing the plain index-controlled `for` instead was the whole difference between DIFF(610) and
  MATCH on a 706-byte function. The extra variable is visible in the FRAME too: it is exactly the
  4 bytes by which the original's `sub esp,0x1c` exceeds an index-controlled version's.
  ⚠ The converse matters just as much — where the index is DEAD in the body (the loop only ever
  writes `a[i]`), cl eliminates it and synthesises the reversed counter ITSELF, so a plain
  `for (i = 0; i < N; i++)` is what matches and adding an explicit `n` there would be unfaithful.
  Three loops in 0x4573e0 settle it both ways: the first two need the explicit counter, the third
  does not. The tell is simply whether the index survives into the body.
  ⚠ NOT the same as a pointer-controlled loop — `for (pp = a; pp < a + 4; pp++)` keeps the pointer
  compare (`lea edx,[esp+0x28] / cmp esi,edx / jb`) and scored WORSE (104175) than either.

- **#37 TWO EARLY EXITS THAT SHARE ONE EPILOGUE IN THE ORIGINAL BUT NOT IN YOURS = A MISSING `||`
  (v480, `WorldBoardPartial::GetNeighborObject`/0x4579d0).** Two consecutive guard clauses each
  written as its own `if (...) return NULL;` gave each one a fully inlined epilogue
  (`pop/pop/pop/add esp,N/ret`), where the original jumps both arms to a single shared exit block.
  Short-circuiting them into one condition — `if (pKind == NULL || HasNoEntryExit(pKind, dir))` —
  produced exactly the original's `je <exit>` / `jne <exit>` pair and took 212002 -> 91964, with
  the compiled length dropping 412 -> 396. Read a `--dump` full of `+`-marked epilogue
  instructions against an original that has one epilogue as this, not as register noise; and note
  the source change is free in faithfulness, since a short-circuited guard is ordinary C++.
  Converse of the same fact: where the original DOES duplicate an epilogue, resist merging.

- **#34 addendum — the initialize-versus-assign tell fires in BOTH directions inside a single
  function, so read each by-value struct return separately (v480, 0x4579d0).** That function has
  two `POINT` returns. The first is ASSIGNED: cl gives temp and local the SAME slot and still
  emits the copy, so the give-away is a self-copy that looks like a compiler bug
  (`mov eax,[eax+4]; mov [esp+0x14],eax` writing `pt.y` over itself). The second is INITIALIZED:
  cl forwards the retbuf, emits no copy at all, and every member read goes through the returned
  pointer (`mov ecx,[eax+4]`) rather than off esp. Getting the pair right was worth
  249974 -> 68911. So: reads through EAX after the call = initialization; a copy into other slots
  (even the same ones) = assignment.

- **#38 A TOP-TESTED `while (p != NULL)` AND A `for (;;)` WITH THE SAME TEST AT THE BOTTOM ARE
  DIFFERENT CODEGEN (v480, `WorldBoardPartial::LinkTrackGraphNeighbours{A,B}Maybe`/0x457830,
  0x457900).** `for (;;) { ...; p = next(p); if (p == NULL) break; }` makes cl rotate the loop and
  DUPLICATE its top into the fall-through path — 11 extra instructions on a 202-byte function
  (`insns 78/67`). The plain `while (p != NULL) { ...; p = next(p); }` compiles to the guard-plus-
  bottom-test shape the original has (`test/je <exit>`, body, `jne <top>`) with no duplication,
  and both functions went straight to EXACT. A `do { } while (cond && (p = next(p)) != NULL);`
  with the compound condition was tried too and is WORSE than either (233 bytes). When a dump
  shows your candidate carrying a second copy of the loop head, the source is a bottom-tested
  loop that should be top-tested.

- **#36 confirmation, and the SECOND independent lever it stacks with.** v482 applied #36 to both
  `TrackGraph::BuildAdjacency{A,B}Maybe` inner direction loops (0x45ce40 / 0x45d1c0) and it moved
  them DIFF(384)/DIFF(388) → DIFF(317)/DIFF(316) — the same signature v481 recorded on 0x4573e0
  (`dec n / jne` beside a live incrementing index, versus `cmp i,4 / jl`), so #36 is now confirmed
  on three functions in two TUs. What matters for future use: **#36 did NOT change the OTHER
  sensitivity in the same loop.** These loops also depend on the direction helper's parameter
  staying BYTE-wide (`GetOppositePathMaybe(unsigned char)`), which costs one `and eax,0xff` per
  inlined copy but preserves the original's `lea ebx,[eax+0xc4]` / `lea esi,[ecx+8]` pointer
  strength-reduction; widening it to `int` collapses that into scaled `[eax+esi*4+0xc4]` indexing
  AND rotates ebx/ebp globally (317 → 566). That probe was first refuted in v431 under the OLD
  loop shape and RE-REFUTED in v482 on the explicit theory that #36 had changed its premise. It
  had not. **Generalize: a loop-shape lever and a parameter-width lever in the same loop are
  independent — fixing one does not license re-testing the other, and "the premise changed" is
  worth exactly one re-run, not a standing invitation.**

- **#39 A VTABLE SLOT NUMBER IS MEANINGLESS WITHOUT THE TABLE IT IS COUNTED FROM, AND TWO CLASSES
  IN ONE HIERARCHY CAN DISAGREE.** `WindowBase`'s vtable (@0x477c30) and `PopupWndBase`'s
  (@0x477898) are NOT prefix-compatible: PopupWndBase's is one slot SHORTER at the front, so
  `RouteMessage` sits at +0x28 in the first and +0x24 in the second, and every per-message slot
  after it is shifted by 4 (`WindowBase` +0x50 = `PopupWndBase` +0x4c = WM_MOUSEMOVE). v482 spent
  real time deriving a `PopupWndBase`-derived class's vtable base as 0x47812c by aligning it
  against `WindowBase` — three independent slots agreed, and all three were wrong, because the
  class derives from `PopupWndBase` and its true base is 0x478130. **The only safe derivation is
  to align a vtable against its OWN immediate base class's table, dumped, not against the root of
  the hierarchy and not against a slot-number comment.** The tell that something is off: a
  documented slot ("OnExit is slot 4") disagreeing with your arithmetic by a constant. That
  constant is the hierarchy shift, not an error in the comment. This also mis-set a claim in
  `docs/subsystems.md` for months (a "+0x38 is the expected DefWindowProcStub" cross-check that
  was reading WM_LBUTTONUP's slot and calling it WM_LBUTTONDOWN's) — corrected v482.

- **A sparse `switch` over scattered key codes reproduces the original's byte-remap dispatch for
  free, and makes the function LONGER than its Ghidra body extent suggests.** `BuildToolCursorWnd::
  OnKeyDown` (0x437180) switches on 0x0d/0x59/0x79/0x1b/0x4e/0x6e; cl emits exactly the original's
  `add eax,-0xd / cmp eax,0x6c / ja / movzx cl,[eax+table] / jmp [ecx*4+table]` pair. ⚠ Both
  tables live INSIDE the COMDAT, past the last instruction — Ghidra's `Body:` span reports 700
  bytes where the real COMDAT (next function's start) is 880. This is the documented `--len` trap
  in its most misleading form yet: the candidate compiled to 826, which reads as "126 bytes too
  long" against 700 and as "content-complete and slightly short" against the true 880. Derive
  `--len` from the next function's start whenever a switch is present, before concluding anything.

- **⭐⭐⭐ #40 A "NET-NEGATIVE HEADER CHANGE" IS ONLY NET-NEGATIVE AT A GIVEN PARITY — A TRUE
  DECLARATION YOU ARE FREE TO WRITE OR NOT WRITE IS A CURRENCY, AND THIS REPO HAS SEVERAL (v486).**
  Five sessions (v442–v445) each parked the same lever — `virtual void RepositionWithHotspot(int,
  int);` on `AnimDescRefObj0x477488` in `src/WidgetBase.h` — and v449 ran the repo-wide sweep they
  all deferred to and refuted it at **−852 B for that lever alone**, dominated by an unpriced third
  victim, `WorldBoardMaybe::FindNearestObjOfCategoryMaybe` (0x457ce0, **951 B**). Every one of
  those measurements was correct. v486 landed the identical lever, unchanged, at **+302 B with
  zero victims.**
  - The mechanism: 0x457ce0 answers to a **PARITY on the COMBINED declaration count of several
    shared headers** (v448 had already caught it as a parity canary for `src/AppWindow.h`). v486
    was independently holding a spare declaration in `src/Obj0x477798Family.h` — `_v15`, a real
    16th vtable slot, added only to pay back what retiring a bogus struct had taken. **Dropping
    that one declaration in the SAME commit as adding the WidgetBase.h one holds the parity**, and
    then not only do all three recorded victims survive, one of them REVERSES SIGN:
    `TilePlacedObj.cpp`'s 0x458310, recorded as a −60 B victim, is a **+143 B gain**.
  - ⇒ **PROCESS: before trusting any parked row that prices a header change as net-negative,
    ask what else could absorb the parity.** Two true declarations competing for one parity slot
    is a CHOICE — take the more valuable one and leave the other as a comment at its slot (with
    an instruction to re-declare it if the winner ever goes away). That is what `_v15` now is.
  - ⚠ **The parity is not a general-purpose escape hatch.** Re-probed at the new parity in the
    same session, the FOUR in-class-dtor levers of the same v449 cluster (`??_GIniFile`,
    `??_GResourceRef`, `??_GLocalPlayerIdentity`, `??_GPostBagFileCache`) STILL cost
    `TutorialWnd::RestorePresenterBackdrop` (0x452b00) its full 249 B — v449's "saturating flip,
    not a parity cycle" finding for THAT victim stands. Different canaries, different mechanics:
    0x457ce0 cycles on parity, 0x452b00 saturates. Probe the specific victim, do not generalise.

- **⭐⭐ #41 VC5 EMITS A CLASS'S VTABLE, ITS `??_G` THUNKS, AND THE OUT-OF-LINE COPIES ITS VTABLE
  POINTS AT ONLY INTO A TU THAT *CONSTRUCTS* THE CLASS — never into the TU that merely DEFINES its
  members (v486).** `/Gy` COMDATs make it irrelevant which TU emits an ordinary member, which is
  why this project groups members by class rather than by original `.obj`. That freedom stops at
  the vtable. `src/Obj0x477798Family.cpp` defines every out-of-line member of `Obj0x477758`, and
  its `.obj` contains NONE of: `??1Obj0x477758`, `?Add@Obj0x477758`, `??_GObj0x477758`. All four
  live in `src/PlacementCursorMaybe.cpp`'s `.obj`, which constructs one — and which is also where
  the ORIGINAL's `.obj` put them. **Put such markers where the class is constructed.**
  - ⚠⚠ **A marker whose COMDAT is absent DOES NOT REPORT MISSING.** `tools/match.py` falls through
    to marker-ORDER pairing, so the marker silently claims its neighbour's function and **every
    later marker in the file shifts by one**. v486 hit this twice. The tell is a plausible small
    `DIFF` against a mangled name that has nothing to do with the marker's address —
    `0x412580 DIFF(30) ??1Obj0x477bd0@@QAE@XZ`. Read the NAME column, not just the DIFF.
  - ⚠ **A `??_G` hint pairs by SUBSTRING, so a class name that is a PREFIX of a sibling's needs the
    mangled terminator.** `(??_GObj0x477758 ...)` silently matched `??_GObj0x477758Base@@UAEPAXI@Z`
    and reported a credible `DIFF(30)` against the wrong function; `(??_GObj0x477758@@ ...)` is
    correct. Same family as #39 — a name that is merely consistent is not a name that is right.

- **⭐⭐ #42 THE TWO `??_G` THUNKS OF A BASE/DERIVED PAIR TELL YOU WHICH DESTRUCTOR IS IN-CLASS, AND
  THE OUTER DESTRUCTORS THAT EMBED THE CLASS TELL YOU WHETHER YOU GOT IT RIGHT (v486).** A
  polymorphic class's slot 1 holds `??_G`, not `??1`. Where the destructor is small and defined
  IN-CLASS, VC5 expands it straight into the thunk (`0x412580`, 62 B, no call); where it is
  out-of-line, the thunk `call`s it (`0x4125c0`, 30 B). **Two thunks at two addresses, at slot 1
  of two different tables, over what a flat class would have made ONE destructor body, is proof of
  a base/derived pair** — that is what refuted five sessions of "flat struct" modelling for the
  `Obj0x477798` family.
  - ⚠ **But "the derived thunk calls it" does NOT mean the derived destructor is out-of-line.** A
    virtual function ALWAYS gets an out-of-line copy emitted for its vtable slot, whether or not it
    is also inline — exactly the arrangement `?Add@Obj0x477758` at 0x412440 already documented.
    v486 wrote the destructor out-of-line first: 0x412410 matched, BOTH thunks matched, and it was
    still wrong. The evidence that decides it is the OUTER destructors of classes that EMBED the
    type — `~NetSessionEventQueue` (0x41d2d0, 55 B) and `~PlacementCursorMaybe` (0x410680, 116 B)
    are each nothing BUT the embedded member's destructor chain expanded in place, and both
    collapsed to 14 B and a `call` (−171 B across two TUs) under the out-of-line spelling.
    ⇒ **When a small virtual destructor's own address matches either way, price it at its
    EMBEDDERS, not at itself.**
  - ⚠ **Corollary for transcription: three of the four statements in such a destructor are not
    source at all.** 0x412410's `m_0c = 0; vtbl = <base table>; m_count = 0; if (m_ptr) delete
    m_ptr; m_ptr = 0;` is really `{ m_0c = 0; }` plus the compiler's own base re-stamp plus the
    base destructor inlined. Transcribing a compiler-generated epilogue as if a programmer wrote
    it byte-matches indefinitely while being false — here, for thirteen sessions. Same family as
    the `pad`-name and duplicate-struct traps: consistent with the bytes ≠ true.

- **⭐⭐ #43 AN EXPLICIT `return` AND A FALL-THROUGH THAT PROVABLY RETURNS ARE NOT INTERCHANGEABLE:
  THE `return` FREES THE VALUE'S REGISTER HOME, THE FALL-THROUGH KEEPS IT (v487).** Closing
  `Ddraw_BltUpdateRect` (0x401280, 601 B) came down to one instruction — the original's
  `mov esi, eax` at 0x4013bb, homing the `Blt` HRESULT in a callee-saved register right after the
  call. Structure, block layout, branch polarity and every other instruction already agreed
  (`insns 193/194`). The cause was a spurious `else { return; }`:
  ```c
  hr = Blt(..., DDBLT_ASYNC, NULL);
  if (hr == DDERR_SURFACELOST)   { ... }
  else if (hr != DD_OK)          { ... }
  else                           { return; }   // <-- costs the register home
  ...
  if (hr != DD_OK) Ddraw_HResultToString(hr);  // the shared tail
  ```
  Deleting the `else` arm is FUNCTIONALLY IDENTICAL — the shared tail's test is false on that path
  either way, and the original does jump straight to the epilogue there — but it changes the
  allocator's answer. With the explicit `return`, `hr` is dead on that edge, so VC5 leaves the
  result in `eax` and compares out of `eax`; with the fall-through, `hr` is live into the shared
  tail across two more calls, so it gets a callee-saved home eagerly. **Deleting the arm was the
  whole fix: 601 B DIFF(205) → EXACT.**
  - The tell to look for: a residual of ONE `mov <callee-saved>, eax` immediately after a call
    whose result you then branch on, with the rest of the function already matching, on a function
    that has a shared error-handling tail. Ask whether one of your early `return`s is really a
    fall-through in the original.
  - Generalises past `return`: any construct that shortens a value's live range (an early `break`,
    a `goto` past the tail, hoisting a use) is a real codegen lever, not a stylistic choice.

- **⭐ #44 A 16-BIT-NARROWED COUNT IS `unsigned short` STORAGE COMPARED THROUGH `int` COUNTERS —
  BOTH HALVES MATTER, AND THEY FAIL DIFFERENTLY (v487).** `DDraw_DarkenRect` (0x401540) narrows
  its three loop extents with an explicit `and reg, 0xffff` and then tests them SIGNED against
  zero (`test/jle`). Three spellings, three distinct outputs:
  - **All `unsigned short` (extents AND loop counters)** → VC5 keeps everything 16-bit and emits
    `test cx, cx / jbe` plus a re-narrowing `and` inside the loop body. Wrong compare width, wrong
    signedness, extra instructions.
  - **All `int`** (extent = `(unsigned short)(x1 - x0)` assigned to an `int`) → right compare, but
    the mask lands IN PLACE (`and edx, 0xffff`), where the original copies first
    (`mov edi, edx; and edi, 0xffff`). One instruction short.
  - **`unsigned short` extents, `int` loop counters** → EXACT. The narrow storage is what forces
    the zero-extension into a FRESH register at each use; the `int` counters are what make the
    comparison a signed 32-bit one after promotion.
  - ⚠ **And do not name a temp for the un-narrowed intermediate.** `int nPitch = scratch[4];
    nPitchPixels = (unsigned short)(nPitch >> 1);` keeps the unmasked shift result live and costs
    two instructions (a copy plus a zero-extend into a new register) versus masking in place;
    inlining it to `(unsigned short)((LONG)scratch[4] >> 1)` was the last edit before EXACT. The
    `(LONG)` is separately load-bearing — the scratch element is `unsigned int`, and reading it
    straight gives `shr` where the original's signed `lPitch` gives `sar`.

- **⭐⭐ #45 A BY-VALUE STRUCT RESULT STAYS IN ITS OWN STRUCT FIELDS — DECOMPOSING IT INTO LOOSE
  SCALARS PROMOTES THE ARITHMETIC AND CHANGES THE FRAME (v487).** `TrainNet_HandleEmptySlotHandoffMaybe`
  (0x43b8c0, 567 B) ends in a four-way switch that calls one of `NetSessionEventQueue`'s
  `Pair16`-returning edge-placement quartet and adjusts the result by ±1 per axis. Written the
  obvious way — a `Pair16 pos` plus two loose `short x, y` — it does not match:
  ```c
  Pair16 pos; short x = 0, y = 0;
  case 0: pos = q.ComputeBottomEdgePlacement(); x = pos.lo + 1; y = pos.hi - 1; break;
  ```
  `pos.lo + 1` is an INT expression, so with the returned dword still live in `eax` VC5 emits
  `lea ecx, [eax + 1]` and 32-bit `xor ecx,ecx` initialisers; the original re-loads 16-bit halves
  out of memory and does `mov cx, word ptr [..] / inc cx`, off 16-bit `xor cx,cx`. It also needs
  one scratch dword more than the original (`sub esp, 8` vs a single `push ecx`). Keeping
  everything inside `Pair16` — a memory `tmp` for the by-value return and a register-resident
  accumulator, exactly the shape `DPlaySessionMgr::HandleQueuedTrainPlacement` (0x43e370) was
  already byte-matched with — is EXACT on the first try:
  ```c
  Pair16 coord, tmp;  coord.lo = 0; coord.hi = 0;
  case 0: tmp = q.ComputeBottomEdgePlacement(); coord = tmp; coord.lo += 1; coord.hi -= 1; break;
  ```
  - The tells: 16-bit `xor cx,cx`/`inc cx`/`dec dx` where you emit 32-bit `lea`, **plus** a frame
    one dword larger than the original. Both say "this value never left its struct".
  - `coord = tmp;` as its own statement is load-bearing — it is what forces the memory round-trip
    the original's `mov cx, word ptr [pair]` reload depends on.
  - ⚠ Per-`case` scoping of the temporary (`case 0: { Pair16 pos = ...; }`) is WORSE, not better
    (233459 vs 176537, and 179/187 insns): VC5 does not overlay the four scopes into one slot.
  - ⭐ **General rule: before inventing a shape for a by-value-struct call site, grep for another
    call site of the SAME function that is already byte-matched.** The precedent here was three
    switch statements in `src/DPlaySessionMgr.cpp` over the identical quartet, and copying its
    spelling went from DIFF(450) to EXACT.

- **⭐⭐⭐ #46 A DESTRUCTOR THE ORIGINAL INLINED INTO ITS `??_G` THUNK MUST BE DEFINED IN-CLASS
  (v488).** `AppWindow`'s vtable (0x4774c4) has exactly ONE slot, pointing at 0x4062a0 — and that
  57-byte function is the compiler-generated SCALAR DELETING destructor with the dtor body inlined
  into it: vptr re-stamp, the `if (g_pIniFile) { delete g_pIniFile; g_pIniFile = 0; }` teardown,
  then the `test byte ptr [esp+8],1` delete flag, `mov eax,esi`, `ret 4`. There is no separate
  `??1AppWindow` COMDAT anywhere in the image. Declaring the dtor in the header and DEFINING it in
  the .cpp cannot match at any address: VC5 emits the two-COMDAT shape instead — a 33-byte
  `??1AppWindow@@UAE@XZ` plus a 30-byte `??_GAppWindow@@UAEPAXI@Z` thunk that calls it. Moving the
  body into the class declaration (`virtual ~AppWindow() { ... }`) makes `/Ob1` inline it and the
  `??_G` goes EXACT at 57 B.
  - **The tell is a LENGTH split, not a diff:** you get two COMDATs summing to roughly the
    original's size (33 + 30 = 63 vs 57) where the original has one. If a `??_G` marker pairs to a
    COMDAT ~half the expected length, stop diffing and ask whether the dtor should be inline.
  - ⚠ The marker still has to live in the `.cpp` and carry the mangled-name hint
    (`// FUNCTION: LOCO 0x4062a0 (??_GAppWindow@@ scalar deleting dtor ...)`) — a STANDALONE
    marker with no definition under it, since the definition is now in the header. Without the
    hint the marker pairs to `??1` and reports a bogus DIFF at the wrong length.
  - The knock-on is a header dependency: the in-class body needs every type it touches complete,
    so `src/AppWindow.h` had to gain `#include "IniFile.h"`. Measure that as a header change.

- **⭐⭐ #47 MODEL A CLASS AS POLYMORPHIC AS SOON AS ITS VTABLE IS CONFIRMED — IT CAN PAY IN
  ANOTHER TU (v488).** `AppWindow` had carried `char padVtbl[4]` at offset 0 for many sessions to
  hold the vptr's space open. Replacing it with a real `virtual ~AppWindow()` is layout-neutral BY
  CONSTRUCTION (the compiler's own vptr takes offset 0, `sizeof` stays 0x28, every later field
  keeps its offset) — and it is what lets the ctor's `mov [esi],0x4774c4` be emitted automatically
  instead of being hand-written through a `pad` field (idiom class D). Measured effect across the
  23 consumer TUs: `src/AppWindow.cpp` +820 B as expected, and `src/WidgetBase.cpp` GAINED 407 B
  and one exact function it had never had. A truer type model is not merely tidier; it rerolls
  consumer codegen and the roll can come up in your favour.
  - Retire the look-alike TU-local views in the same commit: `src/Main.cpp`'s
    `AppWindowCtorModel0x462e90` (a derived class carrying only a ctor declaration) and
    `AppWindowMainView0x462e90` (a standalone struct carrying only a virtual dtor) both became
    `new AppWindow(hInstance)` / `delete g_pApp` at zero byte cost.
  - **Prophylactic first, per the vtable-slot rule:** dump the vtable before modelling. Read the
    dwords until they stop looking like `.text` addresses — `AppWindow`'s slot 1 is `0x00000000`
    and slot 2 is `0x3ff00000` (the high half of a `1.0` double), i.e. the vtable is ONE slot long
    and the rest is unrelated `.rdata`. Guessing the slot count from "how many virtuals feel
    right" invents methods that do not exist.

- **⭐ #48 `x > 0` AND `x != 0` ON AN UNSIGNED VALUE ARE THE SAME TEST AND DIFFERENT OPCODES
  (v488).** Both compile to `test eax,eax` plus a conditional jump, but `> 0` gives `ja`/`jbe` and
  `!= 0` gives `jne`/`je`. One byte, and no amount of register-allocation thinking explains it.
  `AppWindow::CheckMinimumDisplaySpec` (0x406680) was a one-byte DIFF for exactly this — the
  `VerQueryValueA` length check is `cbValue > 0` in the source. Read the mnemonic off the original
  and spell the comparison to match; the same rung of the ladder as the signedness tells below.

- **⭐ #49 A `/2` IS A SIGNEDNESS ORACLE, AND SO IS A COMPARISON'S JUMP (v488).** `g_dwScreenWidth`
  and `g_dwScreenHeight` had been declared `unsigned int` in two TUs for many sessions. They are
  `int`: `AppWindow::LoadWindowAndBalancing` halves both with `cdq; sub eax,edx; sar eax,1` (the
  signed-division-by-2 idiom — an unsigned `/2` is a bare `shr`), and `CheckMinimumDisplaySpec`
  bounds the width with `jle`/`jge` rather than `jbe`/`jae`. `g_dwScreenBpp` is likewise signed,
  pinned by a single `jg`. A wrong signedness on a global is byte-invisible until some consumer
  divides or compares it, so fix it at the first consumer that does and propagate to every TU's
  extern — `src/TutorialWnd.cpp` had already got the two half-extents right as `int`.

- **⭐ VC5 FOLDS A PROVABLY-REDUNDANT NULL CHECK BEFORE `operator delete`, AND NO SOURCE SHAPE
  RECOVERS ONE THE ORIGINAL KEPT (v488).** `AppWindow::ReadOwnFileVersion` (0x4062e0) emits
  `test ebp,ebp; je` immediately before its `operator delete` even though the enclosing
  `if (pInfo != NULL)` makes it provably non-null. Every spelling tried gets it folded: an
  explicit `if (pInfo != NULL) ::operator delete(pInfo);`, a plain `delete pInfo;` inside the
  guard, the same hoisted OUT to a sibling statement (folds AND loses 4 more bytes), and
  `new BYTE[n]` / `delete [] pInfo` (identical — the array operators ICF-fold onto the scalar
  ones at 0x465ce0/0x465cd0, so the array spelling is byte-invisible here). Two instructions;
  park it rather than re-grinding.
- **⭐ THE DECLARATION ORDER OF TWO NULL-INITIALISED LOCALS DECIDES WHETHER VC5 REUSES A ZERO
  REGISTER (same function).** With `LPVOID lpValue = NULL;` declared before `BYTE *pInfo = NULL;`,
  the original's `mov [esp+0x1c],ebp` (reusing the already-zeroed pointer register) is reproduced
  and the function is 416 B — the exact original length. Swapping the two declarations costs 4
  bytes and makes it `mov [esp+0x1c],0`. Cheap to probe both ways; always probe both ways.
- **⚠ THE `lea` SIB BASE/INDEX ORDER IS NOT DRIVEN BY SOURCE OPERAND ORDER — REFUTED TWICE IN ONE
  SESSION (v488).** `lea edx,[ecx+edx*1-0xa]` vs `lea edx,[edx+ecx-0xa]` is a pure allocator
  coin-flip (the v329 class). On `AppWindow::LoadWindowAndBalancing` (0x406480, insns 130/130,
  byte_diff=2) neither parenthesising the extent as `left + (width - 10)` nor writing the sum
  reversed as `width - 10 + left` moved a byte; on `WorldBoardMaybe`'s 0x457ce0 the same swap
  turned out to be a cascade of the zero-register rotation upstream of it, not its cause. When a
  residual is only `lea` operand order at an identical instruction count, autopsy and move on.
- **⭐ #50 A DEAD PARAMETER CAN ONLY BE TYPED FROM ITS CALL SITES (v490).** `CarNetObj::
  HitTestTileMaybe` (0x40e250) has a 4th parameter its body never reads; its `ret 0x10` proves the
  parameter EXISTS but says nothing about its type, and the callee's codegen is identical for
  every type that occupies a 4-byte stack slot. So the callee cannot pin it and no amount of
  grinding the callee will. The CALLER pins it: `PeerTrainSlotQueueMaybe::DrawVisibleCarsInTileMaybe`
  (0x44e2e0) passes the owning train's `bUnk0x2c` as a bare `mov dl, byte ptr [eax+0x2c]; push edx`
  — upper bytes left as garbage, which is exactly what a BYTE argument looks like. Declared
  `short` it was a `movzx dx, byte ptr [...]` at each of the two sites, and those were the only
  two instruction mismatches left in the caller (78267 → 58257). Generalises: when a residual is a
  `movzx`/`movsx` on an argument, suspect the DECLARED parameter type before suspecting the
  allocator — and for a dead parameter, the call sites are the only evidence that exists.
- **⭐ #51 RE-READ THROUGH THE POINTER, DON'T CACHE IN A LOOP-LOCAL — NOW TWICE IN ONE TU (v490).**
  A loop that walks `T **pp` and touches `(*pp)->field` at several points must be written with the
  dereference REPEATED at every use; hoisting it into a `T *p = *pp;` local is a real byte
  difference, not a style choice. `PeerTrainSlotQueueMaybe::DrawVisibleCarsInTileMaybe` (0x44e2e0)
  went 130107 → 78267 and insns 96 → 88 purely from un-caching it, reproducing exactly the lever
  `SelectCarAtPositionMaybe` (0x44e830) in the same TU had already documented (where caching cost
  7 bytes and DIFF(114)). VC5 re-loads the pointer at each use and the source has to say so. Try
  this FIRST on any pointer-walk loop whose candidate is SHORTER than the original.
- **⚠ #52 A BYTE-INVISIBLE WRONG STRING LITERAL IS A REAL, RECURRING DEFECT CLASS — AND THE LINT
  THAT COVERS IT WAS ONE-DIRECTIONAL FOR NINETY SESSIONS (v490).** String CONTENT is never
  compared by anything here, only the masked relocation to it, so a function carrying a wrong
  literal byte-matches EXACTLY. `tools/lint_strings.py` has existed since v396 but only tried
  substituting `'_'` with another character — which structurally could not see v489's
  `"WINDOW ATTRIBUTES"` for `"WINDOW_ATTRIBUTES"` (the reverse substitution) nor v490's
  `"%sCURSORS\\%s"` for `"%s\\CURSORS\\%s"` (a DROPPED character). Both were found by hand. It now
  searches every EDIT-DISTANCE-1 neighbour — substitution, insertion, deletion, any position — and
  strips trailing `//` comments, which were 16 of its 17 prior false positives. Repo is audited
  clean: 952 literals, 952 present in the image verbatim. **Re-run it after any session that adds
  string literals; it is ~0.3 s.** General lesson: when a lint finds nothing for a long time, check
  what shape of bug it is capable of finding before concluding the bug class is absent.
- **⭐ A DELETED TU-LOCAL VIEW STRUCT IS A DIAL LEVER, AND IT CAN PAY 951 BYTES (v490).** The
  declaration-count dials this file documents (`src/AppWindow.h` etc.) are usually discussed as
  header knobs, and v488 concluded `WorldBoardMaybe`'s 0x457ce0 was SPENT because no
  `src/AppWindow.h` count buys it back. That was true and the conclusion was still wrong: v490
  recovered the full 951 B without touching that header, by deleting a one-method TU-LOCAL VIEW
  STRUCT from `src/WorldBoardMaybe.cpp` itself. File-local declarations count toward the same
  parity as header ones. Corollary worth the most: **fixing a `lint_alias.py` finding is not just
  hygiene — retiring the alias retires a declaration, so it can pay in bytes.** Here the two
  arrived together, because a view struct is the main producer of both.
- **⭐⭐ #53 — WRITE AN ARRAY WALK WITH A SUBSCRIPT INDUCTION VARIABLE, NOT A HAND-ROLLED POINTER
  WALK. Two EXACT matches in one session (v491), both otherwise stuck on pure register noise.**
  The decompiler renders every strength-reduced loop as an explicit `T **pp` walk, and
  transcribing it that way is the natural move — but under `/O2` the two forms are NOT equivalent
  in what the register allocator does with them, even though the emitted addressing mode is
  identical. The subscript form lets cl do its own strength reduction and it lands on the
  ORIGINAL's choices; a source-level pointer walk pre-empts that and cl then makes different ones.
  Two distinct symptoms, both cured by the same rewrite:
  - `PeerTrainNodePartial::PeerTrainNode_ReleaseCarSlot` (0x44c310): the compaction loop
    `ppSlot[-1] = *ppSlot; *ppSlot = 0;` emitted the two stores in the OPPOSITE order to the
    original, at an identical instruction count (DIFF(7), insns 37/37 — that was the whole
    residual), and an explicit `T *pNext = *ppSlot;` temp did not move it. Rewritten as
    `for (i = nIndex; i < 3; i++) { carSlots[i] = carSlots[i+1]; carSlots[i+1] = 0; }` it is EXACT.
  - `PeerTrainSlotQueueMaybe::DetachFromBoardMaybe` (0x44e630): two consecutive walks over the
    same array, written as `CarNetObj **ppCar = ...` each time, made cl CSE the two
    `lea edi,[esi+0x10]` into ONE shared base register held across both loops, and swap the
    counter/pointer register roles in each (DIFF(159), insns 163/156). Subscripted, cl emits the
    per-loop `lea` and the original's roles: EXACT.
  This does NOT conflict with #51 (re-read through the pointer, never cache `(*pp)->` in a
  loop-local) — #51 is about how many times you DEREFERENCE, #53 about how you ADVANCE. The
  combined idiom is a subscripted `for` whose body repeats `array[i]->field` at each use.
  **v501 sweep corollary: run #53 against PARKED "intrinsic" residuals too, not just active
  ones.** The v501 sweep of every `pp++`-walk in a non-EXACT function cracked THREE long-parked
  EFFECTIVEs whose notes blamed unsteerable classes: 0x44e830 (the repo's closest non-match,
  DIFF(1) — the subscript form flips even the instruction selector's SIB base/index roles),
  0x43e370 (v240-parked "symmetric-register-swap family (Yoda #29/#30), not source-steerable" —
  subscripting resolved ALL FOUR listed residuals at once), and 0x44d740 (v329-parked
  "LEA-scheduling swap" — subscripting deletes the pointer's `LEA` outright, so the scheduling
  question disappears with it; mechanism: no element pointer, nothing to schedule). But it is
  NOT universal: same sweep measured #53-NEUTRAL (byte-identical compile) at 0x44e3f0, 0x440a80,
  0x427580, 0x429850, 0x433be0 and REFUTED (worse) at 0x44e200, 0x44dc10, and 0x44cb10 (rotates
  the whole prologue) — probe per function, keep the winner, record the matrix in the note.
- **⚠ A HEADER'S DECLARATION-COUNT DIAL IS NOT A PARITY AND NOT A PERIOD — MEASURE THE WHOLE
  CURVE BEFORE PRICING ANYTHING AGAINST IT (v491).** v490 established that file-local declarations
  count toward the same dial as header ones; v491 measured the dial's actual SHAPE for
  `src/TilePlacedObj.h` and it is neither of the simple models anyone had assumed. Adding N dummy
  member declarations on top of that session's four real ones, everything else fixed:
  N=0 both dependent TUs EXACT, N=1..4 `src/WorldBoardMaybe.cpp`'s 0x457ce0 DIFF, N=5..7 both
  EXACT again, N=8..9 DIFF. An earlier sweep the same session, taken before `src/NameAnchorMaybe.h`
  gained one declaration, put the boundaries in DIFFERENT places — so the dial is a property of the
  whole compilation, not of one header's count. Practical consequences: (1) the safe window can be
  under one declaration wide, so a single honest addition can cost a match; (2) padding a header
  with dummy declarations to reach a good N is never the answer — find a REAL declaration to add,
  or demote one you do not have a caller for back to a comment (v491 kept 0x457ce0's 951 B by
  demoting `TrackConnectorTileObj::PumpClaimWaitQueueMaybe`, which nothing in `src/` calls, from a
  declaration to a documented note); (3) the dial is invisible to a per-file `tools/cc.sh` on the
  TU you are editing — only a full `tools/progress.py` per-file diff shows it.
- **⭐⭐ #54 — WHEN A PROVABLY-DEAD COMPARISON SURVIVES INTO THE ORIGINAL'S BYTES, THE SOURCE
  CANNOT HAVE BEEN THE OBVIOUS SPELLING OF IT (v492).** 0x44e3f0's per-car guard is
  `dwModeAMaybe == 2 && dwModeAMaybe == 3 && dwModeBMaybe == 2` — an engine bug
  (`docs/engine-bugs.md`), never true, and the original nonetheless emits all three compares plus
  the never-taken branch. cl 5.0 under `/O2` FOLDS that to false and DELETES the whole block
  whenever both reads of the field are written as the SAME source expression: measured for all
  four same-expression spellings — a plain `&&` chain, three nested `if`s, a cached `int` local,
  and all-`(*pp)->` — every one produces byte-identical output with the block gone (an identical
  324077 score in each case). Only TWO DIFFERENT source expressions naming the same field survive
  the fold, so the transcription deliberately spells one read `(*ppOther)->` and the other
  `pOther->`. ⭐ The general rule: cl's dead-branch fold runs on the SOURCE-LEVEL expression, not
  on the CSE'd value — it will not fold two syntactically distinct reads even when it happily
  CSEs them, and it folds a cached local exactly as readily as a repeated field read. So a
  contradiction that is still in the binary is positive evidence about the shape of the original
  source, not merely about its logic. The residual cost of the mixed spelling is ONE instruction
  (the original CSEs its two reads into a single load; the mixed pair compiles to two memory
  compares) — cheap against the ~15 instructions of block the fold would otherwise remove.
- **⭐ #55 — A SWITCH WHOSE CASE LABELS ARE OFFSET FROM 0 LEAVES A REDUNDANT `add` IN FRONT OF THE
  DISPATCH, AND THAT `add` IS A READABLE FINGERPRINT OF THE REAL CASE VALUES (v492).** 0x44dc10's
  random-consist loop dispatches on `rand() % 3` into three arms that pass the categories 2, 3 and
  4 to `AllocCarSlot`. Transcribed the obvious way — `switch (rand() % 3)` with cases 0/1/2 — cl
  emits `sub edx,ebx` (subtract the zero it already has in a register) ahead of the `je / dec; je
  / dec; jne` sub-chain. The original instead has `add edx,2` immediately followed by
  `sub edx,2` — a pair that cancels and looks like a compiler artifact, but is not: it is the
  source's own `+ 2` followed by the dense-switch base normalization for a case set starting at 2.
  Writing it as `switch (rand() % 3 + 2)` with cases **2/3/4** reproduces both instructions, and
  the resulting source also reads better — the switch value IS the category each arm passes on.
  ⭐ Generalises: on a `sub`-chain switch, the constant in cl's normalizing `sub` is the LOWEST
  CASE LABEL. If the original's normalization subtracts a non-zero constant, the real case labels
  start there; if a stray `add` of the same constant sits in front of it, that `add` is the
  source's own arithmetic on the switch expression, not scheduling noise.
- **⚠⚠ #56 — A DIAL MEASUREMENT IS ONLY MEANINGFUL AGAINST A BASELINE WHERE THE CANARY IS HEALTHY
  (v492).** When several header edits are in flight and a known canary function breaks, the
  natural move is to measure each candidate and keep the ones that "show no change". That
  reasoning is invalid once the canary is already broken: a second, independent cause reads as
  innocent, because "still DIFF" is indistinguishable from "no effect". It happened for real —
  `src/WorldBoardMaybe.cpp`'s 0x457ce0 lost its 951 B, the obvious suspect (a struct moved into a
  shared header, made first, 8 declarations, reaching that TU transitively) was measured and
  convicted, and it was INNOCENT: the move is completely byte-neutral. Two other changes each
  cost the full 951 B on their own, and one of them had been explicitly cleared by a measurement
  taken while the canary was already down.
  ⭐ Rules: (1) after ANY loss, re-measure from a tree where the canary is healthy — revert to
  HEAD, confirm the canary, then re-apply candidates ONE at a time; (2) bisect by reverting each
  candidate individually rather than by adding them cumulatively; (3) "no change" on a broken
  canary is not weak evidence of innocence, it is NO evidence — do not record it as a measurement.
  Same family as the "never `git stash` around a baseline measurement" rule: the measurement you
  took against the wrong baseline is the one that misleads you.
- **⭐ #57 — A CONSTRUCTOR DECLARATION DOES NOT COUNT TOWARD A HEADER'S DECLARATION-COUNT DIAL
  (v492).** Measuring the dial's curve for `src/PeerTrainSlotQueueMaybe.h` against 0x457ce0, three
  ordinary member declarations move it three rungs, but a CONSTRUCTOR plus two ordinary methods
  lands on the TWO-rung position, not three. So when hunting declaration currency to reach a
  favourable rung, only ordinary member functions are legal tender — a class's own ctor is free to
  declare but buys nothing. (Destructors untested here: declaring one on a class modelled with a
  plain `void *pVtbl` field would force it onto a real vtable and change the layout, which is a
  much larger change than a dial move.) Corollary for pricing: count the ORDINARY-METHOD
  declarations when reading a recorded curve, not the raw line count.
- **⭐⭐ #58 — A TERNARY MUST SPAN THE WHOLE EXPRESSION, NOT JUST ITS TWO CONSTANTS, OR cl GOES
  BRANCHLESS (v493).** `nX + (b ? 0x83 : 0x38)` does NOT compile to a branch under cl 5.0: the two
  constants differ by a constant, so it materializes the difference from the borrow flag —
  `neg dl; sbb edx,edx; and edx,0x4b; add edx,0x38`. Seeing that four-instruction
  `neg`/`sbb`/`and imm`/`add imm` clump in your own output where the original has a plain
  `test; je; add r,imm; jmp; add r,imm` is the signature, and the fix is to widen the ternary so
  its arms are non-constant: `b ? nX + 0x83 : nX + 0x38`. ⚠ An `if/else` STATEMENT over a named
  `int` is NOT equivalent either — it branches, but the named variable gets its own register
  (`lea eax,[edi+0x83]`) where the original ADDS INTO the source operand's register
  (`add edi,0x83`). cl only clobbers like that for a temporary consumed immediately by the
  argument push, which a named local is not. Three shapes, three different codegens; the pushed
  temporary is the one to write. (Same family as 0x42d440's `add ecx,ebp` vs `lea edx,[ebp+ecx]`
  for `rect.left + span` — the original treats a reloaded field as a dead temp, a named local
  never is.)
- **⭐⭐ #59 — cl 5.0 FOLDS ANY TWO SOURCE EXPRESSIONS NAMING ONE COMPILE-TIME-KNOWN STACK ADDRESS,
  WHICH BOUNDS WHAT #54 CAN EXPLAIN (v493).** #54 established that a provably-dead comparison
  surviving into the original's bytes means the source was not the obvious spelling of it, and
  that on a FIELD two different spellings of the same read (`(*pp)->x` vs `p->x`) survive where
  one spelling folds. That does NOT generalise to a STACK ADDRESS. 0x45a500 has a live
  `cmp word ptr [esi+0x398],6` guarding two pushes of the same slot in different registers
  (`lea eax,[esp+0x18]` / `lea ecx,[esp+0x18]`) ahead of a shared `push <bounds>; call`, and every
  same-slot spelling folds it away: a ternary `&rc : (RECT *)&rc.left`, the same choice made by
  an if/else over a `RECT *`, and a plain `&rc : &rc` all give byte-identical output with the
  branch gone. So when a surviving branch straddles two *addresses* rather than two field reads,
  do not hunt for a cleverer spelling of the same object — the residual is telling you a SECOND
  REAL OBJECT exists that merely shares the slot, and the honest move is to park it until that
  object is identified.
  ⭐ Corollary measured the same session: cl 5.0 does NOT cross-jump. Two inline expansions of the
  same helper in an if/else DUPLICATE (+105 bytes), they do not merge. So whenever an original
  shows one shared tail under a branch, the branch is inside an ARGUMENT — argument pushes are
  right-to-left, so a branch landing ahead of the last push belongs to the LAST parameter — never
  a merged pair of calls.
- **⭐ #60 — THE `.rdata` VTABLE IS ALSO THE CHEAPEST WAY TO FIND WHAT AN UNCLAIMED FUNCTION IS
  (v493).** CLAUDE.md already requires dumping a class's vtable before inventing a name for a
  declared-only virtual. The same dump run FORWARD is a discovery tool: three of this session's
  targets (0x45a500, 0x45a740, 0x42d440) had zero or one code xref and read as anonymous gap
  functions, and one vtable dump each identified them as slot 3 / slot 4 / slot 3 overrides of
  methods the base class already declares — which supplies the signature, the return type and the
  name for free. A function with NO code xrefs at all is not obscure, it is virtual; find its
  slot before reading its body. ⚠ And when the slot's contract is already declared on the base,
  prefer the family's own name for it over a fresh descriptive one — the descriptive reading
  belongs in the comment, so the class does not end up with two vocabularies for one slot.
- **⭐⭐ #61 — A WRONG VARARG ARGUMENT ORDER IS INVISIBLE TO asmscore's INSTRUCTION DUMP; ONLY
  THE RAW BYTE COMPARE SEES IT (v494).** The first ParseRandomizedRecordMaybe compile scored
  `insns 109/109, align=0, byte_diff=4` with an EMPTY --dump: every opcode, register and branch
  matched, and the four differing bytes were the displacement fields of four `lea [esi+0xXX]`
  address temps in the sscanf push sequence — the hr/min conversion slots were passed in field
  order (&m_4, &m_8) where the original passes (&m_8, &m_4). Same types, same count, same
  instructions; wrong arguments. So: a clean instruction dump with a nonzero byte_diff means
  suspect swapped same-typed arguments (or slot-order swaps in the frame), and the raw
  byte-diff recipe is the one that answers it (COFF section-header parse + reloc masking, 20
  lines). ⚠ And for sscanf specifically: the argument order comes from the PUSH sequence read
  back-to-front (last push = first vararg), never from assuming the record's field order —
  the fix here also SETTLED TimeOfDayMaybe's semantics (m_8 = hour, m_4 = minute).
- **⭐⭐ #62 — cl 5.0 CONSTANT-FOLDS A RETURN ACCUMULATOR ONLY WHERE ITS VALUE IS PROVABLE AT
  THE RETURN EDGE; A SHARED `return acc;` AT A JOIN KEEPS THE VARIABLE (v494).**
  ProcessTimedEventsMaybe's accumulator is read at five exits, four of them duplicated
  epilogues that all emit `mov al,bl` — including the loop-exhausted edge where bl is provably
  0. Every early-`return acc` / `return 0` / `return 1` spelling inside the loop let full
  constant propagation through, producing `xor al,al` / `mov al,1` and collapsing the
  epilogue count. The shape that reproduces the original: fire code INSIDE the do-while, every
  fire arm setting the accumulator and `break`ing, ONE `return acc;` after the loop that all
  five exits join (cl duplicates it per exit edge, but the join itself defeats the proof, so
  the variable survives). If your `return acc;` compiles to `xor al,al` where the original
  has `mov al,bl`, the accumulator's reaching definitions in your structure are provable —
  move the return to a join, don't fight the spelling.
- **⭐⭐ #63 — cl 5.0'S GUARD FOLDING IS SYNTACTIC: A DEAD COMPARISON SURVIVES UNLESS IT NAMES
  THE GUARD'S OWN CONSTANT; COMPUTED-EXPRESSION GUARDS NEVER FOLD (v494).** Bounds what #54
  can explain the same way #59 bounds it for stack addresses. `if (x >= 1) { if (x == 0) … }`
  FOLDS (the `> 0`/`== 0` pair shares the constant 0 — same-block dominance), but the
  semantically identical `if (x < 1) {…} else if (x == 0) {…}` does NOT (the else-edge carries
  `>= 1`; the constants 1 and 0 don't match syntactically). And guards written on a COMPUTED
  expression — `1 - x != 0`, `x + 1 != 0` — never fold at all (no arithmetic tracking), which
  is exactly how the engine's swapped-constant dead cases (docs/engine-bugs.md: period == 0/2
  vs == 1/-1) survive into shipped bytes. When a residual is a surviving dead test, do not
  "clean up" the source into the foldable form — write the guard the way the bytes prove it was
  written: the inverted comparison constant, or the computed expression (`nJitter + 1 != 0`,
  `1 - nPeriod != 0`), whichever matches the emitted `test`/`sub` shape.
- **⭐ #64 — TWO ASSIGNMENT PLACEMENT IDIOMS THAT ARE LOAD-BEARING IN CALL-HEAVY CODE (v494).**
  (a) An ASSIGNMENT EMBEDDED AS A CALL ARGUMENT — `sprintf(buf, fmt, this->count =
  this->count + 1)` — is what produces the original's "evaluate the first argument's address
  BEFORE the increment, push the value, and schedule the store after the pushes" shape; the
  two-statement version stores before the pushes and leaves the value in a different register
  (0x41f8e0 went from DIFF(62) to MATCH on this alone). (b) A NAMED BYTE LOCAL THAT IS
  RETURNED AFTER BEING TESTED — `char bOk = f(); if (bOk == 1) x = bOk; return bOk;` — is
  what produces "fall through to the epilogue with al still holding the call result" and the
  `mov [x], al` store (the test makes al provably the stored constant); spelling it
  `if (f() == 1) { x = 1; return 1; }` gets the store right via comparison propagation but
  splits the epilogues wrong. (b) also keeps the local in al — no stack slot — where a
  longer-lived byte local would get one and shift the whole frame.
- **⭐ #65 — REGISTER PRESSURE IS SOURCE-STEERABLE IN BOTH DIRECTIONS: ADD A NAMED LOCAL TO
  SPEND A REGISTER, SPLIT A HOISTED LOAD PER-ARM TO WASTE ONE (v494).** (a) 0x41f970's tm
  pointer belonged on the stack (the original reloads it per iteration from [esp+0x14]); the
  candidate kept it in ebp because nothing else needed the register. Naming the remap-target
  kind id as a local used across two calls forced a callee-saved register for it, spilled the
  tm pointer to its slot, and took the function from DIFF(276) to MATCH — the inverse of the
  v255 pin lever (that one ADDS a value early to protect a register; this one ADDS a value to
  evict one). (b) 0x420000's spawn x-offset is one invariant field load the candidate hoisted
  and shared across three branches; the original loads it per arm. Writing the x/y assignments
  per-arm (each branch computing from the field) defeated the hoist and took the function
  62726 -> 13796. Both are the same rule: cl's allocator serves source-visible value traffic;
  if a value lives where the original doesn't have it, change how many times the source NAMES
  it, and where.
- **⭐ #66 — A do-while WITH A HEAD break-CHECK GETS ROTATED INTO A while WITH THE CHECK
  DUPLICATED AT THE BOTTOM; THE for-LOOP FORM KEEPS ONE COPY (v494).** Both 0x41fd00's
  widget-list walk and 0x420000's main loop have the shape "guard; body; inc/cmp at the
  bottom" with the screen-state break-check at the loop head. Written as `do { if (!gate)
  break; … } while (i < n)`, cl 5.0 rotates them and emits the gate check TWICE (head +
  bottom, +2 insns and a reshuffled back-edge). Written as `for (i = 0; i < n; i++) { if
  (!gate) break; … }` — with the counter declared at function top so it earns its register —
  the single-check layout the original has falls out. Same loop, same semantics, two layouts;
  check the loop BOTTOM of the original before choosing the spelling.
- **⭐ #67 — VC5 /O2 INLINES AN IN-CLASS DTOR INTO THE COMPILER-GENERATED $E STATIC-DTOR
  THUNKS (v495).** When a TU defines a file-scope object whose class dtor is declared-only
  (out-of-line), the object's atexit ($E) thunk tail-calls the standalone `??1`; with the
  dtor defined IN-CLASS the same thunk gets the whole body folded in and the standalone
  `??1` COMDAT vanishes from every TU (an out-of-line `inline` definition is WORSE: fully
  inlined locally, no COMDAT emitted at all, and marker pairing cascades positionally). So
  the pair "??_G inlines the base-dtor body AND a standalone ??1 exists" cannot be
  reproduced from one header — the original must have compiled the global-owning TU against
  a declaration-only view of the class. Measured on `ThreadWrapper`/`g_worldLoadThread`
  (0x461640/0x461690/0x45c790): taking `??_G` EXACT (+65 B) cost the 0x461690 marker
  (-45 B) — net +20 B exact; trade note in src/ThreadWrapper.cpp.
- **⭐ #68 — COPY A CONDITIONALLY-ADJUSTED PARAMETER INTO A NAMED LOCAL *BEFORE* THE
  CONDITION; THE COPY IS THE SCHEDULING ANCHOR (v502).** 0x44ed80
  (`WidgetTagObj0x478378::HitTestNodeSecondary`) adjusts its hit-x by the carousel scroll
  only for bit-2 nodes and then passes it on. The original emits `mov dl,[edi+0x2c];
  mov eax,ecx; test dl,2; je …` — the flag byte in a REGISTER and the unadjusted x copied
  out BEFORE the test, so both paths inherit one value in one register. Written as
  `if (flags & 2) x += scroll * 0x39;` cl instead emits `test byte ptr [edi+0x2c],2` and
  schedules the whole lea-chain differently (DIFF(244), byte_diff 59 at insns 121/122 —
  one instruction short, exactly the missing `mov eax,ecx`). Written as
  `int nLocalX = x; if (flags & 2) nLocalX = x + scroll * 0x39;` the copy, the register
  load and the chain all land on the original's choices: EXACT. This is the mirror image of
  #51 (un-cache a loop local) and the #65 mechanism seen from the other side — whether a
  value should be cached or re-read is not a style question; it is whatever the original's
  register traffic says, and a source-level name is how you say it.
- **⭐ #69 — AN EQUALITY ARM THAT SITS LAST IN .text UNDER A `je` FORWARD IS AN `if (x != K)`
  NEST, NOT AN if-CHAIN (v502).** Same function: three resourceId arms, original test order
  `cmp 0x2801; je A_end` / `jle B_mid` / `cmp 0x2803; jg B_mid` / C fallthrough — bodies in
  the order C, B, A. An if-else chain `if (==K) A else if (…) B else C` lays bodies
  textually (A first — REFUTED, the equality arm got inlined at its test), and rewriting the
  outer guards as early-outs (`if (x < n) return 0; …`) duplicated the return-0 epilogue per
  guard (insns 132/122 — REFUTED). The shape that reproduces both the test order and the
  body order is nesting the equality on the negative side: `if (id != K) { if (id <= K ||
  id > K+2) { B; return 1; } C; return 0; } A; return 1;` — A falls out textually last, the
  `||` mid-test jumps forward over C into B, and the single shared epilogue survives. When
  an arm's body sits AFTER the code its `je` skips over, read it as the negative-nest, not
  as the chain.
- **⭐ #70 — WHEN THE CONTINUATION IS ALSO THE TARGET OF AN EARLIER BRANCH, GUARD POLARITY
  CONTROLS BLOCK PLACEMENT: write the big continuation as a POSITIVE `if (v)` body and let
  the small path fall out the bottom (v508, `WorldActionCursor::OnKeyDownMaybe`/0x45b3a0,
  DIFF(144) -> DIFF(6) in one compile).** This is the exception to the v430 "control-flow
  spelling is NOT a lever for cold-path sinking" bullet: that one's byte-identical result
  holds for a flat body, but here the continuation (a mode switch) is jumped to from an
  EARLIER guard as well as reached by fall-through, and the two spellings diverge hard.
  `if (!v) { smallPath(); return; } <continuation>` INLINES the small path between the two
  jumps to the continuation (`jne L; small; L: ...`); `if (v) { <continuation>; return; }
  <small path trailing>` outlines the small path at the very END of the function, which is
  the layout the original shows (`je` forward over the whole continuation to the trailing
  block). Duplicating the continuation per exit to invite cross-jumping does NOT fire
  (both copies emitted, +135 B — refuted).
- **#71 — A plain byte move of a CALL RESULT into a byte local (`mov bl,al`, no
  test/setne normalization anywhere) pins the callee's return type as `bool`, not
  `unsigned char` (v508, `UiIconListItem::HandleTextEditKey` retype).** VC5 normalizes a
  non-bool byte into a bool destination (and a byte local out through a bool return) with
  `test al,al; setne`; an assignment that shows NEITHER at the store NOR at any return can
  only be bool->bool. Audit the other callers before flipping: a caller with an explicit
  `? true : false` ternary or one that discards the result is insensitive either way, so a
  long-standing uchar mis-decl can hide until the first caller that assigns it bare. The
  flip is codegen-visible ONLY at such callers (the callee's own `return 0/1` bodies are
  identical under both types), which makes it cheap to verify: flip, recompile every
  caller, the newly-exposed sites move and nothing else does.
- **#72 — To force SHORT locals through MEMORY (word stores + dword re-reads at the push
  sites), make them a short ARRAY, not two short variables (v509, 0x455960).** Two plain
  `short` locals stay in registers end-to-end under /O2; `short aXY[2]; aXY[0] = ...;
  aXY[1] = ...; f(aXY[0], aXY[1], ...)` compiles to the original's
  `mov word [esp+N], r` clamps followed by `mov r, dword [esp+N]` / `push` re-reads
  (overlapping dword reads of the adjacent pair, garbage upper halves included — the normal
  short-arg passing shape). The array also reuses a dead int's stack slot, so it costs no
  frame growth when one is available.
- **#73 — VC5 assigns a double's stack slot at its FIRST STORE, not at its declaration, so
  model FP locals in the original's spill order: declare the reduction variable
  uninitialized-first, the raw `(double)` conversions next (squared THROUGH those slots and
  divided IN PLACE afterwards), the parked endpoint conversions last (v509, 0x455960).** The
  fused `double fdx = (double)dx / dist;` form lets the scheduler spill the endpoint
  conversions into what should be fdx/fdy's slots and mis-assigns every slot below the
  reduction variable. ⚠ Once the slot map is right, the residual fxch-index/spill-order
  dance around the squares-and-sqrt block is NOT source-steerable: moving a parked double's
  declaration after the reduction, or after both in-place divisions, compiles byte-identical
  (FP-stack scheduling coin-flip class; only asmscore align bytes move).
- **#74 — A `rand() % d + 1` fused into a sum with a global folds to `lea r,[rem+tick+1]`;
  split the modulo into a temp statement to get the original's `inc` (v509, 0x405ab0).**
  `tick + rand() % d + 1` as one expression gives cl a 3-operand lea;
  `t = rand() % d + 1; dst = tick + t;` reproduces `call rand; cdq; idiv; inc edx; mov
  eax,[tick]; add`. The remaining add-accumulator direction (`add eax,edx` vs `add edx,eax`)
  is a pure coin-flip: operand-order flips in either arm compile to the same choice.
- **#75 — To make a by-value-struct local (e.g. a RECT built field-by-field) stay
  MEMORY-RESIDENT (per-field stores + a local→outgoing copy at the call) instead of being
  register-forwarded straight into the outgoing area, give the SOURCE struct's fields a second,
  longer-lived consumer: copy them into a named sibling local first (v510, 0x417040).**
  With `dstRect.left = pRes->rect.left; ...` (x4, register-promoted) ahead of
  `srcRect.left = dstRect.left + off; ...` (x4), cl keeps the four raw fields live in
  registers across both by-value copies and spills the computed fields to the local —
  reproducing the original's `sub esp,0x10`-before-compute + per-field-store shape. Without
  the sibling local the same statements compile ~20 B shorter with the local eliminated
  (insns 97/101) — shorter code, worse structure, and the diff metric rewards the longer
  form. Whether the outgoing area is reserved before or after the field computation, and the
  field-load rotation that serves it, remain /Og scheduling (not source-steerable: statement
  order swaps, `&local` pointer spellings and operand-order flips were all byte-identical or
  worse).
- **#76 — When the original's failing branch is the block at the very END of the function
  (a forward `jne` over the success tail into a shared fail+epilogue), write the SUCCESS path
  as `== 1` with an early `return` and leave the failure as a bare trailing statement — NOT
  `!= 1` with the success tail falling through (v511, 0x429dd0).** The `!= 1` spelling makes
  cl emit the fail block INLINE right after the test (`je` over it) and then clone the
  epilogue for the success tail — one extra epilogue, DIFF(26). Flipping the test polarity
  (`if (bDeleted == 1) { …; return; } GetLastError();`) reproduces the original's layout
  exactly. Same /Og block-placement family as the v326/v328 tie-breaks, but steerable: the
  source's statement order IS the lever here.
- **#77 — An inlined implicit copy constructor (e.g. a registry slot-9 `new T(src)` body) is
  a pad-MEMBER oracle: bytes the original copies are real members, bytes it skips are real
  alignment padding (v511, 0x424550).** An explicit `pad0xNN` member is copied like any other
  member (the DecorActor.h lesson), so if the original's copy walk SKIPS an alignment byte,
  the pad must NOT be a declared member — deleting `AnimEffectObj0x477a90`'s `pad0x89`/
  `pad0x95[3]` took the walk from DIFF(140) (compiled +40 B: pad bytes copied as
  byte/word+byte pairs) to EXACT, with `sizeof` and every offset unchanged (natural
  alignment supplies the same gaps). Polarity matters: where the original DOES copy the pad
  (DecorActorBase), the explicit member must stay.
- **#78 — Adding DEFAULT ARGUMENTS to a shared header's ctor declaration is a dial touch
  like any other header edit: `WidgetBase.h`'s `AnimDescRefObj0x477488(... = -1, = -1, = 0,
  = 0)` cost WorldBoardMaybe::FindNearestObjOfCategoryMaybe (0x457ce0) its 951 B EXACT
  (v511, measured, reverted).** Orthogonal to the declaration-COUNT dial on
  WorldActionCursor.h (whose one-free-slot budget is spent on the dtor; a 2nd declaration —
  the measured `WorldActionCursor();` ctor decl — costs TilePlacedObj's 0x4588b0 its 143 B
  EXACT, as v506/v508 predicted). The 0x4589b0 WorldActionCursor ctor (274 B, compiles
  EXACT) needs BOTH edits, i.e. 1094 B of standing EXACTs against it — do not land it piecemeal;
  bundle it with the slot-16 retype (which needs the same parity paid) and re-win the two
  victims in the same session. **v512 addendum: the bundle landed (ctor EXACT 274 B, slot-16
  retype done, the TU-local view retired) and the re-wins did NOT — both victims are dial
  coin-flips with no source lever (0x4588b0: EITHER header edit alone flips it, the 3-vs-4
  callee-saved-register budget flip; 0x457ce0: the default-args edit alone flips it, the
  v375 zero-register-residency class, ecx-vs-eax on the ring counter). Compensation probes
  now refuted on the 0x457ce0 axis: spare struct/free/forward-class declarations at TU top
  are all INERT against a SIGNATURE-shape dial touch — the count dial (#8) and the
  signature dial are different dials, and only same-shape edits move the latter.** Both
  victims parked in docs/PARKED.md v512 with their full probe lists; the toll is paid, so
  the honest declarations stay.
- **#79 — A SHARED base-class vtable-slot RETYPE is priced on the declaration dial like any
  other header edit, and the three cheap idiomatization wins that look like they come free with
  it actually do (v523, measured, reverted).** Retyping `WidgetBase.h`'s slot-19 placeholder
  `virtual void *_v19()` to its honest `virtual char TestAndToggleMenuNodeHoverMaybe(
  MenuNodeObj0x477568 *, int, int)` — the shape its two callers already prove — costs **−1607 B**:
  WorldBoardMaybe.cpp 0x457ce0 (951 B, the usual canary), RoadVehicleActor.cpp (504 B) and
  PlacedObjRegistryMaybe.cpp (152 B). v506 priced the same edit at 143 + 951 B against a
  DIFFERENT victim set (TilePlacedObj's 0x4588b0, which no longer moves); the victim list
  rotates, the verdict has not. Three sub-findings worth more than the price itself:
  - **The probe DELETIONS are free.** Retiring both TU-local `WidgetSlot19VtblProbe` copies AND
    moving the 0x44ef10 body off its `WidgetTagObj0x478378View0x44ef10` view onto the real class
    cost exactly 0 B once the retype was in place. All −1607 B is the shared header alone. So a
    "the honest model is too expensive" verdict should always be split: price the HEADER edit
    separately from the .cpp cleanups it unlocks, because only one of them is usually the bill.
  - **⚠ Retyping a base slot silently RE-SLOTS every derived placeholder that still spells it the
    old way.** `BuildToolButton.h` had a SECOND `virtual void *_v19()` (on `BuildToolButton`, the
    shared 0x44ef00 const-false stub, declared only to position slot 20). Once the base slot was
    renamed, that declaration stopped overriding and OPENED A NEW SLOT, pushing
    `HandleMenuCommandMaybe` off +0x50 and `ContainsHitAreaMaybe` off its real +0x54. It compiles
    clean and cost a further 387 B that read as ordinary dial noise until the vtable was re-dumped.
    Grep EVERY `_vNN` spelling of the slot across all derived headers before retyping a base slot.
  - **Ground-truth the slot on all derived classes first, not just the one you are editing.**
    The audit assumed `WidgetPickerObj0x477cc8` inherited slot 19; the `.rdata` dword at
    0x477cc8+0x4c is **0x4289a0**, which is already transcribed and EXACT as that class's
    `TestTabSwitchMenuCommandMaybe` — declared as an ORDINARY (non-virtual) member while the
    same TU dispatches that same slot VIRTUALLY through the probe cast. That is CLAUDE.md's
    "one function, two models" hazard in its exact documented form, and it means the honest fix
    is not a retype at all but a three-class naming decision (one neutral base name that both
    behavioral overrides adopt). Parked as a dedicated pass, not a cheap cleanup.
- **#80 — When a function's arms each `return 1`, the original's ONE shared `mov al,1` tail is a
  source shape, not a linker/ICF effect: write a single `||` chain into one accumulator.** v523,
  0x44bdb0: four separate `if (cond) return 1;` statements emitted four separate epilogues
  (151 B but DIFF(137), insns 61/49); folding them into
  `unsigned char bResult = 0; if (a || b || c || d) bResult = 1; return bResult;` produced the
  single funnel and paired every arm structurally (insns 50/49). Same family as #18m's
  entry-initialized accumulator, but the trigger here is the SHARED TAIL rather than EBX
  persistence — reach for it whenever the original's arms all `je` to one common `mov <reg>,1`.
  Corollary from the same function: the `||` fold can OVERSHOOT into a shorter body than the
  original (123 B), which usually means a value the original kept as a real local got CSE'd away
  — restoring the local (here `unsigned char nNorthEdgeRow = rows - ysteps;`, which the original
  computes and spills unconditionally at entry) puts the length back without undoing the funnel.
- **#81 — When two ADJACENT, INDEPENDENT, same-shape local initializers produce a residual of
  exactly "one operand load + one counter zero-init scheduled a slot late", SWAP THE TWO
  DECLARATIONS.** v524, `LocoBitmap::CopyOverlapRaw` (0x42c3d0) and its twin `CopyRectRawColorKey`
  (0x42c330): both are `copyH = (srcRect.bottom - srcRect.top) & 0xffff;` beside
  `copyW = (srcRect.right - srcRect.left) & 0xffff;`, and both sat at DIFF(18) with an IDENTICAL
  instruction count (54/54, 55/55) — `align` only, `reg_pen=0`, `byte_diff` confined to two rows.
  Declaring **`copyW` BEFORE `copyH`** took both to EXACT. One of them had been parked on that
  18-byte residual for many sessions.
  - ⚠ **The source order that reproduces the original is the REVERSE of the order the original's
    own instructions appear in.** The original computes `copyH` first (`sub ebp,esi` at 0x42c34c,
    `sub edx,edi` at 0x42c35d), so reading the disasm top-down and transcribing in that order is
    what produces the WRONG answer. VC5's scheduler reverses two independent same-shape
    initializers here. Do not try to derive the order by inspection — it is a one-line, two-way
    probe, so just compile both.
  - Cheap and high-yield: a whole family of sibling functions can share one declaration-order
    mistake, so when the swap cracks one, immediately try it on every sibling carrying the same
    residual signature (that is how the second of these two was recovered, for free).
  - Recognition signature, worth grepping asmscore output for: `exact=False` with
    `reg_pen=0 identity_miss=0`, instruction counts EQUAL, and a `--dump` showing only `-`/`+`
    pairs of the SAME instruction a few slots apart (a pure move, no missing or extra work).
- **#82 — Two functions with BYTE-IDENTICAL bodies are usually ONE source body inlined into the
  other, not two copies — and a devirtualizable virtual call is the tell that says which.** v524,
  `WidgetBaseObj0x4784c8::~WidgetBaseObj0x4784c8` (0x4545a0) vs `ClearOwned` (0x454630, vtable
  slot 15, sitting immediately after it). Both perform the same teardown; the dtor is really just
  `ClearOwned();`.
  - **The tell:** inside a ctor/dtor VC5 knows the dynamic type exactly and DEVIRTUALIZES a call
    on `this` to a direct `call`. Inside an ordinary member it does not. So if the original keeps
    `mov edx,[esi]; call [edx+0x18]` at a site that a dtor-local spelling would devirtualize, the
    body did not originate in the dtor — it arrived by inlining from a normal member. Writing the
    teardown out longhand in the dtor scored insns **38/39** (one SHORT — the missing instruction
    is the vptr load) and DIFF(7); routing it through `ClearOwned()` took it to EXACT.
    Conversely, a DIRECT call at such a site is evidence the body really is dtor-local — cf.
    `AnimDescRefObj0x477488`'s ctor, whose devirtualized `SetDescriptor` call is correct there.
  - **Both levers are required to reproduce it:** the shared body must be marked out-of-class
    **`inline`** AND must sit **before** its caller in the TU. Without `inline` VC5 emits a real
    `call` and the dtor compiles to 79 B / DIFF(29) instead of 144 B EXACT. Because the function is
    `virtual` it still gets its own out-of-line COMDAT for the vtable slot — so the inline copy and
    the standalone address BOTH byte-match from one definition. (Same out-of-class-`inline` shape as
    v518's ProbeComPort, but used to FORCE an inline rather than to suppress one.)
  - Generalizes to any "duplicated body" pair: model the SMALLER/named one as the real function and
    call it from the larger, rather than transcribing the body twice — two copies also violate the
    one-definition discipline and drift apart later.
- **#83 — `return <relational expr>;` and `if (cond) return 1; return 0;` are NOT the same
  codegen, and the tell is which register width the epilogue materializes.** v525, 0x412710
  (`TimeOfDay_IsTimeInWindowMaybe`, a `unsigned char`-returning predicate). The `if`-form lets VC5
  prove the value is a 1-bit constant per path and fold it to a 2-byte `mov al,1` / a single
  shared false tail: 114 B, insns 50/54 — FOUR instructions short of the original with no missing
  content anywhere. Spelling the same predicate as `return a <= b && b <= c;` makes VC5
  materialize the branch fold as a full-width `mov eax,1` (5 bytes) / `xor eax,eax` **per arm**,
  which is what the original has: 125 B EXACT.
  - Recognition signature: the residual is instruction-COUNT short (not a register rename), the
    missing instructions are all epilogue (`xor eax,eax; pop; pop; ret`), and the original mixes
    `mov eax,1` on the computed paths with a narrow `xor al,al` on an early-out guard. That mix is
    the giveaway — a plain `return 0;` guard stays narrow while the computed arms go wide.
  - Sibling-first rule: 0x412670 in the same family was already EXACT in the `return expr;` form.
    Check what a matched sibling does before inventing a spelling.
- **#84 — An accumulator + `break` and an early `return` differ by a whole stack slot, and BOTH
  spellings appear in the original.** v525, the twin pair 0x412b50
  (`TrackDepotTileObj::KeyedList_RemoveByKeyMaybe`) and 0x44f410
  (`TrackConnectorTileObj::RemoveClaimWaiterMaybe`) — same list walk, same key, same unlink.
  - `unsigned char bResult = 0; ... bResult = 1; break;` with ONE `return bResult;` after the loop
    forces the accumulator a HOME on the stack: the original reserves it with a bare `push ecx`,
    stores `xor al,al` into `[esp+0x13]` before the loop and RELOADS it on the exhausted-walk path,
    while /Og still constant-folds `mov al,1` into the found arms. Every epilogue then pays a
    matching `pop ecx`. That is 0x412b50, 125 B.
  - `return 1;` from inside the loop keeps everything in registers, needs no frame slot, and lets
    `this` stay in ecx throughout. That is 0x44f410, 123 B.
  - So when a residual is "the original spills a byte the compiler is proving constant", try the
    single-exit accumulator BEFORE concluding it is an intrinsic spill-decision park.
  - The same pair pins a second, independent lever: hoisting a repeated `pNode->pNext` into a temp
    is SOURCE-VISIBLE. 0x412b50 loads it once (temp), 0x44f410 loads it twice (spelled out at both
    unlink sites). Getting either backwards costs one instruction. **Two hand-written near-copies
    genuinely differ in small ways — do not assume a twin is a character-for-character duplicate.**
- **#85 — A two-level compressed switch table's GROUP-ID sequence reads out the source's case-group
  ORDER, and it is free ground truth.** v525, 0x44ab80
  (`BuildToolButton::ResetAndCloseToolMenuMaybe`). VC5 emits a byte index table (case value →
  group id) plus a dword target table. The ids are assigned in SOURCE case-group order, NOT in
  case-value order and NOT by target address — so a byte table reading `0 0 0 0 2 2 1 1` over
  values 0x2403..0x240a says: the group holding 0x2409/0x240a is written in the source BEFORE the
  group holding 0x2407/0x2408, even though its values are higher. Writing the two `SetNodeState`
  groups first and the do-nothing group last landed the function EXACT on the first compile,
  tables included.
  - Two distinct ids can share one target dword (here ids 0 and 1 both point at the same code):
    that means two source case-groups had IDENTICAL bodies and the compiler tail-merged the CODE
    without merging the TABLE. Read that as "there really are two separate case groups in the
    source", not as one group.
  - Read both tables with `objdump -s -j .text` over the range past the function's last `ret`;
    `app_funcs.txt` and Ghidra's `Body:` span both stop at the last INSTRUCTION and exclude them
    (the documented third `--len` trap — 0x4361e0 the same session reports 138 there against a
    true 156-byte COMDAT).
- **#86 — Declaration-count sensitivity is a PROPERTY OF A HEADER, not of headers in general —
  measure before assuming, in both directions.** v525 added an EIGHT-declaration `extern "C"`
  block of pixel-format globals to `src/Ddraw.h`, which six TUs include (CarNetObj, Ddraw,
  UIResources, WorldActionCursor, DDrawSurface, WidgetBase — two of them position-sensitive
  elsewhere), and a full `progress.py` showed **zero** movement in any of them. Contrast
  `src/LocoBitmap.h` (one member declaration costs −1531 B), `src/AppWindow.h` (one free-function
  declaration costs 951 B) and `src/TilePlacedObj.h` (a documented live dial). The dial is real but
  it is NOT universal, and treating every header as radioactive parks work that is actually free.
  - Corollary that paid the same session: MOVING a declaration between two classes in a
    dial-sensitive header is count-neutral and therefore safe — 0x44f410's declaration moved from
    the `TrackTileObj` base (where the field it walks is not even modeled) down to
    `TrackConnectorTileObj`, and `src/TilePlacedObj.h`'s own documented canary
    (`WorldBoardMaybe.cpp` 0x457ce0) did not move.
  - And: when a global has exactly ONE referencing function in the whole image, it needs no shared
    header at all — file scope in the defining TU is what the original almost certainly had. v525's
    0x45c970 sat parked for two sessions on "where do these six externs live"; the xref sweep
    answered it in one query. A plain file-scope `extern` in a `.cpp` is also not a lint_idiom
    class-I finding — that class is specifically `extern "C"`.

- **#87 — After `test r,r`, `jbe` vs `je` names the SOURCE's comparison form, and a one-byte
  residual of that shape is a free answer.** `test` always clears CF, so `jbe` (CF|ZF) and `je`
  (ZF) are semantically identical there — but VC5 only reaches for `jbe` when the source wrote an
  unsigned RELATIONAL compare, and emits `je` for an equality one. v526's 0x4257f0
  (`CursorDesc::ReleaseRef`) compiled at DIFF(1)/45 insns with the single disagreement
  `jbe`(original) vs `je`(mine); rewriting the refcount guard `if (n != 0)` → `if (n > 0)` on an
  `unsigned short` member made it EXACT on the next compile. Treat a lone `jbe`/`je` (or
  `jae`/`jne`) residual as a naming of the predicate, not as a scheduling tie-break — it is one of
  the very few residuals that tells you the source text outright. Same family as #83's
  return-expression-vs-if-form tell.

- **#88 — Adding a DEFINITION for an ALREADY-DECLARED member is a far cheaper act than adding a
  DECLARATION, and it is the cheapest way into a "dial-sensitive" TU.** v479 established that a new
  definition can rotate a TU's dial, and that rule stands — but v526 landed three bodies
  (0x4257f0 in `src/CursorDesc.cpp`, 0x456d10 in `src/WorldBoardMaybe.cpp`, 0x405870 in
  `src/WidgetBase.cpp`) into TUs all believed position-sensitive, each time into a declaration that
  ALREADY existed, and measured **zero collateral in all three** — including 0x405c40's 951-B-class
  407 B EXACT, which the v525 pickup had explicitly predicted would rotate. So before pricing a gap
  as expensive, check whether its declaration (and even its call site) is already present: the
  project has been accumulating declared-only members for many sessions, and each one is a body
  that can be landed without touching a header at all. Practical search: `grep` the class for the
  method name before assuming a header edit is needed.

- **#89 — A shared header's declaration-count dial can be UNSATISFIABLE across its includers —
  it is not always one parity bit they share.** #86 says measure the header; v526 adds that the
  measurement can come back "no count works". Adding `~RFIndex()` to `src/DSoundChannel.h` (20+
  includers) was measured from a clean baseline at two counts: **+1 → −1169 B** (LocoBitmap
  +124, Obj0x4779e0 −489, ThumbnailBmp −520, WidgetBase −407) and **+2 → −487 B** (those three
  recover, TilePlacedObj −203 appears). One TU IMPROVES at the count where three regress, and
  `WidgetBase`'s 0x405c40 is broken at BOTH — so the includers do not share a phase and no single
  shared-header count satisfies them all. When that happens, stop hunting for the magic count: the
  remaining routes are the v490 per-TU FILE-LOCAL lever, or splitting the type into a narrower
  header so the blast radius shrinks (the right fix for `RFIndex`, which is RF-archive code living
  in the DirectSound channel header for historical reasons only).

- **#90 — Ghidra's `s_` auto-label TRUNCATES at a length cap as well as mangling punctuation, so it
  can silently DROP TRAILING WORDS.** CLAUDE.md already warns that the label mangles non-alphanumerics
  (`s__curr_0047e2a0` for `"~curr"`); v526 hit the other half. `s_Invalid_Rect_found_in_world_draw_0047f124`
  is the label for **`"Invalid Rect found in world draw chain"`** — the label simply stops, and
  nothing about it looks truncated. A literal copied from it compiles, links, pools, and is wrong.
  Always dump the bytes at the string's VA (locate it in the section table and read the raw bytes
  up to the NUL) rather than reading the label; `tools/lint_strings.py` will confirm the literal
  actually appears in the image, which is the cheap post-hoc check.

- **#91 — "A definition is nearly free, a declaration is what costs" (#88) is a WEAK PRIOR, and
  v527 broke it in the SAME TU that established it.** Three bodies were added to
  `src/WidgetBase.cpp`, all EXACT on the first compile, all into declarations that already
  existed — so #88's precondition held for every one of them:

  | definition added | effect on `AdvanceAnimFrameMaybe` (0x405c40, 407 B) |
  |---|---|
  | `WidgetBaseObj0x4784c8::SetDescriptor` (0x454680, 67 B) | **free** — landed |
  | `RectFlagObj0x477820::MarkDirty` (0x436ab0, 41 B) | **−407 B** — withheld |
  | `WidgetBaseObj0x4784c8::ClearOwned` (0x454630, 75 B) | **−407 B**, and it is the INCLUDE the body needs, not the body |

  It is not a COUNT parity either: `SetDescriptor` alone (+1 definition) keeps 0x405c40 EXACT,
  `MarkDirty` alone (+1 definition) breaks it. Which body you add matters, not how many. It is
  also not an external-symbol effect — `MarkDirty`'s only outside reference is `g_worldBoard`,
  already referenced twice in that TU. **Operational rule: land declared-only bodies ONE AT A
  TIME with a full `progress.py` around each.** The cheap ones really are free (this session
  landed six that way across five TUs at zero collateral); the expensive ones are identifiable
  only by measuring, and a 41-byte body paying 407 is not a trade you would ever guess at.
  ⭐ Corollary worth its own line: when a body needs a NEW `#include`, price the include SEPARATELY
  by compiling it with the body removed. For `ClearOwned` that isolation showed the whole 407 B
  was the `#include "EffectSpawner.h"`, so the body itself is free and the park is against the
  include — a materially different retry condition to record.

- **#92 — A NEW .cpp has ZERO blast radius by construction, which makes "open a fresh TU" the
  cheapest move available for any unclaimed contiguous block.** `progress.py`/`cc.sh` compile
  each TU independently and nothing else `#include`s a new `.cpp`, so no existing TU's /Og state
  can move. v527 opened `src/BigObj.cpp` and landed 665 B into it (0x44b290 `BigObj::Load` 602 B
  + 0x44bcd0 63 B) with literally nothing else in the tree changing. Compare the same session's
  `src/WidgetBase.cpp` work, where a 41-byte body cost 407. **When triaging what to do next,
  weigh a fresh-TU block ABOVE a similar-size gap-fill in an established TU** — the established
  TU charges a dial toll that the new TU cannot. ⚠ The limit is declarations, not definitions: a
  new TU still cannot add a member to a shared class without paying that header's dial, which is
  why `BigObj`'s own ctor/dtor (0x44b190/0x44b220/0x44b200, 233 B) stayed out — they need
  `BigObj(unsigned int, char*)`, `virtual ~BigObj()` AND `Load(unsigned int, char*)` on
  `src/CursorDesc.h`, a header measured at −2096 B for one declaration.

- **#93 — `state & 4` on an iostream is `bad()`, and the descriptor-family loaders do NOT all use
  the same stream guard.** The shape `mov edx,[obj]; mov eax,[edx+4]; test byte ptr [obj+eax+8],4;
  jne skip` is `if (!pStream->bad())` — `[edx+4]` is the vbase offset to the `ios` subobject and
  `ios+8` is `state`, with `badbit == 4`. Distinguish it from `is_open()`, which compiles to a
  streambuf hop and an fd compare against −1: `mov ecx,[obj+eax+4]; cmp [ecx+0x4c],-1` (that
  `-1` is usually materialized into a spare register by an `or reg,0xffffffff`, which is a good
  tell). `CursorDesc::Load` (0x424bf0) and `Obj0x4779e0::Load` (0x41e6e0) use `is_open()`;
  `BigObj::Load` (0x44b290) uses `!bad()` on BOTH its istrstream and its ifstream. Do not port
  the guard across the family by analogy — read which shape the function actually has.

- **#94 — A pure BLOCK-LAYOUT residual at identical instruction COUNT is steerable by flipping
  the `if` test's polarity, and it is the first thing to try, not a park.** 0x412a80
  (`TrackDepotTileObj::PumpClaimWaitQueueMaybe`) compiled to the right length with `insns 33/33`,
  `reg_pen 0`, `identity_miss 0` and DIFF(73) — the entire dump was one arm relocated, with the
  `-` rows at the top and the matching `+` rows at the bottom. Writing `if (p == NULL) {...} else
  {...}` put the NULL arm in the fallthrough; the original's `test eax,eax; je <far>` proves the
  NON-NULL arm falls through, i.e. the source tested `if (p != NULL)` first. One edit, EXACT.
  **Read which arm the original FALLS THROUGH to (the `jcc` jumps to the OTHER one) and spell the
  source's condition so that arm comes first** — cl does not reorder the arms of an if/else, so
  this is faithful source recovery, not a trick. Cheap to check: `align` large with `reg_pen 0`
  and equal instruction counts is the signature.

- **#95 — WHEN A DEFINITION'S ARRIVAL COSTS A DECLARATION-COUNT DIAL, RE-SPELL IT AS A TU-LOCAL
  VIEW MEMBER (or vice versa): the two forms are byte-identical everywhere EXCEPT the dial, and
  the view is exactly ONE declaration more expensive.** Measured v528 on `0x454fa0`
  (`Ddraw_RecenterViewportOffsetMaybe`, 59 B) in `src/WorldBoardMaybe.cpp`. Written as the free
  `__fastcall(pBoard)` escape hatch that TU already uses two functions away, it landed 0x454fa0
  EXACT and cost `0x457ce0` (`FindNearestObjOfCategoryMaybe`) its whole 951-byte EXACT — TU total
  3676 B. Written as a one-method `struct …View0x454fa0 : WorldBoardPartial`, it landed 0x454fa0
  EXACT and left 0x457ce0 alone — TU total 4627 B. Same body, same 59 bytes, same instructions at
  every call site; the ONLY difference is that the view declares a member and the free function
  declares nothing. So the escape hatch is not the "lighter" spelling — it is one declaration
  lighter, which on a TU sitting next to a parity rung is the entire question. **This is a
  one-compile lever: whenever a new body costs a dial, try the other spelling before parking it
  or hunting for a declaration to add elsewhere.** Companion to the v490 finding that DELETING a
  view struct pays 951 B — same dial, opposite direction.

- **#96 — A PARITY-CLASS RESIDUAL IS NOT OWNED BY THE FUNCTION IT APPEARS IN; ANY NEW BODY IN THE
  TU RE-ROLLS IT, INCLUDING BACK TO EXACT.** `0x454fe0` (`WorldBoardMaybe_ResetAllTilesMaybe`,
  211 B) was exact, fell to DIFF(2) when v522's 0x456c60 landed, and was parked with "retry only
  on TU-context rotation" — its residual being a `wRows`/`wCols` commutative-load coin-flip no
  source spelling could reach. v528 added an unrelated 121-byte body (`0x4557c0`) to the same TU
  and got all 211 B back for free, so an 82-byte function's session netted the TU +293 B. Two
  consequences: (1) **read the WHOLE per-file table after every landing, not the row you were
  aiming at** — the win can be somewhere you were not looking, and so can the loss; (2) a park
  note that names its retry condition ("on TU-context rotation") is worth writing, because that
  condition arrives on its own as ordinary work continues.

- **#97 — MATCH THE SPELLING THE CALLERS ALREADY USE, OR LANDING A DEFINITION MANUFACTURES ALIAS
  FINDINGS.** `0x4255f0` (`CursorDesc::IsItemAvailableMaybe`) could not be declared on
  `src/CursorDesc.h` (the −2096 B dial), so the reflex was a TU-local view. But THREE consumer
  TUs already declared the address as a free `extern unsigned char __fastcall
  CursorDesc_IsItemAvailableMaybe(CursorDesc *)`. Defining it under a view name would have been a
  FOURTH name for one function and turned three previously-harmless declarations into three live
  `tools/lint_alias.py` findings — calls compiled against a symbol nothing defines, which
  relocation masking hides from `verify.py`. Defining it under the callers' own spelling was
  measured byte-identical (121 B EXACT either way) and kept the alias count at zero.
  **Before choosing how to spell a new definition, grep `src/` for the address: existing consumer
  declarations are a constraint, not just context.** Corollary to #92 — an unclaimed function's
  existing declarations tell you both that it is cheap to land AND how it must be spelled.

- **#98 — THE PER-TU DIAL FIRES ON ANY NEW *FILE* IN THE INCLUDE SET (CONTENT IRRELEVANT — AN
  EMPTY ONE DOES IT) AND ON ANY NEW *TYPE*, BUT NOT ON FREE-FUNCTION DECLARATIONS; AND IT IS A
  THRESHOLD, NOT A PARITY BIT.** #91/#95/#96 all model the dial as a declaration COUNT you can pay
  back by adding one more declaration. In `src/WidgetBase.cpp` (v530) that model is wrong on both
  halves. Adding `~WidgetBaseObj0x4784c8`'s 144-byte body cost a sibling
  (`AnimDescRefObj0x477488::AdvanceAnimFrameMaybe`, 0x405c40) its entire 407-byte EXACT — and **the
  definition alone was FREE; `#include "EffectSpawner.h"` alone paid the whole −407 B.** The full
  matrix, each cell measured against a re-verified `MATCH` control:

  | change to the TU | cost |
  |---|---|
  | the new function DEFINITION alone | free |
  | 1, 2, 3, 4 or 5 added free-function declarations | free |
  | duplicate `#include` of a header already in the TU (guard-swallowed) | free |
  | `#include <stdio.h>` / `<math.h>` / `<time.h>` — already pulled in transitively | free |
  | `#include <assert.h>` — a genuinely NEW file | **−407 B** |
  | `#include "AnythingNew.h"` where the file is **completely EMPTY** | **−407 B** |
  | `#include "EffectSpawner.h"`, or the same header trimmed to one method | **−407 B** |
  | TWO new empty headers instead of one | **−407 B (identical)** |
  | inline in the `.cpp`: `class X { public: void M(void); };` | **−407 B** |
  | inline in the `.cpp`: `struct X { int a; };` (POD, no methods) | **−407 B** |
  | inline in the `.cpp`: `typedef int X;` | **−407 B** |
  | one added MEMBER declaration on an already-included project header | **−407 B** |

  Read the top three rows together: an EMPTY new file costs the same as a real one, and the
  system headers are free only because they were already present — so **content is irrelevant;
  what matters is whether the file is new to the TU.** The mechanism is not identified (it looks
  like compiler bookkeeping state perturbing an allocator tie-break, the same arbitrary-TU-state
  family as Yoda lesson #15), and it does not need to be — the matrix is the usable result.
  Four consequences: (a) when a new body costs a dial, **bisect DEFINITION vs INCLUDE first** —
  they are different levers and this session's whole cost was in the include; (b) **#95's
  "re-spell it as a TU-local view" is NOT an escape** when the block is a missing type, because a
  view struct is itself a new type and costs the identical bytes (while breaking the
  never-duplicate-a-struct rule) — the free `__fastcall` half of #95 is the only spelling that
  adds neither a file nor a type, and it cannot express a `__thiscall` method with a stack
  argument, since VC5 rejects `__thiscall` on free functions; (c) because it is a THRESHOLD, you
  cannot buy it back by adding a second trigger, which is exactly what #95's lever assumes;
  (d) the toll is **one-time**, so a function blocked this way rides in free the moment its TU
  acquires that file or type for any other reason — record the blocked function where the next
  session will see it, and re-measure rather than re-transcribe.

- **#99 — A RECORDED "BLOCKED" VERDICT IS A HYPOTHESIS THAT DECAYS; RE-DERIVE THE BLOCK BEFORE
  BUDGETING AROUND IT.** `Ddraw::Ddraw_Init` (0x45b500, 728 B) was the largest unclaimed app-region
  gap and was carried from v443 to v529 — six sessions — as "toolchain-blocked: needs a real DX5
  SDK `ddraw.h`, which is a repo-wide decision", on the grounds that it QueryInterfaces
  `IID_IDirectDraw4` and builds a 124-byte `DDSURFACEDESC2` the VC5-bundled DX2/3-era header does
  not declare. Every one of those facts is true and the conclusion still did not follow. **Every
  field the body touches — `dwSize`, `dwFlags`, `dwHeight`, `dwWidth`, and `ddsCaps.dwCaps` at
  +0x68 — sits at an IDENTICAL offset in `DDSURFACEDESC` and `DDSURFACEDESC2`; only `sizeof`
  differs (108 vs 124), which is exactly what `DDSurfaceDescPadded0x7c` (shared in
  `src/LocoBitmap.h` since v200 for this very purpose) already supplies.** Likewise every vtable
  slot used sits at the same offset in `IDirectDraw2`/`IDirectDrawSurface` as in the 4-suffixed
  interfaces — a fact `src/Ddraw.h` had already written down two lines above the blocker note. The
  one genuinely missing symbol was the IID, i.e. one `DEFINE_GUID` line. It landed with ZERO
  collateral. The generalizable failure: the note reasoned from TYPE NAMES ("DDSURFACEDESC2 is a
  DX5 type, we lack DX5") instead of from LAYOUT, and it was then copied forward verbatim each
  session, so the cheap check was never re-run. **A block inherited through a pickup block has been
  re-asserted, not re-tested. Cost the re-derivation once — for a top-of-list gap it is minutes —
  and prefer a layout/offset argument to a name-based one.** Same family as #92: the bar for "this
  is unreachable" should be as high as the bar for a byte-match claim.

- **#100 — AN INSTRUCTION-COUNT-EXACT RESIDUAL MADE OF `cmp`s AGAINST CONSECUTIVE CONSTANTS IS A
  SWITCH WRITTEN AS AN IF-CHAIN, AND THE FIX IS FREE.** `TrackTileObj::ResetToBaseSubFrameMaybe`
  (0x44b0b0, 125 B) went from DIFF(89) to byte-identical in ONE compile by respelling its two
  tick-state dispatches as `switch` instead of `if (s == 0) … else if (s == 1) …`. The tell was
  precise and is worth recognizing on sight: **insns 51/51** — identical instruction COUNT, every
  call, operand and branch target already agreeing — with the entire 92032 residual being
  `test eax,eax` / `cmp eax,1` / `cmp eax,2` against the original's `sub eax,0 / je / dec eax /
  je / dec eax`. That ladder IS VC5's dense-switch lowering: it subtracts the base case once and
  then walks with `dec`, where an if-chain re-compares the value against each constant. Same lever
  as `ResolveWorldClickMaybe`'s case-dispatch note (src/WorldBoardMaybe.cpp), now confirmed to
  generalize to small non-contiguous sets too (the second block's cases are 4 and 5, and it still
  lowered as a switch). ⚠ **Check before applying — the inverse shape is real and the same
  function's sibling has it.** `TrackTileObj::ReleaseChannelAndDispatch` (0x44b130) dispatches on
  its argument with `test edx,edx / jne` then `cmp edx,1 / jne`, which is cl's if-CHAIN lowering,
  and that block already matched byte-for-byte; forcing a switch there would have BROKEN it. Read
  which lowering the ORIGINAL used — `sub`/`dec` ladder means switch, `test`+`cmp` means if-chain —
  rather than assuming the denser-looking source is always right.

- **#100a — #100's tell fires on a ONE-CASE switch too: a lone `sub eax,K / je` where you wrote
  `cmp eax,K / je` is the same signal, and the same free fix.** `PeerTrainNode::SetModeBMaybe`
  (0x44c150, 32 B) is the smallest possible instance — `dwModeBMaybe = mode;` then one branch
  choosing between `SetCarsReady(true)` and `SetCarsReady(false)`. Written as `if (mode != 2)` it
  scored insns **10/10** with the ENTIRE 6-byte residual being `cmp eax,2` where the original has
  `sub eax,2`; respelling it as `switch (mode) { case 2: … default: … }` made it byte-identical in
  one compile. So the trigger for #100 is not "several consecutive constants" — it is the
  `sub`-instead-of-`cmp` instruction itself, at any case count. (The third spelling,
  `SetCarsReady(mode != 2)`, is separately wrong here: the original branches to two independent
  `push`/`ret 4` tails rather than folding the flag with a `setne` into one shared call.)

- **#101 — A "DELIBERATELY NOT DECLARED, THE DIAL WOULD COST US" NOTE IS A HYPOTHESIS UNTIL SOMEONE
  PAYS TO PRICE IT — AND THE TU-LOCAL VIEW STRUCT IS USUALLY THE WAY TO HAVE BOTH.** `src/TilePlacedObj.h`
  had withheld `TrackConnectorTileObj::PumpClaimWaitQueueMaybe` for ~40 sessions on the stated
  theory that adding the declaration would cost `src/WorldBoardMaybe.cpp`'s 0x457ce0 and a second
  TU. v532 transcribed the function and measured it: the theory was RIGHT, and now has a number —
  **1103 B** (0x457ce0's whole 951 B plus 152 B in `src/Obj0x477798Family.cpp`) against the 88 B
  the function itself is worth. Two lessons, and the second matters more than the first. (1) Price
  the note once and write the number into it, so the next session neither re-tries it nor treats
  the withheld body as unreachable. (2) **A withheld DECLARATION almost never means a withheld
  FUNCTION.** Moving the body onto a TU-local view struct in the `.cpp` (`struct
  TrackConnectorTileObjPumpView0x44f340 : TrackConnectorTileObj { … };`) landed the same 88 B
  EXACT at zero cost to the shared header — the established `TrackTileObjTickView0x44b0b0`
  escape hatch. ⚠ This is the OPPOSITE of `src/WidgetBase.cpp`'s situation (#98), where a
  TU-local view is NOT an escape because that TU's dial is a threshold on NEW TYPES rather than a
  count of header declarations. Which dial a TU is on is a per-TU fact to measure, not to assume:
  the same trick that is free in one TU is the full price in another.

- **#102 — A CLASS'S OWN VTABLE DWORD NAMES ITS DESTRUCTOR'S NEIGHBOURS, AND THE `??_G` THUNK SITS
  BETWEEN THEM.** Generalizing v531's "resolve the vtable before reading the body" method to the
  dtor case specifically. On both classes claimed in v532 the layout was identical and is worth
  expecting: the ctor's COMDAT, then slot 0's `??_G` scalar-deleting thunk (small, ~30 B, left
  unclaimed per the ICF convention), then the real `??1` destructor, then the rest of the class's
  members in address order. Reading `getInt(vtable + 0)` gives the `??_G` address, and the real
  dtor is the next function after it — which is how `FUN_0044c0d0` was identified as
  `PeerTrainNode::~PeerTrainNode` before a line of its body was read. **A class whose vptr we model
  as a plain `void *pVtbl` field gets a NON-virtual `~T()` in our source and that is correct**: the
  compiler then emits exactly one `??1` COMDAT and no `??_G` of its own, matching the original's
  pairing, and the hand-written body just assigns the vtable field the way the ctor already does.

- **#103 — A VTABLE-SLOT DISPLACEMENT IS NOT A RELOCATION, SO MODELLING A SLOT AS NON-`virtual`
  IS A DEFECT `verify.py` CANNOT SEE.** The whole reason a wrong CALL TARGET is byte-invisible
  here (hence `tools/lint_alias.py`) is that `match.py`/`verify.py` mask relocations. A VIRTUAL
  call has no relocation to mask: `call [eax+0x54]` carries the slot index as a literal
  displacement in the instruction, so the byte-compare DOES check it — but only if you ever
  compile a call through that slot. A class member that is really a vtable slot but is declared
  non-`virtual` therefore sits harmlessly in the header for as long as nothing dispatches through
  it, and detonates the moment some sibling does. v533's 0x428a80 is the case: it reaches
  `WidgetPickerObj0x477cc8`'s slots 21 and 22 virtually, and both were declared as ordinary
  members. **The fix is a real model fix, and it has an arithmetic precondition worth checking
  BEFORE writing the caller:** count the base chain's virtuals to confirm the class's first NEW
  virtual really lands where the vtable says. Here `RectFlagObj0x477820` contributes slots 0–5,
  `AnimDescRefObj0x477488` extends to 14 and `WidgetBaseObj0x4784c8` to 20, so the picker's first
  two new virtuals ARE 21 and 22 — in header declaration order, which is now load-bearing and
  says so in a `⚠` comment. Same family as the "vtable slot and named method are one function"
  rule: read the `.rdata` dwords, don't infer. Dumping the table (`read_memory` at the vtable,
  parse LE dwords) also settles what the function you are about to write IS — 0x428a80 turned out
  to be the slot-20 override, not a new slot, which is why it needed no new declaration at all.

- **#104 — A `bool` PARAMETER THAT YOU SPELL `int` COSTS `xor/test/setne` PER FORWARDING CALL, AND
  C4800 TELLS YOU SO.** v533's `EffectSpawner::BroadcastToAllEffectsMaybe` (0x423f80) forwards its
  one parameter straight into `AnimDescRefObj0x477488::SetReadyStateMaybe(bool)`. Declared `int`,
  it compiled 129 B against the original's 115: each of the two forwarding sites grew a 6-byte
  `xor ecx,ecx / test ebx,ebx / setne cl` triple materializing the conversion. Respelled `bool`,
  byte-identical in one compile. **The compiler names this residual out loud** — `warning C4800:
  'int' : forcing value to bool` appeared once per call site — so treat a C4800 in a
  not-yet-matching function as a signature finding, not noise. The counter-intuitive part, and the
  reason the `int` spelling looked right: the original loads the parameter with a FULL-DWORD
  `mov ebp,[esp+0x14]` and pushes the whole register, which reads like an `int`. It isn't — the
  incoming stack slot is 4 bytes wide whatever the declared type, and cl is free to move all of it.
  Argument WIDTH at the call site is not evidence of the parameter's declared type; what the value
  is USED for is.

- **#105 — A DECLARATION THAT IS TOO EXPENSIVE TO ADD TO A SHARED HEADER IS NOT A REASON TO PARK
  THE FUNCTION: A TU-LOCAL SHIM NAMES A BASE CTOR FOR FREE.** Three descriptor-tier constructors
  (0x436400 `Obj0x478118`, 0x40e600 `CarKindDesc`, 0x44b190 `BigObj`) sat parked for many sessions
  behind the same reasoning, written out in each TU's head comment: the ctor must name
  `CursorDesc`'s 3-arg ctor in its mem-init list, `src/CursorDesc.h` has no such declaration, and
  adding one is a measured −489 B in `src/Obj0x4779e0.cpp`. Every step of that is true. The
  conclusion does not follow, because a mem-init list does not need the declaration to be on the
  REAL base — it needs it on a DIRECT base, and you can supply one locally:
  ```cpp
  class CursorDescCtorShim0x436400 : public CursorDesc {          // no data members
  public: CursorDescCtorShim0x436400(unsigned int, int, int); };  // declared, never defined
  struct Obj0x478118CtorView0x436400 : CursorDescCtorShim0x436400 { ... };
  ```
  Layout is identical (the shim adds nothing), the emitted `call` is reloc-masked so it byte-
  matches the real base ctor, and the shared header is untouched. All three compiled EXACT on the
  first try, +417 B with zero collateral. `src/Obj0x4779e0.cpp` had been doing exactly this for
  its own base ctor the whole time, and `src/Obj0x478118.cpp`'s parking note even SAID so in its
  last sentence — the pattern was known and simply not connected to the functions it unblocks.
  **Generalise the retrospective, not just the trick:** a parked note that names a specific cost
  is evidence about ONE lever, never about the whole search space. Before believing one, ask what
  else in the repo already solves the same sub-problem.
  - Cost, stated honestly: each shim is a declared-never-defined symbol, so it adds one
    `tools/lint_alias.py` finding (4 → 8 across the three TUs). That is the deliberate, documented
    trade the pre-existing shims already make — the call really does reach the right address — not
    a new defect. Suffix the shim with its consumer's address (`...Shim0x436400`) so it never
    becomes a second repo-wide definition of a same-named class and trips idiom-lint class E.

- **#106 — AN INLINE-DEFINED VIRTUAL DTOR ALSO EMITS A STANDALONE OUT-OF-LINE COMDAT, SO THE
  "UNCLAIMED" ADDRESS MAY ALREADY BE IN YOUR `.obj`.** `~LocoBitmap` is defined in the class body
  in `src/LocoBitmap.h` (deliberately — an out-of-line definition stops cl inlining it at
  0x42a140's `??_G` and inside `Resize`, neither of which matches). cl emits BOTH: the inlined
  copies at each use site AND one out-of-line `??1LocoBitmap@@UAE@XZ` COMDAT. That COMDAT is
  0x42a370, it had been compiling correctly for many sessions, and it was worth 90 bytes the
  moment someone added a marker for it — no new source at all. **Check for this before writing
  anything**: `llvm-objdump -t build/<TU>.obj | grep '??1'` (GNU objdump chokes on these objs).
  ⚠ A repo-wide sweep for the general case — every unclaimed app address vs. every COMDAT in
  every built `.obj`, reloc-masked — found NOTHING else, so do not go fishing: 146 unclaimed
  functions produced only four candidates, all of them 62-byte `??_G` templates that are
  byte-identical across five different classes and therefore attributable to none of them. The
  negative result is the useful part; the sweep does not need re-running.

- **#107 — A VIRTUAL-DISPATCHING `delete` NEEDS THE POINTEE COMPLETE, AND AGAINST A FORWARD
  DECLARATION cl SILENTLY EMITS A PLAIN `operator delete` INSTEAD.** A destructor body that does
  `delete pOwnedThing;` compiles fine when `pOwnedThing`'s class is only forward-declared — no
  error, and under VC5 not even a warning — but the emitted code is `push p; call operator delete`
  rather than the original's `mov eax,[ecx]; push 1; call [eax]` slot-0 dispatch. Nothing in this
  repo's harness objects: it is a legal program, the lengths differ so it just reads as a large
  DIFF, and the failure looks like a transcription problem rather than a missing `#include`. When a
  teardown's disasm shows the virtual-delete shape (`push 1` + indirect call through the object's
  own vptr) and your candidate shows the flat form, check for an incomplete type BEFORE re-reading
  the body. ⚠ The fix — completing the type — is a shared-header include, which is a byte lever in
  its own right and may cost more than the function is worth (v536: `LocoBitmap.h` into
  `src/CursorDesc.h` is −1956 B, and the "a guarded include is byte-neutral by construction" rule
  in CLAUDE.md only applies when EVERY consumer already includes the provider ahead of it — here 1
  of 25 did). Measure the include on its own before writing the body.

- **#108 — AN INCLUDE'S BYTE COST IS ITS DECLARATION CONTENT, NOT ITS POSITION IN THE HEADER.**
  Corollary of #107, worth stating because the obvious mitigation does not work. Deferring both the
  `#include` and the body that needs it to the very BOTTOM of a header — past all of that header's
  own class definitions, as an out-of-line `inline Class::~Class()` — was measured byte-for-byte
  IDENTICAL to putting the include at the top (v536, −2792 B either way, same per-TU victims).
  The position-sensitivity documented elsewhere in this file is about a header's OWN declaration
  COUNT; pulling in a foreign header adds its whole declaration set to every consumer TU regardless
  of where the `#include` line sits. Do not spend a measurement on re-ordering it.

- **#109 — WHEN A SWITCH'S ARM BODIES COME OUT IN THE WRONG ORDER, IT IS SOURCE ORDER THAT DECIDES,
  NOT LABEL ORDER.** With a jump table in play the table itself is index-ordered, so the case LABELS
  are already correct; what can still disagree is the physical layout of the arm BODIES after the
  `jmp [table+idx*4]`. cl emits them in the order the `case` groups appear in the SOURCE. v536's
  0x412940 laid `case 2` out first and `case 1/4/5` second; writing the arms in numeric order put
  them the other way round and nothing else in the function differed. Swapping the two `case` groups
  in the source was the last lever between DIFF(8) and EXACT. Cheap to test, invisible to reason
  about from the decompiler's own output (which always prints cases in numeric order).

- **#110 — THREE LEVERS DECIDE A SMALL FLAG-RETURNING FUNCTION, AND THEY ARE ALL VISIBLE IN THE
  EPILOGUES.** From v536's 0x412940 (EXACT, 108 B), a checklist for the very common "compute a
  byte/bool result across a branchy body" shape: (1) BRANCH ORDER — which side of the top-level test
  is the fall-through and which is pushed out of line (worth DIFF 55 → 8 alone here); (2) `break`
  VS `return` — read the exits, `mov al,<reg>` at every one means the source returns a VARIABLE
  everywhere and the arms `break` to a shared tail (cl duplicates the 3-instruction epilogue itself,
  so multiple epilogues are NOT evidence of multiple `return` statements), whereas `xor al,al` at
  some exits means those really do spell `return 0`; (3) source order of the switch arms (#109).
  The epilogue instruction is the cheapest of the three to read and settles (2) immediately.

- **#111 — A HANDLER'S "NOT MINE, USE THE DEFAULT" TAIL MUST BE THE CLASS-QUALIFIED BASE VIRTUAL,
  NOT THE FREE STUB AT THE SAME ADDRESS, AND THE TELL IS REGISTER LIVENESS.** In the window family
  a base slot's default body is very often a shared free function — `PopupWndBase`'s slot 0x34
  default and `WindowBase_DefWindowProcStub` are BOTH 0x422ea0, one body serving a `__stdcall`
  free function and a `__thiscall` vtable slot because it ignores `this`. So an override's fallback
  can be spelled either way, both are semantically correct, both link, and the free-function
  spelling is the one a reader reaches for because sibling TUs genuinely use it as a tail
  (`src/AlbumCardWnd.cpp`, `src/EditCardWnd.cpp`). It is still wrong: v537's `CreditsWnd::
  OnLButtonDown` (0x40f7a0) landed DIFF(15) at the correct 70-byte length and the correct 21
  instructions with `return WindowBase_DefWindowProcStub(...)`, and EXACT on the next compile with
  `return PopupWndBase::OnLButtonDown(...)`.
  ⇒ **Diagnose it by asking whether the ORIGINAL touches ECX on that path.** A `__thiscall` call
  needs `this` still live in ecx, so the original's fallback arm loads nothing into ecx and simply
  falls into the call. Under the free-function spelling cl is free to use ecx as a scratch register
  for the argument shuffle — and it does, which additionally re-orders the load/push interleave and
  shifts every `[esp+N]` displacement downstream. That whole cascade reads as ordinary scheduling
  noise; it is one wrong call spelling. Once applied, the other four slots of the same class
  (0x40f870/0x40f840/0x40f7f0/0x40f820) matched on the first compile.
  ⚠ `lint_alias.py` cannot see this — both spellings name a real symbol at the right address — and
  neither can `verify.py`, which masks the relocation. Only the byte diff shows it.

- **#112 — A HEADER'S DECLARATION DIAL CAN BE A THRESHOLD RATHER THAN A PARITY, AND THE TWO CALL
  FOR OPPOSITE STRATEGIES.** #95's parity levers are paid back by adding a second trigger; a
  THRESHOLD is not, and once crossed every further declaration is free. `src/CreditsWnd.h` is a
  measured threshold (v537): the FIRST declaration added to `CreditsWnd` costs
  `WorldBoardMaybe_ResetAllTilesMaybe` (0x454fe0) its 211-byte EXACT — that TU includes the header
  for `g_pCreditsWnd` — and adding 1, 2 and 5 declarations all cost exactly the same 211 B.
  ⇒ **Test which kind you are holding BEFORE deciding to withhold, by adding a second declaration
  and re-measuring.** On a threshold the correct move is the opposite of the usual caution: land
  EVERYTHING you have for that class in one go, because the marginal declaration is free and only
  the first one is not. v537 turned a −141 B one-function trade into a −8 B five-function one this
  way (+203 B of new EXACT against the single 211 B toll), and all five were vtable slots that had
  been sitting unclaimed. The same reasoning says a threshold is NOT recoverable by trimming one
  declaration back out — you would have to retire every one of them.

- **#113 — THE THREE DECLARATION CURRENCIES OF THE WIDGET FAMILY ARE NOT INTERCHANGEABLE; EACH
  MOVES ITS OWN VICTIM.** v486 recorded that `src/Obj0x477798Family.h`'s `_v15` could be spent to
  absorb a `src/WidgetBase.h` declaration change, and left an instruction to redeclare it "if a
  future session removes that WidgetBase.h declaration". v537 did exactly that and MEASURED the
  result: `_v15` restores its own TU (+152 B) and costs `src/RoadVehicleActor.cpp` −504 B while
  leaving the intended victim, `WorldBoardMaybe::FindNearestObjOfCategoryMaybe` (0x457ce0, 951 B),
  completely untouched. 0x457ce0 answers to `AnimDescRefObj0x477488`'s OWN declaration count and
  nothing else. Two further currencies were tried on the same debt and are also refuted:
  `EnsureSoundPlayingMaybe` (0x405ab0) costs −504/−152/−407 without recovering it, and the class's
  genuinely-missing slot 15 (`SpawnOwnedActorMaybe`/0x4062a0) needs `class DecorActorBase;`
  forward-declared in `src/WidgetBase.h`, which is itself a −6174 B lever across nine TUs.
  ⇒ **A recorded "currency" is a claim about ONE victim at ONE parity, not a general-purpose
  balance.** Re-measure before spending it, and when it does not work, correct the note in place —
  a stale currency note costs a later session a full measurement cycle to disprove.

- **#114 — A RETURN TYPE IS MEASURABLE, NOT A GUESS: A `void` THAT COMPILES EXACTLY ONE EPILOGUE
  ZEROING PER RETURN PATH SHORT IS REALLY A FLAG RETURN.** v538's `TrackTileObj::
  EnqueueClaimWaiterMaybe` (0x44f3a0) was DECLARED `void` in the header on nobody's evidence — no
  caller reads its result — and compiled to 97 bytes against the original's 101. The 4 missing
  bytes were exactly two `xor al,al`, one per return path; retyping to `unsigned char` with an
  explicit `return 0;` landed the correct 101 immediately. cl does not emit a dead zeroing for a
  `void` function, so the shortfall is diagnostic rather than ambiguous.
  ⇒ **When a body you believe is content-complete comes out a small multiple of 2 bytes short and
  the missing instructions are epilogue register zeroings, suspect the DECLARED RETURN TYPE before
  suspecting the body.** Count the return paths first: N paths × 2 bytes is the signature. The
  reverse reading also holds — a candidate 2N bytes LONG with unused `xor al,al` means a declared
  flag return that is really `void`. Related to #110, which reads the same family of tells on a
  function whose flag return was never in doubt.

- **#115 — RE-READING A MEMBER INSIDE EACH ARGUMENT BEATS CACHING IT IN A LOCAL, BECAUSE THE LOAD
  IS CSE'd EITHER WAY AND ONLY THE LIVENESS CHANGES.** v538's `WidgetPickerObj0x477cc8::
  RepositionWithHotspot` (0x428770) builds two arguments out of one descriptor pointer. Cached as
  `BigObj *pKindDesc = pIcon->pKindDesc;` it compiles to the right 57 bytes and the right 23
  instructions and is still DIFF(22): cl computes BOTH arguments into separate registers and then
  pushes both. Written as `embeddedIcon.pKindDesc->fieldA + x` / `...->fieldB + y` directly in the
  argument list it is EXACT — cl CSEs the pointer load anyway, so nothing extra is emitted, but it
  now keeps ONE scratch register and pushes each argument the moment it is computed, which is what
  the original does.
  ⇒ **A named local is not free even when it generates no extra instruction.** It widens the value's
  live range across the whole argument list and that is enough to flip cl's push interleave. This is
  the exact INVERSE of #20c/#81-style "introduce a local to pin an evaluation" levers, so try both
  directions on any argument-shuffle residual; the instruction count is identical either way and
  will not tell you which side you are on.

- **#116 — TWO HALVES OF A "SAME THING, OTHER CLASS" PAIR OFTEN LOWER DIFFERENTLY, AND THAT
  DIFFERENCE IS SOURCE, NOT A TIE-BREAK.** v538 landed both halves of the claim-wait FIFO append:
  the connector half (0x44f3a0) allocates the node FIRST and then branches on the head pointer,
  while the depot half (0x412af0) walks to the tail FIRST — leaving `pTail` NULL on an empty
  queue — and allocates after. Transcribing the second in the first's shape does not match; each
  was written in its own shape and each landed. The depot half's duplicated post-link stores in
  both arms are cl's own tail duplication, not two source copies.
  ⇒ **Do not "harmonize" a twin against the sibling you just matched.** Read the second body's own
  disasm as if the first did not exist. Two humans, or one human on two days, genuinely wrote these
  differently, and the byte-match is what says so.
  ⇒ Also confirmed here, and worth applying before anything subtler: **the empty/NULL case belongs
  in whichever arm the original SINKS.** `if (head != 0) { walk; append } else { setHead }` puts the
  set-head store in an out-of-line tail block past the epilogue (`je` forward); writing the same
  logic as `if (head == 0)` inverts the block layout for DIFF(47) against DIFF(15) on an otherwise
  identical body. The branch's polarity in the disasm reads the source's `if` directly.

- **#117 — A STORE THAT THE ORIGINAL EMITS FIRST MAY NEED TO BE THE SECOND STATEMENT, WHEN BOTH
  FEED A CALL'S SETUP.** The last 15 bytes of 0x44f3a0's residual were `mov ecx,esi` (the callee's
  `this`) landing after a field store instead of before it. The fix was to write the OTHER store
  first in source — the sentinel-pair copy ahead of `dwModeAMaybe = 2` — even though the original
  EMITS the `dwModeAMaybe` store first. With the statements in emission order cl hoists that store
  above the call's own `push`/`mov ecx` setup; with them swapped it schedules the store into the
  setup, which is what the original does.
  ⇒ **Do not read a store's emitted position as its source position when a call follows.** cl
  interleaves independent stores into the following call's argument setup, so emission order is
  evidence about scheduling, not about statement order. Swapping two adjacent independent stores is
  a one-compile probe worth running before any structural theory.

- **#118 (process) — TEST A "THIS DUPLICATED FIELD SHOULD MOVE TO THE BASE" HYPOTHESIS AGAINST THE
  ALLOCATION SITE BEFORE REFACTORING ANYTHING.** `src/TilePlacedObj.h` records that
  `TrackConnectorTileObj` and `TrackDepotTileObj` carry an IDENTICAL `+0x11c/+0x120/+0x124` triple
  and that the duplication "should move down to the base once a tile dtor pins the layout". It must
  not: the factory `NetSessionEventQueue::BigObj_CreateAndInsert` allocates **0x11c** for
  `new TrackTileObj` (at 0x41de4a) against 0x128 for the connector leaf and 0x12c for the depot
  leaf, so the shared-looking fields provably sit PAST the base's end. One `objdump` of the
  factory's four `push <size>` / `call operator new` pairs refuted a multi-file struct refactor in
  under a minute.
  ⇒ **CLAUDE.md's "a `new(N)` site is the `sizeof` oracle" rule is not only for sizing a struct you
  are building — it is the cheapest possible REFUTATION of a layout hypothesis.** Any base/leaf
  question where the base is directly instantiated somewhere answers itself. The consequence for
  0x44f3a0 is that a base method genuinely has to reach a leaf field through a downcast, which is
  ugly and is nevertheless the true model.

- **#119 (process) — THE LINKER SCATTERS COMPILER-GENERATED COMDATs, SO THEY GO UNCLAIMED FOREVER
  UNLESS YOU GO LOOKING BY NAME.** A `??_G` scalar-deleting-dtor thunk is emitted from the same
  source line as the `??1` it calls, but the linker may place the two anywhere: RoadVehicleActor's
  sit 0x1f000 bytes apart (0x4363e0 vs 0x4533f0). Address-order and neighbour-of-a-known-function
  reasoning — the two ways this project normally finds work — therefore never reach them. v539
  found **twenty-five** such COMDATs that our objects were ALREADY EMITTING, worth 967 B, and
  claiming each was a COMMENT-ONLY stacked marker line: byte-neutral by construction.
  The families to sweep for: `??_G` (scalar deleting dtor), `??1`/`??0` (implicit dtor/ctor of a
  class whose only members are embedded objects), `??_F` (vector-ctor-iterator callback for an
  embedded array), `??_D` (the iostream vbase-dtor helper every TU with a stack `ifstream` gets).
  ⇒ `tools/find_unclaimed_comdats.py` automates both halves of it. Re-run it after ANY session
  that lands a destructor — a new `??1` frequently brings a free `??_G` with it.

- **#120 — AN IN-CLASS EMPTY DESTRUCTOR STILL EMITS BOTH OUT-OF-LINE COMDATs, IN THE TU THAT EMITS
  THE VTABLE.** `SavedFileEntry`'s dtor is written `virtual ~SavedFileEntry() {}` inside the class
  body in src/WidgetPicker.h, and src/WidgetPicker.h's own note said its ctor/dtor were "fully
  INLINE at both their call sites". Both are true, and both COMDATs exist anyway: 0x429820 (`??1`,
  14 bytes — a vptr re-stamp plus a TAIL JUMP into the embedded ThumbnailBmp's dtor, no prologue
  at all) and 0x429830 (`??_G`, the ordinary 30-byte thunk). Inlining at the call sites and
  emitting the out-of-line copy are not alternatives; a polymorphic class gets the out-of-line
  definitions wherever its vtable lands. ⚠ Do not read "defined in-class" or "inlined at its call
  site" as "no COMDAT to claim" — check the `.obj`'s symbol table instead.

- **#121 — A `??_G` THUNK IS CLAIMABLE EVEN WHEN THE DESTRUCTOR IT CALLS IS WITHHELD.** 0x454580
  (`??_GWidgetBaseObj0x4784c8`) byte-matches today although ~WidgetBaseObj0x4784c8 (0x4545a0) is
  priced-and-withheld and has no body in src/ at all. They are independent COMDATs, and the thunk's
  30 bytes are identical either way because the call target is a masked relocation. cl emits the
  thunk from the declared-only `virtual ~T()` alongside the vtable; the call is simply an
  unresolved external, exactly like any other call to a not-yet-transcribed function — `??1T` is
  the right symbol by name, so this is NOT a `lint_alias`-style wrong target.

- **#122 (tooling) — A MARKER WHOSE COMDAT DOES NOT EXIST IN THAT TU DOES NOT FAIL, IT MISPAIRS THE
  WHOLE FILE.** `match.py`'s `pair_by_name` falls back to POSITIONAL pairing when a marker's
  derived mangled key matches no COMDAT, so every later marker in the file shifts by one. v539 put
  `??_G` markers on four Obj0x477798Family classes and one PeerTrainNode class whose TUs do not
  emit those thunks: −2383 B and −961 B respectively, with no error, no lint finding, and a
  `progress.py` table that simply looked bad. ⇒ Never add a marker on the strength of a call-target
  or byte argument alone — confirm the symbol is DEFINED (not `sec 0`) in the `.obj` first, and
  always re-run `tools/cc.sh` on the TU before believing a marker landed.

- **#123 (tooling) — PASSING A `::`-QUALIFIED NAME TO `set_function_prototype` DOUBLES THE
  NAMESPACE, INVISIBLY.** Ghidra's rename endpoint rejects some correct names outright (a
  token-subset guard read `GetOrLoadFrameBitmap` as an extension of the existing `Load`), and the
  documented workaround is `set_function_prototype`. But if the function is ALREADY in its class
  namespace, a qualified prototype name produces the FQN
  `Obj0x478118::Obj0x478118::GetOrLoadFrameBitmap` — which `get_function_by_address` renders as the
  correct single-qualified name, so a read-back looks clean. Only `lint_ghidra_sync.py` catches it.
  ⇒ Pass the BARE name to `set_function_prototype`, and when the name is already wrong, rename back
  to the default `FUN_<addr>` first and then set the prototype. Read back with the LINT, not just
  with `get_function_by_address`.

- **#124 — PRICE THE HONEST DECLARATION BEFORE REACHING FOR THE VIEW-STRUCT DODGE, AND RECORD THE
  NUMBER.** 0x4343b0 is genuinely `DecorActorBase::BlitAnimFrameMaybe` (all three actor vtables
  carry it at +0x2c where the non-actor tables carry the base's 0x405e60). Declaring it on the
  class costs src/WorldBoardMaybe.cpp's 0x457ce0 its whole 951 B and src/RoadVehicleActor.cpp 504 B
  — net −1345 B against the 50 B the function is worth — while the TU-local view struct lands the
  same 50 B with the full per-file table showing ONE moved row. Both facts are worth having: the
  view is what ships, and the measured price is what stops the next session re-running it.
  ⚠ Note the +60 B CONSOLATION src/TilePlacedObj.cpp paid out under the expensive variant. A
  header dial can pay one TU while robbing two others, so judge a dial on the TOTAL, never on the
  first row that moves.

- **#125 — THE 13 FILE-SCOPE STATIC-OBJECT INITIALIZERS AT 0x45c530–0x45c7a0 ARE ONE UNCLAIMED
  SEAM (~442 B), AND VC5 WILL NOT CURRENTLY EMIT THEIR SHAPE.** The block is 13 alternating
  24 B/10 B pairs, and each pair is the classic MSVC dynamic initializer for ONE global object:
  `mov ecx,&g_obj; call ctor; push <thunk>; call atexit; add esp,4; ret` plus a 10-byte
  `mov ecx,&g_obj; jmp dtor` thunk. They cluster far from their own TUs' code because with `/Gy`
  every real function is a COMDAT while these are plain `.text`, so the linker piles all 13
  objects' contributions together. TEN of the 13 have BOTH their ctor and dtor already
  transcribed — the seam is blocked on codegen, not on reverse engineering.
  ⇒ Defining the global (e.g. `ScreenSaver g_screenSaver;`, which no TU does today — the linker
  is satisfied by `link/gen_stubs.obj`) makes cl emit `.CRT$XCU`, the `.bss`, and FOUR chained
  COMDATs `$E1`–`$E4` (`$E4 = call $E1; jmp $E3`; `$E1 = mov ecx,obj; jmp ctor`;
  `$E3 = push $E2; call atexit; …`; `$E2` = the dtor thunk) where the original has TWO merged
  functions. Measured: the shape is NOT reached by `/Gy-`, `/GX-`, `/O1`, `/Ox`, or `/Oxs`, and
  `/Ob2` overshoots by inlining the ctor AND dtor bodies into `$E4`/`$E2` outright. The original
  is exactly `$E4` with `$E1`/`$E3` inlined and the ctor still called — a point between `/Ob1`
  and `/Ob2` that this cl does not expose. Adding the definition is byte-neutral for the TU's
  own functions (checked on src/ScreenSaver.cpp: 7/9 exact before and after), so it costs
  nothing to hold. Revisit only if the `$E`-merge lever is ever found.
  ⚠ Corollary for reading `tools/find_unclaimed_comdats.py`: it reports 0 unique matches here
  because our objects do not emit the merged form at all, NOT because the seam is worked out.

- **#126 — "UNCLAIMED" INCREASINGLY MEANS "SITTING BEHIND A PRICED DIAL", NOT "UNREAD".** Of the
  ~99 app-region functions still unclaimed after v539's `??_G` sweep, the four largest clean
  clusters surveyed this session were each blocked by a dial their own header already documents,
  not by difficulty: RectFlagObj0x477820's four bodies (−407 B via 0x405c40, see the ⛔ block in
  src/WidgetBase.h), PeerTrainSlotQueueMaybe's dtor pair (its header's live 0x457ce0 dial, and a
  virtual dtor would force the class off its `void *pVtbl` model), the four Obj0x477798Family
  62 B deleting-dtors plus their `??_G` thunks (the v486 hierarchy problem), and ThreadWrapper's
  0x461690 (deliberately absent since v495 so `??_G` can inline it). ⇒ Budget a survey pass
  BEFORE a transcription pass: read the owning class's header first, because on this repo it now
  usually already carries the answer. The genuinely free ones left are the leaves whose class is
  modelled and whose TU has no canary — 0x459d40 was one, and it landed EXACT on the first
  compile and paid a 60 B bonus besides.

- **#127 — LANDING A REAL DEFINITION CAN CREATE A `lint_alias` DEFECT OUT OF NOTHING, SO RE-RUN
  THE LINT AFTER EVERY NEW BODY.** A TU-local methods-only view struct naming an address under a
  local spelling is CORRECT while that address is untranscribed and becomes a live wrong-call-
  target the instant a real definition lands elsewhere — the count went 8 → 9 the moment
  0x423d20 was transcribed, from a view (`WorldDirtyRectCollectionMaybe::RemoveHandle`) that had
  been harmless for many sessions. The fix (delete the view, include the real header, call the
  real names) was byte-neutral repo-wide here, but it IS a declaration change and must be
  measured with a full per-file table like any other. Same lesson from the other direction as
  v490's: an alias is a latent byte lever AND a latent defect, and transcription is what trips it.

- **#128 — TWIN SELECTION IS NOT A FIXED PROPERTY OF THE TU: AN UNRELATED HEADER'S CONTENT MOVES
  IT, AND TWO TWIN SITES IN ONE TU ARE DECIDED INDEPENDENTLY.** v457 established that when the
  same comparator source exists twice in the image (`CompareEntriesMaybe`, 0x435c00/0x4361e0 and
  friends), WHICH of the two operand-evaluation-order twins a given text produces is "decided by
  the TU, not by the source" — and every note since has treated that as a constant of the .obj.
  v542 refutes the constant half. Adding three method DECLARATIONS to `src/AnimEffectObj.h` for a
  class the comparator never mentions flipped `src/Obj0x477798Family.cpp`'s 0x4243c0 site from the
  152-byte twin (EXACT since v525) to the 156-byte twin, with zero change to that function's own
  source. Worse and more useful: it moved ONLY that site. The same TU's other twin site, 0x4361e0,
  stayed on the 152-byte form — so the file now holds each site on exactly the twin the OTHER one
  wants. ⇒ Treat "this TU sits on twin A" as a measurement with a timestamp, not a property; and
  when two twin sites live in one TU, re-check BOTH after any header change, because they can
  move apart. If a lever is ever found that swaps two inverted sites, both match at once (worth
  +156 B here). Probed and refuted as levers: a spare non-virtual declaration on the class (the
  DIFF is byte-identical with and without — a THRESHOLD, not a parity bit, same as v540's
  `src/WidgetBase.h` result), and a dummy definition inserted immediately ahead of the other site
  (no effect on either). Full writeup: `docs/PARKED.md` v542.

- **#129 — DECLARING A CLASS'S MISSING OVERRIDES IS A CORRECTNESS FIX TO REAL LINKED CONTENT, NOT
  JUST A BYTE PLAY — CHECK THE EMITTED `??_7` COMDAT AGAINST THE IMAGE'S VTABLE.** A derived class
  whose overrides are simply not declared still compiles, still byte-matches every function in the
  TU, and passes every lint — but the vtable COMDAT it emits silently inherits the BASE's function
  addresses in those slots. `AnimEffectObj0x477a90` shipped that way for many sessions: the image's
  vtable 0x477a90 holds 0x423840/0x423890/0x423870 at slots 7/9/10, and the repo emitted
  0x405a20/0x4061b0/0x405c40 (the base's) — 12 of 15 slots correct, with nothing anywhere
  objecting, because those three overrides were merely unclaimed rather than modelled wrong. This
  is the same family as CLAUDE.md's "a vtable slot and a named method at the same address are ONE
  function" hazard, approached from the other side: there, one function grew two names; here, three
  real functions had no name at all and the table quietly filled itself in with the wrong ones.
  ⇒ **Recipe:** dump the class's `??_7…@@6B@` relocations out of the built `.obj`
  (`llvm-objdump -r build/<TU>.obj`, read the `.rdata` block — one reloc per slot, in slot order)
  and diff that list against the image's vtable dwords. Slot count and order line up 1:1, so a
  wrong or base-inherited slot is immediately visible. Worth doing for any class whose unclaimed
  function list contains addresses that the image's own vtable already points at — that is the
  cheapest possible confirmation that a "new" function is really an override, and it names the
  slot for you before you read a single instruction.

- **#130 — `__purecall` IN A VTABLE SLOT IS THE IMAGE TELLING YOU THE METHOD IS `= 0`.** Loco's
  `__purecall` is **0x467e90** (`push 25; call _amsg_exit` — 25 is `_RT_PUREVIRT`, the R6025
  "pure virtual function call" message). A slot holding it was declared PURE VIRTUAL in the
  original source; a slot holding anything else has a real body. Modelling a pure slot as an
  ordinary declared-only virtual compiles, links (the declaration is never defined, so the smoke
  link just gen-stubs it) and byte-matches everything — while the emitted `??_7` COMDAT names
  that stub where the image names `__purecall`. `tools/vtable_audit.py` reports these directly.
  ⚠ **The fix is not local**: `= 0` makes the class abstract, so EVERY derived class that is
  instantiated anywhere must then declare all four/N overrides, or the TU stops compiling with
  `C2259: cannot instantiate abstract class`. Measured on `WidgetBaseObj0x4784c8` (slots 17–20,
  vtable 0x4784c8): adding `= 0` alone turned **12 TUs into COMPILE FAILED** (−34602 B, −162
  exact funcs) because five leaves — `BuildToolButton`, `SelectedObjWidgetMaybe`,
  `WidgetPickerObj0x477cc8`, `WidgetTagObj0x478378`, `WorldActionCursor` — declare none or only
  some of the four. Price the whole leaf set before starting. `DecorActorBase` (slots 16/18) is
  already modelled correctly and is the worked example to copy.

- **#131 — MEASURE BOTH SIDES OF A COVERAGE RATIO WITH THE SAME RULER; `app_funcs.txt` IS NOT
  THE FUNCTION'S REAL EXTENT.** `tools/progress.py` reported "100.47% transcribed" for many
  sessions and the pickup wrote it off as "marker-set vs app-region rounding". It was not
  rounding — it was two different rulers, and the honest figure is **99.20%**. The numerator
  summed OUR compiled COMDAT lengths (`match.trim_pad`); the denominator summed
  `toolchain/test/app_funcs.txt`, i.e. Ghidra's `Body:` span. Those disagree two ways:
  (1) a PARTIAL candidate is routinely longer or shorter than the original (net **+2331 B**
  across 383 functions), and (2) — the bigger one — Ghidra's `Body:` stops at the last
  INSTRUCTION and **omits the trailing jump table the linker copies as part of the same
  COMDAT**, so the denominator undercounts **122 switch-heavy functions by 7293 B total**
  (0x434d70: Ghidra 211 B vs. real 684 B). Plus 3 CRT-region markers (51 B) that counted in the
  numerator only. This is the same jump-table trap CLAUDE.md documents for `asmscore.py --len`,
  showing up in the dashboard instead of in a diff. ⇒ **The right ruler, now used on both
  sides: run to the next function's start, then trim trailing 0xCC/0x90 padding** (floored at
  Ghidra's own span). It is verifiable rather than assumed: across the 822 already-EXACT
  functions — where our COMDAT is byte-identical to the image and therefore ground truth — the
  trimmed extent agrees to within **20 bytes in total**. The corroborating check that the fix
  is right: `progress.py`'s headline `% transcribed` and its independently-computed
  `app-region-only coverage` line now both read 99.20%, where before they read 100.47% and
  99.23%. **Two numbers in one tool that are supposed to mean the same thing and don't is a
  bug report, not a rounding note.**

- **#132 — A CLASS MODELLED SHORT OF ITS REAL VTABLE LENGTH SILENTLY MIS-SLOTS ITS DERIVED
  CLASSES' OVERRIDES, AND NOTHING BUT THE `.rdata` TABLE WILL TELL YOU.** `PopupWndBase` emitted
  21 slots where the image's table (0x477898) runs 0..36. Slots 0–20 agreed, so every audit that
  compared only the slots we emit reported "fully agrees" — `tools/vtable_audit.py` scored the
  class OK right up until the base was extended. The damage was one level down: `CreditsWnd`'s
  `OnClose`/`OnKillFocus` and `BuildToolCursorWnd`'s `OnClose`, having no base slot to override,
  were appended as NEW virtuals and landed at slots 21/22 instead of 31/24 — a `virtual` keyword
  that reads exactly like a correct override and compiles clean. ⇒ **When a derived class'
  "override" of a base method lands in a slot the image doesn't corroborate, suspect the BASE's
  slot COUNT before you suspect the derived declaration.** Two cheap ground truths: the table
  ends where the next `.rdata` object begins (here a NULL dword at 0x47792c, the same shape
  `WindowBase` 0x477c30 and `CreditsWnd` 0x477680 show), and a sibling hierarchy's table can be
  read across — Loco's `PopupWndBase` and `WindowBase` tables are structurally PARALLEL with a
  one-slot shift (their routers sit at 9 and 10) and hold the SAME handler address at every
  shared slot (0x426950, 0x426a60, 0x426ac0, 0x426ad0), which pins ~15 slot identities at once
  without reading a single body.

- **#133 — A BEHAVIOURAL NAME IS NOT EVIDENCE ABOUT A SIGNATURE; THE `ret 0xNN` IS.** 0x438890
  was modelled as `void BuildToolCursorWnd::NotifyToolModeChanged()` — a name invented from what
  the body does, with no call sites to contradict it — for many sessions. It is actually the
  class's WM_KILLFOCUS override: the image ends it `xor eax,eax; pop esi; ret 0x10`, i.e. FOUR
  stack arguments and an LRESULT return, which a zero-argument `void ()` signature cannot emit at
  all (it compiles to a bare `ret`). The wrong model was invisible to every check the repo has —
  it compiled, it smoke-linked, its residual looked like an ordinary CSE tie-break, and
  `lint_ghidra_sync` was happy because Ghidra carried the same invented name. ⇒ **`ret 0xNN`/4 is
  the argument count, free and unambiguous, on every `__thiscall` and `__stdcall` function**;
  check it before believing any hand-derived signature, especially one whose name came from
  behaviour rather than from a vtable slot. Same family as CLAUDE.md's v477 CarNetObj lesson —
  a model merely CONSISTENT with the bytes is not a model that is TRUE.

- **#134 — A REDUNDANT PROBE STRUCT CAN BE LOAD-BEARING FOR BYTES; MEASURE BEFORE DELETING ONE.**
  Once `PopupWndBase` declared all 37 slots itself, `PopupWndBaseVtblProbe` was pure redundancy
  as a TYPE and deleting it (retargeting `PopupWndBase_RouteMessage`'s 17 dispatches from
  `pProbe->` to `pWnd->`) is byte-IDENTICAL for its own TU — and cost `src/WorldBoardMaybe.cpp`'s
  0x454fe0 its 211-byte exact, through `CreditsWnd.h`'s transitive include of the header. This is
  the mirror image of CLAUDE.md's "fixing a `lint_alias.py` finding is a BYTE LEVER": retiring
  declarations moves the dial in whichever direction the dial happens to sit. ⇒ Treat "delete the
  now-redundant helper type" as a MEASURED change, never a cleanup — and if it costs, keeping it
  with a comment recording the measurement is a legitimate outcome (`src/WindowBase.h` keeps its
  own probe on exactly this footing).

- **#135 — THE `PopupWndBase.h` DECLARATION-COUNT DIAL IS NOT A PER-HEADER PARITY BIT, AND ITS
  VICTIM SET IS FIXED.** Extending the class 21 → 37 slots (net +9 declarations) moved
  `src/TutorialWnd.cpp`'s 0x452b00 MATCH → DIFF(30) and `src/WorldBoardMaybe.cpp`'s 0x454fe0
  DIFF → MATCH, simultaneously and in opposite directions, for a net −38 B. Paying the debt back
  in `src/TutorialWnd.h` — the one header TutorialWnd.cpp sees and WorldBoardMaybe.cpp does not —
  did NOT flip it back: one added declaration there is inert. So the CLAUDE.md guidance "pay by
  ADDING a real declaration" is a per-header weak prior, not a mechanism you can steer. 0x452b00
  is the repo's known shared victim of this class (five earlier header levers each knocked it
  MATCH → DIFF(32) independently; v449's repo-wide attempt lost 1008 B), and its documented
  RECT-declaration-order lever scores WORSE here (byte_diff 26 → 28), confirming that order is
  compensating for TU-level codegen state rather than recovering a truer source shape.

- **#136 — A DECLARED-ONLY VIRTUAL IS THE RIGHT MODEL FOR AN ICF-FOLDED SLOT, AND IT CAN BE WORTH
  REAL BYTES.** When two classes share one vtable slot address because the linker folded identical
  COMDATs, only ONE TU can host the body — but *both* classes still override the slot, and a
  declaration is what controls the vtable. `AlbumCardWnd` had neither of its two folded slots
  declared (0x2c = 0x40b4c0, folded with `ApplSetupWnd`; 0x80 = 0x419a10, folded with
  `EditCardWnd`), so we emitted `WindowBase`'s defaults into its table — `vtable_audit.py` scored
  slot 0x80 a hard MISMATCH. Declaring both, bodyless, fixed the table AND paid **+249 B**
  (it handed `src/TutorialWnd.cpp` back the 0x452b00 exact that v544 had spent). The same shape
  cleared three more rows for free: `ApplSetupWnd`/`NetSetupWnd`/`SplashWnd` all carry 0x4323c0
  at slot 0x40 while its body lives in `src/MapWnd.cpp`. ⇒ "The body isn't ours" is a statement
  about the DEFINITION, never about the declaration; `src/NetSetupWnd.h` had withheld its line for
  years on exactly that confusion ("is NOT a NetSetupWnd member"). Declared-only virtuals do not
  break `link_check.sh`.

- **#137 — PROMOTING A NON-VIRTUAL MEMBER TO ITS REAL VTABLE SLOT IS FREE WHEN EVERY CALL SITE
  DISPATCHES ON A CONCRETE OBJECT.** CLAUDE.md's standing warning is that cl 5.0 does not
  devirtualize, so spelling a directly-called member `virtual` turns its call sites into vtable
  dispatches (that is why `WorldActionCursor::TryHandleClickMaybe` is deliberately non-virtual).
  The warning is about calls through a POINTER or REFERENCE. A call on a concrete object —
  `g_worldActionCursor.AdvanceAnimFrameMaybe()` — is statically dispatched *by the standard*, so
  the promotion emits the identical direct call. 0x459da0 (1010 B, slot 10) went virtual with a
  byte-identical repo, its caller 0x45c3c0 still EXACT. ⇒ Before believing a "must stay
  non-virtual" note, check whether the call sites are objects or pointers; and prefer a MATCHING
  caller as the canary, since it fails loudly.

- **#138 — THE SLOT-19 RETYPE ON `src/WidgetBase.h` IS REFUTED HARDER, NOT SOFTER, BY DELETING THE
  TU-LOCAL STRUCTS IT SPAWNED.** v506 priced retyping the base's `virtual void *_v19()` placeholder
  to its honest shape at −1094 B and worked around it with two duplicate `WidgetSlot19VtblProbe`
  structs plus a view struct. The v486/v490 lever theory says a parked price only covers the knobs
  tried, and deleting TU-local types is a known parity currency — so the natural retry is to land
  the retype and delete all three in the same commit. **Measured (v545): −1862 B**, i.e. the
  deletions cost a further ~768 B rather than offsetting anything (victims: 0x457ce0 −951,
  0x405c40 −407, RoadVehicleActor −504, PlacedObjRegistryMaybe −152, Obj0x477798Family +152).
  Idiom debt does drop 14 → 10. ⇒ Currency is not fungible across dials: `_v15` in
  `src/Obj0x477798Family.h` was already known not to substitute for a `src/WidgetBase.h`
  declaration (v537), and TU-local struct deletions do not either. The slot-19 model finding
  itself is free and is now recorded — see the note at the declaration.

- **#139 — A `||` BETWEEN TWO `char`-RETURNING CALLS MATERIALISES A FULL-WIDTH `mov eax,1` /
  `xor eax,eax`, SO THOSE CONSTANTS ARE **NOT** EVIDENCE OF AN `int` RETURN.** `||` yields `int` by
  language rule; truncating that to the function's `char` return is free (the value is already
  correct in `al`) and cl 5.0 never re-narrows it. The narrow `mov al,1` / `xor al,al` form comes
  from a NESTED-IF body, where each `return` is its own `char` constant. v446 read 0x459d60's
  `mov eax,0x1` as proof of an `int` return and priced `char` at DIFF(15)/59 B — but that
  experiment had only ever paired `char` with the nested-if shape, so it refuted the SHAPE and the
  return type took the blame. Written `return A || B;` the function is EXACT at 64 B with `char`.
  ⇒ When a return-type hypothesis rests on constant WIDTH, vary the control-flow shape before
  believing it; the two are confounded. This one mattered structurally: C++ forbids an override
  differing only in return type, so the bogus `int` was the sole reason `vtable_audit.py` could
  not seat `WorldActionCursor`'s slot 2 — its last hard MISMATCH.

- **#140 — `??_G` INLINES THE DESTRUCTOR BODY IFF THE DESTRUCTOR IS INLINE, AND THAT ONE BIT IS
  DECIDED BY EVIDENCE OUTSIDE THE THUNK.** A compiler-generated scalar deleting destructor comes in
  two shapes: `call ??1Class` (dtor defined out-of-line) or the dtor's body folded in — for a
  do-nothing dtor, a bare `mov [esi],&vtbl` (dtor defined inline). Matching the thunk is therefore
  never a thunk-local decision; it is a question about the whole program, and the image answers it:
  **if any out-of-line `??1` body exists and is called, the dtor is out-of-line.** Both cases
  landed in v546. `CarNetStateAlt` — no `??1` anywhere in `.text` (searched for
  `mov [ecx],0x478268; ret`; absent, while its twin's `mov [ecx],0x478264; ret` is right there at
  file offset 0x41e00) ⇒ inline dtor, `??_G` at 0x442ea0 **EXACT +32 B, zero collateral**.
  `CarNetState` — `??1` at 0x442a00 with six callers plus three EH unwind funclets ⇒ out-of-line,
  and forcing the inline form to win the 32 B costs **−177 B** (cl then inlines the same body at
  the member-destruction site in `CarNetObj::~CarNetObj`, killing its 206 B exact and dissolving
  the `xor ebx,ebx` zero-register the original spends on four further operands). ⇒ Search the image
  for the `??1` stub's byte pattern BEFORE choosing; and note the corollary that
  `find_unclaimed_comdats.py --thunks` only recognises the 30-byte `call` shape, so every
  inlined-body `??_G` is invisible to it and must be found by hand.

- **#141 — A DECLARED-ONLY PLACEHOLDER VIRTUAL IN SLOT 0 IS ALMOST ALWAYS THE `??_G` YOU HAVEN'T
  IDENTIFIED YET.** `TimeOfDayMaybe` carried a fabricated `virtual void Method0()` as its slot-0
  placeholder with a NON-virtual `~TimeOfDayMaybe()` beside it. The dword at 0x477840 is 0x412640,
  and 0x412640 is the scalar deleting destructor: the dtor IS the class's only virtual, and
  `Method0` was an invented second name for a slot that already had an owner — the exact failure
  CLAUDE.md's vtable-slot rule describes, reached from the opposite direction (the invented name
  came first). The fix is byte-neutral and layout-neutral (MSVC puts the vptr at offset 0 whether
  the first virtual is a placeholder or the destructor). ⇒ Whenever a class's slot 0 holds a
  placeholder, dump the address and check the `??_G` shape (`test al,1` / `call operator delete`
  / `ret 4`) before inventing anything; and treat a NON-virtual dtor sitting next to a
  placeholder slot-0 virtual as a contradiction on its face.

- **#142 — PROMOTING A FREE `__fastcall` CTOR ESCAPE HATCH TO A REAL MEMBER CTOR IS WHAT MAKES cl
  EMIT THE CLASS'S VFTABLE — AND THEREFORE ITS `??_G`.** A `??_G` COMDAT is generated with the
  vftable, and the vftable is emitted in whichever TU emits a constructor or destructor body for
  the class. So a class whose ctor is transcribed as `void __fastcall Foo_InitFieldsMaybe(Foo *)`
  storing the table by hand (`*(void **)p = g_vtable0xNNNNNN;`) can never claim its own `??_G`,
  however correct the bytes are — nothing in that TU emits a vftable. Promote the ctor to
  `Foo::Foo()`, delete the `extern void *g_vtable0xNNNNNN[]`, and the vptr store becomes the
  compiler's; v548's `WorldBoardPartial` claimed 0x454db0 EXACT at 32 B on the first compile this
  way AND kept the ctor's own 190-byte EXACT. ⚠ **Price it first**: this is a TWO-declaration
  change on the shared header (the ctor plus the `virtual ~Foo() {}` that the `??_G` requires),
  and on a wide header that is often unpayable — the same v548 promotion cost `src/WorldBoardMaybe.h`
  **−1311 B** across six consumer TUs at its best reachable rung. Also check whether the class's
  separate out-of-line `??1` is really yours: for a STATIC object it belongs to the TU that defines
  the object (its atexit thunk `mov ecx,<obj>; jmp <??1>` is the only caller), not to the TU that
  defines the ctor.

- **#143 — A DUMMY DECLARATION IS *NOT* INTERCHANGEABLE WITH A REAL ONE ON A DECLARATION-COUNT
  DIAL.** `src/MailWnd.h`'s note that "a dummy `char AaaZzz();` does it as surely as a real one"
  is TRUE for `src/AppWindow.h` and FALSE for `src/WorldBoardMaybe.h`. Measured in v548: adding one
  real `TilePlacedObj *GetTopPlaneBSlotMaybe(short, short, short *)` moves three consumer TUs that
  one `void ProbeDecl00();` leaves alone, and the throwaway sweep needs N≈3–4 to reach the state
  the single real declaration reaches. So the counter weighs something richer than "one
  declaration" — plausibly the type/symbol entries a signature drags in. ⇒ Sweep a dial with the
  REAL declarations you intend to keep; a probe sweep with dummies gives you the SHAPE of the curve
  but not the rung you are standing on.

- **#144 — MAKING A SHARED STRUCT POLYMORPHIC IS, BY ITSELF, FREE; THE DECLARATION IT ARRIVES WITH
  IS WHAT COSTS.** The obvious suspicion when adding `virtual ~Foo() {}` to a 40-consumer struct is
  that the consumers pay for the type becoming polymorphic. Refuted in v548 with a control: adding
  ONE ordinary NON-virtual member declaration to the untouched baseline header reproduced the
  entire −1160 B core loss exactly (three TUs, identical per-TU figures). ⇒ When a virtual-dtor
  promotion is expensive, do not go looking for a polymorphism explanation — price it purely as
  declaration count, and sweep the dial.

- **#145 — cl 11's BRANCHLESS BYTE-SELECT `cmp al,K / sbb al,al / and eax,ecx` COMES FROM
  REASSIGNING THE PARAMETER, NOT FROM A FRESH BYTE LOCAL.** `return x < K ? x : 0` over a
  freshly-declared `unsigned char` local compiles to a BRANCH (`jae` / `xor al,al`); the same
  expression over the reassigned PARAMETER (`kindId = (unsigned char)((int)kindId >> 10);
  return kindId < 0x10 ? kindId : 0;`) takes the branchless mask instead. Measured over eight
  spellings on 0x446030 — the explicit mask forms (`x & -(x < K)`, `x * (x < K)`) do NOT get there
  either: they emit a redundant `neg` pair or `neg`+`imul`. Related to #139: the leftover junk in
  the upper three bytes of EAX after the `and` is the tell for a `char`/`unsigned char` return.

- **#146 — UNIFYING A "VTABLE SLOT MODELED AS AN ORDINARY MEMBER" IS FREE WHENEVER EVERY CALL SITE
  IS CLASS-QUALIFIED OR ON AN OBJECT.** CLAUDE.md's "a vtable slot and a named method at the same
  address are ONE function" hazard is normally assumed expensive to fix, because a non-virtual
  declaration emits `call <addr>` and a virtual one emits `call [reg+off]`. That is only true when
  the call site reaches the object **through a pointer or a reference**. C++ resolves a virtual
  call statically when the site is either a class-qualified call (`Base::Method(x, y)`, the usual
  shape inside a derived override) or a call on an OBJECT (`g_singleton.Method(x, y)`) — and this
  codebase is full of both, because most of these widgets are file-scope singletons. Six such
  functions were unified in v549 (slot 4 `TryInvokeCallbackA` ×4, slot 17 `HitTestNodeSecondary`
  ×2) and the repo-wide `progress.py` was byte-identical every time: 172014 B / 828 funcs, no
  per-file row moved. Two of the six carried an in-source note explicitly forbidding the edge on
  the "it would become an indirect dispatch" grounds, and one of those (`src/WorldActionCursor.h`'s
  slot 4) contradicted a correct note three declarations below it in the same class. ⇒ Before
  paying for one of these, GREP THE CALL SITES: `virtual` costs nothing at a qualified or
  object call, and it costs no DECLARATION either — it is a rename in place, one declaration
  replaced by one, not an addition to the dial. The real blockers are signature mismatches (a leaf
  whose body needs the DERIVED parameter type cannot take the base's) and leaves whose body lives
  on a TU-local view, where promoting it is a genuine addition to a shared header.

- **#147 — A CHAINED ASSIGNMENT EMITS ITS STORES IN DESCENDING SOURCE ORDER, AND A `memset`
  SCHEDULES ITS `rep stos` SETUP DIFFERENTLY FROM AN EQUIVALENT `for` LOOP.** Two shapes that are
  invisible in a decompile and each worth a few bytes, both pinned on `UIResources::UIResources`
  (0x445f70, v549, 26/26 instructions either way):
  * `a = b = c = d = NULL;` associates right-to-left, so the stores come out `d, c, b, a` —
    DESCENDING by field offset. Four separate statements emit them ascending. On a run of same-
    valued field clears that is the entire difference (byte_diff=4 on four `mov [esi+off],eax`).
    ⇒ A descending run of identical stores to consecutive fields is the tell for a chain.
  * A zero-fill written as `for (i = 0; i < N; i++) a[i] = 0;` and as `memset(a, 0, sizeof a)` both
    become `rep stosd`, but the loop form orders the setup `lea edi / mov ecx / xor eax` where
    `memset` orders it `mov ecx / xor eax / lea edi` — a 12-byte rotation with no register
    difference (align=12). ⇒ On a `rep stos` whose only residual is a rotation of its three setup
    instructions, flip the source between the loop and the `memset`; do not go hunting registers.

- **#148 — A DESTRUCTOR DECLARATION IS A HEAVIER DIAL STEP THAN AN ORDINARY ONE ON THE SAME
  HEADER, ROUGHLY 2×.** Refines #143/#144, which price a dial in units of "one declaration".
  Measured in v549 on `src/DSoundChannel.h` over a 7-TU probe set: one `void ProbeDecl();` costs
  −876 B, while one `~RFIndex();` on the same line of the same struct costs −1581 B. Both are one
  declaration; the destructor additionally makes the struct non-trivially-destructible, which
  propagates to every type that embeds it (here `UIResources`, embedded by value at +0x18) and to
  the `/GX` unwind state of every function holding one. The dial sweep over that header
  (dtor × 0–4 dummy declarations) found NO rung that recovers it — best was −839 — so the baseline
  is the maximum. ⇒ When a byte-match needs a member destructor call, price the DECLARATION as
  roughly two ordinary ones before starting, and expect the dummy-declaration sweep to under-read
  the rung you actually need (which is #143 again, in a sharper form).

- **#149 — A FREE `__fastcall(T *p)` AND THE `__thiscall` MEMBER IT STANDS IN FOR ARE NOT
  INTERCHANGEABLE FOR THE SCHEDULER, EVEN THOUGH BOTH PUT `this` IN `ecx`.** This repo's standard
  escape hatch for reaching an untranscribed method without growing a shared header is to spell it
  as a free `void __fastcall Class_Method(Class *p)`; the call site's bytes are identical, so the
  form has always been treated as free. It is not free INSIDE the body. `SelectedObjWidgetMaybe::
  ClearOwned` (0x42cdd0) sat PARKED as an EFFECTIVE match from v505 to v550 at 24/24 instructions,
  its entire residual one /Og scheduling swap — `mov ecx,esi` hoisted ahead of three pushes rather
  than placed after them. Rewriting the free form as the real member (same statements, `pWidget->`
  dropped) matched EXACTLY on the first compile. The explicit pointer parameter is an ordinary
  local as far as /Og is concerned and competes for the same registers; `this` is not. ⇒ When a
  function transcribed under the `__fastcall` escape hatch is EFFECTIVE-parked on a register or
  scheduling tie-break, re-spell it as a real member BEFORE writing the autopsy. Cost is one
  declaration on the class's header (price it, #143), and the call site is byte-free whenever it
  is class-qualified or on an object (#146).

- **#150 — A HEADER'S DECLARATION DIAL CAN SATURATE RATHER THAN ACCUMULATE: THE FIRST STEP COSTS
  EVERYTHING AND THE NEXT THREE COST NOTHING MORE.** #143/#144 model a dial in units of "one
  declaration", which invites paying per declaration and folding one member at a time. Measured in
  v550 on `src/WorldActionCursor.h`: one added declaration (`ClearOwned`) was FREE, and then 1, 2
  AND 3 further declarations each cost exactly the same flat 60 B — `TilePlacedObj::
  GetFrontRowTilePosMaybe` (0x458310) losing its single 60-byte EXACT, the same
  `sub edx,eax`/`sub eax,edx` coin flip this header family has paid since v442. There is only one
  function on the far side of the threshold, so once it has flipped there is nothing left to lose.
  ⇒ Before folding a class's members one at a time, measure the SECOND step as well as the first.
  If the cost is flat, take EVERYTHING at once — correctness per byte spent is maximised by
  crossing the threshold exactly once. And treat "N declarations cost N × the unit" as a
  hypothesis to test, not the model. ⚠ Corollary for stale header notes: v506 read this same dial
  as a one-declaration BUDGET and v533 read it as GONE; both were true when measured. Re-price.

- **#151 — #149 IS A LEVER TO TRY, NOT A DIAGNOSIS; AND #150's "SATURATED THRESHOLD" IS ONE STEP
  OF A STAIRCASE.** Both v551 lessons were re-tested in v552 and both need bounding.
  (a) #149 held again and closed a second parked residual: `SelectedObjWidgetMaybe::
  BlitIconToolboxClippedMaybe` (0x42d280) had been EFFECTIVE-parked since v505 at DIFF(10) /
  86-of-86 instructions on one /Og scheduling swap, and re-spelling the free `__fastcall` as the
  real member matched it EXACT on the first compile — **+278 B**, no body change. But the SAME
  move is INERT on `WorldBoardMaybe_ResetAllTilesMaybe` (0x454fe0): still DIFF(2), byte-identical
  residual. So the escape-hatch form is a plausible CAUSE of a scheduling residual worth one
  probe, not the explanation for all of them — and the probe is not free, since the member
  declaration it needs cost `src/WorldBoardMaybe.h`'s 0x457ce0 its full 951 B. Try it early
  (it is one compile), but price the declaration before keeping it.
  (b) v550 measured `src/WorldActionCursor.h` at 1, 2 and 3 declarations all costing a flat 60 B
  and concluded THRESHOLD-then-saturated, "nothing left to lose". v552 added two more and paid a
  further **143 B** (`TilePlacedObj::SpawnSeqRecordEffectMaybe`, 0x4588b0, its eighth recorded
  flip). Within the new step the flatness held exactly — ONE declaration and TWO cost the
  identical 143 B, repo total 172151 B either way. ⇒ The dial is a STAIRCASE: flat within a step,
  with another step always beyond it. #150's operational advice survives (once you decide to
  enter a step, take EVERY declaration you want, because the marginal ones are free) but its
  "saturated / nothing left to lose" conclusion does not generalise past the probes that produced
  it. Never read a flat measurement as an exhausted dial.

- **#152 — A MODEL FIX THAT IS BYTE-NEUTRAL BY CONSTRUCTION CAN BE ARGUED BEFORE IT IS MEASURED,
  AND THE ARGUMENT IS WHAT MAKES IT SAFE TO MAKE.** Declaring `WidgetPickerObj0x477cc8`'s 0x4287b0
  as the real vtable slot-17 override (it had been the ordinary non-virtual member
  `TestMenuCommand(UiIconListItem *, int, int)`, so our emitted vtable inherited the base's slot)
  changed the name AND the parameter type to the base's `HitTestNodeSecondary(MenuNodeObj0x477568
  *, int, int)` — C++ has no covariant parameters, so the original's slot necessarily took the
  base pointer and downcast in the body. Three things make it byte-neutral in advance, and each
  is worth checking before attempting this class of fix: the derived type sits at offset 0 of a
  single-inheritance base, so the downcast emits nothing; the slot is the function's ONLY entry
  point (`grep` found no call sites), so no direct call turns into a `call [reg+off]`; and the
  vtable itself is `.rdata` full of masked relocations, which the byte compare never sees. A full
  `progress.py` confirmed output IDENTICAL to the previous run. ⇒ Prefer vtable-model fixes whose
  neutrality follows from offset-0 inheritance + zero call sites; they buy real model correctness
  at provably zero byte risk. Where the class has real call sites, expect to pay for the
  virtual-dispatch conversion instead and price it first.

- **#153 — THE STRONGEST NAME FOR A FIELD IS THE ONE THE SHIPPED BINARY ALREADY SPELLS: HARVEST
  THE PARSER'S TOKEN TABLE BEFORE GUESSING FROM BEHAVIOUR.** This project recovers names from
  behaviour, which is why so many carry `Maybe` — but a data-driven engine contains the authors'
  own vocabulary as string literals, and a keyword parser binds each token to exactly one field.
  `Obj0x4779e0::ParseTokenField` (0x41e9f0) is a 20-arm `_stricmp` chain; reading it named 18
  fields at once in v553, and the token's OPERAND ORDER pins which field is which where the token
  alone would not (`"physical_occupancy"` reads `xSteps, ySteps, layerCount` before its grid;
  `"bitmap_occupancy"` reads `cols, rows` before its own; `"FreeToRoam"` reads a RECT in
  left/top/right/bottom order, which is what retired four `lFreeToRoamA/B/C/DMaybe` letters).
  ⇒ Before grinding a struct's `Maybe` fields one behavioural inference at a time, find whether
  a parser writes them and read its token table; `grep -c '_stricmp\|_strnicmp' src/*.cpp` ranks
  the candidates. Corollaries:
  (a) **A token names a field even when nothing reads it back.** `Unk0x163Maybe` became
  `bButtonVisible` on the strength of the `"ButtonVisible"` token alone. The name is certain; the
  USE is still unknown. Those are different claims and the comment should say which is which.
  (b) **Fields the token table does NOT name must keep their hedge.** `"button"` reads three
  unnamed operands, so `field_0x2eMaybe`/`field_0x30Maybe` stay exactly as they were. Do not
  strip a `Maybe` by analogy with the promoted neighbours in the same commit.
  (c) **A second parser reading the same layout is free corroboration.** The `"shifts"` sscanf
  fills two `TimeOfDayMaybe` embeds as `hour, min, hour, min`, independently confirming an
  m_8=hour/m_4=minute mapping that had rested on one unrelated sscanf's argument order since
  v494 — a hedge that header explicitly said it was keeping *because* of that single witness.
  (d) **A contradiction between a field's writer and one of its readers is resolved in favour of
  the writer.** `src/CursorDesc.h` described +0x16b as "the per-kind cap" — read backwards from
  its ONE consumer — against the `"bitmap_occupancy"` token that writes it and ~20 geometric
  reads. The odd consumer is still reproduced verbatim; it just stops being evidence about
  identity. Surprising is not wrong, and an engine-bugs.md row needs more than surprise.

- **#154 — A GHIDRA CLASS RENAME IS TWO RENAMES, AND HALF-APPLYING IT SILENTLY DEGRADES `this`
  FOR EVERY MEMBER — INCLUDING WHEN THE SIGNATURE STILL LOOKS TYPED.** CLAUDE.md's rule (namespace
  name MUST equal a same-named Structure) has no enforcement, and `lint_ghidra_sync.py` checks
  FUNCTION and GLOBAL names only, never TYPE names — so a rename applied to the namespace but not
  the struct survives indefinitely with every lint green. Two instances found in v553, both dating
  to the same v482 rename sweep, and they fail differently:
  (i) **no struct at all** — `this` degrades to `void*`, which is at least visible;
  (ii) **a 1-byte PlaceHolder Class Structure**, which Ghidra auto-creates under the class's name.
  `get_function_by_address` then prints a perfectly healthy-looking `CarKindDesc *this` while the
  real 1964-byte struct sits unreferenced under its old address-based name and all 0x7ac bytes of
  field access decompile as raw offset math. Shape (ii) is invisible to eyeballing signatures.
  Mechanics worth knowing: the `this` parameter is an AUTO parameter — `setDataType` on it throws
  `InvalidInputException: Auto-parameter may not be modified`. You do not set it; you make the
  namespace and struct names agree and it resolves itself. Remove any 1-byte placeholder FIRST,
  or it blocks reusing the name (docs/GHIDRA_RECIPES.md). ⇒ `tools/lint_ghidra_types.py` now
  checks both shapes; run it after any class rename. Its first run found five more, four of them
  real derived leaves that add no fields (build those as `base` at offset 0) and one that was not
  an image class at all — a TU-local `…Partial` VIEW name from `src/` that had leaked into the DB
  as a class. Views are a source-side device; they should never own a Ghidra namespace.

- **#155 — TWO TUs CAN SPELL ONE SYMBOL DIFFERENTLY AND BOTH COMPILE, BOTH PASS EVERY LINT, AND
  BOTH BYTE-MATCH — AND ONE OF THEM IS CALLING NOTHING.** A file-local `extern` declaration only
  has to satisfy its own TU. If two TUs disagree about a callee's LINKAGE (`extern "C"` vs C++),
  its CALLING CONVENTION, or any PARAMETER/POINTER TYPE, they emit two different mangled symbols
  for one image address, and at most one can be right. Nothing in this repo could see it before
  v554: `verify.py`/`match.py` MASK relocations (the wrong call is byte-identical to the right
  one), we never link, `lint_ghidra_sync.py` checks each declaration independently against Ghidra,
  and **`lint_alias.py` compares NAMES — it is structurally blind to this, because every spelling
  here shares the same name.** ⇒ `tools/lint_desync.py` groups every `build/*.obj` symbol by the
  identifier its mangling encodes and reports any identifier with ≥2 spellings where one is never
  defined. Its first run found 28; 12 were fixed in v554. Rank by whether a DEFINITION exists: a
  defined spelling is ground truth and every dissenter is provably wrong, whereas with no
  definition yet (untranscribed callee) the majority spelling is only a strong convention.
  ⚠ **These fixes are NOT automatically free, and two of them paid the other way:**
  - `__stdcall` missing from `src/DSound.cpp`'s `ArrayDestructWithIteratorMaybe` (0x467280 ends
    `ret 0x10`; `src/GameNet.cpp` had it right) was **the entire residual** of `DSound_Teardown` —
    DIFF(106) → MATCH, +197 B. On a callee WITH arguments, a wrong convention is real emitted
    code, not just a wrong symbol; on a zero-argument callee it is symbol-only.
  - retyping five window singletons in `src/PopupWndBase.cpp` from `WindowBase *` to their
    concrete classes needed five new includes, which moved `CommitScreenUpdate` from
    DIFF(62)/align 24 to DIFF(2)/align 0 via the #149/#150 declaration dial — see #156.
  - but fixing `DDraw_CreateSurfaceFromFile` in `src/LocoBitmap.cpp` COSTS 124 B (`LocoBitmap::Fill`
    EXACT → DIFF(2)). Bisect one declaration at a time when a batch regresses; the other three
    siblings in that same block were free. Under `extern "C"` the symbol is C-mangled regardless
    of return type, so there is no middle form — it is the bytes or the right symbol. Park what
    you cannot afford with the measurement in the note, per the standing "keep the byte-faithful
    form" rule.
  Sequencing note for the `extern "C"` variety: a linkage specification is **illegal at block
  scope** (C2598), so it cannot be added to a function-local `extern`; hoisting it to file scope
  compiles but merely converts the defect into a class I idiom finding. The fix that is both
  correct and free is the one class I always wanted — **include the shared header that already
  declares it**.

- **#156 — AN `align`-HEAVY RESIDUAL CAN BE HIDING A TWO-BYTE SOURCE-ORDER FACT, AND A PARKED
  FUNCTION'S WRITTEN DIAGNOSIS IS NOT EVIDENCE.** `PopupWndBase::CommitScreenUpdate` (0x414c20,
  714 B) sat EFFECTIVE at DIFF(62)/align 24 for many sessions behind a detailed in-source autopsy
  AND a `docs/PARKED.md` row, both asserting the residual was "scratch-register naming plus one
  scheduling slot" in its duplicated SetRect/Blt present blocks. That was simply wrong. An
  unrelated declaration-dial move (#155) collapsed it to DIFF(2)/align 0 — a full 254/254
  instruction pairing — and the two survivors were then legible as `mov [esp+d],ebx` twice with
  the displacements SWAPPED: **the original zeroes `rectBltDest.top` BEFORE `.left`**, and VC5
  emits those two same-valued stores in source order. Swapping the two source lines closed it
  outright. Generalisations: (a) once `align` and `reg_pen` reach 0, a small `byte_diff` is a
  FACT about the source, not noise — get the exact differing offsets (mask+trim the COMDAT, diff
  against the image bytes) rather than reading the dump prose; (b) **re-derive a park after any
  dial move** — the recorded diagnosis describes a residual that no longer exists; (c) a swapped
  pair of same-valued stores to adjacent slots is a field-write ORDER question, whereas a swapped
  `mov`/`imul` operand pair is usually register allocation and NOT source-reachable
  (`LocoBitmap::Fill` resisted `height * width`, `width * height`, a cached local, and an added
  declaration alike).

- **#157 — THE SMOKE LINK IS A LINT, AND IT CHECKS THINGS NO OTHER TOOL HERE CAN.** `verify.py`/
  `match.py` MASK relocations and compare one COMDAT at a time, so nothing in the byte-match
  pipeline ever asks whether the whole image is coherent. `tools/link_check.sh` does, and four
  separate defects had been sitting in it invisibly, each producing a clean `LINK OK`:
  (a) **no `.rsrc` at all** — resources are data, `src/` can never produce them (no `.rc`, and
  VC5 ships no `RC.EXE`), and their absence is not cosmetic: `TileKind_GetOrLoadDescriptor`
  (0x446ea0) turns every tile id into a `LoadStringA` against the exe's own `RT_STRING` table, so
  an empty string table means the game loads no art whatsoever. Fix: walk the original's PE
  resource directory and re-emit it as a `.RES` (`tools/extract_res.py`), then `CVTRES` it in —
  the VC5 linker takes the resulting obj directly, and `CVTRES.EXE` lives in `vc50/BIN`, not
  SP3's. (b) **every stubbed DATA global aliased ONE shared zero dword** — `g_worldBoard` is
  337172 bytes in the original and it, `g_UIResources` and 157 others all named the same 4. Fix:
  lay the stubs out inside a MIRROR of the original's `0x477000..0x501000` span at their true
  offsets, which buys real sizes AND real aliasing for free (`g_RFIndex` genuinely IS
  `g_UIResources+0x18`; the four `NetSessionEventQueue*` view names genuinely ARE 0x4a9990).
  (c) **`?`-prefixed mangled DATA read as code** — the MSVC mangling says which it is in the
  character after the qualified name (`@@[0-4]` is data, function forms are letters), and getting
  it wrong put 128 writable globals in `.text` aliased to a `ret`. (d) **a bare `ret` for every
  code stub**, which is correct only for `__cdecl` — a callee-cleanup stub must pop its own args,
  and `_name@N` says N right in the name.
- **#158 — A `DEFINE_GUID` WITHOUT `INITGUID` IS A DECLARATION, SO AN UNRESOLVED IID BECOMES A
  ZEROED STUB THAT SILENTLY DISABLES A WHOLE SUBSYSTEM.** `IID_IDirectDraw4` came out of the link
  undefined and was zero-filled like any other data stub. But `Ddraw_Init`'s first act after
  `DirectDrawCreate` is `QueryInterface(IID_IDirectDraw4, &g_pDDraw2)`, and an all-zero GUID
  returns `E_NOINTERFACE` — so `Ddraw_Init` returned 0 and no surface could ever be created. The
  stub is indistinguishable from a real symbol to every tool in the repo; only reading the
  generated stub list and asking "what does a zero mean HERE" finds it. Same for
  `g_iidDirectPlayLobby3A`. Define both from the values already read out of the image.
- **#159 — `/OPT:REF` PRUNES BY REACHABILITY, AND IN A HEAVILY-STUBBED SMOKE LINK REACHABILITY IS
  A LIE.** The default `/OPT:REF` discarded `Ddraw_Init` — the only transcribed body that
  *imports* from DDRAW.dll, every other DirectDraw use being a COM vtable call that needs no
  import — because every path to it still runs through a generated stub. The exe then shipped
  with no DDRAW.dll import at all, and the resulting LNK4089 had been explained away in a comment
  as expected noise. `/OPT:NOREF` keeps every COMDAT, restores the import (13/13 DLLs now match
  the original exactly) and makes the section table comparable. Treat a "harmless" linker warning
  in a scaffolding script as an unread bug report.
- **#160 — A CASE-INSENSITIVE FILESYSTEM MAKES `/I src` A TRAP.** Adding `/I src` to a build so
  `src/` headers resolve unqualified also makes the SDK's `#include <ddraw.h>` resolve to this
  repo's own `src/Ddraw.h`, and every TU that touches DirectDraw fails with a cascade of
  syntax errors inside what looks like the system header. Put only the NEW include dir on the
  path and let quoted includes resolve relative to the including file.
- **#161 — A TU-LOCAL VIEW STRUCT IS INVISIBLE TO THE BYTE-MATCH AND FATAL TO THE LINK: THE
  BODY IS PRESENT, EXACT, AND UNREACHABLE.** The repo's standard trick for dodging a shared
  header's declaration-count dial is to declare a method on a methods-only `struct FooView0xNNN
  : public Foo` inside one `.cpp` and define it there. That is *correct* only while nothing
  outside the TU calls it — and nothing here can tell you when that stops being true.
  `verify.py`/`match.py` compare one COMDAT at a time with relocations masked, so the body
  byte-matches its address perfectly; `lint_ghidra_sync` accepts the view's name because the
  ADDRESS is right; the compiler is happy because every TU's own declarations are well-formed.
  Meanwhile the symbol the OTHER TU calls — `?Init@UIResources@@QAEEXZ` — is defined nowhere,
  and a smoke link quietly resolves it to a generated do-nothing stub. Found v557 by tracing
  the stubs at RUNTIME: `UIResources::Init`'s stub returned 0, `AppWindow::
  InitSubsystemsAndWindows` read that as "resources failed to come up", and the game put up its
  fatal MessageBox — so the RF archive, the fonts, the WAV bank and all 15361 tile-kind
  descriptors were never loaded, while a byte-EXACT 752-byte `Init` sat in `src/UIResources.cpp`
  doing nothing. **`tools/lint_alias.py` is the static oracle for this and it was already
  reporting it** (8 findings; 6 remain). Treat every `lint_alias` finding as a live runtime
  defect, not hygiene. The fix is the CLAUDE.md rule — one canonical definition, declared on
  the real class — and it costs whatever that header's dial costs.
- **#162 — THAT DIAL IS A THRESHOLD, NOT A PER-DECLARATION PRICE; MEASURE THE SET YOU NEED, NOT
  ONE DECLARATION.** Retiring both of `src/UIResources.h`'s TU-local views took SIX new method
  declarations. The first three (`Init`, `TileKind_CreateDescriptor`,
  `TileKind_LoadDescriptorRange`) cost **−656 B / −2 funcs** — `AnimDescRefObj0x477488::
  AdvanceAnimFrameMaybe` (0x405c40, 407 B) and `TutorialWnd::RestorePresenterBackdrop`
  (0x452b00, 249 B), neither function itself changed. The next three
  (`Locale_DetectLanguage`, `Shutdown`, `ReleaseAllCachedResources`), added to the same header
  in the same session, cost **exactly zero**. So the per-header notes that price a dial at "N
  declarations cost X" describe a threshold that has already been crossed, not a slope: once
  paid, further declarations in that header can be free. Corollary for planning: a burn-down
  ledger that prices items INDIVIDUALLY and sums them (v555's, which put this class at
  ~−4500 B) systematically OVERSTATES the cost of doing them together — the real bill for the
  whole UIResources unification was 656 B. Never abandon a batch because the sum of its parts
  looks unaffordable; price the batch.
- **#163 — GENERATED LINK STUBS MUST POP THEIR ARGUMENTS, AND CAN REPORT THEMSELVES.** A stub
  that is one bare `ret` is correct only for `__cdecl`. Under `__thiscall`/`__stdcall`/
  `__fastcall` the callee cleans up, so a bare-`ret` stub leaves the caller's stack unbalanced
  by the whole argument list — and 142 of this image's 245 code stubs pop a nonzero amount, 55
  of them a virtual `WndProc(HWND, UINT, WPARAM, LPARAM)` (`ret 16`), i.e. every window message
  reaching a stubbed handler corrupted the stack. `link/gen_stubs.py` now recovers the count by
  parsing the MSVC mangled parameter list (`mangled_pop`); the ORIGINAL IMAGE is the oracle —
  for every stub Ghidra can resolve to an address, the computed pop equals the `ret imm16` the
  real function executes (8/8, including 0x42c330's `ret 0x28` = 40, which exercises a
  back-reference). ⚠ Two parsing facts worth keeping: the `@@`-following char encodes access,
  and only NON-STATIC MEMBERS carry a cv char before the calling-convention letter (`SAE…` is
  static+`__cdecl`, not static+cv+`E`); and back-references (`0`–`9`) index only types encoded
  in MORE than one character, so single-char primitives are skipped when numbering. **The bigger
  lesson is the second half:** each stub now also calls `Stub_Report` with its own name and its
  caller's return address, writing `link/stub_calls.log` straight through kernel32. That one
  change converted the burn-down from a static guess ("245 stubs over 80 classes, start with
  the small shared ones") into a measured, ordered worklist — and immediately showed the real
  distribution is nothing like the static one: 673 of 680 calls were FIVE ctor shims, and the
  headline blocker was an aliasing defect rather than any missing body.
- **#164 — A STUBBED GLOBAL *OBJECT* NEVER RUNS ITS CONSTRUCTOR, AND THAT CAN MATTER MORE THAN
  ANY MISSING FUNCTION BODY.** `link/gen_stubs.py` gives every global no transcribed TU defines a
  zeroed slot in the `.bss` mirror. For plain data that is right. For an object with a
  CONSTRUCTOR it silently deletes the constructor: nothing in the build, and no lint, says the
  initialization that should have happened did not. `g_UIResources` is the case that cost a
  session: `UIResources::UIResources` (0x445f70) is the ONLY code anywhere that identity-fills
  `m_pKindSlotPtrsMaybe[i] = &m_apKindDescriptors[i]`, and
  `UIResources::TileKind_GetOrLoadDescriptor` reaches every descriptor THROUGH that redirect
  table, returning NULL outright when the slot is NULL. Zeroed, the registry answered NULL to
  every lookup no matter how correctly the descriptors themselves had been built — so fixing the
  descriptor ctors (v557b) moved the crash by exactly zero. Construct such a global IN PLACE
  (`link/init_globals.cpp` placement-news it) rather than defining it for real in `src/`: the
  mirror deliberately preserves the original's ALIASING, and `g_RFIndex` IS `g_UIResources+0x18`,
  which a separate real definition would silently orphan. **When a stubbed build misbehaves, ask
  which stubbed globals are objects before hunting for a missing body.**
- **#165 — UNDER WINE, THE EXIT CODE LIES ABOUT CRASHES; ONLY THE LOG TEXT IS EVIDENCE.** An
  unhandled fault starts `winedbg`, which holds the process open — so a crashing run sits there
  until the harness timeout and reports the SAME code as a healthy long-running one (124 for a
  `timeout`-style killer). A whole session's "it no longer crashes" reading was wrong for exactly
  this reason. Grep the captured output for `Unhandled page fault` / `Unhandled exception`; treat
  the exit code as a hint only. Two more facts for this loop: a SIGKILLed run leaves its window
  alive in the wineserver, and `LocoWinMain`'s `FindWindowA("LEGO LOCO")` then takes the
  "another instance is running" path (exit **1**) before reaching the game, so `pkill` the
  previous run AND the wineserver first; and CrossOver's wine is a release build, so
  `WINEDEBUG=+relay` produces NOTHING.
- **#166 — MAP A CRASH ADDRESS THROUGH OUR OWN LINK MAP, NEVER THROUGH GHIDRA.** The smoke/port
  builds are not byte-identical to the original — their `.text` is a different size (0x7b0d5 vs
  0x757e0) — so a faulting VA means a DIFFERENT function in `loco/Loco.exe` than it does in
  `build/Loco-port.exe`. Resolve it against `link/Loco-port.map` (sort the symbol rows by address,
  take the last one at or below the target, report `name +0x<delta>`). The `+0x<delta>` is the
  stable part: across two rebuilds that moved the same fault from 0x40c337 to 0x4126d7, it stayed
  `BuildToolCursorWnd::LoadCursorBitmapSet +0x27`. Adding `/debug /debugtype:cv /pdb:...` to the
  port link is cheap, embeds CodeView in the exe for winedbg, and costs nothing but size — but it
  MOVES every address, so re-map after enabling it.
- **#167 — A "MEASURED PRICE" ON A HEADER DECAYS; RE-MEASURE BEFORE PAYING IT, AND ESPECIALLY
  BEFORE DESIGNING AROUND IT.** `src/CursorDesc.h` carried a v331 bisect result — including
  `TimeOfDayMaybe.h` there rotates `DPlaySessionMgr.cpp` and breaks
  `SelectGridCellFromPointMaybe`'s EXACT — and ~200 sessions of architecture were built to route
  around it: the two +0x534/+0x548 "shifts" embeds stayed modeled as ten raw longs, which forced
  `Obj0x4779e0`'s ctor and dtor onto TU-local layout models (two full duplicate field blocks),
  which in turn forced the ctor shims that made the descriptor pipeline unreachable at link time.
  Re-measured in v557d: the price is ZERO, and typing the embeds was a net **+1222 B / +5 EXACT**
  across five TUs. The lesson is not "v331 was wrong" — it was right when taken. It is that a
  price measured against one header state is not a property of the header, so a note recording
  one is a hypothesis with an expiry date, not a constraint. Re-run the measurement whenever the
  workaround it justifies starts costing structure; the workaround is usually the expensive part.
- **#168 — THE DECLARATION DIAL IS A THRESHOLD YOU CROSS, NOT A TARIFF YOU PAY PER DECLARATION —
  SO PRICE THE SET AND KEEP GOING PAST A LOCAL LOSS.** Measured in one v557d sitting on
  `src/CursorDesc.h`: adding `Obj0x4779e0`'s ctor was FREE (and recovered `ParseEntryExitMaybe`,
  0x41f0c0, +489 B); adding its dtor LOST that same 489 B; adding four more parse-method
  declarations changed NOTHING; adding `BigObj`'s three won the 489 B back. A session that
  stopped after the dtor would have recorded "the dtor costs 489 B" and parked — a true statement
  about a state nobody should stay in. Corollary to CLAUDE.md's parity-bit note: when a
  structurally correct change lands in the red, the next correct change is as likely to clear it
  as to deepen it, so finish the SET before judging any member of it.
- **#169 — A DELIBERATELY MIS-DECLARED VTABLE SLOT IS FREE FOR THE BYTE-MATCH AND FATAL FOR A
  LINKED BUILD, BECAUSE `gen_stubs.py` BELIEVES THE MANGLED NAME.** `WindowBase` slot 0x28 holds
  a bare `__stdcall` free function taking `pWnd` as an explicit 5th STACK arg (0x426140:
  `ret 0x14` at all 32 exits, args from `[esp+0x20]`, ECX never read), which no C++ virtual member
  can spell — so the header keeps `virtual void *_v28();` as a dummy slot-holder. Correct for the
  match; in the port that dummy IS the vtable entry, and `?_v28@WindowBase@@UAEPAXXZ` mangles as
  "no arguments", so the generated stub is a bare `ret` against a call site that pushed 20 bytes
  expecting callee cleanup. Result: every routed message leaked 20 bytes of stack and none ever
  reached the router; 19 messages later, EIP=0. **Any declared-only virtual whose real slot has a
  different ARITY or CONVENTION is a live stack-corruption bug the moment the build links** —
  #163 fixed the "stubs pop nothing" half, and this is the half where the stub pops exactly what
  a deliberately-false declaration told it to. Fix under `#ifdef LOCO_PORT`; before calling the
  real target directly, confirm the slot has ONE possible value by counting occurrences of its
  dword in the original `.rdata` (0x00426140 appears 8 times, all slot 0x28 — no overrides).

- **#170 — A PORT NEEDS A FRAMEBUFFER DUMP BEFORE IT NEEDS ANY THEORY: "the engine drew nothing"
  and "the engine drew a frame nobody showed" are INDISTINGUISHABLE from outside the window.**
  v559 spent its first hours reasoning about descriptor loading, colour keys and dead frame timers
  to explain a blank game, on the shared premise that something upstream had failed. One
  `Port_DumpFrame` — the emulated primary written to a BMP — ended the question instantly: the
  engine had been rendering a pixel-perfect title screen the entire time, and every bug was
  downstream in the PRESENT. The dump is strictly better evidence than the window because it does
  not depend on the display path at all: it works headless, it survives a crash, and it cannot be
  lied to by a compositor. **Build the capture before the theory** — and pair it with a cheap
  always-on statistic (frame counter, checksum, non-black pixel count), because the counter alone
  answers "is anything presenting at all?", which is the question that actually discriminates. The
  general rule: when a symptom is "nothing appears", instrument the LAST stage that you can still
  read directly, and bisect outward from there rather than inward from the first suspect.

- **#171 — A PORT'S PRESENT MUST COVER EVERY PUMP AND EVERY WINDOW; THE ORIGINAL NEEDED NEITHER
  BECAUSE THE PRIMARY *WAS* THE SCREEN.** Two independent v559 bugs shared one root cause — an
  emulated primary makes explicit a step the original got for free. (a) LEGO LOCO has TWO message
  loops: a front-end pump (plain `GetMessage`/`Dispatch`) and an in-game pump that calls
  `FrameDriver_TickMaybe`, which is the ONLY caller of `Port_Present`. The menu therefore rendered
  into a buffer nobody ever blitted — measured as **0 presents in 70 s while the 28 ms timer fired
  600+ times**, which also cleanly exonerated the timer. (b) Once presents ran the frame was still
  invisible, because the game is not one window: the front end is its own top-level window
  (`FRONTWINDOWCLASS`) layered over the main one, so a correct frame was being drawn UNDERNEATH it.
  The symptom of (b) is diagnostic and worth memorising: **a window that shows only its real child
  controls (an EDIT box) on a blank background is a window you are painting behind, not one nobody
  painted.** `WindowFromPoint(centre)` + `GetForegroundWindow()` names the culprit in one line.
  Present to every visible top-level window the process owns, each showing its own portion.

- **#172 — WHEN ENGINE CODE MIXES SCREEN-SPACE AND CLIENT-SPACE READS, THE PORT MUST RESTORE THE
  INVARIANT, NOT PATCH THE CALL SITES.** `PopupWndBase`'s software cursor feeds a raw
  `GetCursorPos` — screen coordinates — straight into the primary as an offset, while
  `PlacementCursorMaybe` converts with `ScreenToClient(hwndOwner)`. Both are correct *simultaneously*
  only while the main window's client origin sits at screen (0,0), which a borderless `WS_POPUP` at
  the origin guaranteed on Windows in 1998 and which winemac silently breaks (it pushes the window
  below the menu bar; v559 measured y=78). "Fixing" the sites that looked wrong would have
  desynchronised the ones that were right. The correct move is to make the emulated primary indexed
  in SCREEN coordinates — map primary (x,y) onto screen (x,y) in the present, and re-anchor the
  window so nothing is cropped — which repairs every call site at once. **Generalisation: a 1998
  fullscreen game encodes "my client area is the screen" as an unstated invariant all over its
  coordinate maths; a windowed port either re-establishes that invariant or inherits a permanent
  audit of every coordinate expression in the codebase.**

- **#173 — IN A LINKED PORT, A DECLARED-ONLY VTABLE SLOT-HOLDER IS NOT AN INERT NO-OP: ITS STUB
  POPS THE WRONG NUMBER OF BYTES.** `link/gen_stubs.py` sizes each stub's `ret N` from the MANGLED
  NAME, which is the right oracle for an ordinary undefined function and a LIE for a placeholder.
  Both message routers in this codebase are `__stdcall` free functions occupying a vtable slot and
  taking the object as an explicit 5th stack arg (`WindowBase` slot 0x28 / 0x426140,
  `PopupWndBase` slot 0x24 / 0x4143e0, both `ret 0x14`, no ECX). A C++ virtual cannot occupy such
  a slot, so the header keeps `_v28`/`_v24` as declared-only dummies — and `?_v24@…@@UAEPAXXZ`
  says "no arguments", so the stub is a bare `ret` while the call site pushes 20 bytes and expects
  callee cleanup. Each routed message therefore leaked 20 bytes of stack AND never reached the
  router; after enough of them the return address is garbage and EIP goes to 0. **The tell is the
  delay** — a stack leak crashes on the Nth call, not the first, so it reads as "intermittent"
  and invites a hunt for a race that is not there. The hazard is exactly "a declared-only slot
  reached through a TYPED function-pointer view struct": a placeholder called as a normal
  zero-arg virtual pushes nothing and stays balanced, so `grep` for the view structs
  (`(__stdcall *)`/`(__thiscall *)` members), not for the placeholders.

- **#174 — WHEN YOU FIX A DEFECT THAT ARISES FROM A MODELING CONVENTION, SWEEP FOR ITS SIBLINGS
  IMMEDIATELY; CONVENTIONS PRODUCE DEFECTS IN SETS.** #173's stack-leak bomb was found and fixed
  for `WindowBase` in v557d and left standing in `PopupWndBase` until v560b, where it cost a
  second crash-hunt from scratch — the two are a declared pair, documented as such, in sibling
  files, with near-identical comments. Same shape as the `lint_alias.py` family: the "view struct"
  and "declared-only slot-holder" conventions are applied wherever the pattern occurs, so the
  first instance found is evidence about a POPULATION, not an incident. Cheapest possible sweep,
  and it is one grep. Compare `docs/PARKED.md`'s own habit of recording a defect class once and
  then listing every site.

- **#175 — A PRICED-AND-WITHHELD TOLL IS A MEASUREMENT WITH AN EXPIRY DATE; RE-MEASURE BEFORE
  BELIEVING IT, AND ESPECIALLY BEFORE SPENDING A SESSION ON THE LEVER MEANT TO UNLOCK IT.**
  `RectFlagObj0x477820`'s four bodies were withheld from v540 at a measured −256 B, because any of
  them cost `AdvanceAnimFrameMaybe` (0x405c40) its whole 407 B EXACT. Landed in v560: **+119 B /
  +3 funcs, three EXACT, and the per-file table moved in exactly ONE place.** The toll did not
  happen — 0x405c40 had already lost its EXACT to something else in the ~20 intervening sessions,
  so the toll being withheld against no longer existed to pay. Both measurements were correct when
  taken; what expired was the assumption that the victim was still alive. Re-running it cost ONE
  compile against ~20 sessions of an unnecessary block. This is the same lesson `docs/PARKED.md`'s
  v542 correction teaches from the other side (a carried headline that had flipped sign), and the
  general rule covers both: **a number inherited from another session is a hypothesis, and the
  cheapest hypotheses to re-test are the ones that gate work.**

- **#176 — A STRUCTURALLY COMPLETE TRANSCRIPTION CAN STILL PASS THE WRONG ARGUMENTS, AND ONE WRONG
  ARGUMENT PAIR CAN MASQUERADE AS THREE SEPARATE BUGS.** `WindowBase::RedrawCustomCursor`
  (0x426eb0) had every local, branch and call site of the original, and both of its final
  `IDirectDrawSurface::Blt` calls took the wrong `(lpDestRect, lpSrcRect)` pair. The user-visible
  result was reported as three defects — cursor pinned to the top-left, transparent region drawn
  black, dirty rects not tracking — and all three were that one pair: `destRect` is the sprite's
  offset WITHIN the 256×256 scratch (a few pixels from the origin, hence the top-left pinning and
  its jitter), and `srcRect` is a coordinate in the cursor STRIP, so reading it back out of the
  scratch returns pixels the save-blit never wrote (hence the black box). Nothing in the harness
  can see this: relocations are masked, both spellings compile, and the function was already
  PARTIAL so its DIFF was expected. **Derive each argument from the stack slot the original
  actually pushes** — here `0x4272d5` loads a POINTER out of a slot rather than `lea`-ing a rect,
  which is what pins the destination as `pPrevRect` (VC5 parks that pointer in the reclaimed
  `bFullRedraw` parameter slot at `0x4270ca`) — and cross-check against the sibling that already
  matches, `PopupWndBase::RedrawSoftwareCursor`, whose save/stamp/present/restore cycle is the
  same four blits in the same order.

- **#177 — RESOLVING A PORT CRASH ADDRESS NEEDS THE LINK MAP *AND* THE ILT.** `tools/build_port.sh`
  links with `/map:link/Loco-port.map`, so a wine backtrace's `loco-port (+0xNNNNN)` frames resolve
  by finding the greatest map symbol ≤ the address. Two gotchas. The build is INCREMENTAL, so
  low addresses (below the first map symbol) are incremental-link THUNKS, which the map does not
  list — disassemble them and read the `jmp` target (`0x401785: jmp 0x45c8c0` ⇒
  `PopupWndBase_WndProc`). And a thunk address appearing on the stack is DATA, not a return
  address: a thunk contains only jumps, so nothing can return into one. Paired with
  `loco/stub_calls.log` (every `gen_stubs` code stub reports its own caller VA), this turns
  "unhandled page fault at 0" from a dead end into a two-minute lookup: the map names the frames,
  the stub log names what ran just before.

- **#178 — THE ORIGINAL'S LIST OF GLOBALS-WITH-CONSTRUCTORS IS IN THE IMAGE; DO NOT INFER IT.**
  A port that supplies its globals as a zero-filled `.bss` mirror silently drops every C++
  dynamic initializer, so any global OBJECT starts life all-zero — vtable pointer included — and
  the first virtual call through one of its embedded sub-objects faults on `call [eax+off]` with
  `eax=0`. VC5 records exactly which globals those are: one thunk per constructed global in
  `.CRT$XC`, merged into `.data`, bounds handed to the SECOND of `__cinit`'s two `_initterm`
  calls (`__xi_a/__xi_z` first = C init, `__xc_a/__xc_z` second = C++ dynamic init). In Loco:
  `__cinit` is `0x4684a0`, the XC table is `0x47e000..0x47e038`, and it has **thirteen** entries.
  Each thunk is the same three-instruction shape — `mov ecx,<object>; call <ctor>; push <atexit
  thunk>; call atexit` — so the table names both the object and its constructor outright, and a
  matching `mov ecx,<object>; jmp <dtor>` thunk sits 0x20 bytes later. That is a complete,
  ordered, authoritative worklist obtained from one `objdump` of 0x270 bytes; recovering the same
  list by reasoning about which classes "look like they have a constructor" is strictly worse and
  cannot tell you the ORDER, which matters when the objects reach for each other. Cross-check
  each entry against the link map before writing a hook: an object a real `src/` TU defines
  outright already gets its initializer from your own CRT, and constructing it again
  double-constructs it (1 of Loco's 13 — `g_worldLoadThread`, `src/FrameDriver.cpp`).

- **#179 — A PORT-SIDE CONSTRUCTION HOOK BELONGS IN THE OWNING TU, NOT THE DRIVER.** The obvious
  shape for #178's fix — placement-new every global from one scaffolding file — does not work,
  and fails SILENTLY rather than at compile time. Two reasons, both real here: where a class's
  layout is modeled by a TU-LOCAL view struct, the header's public class is entry-points-only, so
  `new (&g_Foo) Foo()` compiles and constructs *nothing* (`src/EffectSpawner.h` vs
  `src/EffectSpawner.cpp`'s `EffectSpawnerCtorViewMaybe`); and where the ctor is transcribed in
  the free `__fastcall` escape-hatch form there is no constructor to call at all
  (`WorldBoardMaybe_InitFieldsMaybe`). Put one `#ifdef LOCO_PORT` hook per global at the foot of
  the TU that owns the ctor, DECLARE them all in one shared port header (a per-TU `extern "C"`
  declaration is exactly `lint_idiom.py` class I), and let the driver just call them in the XC
  table's order. Cheap confirmation that a hook really ran: `loco/stub_calls.log`'s first lines
  should be the ctor's own still-stubbed virtuals — those are dispatched THROUGH the vtable
  pointer the ctor just stamped, so their appearance proves the pointer is no longer null.

- **#180 — THE PORT'S SCREEN IS NOT THE HOST'S SCREEN, AND EVERY `GetSystemMetrics(SM_C?SCREEN)`
  CALL SITE IS A LATENT HEAP-CORRUPTION BUG.** `Port_ClampScreenSize` gives the game a 1024x768
  "screen" (`AppWindow::LoadWindowAndBalancing`, 0x406480) and every surface, viewport and window
  derives from those two globals — but the OS still answers `GetSystemMetrics` with the real
  desktop, 3600x2338 under winemac. Any src/ site that asks the OS directly therefore computes
  coordinates in a coordinate space ~3.5x larger than the buffer it will paint into.
  `src/LoadingScreen.cpp`'s `LoadingScreen_Show` was the one such site (v562): it placed the
  loading animation at `(SM_CXSCREEN/4, SM_CYSCREEN/2 - nativeHeight/2)` = **(900, 1047)** — 279
  rows below the bottom of a 768-row work surface — and because `LocoBitmap::RestoreOverlapBlt`'s
  RAW-PIXEL path does NO clipping unless the caller passes flag `0x40` (this caller passes 0),
  `PixelCopyColorKeyBlit` walked ~570 KB past the end of the locked surface straight into the CRT
  heap. Grep for `SM_CXSCREEN|SM_CYSCREEN` after any port change; the only legitimate call site is
  the one that FEEDS the clamp. Fix shape: read `g_dwScreenWidth`/`g_dwScreenHeight` under
  `#ifdef LOCO_PORT`, and declare the two externs INSIDE the guard — `src/AppWindow.h`'s
  declaration count is a measured byte dial, and a declaration that cannot exist in the match
  build cannot move it.

- **#181 — ONE BAD WRITE PRODUCES N UNRELATED CRASHES; FIND THE WRITE, NOT THE CRASH.** v562 spent
  most of a session on three crash sites that shared no code: a `DSoundChannel` whose `pBuffer`
  AND `nVolumePercent` were both `0xffffffff`, a fault inside ntdll's heap walker under
  `new LocoBitmap`, and a wild `call [edx+0x2c]` through a `WindowBase` vptr of `0x8c62ffe0`.
  All three were #180's single blit. The tell that they are ONE bug: the register dump repeats
  byte-for-byte across runs (deterministic ⇒ a specific write, not drifting corruption), and the
  clobbered bytes are RECOGNISABLE — `obj=8c62ffe0 10a210a2 ffffc638 ffffffff ...` reads as RGB565
  pixels (yellow, two identical dark pixels, grey, a run of white), and the absurd allocation
  sizes wine rejected (`0x45460000`, `0x66670000` — its 64K round-up of `0x45454545`/`0x66666666`)
  are solid-colour pixel runs landing in a size field. **Method that worked, in order:** (1) a
  vptr tripwire in `WindowBase_WndProc` dumping the object's head on every routed message, plus a
  bind trace at `GWL_USERDATA` set time, to name the victim; (2) `Port_WatchObject` +
  hooks in `operator new`/`operator delete` — silence from BOTH, with the vptr still clobbered,
  is what PROVED "overrun" over "use-after-free", two theories the crash site cannot distinguish
  and which need opposite fixes; (3) `Port_WatchedInRange` in the blit family to name the writer;
  (4) one more level up for the rect and the widget's `.bmp` path. Each step cost one run and
  removed a whole class of explanation. See `port/PortMode.h` — the toolkit is kept.

- **#182 — `tools/build_port.sh`'s LINK IS INCREMENTAL, AND AN INCREMENTAL LINK REUSES THE OLD
  SYMBOL RESOLUTION.** `/debug` makes LINK 5.10 link against `build/Loco-port.ilk`. That is fine
  while objs only change bodies, but silently wrong the moment a scaffolding obj starts DEFINING
  a symbol that used to come from a library: `link/stubs.cpp` gaining its own `operator new`
  (`??2@YAPAXI@Z`) relinked "PORT LINK OK" FOUR times while `link/Loco-port.map` still showed
  `LIBCMT:new.obj` and the exe still called LIBCMT's. There is no warning — the link prints
  nothing at all — and byte-identical manual links resolved it correctly, so the difference is
  invisible unless you delete the outputs. Fixed by dropping the `.ilk` whenever a scaffolding obj
  is newer than it. **Verify a shadow actually took by grepping the MAP for the symbol's owning
  obj, never by "the link succeeded".**

- **#183 — THE PORT MUST HAVE EXACTLY ONE HEAP, AND `extern "C" _malloc` DOES NOT SHADOW LIBCMT's
  `malloc`.** `link/stubs.cpp` defined `_malloc`/`_free` as `HeapAlloc`/`HeapFree(GetProcessHeap())`
  on the theory that this "shadows LIBCMT's malloc.obj/free.obj (which then never get pulled)" and
  that calling `malloc` would recurse. Both halves are false: `extern "C" void *_malloc(...)`
  mangles to `__malloc`, a DIFFERENT symbol from LIBCMT's `_malloc`, so nothing is shadowed and
  nothing recurses — the map listed `__malloc  stubs_port.obj` and `_malloc  LIBCMT:malloc.obj`
  side by side. The port therefore ran two allocators over two wine heaps (the process heap for
  src/'s `_malloc`, LIBCMT's `HeapCreate`'d `_crtheap` for `operator new`) where the original has
  exactly one — `src/DSoundChannel.h:45` records 0x4673c0 tail-calling `__nh_malloc`, i.e. src/'s
  `_malloc` IS `operator new`'s allocator, so the original can mix the two spellings freely and
  our port could not. Now forwards to LIBCMT's own `malloc`/`free`.

- **#184 — A TU-LOCAL VIEW STRUCT OF A CLASS WHOSE REAL BODY IS TRANSCRIBED IS A LIVE DEFECT, AND
  IT IS INVISIBLE TO EVERY LINT WE HAD.** A C++ method's mangled name carries its SCOPE, so
  `?Load@ViewStruct@@…` and `?Load@RealClass@@…` are two symbols for one address. While the callee
  is untranscribed that is harmless — nothing defines either spelling. The moment a real definition
  lands under the real class, the view's spelling defines NOWHERE, and in the port
  `link/gen_stubs.py` supplies it as `xor eax,eax; ret N`. **Every call into the fully transcribed
  body then returns 0**, silently, forever. `lint_alias.py` compares NAMES and both spellings share
  the name; `lint_desync.py` grouped by `Scope::name`, so the two never even landed in the same
  group; `lint_ghidra_sync.py` is happy because each declaration agrees with Ghidra independently;
  the compiler is happy because each TU is well-formed; and the byte-match is happy because
  `verify.py` masks relocations. Nothing in the project could see it.
  ⇒ v563: NINE methods of the DAT_004a99b0 singleton (`ScriptEventLoader`, fully transcribed in
  `src/ScriptEventLoader.cpp`) were reached through FOUR different view structs and all nine were
  stubs. `LoadTimeEventScriptsMaybe`'s real body `return 1;` unconditionally; the stub returned 0;
  `App_LoadWorldThreadProcMaybe` reads a zero there as a fatal load failure and answers it with
  `PostMessage(WM_CLOSE, wParam=3)`; and `AppWndProc`'s WM_CLOSE arm with a NONZERO wParam is
  resource string 0x14a — **"An error occurred while loading. Please reinstall this software."**
  That message box, chased across several sessions as "the world load failing", was one stub's
  return value. Exactly the same cause CLAUDE.md already records for `UIResources::Init` (0x446050);
  this is the general form of it.
  ⇒ **`tools/lint_desync.py` now detects the class (`VIEW` findings).** The rule is deliberately
  narrow, because classes legitimately share method names (`Init`, `MarkDirty`): it fires only when
  the undefined spelling's class is ALREADY KNOWN TO BE A VIEW — i.e. some global in the project is
  declared with two or more different class types and that class is one of them, which makes them
  alternative models of ONE object by construction. It found 13, in four families; the seven
  EasterEgg ones are fixed for the port and the other six are live.
  ⇒ **The port fix is a forwarder, NOT the real fix.** The real fix is hoisting the class into a
  header its consumers can include, which retires the view — a measured-parity change nobody has
  paid for. The forwarders live in `#ifdef LOCO_PORT` blocks (`src/ScriptEventLoader.cpp`) and in
  `link/stubs.cpp` for views that are TU-local to their consumer, because redeclaring one of those
  in `src/` would be a genuine duplicate-class definition — `lint_idiom.py` class E, the drift
  hazard that rule exists to stop. `lint_idiom.py` only scans `src/`, and `link/stubs.cpp` already
  declares a local `class DSoundChannel` for exactly this "reproduce a mangling without owning a
  model" purpose. The VIEW findings correctly keep firing afterwards: scaffolding is not a fix.

- **#185 — A DECLARED-ONLY VIRTUAL IS A STUB, AND A STUB THAT NEVER WRITES ITS OUT-PARAMETER FAILS
  ITS CALLER'S VALIDITY CHECK.** Same family as #184 but reached through the vtable rather than a
  name. `AnimDescRefObj0x477488::DispatchAnimStateMaybe` (slot 14, 0x405a50) was declared-only, so
  it was a generated stub. Its caller `AnimDescRefObj0x477488::SetDescriptor` calls it via
  `ReleaseChannelAndDispatch` and then tests `if (nSubFrame == -1) { bValid = 0; return 0; }` — and
  the stub never writes `nSubFrame`. So **`SetDescriptor` returned 0 for every widget in the game**,
  which aborted the world load at `BuildToolButton::InitMenuIconsMaybe`'s very first guard. Note the
  shape: the stub is not merely inert, it fabricates a specific wrong ANSWER, and the caller's own
  correctness check is what converts that into a hard failure. When triaging a stub, read what the
  CALLER does with the value, not just whether the callee "matters".

- **#186 — RE-MEASURE A WITHHELD BODY BEFORE BELIEVING ITS PRICE; v563 MAKES IT TWO FOR TWO, AND
  THIS TIME THE TOLL WAS NEGATIVE.** CLAUDE.md's v560 corollary said a priced-and-withheld toll is a
  measurement with an expiry date. `DispatchAnimStateMaybe`'s body sat verified but out of the tree
  from v479 to v562 because merely ADDING it cost the sibling `AdvanceAnimFrameMaybe` (0x405c40) its
  full 407-byte EXACT — measured three separate ways at the time. Landing it in v563 measured
  **EXACT 173238 → 173645 B, +407 B / +1 func**: 0x405c40 came back to EXACT. So the parity bit this
  body was withheld against had not merely expired, it had inverted — paying the toll REFUNDED it.
  Both measurements were correct when taken. One compile is the whole cost of checking, and the
  withheld body was simultaneously blocking the port's entire widget layer (#185).

- **#187 — `lint_desync.py`'s VIEW CHECK IS BLIND TO A VIEW REACHED BY CASTING AN EXISTING GLOBAL,
  AND THAT SHAPE HIDES THE SAME DEFECT.** The v563 check fires only when some global is declared
  under ≥2 class types — that is what identifies a class as a "view". `src/WorldBoardMaybe.cpp`'s
  `PeerTrainSlotQueueView0x456150 : PeerTrainSlotQueueMaybe` never declared a global of its own; the
  one call site CAST the real `g_PeerTrainSlotQueue` to it. The precondition never fired, so the lint
  said nothing — while the derived redeclaration of `CollectAndSortVisibleCarsMaybe()` NAME-HID the
  real class's identical declaration, mangled under the view's name, and reached a symbol defined
  nowhere. In the port it was the hottest stub of a whole boot after the collection forwarders
  landed: once per frame, no train car ever collected or sorted for drawing. **The stub-call log is
  the oracle the lint is not** — `link/stubs.cpp` writes `stub_calls.log`, and sorting it by count
  ranks the live defects by how much they actually cost at runtime. Fix was to delete the view and
  call the real class's method; measured free. A derived "view" that only redeclares methods the base
  already declares is always pure name-hiding — grep for that shape directly, the lint cannot.

- **#188 — A DECLARED-ONLY VIRTUAL LANDS IN THE COMPILER-GENERATED VTABLE, SO ONE STUBBED SLOT
  BREAKS EVERY CALLER OF THAT SLOT — INCLUDING CALLERS THAT SPELL IT THROUGH A DIFFERENT CLASS.**
  The generalisation of #185. The class that CONSTRUCTS an object fixes the vtable that object
  carries for its whole life, so a slot left declared-only there is a `xor eax,eax; ret` for every
  later dispatch, from any TU, under any spelling. Two instances, both found v564 by ranking
  `stub_calls.log`: `PlacedObjCollectionMaybe::Count`/`GetAt` (3480/3476 calls in one run) made
  every `DecorObjMgrMaybe` registry walk see an EMPTY collection each frame; and slot 11 on
  `src/EffectSpawner.cpp`'s four TU-local ctor views did the same to the effect paint/tick walks —
  which reach it as `GetSlotCountMaybe()` on a *completely different* view class, because only the
  slot number is shared. Two consequences worth internalising:
  - **A count-returning slot is the worst one to stub**, because 0 is a valid answer meaning
    "nothing to do", so the walk silently no-ops instead of crashing. Rank stubs by call count.
  - **Forward to the ORIGINAL's own slot split, read out of `.rdata`, not to whichever body seems
    right.** Here the base tables (0x477bd0/0x477b40, and the PlacedObj base) hold `0x424010` =
    CAPACITY at +0x8, while the derived tables (0x477b78/0x477ae8) hold `0x424000` = `m_0c` at +0xc,
    and the live registries all carry the DERIVED table. Two in-tree comments disagreed about which
    one slot 11 was; dumping the four vtable dwords from the image settled it in one script. A
    port-only `virtual` override on the derived class reuses the existing slot, so it changes no
    vtable LAYOUT in either build — `#ifdef LOCO_PORT` in the header is safe and was measured free.
  Only forward slots that do NOT name the element type (here 7/8/11/12); the ones that do (9/10/13,
  and any slot named for a derived override) need their own address evidence first.

- **#189 — AN UNDEFINED *DATA* SYMBOL IN THE PORT IS A SILENTLY ZEROED TABLE, AND UNLIKE A CODE
  STUB IT CANNOT REPORT ITSELF.** The data-side twin of #184/#187/#188, and the single most
  expensive bug this project has had: it hid the entire board renderer for ~14 sessions while every
  gate upstream of it read as healthy. `link/gen_stubs.py` places each unresolved DATA symbol in a
  MIRROR of the original's `0x477000..0x501000` span — which correctly reproduces the spacing
  between neighbouring globals, but the mirror is a **`.bss` section, so every byte of it is zero**.
  That is right for a global that genuinely lives in the original's BSS tail and silently wrong for
  one that lives in initialised `.rdata`/`.data`.
  - **The instance.** `DAT_0047f108` is the packed dirty-bitmap's 8-byte mask table, `0x80 >> i`.
    Zeroed, `bitmap[n>>3] |= 0` marked nothing and `0 & bitmap[n>>3]` found nothing, so
    `WorldBoardMaybe::UpdateDirtyTiles` scanned all 64×48 tiles every frame, coalesced **zero**
    dirty rects and blitted nothing, for ~1850 consecutive frames — while `MarkRectDirty` was being
    called correctly, the in-game gate passed, the viewport was right and `Ddraw_BltUpdateRect`
    succeeded on every call it got. Defining it as real source (one line) put the board on screen.
  - **Why nothing could see it.** No compiler diagnostic (an `extern` array is a complete
    declaration), no lint (`lint_alias`/`lint_desync` compare NAMES, and the name was never in
    dispute), no `stub_calls.log` (a data stub runs no code, so it cannot report itself), and no
    byte-match signal (the match build LINKS nothing). The in-source comment even described the
    table as `1<<(i&7)` — the wrong direction — and that was equally invisible.
  - **The tool.** `tools/datastubs.py` is the data-side analogue of sorting `stub_calls.log` by
    count: for every remaining data stub it reads the ORIGINAL image at that address and ranks by
    how many NON-ZERO bytes are actually there. A non-zero row is a live defect; an all-zero row is
    a real BSS global the mirror already models correctly. Run it after any port change, exactly
    like the stub-call ranking. 20 live rows at first run, 17 after this session.
  - **Do NOT "fix" this by initialising the mirror from the image.** The image has no `.reloc`
    section, so a mirrored ABSOLUTE pointer (every `g_vtable0x*` row is nothing but pointers) cannot
    be distinguished from a scalar and would land on whatever the port's own link put at that VA.
    The fix is per-symbol and is ordinary decompilation: **transcribe the table into real source.**
    Three landed here (`DAT_0047f108`, `g_aMonthDayOffsetMaybe` = the cumulative days-before-month
    table, `g_anDecorCenterBoundsMaybe` = `{7,70,164,224}`), all three measured **byte-free**.
  - **Corollary — a bare `_name` with no `@N` is ambiguous, and the safe default is CODE.**
    `gen_stubs.py`'s `is_code()` read every undecorated `_name` as data, so a `__cdecl` C-linkage
    FUNCTION got a zeroed mirror slot instead of a reporting code stub: calling it jumps into BSS.
    `_DDraw_CreateSurfaceFromFile` was one, on the path every non-8bpp bitmap asset takes.
    Fixed v566 by deciding on this project's own naming convention (`g_`/`DAT_` prefixes are data,
    everything else is code) — a wrong guess toward code merely returns 0 and says so in the log,
    while a wrong guess toward data is silent. Two thunks stopped being zeroed the same way.

- **#190 — WHEN THE RENDERER STARTS WORKING, THE STUB RANKING RE-RANKS; RE-RUN IT AFTER EVERY FIX
  THAT UNBLOCKS A PATH.** `stub_calls.log` only ranks defects on code that actually RUNS, so a
  blocker upstream masks everything behind it. The moment #189's one-line fix made the board paint,
  `BuildToolButtonPaintView0x456700::BlitAllRegionsMaybe` went from *invisible* to **the hottest
  stub in the run at 703 calls** — a plain #184 view struct (`(RECT,int)` on its own class versus
  the real `BuildToolButton::BlitAllRegionsMaybe(RECT,char)`) that had been sitting there the whole
  time, skipping the toolbar on every dirty rect of every frame. `lint_desync.py`'s VIEW check was
  blind to it for the #187 reason *plus* a new one: the view had its own global spelling
  (`DAT_004aa5b8`) rather than sharing `g_BuildToolButton`, so the "one global, ≥2 class types"
  precondition never fired. **Two independent blind spots now; treat the VIEW lint as a floor.**
  ⚠ This fold was the first in the family that was NOT free: `#include "BuildToolButton.h"` cost
  1162 B / 2 funcs, all inside `src/WorldBoardMaybe.cpp`. Paid anyway (a call to a symbol defined
  nowhere is a defect, not a style question), and 951 B of it is recoverable by ONE extra file-scope
  declaration in that TU — see the priced note at the include itself.

- **#191 — MANY VTABLE SLOTS SHARING ONE ADDRESS IS ICF, NOT ONE FUNCTION WORN MANY WAYS: WRITE
  EVERY BODY, MARK EXACTLY ONE.** `WindowBase_Vtbl` (0x477c30) holds 0x422ea0 at eighteen slots and
  `PopupWndBase_Vtbl` (0x477898) at nineteen more; fourteen further slots across seven derived
  classes fold onto seven other addresses. That is the linker folding identical COMDATs — the same
  mechanism that folded this image's `??_G*` thunks — and the original source really did write each
  body out. A `__thiscall` method that never touches its implicit `this` compiles byte-identical to
  the `__stdcall` free function beside it, which is why `WindowBase::OnTimerDefaultMaybe` and
  `WindowBase_DefWindowProcStub` are one 29-byte copy.
  - **Proof, and it is cheap: compare each new COMDAT against the IMAGE's bytes at the folded
    address.** Not "it compiles" — 51/51 byte-identical, which also validates every transcription
    at once. `tools/match.py`'s `coff_functions`/`trim_pad`/`mask` do it in ~20 lines.
  - **ONE address gets ONE `// FUNCTION:` marker.** A second marker double-counts the address in
    `progress.py`. The twins go in unmarked — `progress.py` scores neither bucket for them, so the
    per-file DENOMINATOR rises and EXACT/PARTIAL do not move at all.
  - ⚠ **Do not write the marker text in PROSE.** `_MARKER_RE` does not care that it is inside a
    paragraph; one explanatory sentence containing `// FUNCTION: LOCO 0x422ea0` re-paired the whole
    file and cost it two matches.
  - **The pairing family is real: `OnRButtonDown` is `return OnLButtonDown(...)` in eight classes
    and folds to TWO addresses** — 0x4323c0 for the WindowBase family, 0x451520 for PopupWndBase —
    purely because the two hierarchies put `OnLButtonDown` at different slot displacements. A
    two-address split like that is a fingerprint of which base a class actually derives from.
  - Measured cost of all 51: **zero**, EXACT and PARTIAL unchanged to the byte.

- **#192 — `pair_by_name`'s LOOSE FALLBACK COULD STEAL A COMDAT A LATER MARKER OWNED OUTRIGHT.**
  The derived key is anchored at the front of a mangled name (`?Create@WindowBase@@` vs
  `?Create@WindowBase@@QAEE...`), but the substring fallback that exists for `extern "C"` spellings
  ran in the SAME pass — so `?Create@WindowBase@@` matched `?OnCreate@WindowBase@@` and
  `WindowBase::Create` was scored against a 29-byte body. **Any method whose name merely ENDS with
  another's collides this way**, and it presents as a sudden DIFF on a function you did not touch.
  Fixed v567: anchored matches now run as a first pass over all markers, loose/positional second.

- **#193 — A CLASS THAT RE-DECLARES A VIRTUAL IT ONLY INHERITS IS AN INVISIBLE #184, AND
  `lint_desync.py`'s VIEW CHECK HAS A THIRD BLIND SPOT.** `AnimDescRefHotspotPartial` carried
  `void RepositionWithHotspot(int,int);` — written before `src/WidgetBase.h` declared the virtual,
  and never removed. Matching signature makes it an OVERRIDE, so it takes the slot: every
  `AnimDescRefHotspotPartial::RepositionWithHotspot(...)` call mangled under that class, resolved to
  a symbol nothing defines, and in the port became a do-nothing stub — effects spawned and never
  positioned. **Deleting the declaration is the whole fix** (the qualified call sites still compile,
  the name now resolves to the inherited base member) and it measured **free**.
  - The VIEW check could not see it: `view_class_names()` proves a class is a view by finding one
    global declared under ≥2 class types, and a class that appears only as a BASE is the type of no
    global at all. That is blind spot THREE, after #187's cast-reached view and #190's own-global
    view.
  - **New `INHERIT` finding class closes it** — an undefined VIRTUAL whose class defines nothing
    anywhere, while the identical SIGNATURE is defined under a real class. Both halves are needed:
    signature-only reports 64 findings here and 63 are the ordinary "base declares the slot, derived
    classes override it" shape. Adding "the class defines nothing at all" — a pure re-declaration
    shell — cuts it to 10, and four of the ten are exactly the symbols still in `stub_calls.log`.

- **#194 — BEFORE CALLING MISSING UI A BUG, CHECK WHICH MODE THE REPRO RUNS IN.** The `-s` boot
  repro is SCREENSAVER mode, and `BuildToolButton::InitMenuIconsMaybe` itself does
  `if (g_screenSaver.bScreenSaverMode == 1) SetReadyStateMaybe(0)` — the toolbar is deliberately
  hidden, `bReady` is 0, and `AnimDescRefObj0x477488::BlitAnimFrameMaybe` returns immediately. A
  board with no UI is the CORRECT picture for that flag. Related, and the same shape: the 45
  `desc ... ok=0` loads on the front-end art are not failures either — `startup\goup.dat` and its
  siblings genuinely do not exist in `resource.RFH`, only `startup\goup.bmp` does, so the original
  gets `bLoadOkFlag = 0` there too. Read the archive listing before chasing a loader bug.

- **#195 — `lint_desync.py`'s INHERIT LIST IS A WORKLIST FOR THE *PORT* BUILD, BUT IT MEASURES THE
  *MATCH* BUILD, SO A FIXED ROW NEVER DISAPPEARS.** v568 handed v569 ten INHERIT rows to "work".
  Seven of them are a class of declaration that is body-less ON PURPOSE and always will be: the
  original keeps an out-of-line `call` at the site, and giving the declaration a visible body is
  exactly what lets /O2 inline it away and break the match.
  `EffectCollectionCtorViewMaybe::ReserveMaybe` is the type case — its comment already said
  "DECLARED-ONLY on this half: a visible body is what lets /O2 inline it". What gets fixed is the
  PORT, with an `#ifdef LOCO_PORT` forwarder; the default lint run, which globs `build/*.obj`,
  cannot see that and reports the row forever. Two sessions can each "discover" the same
  already-fixed defect. `--objdir build/port` (added v569) asks the question that has an answer.
  ⚠ But a clean `--objdir build/port` run is NOT proof a class is whole: the INHERIT test fires
  only on a class that defines NOTHING ANYWHERE, so forwarding ONE method blinds it to that
  class's remaining stubs — measured, `PlacedObjCollectionMaybe` still stubs slots 3/9/10/13 while
  build/port reports 0 INHERIT. `link/gen_syms_port.txt` is the authoritative list; it comes out
  of the link and has no heuristic in it.

- **#196 — A PORT DEFECT CAN HAVE SEVERAL INDEPENDENT CAUSES IN SERIES, AND FIXING ONE MOVES THE
  SYMPTOM WITHOUT MOVING THE OUTCOME. INSTRUMENT THE CHAIN, DO NOT REASON ALONG IT.** v568 left a
  precise-sounding lead: four `blt ok src=0,0,3600,2338` on a 1024×768 surface, "the rect is
  `DAT_004fd3d8->rect`, trace who writes 3600×2338 into it". Every part of that was wrong, and the
  cheapest thing that could have said so was one `Port_Tracef` of the CALLER's return address
  (read out of the `__cdecl` frame's own parameter home, `((void **)&firstParam)[-1]`, then mapped
  through `link/Loco-port.map`). It named `WindowBase::CommitScreenUpdate` in one run — not
  LoadingScreen at all. The rect was `this->rectClipBounds`, and the chain behind it had THREE
  independent causes:
  1. **Seven call sites size a full-screen window from `GetClientRect(GetDesktopWindow(), &r)`.**
     In 1998 the desktop client rect WAS the screen; under winemac it is 3600×2338 while every
     surface the game paints into is `g_dwScreenWidth × g_dwScreenHeight`. Fixed with one shared
     `Port_ClampDesktopRect` (port/PortMode.h) rather than seven copies of the clamp.
  2. **That was necessary and NOT sufficient** — all seven then *asked* for 1024×768 and every one
     still came up with a 3600×2338 CLIENT rect, because `WindowBase::Create` hardcodes style
     `0x87000000`, and the `WS_MAXIMIZE` bit in it makes the window manager size the window to the
     desktop whatever the create asked for. Free in 1998 (the game ran AT the desktop resolution,
     so the two agreed); a real bug the moment they differ.
  3. **Dropping `WS_MAXIMIZE` then exposed the third cause**: winemac places a shown popup below
     the Mac menu bar (y=78 here), so `Ddraw_BltUpdateRect`'s SCREEN-space `ClientToScreen` pushed
     the dest to `0,78,1024,846` — off the bottom of an emulated primary that is exactly
     screen-sized, the wholesale-failure mode port/README.md's present notes describe. The port's
     "screen" is the app owner's CLIENT area (that is what `Port_Present` blits into), so the fix
     is to translate `rcDest`/`rcWindow` by the owner's own client origin. That translation is the
     identity when the window manager does place us at 0,0, so it costs nothing on a well-behaved
     host and is self-correcting on one that is not.
  Each fix looked like "the" fix while the symptom was still there. The general lesson: when a
  port symptom survives a correct fix, do not assume the fix was wrong — instrument the next link
  in the chain. Also note cause 2 is invisible to any amount of source reading, because the style
  word is a hex constant transcribed faithfully from the original.

- **#197 — AN UNCLIPPED OVERSIZED BLIT RECT IS A MEMORY-SAFETY BUG, NOT A COSMETIC ONE.**
  `LocoBitmap::RestoreOverlapBlt` does NO clipping unless the caller passes flag 0x40, and the
  front-end paths pass 0. That is the documented v562 heap-corruption mechanism (a write ~570 KB
  past the locked surface, landing on a live object whose vtable pointer became pixel data, and
  surfacing three crashes later as a wild call). So the window-sizing chain in #196 was not a
  "black screen" bug with a memory-safety footnote — the corruption and the black screen were the
  same defect, and the geometry fix is what closes both. When a rect that feeds a blit is wrong,
  price it as corruption first and appearance second.

- **#198 — BEFORE HUNTING A PORT DEFECT, CHECK WHETHER THE CODE IS SUPPOSED TO DO ANYTHING AT ALL
  IN THE STATE IT IS ACTUALLY IN.** v569 closed the front end's geometry chain (#196) and left the
  present still reading `sum=00000000 nonblack=0/786432`, with the reasonable-sounding next step
  "find out whether SplashWnd's backdrop ever blits". It does — just not in the state an unattended
  run sits in. `SplashWnd::OnActivate` (0x421be0, vtable slot 0x20, and the ONLY paint entry the
  class has: it leaves WM_PAINT/slot 0x6c at WindowBase's default) has three arms, and only
  **state 7** — the interactive front end, after the boot videos — draws anything. States 0..6 just
  `CommitScreenUpdate` whatever is already on the work surface. State 0 is the boot-video state and
  is blank ON PURPOSE, because MCIWnd is painting the clip over the top with GDI, outside
  DirectDraw entirely. An unattended run never leaves it: the sequencer only advances on the 0x3b9
  "video finished" notify and `art-res\video\locoIntr.avi` is 103 MB. There was no bug.
  Two cheap habits would have caught it a session earlier — trace the STATE VARIABLE next to the
  symptom (three `Port_Tracef` lines in `SetState`, `OnActivate` and the 0x3b9 case were the whole
  diagnosis, and they are worth keeping: a state machine is otherwise completely invisible in a
  log), and ask what the ORIGINAL would have presented at that same instant.
  Method for driving a boot sequencer forward without input, in general: do not patch the code,
  **starve the data**. Pointing `lego.ini`'s `[Video] Dir=` at a missing file made the 0x3b9 chain
  fall through to its default arm, `SetState(0 -> 7)` fired, the previous clip's MCI child was
  destroyed, THAT invalidated the parent, WM_PAINT reached `WindowBase::OnPaint`, and the state-7
  arm painted the real menu (`sum=b11620eb nonblack=772993/786432`). Faithful, reversible, and it
  exercises the same code path a keypress would.

- **#199 — WIRING A PORT FORWARDER FOR A SLOT THAT DRIVES A `while (count) RemoveLast()` LOOP IS A
  HANG UNLESS THE SLOT IT DISPATCHES THROUGH IS WIRED FIRST — CHECK THE WHOLE CHAIN, NOT THE SLOT.**
  The Obj0x477798 family's slot 6 (destroying clear) has a derived form of exactly
  `while (m_0c != 0) RemoveAndDeleteAt(m_0c - 1);`. Slot 4 is `delete RemoveAt(idx)`, slot 3 is
  what actually decrements the live count — and slot 3 was itself a `ret` stub under one of its two
  declared spellings. Forwarding slot 6 alone would have spun forever, and a stub-count metric
  would have called it an improvement. Before forwarding any slot, walk what its body dispatches
  into and confirm every link has a real definition; `link/gen_syms_port.txt` is the list to check
  against.
  Two sub-lessons from the same fix. **A base and a derived form of the same slot are not
  interchangeable just because both "clear the collection"**: here the BASE (0x424510) walks the
  whole CAPACITY and the DERIVED (0x424270) peels the LIVE range off the end, so pointing the
  derived tier at the base body would call `RemoveAndDeleteAt` on every empty slot of a 100-slot
  registry. Read which address each table actually holds out of `.rdata` and add the real override
  (inside `#ifdef LOCO_PORT`, reusing a slot the base already owns, so no vtable LAYOUT change in
  either build) instead of collapsing them. And **a forwarder does not always cast toward the
  canonical family name**: 0x4241e0 is declared as both `Obj0x477758::RemoveAt` and
  `PlacedObjRegistryMaybe::RemoveAtShiftingTail`, and the BODY landed under the second — so the
  forward runs the other way. The soundness argument is unchanged (one original address, two
  declared spellings, a body that never names the element type); only the direction is.

- **#200 — TWO PLACEHOLDER VIRTUALS FOR ONE VTABLE SLOT MUST AGREE ON THEIR PARAMETER LIST, OR THE
  GENERATED PORT STUB POPS THE WRONG AMOUNT AND SILENTLY CORRUPTS THE CALLER'S STACK.** This is
  #199's family — two declared spellings of one address — but the symptom is much worse than a
  wrong body, and nothing in the repo catches it. `src/EffectSpawner.cpp` declared slot 13 twice:
  `EffectCollectionVtblProbeMaybe::AddMaybe(void *pEffect)` (the call-site spelling) and
  `EffectCollectionCtorViewMaybe::_v13()` **with no parameter** (the declared-only placeholder that
  ends up in the live object's vtable, because the ctor view is what stamps the table). Both are
  `__thiscall`, so the CALLEE pops, and `link/gen_stubs.py` derives the pop from the MANGLED NAME —
  so the stub returned `ret 0` where every call site pushes 4 bytes and expects `ret 4`. The first
  dispatch through the slot left `esp` 4 bytes off and returned to garbage. Fix is one word
  (`_v13(void *pObj)`) and is byte-neutral for the match build; verify with `progress.py` anyway,
  since it is a declaration change.
  **Why no lint sees it:** `lint_alias.py` compares marker-anchored NAMES and neither spelling is
  marker-anchored; `lint_desync.py` groups by the identifier the mangling encodes and the two
  identifiers DIFFER (`AddMaybe` vs `_v13`); and the arity disagreement lives entirely in a
  placeholder that nothing calls by name. **So the rule has to be applied by hand: whenever you
  write a `_vNN()` placeholder for a slot that any sibling struct declares with real parameters,
  copy the parameter list across.** A placeholder's RETURN type is free; its PARAMETER LIST is not.
  ⚠ The same file's slots 4 (`RemoveAtMaybe(nIndex)`) and 8 (`GetEntryMaybe(nIndex)`) carried the
  identical trap, unfired — and slot 8's was unfired *only because slot 13 was broken*: with every
  Add dropped, `m_0c` stayed 0, so the paint family's `for (i = 0; i < GetSlotCountMaybe(); i++)
  GetEntryMaybe(i)` walk ran zero iterations. **Two bugs of this class mask each other, so fix the
  arities of a whole vtable BEFORE giving any one slot a real body** (done v571 for 4/8/13).

- **#201 — A PORT FAULT ON A WORKER THREAD LOOKS EXACTLY LIKE THE MAIN PUMP HANGING, AND THE
  DIFFERENCE IS ONLY VISIBLE IF THE WORKER IS INSTRUMENTED PER STAGE.** v571's crash presented as:
  the in-game pump iterating 6 times and then stopping mid-`MsgWaitForMultipleObjects`, no wine
  exception (`WINEDEBUG=+seh` printed nothing — CrossOver's wine appears to ignore debug channels,
  so a silent `+seh` log is NOT evidence of "no exception"), no `WM_QUIT`, no `PostQuitMessage`, and
  an exit code that happened to coincide with one of the world load's own abort codes. Every one of
  those readings pointed at the main thread and all were wrong: the fault was on
  `g_worldLoadThread`, three call levels inside `BuildToolButton::InitMenuIconsMaybe`.
  What actually localized it, in order: (1) an intra-iteration STEP marker in the pump proved the
  main thread was dying at an arbitrary point rather than at a specific call — which is the
  signature of another thread taking the process down; (2) ENTRY markers per world-load stage in
  `App_LoadWorldThreadProcMaybe`. That function already had five `LOCO_PORT_LOADFAIL` codes, and
  they were actively misleading: **a failure code only reports an ORDERLY abort, so its ABSENCE
  reads as "this stage passed" when it equally means "this stage faulted".** Pair every abort
  diagnostic with an entry diagnostic. Also: an exit code that matches a known error code is a
  coincidence until the matching diagnostic line is in the log — v571 spent a probe on `wParam=5`
  vs exit 5 before noticing no `LOAD ABORT` line had ever been printed.

- **#202 — DRIVE THE PORT'S INPUT WITH A DATA-DRIVEN SYNTHETIC-CLICK SCRIPT, AND REGISTER THE HIT
  RECTS FROM THE ENGINE SIDE.** An unattended run cannot click, which for 40+ sessions meant no
  input path in the port had ever executed and every conclusion about the front end came from what
  it PAINTED. `LOCO_PORT_CLICK="N:target[;N:target...]"` (port/PortMode.h) posts
  WM_MOUSEMOVE + WM_LBUTTONDOWN + WM_LBUTTONUP on present-frame N. Two design points that made it
  work first try, both worth copying: **the coordinates come from the engine, not from arithmetic**
  — `SplashWnd::RefreshClientClipRect` calls `Port_RegisterHitRect("enter", hwndSelf, &rect)` for
  each of its six rects, which is the only way to get them right when every rect is sized from an
  art descriptor's runtime native extent (and the registration log is itself the first time those
  rects were ever printed); and **aim the "just reach the window proc" click at a deliberate
  NON-target** (a corner rect that hits none of the six) rather than the client-rect centre, which
  may land inside a real hit rect. Do NOT patch the state machine to skip input: a click in
  SplashWnd state 0 takes the real `SetState(7)` path, so the same mechanism that tests the click
  surface also skips the 103 MB boot video — which is strictly better than v570's ini-starving
  trick, and needs no data edits at all. `PostMessage`, not `Send`: the button arms `Sleep(150)`
  and the Esc arm spins on `IsReclaimable`, so the work belongs on the window's own pump.
  ⚠ A no-op arm is not a broken arm — all four of the front end's SETTING rects early-return in the
  default configuration (`bSkipSetupWizardMaybe` is already 1, and the remembered-choice pair is
  dead while it is), so a click that changes no pixels there is correct engine behaviour.

- **#203 — A STUBBED VTABLE SLOT MASKS EVERY SLOT DOWNSTREAM OF IT, SO A PORT'S STUB LOG RANKS
  FAULTS BY DEPTH, NOT BY IMPORTANCE — EXPECT TO WALK THE CHAIN ONE LINK PER RUN.** v571 fixed
  slot 13's arity and left the effect-registry Add "one forwarder away". It was four, and each
  one only became VISIBLE after the one in front of it was wired, because the collection's own
  live count gates everything behind it. The measured sequence on `EffectSpawner`'s registries,
  one boot-to-world run each: wire slot 13 (`InsertInSortedPositionMaybe`) plus its 17/10/7/12/18
  chain ⇒ `_v08` goes from **0 to 41526 hits** (the paint/tick walks could finally iterate a
  non-empty collection and reach the entry accessor); wire slot 8 ⇒ `_v04` goes from 0 to
  **12866** (the tick could finally SEE the effects that report themselves finished, and asked to
  remove them); wire slots 2/3/4/5 ⇒ the whole family goes quiet. Three of those four rows read
  **zero hits** in the run before the fix ahead of them landed. The practical rules: (a) never
  read a zero-hit stub as "not needed" while any slot in front of it is stubbed — an absence of
  hits and an unreachable code path are indistinguishable in the log (same shape as #201's
  absent abort code); (b) walk the whole chain in the DISASM before forwarding any link (#199),
  because a partial fix silently relocates the drop one level down AND removes the log row that
  was pointing at it, so it reads as a success; and (c) budget one run per link — the log is the
  only oracle that says which link is live, and it can only ever show you the shallowest one.

- **#204 — `.rdata` DECIDES WHETHER A PORT FORWARDER IS ONE BODY OR TWO; DUMP THE WHOLE VTABLE SET
  BEFORE WRITING ANY OF THEM.** Forwarding a stubbed slot to the family's real body is only
  correct when the tables agree on the address. One `run_script_inline`-free Python read of the
  four `Obj0x477798`-family effect tables (0x477bd0 / 0x477b78 / 0x477b40 / 0x477ae8, base = 14
  dwords, derived = 22) settled every one of v572's twelve slots at a glance, and it split three
  ways rather than two: **one address across all four** (slot 2 `0x424020`, slot 4 `0x4356e0`,
  slot 7 `0x424530`, slot 8 `0x424030`, slot 12 `0x424760`, and slots 0/15/16/19/20/21) ⇒ one body
  on the BASE view; **a base/derived split with genuinely different behaviour** (slot 3
  `0x4356b0`→`0x4241e0`, slot 5 `0x4244f0`→`0x424250`, slot 10 `0x424170`→`0x424290`, plus 6 and
  11) ⇒ a `#ifdef LOCO_PORT` override on the derived view, and ⛔ never a collapse — the derived
  slot 3 SHIFTS the tail and decrements the live count where the base only vacates a slot, and
  the derived slot 10 adds a bounds reject; **derived-only** (slots 13/14/17/18, with slot 13
  NULL in both base tables) ⇒ declared on the derived view alone, since the base's NULL is a real
  pure-virtual-style hole. The third axis — the same slot at a different address per
  INSTANTIATION (slot 10 is eight addresses, slot 17 two) — is the one case where collapsing IS
  faithful: those bodies are one source text made distinct only by each `.obj`'s own copy of the
  ±1.1 growth constant, a relocation difference. So "two addresses" alone never answers the
  question; read what the two bodies DO. The only slot v572 left unforwarded is the one that
  genuinely names the element type (slot 9, which constructs an element) — no cast substitutes
  for a constructor.

- **#205 — VERIFY A PORT FIX WITH A CHANGE-TRIGGERED STATE PRINT, NOT WITH THE ABSENCE OF A STUB
  ROW.** A stub row disappearing proves only that the symbol got a body — it is equally
  consistent with a forwarder that returns the wrong thing, and (per #203) with a fix that merely
  moved the fault downstream. A three-line `#ifdef LOCO_PORT` block that logs the state the fix
  is ABOUT, printing only when it changes, costs one static per field and stays quiet across a
  10000-frame run: `EffectSpawner_TickMaybe`'s `effects: ghost=%u placed=%u candidate=%u` fired
  seven times in a 200-second run and said exactly what the stub log could not — the candidate
  set holds the toolbar sparkle at a steady 1 (so the object is resident where
  `PaintSimpleEffectsMaybe` looks for it), while the ghost set cycles 0↔1 (so spawn AND expiry
  both work), against all three pinned at 0 before. Log the collection the fix targets even if
  the surrounding function never walks it — the candidate count is the informative one here
  precisely because the tick ignores it. Note also what this did NOT prove: the frame checksum
  was byte-identical before and after, so a rendering oracle alone would have called a working
  fix a no-op.

- **⭐ #206 — A LOOP VARIABLE INITIALIZED ONE SCOPE TOO HIGH IS INVISIBLE TO EVERY ORACLE THIS
  PROJECT HAS, AND THE BACK-EDGE TARGET IS THE GROUND TRUTH THAT FINDS IT.** `WorldBoardMaybe`'s
  `FUN_00456700` (0x456700, the dirty-tile painter) was transcribed with `int col = colMin;`
  OUTSIDE its row loop instead of at the top of the body. The inner column walk leaves
  `col == colMax`, so from the second row onward the `if (col < colMax)` guard is false and
  **only the FIRST ROW of each coalesced dirty rect is ever repainted** — the whole incremental
  repaint of the world board, reduced to one tile row per rect, for as long as the function has
  existed. Nothing could see it: it compiles, it is not a raw offset or a duplicate struct or a
  desynced declaration, and the function is a long-PARKED EFFECTIVE residual whose ~1058 DIFF
  swamps the handful of instructions the bug costs (the fix actually moved DIFF 1058 → 1088 —
  **on a heavily-parked function the masked score is NOT evidence about correctness in either
  direction**, and reading it as such would have reverted a proven fix).
  **How to find this class:** in the original, a per-iteration re-seed is a real instruction at
  the loop's ENTRY BLOCK, and the loop's BACK EDGE jumps to it. Disassemble, find the bottom-of-
  loop conditional jump, and look at **what its target address actually executes** — 0x456b1d's
  `jl 0x4567a4` lands on `mov ecx,[esp+0x1c]; mov [esp+0x2c],ecx`, i.e. `col = colMin`, and
  0x4567b7..0x4567c2 re-seed `colIdx`, `xPix` and the cell pointer alongside it. A hoisted
  initializer makes the back edge target a LATER address than the re-seed; that address
  difference is the entire signature. Confirm the slots first (`[esp+0x2c]` is `col`: inc'd at
  the loop bottom and compared to `colMax`; `[esp+0x1c]` is `colMin`: written only by the entry
  ternary, never again).
  **The generalizable rule: when a transcription hoists ANY loop-local out of its loop, the
  original's back-edge target is the arbiter — not the decompiler's variable scoping**, which
  routinely lifts a re-seeded local to the outermost scope it can because SSA-style renaming
  makes the two forms look alike.

- **⭐ #207 — INSTRUMENT THE GATE, NOT THE OUTCOME: COUNT EACH EARLY-OUT OF A DRAW PATH
  SEPARATELY AND THE CULPRIT NAMES ITSELF IN ONE RUN.** The v573 objective listed three
  candidate reasons a resident effect painted nothing — never called, fails its flag test, or
  the blit no-ops — and the true cause was a fourth nobody had listed. The cheap way to that
  answer is one counter per condition on the path, all printed in a single change-triggered
  line: `calls` / `blits` / `haveart` / `isect`. The reading `calls=12288 blits=12287
  haveart=12288 isect=0` refuted all three listed hypotheses at once (the walker runs, the
  entry passes `bValid && bReady`, and the sprite is loaded) and pinned the survivor: the clip
  rect handed in NEVER overlapped the effect's own rect. Duplicating a callee's early-out
  predicates in the CALLER's diagnostic is worth the redundancy — it attributes the no-op to a
  specific `if` without touching a function shared by a dozen other call sites.
  **Then escalate from a counter to a HISTOGRAM over the suspect axis.** `isect=0` still did not
  say why; a 40-bucket tally of which tile ROW each clip rect belonged to did, immediately —
  rows 0/23/27 nonzero and every other row exactly 0 is not a plausible dirty-set, and
  "first row of each rect" falls straight out of it. Pair it with the same histogram on the
  WRITER side (`MarkRectDirty` set bits in all 48 rows) so the reader/writer disagreement is
  the printed result rather than an inference. **Leave behind the one-number collapse of the
  histogram as a permanent canary** — `rows=` (count of DISTINCT rows ever repainted) reads 48
  when healthy and single digits when this regresses, at one static array and no log volume.

- **⭐ #208 — BEFORE DEBUGGING "THE OUTPUT LOOKS WRONG", PIN WHAT THE CODE IS SUPPOSED TO DRAW;
  A SOURCE-PIXEL HISTOGRAM SETTLES IT AND A SPOT-CHECK DOES NOT.** The v574 objective inherited
  the observation "the candidate effect paints a solid dark rectangle" and proposed three causes
  for the defect. There was no defect: the effect is a DROP SHADOW and it was compositing
  correctly. `EffectSpawner_SpawnSimpleMaybe` ORs `nBlitFlags |= 2`, `LocoBitmap::
  RestoreOverlapBlt`'s dispatch case 2 IS `ShadowBlit`, and `ShadowBlit` writes
  `dest = pPalette[srcIndex]` with index 0 = untouched dest and index 1 = the 50%-darkened dest.
  So the decisive measurement is a HISTOGRAM OF THE SOURCE INDICES, not of the output pixels:
  `src0=280 src1=3998 src2=0` over a 69x62 dest (280+3998 = 4278 = 69*62, every pixel accounted
  for) says the sprite is a PURE SHADOW MASK — not one pixel indexes >= 2 — which makes "dark"
  the correct answer and simultaneously makes `palnz=1` (only one non-zero palette entry) a red
  herring rather than the bug it looks like in isolation. **Print the whole input tuple in one
  line** (`flags`, `bConverted`, `w`/`h`, `pPixels`/`pPalette`, the index histogram, `rect`,
  `rectViewport`, the guard mask): here it refuted the source-rect hypothesis in the same breath,
  since `vp=69,0,138,62` is exactly the 2nd of two 69x62 frames in a 138-wide sheet and matches
  the dest 1:1. **And confirm with a CROP, not a pixel spot-check.** Sampling three pixels inside
  the rect against three outside gives ratios (0.33/0.45/0.42) that are consistent with one
  darken, two darkens, or nothing, because the compared pixels sit over different terrain. A
  4x-scaled crop of the region answers it instantly and unambiguously: the grass/dirt boundary
  ran straight THROUGH the darkened area (so the board is compositing, not being overwritten)
  and the silhouette had a handle notch at top-centre (so the index-0 pixels are shaping a real
  toolbox outline, not a rectangle). Same family as #186 — re-measure an inherited observation
  before building a session on it; here the observation was not merely stale but a
  misclassification, and every hypothesis derived from it was unfalsifiable by construction.

- **⚠ #209 — AN ADDRESS IN THE PORT'S STUB LOG IS A PORT-BINARY ADDRESS; RESOLVING IT IN
  `Loco.exe` SILENTLY NAMES THE WRONG FUNCTION.** `loco/stub_calls.log` records the return
  address of each stub hit, and `build/Loco-port.exe` is an independent link with its own layout
  — it is NOT the original's address space, unlike `build/Loco-linked.exe`. Looking `0x4095fc`
  up in Ghidra returns a confident, plausible, completely unrelated answer
  (`ApplSetupWnd::DrawProviderList`, the multiplayer provider list) for a row that was really an
  effect spawn from `AnimEffectObj0x477a90`'s constructor. Nothing flags the mismatch: both are
  real functions at that address in their respective images. **Resolve a stub row by its MANGLED
  SYMBOL instead** — the name carries the view class, so one `grep -rn '<ViewClassName>' src/`
  reaches the call site directly and needs no address mapping at all. Same family as the
  `s_`-label and `IDC_ARROW` traps: the tool answered truthfully, and the plausible reading of
  the answer was wrong.

- **⭐ #210 — WHEN A PORTED ENGINE ASSUMES "THE PRIMARY SURFACE IS THE SCREEN", DO NOT FIGHT THE
  WINDOW MANAGER FOR THE ORIGIN — REDEFINE THE SCREEN, AND DO IT IN THE IMPORT TABLE.** Loco
  assumes its main window's client area IS the desktop: `PopupWndBase`'s software cursor feeds a
  raw `GetCursorPos()` straight in as a primary offset, every front-end screen is a separate
  top-level window placed in screen coordinates, and `Ddraw_BltUpdateRect` paints into the
  primary at each window's own client origin. A borderless `WS_POPUP` at (0,0) made all of that
  true in 1998. Under winemac it is false twice over — the engine's fullscreen style carries
  `WS_MAXIMIZE`, and winemac snaps a maximized window into the WORK AREA below the macOS menu
  bar (measured y=78), so every frame is drawn displaced and clipped. Two rounds of *forcing*
  the window back to screen (0,0) worked and were both wrong answers: they buy a borderless
  window with no titlebar to grab, and the correction has to be re-asserted forever because the
  engine re-styles the window on every display-mode switch (which is why it "did not stick" on
  the way back from the world to the menu).
  The fix that actually holds is to make **"screen coordinates" mean the main window's CLIENT
  coordinates** — a virtual screen that travels with the frame — and to apply the translation at
  the OS boundary rather than at the ~15 engine call sites. **`/D _USER32_` is the whole trick**:
  it makes `WINUSER.H` drop `__declspec(dllimport)`, so a call compiles to `call _GetCursorPos@4`
  instead of `call [__imp__GetCursorPos@4]`, and since the linker searches OBJECTS BEFORE IMPORT
  LIBRARIES (and `USER32.LIB` carries both spellings), one port object that DEFINES those ten
  entry points captures every call site in every TU — with **zero `#ifdef` in `src/`**, and the
  byte-match build untouched by construction because it never compiles that file nor defines the
  macro. Verify the capture in the `.map`: each intercepted name must resolve to `PortWinShim.obj`
  and each deliberately-untranslated one (`GetClientRect`) to `user32`. See
  `port/PortWinShim.cpp`.

- **#211 — A WINDOW MANAGER MOVES A WINDOW WITHOUT EVER CALLING `SetWindowPos`, SO POSITION HAS TO
  BE RE-ASSERTED, NOT SET.** With #210's shim in place the front end still painted at
  `dst=1,-14` instead of `0,0`. The popup was created at exactly the right place (traced:
  asked real 3,120, got 3,120) and no `SetWindowPos` ever touched it — winemac displaced it to
  (4,106) when it was first SHOWN, which is invisible to every intercept. Since
  `Ddraw_BltUpdateRect` targets each window by its OWN client origin, a nudged popup drags the
  whole front end with it. Fix: remember each popup's intended VIRTUAL position at create/move
  time and put it back once per present (`Port_ShimReassertWindows`). Corollary for diagnosis:
  when a create-time re-assert changes nothing but the symptom persists, the mover is not
  something you are intercepting — instrument the value at the CONSUMER, not at the setter.

- **⚠ #212 — A SYNTHETIC CLICK MUST WARP THE REAL POINTER, BECAUSE THE ENGINE CROSS-CHECKS THE
  MOUSE AGAINST THE HOST EVERY FRAME.** `LOCO_PORT_CLICK`'s posted `WM_MOUSEMOVE` +
  `WM_LBUTTONDOWN` reaches `AppWndProc` and sets `bPendingActionAMaybe` correctly, and then
  nothing happens. `PlacementCursorMaybe::OnMouseMoveMaybe` bounds-tests the last mouse position
  against `g_rectAppClientBounds` and asks `WindowFromPoint` who owns it; anything it dislikes
  ends in `SetCursorCapture(0,1,0)`, which clears `bReady` — and `AdvanceAnimFrameMaybe` returns
  immediately when `bReady` is false, so the pending click is never committed. With the physical
  pointer parked outside the window that disarm fires on EVERY tick. Posting `WM_MOUSEMOVE` is
  not enough: the bounds guard reads the posted position but `WindowFromPoint` reads the host.
  `Port_PostClick` now calls `SetCursorPos` first. ⚠ **The trap is that this makes scripted-click
  results depend on where the tester's mouse happens to be** — v575's first successful toolbar
  click was luck (the window sat at (0,0), under the pointer), and the identical script "broke"
  one commit later purely because the window had moved. Never read a scripted-input result as a
  statement about game logic until the pointer is pinned.

- **⭐ #213 — A DECLARED-ONLY PLACEHOLDER VIRTUAL WITH THE WRONG ARITY IS A STACK-CORRUPTION BOMB
  THAT ONLY ARMS WHEN THE SLOT FIRST GETS CALLED.** The vtable dword is emitted from the HEADER
  DECLARATION, not from whatever defines the body. So while `WidgetBase.h`'s slot 19 read
  `virtual void *_v19()` — a placeholder standing in for the honest
  `char(MenuNodeObj0x477568 *, int, int)` — the slot's symbol was a ZERO-ARGUMENT
  `?_v19@WidgetTagObj0x478378@@UAEPAXXZ` that nothing defined, while every call site pushed 12
  bytes through a TU-local vtable probe. The port linked a generated stub whose pop count is
  derived from the MANGLED NAME, so it returned `ret 0` against a caller that had pushed 12: each
  dispatch leaked 12 bytes of ESP, and after a 13-node loop the caller's fixed-size epilogue
  returned into the heap (`EIP` in an allocation, stack full of repeating argument triples).
  Three properties make this class nasty: (a) it is BYTE-NEUTRAL — the match build never links,
  so it costs nothing and no lint fires; (b) it is DORMANT — the slot had never been reached
  because `pMenuListHead` was always NULL, so 70 sessions of running the port proved nothing; and
  (c) the crash surfaces in the CALLER, with no frame belonging to the guilty declaration. **A
  placeholder virtual's ARITY is load-bearing even when its body is unknown — if the honest
  signature is known, spell the arity right even while the name stays a placeholder.** Same
  family as the CLAUDE.md "a vtable slot and a named method at the same address are ONE function"
  rule: a model merely consistent with the bytes is not a model that is true.

- **⭐ #214 — RE-MEASURE EVERY ⛔ "DO NOT RE-RUN" PRICE BEFORE BUILDING ANYTHING ON TOP OF IT;
  v576 CASHED THREE STALE TOLLS IN ONE SESSION.** CLAUDE.md's "a priced-and-withheld toll is a
  measurement with an EXPIRY DATE" corollary is not a rare case, it is the norm, and the notes
  that expire are exactly the ones written most emphatically:
  - `WidgetBase.h` slot 19: priced -1094 B (v506), re-priced -1862 B (v545) with an explicit
    "Do NOT re-run either". Actual v576 cost: **-152 B / -1 func**, one surviving victim
    (`PlacedObjRegistryMaybe::CompareEntriesMaybe`) out of v545's five.
  - `WorldBoardClickView0x411000`'s three methods: priced -124 B twice (2026-07-26 and v407,
    "Do not re-run that measurement"). Actual v576 cost: **+951 B / +1 func** — moving them onto
    the real `WorldBoardPartial` GAINED `0x457ce0` back.
  - `WidgetSlot19VtblProbe` ×2 + `WidgetTagObj0x478378View0x44ef10`: retired for free alongside,
    taking idiom debt 10 -> 6.
  A price is a fact about ONE baseline, and every commit moves the baseline. The cost of
  re-measuring is one `progress.py`; the cost of trusting a stale ⛔ is that the workaround it
  justified becomes permanent — and here all three workarounds were themselves the defects.

- **⭐⭐ #215 — THE TU-LOCAL VIEW STRUCT IS NOT DEBT, IT IS AN UNLINKED CALL, AND THE PORT'S STUB
  LOG IS THE ONLY ORACLE THAT SEES IT.** Four separate defects in v576, all one shape: an address
  transcribed under name N and CALLED under name M, so the emitted call targets a symbol that
  exists nowhere. Nothing in the normal loop can see this — `verify.py`/`match.py` MASK
  RELOCATIONS (the wrong call is byte-identical to the right one), the match build never links,
  `lint_ghidra_sync.py` accepts both an FQN's last `::` component AND its `::`->`_` flattening so
  both spellings pass, and `lint_alias.py`/`lint_desync.py` compare NAMES, which differ. The four:
  | called as | actually defined as | symptom |
  |---|---|---|
  | `_v19@WidgetTagObj0x478378` (0-arg) | `WidgetTagObj0x478378View0x44ef10::TestAndToggle…` | stack corruption, see #213 |
  | `WorldBoardClickView0x411000::ResolveWorldClickMaybe` (+2 siblings) | `WorldBoardClickView0x455d60::…` | clicking to place an object did nothing |
  | `BuildTool_SetAutoCurveConnectModeMaybe` | `AppWindow_BuildTool_SetAutoCurveConnectModeMaybe` | auto-curve mode never changed |
  | `BuildToolButton_ResetAndCloseToolMenu` (free `__fastcall`) | `BuildToolButton::ResetAndCloseToolMenuMaybe` (member) | closing the tool menu never reset it |
  The last is the sharpest: the alias and the definition were **in the same file**, and
  `__fastcall`'s first pointer argument and `__thiscall`'s `this` both ride ECX with neither
  popping — physically the same call, so even a disassembly diff of the call site is clean. ⚠ The
  static sweep that generalizes this is **address-keyed, not name-keyed**: for every stubbed
  symbol, ask whether its ADDRESS already carries a `// FUNCTION: LOCO` marker under a different
  definition name. Name-keyed matching catches only 1 of the 4 above. Until such a lint exists,
  **a boot-to-gameplay run of the port with `loco/stub_calls.log` read afterwards is a required
  step whenever new engine paths become reachable** — every row naming a view/probe spelling is a
  live defect, not a to-do. Resolve a row by its MANGLED SYMBOL, never its address (that address
  is in the port binary's layout).

- **#216 — A DATA-DRIVEN ENGINE'S FILENAME ARITHMETIC IS AS LOAD-BEARING AS ITS STRUCT OFFSETS,
  AND `strcat` IS NOT `strcpy(p + strlen(p) - N, …)`.** `CursorDesc::ParseTokenField` builds the
  shadow/button bitmap path by OVERWRITING the last two characters of `"<prefix><name>.bmp"` —
  `"…bmp"` becomes `"…but"`, and `.but` is a real, populated extension (173 members of
  `loco/rfh.txt`). Transcribed as `strcat(szFullPath, "ut")` it produced `"…bmput"`, a name in no
  archive: the load failed, `pShadowBitmap` stayed NULL, and that is
  `CursorDesc_IsItemAvailableMaybe`'s very first guard — so `InitMenuIconsMaybe` built ZERO menu
  nodes and no build-toolbox icon was clickable, for many sessions. The raw disasm says it
  plainly (`strlen` -> `edx`, `lea eax,[buf]; sub eax,2; add edx,eax`, then the copy): the `-2`
  is explicit and `strcat` cannot produce it. ⭐ **When a decompiled `strcat`/`strcpy` has any
  pointer arithmetic on the destination, re-derive the destination expression from the disasm
  rather than pattern-matching it to the nearest CRT idiom** — and sanity-check the RESULT against
  the asset archive's actual filenames, which is a free oracle this project has sitting in
  `loco/rfh.txt`. The same function also gave up `while (tok[0] != '/' && !eof)` (not an
  `if`-wrapped inner loop with a `break`), a shared-body `||` for the CR/LF trim, and
  `strncpy(dst, &szTok[1], 10)` — index 1, because `getline` picks the line up from just after
  the keyword and `szTok[0]` is the separating space. All four together took 0x424e00 from
  `DIFF(398)` to EXACT at 2032 B.

- **#217 — THE ADDRESS-KEYED UNLINKED-CALL SWEEP NOW EXISTS: `tools/lint_unlinked.py`, and its
  oracle is the ORIGINAL IMAGE rather than any name.** #215 asked for it and explained why
  name-keyed matching cannot work; this is the implementation. For a function whose marker
  address is `A`, a `call rel32` at byte offset `k` in our compiled COMDAT sits at `A+k-1` in the
  original, and the original's own displacement there names the TRUE callee `T`. So a call to an
  **undefined** symbol whose decoded `T` already carries a `// FUNCTION: LOCO 0xT` marker is a
  live unlinked call, not an untranscribed function. Being address-keyed, it is immune to every
  naming question at once — it pairs v576's `BuildToolButton_ResetAndCloseToolMenu` with the
  member `ResetAndCloseToolMenuMaybe` (no name-keyed tool can) exactly as easily as a view-struct
  twin. It needs no Ghidra: `build/*.obj` (run `progress.py` first) plus `loco/Loco.exe`.
  **Two design points that are the difference between a worklist and noise:**
  * ⚠ **Tier findings by whether the CONTAINING function is byte-exact.** Offset `k` maps to
    `A+k-1` only while our bytes agree with the original's up to that point — guaranteed inside an
    EXACT function, merely likely otherwise. The first run reported `??2@YAPAXI@Z` (`operator new`,
    a CRT symbol at 0x465ce0) "targeting" 0x44be50, a constructor: two unrelated `0xE8`s that
    happened to land on the same offset in a drifted function. CONFIRMED (exact) vs UNCONFIRMED
    (not exact, verify by hand) keeps both without conflating them — 43 raw findings became
    18 CONFIRMED + 8 UNCONFIRMED.
  * **Restrict to `link/gen_syms_port.txt`.** "Undefined in `build/*.obj`" also means the whole
    CRT and every import library; the port's stub list is exactly the set with no real provider
    (78 "untranscribed callees" → 5 once filtered).
  Its `--stubs` mode also prints an ADDRESS for every untranscribed callee, which `gen_stubs`
  cannot derive from a mangled name — that is the transcription worklist, previously unavailable.

- **#218 — A "LOAD-BEARING TU-LOCAL VIEW STRUCT" NOTE CAN OUTLIVE BOTH OF ITS OWN PREMISES, AND
  THE VIEW IS USUALLY THE BUG.** v577 retired five view structs; every single one was BOTH
  redundant and a live defect, and in each case the comment defending it was still confidently
  quoting a real measurement taken against a world that no longer existed:
  * `UIResourcesView0x447400` existed because "ANY new method declaration in UIResources.h rotates
    DPlaySessionMgr.cpp" (v340) — but two of its three methods were ALREADY declared on the shared
    `UIResources`, and one was already DEFINED there with a different return type. Re-measured: the
    declaration is byte-FREE. Three TUs' four call sites had been running stubs.
  * `LocoBitmapCaptureView0x457080` existed so the TU would not have to `#include "LocoBitmap.h"`
    — **which that TU already included**, 800 lines above, added later for something else. The
    view bought no insulation whatever, and its `CreateAndFill` call ran a stub that never
    allocated, so `pPixels` stayed garbage and nine `*pOut = 5;` writes faulted. That was the
    user-reported map-save crash: `movb $5,(%ebx)` with EBX = 0x1a8.
  * `WorldBoardRecenterView0x454fa0`'s note said folding the three spellings "is a measured change
    nobody has priced yet" — the honest version, and the right prompt to go price it.
  ⭐ **The generalizable rule: a view struct's justification decays silently, because it is a
  statement about OTHER files.** Includes get added, methods get promoted, victims lose their
  EXACT to something else. Re-read the premise, don't re-read only the price. And note the
  ordering that made this cheap: retiring `LocoBitmapCaptureView0x457080` RESTORED
  `FindNearestObjOfCategoryMaybe` (0x457ce0) the 951 B it had lost earlier in the same session —
  two independent notes had named 0x457ce0 as the victim of two different levers, so paying one
  and refunding the other netted out.

- **#219 — WHEN A FAITHFUL MODEL COSTS BYTES, TAKE THE MODEL (user directive, v577).** Standing
  instruction: prefer the correct source model even when EXACT regresses. `Ddraw_Recenter-
  ViewportOffsetMaybe` had three spellings; a single shared view struct in the header was measured
  byte-FREE and a real `WorldBoardPartial` member cost −484 B, and the member is what landed,
  because 0x454fa0 is `__thiscall` on the board and a view struct is decompilation scaffolding,
  not what the original declared. Likewise `DecorObjMgrMaybe::ActivateEligibleEntriesMaybe` at a
  measured −152 B. **Still measure and still record the number in a comment** — it is real
  information for a later session, and #218 shows those numbers get refunded from unexpected
  directions. What changes is that the price no longer holds a veto. Same spirit as the v560
  "land the body anyway" rule, generalized from bodies to models.
