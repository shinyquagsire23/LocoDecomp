**Write recipes/gotchas (battle-tested):**
- `run_script_inline` = POST JSON `{"code":"..."}`. The code is **Java**, not Python — real
  Java (semicolons, typed locals), NO import statements: fully-qualify every Ghidra class.
  Its response echoes stale compile errors from OLD cached scripts under `~/ghidra_scripts/`
  — only trust output after your own script's `println`s / the `SCRIPT COMPLETED SUCCESSFULLY`
  marker. `currentProgram.save(...)` from WITHIN a script throws (`active transaction`) but
  the mutations still land — call the separate `/save_program` endpoint afterward and verify
  by readback.
  ⚠ **`SymbolTable.getSymbols(String, Namespace)` returns a `java.util.List<Symbol>`, NOT a
  `SymbolIterator`** (confirmed v525 against the live bridge). The `while (it.hasNext())` idiom
  older recipes in this file use fails to COMPILE, and the failure is easy to miss because the
  response is dominated by stale cached errors — iterate with
  `for (ghidra.program.model.symbol.Symbol s : syms)` instead. The single-argument
  `getSymbols(Address)` DOES return a `Symbol[]`, and `getSymbolIterator()` is the iterator one;
  check which overload you are calling.
  Reusable one-liner for moving a function into an EXISTING class/namespace without touching its
  name, calling convention or `this` typing (safer than `set_function_this_type` when no matching
  Structure exists): look the namespace up with the for-each above, then
  `f.setParentNamespace(ns)`.
- Offset-precise structs: `create_struct`'s REST endpoint silently IGNORES field offsets
  (packs from 0). Build via `run_script_inline`: `new StructureDataType(name, 0)`, grow with
  `while (getLength() < size) growStructure(size - getLength());`, then
  `replaceAtOffset(offset, type, len, name, null)` per field, register via
  `dtm.addDataType(s, DataTypeConflictHandler.REPLACE_HANDLER)`. NEVER `recreate_struct`
  (ignores offsets) or `remove_struct_field` (shifts the tail) — `replaceAtOffset` is the
  tool. `modify_struct_field` silently NO-OPs renames (use `setFieldName` in a script);
  `modify_struct_field_type` clobbers the field NAME (restore after). 1-byte bool fields:
  `BooleanDataType` (there is no `BoolDataType`). **`DataTypeComponent.setFieldName(...)` on a
  component whose type is the GENERIC, unsized `undefined` placeholder (Ghidra's own
  "byte not yet analyzed" filler — distinct from a sized placeholder like `undefined1`/
  `undefined4`, which DOES hold a custom name fine) silently NO-OPs: the call returns
  successfully, but a fresh `getComponentAt(offset)` immediately afterward shows the field
  name reverted to `null` (v285, `PeerTrainNode`/`DPlaySessionMgr`/`BuildToolCursorWnd`, 3
  independent confirmations in one batch-rename script).** Give it a real, sized type FIRST via
  `clearComponent(ordinal)` + `replaceAtOffset(offset, new ByteDataType()/new
  SignedWordDataType()/etc., len, name, null)` — THEN the name sticks (verified by an immediate
  readback in the same script). A batch rename script that blindly trusts its own "OK" print
  per field is not sufficient proof — always re-`getComponentAt` and print the field name back
  after a rename pass over previously-`undefined`-typed offsets before trusting it landed.
  Audit for `-BAD-` dangling types after
  struct surgery. **Adding fields to an EXISTING struct (vs. building a new one):** fetching it
  via `dtm.getDataType(path)` returns a live `StructureDB`, not a `StructureDataType` — casting
  to the latter throws `ClassCastException`; cast to the `Structure` INTERFACE instead (both
  implement it), then call `replaceAtOffset` on that directly, no `addDataType` re-registration
  needed (it's already the live instance). ⚠ In `run_script_inline`, `dtm.getDataType(path)`
  called with a plain `String` path (e.g. `"/WorldBoardMaybe"`) throws a bogus
  `"String cannot be converted to SourceArchive"` compile error in this server's script
  harness (an overload-resolution issue, not a real type mismatch) — build an explicit
  `new ghidra.program.model.data.DataTypePath("/", "StructName")` and pass THAT to
  `dtm.getDataType(DataTypePath)` instead; same fix applies to `dtm.getDataTypes(String)`. For
  `dtm.getDataTypes(String)` there is no `DataTypePath` form, so use
  `dtm.findDataTypes(name, someList)` and pick the match by `getLength()` instead (v408, looking
  up `RECT` to retype a struct field).
- **Retrofitting a base-class embedding onto an EXISTING, already-populated struct** (as
  opposed to building fresh): `replaceAtOffset` throws `IllegalArgumentException: Not enough
  undefined bytes...` if its target span contains ANY already-defined components, even ones
  you mean to fully overwrite — it only consumes undefined byte space, unlike building into a
  freshly-grown (all-undefined) struct. Fix: snapshot the ordinals of every component whose
  offset falls in the target span, then `s.clearComponent(ordinal)` each one (iterate ordinals
  in REVERSE so earlier clears don't renumber later ones mid-loop) — `clearComponent` converts
  to undefined bytes WITHOUT shifting/shrinking the structure (unlike `delete()`, which does).
  Only then does `replaceAtOffset(0, baseType, baseLen, "base", null)` succeed.
  Deleting a `GhidraClass` namespace symbol via `removeSymbolSpecial` after
  moving all its members out can leave behind an orphaned 1-byte "PlaceHolder Class Structure"
  data type of the same name (`DataTypeManager.findDataTypes` + check `getDescription()` —
  distinguishes it from a real struct) — sweep for and `dtm.remove(...)` it, else it silently
  blocks reusing that name for the real merged struct.
  ⚠ **That placeholder is worse than an inconvenience: while it exists, it IS what `this` points
  at, and nothing looks wrong.** `get_function_by_address` prints an ordinary `Foo *this` and the
  decompiler accepts it — but the type is 1 byte, so every real field access still comes out as
  raw offset math (found v553: `CarKindDesc`, whose real 1964-byte struct was parked under its
  pre-rename name `Obj0x477610`). `getAllStructures()` does NOT return placeholders, so a sweep
  that uses it reports these as "no struct" rather than "wrong struct" — check
  `getAllDataTypes()` too, which is what `tools/lint_ghidra_types.py` does.
- **`this` is an AUTO parameter — you cannot set its type directly.** `getParameter(0)
  .setDataType(...)` throws `InvalidInputException: Auto-parameter may not be modified`. The
  typing is DERIVED from the class namespace name matching a Structure name, so the fix is always
  to make those two agree (rename the real struct, having removed any placeholder first); the
  parameter then re-resolves on its own, with no per-function edit at all. This is the mechanism
  behind CLAUDE.md's "the Ghidra namespace MUST equal a same-named Structure" rule, and
  `tools/lint_ghidra_types.py` is its enforcement.
- **A renamed class is TWO renames** (namespace + struct) **plus the constructor's own name.**
  A ctor keeps whatever name it had, so after renaming class `Obj0xNNNN` to `Foo` you can be left
  with `Foo::Obj0xNNNN` as the ctor — cosmetically odd and a real desync against `src/`. Grep the
  namespace's members after any class rename, and re-run `tools/lint_ghidra_types.py`.
- `set_function_prototype`/`set_function_this_type` want `function_address`/`this_type` as
  JSON **body** keys (query params silently fail with a generic error). A `__fastcall`
  function needs `set_function_prototype` to `__thiscall` FIRST, before
  `set_function_this_type` will accept it. `prototype` = a full C-style signature string.
  ⚠ On an already-analyzed function, the prototype must repeat the FULL existing parameter
  list verbatim — a shortened one silently demotes the missing params to anonymous stack
  locals (reversible, but always decompile-read the current signature first and change only
  the piece you mean to). ⚠ Converting `__fastcall`→`__thiscall` this way is a DIFFERENT case:
  if the fastcall function's single ECX-passed pointer was previously exposed as an ordinary
  EXPLICIT param (e.g. `void __fastcall F(undefined4 *param_1)`), do NOT repeat that param in
  the new `__thiscall` prototype — thiscall injects its own implicit `this` in ECX, so keeping
  `param_1` too creates a phantom 2nd parameter (silently wrong: the original only ever took
  one argument). Use an empty/`(void)` explicit param list, then `set_function_this_type` to
  type the resulting implicit `this`.
- Retyping a plain (non-`this`) param to a project struct: rewrite the param type in
  `set_function_prototype` alone — sugars the body like `set_function_this_type` but WITHOUT
  the namespace move. Right choice for opcode-handler-shaped free functions taking a state
  pointer, as opposed to true C++ methods.
- ⚠ For a genuine `__thiscall` function, that "plain param" trick does NOT work on the
  IMPLICIT `this` slot — `set_function_prototype`'s param list only ever sets EXPLICIT
  params; adding the struct pointer as a named param just inserts a duplicate parameter
  ahead of the still-`void*` implicit `this` (shifts every other param over by one,
  silently wrong). The only way to type a real `this` is `set_function_this_type`, which
  unavoidably moves the function into the matching struct's namespace — if you want a
  different (subsystem) namespace instead, you must choose one: type `this` and accept the
  struct namespace (`DPlaySessionMgr::Method`), or leave `this` as `void*` and keep the
  subsystem namespace. `f.setParentNamespace(...)` after `set_function_this_type` moves the
  symbol but ALSO reverts the sugaring back to `void*` (namespace ≠ struct name breaks the
  auto-this link) — there is no way to have both a this-typed body AND a non-struct
  namespace on the same thiscall function.
- **A caller whose decompile forwards ITS OWN params verbatim into a callee that Ghidra shows
  taking ZERO explicit params (just `Callee(this)`) is a signal the CALLEE's prototype is wrong,
  not that the real call passes no args** — a callee can't have a variable arg count across call
  sites (the stack-cleanup convention is fixed), so if one caller's decompile assembles/forwards
  several stack values into what looks like a 1-arg call, the callee is under-analyzed. Fix the
  callee's `set_function_prototype` to the real param count/shape (cross-check via
  `objdump`'d `ret 0xNN`), then RE-CHECK every other caller — they'll clean up automatically
  and any that were quietly wrong (e.g. a sibling vtable slot silently zeroing one forwarded
  arg instead of passing it through) becomes visible as a genuine, confirmable behavior
  difference rather than decompiler noise. **A 2nd, distinct tell for the same underlying bug
  (VC5, `FUN_00463600`, v168): a plain free function Ghidra shows as 0-param/`void`, but EVERY
  call site pushes a literal CONSTANT immediately before the `call` with caller-side `add esp,N`
  cleanup AFTER it** — that caller-side cleanup is the cdecl signature of a real argument (a
  genuinely 0-param cdecl call needs no post-call stack fixup at all), even when the different
  call sites push different constants and the callee's own body demonstrably never reads the
  value (confirmed via the callee's raw disasm — no stack read past its own locals). Fix the
  same way (`set_function_prototype` to the real 1-param shape) and pass the real per-site
  constant at each call site; the callee's dead-but-real parameter is worth a `docs/
  engine-bugs.md` "sic" note once that function itself is transcribed. **A 3rd, distinct tell
  (VC5, `WindowBase::CommitRectUpdate` ex-`FUN_00426b70`, v212): a BY-VALUE STRUCT
  argument built via `sub esp,N` + field-by-field stores (no `push` instructions) is a
  separate decompiler blind spot from the two above — the decompiler can fail to recognize
  the reserved stack space as a formal parameter at all, showing a 0-explicit-arg call at
  EVERY site (~20-30, across many unrelated windows/callers in this case) even though
  `ret 0x10` (a fixed non-zero cleanup with no corresponding pushes) is the giveaway a
  same-sized value-type parameter (here a 16-byte `RECT`) is being passed.** Same fix
  (`set_function_prototype`), same "re-check every caller after fixing" payoff — one fix
  cleaned up ~20 call sites' decompile in a single downstream function
  (`EditCardWnd_HandleLButtonDownMaybe`) instantly. **A 4th, distinct tell in the same family:
  a virtual call through an ALREADY-correctly-typed multi-param callee can still under-render
  args at SOME individual call sites while rendering them fully at others** (VC5,
  `WindowBase::RequestModeTransitionFromSourceMaybe`/slot 0xc, v212 — the callee's own
  prototype was already fully 4-param, confirmed via `get_function_by_address`, yet one
  `mov edx,[esi]; call [edx+0xc]`-shaped call site decompiled as `Callee(oneArg)` while a
  structurally similar sibling site in the same function decompiled with all 4 args visible).
  This is NOT a bad-prototype problem (fixing the callee's prototype does nothing, it's
  already right) — treat it as a per-call-site decompiler quirk: don't trust a suspiciously
  short arg list at ANY one virtual-call site just because a sibling call through the same
  slot looks fully rendered elsewhere in the same function; raw-disasm each site
  independently before transcribing.
- **Before accepting a research agent's (or your own) field-ownership hypothesis for offsets
  touched by a `this+N`-shaped function, check whether the receiving struct EMBEDS another
  already-modeled struct at offset 0** — a function passed `WidgetX*` that only touches low
  offsets may really be a method of the embedded base (e.g. `AnimDescRefObj0x477488` embedded
  at offset 0 of `WidgetBaseObj0x4784c8`), not a new set of fields on the outer class. Dump the
  outer struct's components first; if a `base` component covers the touched offsets, this-type
  to the BASE type, not the outer one — the function is then correctly shared/reusable across
  every other embedder too. **Fastest detection method: read the candidate class's own ctor
  literally** — a real base-class relationship shows up as a direct, named call to the base
  type's OWN constructor (e.g. `AnimDescRefObj0x477488::AnimDescRefObj0x477488((AnimDescRefObj0
  x477488*)this,...)` inside `CarNetObj::CarNetObj_Ctor`), not just plausible low-offset field
  overlap — confirmed independently on a SECOND class (`CarNetObj`) embedding
  `AnimDescRefObj0x477488`, closing a whole cluster of previously-"mysterious" fields
  (`pKindDesc`/`bValid`/`bReady`/RECT/name-string) at once by retrofitting the base
  component rather than guessing at each field individually.
- **A field at exactly `this+sizeof(class)` that Ghidra decompiles as `this[1].someMember` is a
  strong signal the class's OWN struct is under-sized by one member, not evidence of a
  genuinely separate adjacent global — even when that byte has no independent static xref of
  its own (VC5, `NetSessionEventQueue::dwReadyBigObjCount`, v208, closed a
  multi-session residual across BOTH `BigObj_CreateAndInsert`/`BigObj_RemoveMaybe`).** A
  prior session had modeled `this+0x18` (one dword past the struct's then-believed 0x18-byte
  end) as a separate global `g_dwReadyBigObjCountMaybe`, reasoning "no static xref exists to
  it, it's only ever reached via dynamic offset math from these two methods" — that reasoning
  is backwards: a genuine struct MEMBER reached only through `this->field` also has no
  independent static xref (nothing ever takes its address directly), so absence of a static
  xref doesn't discriminate between the two hypotheses. What DOES discriminate: a plain
  global-by-name reference ALWAYS compiles to absolute (reloc-masked) addressing under this
  toolchain regardless of memory adjacency to another object, while a real `this->field` access
  compiles to register-relative addressing (`[ebp+0x18]`/`[ecx+0x18]`/etc.) — if the ORIGINAL
  disasm shows register-relative addressing at that offset, it's proof of a genuine member, not
  a hint. Fix: grow the struct by `sizeof(field)`, add the real member, drop the separate
  global. Re-audit any other Ghidra-flagged "adjacent global reached via dynamic offset math"
  note against this same tell before trusting it.
- **Merging several flat `Unk0xNN`/undefined-byte fields into one wider named type (e.g. a
  RECT) is justified once a SECOND, independent consumer reads all 4 sub-fields together as a
  coherent group** (bounds-check, `SetRect`-shaped write, etc.) — one consumer's usage could be
  coincidental sub-field access, but two independent functions agreeing on the same 4-tuple
  read/write pattern is strong evidence it's really one struct field. `clearComponent` the old
  ordinals in reverse, then `replaceAtOffset` the merged type (see struct-surgery recipes above).
- **A scalar struct field (dword/pointer) the ORIGINAL binary places at a non-`sizeof`-aligned
  offset (e.g. an `int` at +0x32) proves the struct is BYTE-PACKED — model it `#pragma pack(1)`
  (VC5/cl 11.00 accepts `#pragma pack(push, 1)` / `#pragma pack(pop)`), not with default
  alignment (VC5, `DPlaySessionMgrProviderSlot` roster records, v229, closed 3 EXACT matches at
  once).** Symptom when NOT packed: the compiler inserts 2-3 padding bytes to align that one
  field, so its store — AND every later field's store — comes out at an offset a small constant
  HIGHER than the original (here every tail field was +2: `[edx+0x34]`/`[edx+0x38]`/`[edx+0x4c]`
  vs. the original's `+0x32`/`+0x36`/`+0x48`), reading as a handful of tiny displacement-byte
  diffs on structurally-identical `mov` instructions (asmscore `align=0 reg_pen=0`, just
  `byte_diff=N`). It ALSO silently grows `sizeof` (the misaligned dword's padding pushes the
  struct past its real size), so an array-of-struct consumer's stride desyncs — here the
  unpacked slot ballooned 0x4c→0x50, which would have regressed `DPlaySessionMgr::
  aProviderSlots[9]`'s existing `BuildPlayerRosterList` match had the packing not restored 0x4c.
  Wire/serialized records (network payloads, file records) are the usual packed culprits; when a
  transcribed field-copy function's only diff is a small constant displacement bump on the stores
  past some interior field, check that field's alignment against the raw disasm's own offset
  before chasing register noise.
- `create_function` needs body key `address`; `rename_function` needs `old_name` alongside
  `function_address`/`new_name`; `set_plate_comment` needs body keys `address` AND `comment`
  (⚠ NOT `plate_comment` despite the endpoint's own name — the wrong key silently returns
  `{"error":"Comment is required"}` rather than a schema-validation error). All three:
  POST JSON body + `program=` query param.
- Globals: use `set_global` (POST body `address`/`type_name`/`name`/`plate_comment`), not
  `rename_data` (rejects Hungarian names unless already pointer-typed). Its linter is strict:
  a `p`-prefixed name needs type `"pointer *"` (literal asterisk); the prefix must match the
  type's WIDTH (`w` for a 2-byte word, not `n`) — on rejection use the message's `suggestion`
  field verbatim instead of guessing again. ⚠ **A global whose type is a raw STRUCT INSTANCE
  (not a pointer to one) has no Hungarian prefix that satisfies the linter** — `dw`/`n`/`p`/
  `sz`/`ab`/`pfn` all assume a scalar/pointer/array, none fit "this IS the object". Tried both
  `g_<Struct>Maybe` and `<Struct>Maybe_Instance`, both rejected (`missing_hungarian_prefix` /
  `missing_g_prefix`). Fix: omit the `name` key entirely and pass only `address`/`type_name`/
  `plate_comment` — the type and comment still apply, the symbol just keeps its `DAT_`-prefixed
  auto-name. (This is likely why other big struct-instance globals like `WorldBoardMaybe`'s own
  `DAT_004aad08` were never given a `g_`-name either — same wall, not an oversight.)
- ⚠ **`set_function_prototype`'s prototype STRING's function-name token is applied as a real
  rename**, not discarded as a syntax placeholder — passing `"void __thiscall FUN(void)"` to
  fix up a return-type/calling-convention actually renames the function to literal `FUN`. Give
  it the function's real current name (or its intended final name) in the prototype string,
  never a placeholder, and if you must use one, follow up with an explicit rename pass
  afterward and verify by readback.
- Renames into class namespaces: `f.getSymbol().setName(...)` + `f.setParentNamespace(...)`.
- **Before a batched naming-promotion rename pass, resolve EVERY candidate through a live
  `SymbolTable.getSymbols(name)` lookup — never trust a doc-cited `src/Foo.h:N` location as the
  owning class.** Two confirmed failure modes (v282, 62-name batch): (1) a shared multi-class
  hierarchy header (e.g. `WidgetBase.h`, which declares `RectFlagObj0x477820`/
  `AnimDescRefObj0x477488`/`WidgetBaseObj0x4784c8`/... in one file) makes the doc-cited file name
  a false signal for the owning class — `SetCategoryIfPrintableMaybe` cited as `WidgetBase.h:88`
  actually belongs to `AnimDescRefObj0x477488`, one of several classes declared in that shared
  file; (2) a lookup returning >1 hit means the literal identifier text is shared by two
  unrelated, independently-hedged functions (e.g. `ShowMaybe` on both `PopupWndBase` and
  `CreditsWnd`) — rename only the confirmed one, and do NOT run a blind repo-wide word-boundary
  replace for that name (it will also silently rewrite the OTHER, unverified function's
  references); pull collision names out of the batch sync script and hand-classify every
  occurrence's context instead.
- **The same live-lookup discipline extends to `[GM]`/`[MM]` (global/struct-member) promotion
  passes, with two extra gotchas beyond the function-naming ones above (v283).** (1)
  `set_global`'s Hungarian-prefix linter can REJECT a rename of a name it (or an earlier session)
  already accepted — `g_guidLocoAppMaybe`→`g_guidLocoApp` was rejected as `missing_hungarian_prefix`
  even though the EXISTING name uses the identical `guid` prefix; the REST endpoint's allowlist is
  narrower than whatever path originally created the symbol. Bypass with a direct
  `Symbol.setName(newName, SourceType.USER_DEFINED)` script call for just that one name rather than
  fighting the linter or picking a worse name. (2) **Promoting a struct MEMBER to a semantically
  meaningful name (not just dropping `Maybe`) can collide with a PRE-EXISTING PARAMETER of the
  same name on a method that reads/writes it — compiles clean, zero diagnostic, and is invisible
  to `tools/progress.py`'s aggregate EXACT/PARTIAL byte totals when the affected function is
  already a non-exact PARTIAL residual** (a correctness regression inside an already-imperfect
  function doesn't move the aggregate counters). Confirmed for real: `GNetManager::
  nUnk0x518Maybe`→`nProtocol` collided with `DPlay_InitConnection(int nProtocol, ...)`'s own
  parameter, turning `nUnk0x518Maybe = nProtocol;` into a no-op `nProtocol = nProtocol;`
  self-assignment. The only reliable catch (extending the existing "diff compiled COMDAT bytes
  per function name" rule from the Naming-conventions section below to promotion passes generally,
  not just member-vs-local shadowing within one function body): before any `[MM]`-class rename
  batch, `git grep -l '#include "TouchedHeader.h"' src/*.cpp` to find every consumer TU, compile
  each via `tools/cc.sh`, and hash every function's `mask(code, relocs, trim_pad(code))` bytes
  (`tools/match.py`'s own helpers) into a `{func_name: hash}` baseline BEFORE the rename+sync;
  re-diff the same hash map after. Any changed hash on a function you didn't mean to touch is the
  tell, regardless of whether that function was ever EXACT. When it fires, the two candidate
  fixes from the Naming-conventions section's shadowing bullet (rename the local, or add `this->`)
  are NOT interchangeable on an already-PARTIAL residual: `this->` can itself shift the function's
  codegen shape (confirmed WORSE on this one), while renaming the shadowing parameter and leaving
  the member reference bare reproduces the original's exact codegen. Prefer renaming the local by
  default, especially on any function that isn't already a clean EXACT match.
  **Confirmed the same technique scales cleanly to a full whole-repo `[UF]` (Unk-field)
  promotion batch, not just single-header `[MM]` passes (v285, ~70 field renames across 15
  headers, all 26 TUs / 365 functions hashed before+after, zero changed hashes)**: whole-repo is
  simpler than per-header scoping when a session's rename set spans many unrelated headers —
  just hash every `src/*.cpp` TU once before the batch and once after, no need to enumerate
  per-header consumer sets individually. The real hazard this pass hit wasn't a local/member
  shadow but a CROSS-CLASS field-name collision: renaming one struct's field to a name a
  DIFFERENT, unrelated struct already uses at its own numeric offset (e.g. `WindowBase::Unk0x40`
  and `PopupWndBase::Unk0x40` are unrelated fields in unrelated classes that happen to share an
  offset) — a blind `\bOldName\b` global rename is safe (old names were unique per-struct here),
  but a NEW name chosen without checking `grep -n "NewName\s*;" src/*.h` first can silently
  collide with an already-existing field of that name elsewhere, requiring line-scoped (not
  global) replacement to fix correctly. Always grep the PROPOSED new name for existing field
  declarations before a batch rename, not just the old name for existing usages.
- **A DIFFERENT, sneakier variant of the same trap (v286, `GameNetRosterResultNode` vs.
  `PeerTrainNode`/wire-message structs, both in the DirectPlay subsystem): the SAME OLD name
  (`bOwnerAMaybe`/`bOwnerBMaybe`) can be a live instance of TWO GENUINELY DIFFERENT already-
  established naming conventions across different structs, not just an accidental offset
  collision.** `PeerTrainNode`/`TrainStateWireMsg`/`TrainOriginWireMsg` all use it as "owner byte
  pair", whose real resolved name is `bOwnerByteA`/`bOwnerByteB` — but `DPlaySessionMgr.h`'s own
  `GameNetRosterResultNode` uses the identical placeholder text for a field that feeds DIRECTLY
  into the ALREADY-RESOLVED sibling `RosterTickRecord::bOwnerA`/`bSlotKey` one struct up in the
  SAME file (a shorter, different established convention) — confirmed via
  `GameNet_BroadcastRosterTick`'s own `rec.bOwnerA = pNode->bOwnerAMaybe;` assignment. A single
  research agent tasked with "resolve this field" can correctly identify EITHER convention
  depending which consumer function it happens to trace first, and a blind repo-wide rename
  picks one winner for every site — wrong for whichever struct doesn't match. The only reliable
  catch: before applying a rename that spans multiple structs sharing one old placeholder name,
  trace each CONSUMING FUNCTION's actual parameter/variable TYPE (not just the identifier text)
  to confirm which struct it's really operating on, and apply the fix per-struct (line-scoped),
  never as one `\bOldName\b` sweep across files whose structs you haven't individually verified
  share the same real convention.
- **A plain FUNCTION rename's grep must cover the WHOLE `src/` tree, not just the file(s) you
  believe declare/define it — a free function or declared-only method can be independently
  re-declared (a 2nd `extern`) and called through a SEPARATE global/instance in a totally
  unrelated TU that a per-file or per-cluster grep misses (v295, naming-debt sweep).** Two
  confirmed misses in one session: (1) `WidgetPickerObj0x477cc8::FUN_00429ef0` renamed via its
  own header/TU (`WidgetPicker.h`/`.cpp`) left a 3rd call site untouched in
  `NetSessionEventQueue.cpp`, reached through a DIFFERENT global (`DAT_004aa818`) of the same
  class — this one silently broke that TU's compile (`error C2039: 'FUN_00429ef0' is not a
  member of...`), caught only because a FULL `progress.py` re-run afterward showed
  `NetSessionEventQueue.cpp COMPILE FAILED` (11 PARTIAL + 1 EXACT func silently dropped from
  the totals — a per-file `tools/cc.sh` re-check on just the files you edited would have missed
  it entirely). (2) A `rand()`-shim (`FUN_00466150`) had its own SEPARATE `extern` declaration
  duplicated in a 2nd TU (`GameNet.cpp`) beyond the one being actively renamed
  (`DPlaySessionMgr.cpp`) — same failure shape, lower stakes (would have compiled fine, just
  left one stale `FUN_` identifier behind). **The fix: `grep -rln "\bOldName\b" src/*.cpp
  src/*.h` — not scoped to "files I think reference this" — is mandatory before considering
  ANY rename done, and a full `tools/progress.py` run (not just the edited file's own
  `tools/cc.sh`) is the real proof no OTHER TU silently broke.** Do this as the LAST step of
  every rename batch, not an afterthought only reached for when something looks wrong.
- Renaming an EXISTING namespace+struct pair takes 3 steps beyond `set_function_this_type`
  (which only retypes `this`/moves NEW functions in): (1) `StructureDataType.setName` on the
  struct; (2) rename the `GhidraClass` symbol (`SymbolTable.getSymbols(oldName,
  globalNamespace)` filtered to `SymbolType.CLASS`, then `Symbol.setName`); (3) rename each
  existing member function via `Function.setName(..., SourceType.USER_DEFINED)` — the REST
  `rename_function` matcher can fail because this server bakes literal
  `"OldClass::OldClass"`-shaped strings into ctor names; `Function.getName()` reveals the
  real string. Sweep `SymbolTable.getAllSymbols(true)` for stale-name stragglers afterward.
- **Ghidra's decompile can mislabel a `__thiscall` call's own `this` ARGUMENT at one specific
  call site while getting a nearly-identical sibling call right** (VC5-confirmed,
  `SaveBoardLayoutMaybe`'s `ThumbnailBmp::WriteObjectRecordMaybe` call vs. its sibling
  `WriteTrainRecordMaybe` call in the same function) — this isn't the usual cosmetic
  scheduling/naming noise, the printed pseudocode literally showed the wrong (wrong-sized,
  implausible) local as `this` for one call while a structurally-identical adjacent call
  decompiled correctly. If a decompiled call's `this` argument doesn't look like a plausible
  object of the right size/shape, don't trust it even when a sibling call looks fine — cross-
  check the raw `objdump` disasm at that exact call site (look for the `lea ecx,[...]`/`mov
  ecx,...` immediately before the `call`) before modeling the call.
- **Finding real callers of a virtual method:** `get_xrefs_to` returns only the vtable DATA
  xref, and a decompile-and-grep sweep never resolves indirect calls to names. What works:
  `objdump -d -M intel loco/Loco.exe` the whole `.text` once, scan for the idiom
  `mov REG,[REG2]` followed (within ~15 insns) by `call DWORD PTR [REG+SLOT]` across ALL GP
  registers — a regex matching only `e?x` registers silently misses the common `esi`/`edi`
  hits. Raw slot offsets are noisy (COM vtables alias low offsets constantly); filter hits by
  the containing function's address range vs the class's known method span.
- **Finding the writer of a struct FIELD:** there is no "xrefs to `this+0xNNN`" query. Grep
  the same full-`.text` objdump for the literal store shape at that offset across every GP
  register (e.g. `grep "0xd50\]" dump.asm | grep -i "mov.*BYTE"`); immediate stores = "set"
  candidates, reg stores after `xor reg,reg` = "clear" candidates; confirm each hit's
  containing function really operates on the target class (offsets alias unrelated structs).
- **A function whose prologue reuses `ebp` as a SECOND `this`-holding register instead of a
  real frame pointer (`sub esp,N; push ebx; push ebp; mov ebp,ecx` — no `push ebp; mov
  ebp,esp`) can make Ghidra's decompiler misplace/unbind an incoming STACK parameter it reads
  deep in the function body, even after `set_function_prototype` correctly sets the
  parameter's storage (VC5-confirmed, `EditCardWnd_ResetAndBeginEditMaybe`/0x416b80, v137).**
  Ground-truth the param count/type first via raw disasm (`ret 0xN` cleanup + every caller's
  own single/multi `push` before the `call`, per the "Finding real callers" recipe above), fix
  the prototype, then script-verify the new `Parameter`'s `VariableStorage` is the standard
  `Stack[0x4]`-style entry-relative location (it usually already is, correctly) — if the
  decompile TEXT still shows the read as a separate unbound `xStack_NN` local instead of the
  named param, that's the decompiler's own net-esp tracking getting confused by the odd
  ebp-as-`this` idiom, not a wrong prototype. Don't keep re-issuing `set_function_prototype`
  calls chasing it; the storage is already right — just note in the plate comment that the
  unbound local IS the real parameter, and transcribe treating it as such.
- **Ghidra-missed function gaps are usually a plain analyzer miss, not a jump table.**
  `objdump -d -M intel --start-address=X --stop-address=Y loco/Loco.exe` (binutils works
  directly on the gitignored EXE) to find where real code resumes after the previous `ret` +
  `0x90` padding, then `create_function` there — Ghidra finds the correct extent on its own.
- **Decoding an inline GUID constant needs a raw file-byte read** — the decompiler splits
  16-byte literals across unlabeled stack dwords in unknown order. Parse the PE section
  headers by hand for a VA→file-offset formula, read the 16 bytes, assemble as
  DWORD-WORD-WORD-BYTE[8]. (`objdump -s` does not reliably slice PE sections by VA.) This is
  how the DirectPlay SP GUIDs (TCPIP/IPX) were pinned. **Same VA→file-offset technique reads
  any `.rdata` FPU double constant** the decompiler shows only as `DAT_00NNNNNN` in a
  `fmul`/`fcomp`/etc — a Python one-liner (`struct.unpack('<d', data[off:off+8])`) turns them
  back into readable values (e.g. `57.29577950560105`=`180/π`, `0.3555...`=`128/360`) and
  often reveals the function's real intent (angle scaling, unit conversion) in one shot.
- **Ghidra's decompile of an x87-heavy leaf can silently DROP an `fmul`/scale instruction from
  the pseudocode** (confirmed on `CarNetObj::ComputeHeadingAngleMaybe`/ex-`0x40df80`: the
  decompiled `fpatan(...)` call had no visible follow-up multiply, but the raw disasm showed
  `fmul QWORD PTR [const]` right after it, changing radians to a fully different quantized
  unit). Never trust decompiled FPU pseudocode as complete on a function doing real
  trigonometry/scaling — always cross-check the raw `objdump` disasm for the full FPU
  instruction sequence before concluding the algorithm.
- **Pinning a message handler's WM_* identity: cross-check the SAME vtable slot in a
  different, already-confirmed sibling class.** All `WindowBase`-derived classes share the
  dispatch layout, so slot N is the same message class-wide (e.g. slot 0x38 =
  `WM_LBUTTONDOWN`). Use before guessing WM_* ids from behavior alone.
- **A ctor's `resourceId`-shaped param can be a `LoadStringA` id naming the window's own
  `WNDCLASSA` class** — if it threads into `LoadStringA` right before `RegisterClassA`, the
  loaded string IS the original class name. Extract the raw `RT_STRING` table (resource id
  32) for the id range: free, authoritative class names — check FIRST for any new
  `WindowBase`/`PopupWndBase`-shaped singleton.
- **Comparing vtables: match same-offset-to-same-offset, never by function-pointer value.**
  MSVC points multiple slots at one shared no-op/trivial body, so "where does this function
  appear in that table" gives wrong slot conclusions. Read dwords per slot side by side,
  resolving each through `FunctionManager.getFunctionAt`.
- **`WindowBase`'s documented "shared vtable slot conventions" list (WM_LBUTTONDOWN=0x38 etc.,
  see `docs/subsystems.md`) is NOT exhaustive — a slot absent from that list can hold a totally
  DIFFERENT, unrelated override per sibling class, even when the BASE's own default at that slot
  has a real, already-named role.** Confirmed (v166): slot 4's `WindowBase` default is the named
  `EndModalCaptureMaybe`, but `AlbumCardWnd` overrides slot 4 with an entirely unrelated own
  method (`0x402660`) — calling it generically (through the vtable) must NOT be modeled as
  `this->EndModalCaptureMaybe()` (a non-virtual, statically-bound call to the WRONG body); it
  needs a real virtual dispatch through the ACTUAL derived vtable. Meanwhile slot 0xc
  (`RequestModeTransitionFromSourceMaybe`) is genuinely NOT overridden by the same class. Always
  raw-read BOTH the base's and the candidate subclass's own vtable dwords at a given slot before
  assuming either meaning transfers. Once a slot's position is ground-truthed, model it as a REAL
  `virtual` on the class (declare dummies for the unidentified slots before it — declaration order
  IS vtable order) and call it by name: the 2026-07-21 cleanup retired the old per-TU throwaway
  padded-dummy-virtuals probe structs for `this` casts (a `((ProbeT*)this)` cast is now a
  lint_idiom class-F finding, same as a raw slot dispatch). Probes remain legitimate ONLY for
  dispatch through a NON-`this` pointer the real class can't be reached through — e.g.
  `WindowBase_RouteMessage`'s `pWnd` (declaring the message handlers would flip every derived
  class's same-named members to overrides), a wholly DIFFERENT opaque singleton's own vtable
  slot (e.g. `MailWnd`'s slot 8 from `AlbumCardWnd.cpp`), or the known-concrete-global
  devirtualization case below.
- **MSVC 5.0 DEVIRTUALIZES a virtual call on a known-concrete GLOBAL object into a DIRECT call —
  to reproduce the original's indirect `call [vtbl+off]` dispatch on a global singleton, declare
  the global as a NON-polymorphic view (no virtuals of its own) and reinterpret-cast a pointer to
  it to a separate padded-vtable probe struct for the virtual call (VC5, `DPlaySessionMgr::
  SetUiModeAndNotifyWidgets`/0x440820, v244, EXACT).** The widget-list singleton at
  `DAT_004a9994` is a value-type global collection (GetItem @ vtbl+0x20, count @ +0xc). Modeling
  it as a struct that DECLARES the virtual `GetItem` on its own type let MSVC prove the exact
  dynamic type of the global and emit a direct `call GetItem` (masked-reloc) instead of the
  original's `mov eax,[obj]; call [eax+0x20]` — the tell in `asmscore.py --dump` is a lone `S`
  (call-target) row showing `call [eax+0x20]` on the left vs `call 0xNN` (a direct rel32) on the
  right. Fix: give the global a non-polymorphic struct (`char pad0x0[0xc]; uint nCount;` — the
  count field still reads as absolute `ds:0x...`), plus a SEPARATE probe struct carrying the
  virtual slot, and call `((Probe*)&g_global)->GetItemImpl(i)`. The reinterpret_cast breaks the
  compiler's type-provenance so it CAN'T devirtualize and dispatches through `[obj+0]`'s vtable —
  same mechanism as the per-TU item probe above, just applied to a global's OWN dispatch rather
  than an item reached through a runtime pointer. (A member-subobject or runtime-pointer virtual
  call does NOT hit this — only a known-concrete global does; the count field read stays a plain
  direct access on the non-polymorphic view.) **Inverse lever (v323): if the ORIGINAL has the
  indirect call but your model already carries the virtual on the class, a plain LOCAL POINTER
  COPY (`MgrT *p = &g_mgr; p->Virtual(...)`) breaks the exact-dynamic-type proof just as well —
  no probe struct needed (`WorldActionCursor::HandleMenuCommandMaybe`'s decor-category mgr
  call).**
- **Dumping a vtable by reading N raw dwords from its address can silently walk into the NEXT
  class's vtable — the linker packs unrelated small vtables back-to-back in `.rdata` with no
  gap.** Ground truth (v114): `SoundBankEntry`'s vtable at `0x478278` was earlier believed to
  have 16 slots (docs kept a stale "14 unknown slots, maybe a shared base with `ThumbnailBmp`"
  note) — really it has exactly 2 (dtor + one method); the other "14 slots" were 3 wholly
  unrelated classes' own short vtables (`UiIconListItem`, `LockableMaybe`,
  `BuildToolButton`) that the linker happened to place immediately after it. Don't trust a
  fixed-size raw memory dump as a vtable's true extent — confirm EACH dword-aligned candidate
  slot address independently via `get_xrefs_to`: a genuine per-class vtable's OWN start has
  incoming DATA xrefs from that class's ctor/dtor (the vtable-store instruction); an address one
  or more dwords past the real end has none (it's sitting inside someone else's table, target of
  no ctor/dtor at all). The first dword-aligned address with zero such xrefs is the table's true
  end.
- **A function with a real vtable-slot DATA xref can still be the wrong this-typing target —
  check whether it actually reads its implicit `this` before typing it into that class.** A
  genuine virtual call passes `this` in ECX regardless of whether the callee uses it; some
  small dispatch/predicate helpers are installed at a real slot in class X's vtable purely for
  polymorphic dispatch convenience but never touch `this`, and are ALSO called directly
  elsewhere with a real object explicitly passed as the first STACK argument. Tell these apart
  from `objdump`: a genuine method reads `this` (register/stack arg 0 under the calling
  convention) somewhere in its body; a this-discarding shared helper's raw disasm shows only
  explicit stack args used (e.g. `ret 0xc` popping exactly the named param count, first real
  read at `[esp+8]` not an implicit slot) and is usually structurally identical to an
  already-known shared stub (confirmed twice: `Widget::ConstFalsePredicateStubMaybe`/
  `ConstTruePredicateStubMaybe`, and `Widget::TestAndToggleMenuNodeHoverMaybe` — the latter
  installed as `WidgetTagObj0x478378`'s real slot 19 yet boxed as a free `Widget::` helper
  instead, since this-typing it would have been silently wrong: the "this" it'd expose is
  never the real argument, the explicit first stack param is).
- **A decompiled call site showing an explicit `(SomeClass *)param_1` cast is a live signal the
  CALLEE's own `this`-type is wrong, not a benign decompiler artifact — check the callee's
  actual callers before trusting anything derived from its body (VC5, `EditCardWnd_
  BuildPreviewCanvasBMaybe`/`0x4216f0`, v135).** An earlier pass had this-typed `0x4216f0` as an
  `EditCardWnd` method (plausible: it built a 1280×1024 canvas, matching `EditCardWnd.h`'s own
  "help/certificate canvas" framing) and then used its `this->pPreviewCanvasBMaybe = ...`
  write as PROOF that an `EditCardWnd` struct field of the same name held a real `LocoBitmap*`
  — which in turn was used to diagnose a whole "engine bug" (a vtable call through a slot that
  didn't belong to that supposedly-real `LocoBitmap*`). The actual chain was broken at the
  root: `0x4216f0`'s ONLY caller (`SplashWnd::FUN_00421500`, itself already correctly
  namespaced) unambiguously passed a `SplashWnd*`, and Ghidra's own independently-built
  `SplashWnd` struct already named that exact offset `pDrawTargetMaybe` — the "proof" function
  had nothing to do with `EditCardWnd` at all, just a superficially similar size/shape. Once
  retyped to `SplashWnd*`, `EditCardWnd`'s OWN same-offset field turned out to hold a
  `BigObj*`/`CursorDesc*` (populated by a DIFFERENT, genuinely-`EditCardWnd` sibling
  function), and the "wrong vtable slot" was simply that class's real, inherited
  `ReleaseRefMaybe` — no bug at all. General rule: before trusting a sibling function's field
  writes as evidence for another class's struct layout, verify that sibling's OWN `this`-type
  against its REAL callers (not just its plausibility) — an explicit cast at any of its call
  sites is exactly the tell that the this-typing doesn't match the caller's real type.
- **A same-address-range namespace-boxing gap-fill can plausibly land a function in the WRONG
  sibling class when the two classes are similarly-shaped — check the function's own memory-
  ACCESS-SIZE evidence (a `memcpy`/`WriteFile`/etc. size argument, or the highest field offset
  touched) against each candidate class's real, `operator-new`-confirmed allocation size before
  trusting the address-boxing alone (VC5, `CarNetStateAlt`→`CarNetState`,
  `PostBag_SaveCardToCategoryMaybe`, v150).** `FUN_00442a70` sat in the address gap between
  `CarNetStateAlt::CreateAndFill`/`InitMaybe` (0x442a10/0x442ec0) — a plausible, in-range
  address-boxing candidate — and had been namespace-boxed there. But its own `WriteFile` call
  wrote `sizeof-4` bytes from `this+4`, spanning to `this+0x39c` — which OVERFLOWS
  `CarNetStateAlt`'s real, `operator-new`-pinned 0x390-byte allocation by 0xc bytes, while
  landing EXACTLY on sibling class `CarNetState`'s own confirmed 0x39c-byte size. Retyping
  `this` to `CarNetState*` immediately turned Ghidra's raw `*(undefined4*)(this+0xc)`
  offset math into clean named-field access with an exact field-by-field size match (920 bytes
  = 0x398), closing the case. General rule: gap-fill address-boxing is a hypothesis about WHICH
  TU a function lives in, not proof of which CLASS it's a method of when two candidate classes
  are both plausible/nearby — cross-check the function's own largest touched offset or
  bulk-copy/write size against each candidate's real allocation size, not just its address
  proximity to known ctors.
- **Two genuinely different classes' scalar-deleting dtors can share ONE physical address** —
  seen between `RectFlagObj0x477820` and the unrelated `VtblFlagStub0x436a00` (both trivial,
  byte-identical generated code, one shared `0x412600` body). Don't conclude "same class" from
  a shared dtor/scalar-dtor address alone; confirm via ctor address + struct size instead —
  those never collided in this case, closing it as coincidental code sharing, not a duplicate.
- **A WndProc-shaped function's identity comes from its `SetWindowLongA(hwnd,-4,func)`
  install site, not its body** — subclass procs are near-identical forward-or-CallWindowProc
  shells; only the install site says which window they belong to.
- **Disambiguating N similar-sized concrete ctors of one class family: read the
  ALLOCATOR/dispatch function's exact `new_alloc(N)` sizes and branch conditions, not just
  the ctors' own field-init bodies.** `BigObj::BigObj_CreateAndInsert`'s dispatch gave
  the true leaf sizes AND which `IsType0x63aInSet*` predicate picks each one in one read,
  vs. inferring it indirectly from field overlaps across the ctors. Also: a base ctor
  directly called by the dispatcher (not just chained by sibling ctors) is a LEAF class in
  its own right, not merely a shared helper — check the dispatcher for direct calls before
  assuming a function is "only" an intermediate base.