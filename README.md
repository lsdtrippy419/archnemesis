# mxutil

## Layout

```
include/     config, util, driver, inject
src/         main, driver, inject
mxdrv.sys    kernel driver (40,080 bytes)
CMakeLists.txt
```

## Flow

1. Admin check  
2. Write/load `mxdrv.sys` via `NtLoadDriver`, open device  
3. Wait for target process  
4. Read sibling `mxhost.dll`  
5. `DeviceIoControl(0x222004)` — buffer `0x1C`, magic `0x2AFBBBDC`  
6. Cleanup  

## Build

```bat
cmake -B build -A x64
cmake --build build --config Release
```

Place `mxhost.dll` next to the built exe (and keep `mxdrv.sys` beside it). Run as Administrator with the game running.

## Constants

| Item | Value |
|------|--------|
| IOCTL | `0x222004` |
| Magic | `0x2AFBBBDC` |
| In-buf size | `0x1C` |
| Payload | `mxhost.dll` |
