// SMOKE-TEST SCAFFOLDING ONLY -- NOT part of the byte-match sources.
//
// Hand-written stubs for symbols whose real definitions live in app TUs that
// have not been transcribed yet (or are spelled differently in src/ for
// byte-match reasons). Everything in this file exists solely so that
// tools/link_check.sh can produce a runnable Loco-linked.exe; as the real TUs
// get transcribed these stubs shrink away. See docs/ and tools/link_check.sh.
//
// What is deliberately NOT here: the ~1000 declared-but-never-defined C++
// members/globals (declared-only virtuals, Partial-class methods, VtblProbe
// byproducts). Those are emitted as raw COFF symbols by link/gen_stubs.py
// instead -- writing matching C++ declarations for all of them is impractical.

#include <windows.h>

// --- Generated-stub call reporting --------------------------------------------
// Every code stub link/gen_stubs.py emits calls this before returning 0, passing
// its own mangled name and its caller's return address. Without it a stubbed
// build just dies somewhere with no diagnostics: the stubs are silent, so the
// first symptom is a wild pointer or a missing window several calls later, and
// there is nothing in the log tying that back to a missing body. With it, the
// tail of link/stub_calls.log names the last function the app wanted and where
// it was called from -- which is exactly the burn-down worklist, in priority
// order, discovered rather than guessed.
//
// Writes straight through kernel32 (no CRT buffering) because the interesting
// runs are the ones that end in an abort, where a buffered tail is lost.
static HANDLE g_hStubLog = INVALID_HANDLE_VALUE;
static long g_nStubCalls = 0;

extern "C" void __cdecl Stub_Report(const char *pszName, void *pCaller) {
    char szLine[512];
    int n = 0;
    unsigned int addr = (unsigned int)pCaller;

    if (g_hStubLog == INVALID_HANDLE_VALUE) {
        g_hStubLog = CreateFileA("stub_calls.log", GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (g_hStubLog == INVALID_HANDLE_VALUE)
            return;
    }
    g_nStubCalls++;

    // "NNNN <caller-va> <name>\n", hand-formatted: wsprintf pulls in USER32 and
    // the CRT's own printf is exactly the buffering we are avoiding.
    {
        long c = g_nStubCalls;
        char tmp[16];
        int i = 0;
        do { tmp[i++] = (char)('0' + (c % 10)); c /= 10; } while (c);
        while (i > 0) szLine[n++] = tmp[--i];
    }
    szLine[n++] = ' ';
    for (int shift = 28; shift >= 0; shift -= 4)
        szLine[n++] = "0123456789abcdef"[(addr >> shift) & 0xf];
    szLine[n++] = ' ';
    for (const char *p = pszName; *p && n < (int)sizeof(szLine) - 2; p++)
        szLine[n++] = *p;
    szLine[n++] = '\n';

    DWORD written = 0;
    WriteFile(g_hStubLog, szLine, (DWORD)n, &written, NULL);
    FlushFileBuffers(g_hStubLog);
}

// --- CRT renames -------------------------------------------------------------
// src/ declares the CRT allocator as extern "C" _malloc/_free (see
// src/DSoundChannel.h:45-49: "the real CRT malloc/free, byte-identical to
// LIBCMT's malloc.obj/free.obj, confirmed v319"). extern "C" decoration adds
// one leading underscore, so the objs reference symbols stored as
// __malloc/__free -- i.e. C names "_malloc"/"_free".
//
// ⚠ These forward to LIBCMT's OWN malloc/free, and must keep doing so. An
// earlier version went straight to HeapAlloc/HeapFree(GetProcessHeap()) on the
// theory that defining __malloc/__free here "shadows LIBCMT's malloc.obj /
// free.obj (which then never get pulled)" and that calling malloc would
// recurse. Both halves of that are false: __malloc and _malloc are DIFFERENT
// symbols, so nothing shadows and nothing recurses -- link/Loco-port.map listed
// stubs_port.obj's __malloc/__free AND LIBCMT:malloc.obj/free.obj side by side.
// The result was TWO live allocators over two different wine heaps (the process
// heap here, LIBCMT's HeapCreate'd _crtheap for operator new / delete), when the
// original has exactly one: 0x4673c0 tail-calls __nh_malloc, i.e. src/'s
// _malloc IS operator new's allocator. Any buffer that crosses between the two
// spellings -- and src/ mixes them freely, exactly as the original could afford
// to -- then allocates out of one heap and frees into the other.
extern "C" void *malloc(unsigned int nSize);
extern "C" void free(void *p);

// --- Allocation watchdog (scaffolding) ----------------------------------------
// A run that ends in heap corruption gives you a detonation site, never the bad
// write. A bogus SIZE, though, is visible at the moment it is requested -- and
// wine already proved one exists here ("allocate_virtual_memory out of memory
// for allocation, base 0x0 size 964f0000", i.e. a 2.5 GB request). Reporting
// every oversized allocation with its caller's return address turns that into an
// address to resolve against link/Loco-port.map. Rides on Stub_Report so the hits
// interleave with the stub trace in one ordered log.
#define LOCO_BIG_ALLOC 0x800000u  // 8 MB -- nothing this game legitimately allocates

static void Alloc_ReportBig(const char *pszWhat, unsigned int nSize, void *pCaller) {
    char szName[64];
    int n = 0;
    for (const char *p = pszWhat; *p; p++) szName[n++] = *p;
    szName[n++] = '(';
    for (int shift = 28; shift >= 0; shift -= 4)
        szName[n++] = "0123456789abcdef"[(nSize >> shift) & 0xf];
    szName[n++] = ')';
    szName[n] = 0;
    Stub_Report(szName, pCaller);
}

// --- Object watch list (scaffolding) ------------------------------------------
// Discriminates the two ways a live object's vtable pointer turns into pixel data:
// something blitted PAST its own buffer into this block, or this block was FREED and
// handed straight back out as a bitmap buffer while an HWND still pointed at it. Those
// need opposite fixes and look identical from the crash site, so watch the allocator:
// a delete of a watched address proves the second, and so does a new that lands on one.
// Silence from both, with the vptr still clobbered, proves the first.
static void *g_apWatch[8];
static unsigned int g_anWatchSize[8];
static int g_nWatch = 0;

extern "C" void Port_WatchObject(void *p, unsigned int nSize) {
    if (g_nWatch < 8 && p != 0) {
        g_apWatch[g_nWatch] = p;
        g_anWatchSize[g_nWatch] = nSize;
        g_nWatch++;
    }
}

// Blit-range form: does the region this blit is about to write cover a watched object?
// Called from the LocoBitmap blit family, which is the only code in the process that writes
// wide runs of 16bpp pixels, and reports the blit BY NAME plus its own caller.
extern "C" void *Port_WatchedInRange(void *pBase, unsigned int nBytes) {
    unsigned int a = (unsigned int)pBase;
    unsigned int b = a + nBytes;
    for (int i = 0; i < g_nWatch; i++) {
        unsigned int wa = (unsigned int)g_apWatch[i];
        if (a <= wa && wa < b) return g_apWatch[i];
    }
    return 0;
}

static void Watch_Check(const char *pszWhat, void *p, unsigned int nSize, void *pCaller) {
    unsigned int a = (unsigned int)p;
    unsigned int b = a + nSize;
    for (int i = 0; i < g_nWatch; i++) {
        unsigned int wa = (unsigned int)g_apWatch[i];
        unsigned int wb = wa + g_anWatchSize[i];
        if (a < wb && wa < b) Alloc_ReportBig(pszWhat, a, pCaller);
    }
}

extern "C" void *_malloc(unsigned int nSize) {
    void *pCaller;
    __asm {
        mov eax, dword ptr [ebp+4]
        mov pCaller, eax
    }
    if (nSize >= LOCO_BIG_ALLOC) Alloc_ReportBig("malloc", nSize, pCaller);
    return malloc(nSize);
}
extern "C" void _free(void *p) { if (p) free(p); }

// Shadows LIBCMT:new.obj/delete.obj -- the ONLY way to see the size argument of
// every `new` in the app, since src/ calls ??2@YAPAXI@Z directly. Drops the
// new-handler retry loop LIBCMT's own version has, which this build never sets.
void *__cdecl operator new(unsigned int nSize) {
    void *pCaller;
    __asm {
        mov eax, dword ptr [ebp+4]
        mov pCaller, eax
    }
    if (nSize >= LOCO_BIG_ALLOC) Alloc_ReportBig("new", nSize, pCaller);
    void *p = malloc(nSize);
    Watch_Check("NEW-ON-WATCHED", p, nSize, pCaller);
    return p;
}
void __cdecl operator delete(void *p) {
    void *pCaller;
    __asm {
        mov eax, dword ptr [ebp+4]
        mov pCaller, eax
    }
    if (p) {
        Watch_Check("DELETE-WATCHED", p, 4, pCaller);
        free(p);
    }
}

// --- Hand-rolled generic array helpers (untranscribed app utility TU) --------
// src/DSound.cpp calls these; the originals are FUN_004671e0 (construct) /
// FUN_00467280 (destruct) per the long comment in src/DSound.cpp. The exact
// mangled name of the construct helper embeds a pointer-to-member-function
// type, which trips a VC5 LINK 5.10 undecorate crash (access violation in
// ReadSymbolTable) the moment it would print LNK2001 for it -- so this stub
// is also what un-wedges the whole link. The class shape below reproduces
// the mangling ?ArrayConstructWithIteratorMaybe@@YGPAXPAXII0P8DSoundChannel@@AEXXZ@Z.
class DSoundChannel { public: void Release(); };
typedef void (DSoundChannel::*DSoundChannelMethodMaybe)();

extern "C" void DSoundChannel_ConstructThunkMaybe(DSoundChannel *pChannel);

// Smoke stub: does NOT invoke the ctor/dtor callbacks (the ctor thunk symbol
// itself resolves to generated-stub garbage in the smoke exe), just zeroes
// the elements and returns the array base like the original helper does.
void *__stdcall ArrayConstructWithIteratorMaybe(void *pArray, unsigned int elemSize, unsigned int count, void *pCtorThunk, DSoundChannelMethodMaybe pDtorThunk) {
    (void)pCtorThunk; (void)pDtorThunk;
    unsigned char *p = (unsigned char *)pArray;
    for (unsigned int i = 0; i < count * elemSize; i++) p[i] = 0;
    return pArray;
}

extern "C" void *ArrayDestructWithIteratorMaybe(void *pArray, unsigned int elemSize, unsigned int count, DSoundChannelMethodMaybe pDtorThunk) {
    (void)elemSize; (void)count; (void)pDtorThunk;
    return pArray;
}

// Note: src/GameNet.cpp declares the same helper __stdcall (a byte-match
// stand-in declaration), so its obj references _ArrayDestructWithIteratorMaybe@16.
// VC5 LINK has no /alternatename, so that spelling is covered by a generated
// stub instead (link/gen_syms_extra.txt).

// --- EasterEgg/ScriptEventLoader view-spelling forwarders ---------------------
// src/ScriptEventLoader.cpp holds the real, fully transcribed bodies for the DAT_004a99b0
// singleton, but the class is TU-local there, so four other TUs reach the same object through
// methods-only VIEW STRUCTS of their own -- and each view mangles its methods under its own
// class name, so every one of those calls resolved to a generated `xor eax,eax; ret N` stub.
// That is what aborted the world load: LoadTimeEventScriptsMaybe's real body returns 1
// unconditionally, the stub returned 0, and src/LoadingScreen.cpp answers a zero there with
// PostMessage(WM_CLOSE, wParam=3) -- i.e. the "An error occurred while loading" box.
//
// The view that lives in a shared header (EasterEggMgrAppView0x406ba0) is forwarded in
// src/ScriptEventLoader.cpp directly. The three TU-LOCAL views are forwarded HERE, for the same
// reason `class DSoundChannel` above is here: redeclaring them in src/ would be a real
// duplicate-class definition and six new lint_idiom class-E findings, whereas this file exists
// precisely to reproduce a mangling without owning a model. The structs below carry no fields,
// and `this` reaches the bridge unchanged (__thiscall passes it in ecx whatever the class).
// Declared in the shared port header (relative path: this file is compiled without /I port by
// tools/link_check.sh, the same reason link/init_globals.cpp spells its own includes this way),
// so these and their definitions in src/ScriptEventLoader.cpp cannot desync.
#include "../port/PortGlobalCtors.h"

struct EasterEggMgrMaybe { // src/UIResources.cpp's view
    void LoadUnlockTableMaybe(const char *pszIniBaseName);
    void ApplySeasonalUnlocksMaybe();
};
void EasterEggMgrMaybe::LoadUnlockTableMaybe(const char *pszIniBaseName) {
    Port_EE_LoadUnlockTable(this, pszIniBaseName);
}
void EasterEggMgrMaybe::ApplySeasonalUnlocksMaybe() { Port_EE_ApplySeasonalUnlocks(this); }

struct EasterEggMgrIdlePumpView0x42cc60 { // src/WorldActionCursor.cpp's view
    void TickWorldIdleMaybe();
};
void EasterEggMgrIdlePumpView0x42cc60::TickWorldIdleMaybe() { Port_EE_TickWorldIdle(this); }

struct EasterEggMgrWndProcView0x4618c0 { // src/Main.cpp's view
    void RestoreExpiredActorDescMaybe(unsigned int wParam);
};
void EasterEggMgrWndProcView0x4618c0::RestoreExpiredActorDescMaybe(unsigned int wParam) {
    Port_EE_RestoreExpiredActorDesc(this, (void *)wParam);
}

// --- Entry glue ---------------------------------------------------------------
// The original entry is WinMainCRTStartup (0x4689e0) -> WinMain -> the app's
// real WinMain body, transcribed as LocoWinMain (0x462e90) in src/Main.cpp.
// Provide the CRT-visible WinMain and forward.
int __stdcall LocoWinMain(void *hInstance, void *hPrevInstance, char *lpCmdLine, int nCmdShow);

// g_UIResources is one of the ~160 globals no transcribed TU defines, so link/gen_stubs.py
// hands it a zeroed slot in the .bss mirror -- and a zeroed slot means its CONSTRUCTOR NEVER
// RUNS. That is not a cosmetic gap: UIResources::UIResources (0x445f70) is the only thing that
// identity-fills m_pKindSlotPtrsMaybe, the redirect table TileKind_GetOrLoadDescriptor reaches
// EVERY descriptor through. With it zeroed the registry answers NULL to every lookup no matter
// how well the descriptors themselves loaded, and SplashWnd::EnsureArtLoaded dereferenced the
// NULL. Construct it in place here rather than defining the object in src/: the mirror
// deliberately preserves real ALIASING, and g_RFIndex IS g_UIResources+0x18 (three TUs read the
// archive through it), which a separate real definition would silently break.
//
// ⚠ Generalize before adding more: any stubbed global whose real type has a constructor has
// this problem. This is the only one the boot path has needed so far.
//
// The construction itself lives in link/init_globals.cpp, not here: this file defines its own
// local `class DSoundChannel` to reproduce one helper's exact mangling, which collides with the
// real one that src/UIResources.h drags in.
extern "C" void Smoke_ConstructGlobals(void);

extern "C" int __stdcall WinMain(void *hInstance, void *hPrevInstance, char *lpCmdLine, int nCmdShow) {
    Smoke_ConstructGlobals();
    return LocoWinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}

// --- COM interface IDs --------------------------------------------------------
// DEFINE_GUID without INITGUID is a DECLARATION only, so these two came out of
// the link as undefined and gen_stubs.py zero-filled them. A zeroed IID is not a
// harmless stub: Ddraw_Init's very first act after DirectDrawCreate is
// QueryInterface(IID_IDirectDraw4, &g_pDDraw2), which against an all-zero GUID
// returns E_NOINTERFACE -- so Ddraw_Init returned 0 and the smoke exe could
// never bring up a single surface. Values read from the original image (see
// src/Ddraw.h:0x4785e8 and src/GNetManager.h:0x479048), not from a header.
extern "C" const GUID IID_IDirectDraw4 =
    { 0x9c59509a, 0x39bd, 0x11d1, { 0x8c, 0x4a, 0x00, 0xc0, 0x4f, 0xd9, 0x30, 0xc5 } };
extern const GUID g_iidDirectPlayLobby3A =
    { 0x2db72491, 0x652c, 0x11d1, { 0xa7, 0xa8, 0x00, 0x00, 0xf8, 0x03, 0xab, 0xfc } };
