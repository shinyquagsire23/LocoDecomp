#pragma once

// IniFile -- the game's .ini config-file accessor singleton (g_pIniFile, DAT_004a9eec).
// Ghidra's own analysis already names this class `IniFile` with real ReadInt/ReadString methods
// (thin GetPrivateProfileIntA/GetPrivateProfileStringA wrappers keyed off an internal iniPath
// field) -- modeled here as an opaque partial view (verify.py masks relocations, so only the
// calling convention at each call site is real, matchable code; no fields needed since every
// known consumer only ever calls through the pointer). Consolidated 2026-07-19 from 2 divergent
// per-TU local copies (DSound.cpp's own `IniFile`, EditCardWnd.cpp's own
// `PostBagIniFileMaybe`) per the never-duplicate-structs rule -- this is now the one shared
// definition. Every method is DEFINED in src/IniFile.cpp as of 2026-07-28 and the class is
// fully transcribed; the signatures below are all confirmed against their own disasm.
struct IniFile {
    // Polymorphic: vtable 0x4784bc (slot 0 = scalar deleting dtor), stored by the ctor
    // (0x452cee). The dtor itself is the bare vtable-store stub, defined in
    // src/IniFile.cpp. Consumers only ever call through g_pIniFile, so adding the vptr
    // changes nothing at any call site.
    // 0x452ce0 -- takes the fully-resolved .ini path and stashes it in the internal iniPath
    // field every Read*/Write* below keys off. sizeof(IniFile) is 0x10c, pinned by
    // AppWindow_LoadConfigDirectories's own `new_alloc(0x10c)` allocation site.
    IniFile(const char *pszIniPath);
    virtual ~IniFile();                                                                // 0x452d50
    int ReadInt(const char *section, const char *key, int defaultValue);              // 0x452d60
    void ReadString(const char *section, const char *key, const char *defaultValue,
                     char *outBuf, int outSize);                                      // 0x452d80
    void WriteInt(const char *section, const char *key, int value);                   // 0x452db0
    void WriteString(const char *section, const char *key, const char *value);        // 0x452df0

    // Fields, transcribed from Ghidra's own struct DB 2026-07-27. Previously modeled as an
    // opaque view with no members, which silently made sizeof(IniFile) 4 instead of 0x10c and
    // so compiled AppWindow_LoadConfigDirectories' `new IniFile(...)` as `push 4` where the
    // original pushes 0x10c -- the allocation site is the sizeof oracle, per CLAUDE.md.
    char iniPath[260];   // +0x4 -- the resolved .ini path; the ctor (0x452ce0) strcpy's its
                          //         argument straight in here (`lea ebx,[edx+4]`), and every
                          //         Read*/Write* passes it as GetPrivateProfile*'s lpFileName
    int Unk0x108;         // +0x108 -- Ghidra's own pre-existing name; no reader identified yet
};

extern "C" IniFile *g_pIniFile;    // DAT_004a9eec
