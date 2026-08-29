// 5x6 GDRAM dump helper - the 32-bit half of the capture tool.
//
// CubeProgrammer_API.dll ships 32-bit in this bench's installation, so the
// 64-bit Qt application cannot load it in-process. This little console
// program can: it opens ONE ephemeral SWD session (HOTPLUG), finds the
// instrument's capture machinery by scanning RAM for the magic "MIR5X6RT",
// drives the ring protocol at DLL speed, writes the raw RGB666 dump to a
// file, and disconnects. The Qt app runs it, reads the file, and never
// touches SWD itself.
//
// Usage:   5x6_dump_helper.exe <out.raw>
// Stdout:  "POS <bytes>" progress lines, then "DONE" - the parent parses.
// Exit:    0 ok · 1 DLL · 2 no probe · 3 connect · 4 not frozen · 5 stalled
//
// The instrument must already be FROZEN (SysEx MIRROR_HALT 1): a dump of a
// moving screen would be a torn image, so the firmware refuses it and this
// helper checks the flag before asking.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---- mirrored from CubeProgrammer_API.h -----------------------------------
// Structs only, no function declarations: every DLL entry point is resolved
// by GetProcAddress below, so pulling the real header would only risk its
// dllimport-decorated prototypes forcing a link against the import library.
// The layout here is proven - connectStLink accepted it on the bench.
enum { NORMAL_MODE_ = 0, HOTPLUG_MODE_ = 1 };

struct frequencies {
    unsigned int jtagFreq[12];
    unsigned int jtagFreqNumber;
    unsigned int swdFreq[12];
    unsigned int swdFreqNumber;
};

struct debugConnectParameters {
    int  dbgPort;                 // 1 = SWD
    int  index;
    char serialNumber[33];
    char firmwareVersion[20];
    char targetVoltage[5];
    int  accessPortNumber;
    int  accessPort;
    int  connectionMode;          // 1 = HOTPLUG
    int  resetMode;
    int  isOldFirmware;
    frequencies freq;
    int  frequency;
    int  isBridge;
    int  shared;
    char board[100];
    int  DBG_Sleep;
    int  speed;
};

struct displayCallBacks {
    void (*initProgressBar)();
    void (*logMessage)(int msgType, const wchar_t* str);
    void (*loadBar)(int x, int n);
};

static void cbInit() {}
static void cbLog(int, const wchar_t*) {}
static void cbBar(int, int) {}

// ---- the DLL, resolved at run time ----------------------------------------
typedef int  (*getStLinkList_t)(debugConnectParameters** list, int shared);
typedef int  (*connectStLink_t)(debugConnectParameters p);
typedef int  (*readMemory_t)(unsigned int addr, unsigned char** data, unsigned int size);
typedef int  (*writeMemory_t)(unsigned int addr, char* data, unsigned int size);
typedef void (*disconnect_t)();
typedef void (*freeLibraryMemory_t)(void* ptr);
typedef void (*setLoadersPath_t)(const char* path);
typedef void (*setDisplayCallbacks_t)(displayCallBacks c);

static getStLinkList_t       pGetStLinkList;
static connectStLink_t       pConnectStLink;
static readMemory_t          pReadMemory;
static writeMemory_t         pWriteMemory;
static disconnect_t          pDisconnect;
static freeLibraryMemory_t   pFreeLibraryMemory;

static const char* DLL_DIR =
    "C:\\Program Files (x86)\\STMicroelectronics\\STM32Cube\\STM32CubeProgrammer\\api\\lib";
static const char* LOADERS_DIR =
    "C:\\Program Files (x86)\\STMicroelectronics\\STM32Cube\\STM32CubeProgrammer\\bin";

static bool rdBlock(uint32_t addr, void* out, uint32_t size)
{
    unsigned char* p = nullptr;
    if (pReadMemory(addr, &p, size) != 0 || !p) return false;
    memcpy(out, p, size);
    pFreeLibraryMemory(p);
    return true;
}

static bool wr32(uint32_t addr, uint32_t v)
{
    return pWriteMemory(addr, reinterpret_cast<char*>(&v), 4) == 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) { fprintf(stderr, "usage: 5x6_dump_helper <out.raw>\n"); return 1; }

    SetDllDirectoryA(DLL_DIR);
    HMODULE dll = LoadLibraryA("CubeProgrammer_API.dll");
    if (!dll) { fprintf(stderr, "DLL introuvable\n"); return 1; }

    pGetStLinkList     = (getStLinkList_t)GetProcAddress(dll, "getStLinkList");
    pConnectStLink     = (connectStLink_t)GetProcAddress(dll, "connectStLink");
    pReadMemory        = (readMemory_t)GetProcAddress(dll, "readMemory");
    pWriteMemory       = (writeMemory_t)GetProcAddress(dll, "writeMemory");
    pDisconnect        = (disconnect_t)GetProcAddress(dll, "disconnect");
    pFreeLibraryMemory = (freeLibraryMemory_t)GetProcAddress(dll, "freeLibraryMemory");
    setLoadersPath_t      pSetLoadersPath      = (setLoadersPath_t)GetProcAddress(dll, "setLoadersPath");
    setDisplayCallbacks_t pSetDisplayCallbacks = (setDisplayCallbacks_t)GetProcAddress(dll, "setDisplayCallbacks");
    if (!pGetStLinkList || !pConnectStLink || !pReadMemory || !pWriteMemory ||
        !pDisconnect || !pFreeLibraryMemory || !pSetLoadersPath || !pSetDisplayCallbacks) {
        fprintf(stderr, "exports manquants\n");
        return 1;
    }

    displayCallBacks cb = { cbInit, cbLog, cbBar };
    pSetDisplayCallbacks(cb);
    pSetLoadersPath(LOADERS_DIR);

    debugConnectParameters* list = nullptr;
    int n = pGetStLinkList(&list, 0);
    if (n < 1 || !list) { fprintf(stderr, "aucune sonde\n"); return 2; }

    debugConnectParameters p = list[0];
    p.dbgPort = 1;                      // SWD
    p.connectionMode = HOTPLUG_MODE_;   // no reset - but it halts anyway, see below
    p.shared = 0;
    if (pConnectStLink(p) != 0) { fprintf(stderr, "connexion refusee\n"); return 3; }

    // The session HALTS the core on connect - measured on the bench: the dump
    // servant went silent for exactly the session's life and finished the
    // instant disconnect() ran. The API has no run call, but DHCSR is memory:
    // DBGKEY with C_HALT and C_DEBUGEN clear, and the core runs on with the
    // session still open.
    {
        unsigned int dbgkey = 0xA05F0000u;
        pWriteMemory(0xE000EDF0u, reinterpret_cast<char*>(&dbgkey), 4);
        Sleep(50);
    }

    // ---- find the instrument's map: scan the G070's 36K of RAM ------------
    const uint32_t RAM = 0x20000000u, RAM_SIZE = 0x9000u;
    std::vector<uint8_t> ram(RAM_SIZE);
    bool ok = true;
    for (uint32_t off = 0; off < RAM_SIZE && ok; off += 0x1000)
        ok = rdBlock(RAM + off, ram.data() + off, 0x1000);
    if (!ok) { fprintf(stderr, "lecture RAM impossible\n"); pDisconnect(); return 3; }

    static const char MAGIC[8] = { 'M','I','R','5','X','6','R','T' };
    size_t at = std::string::npos;
    for (size_t i = 0; i + 8 + 28 <= RAM_SIZE; ++i)
        if (!memcmp(ram.data() + i, MAGIC, 8)) { at = i; break; }
    if (at == std::string::npos) {
        fprintf(stderr, "magie absente (capture non compilee ?)\n");
        pDisconnect();
        return 3;
    }
    auto t32 = [&](size_t off) {
        uint32_t v; memcpy(&v, ram.data() + at + 8 + off * 4, 4); return v;
    };
    const uint32_t headAddr = t32(0), tailAddr = t32(1), bufAddr = t32(2),
                   bufSize  = t32(3), frozenAddr = t32(4), reqAddr = t32(5),
                   posAddr  = t32(6);

    uint32_t frozen = 0;
    if (!rdBlock(frozenAddr, &frozen, 1) || !(frozen & 0xff)) {
        fprintf(stderr, "machine non gelee - envoyer MIRROR_HALT 1 d'abord\n");
        pDisconnect();
        return 4;
    }

    // ---- the dump conversation -------------------------------------------
    const uint32_t TOTAL = 480u * 320u * 3u;
    std::vector<uint8_t> image;
    image.reserve(TOTAL);

    if (!wr32(tailAddr, 0) || !wr32(reqAddr, 1)) {
        fprintf(stderr, "ecriture refusee\n");
        pDisconnect();
        return 3;
    }

    // Do not consume the ring until the servant PROVES it restarted: pos
    // leaves zero only after its start block has re-zeroed head and clocked
    // fresh pixels. Guards against draining a previous attempt's leftovers.
    {
        DWORD t0 = GetTickCount();
        uint32_t pos = 0;
        while (GetTickCount() - t0 < 3000) {
            if (rdBlock(posAddr, &pos, 4) && pos > 0) break;
            Sleep(5);
        }
        if (pos == 0) {
            fprintf(stderr, "le servant ne demarre pas (tache Dump ? gel ?)\n");
            pDisconnect();
            return 5;
        }
    }

    uint32_t tail = 0;
    DWORD lastProgress = GetTickCount();
    uint32_t lastSize = 0;
    std::vector<uint8_t> ring(bufSize);
    while (image.size() < TOTAL) {
        // Head first, then the WHOLE ring in one aligned read. The DLL's
        // readMemory refuses unaligned addresses - a span starting at an odd
        // tail stalled the first bench run forever - and one aligned 8K read
        // is faster than clever spans anyway. Bytes past the head snapshot
        // are simply not consumed.
        uint32_t headw = 0;
        if (!rdBlock(headAddr, &headw, 4)) break;
        uint32_t head = headw & 0xffff;
        if (head != tail) {
            if (!rdBlock(bufAddr, ring.data(), bufSize)) break;
            while (tail != head && image.size() < TOTAL) {
                image.push_back(ring[tail]);
                tail = (tail + 1) % bufSize;
            }
            wr32(tailAddr, tail);
            printf("POS %u\n", (unsigned)image.size());
            fflush(stdout);
        } else {
            Sleep(2);
        }
        if (image.size() != lastSize) { lastSize = (uint32_t)image.size(); lastProgress = GetTickCount(); }
        else if (GetTickCount() - lastProgress > 10000) {
            fprintf(stderr, "dump immobile depuis 10 s\n");
            pDisconnect();
            return 5;
        }
    }

    pDisconnect();

    if (image.size() < TOTAL) { fprintf(stderr, "dump incomplet\n"); return 5; }

    FILE* f = fopen(argv[1], "wb");
    if (!f) { fprintf(stderr, "fichier de sortie\n"); return 1; }
    fwrite(image.data(), 1, TOTAL, f);
    fclose(f);
    printf("DONE\n");
    return 0;
}
