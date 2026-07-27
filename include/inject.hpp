#pragma once

#include <windows.h>
#include <cstdint>
#include <vector>

namespace mx {

DWORD find_process_id(const wchar_t* exe_name);

// Drop the payload DLL to the path the driver expects.
// Returns true if the file was written successfully.
bool drop_payload(const wchar_t* drop_path, const std::vector<uint8_t>& dll_image);

// Clean up the dropped payload file.
void clean_payload(const wchar_t* drop_path);

}  // namespace mx
