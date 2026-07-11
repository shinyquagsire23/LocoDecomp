// The DirectSound manager singleton: device init, the fixed channel pool, and
// the channel-selection/steal dispatcher. See src/DSound.cpp and
// docs/subsystems.md's DirectSound section.
#pragma once

#include <dsound.h>
#include "DSoundChannel.h"

class DSound {
public:
    int nCeilingHigh2Master;
    int nCeilingHigh1Master;
    int nCeilingMedMaster;
    int nCeilingLowMaster;
    int nCeilingHigh2Effective;
    int nCeilingHigh1Effective;
    int nCeilingMedEffective;
    int nCeilingLowEffective;
    DSCAPS caps;
    GUID deviceGuid;
    int nChannelCount;
    DSoundChannel *pChannels;
    unsigned int *pLastUsedTicks;
    int nListenerX;
    int nListenerY;
    LPDIRECTSOUND pDirectSound;
    LPDIRECTSOUNDBUFFER pPrimaryBuffer;
    int Unk0xb0;
    bool bPersistentMute;

    DSound();
    virtual ~DSound() { DSound_Teardown(0); }

    unsigned char DSound_InitDeviceAndChannelPool(int nDefaultChannelCount, HWND hwndOwner);
    void DSound_Teardown(HWND hwndOwner);
    HRESULT CreateSoundBuffer(LPDSBUFFERDESC pDesc, LPLPDIRECTSOUNDBUFFER ppBuffer, IUnknown *pUnkOuter);
    void DSound_SetListenerPosition(int x, int y);
    void ReclaimFinishedChannels();
    void ReleaseAllChannels();
    void PlaySoundById(UINT nSoundId);
    void PlaySoundByIdWithHandle(UINT nSoundId, DSoundChannel **ppHandle);
    void AcquireChannelForSound(SoundBankEntry *pDesc, DSoundChannel **ppHandle, int x, int y, unsigned int category, char bLoop);
    void DSound_SetPersistentMute(bool bMute);
    void DSound_SetTemporaryDuck(bool bDuck);
    void DSound_ApplyIniVolumeDefaults(int nLow, int nMed, int nHigh1, int nHigh2);

    static BOOL CALLBACK DSound_PickBestDeviceCallback(GUID *lpGuid, LPSTR lpcstrDescription, LPSTR lpcstrModule, LPVOID lpContext);
};

extern "C" {
    extern DSound *g_pDSoundManager;
}

// 0x45bb20 -- the subsystem-level shutdown: writes the three master volume ceilings back to
// lego.ini's [Sound] section, tears the device down and deletes the manager singleton. Not
// transcribed yet (declared-only). Ghidra had it misfiled in the LocoBitmap namespace as
// FUN_0045bb20 until v356.
void DSound_SaveVolumesAndShutdown();
