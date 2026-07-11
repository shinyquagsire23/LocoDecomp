// Phase 4: the WAV/RIFF resource loader -- a tiny, self-contained 3rd TU in
// the DirectSound cluster.
//
// Address-contiguous, confirmed clean TU boundaries: starts immediately
// after src/DSound.cpp (0x41365a) with no gap at Wav_ParseAndLoad (0x413660);
// ends at 0x413ab0 where PopupWndBase::PopupWndBase (an unrelated UI-popup
// ctor) begins. Just 2 real functions -- Wav_ParseAndLoad plus its one
// private helper Wav_ReadOrFindChunk -- plus the /GX-auto-generated
// catch(...) funclet. Sole caller: SoundBankEntry::EnsureLoaded (0x448d60,
// a separate, not-yet-tackled TU). See docs/subsystems.md's DirectSound
// section.
//
// First real use of genuine try/catch (not just RAII auto-unwind
// scaffolding) and of the statically-linked iostream classes (istream/
// ifstream/istrstream) as real, directly-used C++ types in this project --
// both confirmed correct by cross-referencing toolchain/vc50/INCLUDE's
// actual VC5 headers against the raw disasm (see docs/subsystems.md).

#include "DSoundChannel.h"

#include <fstream.h>
#include <strstrea.h>
#include <string.h>

// Reads a plain 12-byte header (mode 0), or searches forward from the
// stream's current position for an 8-byte [id][size] chunk header matching
// *pOut's id (mode 0x10), skipping non-matching chunks via ignore(). Modes
// 0x20/0x40 are a real but unexercised no-op branch in the original (this
// TU's only caller never passes them). private to Wav_ParseAndLoad.
// FUNCTION: LOCO 0x413980
int Wav_ReadOrFindChunk(istream *pStream, int *pOut, int reserved, int mode) {
    int nResult = 0;
    if (pStream == NULL) {
        return -1;
    }
    if (mode != 0x10) {
        if (mode != 0x20 && mode != 0x40) {
            pStream->seekg(0, ios::beg);
            pStream->read((char *)pOut, 0xc);
            if (pStream->gcount() != 0xc) {
                nResult = 0x109;
            }
        }
    } else {
        pStream->sync();
        int chunkHdr[5];
        pStream->read((char *)chunkHdr, 8);
        if (chunkHdr[0] != *pOut) {
            do {
                if (pStream->eof()) {
                    break;
                }
                int nChunkSize = chunkHdr[1];
                pStream->ignore(nChunkSize);
                pStream->read((char *)chunkHdr, 8);
            } while (chunkHdr[0] != *pOut);
            if (chunkHdr[0] != *pOut) {
                nResult = 0x109;
            }
        }
        if (nResult == 0) {
            // sic: only chunkHdr[0..1] were ever written by the reads above --
            // chunkHdr[2..4] are genuinely uninitialized stack garbage here, copied
            // into the caller's locals along with the real id/size. Harmless: the
            // caller only ever reads back the id/size, never the 3 garbage dwords.
            int *pSrc = chunkHdr;
            for (int i = 5; i != 0; i--) {
                *pOut = *pSrc;
                pSrc++;
                pOut++;
            }
        }
    }
    return nResult;
}

// Loads a WAV resource by path, preferring the RF archive (stripping the
// fixed install-path prefix) and falling back to a loose file, then parses
// the RIFF/WAVE/"fmt "/"data" chunk structure into *pOut. Real parse/format
// errors are reported via a bare `throw <int-errcode>;` (confirmed from raw
// disasm: each site pushes &localCode and the fixed ThrowInfo-for-int
// descriptor at 0x47a950, then calls the compiler's RaiseException-based
// _CxxThrowException equivalent at 0x466ce0 -- NOT a callable helper
// function, despite looking like one from the decompiler's FUN_ view) --
// caught by `catch (int)` below and returned as the function's own result
// (confirmed via the auto-generated catch funclet at 0x413971, which
// literally does `nReturnCode = <caught int>;`; NOT swallowed as previously
// assumed). The `try` only wraps the chunk-parsing logic, not the
// RF-archive/file-open resource acquisition above it -- those two `new`
// calls get their own, separate compiler-automatic protection (free the raw
// block if the ctor throws) that is active regardless of the user `try`.
// FUNCTION: LOCO 0x413660
int Wav_ParseAndLoad(LPCSTR pszPath, WavResource *pOut) {
    istrstream *pRfStream = NULL;
    void *pRfBuf = NULL;
    ifstream *pFileStream = NULL;
    istream *pStream = NULL;
    if (pOut == NULL) {
        return -1;
    }

    int nReturnCode = 0;

    if (g_RFIndex.pFile != NULL) {
        int nBufSize;
        pRfBuf = g_RFIndex.LoadResource((const unsigned char *)(pszPath + strlen(g_pInstallPathPrefix)), &nBufSize);
        if (pRfBuf != NULL) {
            pRfStream = new istrstream((char *)pRfBuf, nBufSize);
            if (pRfStream != NULL) {
                pStream = pRfStream;
            }
        }
    }
    if (pStream == NULL) {
        pFileStream = new ifstream();
        if (pFileStream != NULL) {
            pFileStream->open(pszPath, ios::in | ios::nocreate | ios::binary);
            if (pFileStream->fd() != -1) {
                pStream = pFileStream;
            }
        }
        if (pStream == NULL) {
            goto cleanup;
        }
    }

    // Shared chunk-header scratch, reused across all 3 header reads below --
    // matches the original, which reuses one stack region rather than a
    // fresh local per call. chunkHdr[0]/[1] = chunk id/size; [2] is only
    // meaningful for the plain 12-byte RIFF/WAVE header read (id/size/waveId).
    // Sized 5, not 3: Wav_ReadOrFindChunk's mode-0x10 copy loop always
    // writes 5 dwords into *pOut (see its own "sic" comment) regardless of
    // caller need -- the caller must reserve the full 20 bytes even though
    // [3]/[4] are never read back here.
    int chunkHdr[5];

    try {
        if (Wav_ReadOrFindChunk(pStream, chunkHdr, 0, 0) != 0) {
            throw 0xe102;
        }
        if (chunkHdr[0] != 0x46464952 || chunkHdr[2] != 0x45564157) { // "RIFF" / "WAVE"
            throw 0xe101;
        }

        chunkHdr[0] = 0x20746d66; // "fmt "
        if (Wav_ReadOrFindChunk(pStream, chunkHdr, 0, 0x10) != 0) {
            throw 0xe101;
        }
        if (chunkHdr[1] > 0x12) {
            throw 0xe101;
        }
        {
            unsigned int nFmtBytesRead;
            if (pOut->fmtChunkRaw == NULL || pStream->bad() || (int)chunkHdr[1] < 1) {
                nFmtBytesRead = 0;
            } else {
                pStream->read((char *)pOut->fmtChunkRaw, chunkHdr[1]);
                nFmtBytesRead = pStream->gcount();
            }
            if (nFmtBytesRead != (unsigned int)chunkHdr[1]) {
                throw 0xe102;
            }
        }

        chunkHdr[0] = 0x61746164; // "data"
        if (Wav_ReadOrFindChunk(pStream, chunkHdr, 0, 0x10) != 0) {
            throw 0xe101;
        }
        pOut->nDataSize = chunkHdr[1];
        pOut->pData = _malloc(pOut->nDataSize + 1);
        if (pOut->pData == NULL) {
            throw 0xe000;
        }
        {
            unsigned int nDataBytesRead;
            if (pOut->pData == NULL || pStream->bad() || (int)pOut->nDataSize < 1) {
                nDataBytesRead = 0;
            } else {
                pStream->read((char *)pOut->pData, pOut->nDataSize);
                nDataBytesRead = pStream->gcount();
            }
            if (nDataBytesRead != pOut->nDataSize) {
                throw 0xe102;
            }
        }
    }
    catch (int nErr) {
        nReturnCode = nErr;
    }

cleanup:
    if (pRfStream != NULL) {
        delete pRfStream;
    }
    if (pRfBuf != NULL) {
        _free(pRfBuf);
    }
    if (pFileStream != NULL) {
        pFileStream->close();
        delete pFileStream;
    }
    return nReturnCode;
}
