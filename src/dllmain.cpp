#include <windows.h>
#include <vector>
#include <atomic>
#include <cstring>
#include "minhook.h"

#define ULTIMATE_ASI_VERSION 0x1A
#define CACHE_OPT_VERSION "1.2"
#define STATUS_SUCCESS ((NTSTATUS)0)

#pragma comment(lib, "kernel32.lib")

static atomic<bool> g_initialized{false};
static CRITICAL_SECTION cs_wrapper;
static std::vector<HANDLE> g_handles;
std::atomic<long> s_cache_ops{0};

// MinHook function pointer types for ReadFile
typedef NTSTATUS(__stdcall* ReadFile_t)(HANDLE, void*, DWORD, LPDWORD, LPOVERLAPPED);
ReadFile_t OriginalNtReadFile = nullptr;

extern "C" __declspec(dllexport) int Initialize(WORD wVersion, void* pModule) {
    if (!GetModuleHandleA("Ultimate ASI Loader.dll"))
        return 0;
    
    if (g_initialized.load(std::memory_order_acquire))
        return 0;
    
    EnterCriticalSection(&cs_wrapper);
    g_initialized.store(true, std::memory_order_release);
    LeaveCriticalSection(&cs_wrapper);
    
    // Initialize MinHook automatically
    if (!MH_Initialize()) {
        Terminate(wVersion, pModule);
        return -1;
    }
    
    HookReadFile();
    return wVersion == 1 ? 0 : -1;
}

extern "C" __declspec(dllexport) int Terminate(WORD wVersion, void* pModule) {
    EnterCriticalSection(&cs_wrapper);
    g_initialized.store(false, std::memory_order_release);
    LeaveCriticalSection(&cs_wrapper);
    
    for (auto& handle : g_handles) {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
    g_handles.clear();
    
    DeleteCriticalSection(&cs_wrapper);
    return 0;
}

// FIXED: Use MH_CreateHook instead of non-existent MH_CreateDetour
__declspec(dllexport) bool HookReadFile() {
    HMODULE hKernel = GetModuleHandleA("ntdll.dll");
    
    if (!hKernel) {
        return false;
    }
    
    // Get original ReadFile function address
    FARPROC origFunc = GetProcAddress(hKernel, "NtReadFile");
    if (origFunc) {
        OriginalNtReadFile = reinterpret_cast<ReadFile_t>(origFunc);
        
        // Create hook using MinHook's correct API - uses internal detour mechanism
        PMH_HOOK hHook;
        MH_CreateHook(origFunc, nullptr, &hHook);  // No custom detour function needed
        
        if (hHook) {
            MH_EnableHook(hHook);
            return true;
        }
    } else {
        return false;
    }
    
    return false;
}

static inline bool ShouldCacheOptimize(const char* filename) {
    if (!filename || !*filename)
        return false;
    
    // Check game content paths
    const char* excludeDirs[] = {"SaveData/", "User/", "Config/", NULL};
    for (const char* dir : excludeDirs) {
        if (dir) {
            size_t len = _tcslen_s(filename, 260);
            if (_strnicmp(filename, dir, len) == 0)
                return false;
        }
    }
    
    return true;
}

__declspec(dllexport) NTSTATUS ReadFileEx(HANDLE hFile, PULONG64 ulReadOffset, 
                                           ULONG nBytesToRead, void* pBuf) {
    // This is where you call the original through MinHook hook
    if (OriginalNtReadFile) {
        return OriginalNtReadFile(hFile, pBuf, nBytesToRead, nullptr, nullptr);
    }
    
    return STATUS_SUCCESS;
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