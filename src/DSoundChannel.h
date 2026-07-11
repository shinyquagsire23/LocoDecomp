// Shared declarations for the DirectSound subsystem's per-voice "channel" object.
// Split out of src/DSoundChannel.cpp (Phase 4's first real TU) when the DSound
// manager TU (src/DSound.cpp) needed the same types -- CLAUDE.md forbids a
// second definition of an already-modeled class, so both TUs include this.
#pragma once

#include <dsound.h>
#include <stdio.h>  // FILE -- RFIndex holds the .RFD handle open for the lifetime of the game

class istream; // <fstream.h>/<strstrea.h>, forward-declared to avoid pulling iostream.lib's headers into every consumer of this file

// Cross-TU dependencies shared by every TU in this DirectSound cluster (Wav.cpp,
// SoundBank.cpp) -- opaque shapes, same pattern as the rest of this cluster
// (verify.py's byte comparison masks relocations; only the calling convention
// at each call site is real, matchable code). Promoted here from src/Wav.cpp
// (v90) once a 2nd TU (src/SoundBank.cpp, v113) needed the same RFIndex
// type -- CLAUDE.md forbids a 2nd definition of an already-modeled struct.
// 16 bytes, and embedded (not pointed to) in UIResources at +0x18 -- see UIResources.h,
// whose m_rfIndex IS DAT_00485600. Field names from RFIndex::Open (0x45caa0), which is the
// only function that writes all four; Unk0x8 is zeroed twice by Open and never read.
// One parsed .RFH index record, 16 bytes (pinned by RFIndex::Open's own operator new(0x10)
// at 0x45cbe0). The on-disk record is [u32 namelen][name][u32 size][u32 flags] -- note the
// STRUCT orders flags before size, the opposite of the file. flags&1 = Huffman-compressed
// (see loco/rf-extract.py, loco/rfh.txt).
struct RFRecord {
    char *pName;         // +0x0  malloc'd copy of the record name (e.g. "roads\\half-vwint.dat")
    unsigned int flags;  // +0x4
    unsigned int size;   // +0x8  uncompressed byte count
    RFRecord *pNext;     // +0xc  singly-linked, tail-appended in file order
};

struct RFIndex {  // TODO: idiom
    FILE *pFile;         // +0x0  non-null once the RF archive (.RFD) is open
    RFRecord *pRecords;  // +0x4  head of the RFRecord index list parsed out of the .RFH
    int Unk0x8;          // +0x8  zeroed by Open, never read back
    char *pRfdPath0xc;   // +0xc  malloc'd copy of the .RFD path
    void *LoadResource(const unsigned char *name, int *outSize);
    unsigned char Open(char *path);  // 0x45caa0
    unsigned char IsOpen() { return pFile != 0; }
    RFIndex();    // 0x45ca10
};
extern "C" {  // TODO: idiom
    extern RFIndex g_RFIndex;        // DAT_00485600 == UIResources(DAT_004855e8)+0x18, the embedded RFIndex
    extern char g_pInstallPathPrefix[];      // DAT_004a99c8 -- stripped off the front of every loose-file path
    extern void *_malloc(unsigned int size); // 0x4673c0 -- the real CRT malloc() (was misnamed Rf_AllocFromResourceHeap;
                                              // byte-identical to LIBCMT's malloc.obj _malloc, confirmed v319); pushes
                                              // DAT_004ff268 (the new-handler flag) and tail-calls __nh_malloc
    extern void _free(void *p);              // 0x466c70 -- the real CRT free() (was misnamed Rf_FreeResourceBuffer;
                                              // byte-identical to LIBCMT's free.obj _free, confirmed v319)
}

// sizeof == 0x12C (300), confirmed by 2 independent operator-new(0x12C) allocation
// sites (SoundBank_PreloadWavRangeMaybe/0x446cc0, Sound_PlayOneShotAtPosition/
// 0x447a70). Polymorphic (vtable 0x478278); only slot 0 (the virtual dtor) and
// slot 1 (ParseSettingsLine) are understood -- the other 14 slots belong to
// a still-unidentified shared base class, see docs/subsystems.md. NOT the same
// object as WavResource below -- EnsureLoaded constructs/owns a local
// WavResource and passes it to Wav_ParseAndLoad; the two are related by
// composition, not identity. Full cluster transcribed in src/SoundBank.cpp
// (v113) -- see that file's header comment for the TU boundary.
struct SoundBankEntry {
    int nSoundId;
    unsigned char bPersistent; // 0x8: skip auto-unload-at-refcount-0 in Release
    unsigned char bLoaded;     // 0x9: EnsureLoaded success flag; also (ab)used by
                                     // ResolvePathAndCheckExists as a "settings resolved" flag
    char pad0xa[2];
    LPDIRECTSOUNDBUFFER pMasterBuffer;
    int nRetriggerDelay;
    int nNextAllowedTick;
    char szPath[0x108];             // 0x18: passed directly to Wav_ParseAndLoad
    int nRefCount;                  // 0x120
    unsigned int nMaxInstances;     // 0x124 -- UNSIGNED: ParseSettingsLine reads it through
                                    // 0x464f70 = istream::operator>>(unsigned long&) (strtoul,
                                    // stores a DWORD), while the very next read in the same
                                    // function (nRetriggerDelay) goes to the SIGNED 0x4646c0.
                                    // Two different callees, so the two fields cannot share
                                    // signedness. Was `int` until v412.
    unsigned char bGlobalFocus; // 0x128: DSBCAPS_GLOBALFOCUS selector, see EnsureLoaded
    char pad0x129[3];

    SoundBankEntry(int nSoundIdArg, const char *pszBaseName);
    SoundBankEntry(const char *pszFullPath);
    // Virtual (vtable slot 0); defined inline so it also byte-matches the
    // auto-generated "scalar deleting destructor" (the ONLY dtor this class
    // has -- confirmed via raw disasm: a `push esi;...;ret 4`-shaped single
    // COMDAT taking the usual delete-flag byte param, no separate non-deleting
    // variant). See CLAUDE.md's virtual-dtor-in-header lesson (DSound.h
    // precedent).
    virtual ~SoundBankEntry() {
        if (pMasterBuffer != NULL) {
            pMasterBuffer->Stop();
            pMasterBuffer->Release();
            pMasterBuffer = NULL;
        }
        bLoaded = 0;
    }

    void ResolvePathAndCheckExists();
    // Virtual (vtable slot 1); parses "Key Value"-shaped override lines
    // (MaxInstances/ResourceReplayDelay/Global) from a per-entry settings
    // stream. Called by ResolvePathAndCheckExists against whichever
    // stream (RF-archived or loose-file) it found.
    virtual unsigned char ParseSettingsLine(istream *pStream);
    unsigned char EnsureLoaded();
    unsigned char Release();
};

struct DSoundChannel {
    DSoundChannel **pExternalHandle;
    bool bLoop;
    int nCategory;
    int nVolumePercent;
    int nState;
    LPDIRECTSOUNDBUFFER pBuffer;
    int nPosX;
    int nPosY;
    int nBoundX;
    int nBoundY;
    int nCeilingLow;
    int nCeilingMed;
    int nCeilingHigh1;
    int nCeilingHigh2;
    int nSoundId;

    DSoundChannel();
    void Init();
    void Release();
    void ClearExternalHandle();
    void BindExternalHandle(DSoundChannel **externalHandle);
    void AcquireAndPlay(LPDIRECTSOUND pDS, SoundBankEntry *pDesc, int x, int y, int category, char loop);
    void Pause();
    void ResumeOrRestart();
    unsigned char IsIdle();
    unsigned char IsReclaimable();
    void SetBounds(int x, int y);
    void SetPosition(int x, int y);
    void SetCeilingsAndApply(int low, int med, int high1, int high2);
    void SetCategory(int category);
    void SetVolume(int percent);
};

// PARTIAL -- fields-only (this TU never calls any of its own methods, just
// fills in fields on an already-constructed instance). Real class: vtable
// 0x478278, two ctor overloads (0x448990/0x448a20, not this TU's concern).
// Layout recovered 2026-07-14 (v90) from src/Wav.cpp's Wav_ParseAndLoad, the
// RIFF/WAVE parser that fills one of these in from a loaded WAV resource.
struct WavResource {
    void *vtable;
    unsigned char fmtChunkRaw[0x14]; // raw "fmt " chunk bytes (<=0x12 used), WAVEFORMATEX-shaped
    unsigned int nDataSize;          // "data" chunk size
    void *pData;                     // "data" chunk bytes, _malloc'd
};
