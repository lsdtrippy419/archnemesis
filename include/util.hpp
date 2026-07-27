#pragma once

#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>

namespace mx {

inline void enable_ansi() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode))
            SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    system("color 3");
}

inline void info(const char* step, const char* msg) {
    std::printf("\x1b[90m [\x1b[36m%s\x1b[90m] \x1b[37m%s\x1b[0m\n", step, msg);
}
inline void ok(const char* msg) {
    std::printf("\x1b[90m  |--> \x1b[32mSUCCESS: \x1b[37m%s\x1b[0m\n", msg);
}
inline void fail(const char* msg) {
    std::printf("\x1b[90m  |--> \x1b[31mERROR: \x1b[37m%s\x1b[0m\n", msg);
}
inline void note(const char* msg) {
    std::printf("\x1b[90m  |--> \x1b[36mINFO: \x1b[37m%s\x1b[0m\n", msg);
}
inline void sep() {
    std::printf("\x1b[90m --------------------------------------------------------\x1b[0m\n");
}

inline void pause_exit(int code) {
    system("pause");
    ExitProcess(static_cast<UINT>(code));
}

inline bool is_admin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                 &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

inline std::wstring exe_dir() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring s(path);
    auto pos = s.find_last_of(L"\\/");
    if (pos != std::wstring::npos) s.resize(pos + 1);
    return s;
}

inline std::wstring temp_dir() {
    wchar_t buf[MAX_PATH]{};
    GetTempPathW(MAX_PATH, buf);
    return buf;
}

inline std::string random_service_name(size_t n = 15) {
    // Alphanumeric service name
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::string out(n, 'A');
    HCRYPTPROV prov = 0;
    if (CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(prov, static_cast<DWORD>(n), reinterpret_cast<BYTE*>(out.data()));
        CryptReleaseContext(prov, 0);
        for (size_t i = 0; i < n; ++i)
            out[i] = alphabet[static_cast<unsigned char>(out[i]) % (sizeof(alphabet) - 1)];
    }
    return out;
}

inline bool read_file(const std::wstring& path, std::vector<uint8_t>& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 64ll * 1024 * 1024) {
        CloseHandle(h);
        return false;
    }
    out.resize(static_cast<size_t>(sz.QuadPart));
    DWORD rd = 0;
    BOOL okRead = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &rd, nullptr);
    CloseHandle(h);
    return okRead && rd == out.size();
}

inline bool write_file(const std::wstring& path, const void* data, size_t len) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0;
    BOOL okWrite = WriteFile(h, data, static_cast<DWORD>(len), &wr, nullptr);
    CloseHandle(h);
    return okWrite && wr == len;
}

inline void write_run_marker() {
    char name[32];
    std::snprintf(name, sizeof(name), "mx_%u.tmp", GetCurrentProcessId() % 100000);
    std::wstring path = temp_dir();
    for (char* p = name; *p; ++p) path.push_back(static_cast<wchar_t>(*p));
    write_file(path, name, std::strlen(name));
}

}  // namespace mx
