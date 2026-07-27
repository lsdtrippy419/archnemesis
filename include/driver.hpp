#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <windows.h>

namespace mx {

struct DriverContext {
    std::string service_name;
    std::wstring sys_path;
    std::wstring reg_service;
};

bool load_mapper(const std::vector<uint8_t>& driver_image, DriverContext& ctx);
void unload_mapper(DriverContext& ctx);

}  // namespace mx
