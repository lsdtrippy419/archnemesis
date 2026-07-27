#include "driver.hpp"
#include "config.hpp"
#include "util.hpp"

#include <windows.h>
#include <winternl.h>
#include <string>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "kernel32.lib")

namespace mx {

namespace {

using RtlAdjustPrivilege_t = NTSTATUS(NTAPI*)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
using NtLoadDriver_t = NTSTATUS(NTAPI*)(PUNICODE_STRING);
using NtUnloadDriver_t = NTSTATUS(NTAPI*)(PUNICODE_STRING);
using RtlInitUnicodeString_t = VOID(NTAPI*)(PUNICODE_STRING, PCWSTR);

bool enable_load_driver_privilege() {
    // SeLoadDriverPrivilege = 10
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    auto RtlAdjustPrivilege =
        reinterpret_cast<RtlAdjustPrivilege_t>(GetProcAddress(ntdll, "RtlAdjustPrivilege"));
    if (!RtlAdjustPrivilege) return false;
    BOOLEAN was = FALSE;
    NTSTATUS st = RtlAdjustPrivilege(10 /* SeLoadDriverPrivilege */, TRUE, FALSE, &was);
    return st >= 0;
}

std::wstring to_nt_path(const std::wstring& win32) {
    // \??\C:\...\file.sys
    if (win32.rfind(L"\\??\\", 0) == 0) return win32;
    return L"\\??\\" + win32;
}

bool write_service_registry(const std::string& name, const std::wstring& nt_image) {
    std::wstring key = L"SYSTEM\\CurrentControlSet\\Services\\";
    key += std::wstring(name.begin(), name.end());
    HKEY h = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &h,
                        nullptr) != ERROR_SUCCESS)
        return false;
    // Type = 1 (SERVICE_KERNEL_DRIVER), Start = 3 (DEMAND)
    DWORD type = 1;
    DWORD start = 3;
    DWORD err = 1;
    RegSetValueExW(h, L"Type", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&type), sizeof(type));
    RegSetValueExW(h, L"Start", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&start), sizeof(start));
    RegSetValueExW(h, L"ErrorControl", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&err),
                   sizeof(err));
    RegSetValueExW(h, L"ImagePath", 0, REG_EXPAND_SZ,
                   reinterpret_cast<const BYTE*>(nt_image.c_str()),
                   static_cast<DWORD>((nt_image.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(h);
    return true;
}

bool nt_load_driver(const std::wstring& reg_path) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    auto NtLoadDriver = reinterpret_cast<NtLoadDriver_t>(GetProcAddress(ntdll, "NtLoadDriver"));
    auto RtlInitUnicodeString =
        reinterpret_cast<RtlInitUnicodeString_t>(GetProcAddress(ntdll, "RtlInitUnicodeString"));
    if (!NtLoadDriver || !RtlInitUnicodeString) return false;
    UNICODE_STRING u{};
    RtlInitUnicodeString(&u, reg_path.c_str());
    NTSTATUS st = NtLoadDriver(&u);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "returned: 0x%08lX (%s)", static_cast<unsigned long>(st),
                  st >= 0 ? "SUCCESS" : "FAILURE");
    note(buf);
    return st >= 0;
}

bool nt_unload_driver(const std::wstring& reg_path) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    auto NtUnloadDriver = reinterpret_cast<NtUnloadDriver_t>(GetProcAddress(ntdll, "NtUnloadDriver"));
    auto RtlInitUnicodeString =
        reinterpret_cast<RtlInitUnicodeString_t>(GetProcAddress(ntdll, "RtlInitUnicodeString"));
    if (!NtUnloadDriver || !RtlInitUnicodeString) return false;
    UNICODE_STRING u{};
    RtlInitUnicodeString(&u, reg_path.c_str());
    return NtUnloadDriver(&u) >= 0;
}

void delete_service_registry(const std::string& name) {
    std::wstring key = L"SYSTEM\\CurrentControlSet\\Services\\";
    key += std::wstring(name.begin(), name.end());
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, key.c_str());
}

}  // namespace

bool load_mapper(const std::vector<uint8_t>& driver_image, DriverContext& ctx) {
    if (!enable_load_driver_privilege()) {
        // Still attempt load even if privilege adjustment failed.
        note("RtlAdjustPrivilege(SeLoadDriverPrivilege) failed");
    }

    ctx.service_name = random_service_name();
    note((std::string("service name: ") + ctx.service_name).c_str());

    // Primary: %TEMP%\<svc>.sys
    std::wstring temp_path = temp_dir();
    temp_path += std::wstring(ctx.service_name.begin(), ctx.service_name.end());
    temp_path += L".sys";

    bool loaded = false;
    if (write_file(temp_path, driver_image.data(), driver_image.size())) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "file wrote (%zu bytes).", driver_image.size());
        ok(buf);
        ctx.sys_path = temp_path;
        write_service_registry(ctx.service_name, to_nt_path(temp_path));
        ctx.reg_service = std::wstring(kServiceRegPrefix) +
                          std::wstring(ctx.service_name.begin(), ctx.service_name.end());
        note("Loading ...");
        loaded = nt_load_driver(ctx.reg_service);
        if (!loaded) note("Registry load failed.");
    } else {
        fail("Failed to write.");
    }

    if (!loaded) {
        note("Trying to load from current directory...");
        std::wstring cwd = exe_dir();
        cwd += std::wstring(ctx.service_name.begin(), ctx.service_name.end());
        cwd += L".sys";
        if (!write_file(cwd, driver_image.data(), driver_image.size())) {
            fail("CWD fallback also failed.");
            return false;
        }
        {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "file wrote (%zu bytes).", driver_image.size());
            ok(buf);
        }
        ctx.sys_path = cwd;
        write_service_registry(ctx.service_name, to_nt_path(cwd));
        ctx.reg_service = std::wstring(kServiceRegPrefix) +
                          std::wstring(ctx.service_name.begin(), ctx.service_name.end());
        note("Loading ...");
        loaded = nt_load_driver(ctx.reg_service);
        if (!loaded) {
            fail("Failed to initialize (all attempts failed).");
            return false;
        }
    }

    // No device object — new driver is autonomous (process/image notify callbacks).
    // Driver is loaded and watching for PioneerGame.exe.
    ok("driver loaded (autonomous mode)");
    ok("ready.");
    write_run_marker();
    return true;
}

void unload_mapper(DriverContext& ctx) {
    if (!ctx.reg_service.empty()) {
        nt_unload_driver(ctx.reg_service);
        delete_service_registry(ctx.service_name);
    }
    if (!ctx.sys_path.empty()) DeleteFileW(ctx.sys_path.c_str());
}

}  // namespace mx
