#pragma once

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <cstdint>
#include <cstddef>

enum class InjectionStatus : uint8_t {
    OK                    = 0,
    ALLOC_FAILED          = 1,
    CREATE_PROCESS_FAILED = 2,
    OPEN_PROCESS_FAILED   = 3,
    PROC_ADDRESS_FAILED   = 4,
    REMOTE_ALLOC_FAILED   = 5,
    WRITE_FAILED          = 6,
    CREATE_THREAD_FAILED  = 8,
    INIT_FAILED           = 9
};

static constexpr auto  INJECTOR_INIT_FUNCTION_NAME = "Init";
static constexpr DWORD INJECTOR_INIT_SUCCESS       = 0;

extern "C" {
// Start a victim process and immediately inject a DLL payload into it
InjectionStatus inject_start(LPCWSTR victim, LPCWSTR commandLine, LPCWSTR payload, DWORD* pid);
// Inject a DLL payload into an already running victim process
InjectionStatus inject(DWORD victimPid, LPCWSTR payload);
}