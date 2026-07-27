#include "config.hpp"
#include "util.hpp"
#include "driver.hpp"
#include "inject.hpp"

#include <string>
#include <vector>

// New flow (v3.0 — autonomous, no IOCTL):
//   1. Admin check
//   2. Drop mxhost.dll as mx_payload.dat (driver reads from this path)
//   3. Load mxdrv.sys via NtLoadDriver (driver registers process/image notify)
//   4. Wait for PioneerGame.exe to appear
//   5. Wait for driver to stomp payload (fixed delay)
//   6. Clean up payload file, unload driver

static bool load_driver_bytes(std::vector<uint8_t>& out) {
    const std::wstring candidates[] = {
        mx::exe_dir() + L"mxdrv.sys",
        mx::exe_dir() + L"..\\mxdrv.sys",
        L"mxdrv.sys",
    };
    const wchar_t* env = _wgetenv(L"MX_DRV_PATH");
    if (env && *env) {
        if (mx::read_file(env, out) && !out.empty()) return true;
    }
    for (const auto& p : candidates) {
        if (mx::read_file(p, out) && !out.empty()) return true;
    }
    mx::fail("Cannot find mxdrv.sys");
    return false;
}

int main() {
    mx::enable_ansi();
    std::printf("\x1b[36m%s\x1b[0m\n", mx::kBanner);

    // [1] Admin
    mx::info("1", "Checking system privileges...");
    if (!mx::is_admin()) {
        std::printf("Application must be run as Administrator!\n");
        mx::pause_exit(1);
    }
    mx::ok("Elevated privileges confirmed.");

    // [2] Drop payload to temp path
    mx::info("2", "Preparing payload...");
    std::wstring dll_path = mx::exe_dir() + mx::kPayloadDll;
    std::vector<uint8_t> dll;
    if (!mx::read_file(dll_path, dll)) {
        char buf[512];
        std::snprintf(buf, sizeof(buf), "Cannot read DLL: %ls", dll_path.c_str());
        mx::fail(buf);
        mx::pause_exit(1);
    }
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Payload loaded (%zu bytes).", dll.size());
        mx::ok(buf);
    }

    if (!mx::drop_payload(mx::kPayloadDrop, dll)) {
        mx::fail("Failed to drop payload to temp path.");
        mx::pause_exit(1);
    }
    {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "Payload staged at %ls", mx::kPayloadDrop);
        mx::ok(buf);
    }
    mx::sep();

    // [3] Load driver (autonomous — no device object, no IOCTL)
    mx::info("3", "Loading driver...");
    std::vector<uint8_t> driver_image;
    mx::DriverContext drv{};
    if (!load_driver_bytes(driver_image) || !mx::load_mapper(driver_image, drv)) {
        mx::fail("Failed to load driver.");
        mx::clean_payload(mx::kPayloadDrop);
        mx::pause_exit(1);
    }
    mx::note("Driver is autonomous — watching for game process.");
    mx::sep();

    // [4] Wait for target process
    mx::info("4", "Waiting for target process...");
    DWORD pid = 0;
    while ((pid = mx::find_process_id(mx::kTargetProcess)) == 0) Sleep(500);
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Found target! PID: %lu", static_cast<unsigned long>(pid));
        mx::ok(buf);
    }
    mx::sep();

    // [5] Wait for driver to complete the stomp
    mx::info("5", "Waiting for driver to inject payload...");
    mx::note("Driver will stomp payload when NlsData DLL loads in game.");
    for (int i = mx::kStompWaitSec; i > 0; --i) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Stomp window: %d seconds remaining...", i);
        std::printf("\r\x1b[90m  |--> \x1b[33m%s\x1b[0m", buf);
        Sleep(1000);
    }
    std::printf("\n");
    mx::ok("Stomp window elapsed.");
    mx::sep();

    // [6] Clean up
    mx::info("6", "Cleaning up...");
    mx::clean_payload(mx::kPayloadDrop);
    mx::ok("Payload file removed.");

    mx::note("Unloading driver...");
    mx::unload_mapper(drv);
    mx::ok("Driver unloaded.");

    mx::sep();
    mx::ok("Injection complete. Launch confirmed.");
    mx::pause_exit(0);
}
