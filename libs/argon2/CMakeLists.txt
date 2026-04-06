cmake_minimum_required(VERSION 3.12)
project(argon2 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)

# ==============================
# 源文件（官方 Makefile 1:1）
# ==============================
set(ARGON2_SRC
        libargon2/src/argon2.c
        libargon2/src/core.c
        libargon2/src/blake2/blake2b.c
        libargon2/src/thread.c
        libargon2/src/encoding.c
)

# ==============================
# 自动选择优化版 / 参考版
# ==============================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libargon2/src/opt.c)
    message(STATUS "argon2: 使用优化实现 (opt.c)")
    list(APPEND ARGON2_SRC libargon2/src/opt.c)
else()
    message(STATUS "argon2: 使用通用实现 (ref.c)")
    list(APPEND ARGON2_SRC libargon2/src/ref.c)
endif()

# ==============================
# 系统库（Linux 需要 rt dl pthread）
# ==============================
add_library(argon2 STATIC ${ARGON2_SRC})
add_library(argon2::argon2 ALIAS argon2)

# ==============================
# 头文件路径
# ==============================
target_include_directories(argon2
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/libargon2/include
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libargon2/src
        ${CMAKE_CURRENT_SOURCE_DIR}/libargon2/src/blake2
)

# ==============================
# 编译选项（和官方 Makefile 一致）
# ==============================
target_compile_options(argon2 PRIVATE
        -O3
        -Wall
        -g
)

if(UNIX AND NOT APPLE)
    target_compile_options(argon2 PRIVATE -pthread)
endif()


if(UNIX AND NOT APPLE)
    target_link_libraries(argon2 PUBLIC rt dl pthread)
endif()

# ==============================
# 版本信息
# ==============================
set_target_properties(argon2 PROPERTIES
        VERSION 1
        SOVERSION 1
)