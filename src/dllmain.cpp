#include <windows.h>
#include <vector>
#include <atomic>
#include <cstring>
#include "minhook.h"

#define ULTIMATE_ASI_VERSION 0x1A
#define CACHE_OPT_VERSION "1.2"
#define STATUS_SUCCESS ((NTSTATUS)0)

#pragma comment(lib, "kernel32.lib")

// Forward declarations
extern "C" __declspec(dllexport) int Terminate(WORD wVersion, void* pModule);
__declspec(dllexport) bool HookReadFile();

static std::atomic<bool> g_initialized{false};
static CRITICAL_SECTION cs_wrapper;
static std::vector<HANDLE> g_handles;
std::atomic<long> s_cache_ops{0};

// MinHook function pointer types for ReadFile
typedef NTSTATUS(NTAPI* ReadFile_t)(HANDLE, HANDLE, PVOID, PVOID, PVOID, PVOID, ULONG, PLARGE_INTEGER, PULONG);
ReadFile_t OriginalNtReadFile = nullptr;

// Detour function for NtReadFile
NTSTATUS NTAPI DetourNtReadFile(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine, PVOID ApcContext, PVOID IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset, PULONG Key) {
    s_cache_ops.fetch_add(1, std::memory_order_relaxed);
    if (OriginalNtReadFile) {
        return OriginalNtReadFile(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, Buffer, Length, ByteOffset, Key);
    }
    return STATUS_SUCCESS;
}

extern "C" __declspec(dllexport) int Initialize(WORD wVersion, void* pModule) {
    if (!GetModuleHandleA("Ultimate ASI Loader.dll"))
        return 0;
    
    if (g_initialized.load(std::memory_order_acquire))
        return 0;
    
    EnterCriticalSection(&cs_wrapper);
    g_initialized.store(true, std::memory_order_release);
    LeaveCriticalSection(&cs_wrapper);
    
    // Initialize MinHook automatically
    if (MH_Initialize() != MH_OK) {
        Terminate(wVersion, pModule);
        return -1;
    }
    
    if (!HookReadFile()) {
        return -1;
    }
    
    return wVersion == 1 ? 0 : -1;
}

extern "C" __declspec(dllexport) int Terminate(WORD wVersion, void* pModule) {
    EnterCriticalSection(&cs_wrapper);
    g_initialized.store(false, std::memory_order_release);
    LeaveCriticalSection(&cs_wrapper);
    
    MH_Uninitialize();

    for (auto& handle : g_handles) {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
    g_handles.clear();
    
    DeleteCriticalSection(&cs_wrapper);
    return 0;
}

__declspec(dllexport) bool HookReadFile() {
    HMODULE hKernel = GetModuleHandleA("ntdll.dll");
    if (!hKernel) {
        return false;
    }
    
    FARPROC origFunc = GetProcAddress(hKernel, "NtReadFile");
    if (origFunc) {
        if (MH_CreateHook(origFunc, &DetourNtReadFile, reinterpret_cast<LPVOID*>(&OriginalNtReadFile)) == MH_OK) {
            if (MH_EnableHook(origFunc) == MH_OK) {
                return true;
            }
        }
    }
    return false;
}

static inline bool ShouldCacheOptimize(const char* filename) {
    if (!filename || !*filename)
        return false;
    
    const char* excludeDirs[] = {"SaveData/", "User/", "Config/", NULL};
    for (const char* dir : excludeDirs) {
        if (dir) {
            size_t len = strnlen_s(filename, 260);
            if (_strnicmp(filename, dir, len) == 0)
                return false;
        }
    }
    
    return true;
}

extern "C" __declspec(dllexport) BOOL APIENTRY DllMain(HMODULE hModule, 
                                                         DWORD ulReason,
                                                         LPVOID lpReserved) {
    if (ulReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        InitializeCriticalSection(&cs_wrapper);
    }
    else if (ulReason == DLL_PROCESS_DETACH) {
        DeleteCriticalSection(&cs_wrapper);
        g_initialized.store(false, std::memory_order_release);
    }
    
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL ReadFileOptimization(HANDLE hFile, 
                                                             ULONG nBytesToRead, 
                                                             void* pBuf) {
    s_cache_ops.fetch_add(1, std::memory_order_relaxed);
    return TRUE;
}

extern "C" __declspec(dllexport) void CleanupWrapper() {
    if (g_initialized.load(std::memory_order_acquire)) {
        g_initialized.store(false, std::memory_order_release);
        DeleteCriticalSection(&cs_wrapper);
    }
}

extern "C" __declspec(dllexport) int GetDllVersion() {
    return ULTIMATE_ASI_VERSION;
}

__forceinline void AtomicIncrementCacheOps(long* value) {
    InterlockedIncrement(value);
}