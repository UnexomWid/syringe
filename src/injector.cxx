#include <Windows.h>
#include <TlHelp32.h>

#include "injector.hxx"

static InjectionStatus inject_internal(HANDLE victim, LPCWSTR payload);

class VirtualPtr {
    HANDLE process;
    void*  addr;

    public:
    VirtualPtr(HANDLE process, SIZE_T size) {
        this->process = process;
        addr          = VirtualAllocEx(process, NULL, size, MEM_COMMIT, PAGE_READWRITE);
    }

    void* get() {
        return addr;
    }
    void release() {
        addr = NULL;
    }

    ~VirtualPtr() {
        if (addr != NULL) {
            VirtualFreeEx(process, addr, 0, MEM_RELEASE);
        }
    }
};

class HeapPtr {
    void* addr;

    public:
    HeapPtr() {
        addr = NULL;
    }
    HeapPtr(SIZE_T size) {
        alloc(size);
    }

    void alloc(SIZE_T size) {
        if (!addr) {
            addr = HeapAlloc(GetProcessHeap(), 0, size);
        }
    }
    void* get() {
        return addr;
    }
    void release() {
        addr = NULL;
    }

    ~HeapPtr() {
        if (addr != NULL) {
            HeapFree(GetProcessHeap(), 0, addr);
        }
    }
};

class LibraryHandle {
    HMODULE handle;

    public:
    LibraryHandle(LPCWSTR path) {
        // DONT_RESOLVE_DLL_REFERENCES is usually not recommended. However, for our purposes, it fits perfectly.
        handle = LoadLibraryExW(path, NULL, DONT_RESOLVE_DLL_REFERENCES);
    }

    HMODULE get() {
        return handle;
    }

    ~LibraryHandle() {
        if (handle) {
            FreeLibrary(handle);
        }
    }
};

class HandleGuard {
    HANDLE handle;

    public:
    HandleGuard(HANDLE handle) {
        this->handle = handle;
    }

    void release() {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }

    ~HandleGuard() {
        if (INVALID_HANDLE_VALUE != handle) {
            CloseHandle(handle);
        }
    }
};

InjectionStatus inject_start(LPCWSTR victim, LPCWSTR commandLine, LPCWSTR payload, DWORD* pid) {
    STARTUPINFOW        info {};
    PROCESS_INFORMATION process;

    {
        HeapPtr cmdLine;

        if (commandLine) {
            // Since CreateProcessW requires LPWSTR, we have to copy the original constant command line to a mutable buffer.
            auto cmdLineLength = wcslen(commandLine);
            cmdLine.alloc(sizeof(WCHAR) * (cmdLineLength + 1));

            if (!cmdLine.get()) {
                return InjectionStatus::ALLOC_FAILED;
            }

            wcscpy_s((LPWSTR) cmdLine.get(), cmdLineLength + 1, commandLine);
        }

        // Create the victim process. It should start suspended, and resume execution after the injection.
        auto createStatus = CreateProcessW(victim, (LPWSTR) cmdLine.get(), NULL, NULL, FALSE, CREATE_SUSPENDED | CREATE_NEW_CONSOLE, NULL, NULL, &info, &process);

        if (!createStatus) {
            return InjectionStatus::CREATE_PROCESS_FAILED;
        }
    }

    auto status = inject_internal(process.hProcess, payload);

    if (status != InjectionStatus::OK) {
        TerminateProcess(process.hProcess, 1u);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);

        return status;
    }

    ResumeThread(process.hThread);

    if (pid) {
        *pid = process.dwProcessId;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    return InjectionStatus::OK;
}

InjectionStatus inject(DWORD victimPid, LPCWSTR payload) {
    auto victim = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, victimPid);

    if (!victim) {
        return InjectionStatus::OPEN_PROCESS_FAILED;
    }

    auto status = inject_internal(victim, payload);
    CloseHandle(victim);

    return status;
}

static InjectionStatus inject_internal(HANDLE victim, LPCWSTR payload) {
    // Get a handle for KERNEL32.dll
    // In most cases, this handle is also valid for the victim process.
    auto loadLibrary = (LPVOID) GetProcAddress(GetModuleHandleW(L"KERNEL32.dll"), "LoadLibraryW");

    if (!loadLibrary) {
        return InjectionStatus::PROC_ADDRESS_FAILED;
    }

    auto       length = (wcslen(payload) + 1) * sizeof(WCHAR);
    VirtualPtr addr(victim, length);

    if (!addr.get()) {
        return InjectionStatus::REMOTE_ALLOC_FAILED;
    }

    if (!WriteProcessMemory(victim, addr.get(), payload, length, NULL)) {
        return InjectionStatus::WRITE_FAILED;
    }

    auto thread = CreateRemoteThread(victim, NULL, 0, (LPTHREAD_START_ROUTINE) loadLibrary, addr.get(), 0, NULL);

    if (!thread) {
        return InjectionStatus::CREATE_THREAD_FAILED;
    }

    // Wait for the DLL to be loaded.
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);

    // Find the payload DLL inside the victim process.
    auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetProcessId(victim));

    if (INVALID_HANDLE_VALUE == snapshot) {
        return InjectionStatus::PROC_ADDRESS_FAILED;
    }

    HandleGuard snapGuard(snapshot); // Automatically free the handle when going out of scope if it wasn't freed already.

    MODULEENTRY32W entry;
    auto           found = false;

    entry.dwSize = sizeof(MODULEENTRY32W);

    auto payloadLength = wcslen(payload);
    auto status        = Module32FirstW(snapshot, &entry);

    if (!status) {
        return InjectionStatus::PROC_ADDRESS_FAILED;
    }

    do {
        auto moduleLength = wcslen(entry.szModule);
        auto limit        = payloadLength < moduleLength ? payloadLength : moduleLength;

        found = true;

        // wcsrnicmp
        for (size_t i = 0; i < limit; ++i) {
            if (towlower(payload[payloadLength - i - 1]) != towlower(entry.szModule[moduleLength - i - 1])) {
                found = false;
                break;
            }
        }

        if (found) {
            break;
        }

        status = Module32NextW(snapshot, &entry);
    } while (status);

    snapGuard.release();

    if (!found) {
        return InjectionStatus::PROC_ADDRESS_FAILED;
    }

    // Address of the 'Init' function inside the payload DLL, in the victim memory space.
    FARPROC victimInitProc;

    // Find the 'Init' function inside the DLL. Load the DLL in the current process (injector) without calling DllMain,
    // find the proc address, subtract the base handle from it, and use that offset with the handle from the victim process.
    {
        LibraryHandle payloadHandle(payload);

        if (!payloadHandle.get()) {
            return InjectionStatus::PROC_ADDRESS_FAILED;
        }

        auto initAddr = GetProcAddress(payloadHandle.get(), INJECTOR_INIT_FUNCTION_NAME);

        if (!initAddr) {
            return InjectionStatus::OK; // Payload DLL has no function named 'Init', so don't call anything.
        }

        victimInitProc = (FARPROC) ((char*) initAddr - (char*) payloadHandle.get() + (char*) entry.hModule);
    }

    // Call the 'Init' function.
    thread = CreateRemoteThread(victim, NULL, 0, (LPTHREAD_START_ROUTINE) victimInitProc, NULL, 0, NULL);

    if (!thread) {
        return InjectionStatus::CREATE_THREAD_FAILED;
    }

    // Wait for the 'Init' function to finish
    WaitForSingleObject(thread, INFINITE);

    DWORD initStatus;

    if (!GetExitCodeThread(thread, &initStatus)) {
        return InjectionStatus::INIT_FAILED;
    }

    if (initStatus != INJECTOR_INIT_SUCCESS) {
        return InjectionStatus::INIT_FAILED;
    }

    CloseHandle(thread);

    return InjectionStatus::OK;
}