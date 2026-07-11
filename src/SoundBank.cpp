// Phase 4: SoundBankEntry, the lazily-loaded/refcounted DirectSound buffer wrapper
// around a WAV resource -- ties together src/Wav.cpp (the RIFF/WAVE loader) and
// src/DSound.cpp (the buffer/channel manager) with the actual "load once, reuse
// across plays" caching logic.
//
// Address-contiguous cluster, 0x448990-0x448f2f: preceded by an unrelated
// PostMessageA stub (0x448970, ends 0x448990 with no gap), followed by
// UiIconListItem's ctor (0x448f30, a different class) -- clean TU boundary on
// both sides. All 6 functions now transcribed: 2 ctor overloads (0x448990,
// taking a resource id + optional base name that gets sprintf'd into
// "<installPrefix><name>.wav"; 0x448a20, taking a pre-built path verbatim),
// the virtual dtor (0x4489d0, vtable slot 0 -- also the auto-generated scalar
// deleting destructor byproduct, see the inline body in src/DSoundChannel.h),
// ResolvePathAndCheckExists (0x448a70, builds a "<path>.wav" + "dat"
// sidecar-settings path -- sic, no separator -- and tries to load per-entry
// overrides from it, RF-archived first then loose-file), and
// ParseSettingsLine (0x448c90, vtable slot 1: the actual key=value
// override reader, called by ResolvePathAndCheckExists against whichever
// stream it found).
//
// SoundBankEntry is polymorphic (vtable 0x478278, CLOSED at exactly 2 slots --
// dtor + ParseSettingsLine, see docs/subsystems.md v114); see
// src/DSoundChannel.h for the struct. No shared base with ThumbnailBmp or any
// other class -- the earlier "14 more slots" reading was a raw-memory-dump
// boundary-crossing artifact into 3 unrelated adjacent vtables in .rdata.

#include "DSound.h"
#include "DSoundChannel.h"

#include <fstream.h>
#include <strstrea.h>
#include <string.h>
#include <stdio.h>

// Cross-TU dependencies not already covered by DSoundChannel.h's shared block
// (g_RFIndex/g_pInstallPathPrefix/_free come from there).
extern int Wav_ParseAndLoad(LPCSTR pszPath, WavResource *pOut); // src/Wav.cpp, 0x413660
// Real C++ linkage, NOT extern "C" -- src/FrameDriver.cpp DEFINES it C++-mangled; see the
// matching note in src/DSoundChannel.cpp.
char *LocoBitmap_GetDSoundErrorString(int hresult); // 0x45c2e0, src/FrameDriver.cpp

// Builds this entry into a fresh object: sets nSoundId, resolves szPath from
// the install-path-prefixed base name (only when pszBaseName is given -- the
// alternate overload below is used when the caller already has a full path),
// then delegates to ResolvePathAndCheckExists for the settings-load side.
// FUNCTION: LOCO 0x448990
SoundBankEntry::SoundBankEntry(int nSoundIdArg, const char *pszBaseName) {
    nSoundId = nSoundIdArg;
    if (pszBaseName != NULL) {
        sprintf(szPath, "%s%s.wav", g_pInstallPathPrefix, pszBaseName);
    }
    ResolvePathAndCheckExists();
}

// FUNCTION: LOCO 0x4489d0 (??_GSoundBankEntry scalar dtor; ~SoundBankEntry()
// itself inlines into this wrapper -- see the inline body in src/DSoundChannel.h)

// Alternate overload: caller already has a full, ready-to-use path (used by
// Sound_PlayOneShotAtPosition, 0x447a70). nSoundId is left as -1 (no
// resource id -- this entry is never looked up by id, only played directly).
// FUNCTION: LOCO 0x448a20
SoundBankEntry::SoundBankEntry(const char *pszFullPath) {
    ResolvePathAndCheckExists();
    nSoundId = -1;
    if (pszFullPath != NULL) {
        strcpy(szPath, pszFullPath);
    }
}

// Resolves this entry's per-sound settings override (MaxInstances/
// ResourceReplayDelay/Global), reset to defaults up front, then loaded from a
// sidecar path built as szPath + "dat" (sic -- no separator: "foo.wav" becomes
// "foo.wavdat", not "foo.dat"; confirmed via raw disasm, not a transcription
// error). Prefers the RF archive (stripping the install-path prefix, same
// idiom as Wav_ParseAndLoad); falls back to a loose file with the same
// sidecar path (is_open(), not fd()!=-1 -- see docs/PARKED.md, fd()'s own
// EOF-ternary body doubles the compare). If NEITHER settings source exists
// AND the sidecar path itself doesn't exist on disk either, bLoaded is
// forced true anyway -- treats "nothing to configure" as resolved rather than
// as an error EnsureLoaded would need to retry.
// PARKED (v113, DIFF 317/508): structurally transcribed and correct (every
// field write, call, and branch verified against the raw disasm) but one
// residual not yet closed -- see docs/PARKED.md for the specific probe
// history; this is the biggest still-open item in this TU.
// FUNCTION: LOCO 0x448a70
void SoundBankEntry::ResolvePathAndCheckExists() {
    ifstream fileStream;

    nRefCount = 0;
    nMaxInstances = -1;
    nRetriggerDelay = 0;
    nNextAllowedTick = 0;
    bGlobalFocus = 0;
    bPersistent = 0;
    pMasterBuffer = NULL;
    bLoaded = 0;

    char szSettingsPath[264];
    strcpy(szSettingsPath, szPath);
    strcat(szSettingsPath, "dat"); // sic: see comment above

    if (g_RFIndex.pFile != NULL) {
        char szResourceName[264];
        strcpy(szResourceName, szSettingsPath + strlen(g_pInstallPathPrefix));
        int nBufSize;
        void *pRfBuf = g_RFIndex.LoadResource((const unsigned char *)szResourceName, &nBufSize);
        if (pRfBuf != NULL) {
            istrstream *pStrm = new istrstream((char *)pRfBuf, nBufSize);
            if (pStrm != NULL) {
                bLoaded = ParseSettingsLine(pStrm);
                delete pStrm;
            }
            _free(pRfBuf);
        }
    }

    if (bLoaded == 0) {
        fileStream.open(szSettingsPath, ios::nocreate);
        if (fileStream.is_open()) {
            bLoaded = ParseSettingsLine(&fileStream);
            fileStream.close();
        }
    }

    if (bLoaded != 1) {
        if (GetFileAttributesA(szSettingsPath) == 0xffffffff) {
            bLoaded = 1;
        }
    }
}

// Case-insensitive key match against a just-read "key=value"-shaped ini line
// (pStream >> szKey, a whitespace-delimited token read); sets the matching
// field from the line's remaining value via >>. Real semantics: reads
// consecutive "Key Value" pairs until the stream hits eof, updating
// nMaxInstances/nRetriggerDelay/bGlobalFocus from whichever keys are
// present ("MaxInstances"/"ResourceReplayDelay"/"Global" -- "Global" takes no
// value, just sets the flag). Return value is 1 as soon as the stream isn't
// bad() -- set BEFORE the eof() check, so a stream that's already at eof on
// entry (nothing to parse) still returns 1, not 0 (confirmed via raw disasm:
// the bl=1 store happens between the bad() and eof() tests, not after both;
// MATCH depended on this exact ordering -- writing the two guards as a single
// `bad() || eof()` or nesting the assignment inside the eof() branch both
// compile to a structurally different, non-matching shape).
// FUNCTION: LOCO 0x448c90
unsigned char SoundBankEntry::ParseSettingsLine(istream *pStream) {
    unsigned char bResult = 0;
    if (!pStream->bad()) {
        bResult = 1;
        if (!pStream->eof()) {
            char szKey[264];
            do {
                *pStream >> szKey;
                if (_stricmp(szKey, "MaxInstances") == 0) {
                    *pStream >> nMaxInstances;
                } else if (_stricmp(szKey, "ResourceReplayDelay") == 0) {
                    *pStream >> nRetriggerDelay;
                } else if (_stricmp(szKey, "Global") == 0) {
                    bGlobalFocus = 1;
                }
                szKey[0] = 0;
            } while (!pStream->eof());
        }
    }
    return bResult;
}

// Lazily loads this entry's WAV data into a fresh DirectSoundBuffer on first call
// (refcounted: every call increments nRefCount; a buffer already loaded is a cheap
// refcount-only hit). Real DSBUFFERDESC fields: dwSize=sizeof(DSBUFFERDESC),
// dwFlags=DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLPAN|DSBCAPS_LOCSOFTWARE|DSBCAPS_STATIC
// (0xca), plus DSBCAPS_GLOBALFOCUS (0x8000) when bGlobalFocus is set,
// dwBufferBytes/lpwfxFormat straight from the just-loaded WavResource. Bails
// with a hardcoded E_NOTIMPL-shaped code if the parsed format's wBitsPerSample < 8
// (confirmed via raw disasm: the WORD compared sits at fmtChunkRaw+14, which is
// WAVEFORMATEX.wBitsPerSample's real offset, NOT nBlockAlign -- nBlockAlign is at
// +12; a `< 8`-bits sanity check makes more sense as a validity guard anyway).
// Return value is the usual masked-bool-in-EAX idiom (CONCAT31 smuggling garbage
// upper bytes) -- real semantics are a plain bool.
// Was parked as an intrinsic "zero-register / eax<->ecx swap" tie-break for many
// sessions; BOTH halves turned out to be source facts (v360):
//   * the two DSBUFFERDESC stores are INDEPENDENT statements, so their source ORDER
//     picks which one lands in eax vs ecx -- dwBufferBytes must be written first;
//   * `test eax,eax` vs `cmp eax,<zero-reg>` is chosen by whether the compared value
//     is an INLINE CALL RESULT (-> test) or a NAMED LOCAL (-> cmp reg,zeroreg), even
//     though the zero register is equally live at both sites. Hence Wav_ParseAndLoad
//     is tested inline while Lock's HRESULT goes through `hr` first.
// `if (x)` vs `if (x != 0)` is NOT the lever here (byte-identical either way).
// FUNCTION: LOCO 0x448d60
unsigned char SoundBankEntry::EnsureLoaded() {
    void *pAudio1 = NULL;
    unsigned long nBytes1 = 0;
    if (g_pDSoundManager == NULL) {
        return 0;
    }
    nRefCount++;
    if (pMasterBuffer != NULL) {
        return 1;
    }
    bLoaded = 0;

    WavResource wav;
    memset(&wav, 0, sizeof(wav));
    if (Wav_ParseAndLoad(szPath, &wav) != 0) {
        return 0;
    }
    if (wav.pData == NULL) {
        return 0;
    }

    DSBUFFERDESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(DSBUFFERDESC);
    desc.dwFlags = 0xca;
    if (bGlobalFocus != 0) {
        desc.dwFlags = 0x80ca;
    }
    desc.dwBufferBytes = wav.nDataSize;
    desc.lpwfxFormat = (LPWAVEFORMATEX)wav.fmtChunkRaw;

    HRESULT hr;
    if (((LPWAVEFORMATEX)wav.fmtChunkRaw)->wBitsPerSample >= 8) {
        hr = g_pDSoundManager->CreateSoundBuffer(&desc, &pMasterBuffer, NULL);
    } else {
        hr = 0x80004001; // E_NOTIMPL: bit depth too small to be a real WAVEFORMATEX
    }
    if (hr != 0) {
        LocoBitmap_GetDSoundErrorString(hr);
        return 0;
    }

    hr = pMasterBuffer->Lock(0, 0, &pAudio1, &nBytes1, NULL, NULL, 2 /* DSBLOCK_ENTIREBUFFER, undefined in this toolchain's dsound.h */);
    if (hr != 0) {
        return 0;
    }
    memcpy(pAudio1, wav.pData, nBytes1); // idiom-exempt: runtime length (Lock's own out-param)
    pMasterBuffer->Unlock(pAudio1, nBytes1, NULL, 0);

    if (wav.pData != NULL) {
        _free(wav.pData);
    }
    bLoaded = 1;
    return 1;
}

// Releases one reference; when the count reaches 0 (and the entry isn't marked
// bPersistent), stops and releases the underlying DirectSoundBuffer so the
// resource can be reclaimed. Not the destructor -- no vtable reset, no
// operator delete, the SoundBankEntry object itself survives.
// FUNCTION: LOCO 0x448ee0
unsigned char SoundBankEntry::Release() {
    if (nRefCount > 0) {
        nRefCount--;
    }
    if (nRefCount == 0 && pMasterBuffer != NULL && bPersistent != 1) {
        pMasterBuffer->Stop();
        pMasterBuffer->Release();
        pMasterBuffer = NULL;
    }
    return 1;
}
