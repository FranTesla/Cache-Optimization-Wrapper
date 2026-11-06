cmake_minimum_required(VERSION 3.16)
project(ASI_CacheOptimizer VERSION 1.2.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Ox")
set(CMAKE_CXX_FLAGS_RELEASE "/Ox /DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DDEBUG")

set(OUT_DIR "${CMAKE_BINARY_DIR}/build/ASI")

# Fetch MinHook from releases ZIP (not git source)
include(FetchContent)
FetchContent_Declare(
    minhook
    URL https://github.com/TsudaKageyu/minhook/releases/download/v1.3.4/MinHook_134_lib.zip
    DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/minhook_download"
)

# Add MinHook to build after fetching it
FetchContent_MakeAvailable(minhook)

# Create the ASI DLL with MinHook - use correct target name
add_library(ASICacheOptimizer MODULE src/dllmain.cpp)

target_include_directories(ASICacheOptimizer PRIVATE 
    .
    ${minhook_SOURCE_DIR}/include
)

target_link_libraries(ASICacheOptimizer PRIVATE 
    minhook
    kernel32
)

# Set output to UAL's expected location  
set_target_properties(ASICacheOptimizer PROPERTIES
    SUFFIX ".asi"
    RUNTIME_OUTPUT_DIRECTORY "${OUT_DIR}"
    POSITION_INDEPENDENT_CODE ON
)

# Create PDB file copy command
add_custom_command(TARGET ASICacheOptimizer POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_if_different 
            $<TARGET_PDB_FILE:ASICacheOptimizer>
            "${OUT_DIR}/ASICacheOptimizer.pdb"
)

message(STATUS "ASI Cache Optimizer will be built for Ultimate ASI Loader")