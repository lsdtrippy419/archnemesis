#pragma once

namespace mx {

inline constexpr wchar_t kTargetProcess[] = L"PioneerGame.exe";
inline constexpr wchar_t kPayloadDll[]    = L"mxhost.dll";
inline constexpr wchar_t kPayloadDrop[]   = L"C:\\Windows\\Temp\\mx_payload.dat";

inline constexpr char kBanner[]       = " mxutil v3.0 ";
inline constexpr char kProductTitle[] = "MxUtil Service";

inline constexpr wchar_t kServiceRegPrefix[] =
    L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\";

// Seconds to wait after game appears before assuming stomp complete
inline constexpr int kStompWaitSec = 10;

}  // namespace mx
