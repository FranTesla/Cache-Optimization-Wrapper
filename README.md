# ASI Cache Optimization Wrapper

## Description
This DLL wrapper intercepts file read operations in games to ensure Windows cache is used instead of bypassing it. This can improve texture streaming and overall game performance when loaded via Ultimate ASI Loader.

## Requirements
- Windows 10/11 x64
- Ultimate ASI Loader compatible (UAL v5 or higher)
- C++17 compiler (MSVC 2019/2022 recommended for Windows builds)
- MinHook library for system call interception

## Building
### Using CMake:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -A x64
cmake --build build --config Release