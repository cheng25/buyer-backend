cmake_minimum_required(VERSION 3.5)
project(buyer-backend CXX)

# ========== 输出目录统一配置 ==========
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/runtime)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME} main.cc)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()

# ##############################################################################
# If you include the drogon source code locally in your project, use this method
# to add drogon 
# add_subdirectory(drogon) 
# target_link_libraries(${PROJECT_NAME} PRIVATE drogon)
#
# and comment out the following line
# find_package(redis++ CONFIG REQUIRED)

#find_package(boost_uuid REQUIRED CONFIG)
#find_package(cppzmq CONFIG REQUIRED)
find_package(Drogon CONFIG REQUIRED)
#find_package(jwt-cpp CONFIG REQUIRED)
#find_package(unofficial-argon2 CONFIG REQUIRED)
#find_package(aws-cpp-sdk-core REQUIRED)
#find_package(aws-cpp-sdk-s3 REQUIRED)
#find_package(unordered_dense CONFIG REQUIRED)
#find_package(glaze CONFIG REQUIRED)
# ==============================
# 子模块依赖：全部 add_subdirectory
# ==============================

# 1.

# 2. jwt-cpp
add_subdirectory(libs/jwt-cpp)

# 3. argon2
set(ARGON2_TESTS OFF CACHE BOOL "" FORCE)
set(ARGON2_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(libs/libargon2)

# 4. AWS SDK (只开 S3，加快编译)
# ==============================================
# 4.1. 先编译：curl（源码构建，供 AWS SDK 使用）
# ==============================================
# ==============================================
# curl 源码构建（无任何外部依赖，专为 AWS SDK）
# ==============================================
# 关键：关闭所有额外依赖（解决你现在的报错）
set(CURL_USE_BROTLI OFF CACHE BOOL "" FORCE)
set(CURL_USE_ZSTD OFF CACHE BOOL "" FORCE)
set(CURL_USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)

set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)        # 不编译 curl 命令行
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)     # 编译静态库

# 只保留 SSL（AWS 必须）
set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)       # 启用 SSL（AWS 必须）
set(CURL_ENABLE_SSL ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(libs/curl)

# ==============================================
# 给 AWS SDK 提供 CURL 变量
# 不使用 generator expression，完全兼容 AWS
# ==============================================
if (TARGET libcurl_static)
    set(CURL_LIBRARY libcurl_static)
    set(CURL_LIBRARIES libcurl_static)
endif()

get_target_property(CURL_INCLUDE_DIRS libcurl_static INTERFACE_INCLUDE_DIRECTORIES)
set(CURL_INCLUDE_DIR ${CURL_INCLUDE_DIRS})
set(CURL_FOUND TRUE)

# ==============================================
# 4.2. 再编译：AWS SDK（自动使用上面的静态 curl）
# ==============================================
set(BUILD_ONLY "s3" CACHE STRING "" FORCE)
set(AWS_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(AWS_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)

# 强制 AWS 使用我们编译的 curl
set(FORCE_CURL_TO_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(LEGACY_BUILD ON CACHE BOOL "" FORCE)  # 必须开 ON，否则不兼容静态库

add_subdirectory(libs/aws-sdk-cpp)

# --------------------------------------------------------------------
# 5.1 ZeroMQ（必须先编译！cppzmq只是头文件，需要底层libzmq）
# --------------------------------------------------------------------
set(ZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(WITH_PERF_TOOL OFF CACHE BOOL "" FORCE)
set(ZMQ_CPP11 ON CACHE BOOL "" FORCE)
set(ZMQ_WSS OFF CACHE BOOL "" FORCE)
add_subdirectory(libs/libzmq)

# 5.2 cppzmq（头文件包装）
set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(libs/cppzmq)
# 禁用 cppzmq 自动链接 libzmq（解决 -llibzmq 报错！）
set_target_properties(cppzmq PROPERTIES INTERFACE_LINK_LIBRARIES "")
target_link_libraries(cppzmq INTERFACE libzmq-static)
target_include_directories(${PROJECT_NAME} PRIVATE
        libs/libzmq/include)

# 6. boost-uuid
# ======================
add_subdirectory(libs/boost)
# 添加 boost 头文件路径
target_include_directories(${PROJECT_NAME} PRIVATE
        libs/boost/assert/include
        libs/boost/config/include
        libs/boost/core/include
        libs/boost/throw_exception/include
        libs/boost/type_traits/include
        libs/boost/uuid/include)

#include_directories(libs/boost)

# 7. glaze
add_subdirectory(libs/glaze)

# 8. unordered_dense
add_subdirectory(libs/unordered-dense)

# 9. redis-plus-plus
set(REDIS_PLUS_PLUS_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(REDIS_PLUS_PLUS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(REDIS_PLUS_PLUS_CXX17 ON CACHE BOOL "" FORCE)
set(REDIS_PLUS_PLUS_ASYNC ON CACHE BOOL "" FORCE)
set(REDIS_PLUS_PLUS_TLS ON CACHE BOOL "" FORCE)
add_subdirectory(libs/redis-plus-plus)

target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon 
                                              jwt-cpp::jwt-cpp 
                                              #unofficial::argon2::libargon2
                                              argon2::argon2
                                              cppzmq 
                                              libzmq-static
                                              redis++_static
                                              #Boost::uuid 源码引入，不需要
                                              aws-cpp-sdk-core
                                              aws-cpp-sdk-s3
                                              unordered_dense::unordered_dense
                                              glaze::glaze
                                              libcurl_static
                      )

# ##############################################################################

file(GLOB_RECURSE SERVICE_SRC CONFIGURE_DEPENDS 
     ${PROJECT_SOURCE_DIR}/services/*
    )
aux_source_directory(controllers CTL_SRC)
aux_source_directory(filters FILTER_SRC)
aux_source_directory(plugins PLUGIN_SRC)
# aux_source_directory(models MODEL_SRC) # Not used for now
aux_source_directory(config CONFIG_SRC)

drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
                    ${CMAKE_CURRENT_BINARY_DIR})
# use the following line to create views with namespaces.
# drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
#                     ${CMAKE_CURRENT_BINARY_DIR} TRUE)
# use the following line to create views with namespace CHANGE_ME prefixed
# and path namespaces.
# drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
#                     ${CMAKE_CURRENT_BINARY_DIR} TRUE CHANGE_ME)

target_include_directories(${PROJECT_NAME}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                   # ${CMAKE_CURRENT_SOURCE_DIR}/models
                                   aws-cpp-sdk-core 
                                   )
target_sources(${PROJECT_NAME}
               PRIVATE
               ${SRC_DIR}
               ${CTL_SRC}
               ${FILTER_SRC}
               ${PLUGIN_SRC}
               #${MODEL_SRC}
               ${CONFIG_SRC}
               ${SERVICE_SRC})
# ##############################################################################
# uncomment the following line for dynamically loading views 
# set_property(TARGET ${PROJECT_NAME} PROPERTY ENABLE_EXPORTS ON)

# ##############################################################################

# Database management for tests
# Add if not using docker and Postgres is installed locally !!!
# if(WIN32)
#     add_custom_target(setup_test_db
#         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/setup_test_db.bat
#         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#         COMMENT "Setting up test database"
#     )
# else()
#     add_custom_target(setup_test_db
#         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/setup_test_db.sh
#         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#         COMMENT "Setting up test database"
#     )
# endif()

option(ENABLE_TESTS "Enable building tests" ON)

if (ENABLE_TESTS)
    add_subdirectory(test)
endif()
