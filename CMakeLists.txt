cmake_minimum_required(VERSION 3.15)

project(bincollector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(deps/digestpp)
add_subdirectory(deps/fmt)
#add_subdirectory(deps/miniz)
add_subdirectory(deps/zstd)
add_subdirectory(deps/zlib)

add_executable(bincollector
    src/app.hpp
    src/app.cpp
    src/common/bt_error.cpp
    src/common/bt_error.hpp
    src/common/fltbf.hpp
    src/common/fs.hpp
    src/common/magic.hpp
    src/common/magic.cpp
    src/common/mmap.cpp
    src/common/mmap.hpp
    src/common/string.hpp
    src/common/xxhash64.hpp
    src/file/base.cpp
    src/file/base.hpp
    src/file/hashlist.cpp
    src/file/hashlist.hpp
    src/file/raw.cpp
    src/file/raw.hpp
    src/file/sln/manifest.hpp
    src/file/sln/manifest.cpp
    src/file/rlsm.cpp
    src/file/rlsm.hpp
    src/file/rlsm/manifest.cpp
    src/file/rlsm/manifest.hpp
    src/file/rman.cpp
    src/file/rman.hpp
    src/file/rman/manifest.cpp
    src/file/rman/manifest.hpp
    src/file/wad.cpp
    src/file/wad.hpp
    src/file/wad/wad.cpp
    src/file/wad/wad.hpp
    src/main.cpp
    )
target_link_libraries(bincollector PRIVATE digestpp zstd fmt zlib)
target_include_directories(bincollector PRIVATE src/)
