cmake_minimum_required(VERSION 3.16)
project(ASI_CacheOptimizer VERSION 1.2.0 LANGUAGES CXX C)

# Fix FetchContent timestamp policy warning
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Ox")
set(CMAKE_CXX_FLAGS_RELEASE "/Ox /DNDEBUG")

# Fetch MinHook from releases ZIP
include(FetchContent)
FetchContent_Declare(
    minhook
    URL https://github.com/TsudaKageyu/minhook/releases/download/v1.3.4/MinHook_134_lib.zip
    DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/minhook_download"
)

FetchContent_MakeAvailable(minhook)

# Create the ASI DLL with MinHook
add_library(ASICacheOptimizer MODULE src/dllmain.cpp)

target_include_directories(ASICacheOptimizer PRIVATE 
    .
    ${minhook_SOURCE_DIR}/include
)

target_link_libraries(ASICacheOptimizer PRIVATE 
    ${minhook_SOURCE_DIR}/lib/libMinHook.x64.lib
    kernel32
)

# Set the file extension to .asi
set_target_properties(ASICacheOptimizer PROPERTIES
    SUFFIX ".asi"
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "ASI Cache Optimizer will be built for Ultimate ASI Loader")