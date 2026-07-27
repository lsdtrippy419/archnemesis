#include "inject.hpp"
#include "util.hpp"

#include <tlhelp32.h>

namespace mx {

DWORD find_process_id(const wchar_t* exe_name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, exe_name) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

bool drop_payload(const wchar_t* drop_path, const std::vector<uint8_t>& dll_image) {
    if (!drop_path || dll_image.empty()) return false;
    return write_file(drop_path, dll_image.data(), dll_image.size());
}

void clean_payload(const wchar_t* drop_path) {
    if (drop_path) DeleteFileW(drop_path);
}

}  // namespace mx
